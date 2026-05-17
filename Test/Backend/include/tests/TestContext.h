#pragma once

#include "protocol/MessageRegistry.h"
#include "recorder/ParsedMessageRecorder.h"
#include "recorder/ReceiveRecorder.h"
#include "tests/TestDefinition.h"
#include "transport/ITransport.h"

#include <atomic>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace tests
{
class TestContext
{
public:
    TestContext(const std::string& source,
                const TestParams& params,
                protocol::MessageRegistry& registry,
                recorder::ReceiveRecorder& rawRecorder,
                recorder::ParsedMessageRecorder& parsedRecorder,
                transport::ITransport* transport,
                const std::atomic<bool>& stopRequested,
                std::function<void(const std::string&)> logger);

    const std::string& source() const;
    bool stopRequested() const;
    void log(const std::string& message) const;
    double paramNumber(const std::string& name, double defaultValue) const;

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
    protocol::MessageRegistry& registry_;
    recorder::ReceiveRecorder& rawRecorder_;
    recorder::ParsedMessageRecorder& parsedRecorder_;
    transport::ITransport* transport_;
    const std::atomic<bool>& stopRequested_;
    std::function<void(const std::string&)> logger_;
};
}
