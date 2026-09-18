#include "OllamaClient.h"
#include "../core/JsonParser.h"
#include "WinHttpUtils.h"
#include <winhttp.h>
#include <sstream>
#include <iomanip>
#include <algorithm>

#pragma comment(lib, "winhttp.lib")

namespace Gety {

std::wstring OllamaClient::Utf8ToWide(const std::string& str) {
    if (str.empty()) return L"";
    int count = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.length(), NULL, 0);
    if (count <= 0) return L"";
    std::wstring result(count, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.length(), &result[0], count);
    return result;
}

std::string OllamaClient::WideToUtf8(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int count = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.length(), NULL, 0, NULL, NULL);
    if (count <= 0) return "";
    std::string result(count, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.length(), &result[0], count, NULL, NULL);
    return result;
}

std::wstring OllamaClient::FormatBytes(uint64_t bytes) {
    wchar_t buf[64];
    if (bytes >= 1024ULL * 1024ULL * 1024ULL) {
        double gb = (double)bytes / (1024.0 * 1024.0 * 1024.0);
        swprintf_s(buf, L"%.2f GB", gb);
    } else if (bytes >= 1024ULL * 1024ULL) {
        double mb = (double)bytes / (1024.0 * 1024.0);
        swprintf_s(buf, L"%.1f MB", mb);
    } else if (bytes >= 1024ULL) {
        double kb = (double)bytes / 1024.0;
        swprintf_s(buf, L"%.0f KB", kb);
    } else {
        swprintf_s(buf, L"%llu B", bytes);
    }
    return buf;
}

static std::wstring TrimWide(const std::wstring& s) {
    size_t start = s.find_first_not_of(L" \t\r\n\"'");
    if (start == std::wstring::npos) return L"";
    size_t end = s.find_last_not_of(L" \t\r\n\"'");
    return s.substr(start, end - start + 1);
}

OllamaModelSpec OllamaClient::ParseModelSpec(const std::wstring& input) {
    OllamaModelSpec spec;
    spec.rawInput = input;

    std::wstring s = TrimWide(input);
    if (s.empty()) return spec;

    // 1. Remove CLI command prefixes (e.g. "ollama run llama3.2", "ollama pull ...")
    const std::wstring prefixes[] = {
        L"ollama run ",
        L"ollama pull ",
        L"ollama-dl ",
        L"ollama://"
    };
    for (const auto& pfx : prefixes) {
        if (s.length() > pfx.length()) {
            std::wstring sub = s.substr(0, pfx.length());
            for (auto& c : sub) c = towlower(c);
            if (sub == pfx) {
                s = TrimWide(s.substr(pfx.length()));
                break;
            }
        }
    }

    // 2. Remove scheme if present (e.g. "ollama:llama3.2")
    if (s.rfind(L"ollama:", 0) == 0) {
        s = TrimWide(s.substr(7));
    }

    // 3. Handle Web URL (e.g. "https://ollama.com/library/nomic-embed-text-v2-moe:latest" or "ollama.com/library/...")
    size_t libPos = s.find(L"ollama.com/library/");
    if (libPos != std::wstring::npos) {
        s = s.substr(libPos + 19);
    } else {
        size_t aiPos = s.find(L"ollama.ai/library/");
        if (aiPos != std::wstring::npos) {
            s = s.substr(aiPos + 18);
        } else {
            size_t hostPos = s.find(L"ollama.com/");
            if (hostPos != std::wstring::npos) {
                s = s.substr(hostPos + 11);
            }
        }
    }

    // Clean any query string or fragment
    size_t qPos = s.find_first_of(L"?#");
    if (qPos != std::wstring::npos) {
        s = s.substr(0, qPos);
    }
    s = TrimWide(s);

    if (s.empty()) return spec;

    // 4. Parse namespace and model:tag
    // Examples:
    // "nomic-embed-text-v2-moe:latest"
    // "nomic-embed-text-v2-moe"
    // "library/llama3.2:1b"
    // "myauthor/mymodel:v1"
    size_t slashPos = s.find(L'/');
    std::wstring modelAndTag;
    if (slashPos != std::wstring::npos) {
        spec.namespaceName = s.substr(0, slashPos);
        modelAndTag = s.substr(slashPos + 1);
    } else {
        spec.namespaceName = L"library";
        modelAndTag = s;
    }

    // Trim namespace
    spec.namespaceName = TrimWide(spec.namespaceName);
    if (spec.namespaceName.empty()) {
        spec.namespaceName = L"library";
    }

    // Parse model name and tag
    size_t colonPos = modelAndTag.rfind(L':');
    if (colonPos != std::wstring::npos) {
        spec.modelName = modelAndTag.substr(0, colonPos);
        spec.tag = modelAndTag.substr(colonPos + 1);
    } else {
        spec.modelName = modelAndTag;
        spec.tag = L"latest";
    }

    spec.modelName = TrimWide(spec.modelName);
    spec.tag = TrimWide(spec.tag);

    if (spec.modelName.empty()) return spec;
    if (spec.tag.empty()) spec.tag = L"latest";

    // Clean invalid characters from modelName and tag
    const wchar_t invalidChars[] = L" \\/:*?\"<>|";
    for (wchar_t ch : invalidChars) {
        spec.modelName.erase(std::remove(spec.modelName.begin(), spec.modelName.end(), ch), spec.modelName.end());
        spec.tag.erase(std::remove(spec.tag.begin(), spec.tag.end(), ch), spec.tag.end());
    }

    if (spec.modelName.empty()) return spec;

    spec.fullName = spec.namespaceName + L"/" + spec.modelName + L":" + spec.tag;
    if (spec.namespaceName == L"library") {
        spec.displayName = spec.modelName + L":" + spec.tag;
    } else {
        spec.displayName = spec.namespaceName + L"/" + spec.modelName + L":" + spec.tag;
    }

    spec.isValid = true;
    return spec;
}

