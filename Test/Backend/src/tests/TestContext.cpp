#include "tests/TestContext.h"

#include <chrono>
#include <thread>
#include <utility>

namespace tests
{
TestChannel::TestChannel(channel::ChannelManager& channels,
                         std::string channelName,
                         const std::atomic<bool>& stopRequested)
    : channels_(channels),
      channelName_(std::move(channelName)),
      stopRequested_(stopRequested)
{
}

const std::string& TestChannel::name() const
{
    return channelName_;
}

bool TestChannel::exists() const
{
    return channels_.hasChannel(channelName_);
}

bool TestChannel::sendMessage(const std::string& type,
                              const std::map<std::string, double>& payload,
                              std::string& error)
{
    return channels_.sendMessage(channelName_, type, payload, error);
}

std::vector<uint8_t> TestChannel::snapshotRaw() const
{
    return channels_.rawValues(channelName_);
}

uint64_t TestChannel::rawTotal() const
{
    return channels_.rawTotal(channelName_);
}

std::vector<recorder::ParsedMessage> TestChannel::snapshotParsed(const std::string& type) const
{
    const auto messages = channels_.parsedSnapshot(channelName_);
    if (type.empty())
    {
        return messages;
    }

    std::vector<recorder::ParsedMessage> filtered;
    for (const auto& message : messages)
    {
        if (message.type == type)
        {
            filtered.push_back(message);
        }
    }
    return filtered;
}

uint64_t TestChannel::parsedTotal() const
{
    return channels_.parsedTotal(channelName_);
}

bool TestChannel::waitForParsed(const std::string& type,
                                uint64_t sinceIndex,
                                uint32_t timeoutMs,
                                recorder::ParsedMessage& message)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);

    while (!stopRequested_ && std::chrono::steady_clock::now() < deadline)
    {
        const auto messages = channels_.parsedSnapshot(channelName_, 100U);
        for (const auto& item : messages)
        {
            if (item.index >= sinceIndex && (type.empty() || item.type == type))
            {
                message = item;
                return true;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    return false;
}

TestContext::TestContext(const std::string& source,
                         const TestParams& params,
                         channel::ChannelManager& channels,
                         const std::atomic<bool>& stopRequested,
                         std::function<void(const std::string&)> logger)
    : source_(source),
      params_(params),
      channels_(channels),
      stopRequested_(stopRequested),
      logger_(std::move(logger))
{
}

const std::string& TestContext::source() const
{
    return source_;
}

bool TestContext::stopRequested() const
{
    return stopRequested_;
}

void TestContext::log(const std::string& message) const
{
    if (logger_)
    {
        logger_(message);
    }
}

double TestContext::paramNumber(const std::string& name, double defaultValue) const
{
    const auto it = params_.find(name);
    return it == params_.end() ? defaultValue : it->second;
}

TestChannel TestContext::channel(const std::string& channelName) const
{
    return TestChannel(channels_, channelName, stopRequested_);
}

bool TestContext::sendMessage(const std::string& type,
                              const std::map<std::string, double>& payloadValues,
                              std::string& error)
{
    return channel(channel::DefaultChannelName).sendMessage(type, payloadValues, error);
}

std::vector<uint8_t> TestContext::snapshotRaw() const
{
    return channel(channel::DefaultChannelName).snapshotRaw();
}

uint64_t TestContext::rawTotal() const
{
    return channel(channel::DefaultChannelName).rawTotal();
}

std::vector<recorder::ParsedMessage> TestContext::snapshotParsed(const std::string& type) const
{
    return channel(channel::DefaultChannelName).snapshotParsed(type);
}

uint64_t TestContext::parsedTotal() const
{
    return channel(channel::DefaultChannelName).parsedTotal();
}

bool TestContext::waitForParsed(const std::string& type,
                                uint64_t sinceIndex,
                                uint32_t timeoutMs,
                                recorder::ParsedMessage& message)
{
    return channel(channel::DefaultChannelName).waitForParsed(type, sinceIndex, timeoutMs, message);
}
}
