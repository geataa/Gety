#include "HuggingFaceClient.h"
#include "../core/JsonParser.h"
#include "WinHttpUtils.h"
#include <winhttp.h>
#include <sstream>
#include <iomanip>
#include <algorithm>

#pragma comment(lib, "winhttp.lib")

namespace Gety {

std::wstring HuggingFaceClient::Utf8ToWide(const std::string& str) {
    if (str.empty()) return L"";
    int count = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.length(), NULL, 0);
    if (count <= 0) return L"";
    std::wstring result(count, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.length(), &result[0], count);
    return result;
}

std::string HuggingFaceClient::WideToUtf8(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int count = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.length(), NULL, 0, NULL, NULL);
    if (count <= 0) return "";
    std::string result(count, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.length(), &result[0], count, NULL, NULL);
    return result;
}

std::wstring HuggingFaceClient::FormatBytes(uint64_t bytes) {
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

std::wstring HuggingFaceClient::FormatStars(int stars) {
    if (stars <= 0) return L"";
    std::wstring s;
    for (int i = 0; i < stars && i < 5; ++i) {
        s += L"⭐";
    }
    return s;
}

static std::wstring TrimWide(const std::wstring& s) {
    size_t start = s.find_first_not_of(L" \t\r\n\"'");
    if (start == std::wstring::npos) return L"";
    size_t end = s.find_last_not_of(L" \t\r\n\"'");
    return s.substr(start, end - start + 1);
}

static std::wstring ToUpperWide(const std::wstring& s) {
    std::wstring u = s;
    for (auto& c : u) c = towupper(c);
    return u;
}

void HuggingFaceClient::AnalyzeQuantization(const std::wstring& filename,
                                           std::wstring& outQuant,
                                           int& outStars,
                                           std::wstring& outDescription,
                                           bool& outIsRecommended)
{
    outQuant = L"";
    outStars = 0;
    outDescription = L"";
    outIsRecommended = false;

    std::wstring upper = ToUpperWide(filename);

    struct QuantDef {
        const wchar_t* tag;
        int stars;
        const wchar_t* desc;
        bool isRecommended;
    };

    static const QuantDef quantDefs[] = {
        // High-quality K-quants & recommended
        { L"Q4_K_M", 4, L"Önerilen (Hız, boyut ve kalite dengesi)", true },
        { L"Q4_K_XL", 4, L"Önerilen Yüksek Kalite", true },
        { L"Q5_K_M", 5, L"Çok Yüksek Kalite (Düşük kayıp)", false },
        { L"Q8_0",   5, L"En Yüksek Kalite (Neredeyse kayıpsız 8-bit)", false },
        { L"Q6_K",   5, L"Çok Yüksek Kalite (6-bit)", false },
        { L"Q5_K_S", 4, L"Yüksek Kalite (Kompakt 5-bit)", false },
        { L"Q4_K_S", 4, L"İyi Kalite (Q4_K_M'den biraz daha küçük)", false },

        // i-Matrix quants
        { L"IQ4_NL", 4, L"Modern i-Matrix Kalite (Önerilen)", false },
        { L"IQ4_XS", 4, L"Modern i-Matrix Kompakt", false },
        { L"IQ3_M",  3, L"i-Matrix Dengeli (3-bit)", false },
        { L"IQ3_S",  3, L"i-Matrix Kompakt (3-bit)", false },
        { L"IQ3_XXS", 2, L"i-Matrix Küçük (Düşük VRAM)", false },
        { L"IQ2_M",  2, L"i-Matrix Ultra Küçük", false },
        { L"IQ2_S",  2, L"i-Matrix Ultra Küçük", false },
        { L"IQ2_XXS", 1, L"i-Matrix Ekstrem Küçük", false },
        { L"IQ1_S",  1, L"1-Bit Ekstrem Sıkıştırma", false },
        { L"IQ1_M",  1, L"1-Bit Ekstrem Sıkıştırma", false },

        // Legacy / standard quants
        { L"Q5_0",   4, L"Standart 5-bit", false },
        { L"Q5_1",   4, L"Standart 5-bit", false },
        { L"Q4_0",   3, L"Standart 4-bit (Temel)", false },
        { L"Q4_1",   3, L"Standart 4-bit", false },
        { L"Q3_K_L", 3, L"Orta Kalite (3-bit)", false },
        { L"Q3_K_M", 3, L"Orta Kalite (3-bit)", false },
        { L"Q3_K_S", 2, L"Düşük Kalite (Kayıplı)", false },
        { L"Q2_K",   1, L"Çok Düşük Kalite (Acil durum / minimal RAM)", false },

        // Full precision
        { L"BF16",   5, L"Orijinal BFloat16 Hassasiyet", false },
        { L"FP16",   5, L"Orijinal Float16 Hassasiyet", false },
        { L"F16",    5, L"Orijinal Float16 Hassasiyet", false },
        { L"FP32",   5, L"Tam Hassasiyet Float32", false },
        { L"F32",    5, L"Tam Hassasiyet Float32", false }
    };

    for (const auto& q : quantDefs) {
        // Search as a token with delimiters or prefix/suffix
        size_t pos = upper.find(q.tag);
        if (pos != std::wstring::npos) {
            bool leftOk = (pos == 0 || !iswalnum(upper[pos - 1]));
            size_t endPos = pos + wcslen(q.tag);
            bool rightOk = (endPos >= upper.length() || !iswalnum(upper[endPos]));
            if (leftOk && rightOk) {
                outQuant = q.tag;
                outStars = q.stars;
                outDescription = q.desc;
                outIsRecommended = q.isRecommended;
                return;
            }
        }
    }

    // Generic fallback if .gguf
    if (upper.length() >= 5 && upper.substr(upper.length() - 5) == L".GGUF") {
        outQuant = L"GGUF";
        outStars = 3;
        outDescription = L"GGUF Model Ağırlıkları";
        outIsRecommended = false;
    }
}

HuggingFaceModelSpec HuggingFaceClient::ParseModelSpec(const std::wstring& input) {
    HuggingFaceModelSpec spec;
    spec.rawInput = input;

    std::wstring s = TrimWide(input);
    if (s.empty()) return spec;

    // 1. Remove CLI command prefixes
    const std::wstring prefixes[] = {
        L"huggingface-cli download ",
        L"hf download ",
        L"hf-dl ",
        L"hf://",
        L"huggingface://"
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

    // 2. Remove scheme if present
    if (s.rfind(L"https://", 0) == 0) {
        s = s.substr(8);
    } else if (s.rfind(L"http://", 0) == 0) {
        s = s.substr(7);
    }

    // 3. Remove hostname if huggingface.co or hf.co
    if (s.rfind(L"huggingface.co/", 0) == 0) {
        s = s.substr(15);
    } else if (s.rfind(L"hf.co/", 0) == 0) {
        s = s.substr(6);
    } else if (s.rfind(L"www.huggingface.co/", 0) == 0) {
        s = s.substr(19);
    }

    // Clean any query string or fragment
    size_t qPos = s.find_first_of(L"?#");
    if (qPos != std::wstring::npos) {
        s = s.substr(0, qPos);
    }
    s = TrimWide(s);

    if (s.empty()) return spec;

    // Check if path has revision and subpath (e.g. "owner/repo/tree/main", "owner/repo/blob/main/file.gguf", "owner/repo/resolve/main/file.gguf")
    spec.revision = L"main";

    // Detect @revision syntax: "owner/repo@revision"
    size_t atPos = s.find(L'@');
    if (atPos != std::wstring::npos) {
        spec.revision = TrimWide(s.substr(atPos + 1));
        s = TrimWide(s.substr(0, atPos));
    }

    // Detect :quant syntax: "owner/repo:q4_k_m"
    size_t colonPos = s.rfind(L':');
    if (colonPos != std::wstring::npos && colonPos > s.find(L'/')) {
        spec.quantFilter = TrimWide(s.substr(colonPos + 1));
        s = TrimWide(s.substr(0, colonPos));
    }

    // Split path components
    std::vector<std::wstring> parts;
    size_t start = 0;
    while (start < s.length()) {
        size_t nextSlash = s.find(L'/', start);
        if (nextSlash == std::wstring::npos) {
            parts.push_back(s.substr(start));
            break;
        }
        parts.push_back(s.substr(start, nextSlash - start));
        start = nextSlash + 1;
    }

    if (parts.size() >= 2) {
        spec.repoId = parts[0] + L"/" + parts[1];

        // Check if there are path modifiers (tree, blob, resolve, raw)
        if (parts.size() >= 4 && (parts[2] == L"tree" || parts[2] == L"blob" || parts[2] == L"resolve" || parts[2] == L"raw")) {
            spec.revision = parts[3];
            if (parts.size() >= 5) {
                std::wstring sub;
                for (size_t i = 4; i < parts.size(); ++i) {
                    if (!sub.empty()) sub += L"/";
                    sub += parts[i];
                }
                spec.subPath = sub;
            }
        }
    } else if (parts.size() == 1 && !parts[0].empty()) {
        // Single name repo (e.g. "gpt2")
        spec.repoId = parts[0];
    } else {
        return spec;
    }

    if (spec.revision.empty()) {
        spec.revision = L"main";
    }

    spec.displayName = spec.repoId;
    if (spec.revision != L"main") {
        spec.displayName += L" @" + spec.revision;
    }
    if (!spec.quantFilter.empty()) {
        spec.displayName += L":" + spec.quantFilter;
    }

    spec.isValid = (!spec.repoId.empty() && spec.repoId.find(L'/') != std::wstring::npos);
    return spec;
}

bool HuggingFaceClient::IsLikelyHuggingFaceModel(const std::wstring& input) {
    std::wstring s = TrimWide(input);
    if (s.empty()) return false;

    // Explicit domain and tool signatures
    if (s.find(L"huggingface.co/") != std::wstring::npos) return true;
    if (s.find(L"hf.co/") != std::wstring::npos) return true;
    if (s.rfind(L"hf://", 0) == 0) return true;
    if (s.rfind(L"huggingface://", 0) == 0) return true;
    if (s.rfind(L"hf download", 0) == 0) return true;
    if (s.rfind(L"huggingface-cli", 0) == 0) return true;

    std::wstring lower = s;
    for (auto& c : lower) c = towlower(c);

    // Common GGUF quant publishers
    const wchar_t* knownAuthors[] = {
        L"thebloke/", L"bartowski/", L"maziyarpanahi/", L"mradermacher/",
        L"lmstudio-community/", L"unsloth/", L"second-state/", L"nomic-ai/",
        L"deepseek-ai/", L"meta-llama/", L"mistralai/", L"google/"
    };
    for (const auto* auth : knownAuthors) {
        if (lower.find(auth) != std::wstring::npos) {
            return true;
        }
    }

    // Contains repo pattern owner/name with GGUF or safetensors extension/keyword
    if (lower.find(L'/') != std::wstring::npos) {
        if (lower.find(L"gguf") != std::wstring::npos ||
            lower.find(L"safetensors") != std::wstring::npos) {
            return true;
        }
    }

    return false;
}

HuggingFaceFetchResult HuggingFaceClient::FetchModelTree(const HuggingFaceModelSpec& spec,
                                                        const std::wstring& token,
                                                        std::function<void(const std::wstring&)> statusCb)
{
    HuggingFaceFetchResult result;
    result.info.spec = spec;
    result.success = false;

    if (!spec.isValid) {
        result.errorMsg = L"Geçersiz Hugging Face model kimliği (Örnek: TheBloke/Mistral-7B-Instruct-v0.2-GGUF)";
        return result;
    }

    if (statusCb) {
        statusCb(L"Hugging Face Hub API'sine bağlanılıyor: " + spec.repoId + L"...");
    }

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

    DWORD secureProtocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2 | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3;
    WinHttpSetOption(hSession, WINHTTP_OPTION_SECURE_PROTOCOLS, &secureProtocols, sizeof(secureProtocols));

    HINTERNET hConnect = WinHttpConnect(hSession, L"huggingface.co", INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        result.errorMsg = L"huggingface.co sunucusuna bağlanılamadı.";
        return result;
    }

    // Path: /api/models/{repo_id}/tree/{revision}
    std::wstring path = L"/api/models/" + spec.repoId + L"/tree/" + spec.revision;

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
        result.errorMsg = L"WinHTTP istek nesnesi oluşturulamadı.";
        return result;
    }

    DWORD redirectPolicy = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
    WinHttpSetOption(hRequest, WINHTTP_OPTION_REDIRECT_POLICY, &redirectPolicy, sizeof(redirectPolicy));

    // Headers
    std::wstring headers = L"Accept: application/json\r\n";
    if (!token.empty()) {
        headers += L"Authorization: Bearer " + token + L"\r\n";
    }

    if (!WinHttpSendRequest(hRequest, headers.c_str(), (DWORD)headers.length(), WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        result.errorMsg = L"Hugging Face Hub API isteği gönderilemedi.";
        return result;
    }

    if (!WinHttpReceiveResponse(hRequest, NULL)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        result.errorMsg = L"Hugging Face Hub yanıtı alınamadı.";
        return result;
    }

    DWORD statusCode = 0;
    DWORD statusSize = sizeof(statusCode);
    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusSize, WINHTTP_NO_HEADER_INDEX);
    result.statusCode = statusCode;

    if (statusCode == 401 || statusCode == 403) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        result.errorMsg = L"Bu model erişim izni gerektiriyor (Gated Model) veya özel. Lütfen Hugging Face Erişim Jetonunuzu (hf_...) girin.";
        return result;
    } else if (statusCode == 404) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        result.errorMsg = L"Hugging Face deposu veya dalı bulunamadı: " + spec.repoId + L" (Dal: " + spec.revision + L")";
        return result;
    } else if (statusCode != 200) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        result.errorMsg = L"Hugging Face Hub API hatası: HTTP " + std::to_wstring(statusCode);
        return result;
    }

    // Read full JSON response body
    std::string responseBody;
    DWORD bytesAvailable = 0;
    while (WinHttpQueryDataAvailable(hRequest, &bytesAvailable) && bytesAvailable > 0) {
        std::vector<char> buffer(bytesAvailable);
        DWORD bytesRead = 0;
        if (WinHttpReadData(hRequest, buffer.data(), bytesAvailable, &bytesRead) && bytesRead > 0) {
            responseBody.append(buffer.data(), bytesRead);
        }
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    if (responseBody.empty()) {
        result.errorMsg = L"Hugging Face sunucusundan boş yanıt alındı.";
        return result;
    }

    // Parse JSON
    try {
        JsonParser parser(responseBody);
        JValue root = parser.Parse();

        if (root.type != JType::Array) {
            result.errorMsg = L"Hugging Face dosya ağacı beklenmeyen biçimde döndü.";
            return result;
        }

        std::wstring quantFilterUpper = ToUpperWide(spec.quantFilter);

        for (const auto& item : root.arr) {
            if (item.type != JType::Object) continue;

            std::string type = item.getString("type");
            if (type != "file") continue;

            std::string p = item.getString("path");
            if (p.empty()) continue;

            std::wstring filePath = Utf8ToWide(p);
            std::wstring filename = filePath;
            size_t slash = filename.rfind(L'/');
            if (slash != std::wstring::npos) {
                filename = filename.substr(slash + 1);
            }

            uint64_t size = 0;
            if (item.has("size")) {
                size = (uint64_t)item.getInt("size");
            }

            bool isLfs = false;
            std::string sha256;
            if (item.has("lfs")) {
                const auto& lfs = item.getObject("lfs");
                isLfs = true;
                if (lfs.has("size")) {
                    size = (uint64_t)lfs.getInt("size");
                }
                if (lfs.has("oid")) {
                    sha256 = lfs.getString("oid");
                }
            }

            HuggingFaceFile file;
            file.path = filePath;
            file.filename = filename;
            file.size = size;
            file.isLfs = isLfs;
            file.sha256 = sha256;
            file.downloadUrl = L"https://huggingface.co/" + spec.repoId + L"/resolve/" + spec.revision + L"/" + filePath;

            std::wstring upperFn = ToUpperWide(filename);
            if (upperFn.length() >= 5 && upperFn.substr(upperFn.length() - 5) == L".GGUF") {
                file.isGguf = true;
                result.info.hasGguf = true;
                result.info.totalGgufCount++;

                AnalyzeQuantization(filename, file.quantType, file.qualityStars, file.description, file.isRecommended);
            } else if (upperFn.length() >= 12 && upperFn.substr(upperFn.length() - 12) == L".SAFETENSORS") {
                file.isSafetensors = true;
                result.info.hasSafetensors = true;
                result.info.totalSafetensorsCount++;
                file.description = L"Hugging Face Safetensors Ağırlıkları";
                file.qualityStars = 5;
            }

            result.info.totalSize += size;
            result.info.files.push_back(file);
        }

        if (result.info.files.empty()) {
            result.errorMsg = L"Model deposunda dosya bulunamadı.";
            return result;
        }

        // Sort files: GGUF files first (sorted by size or quality), then safetensors, then others
        std::sort(result.info.files.begin(), result.info.files.end(), [](const HuggingFaceFile& a, const HuggingFaceFile& b) {
            if (a.isGguf != b.isGguf) return a.isGguf > b.isGguf;
            if (a.isSafetensors != b.isSafetensors) return a.isSafetensors > b.isSafetensors;
            if (a.qualityStars != b.qualityStars) return a.qualityStars > b.qualityStars;
            return a.size < b.size;
        });

        // Determine recommended file index
        int bestIdx = -1;
        int q4kmIdx = -1;
        int firstGgufIdx = -1;
        int filterMatchedIdx = -1;

        for (size_t i = 0; i < result.info.files.size(); ++i) {
            const auto& f = result.info.files[i];

            if (!quantFilterUpper.empty() && ToUpperWide(f.filename).find(quantFilterUpper) != std::wstring::npos) {
                if (filterMatchedIdx < 0) filterMatchedIdx = (int)i;
            }

            if (f.isGguf) {
                if (firstGgufIdx < 0) firstGgufIdx = (int)i;
                if (f.quantType == L"Q4_K_M") {
                    q4kmIdx = (int)i;
                }
                if (f.isRecommended && bestIdx < 0) {
                    bestIdx = (int)i;
                }
            }
        }

        if (filterMatchedIdx >= 0) {
            result.info.recommendedGgufIndex = filterMatchedIdx;
        } else if (q4kmIdx >= 0) {
            result.info.recommendedGgufIndex = q4kmIdx;
        } else if (bestIdx >= 0) {
            result.info.recommendedGgufIndex = bestIdx;
        } else if (firstGgufIdx >= 0) {
            result.info.recommendedGgufIndex = firstGgufIdx;
        } else {
            result.info.recommendedGgufIndex = 0;
        }

        result.success = true;
    } catch (const std::exception& ex) {
        result.errorMsg = L"Hugging Face yanıtı ayrıştırılamadı: " + Utf8ToWide(ex.what());
    }

    return result;
}

