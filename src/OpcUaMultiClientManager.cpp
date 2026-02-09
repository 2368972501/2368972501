#include "opcua/OpcUaMultiClientManager.hpp"

#include <iostream>
#include <stdexcept>

namespace opcua {

OpcUaMultiClientManager::~OpcUaMultiClientManager() {
    stopAll();
}

bool OpcUaMultiClientManager::addServer(const ServerConfig& config) {
    auto session = std::make_shared<OpcUaSession>(config);
    session->setConnectionCallback([](const std::string& serverId, bool connected) {
        std::cout << "[" << serverId << "] " << (connected ? "connected" : "disconnected") << std::endl;
    });

    std::lock_guard<std::mutex> lock(m_sessionsMutex);
    auto [it, inserted] = m_sessions.emplace(config.serverId, std::move(session));
    return inserted;
}

bool OpcUaMultiClientManager::removeServer(const std::string& serverId) {
    OpcUaSessionPtr removed;
    {
        std::lock_guard<std::mutex> lock(m_sessionsMutex);
        auto it = m_sessions.find(serverId);
        if (it == m_sessions.end()) {
            return false;
        }
        removed = it->second;
        m_sessions.erase(it);
    }

    removed->stop();
    return true;
}

void OpcUaMultiClientManager::startAll() {
    std::lock_guard<std::mutex> lock(m_sessionsMutex);
    for (auto& [_, session] : m_sessions) {
        session->start();
    }
}

void OpcUaMultiClientManager::stopAll() {
    std::vector<OpcUaSessionPtr> sessions;
    {
        std::lock_guard<std::mutex> lock(m_sessionsMutex);
        for (auto& [_, session] : m_sessions) {
            sessions.push_back(session);
        }
    }

    for (const auto& session : sessions) {
        session->stop();
    }
}

bool OpcUaMultiClientManager::isServerConnected(const std::string& serverId) const {
    auto session = getSession(serverId);
    return session->isConnected();
}

std::vector<std::string> OpcUaMultiClientManager::listServers() const {
    std::vector<std::string> ids;
    std::lock_guard<std::mutex> lock(m_sessionsMutex);
    ids.reserve(m_sessions.size());

    for (const auto& [id, _] : m_sessions) {
        ids.push_back(id);
    }
    return ids;
}

std::vector<NodeReadResult> OpcUaMultiClientManager::batchRead(
    const std::string& serverId,
    const std::vector<NodeReadRequest>& requests) const {
    auto session = getSession(serverId);
    return session->batchRead(requests);
}

std::vector<NodeWriteResult> OpcUaMultiClientManager::batchWrite(
    const std::string& serverId,
    const std::vector<NodeWriteRequest>& requests) const {
    auto session = getSession(serverId);
    return session->batchWrite(requests);
}

OpcUaSessionPtr OpcUaMultiClientManager::getSession(const std::string& serverId) const {
    std::lock_guard<std::mutex> lock(m_sessionsMutex);
    auto it = m_sessions.find(serverId);
    if (it == m_sessions.end()) {
        throw std::runtime_error("server not registered: " + serverId);
    }
    return it->second;
}

} // namespace opcua
