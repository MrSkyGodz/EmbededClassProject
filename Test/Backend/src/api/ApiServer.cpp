#include "api/ApiServer.h"

#include "protocol/Frame.h"
#include "transport/MockTransport.h"
#include "transport/SerialTransport.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cerrno>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace
{
std::string jsonEscape(const std::string& value)
{
    std::string escaped;
    for (char ch : value)
    {
        if (ch == '"' || ch == '\\')
        {
            escaped.push_back('\\');
        }
        if (ch == '\n')
        {
            escaped += "\\n";
        }
        else
        {
            escaped.push_back(ch);
        }
    }
    return escaped;
}

std::string boolJson(bool value)
{
    return value ? "true" : "false";
}

bool extractString(const std::string& body, const std::string& key, std::string& value)
{
    const std::string token = "\"" + key + "\"";
    size_t pos = 0U;
    while ((pos = body.find(token, pos)) != std::string::npos)
    {
        size_t colon = pos + token.size();
        while (colon < body.size() && std::isspace(static_cast<unsigned char>(body[colon])))
        {
            ++colon;
        }
        if (colon < body.size() && body[colon] == ':')
        {
            pos = colon;
            break;
        }
        pos += token.size();
    }

    if (pos == std::string::npos)
    {
        return false;
    }
    pos = body.find(':', pos);
    if (pos == std::string::npos)
    {
        return false;
    }
    pos = body.find('"', pos);
    if (pos == std::string::npos)
    {
        return false;
    }
    const size_t end = body.find('"', pos + 1U);
    if (end == std::string::npos)
    {
        return false;
    }
    value = body.substr(pos + 1U, end - pos - 1U);
    return true;
}

bool extractNumber(const std::string& body, const std::string& key, double& value)
{
    const std::string token = "\"" + key + "\"";
    size_t pos = 0U;
    while ((pos = body.find(token, pos)) != std::string::npos)
    {
        size_t colon = pos + token.size();
        while (colon < body.size() && std::isspace(static_cast<unsigned char>(body[colon])))
        {
            ++colon;
        }
        if (colon < body.size() && body[colon] == ':')
        {
            pos = colon;
            break;
        }
        pos += token.size();
    }

    if (pos == std::string::npos)
    {
        return false;
    }
    pos = body.find(':', pos);
    if (pos == std::string::npos)
    {
        return false;
    }
    ++pos;
    while (pos < body.size() && std::isspace(static_cast<unsigned char>(body[pos])))
    {
        ++pos;
    }
    size_t end = pos;
    while (end < body.size() && (std::isdigit(static_cast<unsigned char>(body[end])) || body[end] == '.' || body[end] == '-'))
    {
        ++end;
    }
    if (end == pos)
    {
        return false;
    }

    value = std::stod(body.substr(pos, end - pos));
    return true;
}

std::map<std::string, double> extractPayloadValues(const std::string& body)
{
    std::map<std::string, double> values;
    for (const std::string key : {"pwm", "motor1AngleDeg", "motor2AngleDeg", "motor1", "motor2"})
    {
        double value = 0.0;
        if (extractNumber(body, key, value))
        {
            if (key == "motor1")
            {
                values["motor1AngleDeg"] = value;
            }
            else if (key == "motor2")
            {
                values["motor2AngleDeg"] = value;
            }
            else
            {
                values[key] = value;
            }
        }
    }
    return values;
}

std::string nowStamp()
{
    const auto now = std::chrono::system_clock::now();
    const std::time_t time = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &time);
#else
    localtime_r(&time, &tm);
#endif
    std::ostringstream out;
    out << std::put_time(&tm, "%Y%m%d_%H%M%S");
    return out.str();
}

void closeSocket(int socketFd)
{
#ifdef _WIN32
    closesocket(socketFd);
#else
    close(socketFd);
#endif
}

std::string lastSocketError()
{
#ifdef _WIN32
    return "WSA error " + std::to_string(WSAGetLastError());
#else
    return std::strerror(errno);
#endif
}

std::string responseText(int status, const std::string& body)
{
    const char* reason = status == 200 ? "OK" : (status == 400 ? "Bad Request" : "Internal Server Error");
    std::ostringstream out;
    out << "HTTP/1.1 " << status << ' ' << reason << "\r\n";
    out << "Content-Type: application/json\r\n";
    out << "Access-Control-Allow-Origin: *\r\n";
    out << "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n";
    out << "Access-Control-Allow-Headers: Content-Type\r\n";
    out << "Content-Length: " << body.size() << "\r\n";
    out << "Connection: close\r\n\r\n";
    out << body;
    return out.str();
}
}

