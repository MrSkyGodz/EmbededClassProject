#pragma once

#include "channel/ChannelManager.h"
#include "tests/TestDefinition.h"

#include <atomic>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace tests
{
class TestChannel
{
public:
    TestChannel(channel::ChannelManager& channels,
                std::string channelName,
                const std::atomic<bool>& stopRequested);

    const std::string& name() const;
    bool exists() const;

    bool sendMessage(const std::string& type,
                     const std::map<std::string, double>& payload,
                     std::string& error);

    std::vector<uint8_t> snapshotRaw() const;
    uint64_t rawTotal() const;
    std::vector<recorder::ParsedMessage> snapshotParsed(const std::string& type = "") const;
    uint64_t parsedTotal() const;

    bool waitForParsed(const std::string& type,
                       uint64_t sinceIndex,
                       uint32_t timeoutMs,
                       recorder::ParsedMessage& message);

private:
    channel::ChannelManager& channels_;
    std::string channelName_;
    const std::atomic<bool>& stopRequested_;
};

class TestContext
{
public:
    TestContext(const std::string& source,
                const TestParams& params,
                channel::ChannelManager& channels,
                const std::atomic<bool>& stopRequested,
                std::function<void(const std::string&)> logger);

    const std::string& source() const;
    bool stopRequested() const;
    void log(const std::string& message) const;
    double paramNumber(const std::string& name, double defaultValue) const;
    TestChannel channel(const std::string& channelName) const;

    bool sendMessage(const std::string& type,
                     const std::map<std::string, double>& payload,
                     std::string& error);

    std::vector<uint8_t> snapshotRaw() const;
    uint64_t rawTotal() const;
    std::vector<recorder::ParsedMessage> snapshotParsed(const std::string& type = "") const;
    uint64_t parsedTotal() const;

    bool waitForParsed(const std::string& type,
                       uint64_t sinceIndex,
                       uint32_t timeoutMs,
                       recorder::ParsedMessage& message);

private:
    std::string source_;
    TestParams params_;
    channel::ChannelManager& channels_;
    const std::atomic<bool>& stopRequested_;
    std::function<void(const std::string&)> logger_;
};
}
