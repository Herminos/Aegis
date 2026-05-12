#pragma once

#define CURRENT_VERSION "Alpha 0.1.0"

#include<vector>
#include<string>
#include<sodium.h>

std::string bin_to_base64(const std::vector<uint8_t>& bin) {

    size_t b64_len = sodium_base64_encoded_len(bin.size(), sodium_base64_VARIANT_ORIGINAL);
    
    std::string b64_str;
    b64_str.resize(b64_len);

    sodium_bin2base64(b64_str.data(), b64_str.size(), bin.data(), bin.size(), sodium_base64_VARIANT_ORIGINAL);

    if (!b64_str.empty() && b64_str.back() == '\0') {
        b64_str.pop_back();
    }
    return b64_str;
}

std::vector<uint8_t> base64_to_bin(const std::string& b64) {
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