namespace api
{
ApiServer::ApiServer(unsigned port)
    : port_(port),
      testRunner_(registry_, recorder_)
{
}

ApiServer::~ApiServer()
{
    stop();
}

bool ApiServer::run(std::string& error)
{
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        error = "WSAStartup failed";
        return false;
    }
#endif

    const int serverFd = static_cast<int>(socket(AF_INET, SOCK_STREAM, 0));
    if (serverFd < 0)
    {
        error = "failed to create server socket";
        return false;
    }

    int yes = 1;
    setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&yes), sizeof(yes));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(static_cast<uint16_t>(port_));

    if (bind(serverFd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0)
    {
        error = "failed to bind backend port " + std::to_string(port_) + ": " + lastSocketError();
        closeSocket(serverFd);
        return false;
    }

    if (listen(serverFd, 8) < 0)
    {
        error = "failed to listen";
        closeSocket(serverFd);
        return false;
    }

    serverRunning_ = true;
    addLog("Backend listening on port " + std::to_string(port_));

    while (serverRunning_)
    {
        sockaddr_in clientAddress{};
#ifdef _WIN32
        int clientLength = sizeof(clientAddress);
#else
        socklen_t clientLength = sizeof(clientAddress);
#endif
        const int clientFd = static_cast<int>(accept(serverFd, reinterpret_cast<sockaddr*>(&clientAddress), &clientLength));
        if (clientFd < 0)
        {
            continue;
        }

        char buffer[16384] = {};
        const int received = static_cast<int>(recv(clientFd, buffer, sizeof(buffer) - 1U, 0));
        if (received <= 0)
        {
            closeSocket(clientFd);
            continue;
        }

        std::string raw(buffer, static_cast<size_t>(received));
        HttpRequest request;
        std::istringstream input(raw);
        input >> request.method >> request.path;
        const size_t bodyPos = raw.find("\r\n\r\n");
        if (bodyPos != std::string::npos)
        {
            request.body = raw.substr(bodyPos + 4U);
        }

        const auto response = route(request);
        const auto text = responseText(response.status, response.body);
        send(clientFd, text.c_str(), static_cast<int>(text.size()), 0);
        closeSocket(clientFd);
    }

    closeSocket(serverFd);
#ifdef _WIN32
    WSACleanup();
#endif
    return true;
}

void ApiServer::stop()
{
    serverRunning_ = false;
    stopReader();
    testRunner_.stop();
    std::lock_guard<std::mutex> lock(transportMutex_);
    if (transport_)
    {
        transport_->close();
    }
}

ApiServer::HttpResponse ApiServer::route(const HttpRequest& request)
{
    if (request.method == "OPTIONS")
    {
        return {200, "{\"ok\":true}"};
    }
    if (request.method == "GET" && request.path == "/api/health")
    {
        return {200, healthJson()};
    }
    if (request.method == "GET" && request.path == "/api/messages")
    {
        return {200, messagesJson()};
    }
    if (request.method == "POST" && request.path == "/api/ports/open")
    {
        return openPort(request.body);
    }
    if (request.method == "POST" && request.path == "/api/ports/close")
    {
        return closePort();
    }
    if (request.method == "GET" && request.path == "/api/ports/status")
    {
        return {200, portStatusJson()};
    }
    if (request.method == "POST" && request.path == "/api/messages/send")
    {
        return sendMessage(request.body);
    }
    if (request.method == "GET" && request.path == "/api/received")
    {
        return {200, receivedJson()};
    }
    if (request.method == "POST" && request.path == "/api/received/clear")
    {
        recorder_.clear();
        addLog("Received buffer cleared.");
        return {200, "{\"ok\":true}"};
    }
    if (request.method == "POST" && request.path == "/api/received/export")
    {
        return exportReceived(request.body);
    }
    if (request.method == "GET" && request.path == "/api/logs")
    {
        return {200, logsJson()};
    }
    if (request.method == "GET" && request.path == "/api/tests")
    {
        return {200, testsJson()};
    }
    if (request.method == "POST" && request.path == "/api/tests/run")
    {
        return runTest(request.body);
    }
    if (request.method == "POST" && request.path == "/api/tests/stop")
    {
        const auto status = testRunner_.stop();
        return {200, "{\"ok\":true,\"state\":\"" + status.state + "\",\"message\":\"" + jsonEscape(status.message) + "\"}"};
    }
    if (request.method == "GET" && request.path == "/api/tests/status")
    {
        return {200, testStatusJson()};
    }

    return {400, "{\"ok\":false,\"error\":\"unknown endpoint\"}"};
}

