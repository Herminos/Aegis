#pragma once

#define CURRENT_VERSION "Alpha 1.0.0"

#include<vector>
#include<string>
#include<sodium.h>
#include<stdexcept>

using namespace std;

const std::string RESET   = "\033[0m";
const std::string RED     = "\033[31m";
const std::string GREEN   = "\033[32m";
const std::string YELLOW  = "\033[33m";
const std::string BLUE    = "\033[34m";
const std::string CYAN    = "\033[36m";
const std::string BOLD    = "\033[1m";

inline std::string bin_to_base64(const std::vector<uint8_t>& bin) {

    size_t b64_len = sodium_base64_encoded_len(bin.size(), sodium_base64_VARIANT_ORIGINAL);
    
    std::string b64_str;
    b64_str.resize(b64_len);

    sodium_bin2base64(b64_str.data(), b64_str.size(), bin.data(), bin.size(), sodium_base64_VARIANT_ORIGINAL);

    if (!b64_str.empty() && b64_str.back() == '\0') {
        b64_str.pop_back();
    }
    return b64_str;
}

inline std::vector<uint8_t> base64_to_bin(const std::string& b64) {
    if (b64.empty()) {
        return {};
    }

    std::vector<uint8_t> bin(b64.size());
    size_t actual_bin_len = 0;

    int result = sodium_base642bin(
        bin.data(),
        bin.size(),
        b64.data(),
        b64.size(),
        nullptr,
        &actual_bin_len,
        nullptr,
        sodium_base64_VARIANT_ORIGINAL
    );

    if (result != 0) {
        throw std::runtime_error("[Encryptor Error] Invalid Base64 string received.");
    }

    bin.resize(actual_bin_len);
    
    return bin;
}

inline uint16_t calculate_header_crc16(const uint8_t* data, size_t length) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < length; ++i) {
        crc ^= (static_cast<uint16_t>(data[i]) << 8);
        for (int j = 0; j < 8; ++j) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

inline void log_error(const string& error) {
    printf("%s\n", (RED + error + RESET).c_str()); 
}
inline void log_warning(const string& warning) {
    printf("%s\n", (YELLOW + warning + RESET).c_str());
}
inline void log_info(const string& info) {
    //printf("%s\n", info.c_str());
}
inline void print_info(const string& info) {
    printf("%s\n", info.c_str());
}

inline void success_info(const string& success) {
    printf("%s\n", (GREEN + success + RESET).c_str());
}