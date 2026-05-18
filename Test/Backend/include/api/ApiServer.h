#pragma once

#include "channel/ChannelManager.h"
#include "protocol/MessageRegistry.h"
#include "tests/TestRunner.h"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <mutex>
#include <string>
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

    void handleClient(int clientFd);
    void serveEventStream(int clientFd);

    HttpResponse route(const HttpRequest& request);
    HttpResponse openPort(const std::string& body);
    HttpResponse closePort();
    HttpResponse openChannel(const std::string& body);
    HttpResponse closeChannel(const std::string& body);
    HttpResponse sendMessage(const std::string& body);
    HttpResponse exportReceived(const std::string& body);
    HttpResponse runTest(const std::string& body);

    std::string healthJson() const;
    std::string messagesJson() const;
    std::string portStatusJson() const;
    std::string channelsStatusJson() const;
    std::string receivedJson(const std::string& channelName, size_t entryLimit, size_t parsedLimit) const;
    std::string logsJson(size_t limit) const;
    std::string testsJson() const;
    std::string testStatusJson() const;

    void addLog(const std::string& message);

    unsigned port_;
    std::atomic<bool> serverRunning_{false};

    protocol::MessageRegistry registry_;
    channel::ChannelManager channels_;
    tests::TestRunner testRunner_;
    std::chrono::steady_clock::time_point startedAt_;
    uint8_t txCounter_ = 0U;

    mutable std::mutex logMutex_;
    std::vector<std::string> logs_;
};
}