ApiServer::HttpResponse ApiServer::openPort(const std::string& body)
{
    std::string mode = "mock";
    std::string requestedPort = "mock";
    double baud = 115200.0;
    extractString(body, "mode", mode);
    extractString(body, "port", requestedPort);
    extractNumber(body, "baud", baud);

    std::unique_ptr<transport::ITransport> nextTransport;
    if (mode == "serial")
    {
        nextTransport = std::make_unique<transport::SerialTransport>();
    }
    else
    {
        mode = "mock";
        nextTransport = std::make_unique<transport::MockTransport>();
    }

    std::string error;
    if (!nextTransport->open(requestedPort, static_cast<unsigned>(baud), error))
    {
        return {400, "{\"ok\":false,\"error\":\"" + jsonEscape(error) + "\"}"};
    }

    stopReader();
    {
        std::lock_guard<std::mutex> lock(transportMutex_);
        transport_ = std::move(nextTransport);
        transportMode_ = mode;
        portName_ = requestedPort;
        baudRate_ = static_cast<unsigned>(baud);
    }
    startReader();
    addLog("Opened " + mode + " transport on " + requestedPort + ".");
    return {200, "{\"ok\":true," + portStatusJson().substr(1U)};
}

ApiServer::HttpResponse ApiServer::closePort()
{
    stopReader();
    {
        std::lock_guard<std::mutex> lock(transportMutex_);
        if (transport_)
        {
            transport_->close();
        }
        transport_.reset();
        transportMode_ = "none";
        portName_.clear();
    }
    addLog("Transport closed.");
    return {200, "{\"ok\":true}"};
}

ApiServer::HttpResponse ApiServer::sendMessage(const std::string& body)
{
    std::string type;
    if (!extractString(body, "type", type))
    {
        return {400, "{\"ok\":false,\"error\":\"type is required\"}"};
    }

    std::vector<uint8_t> payload;
    std::string error;
    if (!registry_.buildPayload(type, extractPayloadValues(body), payload, error))
    {
        return {400, "{\"ok\":false,\"error\":\"" + jsonEscape(error) + "\"}"};
    }

    const auto frame = protocol::buildFrame(payload);
    {
        std::lock_guard<std::mutex> lock(transportMutex_);
        if (!transport_ || !transport_->isOpen())
        {
            return {400, "{\"ok\":false,\"error\":\"transport is not open\"}"};
        }

        if (!transport_->write(frame.bytes.data(), frame.bytes.size(), error))
        {
            return {400, "{\"ok\":false,\"error\":\"" + jsonEscape(error) + "\"}"};
        }
    }

    addLog("Sent " + type + " frame: " + protocol::bytesToHex(frame.bytes));
    return {200, "{\"ok\":true,\"frameHex\":\"" + protocol::bytesToHex(frame.bytes) + "\"}"};
}

ApiServer::HttpResponse ApiServer::exportReceived(const std::string& body)
{
    std::string path;
    if (!extractString(body, "path", path))
    {
        path = "output/received_" + nowStamp() + ".txt";
    }

    std::string error;
    if (!recorder_.exportText(path, error))
    {
        return {400, "{\"ok\":false,\"error\":\"" + jsonEscape(error) + "\"}"};
    }

    addLog("Exported received data to " + path + ".");
    return {200, "{\"ok\":true,\"path\":\"" + jsonEscape(path) + "\"}"};
}

ApiServer::HttpResponse ApiServer::runTest(const std::string& body)
{
    std::string testId;
    std::string source;
    extractString(body, "testId", testId);
    extractString(body, "source", source);

    if (source.empty())
    {
        source = "mock";
    }

    transport::ITransport* current = nullptr;
    {
        std::lock_guard<std::mutex> lock(transportMutex_);
        current = transport_.get();
    }

    const auto status = testRunner_.run(testId, source, current);
    addLog("Test " + testId + " -> " + status.state + ": " + status.message);
    return {200, "{\"ok\":" + boolJson(status.passed) + ",\"state\":\"" + status.state + "\",\"message\":\"" + jsonEscape(status.message) + "\"}"};
}

std::string ApiServer::healthJson() const
{
    return "{\"ok\":true,\"service\":\"usart-test-backend\"}";
}

