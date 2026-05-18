#include "channel/ChannelManager.h"

#include "protocol/Frame.h"
#include "transport/TransportFactory.h"

#include <chrono>
#include <utility>

namespace channel
{
struct ChannelManager::Channel
{
    explicit Channel(const std::string& channelName,
                     protocol::MessageRegistry& registry,
                     LogCallback logger)
        : name(channelName),
          parser(registry, parsedRecorder, [channelName, logger](const std::string& error) {
              if (logger)
              {
                  logger("[" + channelName + "] " + error);
              }
          })
    {
    }

    std::string name;
    std::string mode = "none";
    std::string port;
    unsigned baud = 115200U;
    std::unique_ptr<transport::ITransport> transport;
    recorder::ReceiveRecorder rawRecorder;
    recorder::ParsedMessageRecorder parsedRecorder;
    protocol::IcdOnlineParser parser;
    std::thread readerThread;
    std::atomic<bool> readerRunning{false};
};

ChannelManager::ChannelManager(protocol::MessageRegistry& registry, LogCallback logger)
    : registry_(registry),
      logger_(std::move(logger))
{
}

ChannelManager::~ChannelManager()
{
    closeAll();
}

bool ChannelManager::openChannel(const std::string& name,
                                 const std::string& mode,
                                 const std::string& port,
                                 unsigned baud,
                                 std::string& error)
{
    auto transportResult = transport::createTransport(mode);
    auto nextTransport = std::move(transportResult.transport);

    if (!nextTransport->open(port, baud, error))
    {
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    Channel& channel = ensureChannelLocked(name);
    stopReader(channel);
    if (channel.transport)
    {
        channel.transport->close();
    }

    channel.transport = std::move(nextTransport);
    channel.mode = transportResult.mode;
    channel.port = port;
    channel.baud = baud;
    channel.parser.reset();
    startReader(channel);
    log("Opened " + channel.name + " channel as " + channel.mode + " on " + port + ".");
    return true;
}

void ChannelManager::closeChannel(const std::string& name)
{
    std::lock_guard<std::mutex> lock(mutex_);
    Channel* channel = findChannelLocked(name);
    if (channel == nullptr)
    {
        return;
    }

    stopReader(*channel);
    if (channel->transport)
    {
        channel->transport->close();
    }
    channel->transport.reset();
    channel->mode = "none";
    channel->port.clear();
    log("Closed " + channel->name + " channel.");
}

void ChannelManager::closeAll()
{
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& item : channels_)
    {
        stopReader(*item.second);
        if (item.second->transport)
        {
            item.second->transport->close();
        }
        item.second->transport.reset();
        item.second->mode = "none";
        item.second->port.clear();
    }
}

void ChannelManager::clearChannel(const std::string& name)
{
    std::lock_guard<std::mutex> lock(mutex_);
    Channel* channel = findChannelLocked(name);
    if (channel == nullptr)
    {
        return;
    }

    channel->rawRecorder.clear();
    channel->parsedRecorder.clear();
    channel->parser.reset();
}

void ChannelManager::clearAll()
{
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& item : channels_)
    {
        item.second->rawRecorder.clear();
        item.second->parsedRecorder.clear();
        item.second->parser.reset();
    }
}

bool ChannelManager::hasChannel(const std::string& name) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return findChannelLocked(name) != nullptr;
}

bool ChannelManager::sendMessage(const std::string& channelName,
                                 const std::string& type,
                                 const std::map<std::string, double>& values,
                                 std::string& error)
{
    std::vector<uint8_t> payload;
    if (!registry_.buildPayload(type, values, 0U, 0U, payload, error))
    {
        return false;
    }
    const auto frame = protocol::buildFrame(payload);

    std::lock_guard<std::mutex> lock(mutex_);
    Channel* channel = findChannelLocked(channelName);
    if (channel == nullptr)
    {
        error = "channel not found: " + channelName;
        return false;
    }
    if (!channel->transport || !channel->transport->isOpen())
    {
        error = "channel is not open: " + channelName;
        return false;
    }

    return channel->transport->write(frame.bytes.data(), frame.bytes.size(), error);
}

