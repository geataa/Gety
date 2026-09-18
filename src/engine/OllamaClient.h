#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <functional>
#include <cstdint>

namespace Gety {

struct OllamaModelSpec {
    std::wstring rawInput;
    std::wstring namespaceName; // e.g. "library"
    std::wstring modelName;     // e.g. "nomic-embed-text-v2-moe"
    std::wstring tag;           // e.g. "latest"
    std::wstring fullName;      // e.g. "library/nomic-embed-text-v2-moe:latest"
    std::wstring displayName;   // e.g. "nomic-embed-text-v2-moe:latest"
    bool isValid = false;
};

struct OllamaLayer {
    std::string mediaType;
    std::string digest;
    uint64_t size = 0;
    std::wstring shortHash;
    std::wstring targetFilename;
    std::wstring downloadUrl;
    bool isModel = false;
};

struct OllamaModelInfo {
    OllamaModelSpec spec;
    int schemaVersion = 2;
    std::string configDigest;
    uint64_t configSize = 0;
    std::vector<OllamaLayer> layers;
    int ggufLayerIndex = -1;
    uint64_t totalSize = 0;
    uint64_t ggufSize = 0;
    std::wstring recommendedGgufFilename;
    std::wstring recommendedPackageDir;
};

struct OllamaFetchResult {
    bool success = false;
    DWORD statusCode = 0;
    std::wstring errorMsg;
    OllamaModelInfo info;
};

enum class OllamaDownloadFormat {
    Gguf = 0,         // Single .gguf file (llama.cpp, LM Studio, etc.)
    OriginalPackage = 1 // All layers (model, template, params, license, system) in subfolder
};

class OllamaClient {
public:
    // Parse an input string into model spec
    static OllamaModelSpec ParseModelSpec(const std::wstring& input);

    // Heuristic check if string looks like an Ollama model identifier or URL
    static bool IsLikelyOllamaModel(const std::wstring& input);

    // Fetch manifest from Ollama registry (runs synchronously, call from background thread)
    static OllamaFetchResult FetchModelInfo(const OllamaModelSpec& spec,
                                           std::function<void(const std::wstring&)> statusCb = nullptr);

    // Helper to format bytes (e.g. "913.3 MB", "4.2 GB")
    static std::wstring FormatBytes(uint64_t bytes);

    // Converts UTF-8 string to wide string and vice versa
    static std::wstring Utf8ToWide(const std::string& str);
    static std::string WideToUtf8(const std::wstring& wstr);
};

} // namespace Gety
