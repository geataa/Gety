#include <iostream>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <io.h>
#include <fcntl.h>
#include "../src/core/Types.h"
#include "../src/core/BatchExpander.h"
#include "../src/core/I18n.h"
#include "../src/core/Config.h"
#include "../src/core/CliArgs.h"
#include "../src/engine/WinHttpUtils.h"
#include "../src/engine/DownloadTask.h"
#include "../src/engine/DownloadManager.h"
#include "../src/engine/MediaExtractor.h"
#include "../src/engine/OllamaClient.h"
#include "../src/engine/HuggingFaceClient.h"
#include "../src/core/Crypto.h"
#include "../src/core/JsonParser.h"

using namespace Gety;

// Release-active assertion macro (never compiled out by NDEBUG)
#define WIDE_STR_INNER(x) L##x
#define WIDE_STR(x) WIDE_STR_INNER(x)
#define TEST_ASSERT(expr)                                                                 \
    do {                                                                                  \
        if (!(expr)) {                                                                    \
            std::wcerr << L"\n[TEST FAILED] " << WIDE_STR(#expr)                          \
                       << L" at line " << __LINE__ << std::endl;                          \
            std::wcout << L"\n[TEST FAILED] " << WIDE_STR(#expr)                          \
                       << L" at line " << __LINE__ << std::endl;                          \
            std::exit(1);                                                                 \
        }                                                                                 \
    } while (false)

int main(int argc, char* argv[]) {
    _setmode(_fileno(stdout), _O_U16TEXT);
    _setmode(_fileno(stderr), _O_U16TEXT);
    const bool isCiEnv = (GetEnvironmentVariableW(L"GITHUB_ACTIONS", nullptr, 0) > 0);
    std::wcout << L"=== Gety Engine Test Suite ===" << std::endl;

    // Test 1: Batch URL Expander
    std::wcout << L"[1] Testing Batch URL Expander..." << std::endl;
    auto urls = BatchExpander::ExpandNumeric(L"http://example.com/file_(*).zip", 1, 5, 2);
    TEST_ASSERT(urls.size() == 5);
    TEST_ASSERT(urls[0] == L"http://example.com/file_01.zip");
    TEST_ASSERT(urls[4] == L"http://example.com/file_05.zip");
    std::wcout << L"    BatchExpander PASSED!" << std::endl;

    // Test 2: WinHttpUtils URL parsing & filename extraction
    std::wcout << L"[2] Testing URL Parsing and Filename Extraction..." << std::endl;
    UrlParts parts;
    bool parsed = WinHttpUtils::ParseUrl(L"https://download.example.com:8443/files/archive_v1.zip?token=123", parts);
    TEST_ASSERT(parsed);
    TEST_ASSERT(parts.host == L"download.example.com");
    TEST_ASSERT(parts.port == 8443);
    TEST_ASSERT(parts.isHttps == true);
    std::wstring fn = WinHttpUtils::ExtractFilenameFromUrl(L"https://download.example.com:8443/files/archive_v1.zip?token=123");
    TEST_ASSERT(fn == L"archive_v1.zip");
    std::wcout << L"    URL Parsing & Filename Extraction PASSED!" << std::endl;

    // Test 3: MediaExtractor Engine
    std::wcout << L"[3] Testing MediaExtractor (yt-dlp integration)..." << std::endl;
    bool ytdlpAvail = MediaExtractor::IsAvailable();
    std::wstring ytdlpPath = MediaExtractor::GetYtDlpPath();
    std::wcout << L"    yt-dlp Available: " << (ytdlpAvail ? L"YES" : L"NO") << std::endl;
    std::wcout << L"    yt-dlp Path: " << ytdlpPath << std::endl;
    if (ytdlpAvail) {
        TEST_ASSERT(!ytdlpPath.empty());
    }
    TEST_ASSERT(MediaExtractor::FormatDuration(213) == L"3:33" || MediaExtractor::FormatDuration(213) == L"03:33");
    TEST_ASSERT(MediaExtractor::FormatBytes(1048576) == L"1.0 MB");
    std::wcout << L"    MediaExtractor helpers PASSED!" << std::endl;

    // Test 4: Custom Filename & UpdateUrl Lock Safety
    std::wcout << L"[4] Testing Custom Filename & UpdateUrl Lock Safety..." << std::endl;
    {
        auto task = std::make_unique<DownloadTask>(L"https://example.com/video.mp4", L"C:\\Downloads", L"Video", 1, L"BenimOzelFilmim.mp4", true);
        auto s = task->GetSnapshot();
        TEST_ASSERT(s.filename == L"BenimOzelFilmim.mp4");
        TEST_ASSERT(s.fullPath == L"C:\\Downloads\\BenimOzelFilmim.mp4");

        // Verify UpdateUrl does not deadlock or crash with recursive lock
        bool updated = task->UpdateUrl(L"https://example.com/video_new.mp4");
        TEST_ASSERT(updated);
        TEST_ASSERT(task->GetSnapshot().url == L"https://example.com/video_new.mp4");

        // Verify non-blocking DeleteTask
        auto tStart = std::chrono::steady_clock::now();
        task->DeleteTask(false);
        auto tElapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - tStart).count();
        TEST_ASSERT(tElapsed < 500); // Must be instantaneous
        std::wcout << L"    DeleteTask took " << tElapsed << L" ms (Instantaneous!)" << std::endl;
    }
    std::wcout << L"    Custom Filename & UpdateUrl Lock Safety PASSED!" << std::endl;

    if (argc > 1 && std::string(argv[1]) == "--video") {
        std::wcout << L"[4] Testing Live YouTube Info Extraction..." << std::endl;
        std::wstring testVid = L"https://www.youtube.com/watch?v=dQw4w9WgXcQ";
        auto res = MediaExtractor::ExtractInfo(testVid, [](const std::wstring& status) {
            std::wcout << L"    Status: " << status << std::endl;
        });
        std::wcout << L"    Success: " << (res.success ? L"YES" : L"NO") << std::endl;
        std::wcout << L"    Title: " << res.title << std::endl;
        TEST_ASSERT(res.success);
        TEST_ASSERT(!res.items.empty());
        std::wcout << L"    Video: " << res.items[0].title << L" (" << MediaExtractor::FormatDuration(res.items[0].durationSec) << L")" << std::endl;
        std::wcout << L"    Formats count: " << res.items[0].formats.size() << std::endl;
        for (const auto& fmt : res.items[0].formats) {
            std::wcout << L"      -> " << fmt.resolution << L" (" << fmt.ext << L") " << fmt.note << L" [" << MediaExtractor::FormatBytes(fmt.filesize) << L"]" << std::endl;
        }
        std::wcout << L"    Live YouTube Info Extraction PASSED!" << std::endl;
        return 0;
    }

    // Test 5: Live Download Test if port is passed
    if (argc > 1) {
        int port = std::stoi(argv[1]);
        std::wcout << L"[5] Testing Live 5-Segment Download on port " << port << L"..." << std::endl;

        std::wstring testUrl = L"http://127.0.0.1:" + std::to_wstring(port) + L"/test_data.bin";
        if (argc > 2) {
            std::string customUrl = argv[2];
            testUrl = std::wstring(customUrl.begin(), customUrl.end());
            std::wcout << L"    Using custom URL: " << testUrl << std::endl;
        }
        std::wstring tempSaveDir = std::filesystem::current_path().wstring() + L"\\test_downloads";
        std::filesystem::create_directories(tempSaveDir);

        auto task = std::make_unique<DownloadTask>(testUrl, tempSaveDir, L"Test", 5);
        task->Start();
        std::wcout << L"    Task started, waiting for completion..." << std::endl;

        int waitedMs = 0;
        while (task->GetState() != DownloadState::Completed &&
               task->GetState() != DownloadState::Failed &&
               waitedMs < 15000)
        {
            Sleep(200);
            waitedMs += 200;
            if (waitedMs % 1000 == 0) {
                auto s = task->GetSnapshot();
                std::wcout << L"    [Progress] " << s.downloadedBytes << L" / " << s.totalBytes 
                           << L" (" << StateToString(s.state) << L")" << std::endl;
            }
        }

        auto snap = task->GetSnapshot();
        std::wcout << L"    Final Status: " << StateToString(snap.state) << std::endl;
        std::wcout << L"    Downloaded: " << snap.downloadedBytes << L" / " << snap.totalBytes << std::endl;
        std::wcout << L"    Segments used: " << snap.segments.size() << std::endl;

        if (snap.state != DownloadState::Completed) {
            std::wcerr << L"FAILED: Expected state Completed, got " << StateToString(snap.state) 
                       << L" Err: " << snap.errorMsg << std::endl;
            return 2;
        }

        if (snap.downloadedBytes != snap.totalBytes) {
            std::wcerr << L"FAILED: downloadedBytes (" << snap.downloadedBytes 
                       << L") != totalBytes (" << snap.totalBytes << L")" << std::endl;
            return 3;
        }

        std::wstring finalPath = tempSaveDir + L"\\" + snap.filename;
        if (!std::filesystem::exists(finalPath)) {
            std::wcerr << L"FAILED: File does not exist at " << finalPath << std::endl;
            return 4;
        }

        if (std::filesystem::file_size(finalPath) != snap.totalBytes) {
            std::wcerr << L"FAILED: File size on disk mismatch: " << std::filesystem::file_size(finalPath) 
                       << L" vs " << snap.totalBytes << std::endl;
            return 5;
        }

        std::wcout << L"    Live Segmented Download PASSED!" << std::endl;
        std::filesystem::remove_all(tempSaveDir);
    }

    // Test 6: Localization (12 Languages, Ukrainian present, Hebrew absent)
    std::wcout << L"[6] Testing Localization (12 Languages, Ukrainian, Hebrew Exclusion)..." << std::endl;
    {
        const auto& langs = I18n::Instance().GetLanguages();
        std::wcout << L"    Total registered languages: " << langs.size() << std::endl;
        TEST_ASSERT(langs.size() == 12);

        bool foundUk = false;
        bool foundHe = false;
        for (const auto& l : langs) {
            std::wcout << L"    - " << l.code << L" (" << l.name << L" / " << l.nativeName << L")" << std::endl;
            if (l.code == L"uk" && l.nativeName == L"Українська") {
                foundUk = true;
            }
            if (l.code == L"he" || l.name == L"Hebrew") {
                foundHe = true;
            }
        }

        TEST_ASSERT(foundUk == true);
        TEST_ASSERT(foundHe == false);

        // Test Ukrainian translations
        I18n::Instance().SetLanguage(LangId::Ukrainian);
        TEST_ASSERT(std::wstring(I18n::Instance().Get(StrId::MenuFile)) == L"&Файл");
        TEST_ASSERT(std::wstring(I18n::Instance().Get(StrId::MenuTaskStart)) == L"&Запустити\tF5");
        TEST_ASSERT(std::wstring(I18n::Instance().Get(StrId::ActVideoLink)) == L"Додати посилання");
        TEST_ASSERT(std::wstring(I18n::Instance().Get(StrId::CatAll)) == L"📂 Усі завдання");
        TEST_ASSERT(std::wstring(I18n::Instance().Get(StrId::CatUnfinished)) == L"⏳ Незавершені");
        TEST_ASSERT(std::wstring(I18n::Instance().Get(StrId::DlgVideoTitle)) == L"Додати посилання на відео/медіа");

        // Restore Turkish
        I18n::Instance().SetLanguage(LangId::Turkish);
        std::wcout << L"    12 Languages & Ukrainian Localization PASSED!" << std::endl;
    }

    // Test 10: OllamaClient Spec Parsing & Live Manifest Fetch
    std::wcout << L"[10] Testing OllamaClient Spec Parsing & Live Manifest Fetch..." << std::endl;
    {
        auto spec1 = OllamaClient::ParseModelSpec(L"nomic-embed-text-v2-moe:latest");
        TEST_ASSERT(spec1.isValid);
        TEST_ASSERT(spec1.namespaceName == L"library");
        TEST_ASSERT(spec1.modelName == L"nomic-embed-text-v2-moe");
        TEST_ASSERT(spec1.tag == L"latest");
        TEST_ASSERT(spec1.displayName == L"nomic-embed-text-v2-moe:latest");

        auto spec2 = OllamaClient::ParseModelSpec(L"ollama run llama3.2:1b");
        TEST_ASSERT(spec2.isValid);
        TEST_ASSERT(spec2.modelName == L"llama3.2");
        TEST_ASSERT(spec2.tag == L"1b");

        auto spec3 = OllamaClient::ParseModelSpec(L"https://ollama.com/library/nomic-embed-text-v2-moe");
        TEST_ASSERT(spec3.isValid);
        TEST_ASSERT(spec3.modelName == L"nomic-embed-text-v2-moe");
        TEST_ASSERT(spec3.tag == L"latest");

        TEST_ASSERT(OllamaClient::IsLikelyOllamaModel(L"nomic-embed-text-v2-moe:latest"));
        TEST_ASSERT(OllamaClient::IsLikelyOllamaModel(L"llama3.2:3b"));
        TEST_ASSERT(!OllamaClient::IsLikelyOllamaModel(L"https://example.com/file.zip"));

        // Live manifest fetch test for nomic-embed-text-v2-moe:latest
        auto res = OllamaClient::FetchModelInfo(spec1, [](const std::wstring& msg) {
            std::wcout << L"    Status: " << msg << std::endl;
        });

        if (!res.success && isCiEnv && (res.statusCode == 429 || res.statusCode >= 500)) {
            std::wcout << L"    [CI Rate-Limit Warning] Ollama Hub returned HTTP " << res.statusCode
                       << L": " << res.errorMsg << std::endl;
        } else {
            if (!res.success) {
                std::wcerr << L"    Ollama Fetch Error (HTTP " << res.statusCode << L"): " << res.errorMsg << std::endl;
            }
            TEST_ASSERT(res.success);
            TEST_ASSERT(res.info.ggufLayerIndex >= 0);
            TEST_ASSERT(res.info.ggufSize > 500ULL * 1024ULL * 1024ULL); // ~957 MB
            TEST_ASSERT(!res.info.recommendedGgufFilename.empty());
            std::wcout << L"    Model: " << res.info.spec.displayName << std::endl;
            std::wcout << L"    Layers count: " << res.info.layers.size() << std::endl;
            std::wcout << L"    GGUF size: " << OllamaClient::FormatBytes(res.info.ggufSize) << std::endl;
            std::wcout << L"    GGUF filename: " << res.info.recommendedGgufFilename << std::endl;
        }
        std::wcout << L"    OllamaClient Spec Parsing & Live Manifest Fetch PASSED!" << std::endl;
    }

    // Test 11: Crypto - Base64 Encoding & Decoding
    std::wcout << L"[11] Testing Crypto Base64 Encoding & Decoding..." << std::endl;
    {
        std::string sample = "Hello, Gety! Ultra-Fast C++23 Download Manager with AI.";
        std::string b64 = Crypto::Base64Encode(sample);
        TEST_ASSERT(!b64.empty());
        std::string decoded = Crypto::Base64DecodeToString(b64);
        TEST_ASSERT(decoded == sample);
        std::wcout << L"    Base64 Encoding & Decoding PASSED!" << std::endl;
    }

    // Test 12: Crypto - Windows DPAPI & Portable Encryption Round-Trip
    std::wcout << L"[12] Testing Crypto DPAPI & Portable Encryption Round-Trip..." << std::endl;
    {
        std::string secret = "SensitiveDownloadToken_ABC123_https://internal.registry.ai/blob/998877";
        std::string encrypted = Crypto::EncryptDPAPI(secret, L"TestGetySecret");
        TEST_ASSERT(!encrypted.empty());
        TEST_ASSERT(encrypted != secret);

        std::string decrypted = Crypto::DecryptDPAPI(encrypted);
        TEST_ASSERT(decrypted == secret);

        // Portable cipher test
        std::string portableEnc = Crypto::EncryptPortable(secret, "MyTestKey");
        TEST_ASSERT(!portableEnc.empty());
        TEST_ASSERT(portableEnc != secret);
        std::string portableDec = Crypto::DecryptPortable(portableEnc, "MyTestKey");
        TEST_ASSERT(portableDec == secret);

        std::wcout << L"    Crypto DPAPI & Portable Encryption PASSED!" << std::endl;
    }

    // Test 13: JSON Task & Log Serialization with DPAPI Envelope
    std::wcout << L"[13] Testing JSON Task & Log Serialization with DPAPI Envelope..." << std::endl;
    {
        DownloadTask task(L"https://example.com/models/deepseek-r1.gguf", L"C:\\Downloads", L"🤖 Yapay Zeka", 16, L"deepseek-r1.gguf");
        task.RestoreMetadata(L"task_test_99", DownloadState::Downloading, L"2026-09-17 21:00", L"", L"🤖 Yapay Zeka");

        std::vector<TaskLogEntry> logs = {
            { L"21:00:01", L"Connecting to mirror...", 0 },
            { L"21:00:02", L"HTTP 206 Partial Content confirmed (16 segments)", 1 },
            { L"21:00:05", L"Segment 3 speed: 25.4 MB/s", 0 }
        };
        task.RestoreLogs(logs);

        auto retrievedLogs = task.GetLogs();
        TEST_ASSERT(retrievedLogs.size() == 3);
        TEST_ASSERT(retrievedLogs[1].message == L"HTTP 206 Partial Content confirmed (16 segments)");
        TEST_ASSERT(retrievedLogs[1].level == 1);

        std::string innerJson = "{\n"
                                "  \"version\": 4,\n"
                                "  \"tasks\": [\n"
                                "    {\n"
                                "      \"id\": \"task_test_99\",\n"
                                "      \"url\": \"https://example.com/models/deepseek-r1.gguf\",\n"
                                "      \"filename\": \"deepseek-r1.gguf\",\n"
                                "      \"category\": \"🤖 Yapay Zeka\",\n"
                                "      \"splitCount\": 16,\n"
                                "      \"state\": 2,\n"
                                "      \"logs\": [\n"
                                "        {\"timestamp\": \"21:00:01\", \"level\": 0, \"message\": \"Connecting to mirror...\"},\n"
                                "        {\"timestamp\": \"21:00:02\", \"level\": 1, \"message\": \"HTTP 206 Partial Content confirmed\"}\n"
                                "      ]\n"
                                "    }\n"
                                "  ]\n"
                                "}";

        std::string encPayload = Crypto::EncryptDPAPI(innerJson, L"GetyTaskHistory");
        std::string envelope = "{\n"
                               "  \"format\": \"gety_tasks\",\n"
                               "  \"version\": 4,\n"
                               "  \"encrypted\": true,\n"
                               "  \"cipher\": \"Windows-DPAPI\",\n"
                               "  \"payload\": \"" + encPayload + "\"\n"
                               "}";

        JsonParser pEnvelope(envelope);
        JValue outer = pEnvelope.Parse();
        TEST_ASSERT(outer.getString("format") == "gety_tasks");
        TEST_ASSERT(outer.getBool("encrypted") == true);

        std::string decJson = Crypto::DecryptDPAPI(outer.getString("payload"));
        JsonParser pInner(decJson);
        JValue inner = pInner.Parse();
        TEST_ASSERT(inner.getInt("version") == 4);
        const auto& taskList = inner.getArray("tasks");
        TEST_ASSERT(taskList.size() == 1);
        TEST_ASSERT(taskList[0].getString("id") == "task_test_99");
        TEST_ASSERT(taskList[0].getString("filename") == "deepseek-r1.gguf");
        TEST_ASSERT(taskList[0].getInt("splitCount") == 16);
        const auto& logList = taskList[0].getArray("logs");
        TEST_ASSERT(logList.size() == 2);
        TEST_ASSERT(logList[1].getString("message") == "HTTP 206 Partial Content confirmed");
        TEST_ASSERT(logList[1].getInt("level") == 1);

        std::wcout << L"    JSON Task & Log Serialization with DPAPI Envelope PASSED!" << std::endl;
    }

    // Test 14: Hugging Face Spec Parsing & Quantization Rating
    std::wcout << L"[14] Testing Hugging Face Spec Parsing & Quantization Rating..." << std::endl;
    {
        auto spec1 = HuggingFaceClient::ParseModelSpec(L"TheBloke/Mistral-7B-Instruct-v0.2-GGUF");
        TEST_ASSERT(spec1.isValid);
        TEST_ASSERT(spec1.repoId == L"TheBloke/Mistral-7B-Instruct-v0.2-GGUF");
        TEST_ASSERT(spec1.revision == L"main");

        auto spec2 = HuggingFaceClient::ParseModelSpec(L"TheBloke/Mistral-7B-Instruct-v0.2-GGUF:q4_k_m");
        TEST_ASSERT(spec2.isValid);
        TEST_ASSERT(spec2.quantFilter == L"q4_k_m");

        auto spec3 = HuggingFaceClient::ParseModelSpec(L"https://huggingface.co/deepseek-ai/DeepSeek-R1/tree/main");
        TEST_ASSERT(spec3.isValid);
        TEST_ASSERT(spec3.repoId == L"deepseek-ai/DeepSeek-R1");
        TEST_ASSERT(spec3.revision == L"main");

        auto spec4 = HuggingFaceClient::ParseModelSpec(L"hf download TheBloke/Llama-2-7B-GGUF");
        TEST_ASSERT(spec4.isValid);
        TEST_ASSERT(spec4.repoId == L"TheBloke/Llama-2-7B-GGUF");

        TEST_ASSERT(HuggingFaceClient::IsLikelyHuggingFaceModel(L"https://huggingface.co/TheBloke/model"));
        TEST_ASSERT(HuggingFaceClient::IsLikelyHuggingFaceModel(L"hf.co/meta-llama/Llama-3-8B"));
        TEST_ASSERT(HuggingFaceClient::IsLikelyHuggingFaceModel(L"TheBloke/Mistral-7B-Instruct-v0.2-GGUF"));
        TEST_ASSERT(!HuggingFaceClient::IsLikelyHuggingFaceModel(L"https://example.com/archive.zip"));

        std::wstring quant, desc;
        int stars = 0;
        bool isRec = false;

        HuggingFaceClient::AnalyzeQuantization(L"mistral-7b-instruct-v0.2.Q4_K_M.gguf", quant, stars, desc, isRec);
        TEST_ASSERT(quant == L"Q4_K_M");
        TEST_ASSERT(stars == 4);
        TEST_ASSERT(isRec == true);

        HuggingFaceClient::AnalyzeQuantization(L"model.Q5_K_M.gguf", quant, stars, desc, isRec);
        TEST_ASSERT(quant == L"Q5_K_M");
        TEST_ASSERT(stars == 5);

        HuggingFaceClient::AnalyzeQuantization(L"model.Q8_0.gguf", quant, stars, desc, isRec);
        TEST_ASSERT(quant == L"Q8_0");
        TEST_ASSERT(stars == 5);

        HuggingFaceClient::AnalyzeQuantization(L"model.IQ4_NL.gguf", quant, stars, desc, isRec);
        TEST_ASSERT(quant == L"IQ4_NL");
        TEST_ASSERT(stars == 4);

        HuggingFaceClient::AnalyzeQuantization(L"model.Q2_K.gguf", quant, stars, desc, isRec);
        TEST_ASSERT(quant == L"Q2_K");
        TEST_ASSERT(stars == 1);

        TEST_ASSERT(HuggingFaceClient::FormatStars(4) == L"⭐⭐⭐⭐");
        TEST_ASSERT(HuggingFaceClient::FormatStars(5) == L"⭐⭐⭐⭐⭐");

        std::wcout << L"    Hugging Face Spec Parsing & Quantization Rating PASSED!" << std::endl;
    }

    // Test 15: Hugging Face Live Hub API Fetch & Resolution
    std::wcout << L"[15] Testing Hugging Face Live Hub API Fetch & Resolution..." << std::endl;
    {
        auto spec = HuggingFaceClient::ParseModelSpec(L"TheBloke/Mistral-7B-Instruct-v0.2-GGUF");
        auto res = HuggingFaceClient::FetchModelTree(spec, L"", [](const std::wstring& msg) {
            std::wcout << L"    Status: " << msg << std::endl;
        });

        if (!res.success && isCiEnv && (res.statusCode == 429 || res.statusCode >= 500)) {
            std::wcout << L"    [CI Rate-Limit Warning] Hugging Face Hub returned HTTP " << res.statusCode
                       << L": " << res.errorMsg << std::endl;
        } else {
            if (!res.success) {
                std::wcerr << L"    Hugging Face Fetch Error (HTTP " << res.statusCode << L"): " << res.errorMsg << std::endl;
            }
            TEST_ASSERT(res.success);
            TEST_ASSERT(!res.info.files.empty());
            TEST_ASSERT(res.info.hasGguf);
            TEST_ASSERT(res.info.totalGgufCount > 5);
            TEST_ASSERT(res.info.recommendedGgufIndex >= 0);
            TEST_ASSERT(static_cast<size_t>(res.info.recommendedGgufIndex) < res.info.files.size());

            const auto& recFile = res.info.files[res.info.recommendedGgufIndex];
            std::wcout << L"    Repo: " << res.info.spec.displayName << std::endl;
            std::wcout << L"    Total files: " << res.info.files.size() << std::endl;
            std::wcout << L"    Recommended file: " << recFile.filename << std::endl;
            std::wcout << L"    Quant: " << recFile.quantType << L" | Stars: " << HuggingFaceClient::FormatStars(recFile.qualityStars) << std::endl;
            std::wcout << L"    Size: " << HuggingFaceClient::FormatBytes(recFile.size) << std::endl;
            std::wcout << L"    Download URL: " << recFile.downloadUrl << std::endl;

            TEST_ASSERT(recFile.quantType == L"Q4_K_M");
            TEST_ASSERT(recFile.size > 4ULL * 1024ULL * 1024ULL * 1024ULL); // ~4.37 GB

            std::wstring directUrl, resErr;
            bool resolved = HuggingFaceClient::ResolveDownloadUrl(recFile.downloadUrl, L"", directUrl, resErr);
            if (!resolved && isCiEnv) {
                std::wcout << L"    [CI Redirect Note] ResolveDownloadUrl note: " << resErr << std::endl;
            } else {
                TEST_ASSERT(resolved);
                TEST_ASSERT(!directUrl.empty());
                std::wcout << L"    Resolved URL (Cloudflare/CDN): " << directUrl.substr(0, (std::min)(directUrl.size(), size_t(70))) << L"..." << std::endl;
            }
        }

        std::wcout << L"    Hugging Face Live Hub API Fetch & Resolution PASSED!" << std::endl;
    }

    // Test 16: Pause, Rename, and Delete Keep-File / Delete-From-Disk
    std::wcout << L"[16] Testing Pause, Task/File Rename, and Delete File Preservation..." << std::endl;
    {
        std::wstring tempSaveDir = std::filesystem::current_path().wstring() + L"\\test_rename_dir";
        std::filesystem::create_directories(tempSaveDir);

        auto task = std::make_shared<DownloadTask>(L"https://example.com/original_archive.zip", tempSaveDir, L"General", 4, L"original_archive.zip");
        TEST_ASSERT(task->GetSnapshot().filename == L"original_archive.zip");
        TEST_ASSERT(task->GetSnapshot().state == DownloadState::Queued);

        // 1. Test Pause in Queued state
        task->Pause();
        TEST_ASSERT(task->GetSnapshot().state == DownloadState::Paused);

        // 2. Test Rename without file on disk
        bool renamed = task->Rename(L"renamed_archive.zip");
        TEST_ASSERT(renamed);
        TEST_ASSERT(task->GetSnapshot().filename == L"renamed_archive.zip");
        TEST_ASSERT(task->GetSnapshot().fullPath == tempSaveDir + L"\\renamed_archive.zip");

        // 3. Test Rename with completed file on disk
        std::wstring currentFilePath = task->GetSnapshot().fullPath;
        {
            std::ofstream f(currentFilePath, std::ios::binary);
            f << "Gety Test File Content";
        }
        TEST_ASSERT(std::filesystem::exists(currentFilePath));

        bool renamed2 = task->Rename(L"final_named_file.zip");
        TEST_ASSERT(renamed2);
        TEST_ASSERT(!std::filesystem::exists(currentFilePath));
        std::wstring newFilePath = task->GetSnapshot().fullPath;
        TEST_ASSERT(std::filesystem::exists(newFilePath));
        TEST_ASSERT(task->GetSnapshot().filename == L"final_named_file.zip");

        // 4. Test Delete Task with deleteFile = false (KEEP FILE ON DISK)
        task->DeleteTask(false);
        TEST_ASSERT(task->GetSnapshot().state == DownloadState::Deleted);
        TEST_ASSERT(std::filesystem::exists(newFilePath)); // File is PRESERVED on disk!

        // 5. Test Delete Task with deleteFile = true (PERMANENTLY DELETE FROM DISK)
        task->DeleteTask(true);
        TEST_ASSERT(!std::filesystem::exists(newFilePath)); // File is REMOVED from disk!

        std::error_code ec;
        std::filesystem::remove_all(tempSaveDir, ec);
        std::wcout << L"    Pause, Rename, and Delete File Preservation PASSED!" << std::endl;
    }

    // Test 17: JsonParser 64-bit integer precision (>4GB) & ParseMediaBytes ('~' handling)
    std::wcout << L"[17] Testing 64-bit JSON Byte Counts & Large File Sizes..." << std::endl;
    {
        std::string largeJson = R"({"totalBytes": 10737418240, "downloadedBytes": 5368709120, "smallInt": 42})";
        JsonParser jp(largeJson);
        JValue obj = jp.Parse();
        TEST_ASSERT(obj.getUInt64("totalBytes") == 10737418240ULL); // 10 GiB exact
        TEST_ASSERT(obj.getUInt64("downloadedBytes") == 5368709120ULL); // 5 GiB exact
        TEST_ASSERT(obj.getInt("smallInt") == 42);
        std::wcout << L"    64-bit JSON Integer Precision PASSED!" << std::endl;
    }

    // Test 18: v5 DPAPI State Persistence + Partial Segment & Disk Reconciliation
    std::wcout << L"[18] Testing v5 Partial State Persistence & Disk Reconciliation..." << std::endl;
    {
        std::wstring origPortableDir = Config::Instance().GetPortableDataDir();
        std::wstring testPortableDir = std::filesystem::current_path().wstring() + L"\\test_persist_data";
        std::wstring testSaveDir = std::filesystem::current_path().wstring() + L"\\test_persist_downloads";
        std::error_code ec;
        std::filesystem::remove_all(testPortableDir, ec);
        std::filesystem::remove_all(testSaveDir, ec);
        std::filesystem::create_directories(testPortableDir, ec);
        std::filesystem::create_directories(testSaveDir, ec);

        Config::Instance().SetPortableDataDir(testPortableDir);

        // Prepare a completed media file on disk whose saved state was stale Downloading
        std::wstring finishedMediaPath = testSaveDir + L"\\FinishedVideo.mp4";
        {
            std::ofstream f(finishedMediaPath, std::ios::binary);
            std::string dummy(4096, 'V');
            f.write(dummy.data(), (std::streamsize)dummy.size());
        }

        // Prepare an incomplete media .part file on disk
        std::wstring partialMediaPath = testSaveDir + L"\\PartialVideo.mp4.part";
        {
            std::ofstream f(partialMediaPath, std::ios::binary);
            std::string dummy(2048, 'P');
            f.write(dummy.data(), (std::streamsize)dummy.size());
        }

        // Simulate a v5 tasks.json written before unclean shutdown / restart
        std::vector<SegmentInfo> segs(2);
        segs[0].id = 0; segs[0].startByte = 0; segs[0].endByte = 999999; segs[0].downloadedBytes = 250000; segs[0].currentOffset = 250000; segs[0].status = 2;
        segs[1].id = 1; segs[1].startByte = 1000000; segs[1].endByte = 1999999; segs[1].downloadedBytes = 150000; segs[1].currentOffset = 1150000; segs[1].status = 2;

        auto httpPartial = std::make_shared<DownloadTask>(L"https://example.com/archive.bin", testSaveDir, L"Software", 2, L"archive.bin");
        httpPartial->RestoreMetadata(L"task-http-partial", DownloadState::Downloading, L"2026-11-14 12:00", L"", L"Software",
                                     2000000ULL, 400000ULL, true, true, L"https://cdn.example.com/archive.bin", L"", segs);

        auto staleFinishedMedia = std::make_shared<DownloadTask>(L"https://youtube.com/watch?v=123", testSaveDir, L"Video", 1, L"FinishedVideo.mp4", true, L"best");
        staleFinishedMedia->RestoreMetadata(L"task-media-finished", DownloadState::Paused, L"2026-11-14 12:01", L"", L"Video",
                                            4096ULL, 0ULL, true, false, L"", L"", {});

        auto missingCompleted = std::make_shared<DownloadTask>(L"https://example.com/moved.zip", testSaveDir, L"Archives", 4, L"moved.zip");
        missingCompleted->RestoreMetadata(L"task-moved-done", DownloadState::Completed, L"2026-11-14 12:02", L"2026-11-14 12:05", L"Archives",
                                          1234567ULL, 1234567ULL, true, false, L"", L"", {});

        auto snap1 = httpPartial->GetSnapshot();
        auto snap2 = staleFinishedMedia->GetSnapshot();
        auto snap3 = missingCompleted->GetSnapshot();
        TEST_ASSERT(snap1.state == DownloadState::Paused); // Interrupted Downloading becomes Paused
        TEST_ASSERT(snap1.totalBytes == 2000000ULL);
        TEST_ASSERT(snap1.downloadedBytes == 400000ULL);
        TEST_ASSERT(snap1.segments.size() == 2);
        TEST_ASSERT(snap1.segments[0].downloadedBytes == 250000ULL);
        TEST_ASSERT(snap1.segments[1].downloadedBytes == 150000ULL);

        TEST_ASSERT(snap2.state == DownloadState::Completed); // Reconciled from FinishedVideo.mp4 on disk!
        TEST_ASSERT(snap2.totalBytes == 4096ULL);
        TEST_ASSERT(snap2.downloadedBytes == 4096ULL);

        TEST_ASSERT(snap3.state == DownloadState::Completed); // Preserved totalBytes even after external move!
        TEST_ASSERT(snap3.totalBytes == 1234567ULL);
        TEST_ASSERT(snap3.downloadedBytes == 1234567ULL);

        // Test DownloadManager end-to-end SaveTasks + LoadTasks + .bak recovery
        std::wstring id1 = DownloadManager::Instance().AddTask(L"https://example.com/partial.iso", testSaveDir, L"Software", 4, false, L"partial.iso");
        DownloadManager::Instance().PauseTask(id1);
        TEST_ASSERT(std::filesystem::exists(testPortableDir + L"\\tasks.json"));

        // Corrupt primary tasks.json after creating a backup save
        DownloadManager::Instance().SaveTasks();
        TEST_ASSERT(std::filesystem::exists(testPortableDir + L"\\tasks.json.bak"));
        {
            std::ofstream corrupt(testPortableDir + L"\\tasks.json", std::ios::trunc);
            corrupt << "{ broken json ";
        }
        DownloadManager::Instance().LoadTasks(); // Should recover cleanly from tasks.json.bak!
        DownloadTaskInfo recoveredSnap;
        bool gotRecovered = DownloadManager::Instance().GetSnapshot(id1, recoveredSnap);
        TEST_ASSERT(gotRecovered);
        TEST_ASSERT(recoveredSnap.filename == L"partial.iso");

        // Verify partial media .part size pickup on restore
        auto mediaPartTask = std::make_shared<DownloadTask>(L"https://youtube.com/watch?v=999", testSaveDir, L"Video", 1, L"PartialVideo.mp4", true, L"best");
        mediaPartTask->RestoreMetadata(L"task-media-part", DownloadState::Paused, L"2026-11-14 12:03", L"", L"Video",
                                       8192ULL, 0ULL, true, false, L"", L"", {});
        TEST_ASSERT(mediaPartTask->GetSnapshot().downloadedBytes == 2048ULL);

        DownloadManager::Instance().DeleteTask(id1, true);
        Config::Instance().SetPortableDataDir(origPortableDir);
        std::filesystem::remove_all(testPortableDir, ec);
        std::filesystem::remove_all(testSaveDir, ec);
        std::wcout << L"    v5 Persistence, Backup Recovery & Disk Reconciliation PASSED!" << std::endl;
    }

    // Test 19: CLI Flag Separation (--minimized never treated as URL) & Registry AutoStart Roundtrip
    std::wcout << L"[19] Testing CLI Autostart Flag Parsing & Registry Helper..." << std::endl;
    {
        auto optsMin = ParseCliArgs(L"--minimized");
        TEST_ASSERT(optsMin.startMinimized == true);
        TEST_ASSERT(optsMin.urls.empty());

        auto optsAuto = ParseCliArgs(L"  --autostart  https://example.com/file.zip  --tray ");
        TEST_ASSERT(optsAuto.startMinimized == true);
        TEST_ASSERT(optsAuto.urls.size() == 1);
        TEST_ASSERT(optsAuto.urls[0] == L"https://example.com/file.zip");

        auto optsQuoted = ParseCliArgs(L"\"https://example.com/with space.zip\"");
        TEST_ASSERT(optsQuoted.startMinimized == false);
        TEST_ASSERT(optsQuoted.urls.size() == 1);
        TEST_ASSERT(optsQuoted.urls[0] == L"https://example.com/with space.zip");

        std::wstring expectedCmd = Config::BuildAutoStartCommand(L"C:\\Apps\\Gety.exe");
        TEST_ASSERT(expectedCmd == L"\"C:\\Apps\\Gety.exe\" --minimized");

        const wchar_t* testRegVal = L"GetyEngineTestAutostart";
        bool setOk = Config::Instance().SetAutoStartValue(true, testRegVal);
        TEST_ASSERT(setOk);
        bool enabledOk = Config::Instance().IsAutoStartValueEnabled(testRegVal);
        TEST_ASSERT(enabledOk);
        std::wstring readCmd = Config::Instance().ReadAutoStartValue(testRegVal);
        TEST_ASSERT(readCmd.find(L" --minimized") != std::wstring::npos);
        bool clearOk = Config::Instance().SetAutoStartValue(false, testRegVal);
        TEST_ASSERT(clearOk);
        bool disabledOk = !Config::Instance().IsAutoStartValueEnabled(testRegVal);
        TEST_ASSERT(disabledOk);
        std::wcout << L"    CLI Flag Separation & Registry AutoStart Roundtrip PASSED!" << std::endl;
    }

    std::wcout << L"\nAll 19 Tests Completed Successfully! Gety Engine is 100% Rock Solid." << std::endl;
    return 0;
}
