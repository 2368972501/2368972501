#pragma once

#include "opcua/OpcUaSession.hpp"

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace opcua {

class OpcUaMultiClientManager {
public:
    OpcUaMultiClientManager() = default;
    ~OpcUaMultiClientManager();

    bool addServer(const ServerConfig& config);
    bool removeServer(const std::string& serverId);

    void startAll();
    void stopAll();

    bool isServerConnected(const std::string& serverId) const;
    std::vector<std::string> listServers() const;

    std::vector<NodeReadResult> batchRead(
        const std::string& serverId,
        const std::vector<NodeReadRequest>& requests) const;

    std::vector<NodeWriteResult> batchWrite(
        const std::string& serverId,
        const std::vector<NodeWriteRequest>& requests) const;

private:
    OpcUaSessionPtr getSession(const std::string& serverId) const;

private:
    mutable std::mutex m_sessionsMutex;
    std::unordered_map<std::string, OpcUaSessionPtr> m_sessions;
};

} // namespace opcua
