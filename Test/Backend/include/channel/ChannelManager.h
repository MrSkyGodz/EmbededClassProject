#pragma once

#include "protocol/IcdOnlineParser.h"
#include "protocol/MessageRegistry.h"
#include "recorder/ParsedMessageRecorder.h"
#include "recorder/ReceiveRecorder.h"
#include "transport/ITransport.h"

#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace channel
{
constexpr const char* DefaultChannelName = "default";

struct ChannelStatus
{
    std::string name;
    bool open = false;
    std::string mode;
    std::string port;
    unsigned baud = 115200U;
    bool readerRunning = false;
    uint64_t receivedBytes = 0U;
    uint64_t parsedMessages = 0U;
};

class ChannelManager
{
public:
    using LogCallback = std::function<void(const std::string&)>;

    ChannelManager(protocol::MessageRegistry& registry, LogCallback logger);
    ~ChannelManager();

    bool openChannel(const std::string& name,
                     const std::string& mode,
                     const std::string& port,
                     unsigned baud,
                     std::string& error);
    void closeChannel(const std::string& name);
    void closeAll();
    void clearChannel(const std::string& name);
    void clearAll();

    bool hasChannel(const std::string& name) const;
    bool sendMessage(const std::string& channelName,
                     const std::string& type,
                     const std::map<std::string, double>& values,
                     std::string& error);
    bool writeBytes(const std::string& channelName,
                    const uint8_t* data,
                    size_t length,
                    std::string& error);

    ChannelStatus status(const std::string& name) const;
    std::vector<ChannelStatus> statuses() const;

    std::vector<recorder::ReceivedByte> rawSnapshot(const std::string& name, size_t maxCount = 0U) const;
    std::vector<uint8_t> rawValues(const std::string& name) const;
    uint64_t rawTotal(const std::string& name) const;
    size_t rawSize(const std::string& name) const;

    std::vector<recorder::ParsedMessage> parsedSnapshot(const std::string& name, size_t maxCount = 0U) const;
    uint64_t parsedTotal(const std::string& name) const;

private:
    struct Channel;

    Channel* findChannelLocked(const std::string& name) const;
    Channel& ensureChannelLocked(const std::string& name);
    void startReader(Channel& channel);
    void stopReader(Channel& channel);
    void log(const std::string& message) const;

    protocol::MessageRegistry& registry_;
    LogCallback logger_;
    mutable std::mutex mutex_;
    std::map<std::string, std::unique_ptr<Channel>> channels_;
};
}
