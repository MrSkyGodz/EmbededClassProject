#include "api/ApiServer.h"

#include "protocol/Frame.h"
#include "transport/TransportFactory.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cerrno>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <fstream>
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

std::string pathOnly(const std::string& path)
{
    const size_t queryPos = path.find('?');
    if (queryPos == std::string::npos)
    {
        return path;
    }

    return path.substr(0U, queryPos);
}

bool querySizeT(const std::string& path, const std::string& key, size_t& value)
{
    const size_t queryPos = path.find('?');
    if (queryPos == std::string::npos)
    {
        return false;
    }

    const std::string token = key + "=";
    size_t pos = path.find(token, queryPos + 1U);
    if (pos == std::string::npos)
    {
        return false;
    }

    pos += token.size();

    char* endPtr = nullptr;
    const unsigned long parsed = std::strtoul(path.c_str() + pos, &endPtr, 10);

    if (endPtr == path.c_str() + pos)
    {
        return false;
    }

    value = static_cast<size_t>(parsed);
    return true;
}

size_t clampSizeT(size_t value, size_t minValue, size_t maxValue)
{
    if (value < minValue)
    {
        return minValue;
    }

    if (value > maxValue)
    {
        return maxValue;
    }

    return value;
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
    while (end < body.size() &&
           (std::isdigit(static_cast<unsigned char>(body[end])) || body[end] == '.' || body[end] == '-'))
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

std::string aliasForField(const std::string& fieldName)
{
    if (fieldName == "motor1AngleDeg")
    {
        return "motor1";
    }
    if (fieldName == "motor2AngleDeg")
    {
        return "motor2";
    }
    return "";
}

std::map<std::string, double> extractPayloadValues(const std::string& body, const protocol::MessageDescription& description)
{
    std::map<std::string, double> values;
    for (const auto& field : description.fields)
    {
        double value = 0.0;
        if (extractNumber(body, field.name, value))
        {
            values[field.name] = value;
            continue;
        }

        const std::string alias = aliasForField(field.name);
        if (!alias.empty() && extractNumber(body, alias, value))
        {
            values[field.name] = value;
        }
    }
    return values;
}

std::map<std::string, double> extractObjectNumbers(const std::string& body, const std::string& objectKey)
{
    std::map<std::string, double> values;
    const std::string token = "\"" + objectKey + "\"";
    size_t pos = body.find(token);
    if (pos == std::string::npos)
    {
        return values;
    }

    pos = body.find('{', pos + token.size());
    if (pos == std::string::npos)
    {
        return values;
    }

    const size_t endObject = body.find('}', pos + 1U);
    if (endObject == std::string::npos)
    {
        return values;
    }

    while (pos < endObject)
    {
        const size_t keyStart = body.find('"', pos + 1U);
        if (keyStart == std::string::npos || keyStart >= endObject)
        {
            break;
        }

        const size_t keyEnd = body.find('"', keyStart + 1U);
        if (keyEnd == std::string::npos || keyEnd >= endObject)
        {
            break;
        }

        const std::string key = body.substr(keyStart + 1U, keyEnd - keyStart - 1U);
        const size_t colon = body.find(':', keyEnd + 1U);
        if (colon == std::string::npos || colon >= endObject)
        {
            break;
        }

        size_t valueStart = colon + 1U;
        while (valueStart < endObject && std::isspace(static_cast<unsigned char>(body[valueStart])))
        {
            ++valueStart;
        }

        size_t valueEnd = valueStart;
        while (valueEnd < endObject &&
               (std::isdigit(static_cast<unsigned char>(body[valueEnd])) || body[valueEnd] == '.' || body[valueEnd] == '-'))
        {
            ++valueEnd;
        }

        if (valueEnd > valueStart)
        {
            values[key] = std::stod(body.substr(valueStart, valueEnd - valueStart));
        }

        pos = valueEnd;
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

bool sendAll(int socketFd, const std::string& text)
{
    const char* data = text.c_str();
    size_t remaining = text.size();

    while (remaining > 0U)
    {
        const int sent = send(socketFd, data, static_cast<int>(remaining), 0);
        if (sent <= 0)
        {
            return false;
        }

        data += sent;
        remaining -= static_cast<size_t>(sent);
    }

    return true;
}
}

namespace api
{
ApiServer::ApiServer(unsigned port)
    : port_(port),
      parser_(registry_, parsedRecorder_, [this](const std::string& error) { addLog(error); }),
      testRunner_(registry_, recorder_, parsedRecorder_),
      startedAt_(std::chrono::steady_clock::now())
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

        std::thread(&ApiServer::handleClient, this, clientFd).detach();
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


void ApiServer::handleClient(int clientFd)
{
    char buffer[16384] = {};
    const int received = static_cast<int>(recv(clientFd, buffer, sizeof(buffer) - 1U, 0));
    if (received <= 0)
    {
        closeSocket(clientFd);
        return;
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

    const std::string cleanPath = pathOnly(request.path);

    if (request.method == "GET" && cleanPath == "/api/events")
    {
        serveEventStream(clientFd);
        closeSocket(clientFd);
        return;
    }

    const auto response = route(request);
    const auto text = responseText(response.status, response.body);
    sendAll(clientFd, text);
    closeSocket(clientFd);
}

void ApiServer::serveEventStream(int clientFd)
{
    std::ostringstream header;
    header << "HTTP/1.1 200 OK\r\n";
    header << "Content-Type: text/event-stream\r\n";
    header << "Cache-Control: no-cache\r\n";
    header << "Connection: keep-alive\r\n";
    header << "Access-Control-Allow-Origin: *\r\n";
    header << "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n";
    header << "Access-Control-Allow-Headers: Content-Type\r\n\r\n";

    if (!sendAll(clientFd, header.str()))
    {
        return;
    }

    bool hasLastParsedIndex = false;
    size_t lastParsedIndex = 0U;
    size_t lastLogCount = 0U;
    unsigned heartbeatCounter = 0U;

    while (serverRunning_)
    {
        const auto parsedMessages = parsedRecorder_.snapshot(100U);

        for (const auto& message : parsedMessages)
        {
            if (!hasLastParsedIndex || message.index > lastParsedIndex)
            {
                std::ostringstream data;
                data << "event: parsed\n";
                data << "data: {";
                data << "\"index\":" << message.index
                     << ",\"timestampMs\":" << message.timestampMs
                     << ",\"timetagMs\":" << message.timetagMs
                     << ",\"counter\":" << static_cast<unsigned>(message.counter)
                     << ",\"icdType\":" << static_cast<unsigned>(message.icdType)
                     << ",\"type\":\"" << jsonEscape(message.type) << "\",\"fields\":{";

                size_t fieldIndex = 0U;
                for (const auto& field : message.values)
                {
                    if (fieldIndex++ != 0U)
                    {
                        data << ',';
                    }

                    data << "\"" << jsonEscape(field.first) << "\":" << field.second;
                }

                data << "}}\n\n";

                if (!sendAll(clientFd, data.str()))
                {
                    return;
                }

                lastParsedIndex = message.index;
                hasLastParsedIndex = true;
            }
        }

        {
            std::lock_guard<std::mutex> lock(logMutex_);

            if (logs_.size() < lastLogCount)
            {
                lastLogCount = 0U;
            }

            for (size_t i = lastLogCount; i < logs_.size(); ++i)
            {
                std::ostringstream data;
                data << "event: log\n";
                data << "data: {\"message\":\"" << jsonEscape(logs_[i]) << "\"}\n\n";

                if (!sendAll(clientFd, data.str()))
                {
                    return;
                }
            }

            lastLogCount = logs_.size();
        }

        if (++heartbeatCounter >= 25U)
        {
            heartbeatCounter = 0U;
            if (!sendAll(clientFd, ": heartbeat\n\n"))
            {
                return;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
}

ApiServer::HttpResponse ApiServer::route(const HttpRequest& request)
{
    const std::string cleanPath = pathOnly(request.path);

    if (request.method == "OPTIONS")
    {
        return {200, "{\"ok\":true}"};
    }
    if (request.method == "GET" && cleanPath == "/api/health")
    {
        return {200, healthJson()};
    }
    if (request.method == "GET" && cleanPath == "/api/messages")
    {
        return {200, messagesJson()};
    }
    if (request.method == "POST" && cleanPath == "/api/ports/open")
    {
        return openPort(request.body);
    }
    if (request.method == "POST" && cleanPath == "/api/ports/close")
    {
        return closePort();
    }
    if (request.method == "GET" && cleanPath == "/api/ports/status")
    {
        return {200, portStatusJson()};
    }
    if (request.method == "POST" && cleanPath == "/api/messages/send")
    {
        return sendMessage(request.body);
    }
    if (request.method == "GET" && cleanPath == "/api/received")
    {
        size_t entryLimit = 0U;
        size_t parsedLimit = 60U;

        querySizeT(request.path, "entryLimit", entryLimit);
        querySizeT(request.path, "parsedLimit", parsedLimit);

        entryLimit = clampSizeT(entryLimit, 0U, 200U);
        parsedLimit = clampSizeT(parsedLimit, 0U, 100U);

        return {200, receivedJson(entryLimit, parsedLimit)};
    }
    if (request.method == "POST" && cleanPath == "/api/received/clear")
    {
        recorder_.clear();
        parsedRecorder_.clear();
        addLog("Received buffer cleared.");
        return {200, "{\"ok\":true}"};
    }
    if (request.method == "POST" && cleanPath == "/api/received/export")
    {
        return exportReceived(request.body);
    }
    if (request.method == "GET" && cleanPath == "/api/logs")
    {
        size_t limit = 80U;

        querySizeT(request.path, "limit", limit);
        limit = clampSizeT(limit, 0U, 200U);

        return {200, logsJson(limit)};
    }
    if (request.method == "GET" && cleanPath == "/api/tests")
    {
        return {200, testsJson()};
    }
    if (request.method == "POST" && cleanPath == "/api/tests/run")
    {
        return runTest(request.body);
    }
    if (request.method == "POST" && cleanPath == "/api/tests/stop")
    {
        const auto status = testRunner_.stop();
        return {200, "{\"ok\":true,\"state\":\"" + status.state + "\",\"message\":\"" + jsonEscape(status.message) + "\"}"};
    }
    if (request.method == "GET" && cleanPath == "/api/tests/status")
    {
        return {200, testStatusJson()};
    }

    return {400, "{\"ok\":false,\"error\":\"unknown endpoint\"}"};
}

ApiServer::HttpResponse ApiServer::openPort(const std::string& body)
{
    testRunner_.stop();

    std::string mode = "mock";
    std::string requestedPort = "mock";
    double baud = 115200.0;
    extractString(body, "mode", mode);
    extractString(body, "port", requestedPort);
    extractNumber(body, "baud", baud);

    auto transportResult = transport::createTransport(mode);
    mode = transportResult.mode;
    auto nextTransport = std::move(transportResult.transport);

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
    parser_.reset();
    startReader();
    addLog("Opened " + mode + " transport on " + requestedPort + ".");
    return {200, "{\"ok\":true," + portStatusJson().substr(1U)};
}

ApiServer::HttpResponse ApiServer::closePort()
{
    testRunner_.stop();

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
    const auto elapsed = std::chrono::steady_clock::now() - startedAt_;
    const auto timetagMs = static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count());
    const uint8_t counter = txCounter_++;
    const auto* description = registry_.findByType(type);
    if (description == nullptr)
    {
        return {400, "{\"ok\":false,\"error\":\"unknown message type\"}"};
    }

    if (!registry_.buildPayload(type, extractPayloadValues(body, *description), timetagMs, counter, payload, error))
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

    std::ofstream parsedFile(path, std::ios::app);
    if (parsedFile)
    {
        parsedFile << "\nparsed_index,timestamp_ms,timetag_ms,counter,icd_type,type,fields\n";
        for (const auto& message : parsedRecorder_.snapshot())
        {
            parsedFile << message.index << ',' << message.timestampMs << ',' << message.timetagMs << ','
                       << static_cast<unsigned>(message.counter) << ',' << static_cast<unsigned>(message.icdType) << ','
                       << message.type << ',';
            bool first = true;
            for (const auto& field : message.values)
            {
                if (!first)
                {
                    parsedFile << ';';
                }
                first = false;
                parsedFile << field.first << '=' << field.second;
            }
            parsedFile << '\n';
        }
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

    const auto params = extractObjectNumbers(body, "params");

    transport::ITransport* current = nullptr;
    {
        std::lock_guard<std::mutex> lock(transportMutex_);
        current = transport_.get();
    }

    const auto status = testRunner_.run(testId, source, params, current, [this](const std::string& message) { addLog(message); });
    addLog("Test " + testId + " -> " + status.state + ": " + status.message);
    const bool accepted = status.state == "running" || status.passed;
    return {200, "{\"ok\":" + boolJson(accepted) + ",\"id\":\"" + jsonEscape(status.id)
                     + "\",\"state\":\"" + status.state + "\",\"message\":\"" + jsonEscape(status.message) + "\"}"};
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
        out << "{\"type\":\"" << desc.type << "\",\"icdType\":" << static_cast<unsigned>(desc.icdType)
            << ",\"direction\":\"" << desc.direction << "\",\"fields\":[";
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
        << ",\"parsedMessages\":" << parsedRecorder_.totalParsed()
        << '}';
    return out.str();
}

std::string ApiServer::receivedJson(size_t entryLimit, size_t parsedLimit) const
{
    const auto entries = recorder_.snapshot(entryLimit);
    const auto parsedMessages = parsedRecorder_.snapshot(parsedLimit);

    std::ostringstream out;
    out << "{\"totalReceived\":" << recorder_.totalReceived()
        << ",\"buffered\":" << recorder_.size()
        << ",\"entries\":[";

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

        out << std::uppercase << std::hex << std::setfill('0') << std::setw(2)
            << static_cast<unsigned>(entry.value);

        out << std::dec << "\",\"ascii\":\"" << ascii << "\"}";
    }

    out << "],\"parsedTotal\":" << parsedRecorder_.totalParsed()
        << ",\"parsedMessages\":[";

    for (size_t i = 0U; i < parsedMessages.size(); ++i)
    {
        const auto& message = parsedMessages[i];
        if (i != 0U)
        {
            out << ',';
        }

        out << "{\"index\":" << message.index
            << ",\"timestampMs\":" << message.timestampMs
            << ",\"timetagMs\":" << message.timetagMs
            << ",\"counter\":" << static_cast<unsigned>(message.counter)
            << ",\"icdType\":" << static_cast<unsigned>(message.icdType)
            << ",\"type\":\"" << jsonEscape(message.type) << "\",\"fields\":{";

        size_t fieldIndex = 0U;
        for (const auto& field : message.values)
        {
            if (fieldIndex++ != 0U)
            {
                out << ',';
            }
            out << "\"" << jsonEscape(field.first) << "\":" << field.second;
        }

        out << "}}";
    }

    out << "]}";
    return out.str();
}

std::string ApiServer::logsJson(size_t limit) const
{
    std::lock_guard<std::mutex> lock(logMutex_);

    const size_t logCount = logs_.size();
    const size_t startIndex = logCount > limit ? logCount - limit : 0U;

    std::ostringstream out;
    out << "{\"logs\":[";

    for (size_t i = startIndex; i < logCount; ++i)
    {
        if (i != startIndex)
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
        out << "{\"id\":\"" << tests[i].id
            << "\",\"name\":\"" << tests[i].name
            << "\",\"source\":\"" << tests[i].source
            << "\",\"allowedSources\":[";

        for (size_t j = 0U; j < tests[i].allowedSources.size(); ++j)
        {
            if (j != 0U)
            {
                out << ',';
            }
            out << '"' << jsonEscape(tests[i].allowedSources[j]) << '"';
        }

        out << "],\"parameters\":[";

        for (size_t j = 0U; j < tests[i].parameters.size(); ++j)
        {
            const auto& parameter = tests[i].parameters[j];
            if (j != 0U)
            {
                out << ',';
            }

            out << "{\"name\":\"" << jsonEscape(parameter.name)
                << "\",\"label\":\"" << jsonEscape(parameter.label)
                << "\",\"type\":\"" << jsonEscape(parameter.type)
                << "\",\"defaultValue\":" << parameter.defaultValue
                << ",\"min\":" << parameter.minValue
                << ",\"max\":" << parameter.maxValue
                << '}';
        }

        out << "]}";
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
                for (size_t i = 0U; i < count; ++i)
                {
                    parser_.pushByte(buffer[i]);
                }
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