bool ChannelManager::writeBytes(const std::string& channelName,
                                const uint8_t* data,
                                size_t length,
                                std::string& error)
{
    std::lock_guard<std::mutex> lock(mutex_);
    Channel* channel = findChannelLocked(channelName);
    if (channel == nullptr)
    {
        error = "channel not found: " + channelName;
        return false;
    }
    if (!channel->transport || !channel->transport->isOpen())
    {
        error = "channel is not open: " + channelName;
        return false;
    }

    return channel->transport->write(data, length, error);
}

ChannelStatus ChannelManager::status(const std::string& name) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    const Channel* channel = findChannelLocked(name);
    if (channel == nullptr)
    {
        ChannelStatus missing{};
        missing.name = name;
        return missing;
    }

    const bool open = channel->transport && channel->transport->isOpen();
    return {channel->name,
            open,
            channel->mode,
            channel->port,
            channel->baud,
            channel->readerRunning,
            channel->rawRecorder.totalReceived(),
            channel->parsedRecorder.totalParsed()};
}

std::vector<ChannelStatus> ChannelManager::statuses() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<ChannelStatus> result;
    result.reserve(channels_.size());
    for (const auto& item : channels_)
    {
        const Channel& channel = *item.second;
        const bool open = channel.transport && channel.transport->isOpen();
        result.push_back({channel.name,
                          open,
                          channel.mode,
                          channel.port,
                          channel.baud,
                          channel.readerRunning,
                          channel.rawRecorder.totalReceived(),
                          channel.parsedRecorder.totalParsed()});
    }
    return result;
}

std::vector<recorder::ReceivedByte> ChannelManager::rawSnapshot(const std::string& name, size_t maxCount) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    const Channel* channel = findChannelLocked(name);
    return channel == nullptr ? std::vector<recorder::ReceivedByte>{} : channel->rawRecorder.snapshot(maxCount);
}

std::vector<uint8_t> ChannelManager::rawValues(const std::string& name) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    const Channel* channel = findChannelLocked(name);
    return channel == nullptr ? std::vector<uint8_t>{} : channel->rawRecorder.snapshotValues();
}

uint64_t ChannelManager::rawTotal(const std::string& name) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    const Channel* channel = findChannelLocked(name);
    return channel == nullptr ? 0U : channel->rawRecorder.totalReceived();
}

size_t ChannelManager::rawSize(const std::string& name) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    const Channel* channel = findChannelLocked(name);
    return channel == nullptr ? 0U : channel->rawRecorder.size();
}

std::vector<recorder::ParsedMessage> ChannelManager::parsedSnapshot(const std::string& name, size_t maxCount) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    const Channel* channel = findChannelLocked(name);
    return channel == nullptr ? std::vector<recorder::ParsedMessage>{} : channel->parsedRecorder.snapshot(maxCount);
}

uint64_t ChannelManager::parsedTotal(const std::string& name) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    const Channel* channel = findChannelLocked(name);
    return channel == nullptr ? 0U : channel->parsedRecorder.totalParsed();
}

ChannelManager::Channel* ChannelManager::findChannelLocked(const std::string& name) const
{
    const auto it = channels_.find(name);
    return it == channels_.end() ? nullptr : it->second.get();
}

ChannelManager::Channel& ChannelManager::ensureChannelLocked(const std::string& name)
{
    auto it = channels_.find(name);
    if (it == channels_.end())
    {
        auto channel = std::make_unique<Channel>(name, registry_, logger_);
        it = channels_.emplace(name, std::move(channel)).first;
    }
    return *it->second;
}

void ChannelManager::startReader(Channel& channel)
{
    channel.readerRunning = true;
    channel.readerThread = std::thread([this, &channel]() {
        uint8_t buffer[256] = {};
        while (channel.readerRunning)
        {
            if (!channel.transport || !channel.transport->isOpen())
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(25));
                continue;
            }

            std::string error;
            const size_t count = channel.transport->read(buffer, sizeof(buffer), error);
            if (count > 0U)
            {
                channel.rawRecorder.append(buffer, count);
                for (size_t i = 0U; i < count; ++i)
                {
                    channel.parser.pushByte(buffer[i]);
                }
                if (channel.transport->name() == "mock")
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

void ChannelManager::stopReader(Channel& channel)
{
    channel.readerRunning = false;
    if (channel.readerThread.joinable())
    {
        channel.readerThread.join();
    }
}

void ChannelManager::log(const std::string& message) const
{
    if (logger_)
    {
        logger_(message);
    }
}
}