bool OllamaClient::IsLikelyOllamaModel(const std::wstring& input) {
    std::wstring s = TrimWide(input);
    if (s.empty()) return false;

    // Direct url / command signatures
    if (s.find(L"ollama.com/") != std::wstring::npos) return true;
    if (s.find(L"ollama.ai/") != std::wstring::npos) return true;
    if (s.rfind(L"ollama:", 0) == 0) return true;
    if (s.rfind(L"ollama run ", 0) == 0) return true;
    if (s.rfind(L"ollama pull ", 0) == 0) return true;
    if (s.rfind(L"ollama-dl ", 0) == 0) return true;

    // Check common known model keywords
    std::wstring lower = s;
    for (auto& c : lower) c = towlower(c);

    const wchar_t* keywords[] = {
        L"llama", L"mistral", L"gemma", L"deepseek", L"qwen", L"nomic",
        L"phi3", L"phi4", L"starcoder", L"codellama", L"command-r", L"llava",
        L"bge-", L"snowflake-arctic", L"all-minilm", L"tinyllama"
    };
    for (const auto* kw : keywords) {
        if (lower.find(kw) != std::wstring::npos) {
            // If it also doesn't look like a normal web URL with http/https
            if (lower.find(L"http://") == std::wstring::npos && lower.find(L"https://") == std::wstring::npos) {
                return true;
            }
        }
    }

    // Format like "name:tag" where tag looks like a quant or version or "latest"
    size_t colon = lower.find(L':');
    if (colon != std::wstring::npos && colon > 0 && colon < lower.length() - 1) {
        std::wstring tag = lower.substr(colon + 1);
        if (tag == L"latest" || tag == L"instruct" || tag == L"chat" ||
            tag == L"1b" || tag == L"2b" || tag == L"3b" || tag == L"7b" || tag == L"8b" ||
            tag == L"14b" || tag == L"32b" || tag == L"70b" ||
            tag.find(L"q4") != std::wstring::npos || tag.find(L"q5") != std::wstring::npos ||
            tag.find(L"q8") != std::wstring::npos || tag.find(L"fp16") != std::wstring::npos) {
            return true;
        }
    }

    return false;
}