std::string ApiServer::messagesJson() const
{
    std::ostringstream out;
    out << "{\"messages\":[";
    const auto& descriptions = registry_.descriptions();
    for (size_t i = 0U; i < descriptions.size(); ++i)
    {
        const auto& desc = descriptions[i];
        if (i != 0U)
        {
            out << ',';
        }
        out << "{\"type\":\"" << desc.type << "\",\"header\":" << static_cast<unsigned>(desc.header) << ",\"fields\":[";
        for (size_t j = 0U; j < desc.fields.size(); ++j)
        {
            const auto& field = desc.fields[j];
            if (j != 0U)
            {
                out << ',';
            }
            out << "{\"name\":\"" << field.name << "\",\"type\":\"" << field.type
                << "\",\"min\":" << field.minValue << ",\"max\":" << field.maxValue << '}';
        }
        out << "]}";
    }
    out << "]}";
    return out.str();
}

std::string ApiServer::portStatusJson() const
{
    std::lock_guard<std::mutex> lock(transportMutex_);
    const bool open = transport_ && transport_->isOpen();
    std::ostringstream out;
    out << "{\"open\":" << boolJson(open)
        << ",\"mode\":\"" << transportMode_
        << "\",\"port\":\"" << jsonEscape(portName_)
        << "\",\"baud\":" << baudRate_
        << ",\"readerRunning\":" << boolJson(readerRunning_)
        << ",\"receivedBytes\":" << recorder_.totalReceived()
        << '}';
    return out.str();
}

std::string ApiServer::receivedJson() const
{
    const auto entries = recorder_.snapshot(200U);
    std::ostringstream out;
    out << "{\"totalReceived\":" << recorder_.totalReceived() << ",\"buffered\":" << recorder_.size() << ",\"entries\":[";
    for (size_t i = 0U; i < entries.size(); ++i)
    {
        const auto& entry = entries[i];
        if (i != 0U)
        {
            out << ',';
        }
        const char ascii = (entry.value >= 32U && entry.value <= 126U) ? static_cast<char>(entry.value) : '.';
        out << "{\"index\":" << entry.index
            << ",\"timestampMs\":" << entry.timestampMs
            << ",\"hex\":\"";
        out << std::uppercase << std::hex << std::setfill('0') << std::setw(2) << static_cast<unsigned>(entry.value);
        out << std::dec << "\",\"ascii\":\"" << ascii << "\"}";
    }
    out << "]}";
    return out.str();
}

std::string ApiServer::logsJson() const
{
    std::lock_guard<std::mutex> lock(logMutex_);
    std::ostringstream out;
    out << "{\"logs\":[";
    for (size_t i = 0U; i < logs_.size(); ++i)
    {
        if (i != 0U)
        {
            out << ',';
        }
        out << '"' << jsonEscape(logs_[i]) << '"';
    }
    out << "]}";
    return out.str();
}

std::string ApiServer::testsJson() const
{
    const auto tests = testRunner_.listTests();
    std::ostringstream out;
    out << "{\"tests\":[";
    for (size_t i = 0U; i < tests.size(); ++i)
    {
        if (i != 0U)
        {
            out << ',';
        }
        out << "{\"id\":\"" << tests[i].id << "\",\"name\":\"" << tests[i].name << "\",\"source\":\"" << tests[i].source << "\"}";
    }
    out << "]}";
    return out.str();
}

std::string ApiServer::testStatusJson() const
{
    const auto status = testRunner_.status();
    return "{\"id\":\"" + jsonEscape(status.id) + "\",\"state\":\"" + status.state + "\",\"passed\":" +
           boolJson(status.passed) + ",\"message\":\"" + jsonEscape(status.message) + "\"}";
}

void ApiServer::startReader()
{
    readerRunning_ = true;
    readerThread_ = std::thread([this]() {
        uint8_t buffer[256] = {};
        while (readerRunning_)
        {
            transport::ITransport* current = nullptr;
            {
                std::lock_guard<std::mutex> lock(transportMutex_);
                current = transport_.get();
            }

            if (current == nullptr || !current->isOpen())
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(25));
                continue;
            }

            std::string error;
            const size_t count = current->read(buffer, sizeof(buffer), error);
            if (count > 0U)
            {
                recorder_.append(buffer, count);
                if (current->name() == "mock")
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
            }
            else
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(25));
            }
        }
    });
}

void ApiServer::stopReader()
{
    readerRunning_ = false;
    if (readerThread_.joinable())
    {
        readerThread_.join();
    }
}

void ApiServer::addLog(const std::string& message)
{
    std::lock_guard<std::mutex> lock(logMutex_);
    logs_.push_back(message);
    if (logs_.size() > 200U)
    {
        logs_.erase(logs_.begin());
    }
}
}
