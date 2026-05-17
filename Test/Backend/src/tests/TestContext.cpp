#include "tests/TestContext.h"

#include "protocol/Frame.h"

#include <chrono>
#include <thread>

namespace tests
{
TestContext::TestContext(const std::string& source,
                         const TestParams& params,
                         protocol::MessageRegistry& registry,
                         recorder::ReceiveRecorder& rawRecorder,
                         recorder::ParsedMessageRecorder& parsedRecorder,
                         transport::ITransport* transport,
                         const std::atomic<bool>& stopRequested,
                         std::function<void(const std::string&)> logger)
    : source_(source),
      params_(params),
      registry_(registry),
      rawRecorder_(rawRecorder),
      parsedRecorder_(parsedRecorder),
      transport_(transport),
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

bool TestContext::sendMessage(const std::string& type,
                              const std::map<std::string, double>& payloadValues,
                              std::string& error)
{
    if (transport_ == nullptr || !transport_->isOpen())
    {
        error = "Transport is not open.";
        return false;
    }

    std::vector<uint8_t> payload;
    if (!registry_.buildPayload(type, payloadValues, 0U, 0U, payload, error))
    {
        return false;
    }

    const auto frame = protocol::buildFrame(payload);
    if (!protocol::startsWithFrameHeader(frame.bytes))
    {
        error = "Frame header is invalid.";
        return false;
    }

    return transport_->write(frame.bytes.data(), frame.bytes.size(), error);
}

std::vector<uint8_t> TestContext::snapshotRaw() const
{
    return rawRecorder_.snapshotValues();
}

uint64_t TestContext::rawTotal() const
{
    return rawRecorder_.totalReceived();
}

std::vector<recorder::ParsedMessage> TestContext::snapshotParsed(const std::string& type) const
{
    const auto messages = parsedRecorder_.snapshot();
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

uint64_t TestContext::parsedTotal() const
{
    return parsedRecorder_.totalParsed();
}

bool TestContext::waitForParsed(const std::string& type,
                                uint64_t sinceIndex,
                                uint32_t timeoutMs,
                                recorder::ParsedMessage& message)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);

    while (!stopRequested_ && std::chrono::steady_clock::now() < deadline)
    {
        const auto messages = parsedRecorder_.snapshot(100U);
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
}