bool HuggingFaceClient::ResolveDownloadUrl(const std::wstring& downloadUrl,
                                          const std::wstring& token,
                                          std::wstring& outDirectUrl,
                                          std::wstring& outError)
{
    outDirectUrl = downloadUrl;
    outError = L"";

    UrlParts parts;
    if (!WinHttpUtils::ParseUrl(downloadUrl, parts)) {
        outError = L"Geçersiz indirme bağlantısı";
        return false;
    }

    HINTERNET hSession = WinHttpOpen(
        L"Gety/1.9.6 (Windows; Win64; x64)",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0
    );
    if (!hSession) {
        outError = L"WinHTTP başlatılamadı";
        return false;
    }

    DWORD secureProtocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2 | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3;
    WinHttpSetOption(hSession, WINHTTP_OPTION_SECURE_PROTOCOLS, &secureProtocols, sizeof(secureProtocols));

    HINTERNET hConnect = WinHttpConnect(hSession, parts.host.c_str(), parts.port, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        outError = L"Sunucuya bağlanılamadı: " + parts.host;
        return false;
    }

    DWORD reqFlags = parts.isHttps ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"HEAD", parts.path.c_str(), NULL,
                                           WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, reqFlags);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        outError = L"İstek oluşturulamadı";
        return false;
    }

    // Do NOT follow redirects automatically so we can capture the final CDN/S3 Location
    DWORD redirectPolicy = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
    WinHttpSetOption(hRequest, WINHTTP_OPTION_REDIRECT_POLICY, &redirectPolicy, sizeof(redirectPolicy));

    std::wstring headers = L"Accept: */*\r\n";
    if (!token.empty()) {
        headers += L"Authorization: Bearer " + token + L"\r\n";
    }

    if (!WinHttpSendRequest(hRequest, headers.c_str(), (DWORD)headers.length(), WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        outError = L"İstek gönderilemedi";
        return false;
    }

    if (!WinHttpReceiveResponse(hRequest, NULL)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        outError = L"Sunucudan yanıt alınamadı";
        return false;
    }

    DWORD statusCode = 0;
    DWORD statusSize = sizeof(statusCode);
    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusSize, WINHTTP_NO_HEADER_INDEX);

    if (statusCode == 301 || statusCode == 302 || statusCode == 303 || statusCode == 307 || statusCode == 308) {
        wchar_t locBuf[4096] = { 0 };
        DWORD locSize = sizeof(locBuf);
        if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_LOCATION, WINHTTP_HEADER_NAME_BY_INDEX,
                                locBuf, &locSize, WINHTTP_NO_HEADER_INDEX)) {
            outDirectUrl = WinHttpUtils::CombineUrl(downloadUrl, locBuf);
        }
    } else if (statusCode == 401 || statusCode == 403) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        outError = L"Erişim yetkisi yetersiz veya özel model (Gated). Hugging Face token gereklidir.";
        return false;
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return true;
}

} // namespace Gety
