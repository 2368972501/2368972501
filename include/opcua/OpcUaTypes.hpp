#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace opcua {

struct NodeReadRequest {
    std::uint16_t namespaceIndex {0};
    std::string identifier;
    bool numericIdentifier {false};
};

struct NodeReadResult {
    std::string nodeDisplay;
    std::string valueAsString;
    std::uint32_t statusCode {0};
};

struct NodeWriteRequest {
    std::uint16_t namespaceIndex {0};
    std::string identifier;
    bool numericIdentifier {false};
    double value {0.0};
};

struct NodeWriteResult {
    std::string nodeDisplay;
    std::uint32_t statusCode {0};
};

struct ServerConfig {
    std::string serverId;
    std::string endpointUrl;
    std::uint32_t requestTimeoutMs {3000};
    std::uint32_t reconnectIntervalMs {1000};
    std::uint32_t maxReconnectIntervalMs {8000};
};

} // namespace opcua