OllamaFetchResult OllamaClient::FetchModelInfo(const OllamaModelSpec& spec,
                                             std::function<void(const std::wstring&)> statusCb)
{
    OllamaFetchResult result;
    result.info.spec = spec;
    result.success = false;

    if (!spec.isValid) {
        result.errorMsg = L"Geçersiz model adı veya biçimi.";
        return result;
    }

    if (statusCb) {
        statusCb(L"Ollama kayıt sunucusuna bağlanılıyor: " + spec.displayName + L"...");
    }

    // 1. Initialize WinHTTP session
    HINTERNET hSession = WinHttpOpen(
        L"Gety/1.9.6 (Windows; Win64; x64)",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0
    );
    if (!hSession) {
        result.errorMsg = L"WinHTTP başlatılamadı.";
        return result;
    }

    // Enable TLS 1.2 and 1.3
    DWORD secureProtocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2 | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3;
    WinHttpSetOption(hSession, WINHTTP_OPTION_SECURE_PROTOCOLS, &secureProtocols, sizeof(secureProtocols));

    // Connect to registry.ollama.ai
    HINTERNET hConnect = WinHttpConnect(hSession, L"registry.ollama.ai", INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        result.errorMsg = L"Ollama registry sunucusuna (registry.ollama.ai) bağlanılamadı.";
        return result;
    }

    // 2. Open Request for Manifest
    std::wstring path = L"/v2/" + spec.namespaceName + L"/" + spec.modelName + L"/manifests/" + spec.tag;
    HINTERNET hRequest = WinHttpOpenRequest(
        hConnect,
        L"GET",
        path.c_str(),
        NULL,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        WINHTTP_FLAG_SECURE
    );

    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        result.errorMsg = L"HTTP isteği oluşturulamadı.";
        return result;
    }

    // Follow redirects
    DWORD redirectPolicy = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
    WinHttpSetOption(hRequest, WINHTTP_OPTION_REDIRECT_POLICY, &redirectPolicy, sizeof(redirectPolicy));

    // Add Accept headers for Docker / OCI distribution manifests
    LPCWSTR manifestHeaders = L"Accept: application/vnd.docker.distribution.manifest.v2+json, application/vnd.oci.image.manifest.v1+json\r\n";
    WinHttpAddRequestHeaders(hRequest, manifestHeaders, -1L, WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE);

    // Send Request
    if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
        DWORD err = GetLastError();
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        result.errorMsg = L"İstek gönderilemedi. Hata kodu: " + std::to_wstring(err);
        return result;
    }

    if (!WinHttpReceiveResponse(hRequest, NULL)) {
        DWORD err = GetLastError();
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        result.errorMsg = L"Sunucudan yanıt alınamadı. Hata kodu: " + std::to_wstring(err);
        return result;
    }

    // Check HTTP Status Code
    DWORD statusCode = 0;
    DWORD statusSize = sizeof(statusCode);
    WinHttpQueryHeaders(
        hRequest,
        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX,
        &statusCode,
        &statusSize,
        WINHTTP_NO_HEADER_INDEX
    );
    result.statusCode = statusCode;

    if (statusCode == 404) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        result.errorMsg = L"Model bulunamadı (HTTP 404): " + spec.displayName + L"\nLütfen model adını ve etiketini kontrol edin.";
        return result;
    }

    if (statusCode != 200) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        result.errorMsg = L"Ollama sunucusu HTTP hatası döndürdü: " + std::to_wstring(statusCode);
        return result;
    }

    if (statusCb) {
        statusCb(L"Manifest indirildi, katmanlar çözümleniyor...");
    }

    // Read Response Body
    std::string responseBody;
    DWORD bytesAvailable = 0;
    while (WinHttpQueryDataAvailable(hRequest, &bytesAvailable) && bytesAvailable > 0) {
        std::vector<char> buffer(bytesAvailable);
        DWORD bytesRead = 0;
        if (WinHttpReadData(hRequest, buffer.data(), bytesAvailable, &bytesRead) && bytesRead > 0) {
            responseBody.append(buffer.data(), bytesRead);
        } else {
            break;
        }
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    if (responseBody.empty()) {
        result.errorMsg = L"Ollama sunucusundan boş yanıt alındı.";
        return result;
    }

    // 3. Parse JSON Manifest
    JsonParser parser(responseBody);
    JValue root = parser.Parse();

    if (root.type != JType::Object) {
        result.errorMsg = L"Geçersiz JSON manifest yanıtı.";
        return result;
    }

    result.info.schemaVersion = static_cast<int>(root.getInt("schemaVersion", 2));

    // Config layer
    auto configIt = root.obj.find("config");
    if (configIt != root.obj.end() && configIt->second.type == JType::Object) {
        result.info.configDigest = configIt->second.getString("digest");
        result.info.configSize = static_cast<uint64_t>(configIt->second.getInt("size", 0));
    }

    // Layers
    const auto& jsonLayers = root.getArray("layers");
    if (jsonLayers.empty()) {
        result.errorMsg = L"Manifest katman içermiyor.";
        return result;
    }

    result.info.totalSize = 0;
    result.info.ggufSize = 0;
    result.info.ggufLayerIndex = -1;

    for (size_t i = 0; i < jsonLayers.size(); ++i) {
        const auto& layerVal = jsonLayers[i];
        OllamaLayer layer;
        layer.mediaType = layerVal.getString("mediaType");
        layer.digest = layerVal.getString("digest");
        layer.size = static_cast<uint64_t>(layerVal.getInt("size", 0));

        // Calculate short hash (first 12 chars of digest after "sha256:")
        std::string rawHash = layer.digest;
        size_t colon = rawHash.find(':');
        if (colon != std::string::npos) {
            rawHash = rawHash.substr(colon + 1);
        }
        if (rawHash.length() > 12) {
            rawHash = rawHash.substr(0, 12);
        }
        layer.shortHash = Utf8ToWide(rawHash);

        // Download URL for blob
        layer.downloadUrl = L"https://registry.ollama.ai/v2/" + spec.namespaceName + L"/" + spec.modelName + L"/blobs/" + Utf8ToWide(layer.digest);

        // Map media type to target filename (compatible with akx/ollama-dl)
        if (layer.mediaType == "application/vnd.ollama.image.model") {
            layer.isModel = true;
            layer.targetFilename = L"model-" + layer.shortHash + L".gguf";
            result.info.ggufLayerIndex = static_cast<int>(i);
            result.info.ggufSize = layer.size;
        } else if (layer.mediaType == "application/vnd.ollama.image.template") {
            layer.targetFilename = L"template-" + layer.shortHash + L".txt";
        } else if (layer.mediaType == "application/vnd.ollama.image.params") {
            layer.targetFilename = L"params-" + layer.shortHash + L".json";
        } else if (layer.mediaType == "application/vnd.ollama.image.system") {
            layer.targetFilename = L"system-" + layer.shortHash + L".txt";
        } else if (layer.mediaType == "application/vnd.ollama.image.license") {
            layer.targetFilename = L"license-" + layer.shortHash + L".txt";
        } else {
            layer.targetFilename = L"layer-" + layer.shortHash + L".bin";
        }

        result.info.totalSize += layer.size;
        result.info.layers.push_back(layer);
    }

    // Recommended filenames
    if (spec.tag == L"latest") {
        result.info.recommendedGgufFilename = spec.modelName + L".gguf";
    } else {
        result.info.recommendedGgufFilename = spec.modelName + L"-" + spec.tag + L".gguf";
    }
    result.info.recommendedPackageDir = spec.modelName + L"-" + spec.tag;

    result.success = true;
    return result;
}

} // namespace Gety
