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

    // 安全配置
    // None / Sign / SignAndEncrypt
    std::string securityMode {"None"};
    // 例如: http://opcfoundation.org/UA/SecurityPolicy#None
    std::string securityPolicyUri {
        "http://opcfoundation.org/UA/SecurityPolicy#None"
    };

    // 证书文件路径（启用加密时需提供）
    std::string clientCertificatePath;
    std::string clientPrivateKeyPath;
    std::vector<std::string> trustListPaths;

    // 用户认证配置
    bool useUsernamePassword {false};
    std::string username;
    std::string password;
};

} // namespace opcua
