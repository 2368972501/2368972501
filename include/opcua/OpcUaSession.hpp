#pragma once

#include "opcua/OpcUaTypes.hpp"

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

extern "C" {
#include <open62541/client.h>
#include <open62541/client_config_default.h>
}

namespace opcua {

class OpcUaSession {
public:
    using ConnectionCallback = std::function<void(const std::string&, bool)>;

    explicit OpcUaSession(ServerConfig config);
    ~OpcUaSession();

    OpcUaSession(const OpcUaSession&) = delete;
    OpcUaSession& operator=(const OpcUaSession&) = delete;

    void start();
    void stop();

    bool isConnected() const;
    std::vector<NodeReadResult> batchRead(const std::vector<NodeReadRequest>& requests);
    std::vector<NodeWriteResult> batchWrite(const std::vector<NodeWriteRequest>& requests);

    void setConnectionCallback(ConnectionCallback callback);
    const ServerConfig& config() const;

private:
    void runLoop();
    bool connectInternal();
    void disconnectInternal();
    void emitConnection(bool connected);

    UA_NodeId makeNodeId(std::uint16_t ns, const std::string& identifier, bool numeric) const;
    static std::string variantToString(const UA_Variant* variant);

private:
    ServerConfig m_config;
    UA_Client* m_client {nullptr};

    mutable std::mutex m_clientMutex;
    std::atomic<bool> m_running {false};
    std::atomic<bool> m_connected {false};
    std::thread m_worker;

    std::mutex m_callbackMutex;
    ConnectionCallback m_connectionCallback;
};

using OpcUaSessionPtr = std::shared_ptr<OpcUaSession>;

} // namespace opcua
