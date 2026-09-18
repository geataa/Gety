#include "Crypto.h"

namespace Gety {

std::string Crypto::Base64Encode(const std::vector<uint8_t>& data) {
    if (data.empty()) return "";
    DWORD len = 0;
    if (!CryptBinaryToStringA(data.data(), (DWORD)data.size(), CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, NULL, &len)) {
        return "";
    }
    std::string out(len, '\0');
    if (CryptBinaryToStringA(data.data(), (DWORD)data.size(), CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, &out[0], &len)) {
        while (!out.empty() && (out.back() == '\0' || out.back() == '\r' || out.back() == '\n')) {
            out.pop_back();
        }
        return out;
    }
    return "";
}

std::string Crypto::Base64Encode(const std::string& str) {
    std::vector<uint8_t> data(str.begin(), str.end());
    return Base64Encode(data);
}

std::vector<uint8_t> Crypto::Base64Decode(const std::string& b64) {
    if (b64.empty()) return {};
    DWORD len = 0;
    if (!CryptStringToBinaryA(b64.c_str(), (DWORD)b64.length(), CRYPT_STRING_BASE64, NULL, &len, NULL, NULL)) {
        return {};
    }
    std::vector<uint8_t> out(len);
    if (CryptStringToBinaryA(b64.c_str(), (DWORD)b64.length(), CRYPT_STRING_BASE64, out.data(), &len, NULL, NULL)) {
        out.resize(len);
        return out;
    }
    return {};
}

std::string Crypto::Base64DecodeToString(const std::string& b64) {
    auto vec = Base64Decode(b64);
    if (vec.empty()) return "";
    return std::string((char*)vec.data(), vec.size());
}

std::string Crypto::EncryptDPAPI(const std::string& plaintext, const std::wstring& description) {
    if (plaintext.empty()) return "";
    DATA_BLOB inBlob;
    inBlob.pbData = (BYTE*)plaintext.data();
    inBlob.cbData = (DWORD)plaintext.size();

    DATA_BLOB outBlob = { 0, NULL };
    if (CryptProtectData(&inBlob, description.c_str(), NULL, NULL, NULL, CRYPTPROTECT_UI_FORBIDDEN, &outBlob)) {
        std::vector<uint8_t> cipher(outBlob.pbData, outBlob.pbData + outBlob.cbData);
        LocalFree(outBlob.pbData);
        return Base64Encode(cipher);
    }
    return EncryptPortable(plaintext);
}

std::string Crypto::DecryptDPAPI(const std::string& b64Ciphertext) {
    if (b64Ciphertext.empty()) return "";
    std::vector<uint8_t> cipher = Base64Decode(b64Ciphertext);
    if (cipher.empty()) return "";

    DATA_BLOB inBlob;
    inBlob.pbData = cipher.data();
    inBlob.cbData = (DWORD)cipher.size();

    DATA_BLOB outBlob = { 0, NULL };
    if (CryptUnprotectData(&inBlob, NULL, NULL, NULL, NULL, CRYPTPROTECT_UI_FORBIDDEN, &outBlob)) {
        std::string plain((char*)outBlob.pbData, outBlob.cbData);
        LocalFree(outBlob.pbData);
        return plain;
    }
    return DecryptPortable(b64Ciphertext);
}

std::string Crypto::EncryptPortable(const std::string& plaintext, const std::string& key) {
    if (plaintext.empty()) return "";
    std::vector<uint8_t> out(plaintext.size());
    size_t klen = key.empty() ? 1 : key.length();
    for (size_t i = 0; i < plaintext.size(); ++i) {
        uint8_t k = (uint8_t)(key[i % klen] ^ ((i * 37 + 13) & 0xFF));
        out[i] = (uint8_t)plaintext[i] ^ k;
    }
    return Base64Encode(out);
}

std::string Crypto::DecryptPortable(const std::string& b64Ciphertext, const std::string& key) {
    if (b64Ciphertext.empty()) return "";
    std::vector<uint8_t> cipher = Base64Decode(b64Ciphertext);
    if (cipher.empty()) return "";
    std::string out(cipher.size(), '\0');
    size_t klen = key.empty() ? 1 : key.length();
    for (size_t i = 0; i < cipher.size(); ++i) {
        uint8_t k = (uint8_t)(key[i % klen] ^ ((i * 37 + 13) & 0xFF));
        out[i] = (char)(cipher[i] ^ k);
    }
    return out;
}

} // namespace Gety
