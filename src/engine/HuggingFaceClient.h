#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <functional>
#include <cstdint>

namespace Gety {

struct HuggingFaceModelSpec {
    std::wstring rawInput;
    std::wstring repoId;        // e.g. "TheBloke/Mistral-7B-Instruct-v0.2-GGUF" or "deepseek-ai/DeepSeek-R1"
    std::wstring revision;      // e.g. "main"
    std::wstring quantFilter;   // e.g. "q4_k_m" (optional)
    std::wstring subPath;       // e.g. specific file path if full URL was provided
    std::wstring displayName;   // e.g. "TheBloke/Mistral-7B-Instruct-v0.2-GGUF"
    bool isValid = false;
};

struct HuggingFaceFile {
    std::wstring path;          // e.g. "mistral-7b-instruct-v0.2.Q4_K_M.gguf"
    std::wstring filename;      // basename
    uint64_t size = 0;
    bool isLfs = false;
    std::string sha256;         // from LFS oid if available
    bool isGguf = false;
    bool isSafetensors = false;
    std::wstring quantType;     // e.g. "Q4_K_M", "Q5_K_M", "Q8_0"
    int qualityStars = 0;       // 1 to 5
    std::wstring description;   // e.g. "Onerilen (Hiz / Boyut Dengesi)"
    bool isRecommended = false;
    std::wstring downloadUrl;   // https://huggingface.co/{repoId}/resolve/{revision}/{path}
};

struct HuggingFaceModelInfo {
    HuggingFaceModelSpec spec;
    std::vector<HuggingFaceFile> files;
    bool hasGguf = false;
    bool hasSafetensors = false;
    int recommendedGgufIndex = -1;
    size_t totalGgufCount = 0;
    size_t totalSafetensorsCount = 0;
    uint64_t totalSize = 0;
};

struct HuggingFaceFetchResult {
    bool success = false;
    DWORD statusCode = 0;
    std::wstring errorMsg;
    HuggingFaceModelInfo info;
};

class HuggingFaceClient {
public:
    // Parse an input string (repo id, huggingface.co URL, etc.) into model spec
    static HuggingFaceModelSpec ParseModelSpec(const std::wstring& input);

    // Heuristic check if string looks like a Hugging Face repo or URL
    static bool IsLikelyHuggingFaceModel(const std::wstring& input);

    // Fetch repo tree from Hugging Face Hub API (synchronous, call from background thread)
    static HuggingFaceFetchResult FetchModelTree(const HuggingFaceModelSpec& spec,
                                                const std::wstring& token = L"",
                                                std::function<void(const std::wstring&)> statusCb = nullptr);

    // Resolve direct download URL (following 302 Found redirect with auth to CDN / S3 signed URL)
    static bool ResolveDownloadUrl(const std::wstring& downloadUrl,
                                   const std::wstring& token,
                                   std::wstring& outDirectUrl,
                                   std::wstring& outError);

    // Quantization quality and star analysis helper
    static void AnalyzeQuantization(const std::wstring& filename,
                                    std::wstring& outQuant,
                                    int& outStars,
                                    std::wstring& outDescription,
                                    bool& outIsRecommended);

    // Helper to format star rating (e.g. 4 -> "⭐⭐⭐⭐")
    static std::wstring FormatStars(int stars);

    // Helper to format bytes (e.g. "4.37 GB", "913.3 MB")
    static std::wstring FormatBytes(uint64_t bytes);

    // String conversion helpers
    static std::wstring Utf8ToWide(const std::string& str);
    static std::string WideToUtf8(const std::wstring& wstr);
};

} // namespace Gety
