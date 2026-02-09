#include "opcua/OpcUaMultiClientManager.hpp"

#include <chrono>
#include <iostream>
#include <thread>

using namespace opcua;

int main() {
    OpcUaMultiClientManager manager;

    manager.addServer({
        .serverId = "line-a",
        .endpointUrl = "opc.tcp://127.0.0.1:4840",
        .requestTimeoutMs = 3000,
        .reconnectIntervalMs = 1000,
        .maxReconnectIntervalMs = 8000,
    });

    manager.addServer({
        .serverId = "line-b",
        .endpointUrl = "opc.tcp://127.0.0.1:4850",
        .requestTimeoutMs = 3000,
        .reconnectIntervalMs = 1000,
        .maxReconnectIntervalMs = 8000,
    });

    manager.startAll();

    std::this_thread::sleep_for(std::chrono::seconds(2));

    std::vector<NodeReadRequest> readReq {
        {1, "Temperature", false},
        {1, "Pressure", false}
    };

    for (const auto& id : manager.listServers()) {
        auto readResults = manager.batchRead(id, readReq);
        for (const auto& res : readResults) {
            std::cout << "server=" << id
                      << " node=" << res.nodeDisplay
                      << " status=0x" << std::hex << res.statusCode << std::dec
                      << " value=" << res.valueAsString << std::endl;
        }
    }

    std::vector<NodeWriteRequest> writeReq {
        {1, "SetPoint", false, 42.5},
        {1, "Limit", false, 85.0}
    };

    for (const auto& id : manager.listServers()) {
        auto writeResults = manager.batchWrite(id, writeReq);
        for (const auto& res : writeResults) {
            std::cout << "server=" << id
                      << " write node=" << res.nodeDisplay
                      << " status=0x" << std::hex << res.statusCode << std::dec
                      << std::endl;
        }
    }

    std::this_thread::sleep_for(std::chrono::seconds(2));
    manager.stopAll();
    return 0;
}
