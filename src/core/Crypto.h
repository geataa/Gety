#pragma once

#include <windows.h>
#include <wincrypt.h>
#include <string>
#include <vector>
#include <cstdint>

#pragma comment(lib, "Crypt32.lib")

namespace Gety {

class Crypto {
public:
    // Base64 encoding & decoding
    static std::string Base64Encode(const std::vector<uint8_t>& data);
    static std::string Base64Encode(const std::string& str);
    static std::vector<uint8_t> Base64Decode(const std::string& b64);
    static std::string Base64DecodeToString(const std::string& b64);

    // Windows DPAPI (Data Protection API) encryption
    // Uses current logged-in Windows user master key (AES-256 hardware/OS level).
    // Returns Base64-encoded ciphertext.
    static std::string EncryptDPAPI(const std::string& plaintext, const std::wstring& description = L"GetySecureData");
    
    // Decrypts Base64-encoded DPAPI ciphertext back to plaintext.
    static std::string DecryptDPAPI(const std::string& b64Ciphertext);

    // Fallback or portable simple encryption (keyed stream cipher)
    // Useful if data needs to be transferred between different machines
    static std::string EncryptPortable(const std::string& plaintext, const std::string& key = "SkySoft_Gety_SecureKey_2026");
    static std::string DecryptPortable(const std::string& b64Ciphertext, const std::string& key = "SkySoft_Gety_SecureKey_2026");
};

} // namespace Gety
