#pragma once

#include "protocol/MessageRegistry.h"
#include "protocol/IcdOnlineParser.h"
#include "recorder/ParsedMessageRecorder.h"
#include "recorder/ReceiveRecorder.h"
#include "tests/TestRunner.h"
#include "transport/ITransport.h"

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace api
{
class ApiServer
{
public:
    explicit ApiServer(unsigned port = 8080U);
    ~ApiServer();

    bool run(std::string& error);
    void stop();

private:
    struct HttpRequest
    {
        std::string method;
        std::string path;
        std::string body;
    };

    struct HttpResponse
    {
        int status = 200;
        std::string body;
    };

    HttpResponse route(const HttpRequest& request);
    HttpResponse openPort(const std::string& body);
    HttpResponse closePort();
    HttpResponse sendMessage(const std::string& body);
    HttpResponse exportReceived(const std::string& body);
    HttpResponse runTest(const std::string& body);

    std::string healthJson() const;
    std::string messagesJson() const;
    std::string portStatusJson() const;
    std::string receivedJson() const;
    std::string logsJson() const;
    std::string testsJson() const;
    std::string testStatusJson() const;

    void startReader();
    void stopReader();
    void addLog(const std::string& message);

    unsigned port_;
    std::atomic<bool> serverRunning_{false};
    std::atomic<bool> readerRunning_{false};
    std::thread readerThread_;

    mutable std::mutex transportMutex_;
    std::unique_ptr<transport::ITransport> transport_;
    std::string transportMode_ = "none";
    std::string portName_;
    unsigned baudRate_ = 115200U;

    protocol::MessageRegistry registry_;
    recorder::ReceiveRecorder recorder_;
    recorder::ParsedMessageRecorder parsedRecorder_;
    protocol::IcdOnlineParser parser_;
    tests::TestRunner testRunner_;
    std::chrono::steady_clock::time_point startedAt_;
    uint8_t txCounter_ = 0U;

    mutable std::mutex logMutex_;
    std::vector<std::string> logs_;
};
}
