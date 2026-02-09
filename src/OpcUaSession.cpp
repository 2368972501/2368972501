#include "opcua/OpcUaSession.hpp"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <sstream>

namespace opcua {

OpcUaSession::OpcUaSession(ServerConfig config)
    : m_config(std::move(config)) {
    m_client = UA_Client_new();
    UA_ClientConfig_setDefault(UA_Client_getConfig(m_client));
    UA_Client_getConfig(m_client)->timeout = m_config.requestTimeoutMs;
}

OpcUaSession::~OpcUaSession() {
    stop();
    if (m_client != nullptr) {
        UA_Client_delete(m_client);
        m_client = nullptr;
    }
}

void OpcUaSession::start() {
    bool expected = false;
    if (!m_running.compare_exchange_strong(expected, true)) {
        return;
    }

    m_worker = std::thread(&OpcUaSession::runLoop, this);
}

void OpcUaSession::stop() {
    bool expected = true;
    if (!m_running.compare_exchange_strong(expected, false)) {
        return;
    }

    if (m_worker.joinable()) {
        m_worker.join();
    }

    std::lock_guard<std::mutex> lock(m_clientMutex);
    disconnectInternal();
}

bool OpcUaSession::isConnected() const {
    return m_connected.load();
}

std::vector<NodeReadResult> OpcUaSession::batchRead(const std::vector<NodeReadRequest>& requests) {
    std::vector<NodeReadResult> results;
    results.reserve(requests.size());

    std::lock_guard<std::mutex> lock(m_clientMutex);
    if (!m_connected.load()) {
        for (const auto& req : requests) {
            results.push_back({req.identifier, {}, UA_STATUSCODE_BADCONNECTIONCLOSED});
        }
        return results;
    }

    UA_ReadRequest readRequest;
    UA_ReadRequest_init(&readRequest);
    readRequest.nodesToReadSize = requests.size();
    readRequest.nodesToRead = static_cast<UA_ReadValueId*>(
        UA_Array_new(readRequest.nodesToReadSize, &UA_TYPES[UA_TYPES_READVALUEID]));

    std::vector<UA_NodeId> nodeIds;
    nodeIds.reserve(requests.size());

    for (std::size_t i = 0; i < requests.size(); ++i) {
        UA_ReadValueId_init(&readRequest.nodesToRead[i]);
        nodeIds.emplace_back(makeNodeId(requests[i].namespaceIndex, requests[i].identifier,
                                        requests[i].numericIdentifier));
        readRequest.nodesToRead[i].nodeId = nodeIds.back();
        readRequest.nodesToRead[i].attributeId = UA_ATTRIBUTEID_VALUE;
    }

    UA_ReadResponse response = UA_Client_Service_read(m_client, readRequest);

    for (std::size_t i = 0; i < requests.size(); ++i) {
        NodeReadResult result;
        result.nodeDisplay = requests[i].identifier;
        if (i < response.resultsSize) {
            result.statusCode = response.results[i].status;
            if (response.results[i].status == UA_STATUSCODE_GOOD) {
                result.valueAsString = variantToString(&response.results[i].value);
            }
        } else {
            result.statusCode = UA_STATUSCODE_BADUNEXPECTEDERROR;
        }
        results.push_back(std::move(result));
    }

    UA_ReadResponse_clear(&response);
    UA_ReadRequest_clear(&readRequest);
    return results;
}

std::vector<NodeWriteResult> OpcUaSession::batchWrite(const std::vector<NodeWriteRequest>& requests) {
    std::vector<NodeWriteResult> results;
    results.reserve(requests.size());

    std::lock_guard<std::mutex> lock(m_clientMutex);
    if (!m_connected.load()) {
        for (const auto& req : requests) {
            results.push_back({req.identifier, UA_STATUSCODE_BADCONNECTIONCLOSED});
        }
        return results;
    }

    UA_WriteRequest writeRequest;
    UA_WriteRequest_init(&writeRequest);
    writeRequest.nodesToWriteSize = requests.size();
    writeRequest.nodesToWrite = static_cast<UA_WriteValue*>(
        UA_Array_new(writeRequest.nodesToWriteSize, &UA_TYPES[UA_TYPES_WRITEVALUE]));

    std::vector<UA_NodeId> nodeIds;
    nodeIds.reserve(requests.size());

    for (std::size_t i = 0; i < requests.size(); ++i) {
        UA_WriteValue_init(&writeRequest.nodesToWrite[i]);
        nodeIds.emplace_back(makeNodeId(requests[i].namespaceIndex, requests[i].identifier,
                                        requests[i].numericIdentifier));
        writeRequest.nodesToWrite[i].nodeId = nodeIds.back();
        writeRequest.nodesToWrite[i].attributeId = UA_ATTRIBUTEID_VALUE;

        UA_Variant_setScalarCopy(
            &writeRequest.nodesToWrite[i].value.value,
            &requests[i].value,
            &UA_TYPES[UA_TYPES_DOUBLE]);
        writeRequest.nodesToWrite[i].value.hasValue = true;
    }

    UA_WriteResponse response = UA_Client_Service_write(m_client, writeRequest);

    for (std::size_t i = 0; i < requests.size(); ++i) {
        NodeWriteResult result;
        result.nodeDisplay = requests[i].identifier;
        result.statusCode = (i < response.resultsSize)
            ? response.results[i]
            : UA_STATUSCODE_BADUNEXPECTEDERROR;
        results.push_back(result);
    }

    UA_WriteResponse_clear(&response);
    UA_WriteRequest_clear(&writeRequest);
    return results;
}

void OpcUaSession::setConnectionCallback(ConnectionCallback callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_connectionCallback = std::move(callback);
}

const ServerConfig& OpcUaSession::config() const {
    return m_config;
}

void OpcUaSession::runLoop() {
    std::uint32_t intervalMs = m_config.reconnectIntervalMs;

    while (m_running.load()) {
        {
            std::lock_guard<std::mutex> lock(m_clientMutex);
            if (!m_connected.load()) {
                if (connectInternal()) {
                    m_connected.store(true);
                    emitConnection(true);
                    intervalMs = m_config.reconnectIntervalMs;
                } else {
                    std::this_thread::sleep_for(std::chrono::milliseconds(intervalMs));
                    intervalMs = std::min(intervalMs * 2, m_config.maxReconnectIntervalMs);
                    continue;
                }
            }

            UA_StatusCode iterCode = UA_Client_run_iterate(m_client, 100);
            if (iterCode != UA_STATUSCODE_GOOD) {
                disconnectInternal();
                m_connected.store(false);
                emitConnection(false);
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

bool OpcUaSession::connectInternal() {
    UA_StatusCode status = UA_Client_connect(m_client, m_config.endpointUrl.c_str());
    return status == UA_STATUSCODE_GOOD;
}

void OpcUaSession::disconnectInternal() {
    UA_Client_disconnect(m_client);
}

void OpcUaSession::emitConnection(bool connected) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    if (m_connectionCallback) {
        m_connectionCallback(m_config.serverId, connected);
    }
}

UA_NodeId OpcUaSession::makeNodeId(std::uint16_t ns, const std::string& identifier, bool numeric) const {
    if (numeric) {
        return UA_NODEID_NUMERIC(ns, static_cast<UA_UInt32>(std::stoul(identifier)));
    }
    return UA_NODEID_STRING_ALLOC(ns, identifier.c_str());
}

std::string OpcUaSession::variantToString(const UA_Variant* variant) {
    if (!UA_Variant_isScalar(variant) || variant->data == nullptr || variant->type == nullptr) {
        return "<unsupported>";
    }

    std::ostringstream os;
    if (variant->type == &UA_TYPES[UA_TYPES_BOOLEAN]) {
        os << (*static_cast<UA_Boolean*>(variant->data) ? "true" : "false");
    } else if (variant->type == &UA_TYPES[UA_TYPES_INT32]) {
        os << *static_cast<UA_Int32*>(variant->data);
    } else if (variant->type == &UA_TYPES[UA_TYPES_UINT32]) {
        os << *static_cast<UA_UInt32*>(variant->data);
    } else if (variant->type == &UA_TYPES[UA_TYPES_DOUBLE]) {
        os << *static_cast<UA_Double*>(variant->data);
    } else if (variant->type == &UA_TYPES[UA_TYPES_STRING]) {
        const UA_String* str = static_cast<UA_String*>(variant->data);
        os << std::string(reinterpret_cast<char*>(str->data), str->length);
    } else {
        os << "<type:" << variant->type->typeName << ">";
    }

    return os.str();
}

} // namespace opcua
