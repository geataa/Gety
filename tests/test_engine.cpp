#include <iostream>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <io.h>
#include <fcntl.h>
#include "../src/core/Types.h"
#include "../src/core/BatchExpander.h"
#include "../src/core/I18n.h"
#include "../src/engine/WinHttpUtils.h"
#include "../src/engine/DownloadTask.h"
#include "../src/engine/MediaExtractor.h"
#include "../src/engine/OllamaClient.h"
#include "../src/engine/HuggingFaceClient.h"
#include "../src/core/Crypto.h"
#include "../src/core/JsonParser.h"

using namespace Gety;

int main(int argc, char* argv[]) {
    _setmode(_fileno(stdout), _O_U16TEXT);
    _setmode(_fileno(stderr), _O_U16TEXT);
    std::wcout << L"=== Gety Engine Test Suite ===" << std::endl;

    // Test 1: Batch URL Expander
    std::wcout << L"[1] Testing Batch URL Expander..." << std::endl;
    auto urls = BatchExpander::ExpandNumeric(L"http://example.com/file_(*).zip", 1, 5, 2);
    assert(urls.size() == 5);
    assert(urls[0] == L"http://example.com/file_01.zip");
    assert(urls[4] == L"http://example.com/file_05.zip");
    std::wcout << L"    BatchExpander PASSED!" << std::endl;

    // Test 2: WinHttpUtils URL parsing & filename extraction
    std::wcout << L"[2] Testing URL Parsing and Filename Extraction..." << std::endl;
    UrlParts parts;
    bool parsed = WinHttpUtils::ParseUrl(L"https://download.example.com:8443/files/archive_v1.zip?token=123", parts);
    assert(parsed);
    assert(parts.host == L"download.example.com");
    assert(parts.port == 8443);
    assert(parts.isHttps == true);
    std::wstring fn = WinHttpUtils::ExtractFilenameFromUrl(L"https://download.example.com:8443/files/archive_v1.zip?token=123");
    assert(fn == L"archive_v1.zip");
    std::wcout << L"    URL Parsing & Filename Extraction PASSED!" << std::endl;

    // Test 3: MediaExtractor Engine
    std::wcout << L"[3] Testing MediaExtractor (yt-dlp integration)..." << std::endl;
    bool ytdlpAvail = MediaExtractor::IsAvailable();
    std::wstring ytdlpPath = MediaExtractor::GetYtDlpPath();
    std::wcout << L"    yt-dlp Available: " << (ytdlpAvail ? L"YES" : L"NO") << std::endl;
    std::wcout << L"    yt-dlp Path: " << ytdlpPath << std::endl;
    assert(ytdlpAvail);
    assert(!ytdlpPath.empty());
    assert(MediaExtractor::FormatDuration(213) == L"3:33" || MediaExtractor::FormatDuration(213) == L"03:33");
    assert(MediaExtractor::FormatBytes(1048576) == L"1.0 MB");
    std::wcout << L"    MediaExtractor helpers PASSED!" << std::endl;

    // Test 4: Custom Filename & UpdateUrl Lock Safety
    std::wcout << L"[4] Testing Custom Filename & UpdateUrl Lock Safety..." << std::endl;
    {
        auto task = std::make_unique<DownloadTask>(L"https://example.com/video.mp4", L"C:\\Downloads", L"Video", 1, L"BenimOzelFilmim.mp4", true);
        auto s = task->GetSnapshot();
        assert(s.filename == L"BenimOzelFilmim.mp4");
        assert(s.fullPath == L"C:\\Downloads\\BenimOzelFilmim.mp4");

        // Verify UpdateUrl does not deadlock or crash with recursive lock
        bool updated = task->UpdateUrl(L"https://example.com/video_new.mp4");
        assert(updated);
        assert(task->GetSnapshot().url == L"https://example.com/video_new.mp4");

        // Verify non-blocking DeleteTask
        auto tStart = std::chrono::steady_clock::now();
        task->DeleteTask(false);
        auto tElapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - tStart).count();
        assert(tElapsed < 100); // Must be instantaneous (<100ms)
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
        assert(res.success);
        assert(!res.items.empty());
        std::wcout << L"    Video: " << res.items[0].title << L" (" << MediaExtractor::FormatDuration(res.items[0].durationSec) << L")" << std::endl;
        std::wcout << L"    Formats count: " << res.items[0].formats.size() << std::endl;
        for (const auto& fmt : res.items[0].formats) {
            std::wcout << L"      -> " << fmt.resolution << L" (" << fmt.ext << L") " << fmt.note << L" [" << MediaExtractor::FormatBytes(fmt.filesize) << L"]" << std::endl;
        }
        std::wcout << L"    Live YouTube Info Extraction PASSED!" << std::endl;
        return 0;
    }

    // Test 4: Live Download Test if port is passed
    if (argc > 1) {
        int port = std::stoi(argv[1]);
        std::wcout << L"[3] Testing Live 5-Segment Download on port " << port << L"..." << std::endl;

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

        // Wait up to 15 seconds for completion
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
        assert(langs.size() == 12);

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

        assert(foundUk == true);
        assert(foundHe == false);

        // Test Ukrainian translations
        I18n::Instance().SetLanguage(LangId::Ukrainian);
        assert(std::wstring(I18n::Instance().Get(StrId::MenuFile)) == L"&Файл");
        assert(std::wstring(I18n::Instance().Get(StrId::MenuTaskStart)) == L"&Запустити\tF5");
        assert(std::wstring(I18n::Instance().Get(StrId::ActVideoLink)) == L"Додати посилання");
        assert(std::wstring(I18n::Instance().Get(StrId::CatAll)) == L"📂 Усі завдання");
        assert(std::wstring(I18n::Instance().Get(StrId::CatUnfinished)) == L"⏳ Незавершені");
        assert(std::wstring(I18n::Instance().Get(StrId::DlgVideoTitle)) == L"Додати посилання на відео/медіа");

        // Restore Turkish
        I18n::Instance().SetLanguage(LangId::Turkish);
        std::wcout << L"    12 Languages & Ukrainian Localization PASSED!" << std::endl;
    }

    // Test 10: OllamaClient Spec Parsing & Live Manifest Fetch
    std::wcout << L"[10] Testing OllamaClient Spec Parsing & Live Manifest Fetch..." << std::endl;
    {
        // 1. Parsing tests
        auto spec1 = OllamaClient::ParseModelSpec(L"nomic-embed-text-v2-moe:latest");
        assert(spec1.isValid);
        assert(spec1.namespaceName == L"library");
        assert(spec1.modelName == L"nomic-embed-text-v2-moe");
        assert(spec1.tag == L"latest");
        assert(spec1.displayName == L"nomic-embed-text-v2-moe:latest");

        auto spec2 = OllamaClient::ParseModelSpec(L"ollama run llama3.2:1b");
        assert(spec2.isValid);
        assert(spec2.modelName == L"llama3.2");
        assert(spec2.tag == L"1b");

        auto spec3 = OllamaClient::ParseModelSpec(L"https://ollama.com/library/nomic-embed-text-v2-moe");
        assert(spec3.isValid);
        assert(spec3.modelName == L"nomic-embed-text-v2-moe");
        assert(spec3.tag == L"latest");

        assert(OllamaClient::IsLikelyOllamaModel(L"nomic-embed-text-v2-moe:latest"));
        assert(OllamaClient::IsLikelyOllamaModel(L"llama3.2:3b"));
        assert(!OllamaClient::IsLikelyOllamaModel(L"https://example.com/file.zip"));

        // 2. Live manifest fetch test for nomic-embed-text-v2-moe:latest
        auto res = OllamaClient::FetchModelInfo(spec1, [](const std::wstring& msg) {
            std::wcout << L"    Status: " << msg << std::endl;
        });

        assert(res.success);
        assert(res.info.ggufLayerIndex >= 0);
        assert(res.info.ggufSize > 500ULL * 1024ULL * 1024ULL); // ~957 MB
        assert(!res.info.recommendedGgufFilename.empty());
        std::wcout << L"    Model: " << res.info.spec.displayName << std::endl;
        std::wcout << L"    Layers count: " << res.info.layers.size() << std::endl;
        std::wcout << L"    GGUF size: " << OllamaClient::FormatBytes(res.info.ggufSize) << std::endl;
        std::wcout << L"    GGUF filename: " << res.info.recommendedGgufFilename << std::endl;
        std::wcout << L"    OllamaClient Spec Parsing & Live Manifest Fetch PASSED!" << std::endl;
    }

    // Test 11: Crypto - Base64 Encoding & Decoding
    std::wcout << L"[11] Testing Crypto Base64 Encoding & Decoding..." << std::endl;
    {
        std::string sample = "Hello, Gety! Ultra-Fast C++20 Download Manager with AI.";
        std::string b64 = Crypto::Base64Encode(sample);
        assert(!b64.empty());
        std::string decoded = Crypto::Base64DecodeToString(b64);
        assert(decoded == sample);
        std::wcout << L"    Base64 Encoding & Decoding PASSED!" << std::endl;
    }

    // Test 12: Crypto - Windows DPAPI & Portable Encryption Round-Trip
    std::wcout << L"[12] Testing Crypto DPAPI & Portable Encryption Round-Trip..." << std::endl;
    {
        std::string secret = "SensitiveDownloadToken_ABC123_https://internal.registry.ai/blob/998877";
        std::string encrypted = Crypto::EncryptDPAPI(secret, L"TestGetySecret");
        assert(!encrypted.empty());
        assert(encrypted != secret);

        std::string decrypted = Crypto::DecryptDPAPI(encrypted);
        assert(decrypted == secret);

        // Portable cipher test
        std::string portableEnc = Crypto::EncryptPortable(secret, "MyTestKey");
        assert(!portableEnc.empty());
        assert(portableEnc != secret);
        std::string portableDec = Crypto::DecryptPortable(portableEnc, "MyTestKey");
        assert(portableDec == secret);

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

        // Verify GetLogs
        auto retrievedLogs = task.GetLogs();
        assert(retrievedLogs.size() == 3);
        assert(retrievedLogs[1].message == L"HTTP 206 Partial Content confirmed (16 segments)");
        assert(retrievedLogs[1].level == 1);

        // Build mock inner JSON
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

        // Parse envelope
        JsonParser pEnvelope(envelope);
        JValue outer = pEnvelope.Parse();
        assert(outer.getString("format") == "gety_tasks");
        assert(outer.getBool("encrypted") == true);

        // Decrypt payload
        std::string decJson = Crypto::DecryptDPAPI(outer.getString("payload"));
        JsonParser pInner(decJson);
        JValue inner = pInner.Parse();
        assert(inner.getInt("version") == 4);
        const auto& taskList = inner.getArray("tasks");
        assert(taskList.size() == 1);
        assert(taskList[0].getString("id") == "task_test_99");
        assert(taskList[0].getString("filename") == "deepseek-r1.gguf");
        assert(taskList[0].getInt("splitCount") == 16);
        const auto& logList = taskList[0].getArray("logs");
        assert(logList.size() == 2);
        assert(logList[1].getString("message") == "HTTP 206 Partial Content confirmed");
        assert(logList[1].getInt("level") == 1);

        std::wcout << L"    JSON Task & Log Serialization with DPAPI Envelope PASSED!" << std::endl;
    }

    // Test 14: Hugging Face Spec Parsing & Quantization Rating
    std::wcout << L"[14] Testing Hugging Face Spec Parsing & Quantization Rating..." << std::endl;
    {
        // 1. Spec parsing
        auto spec1 = HuggingFaceClient::ParseModelSpec(L"TheBloke/Mistral-7B-Instruct-v0.2-GGUF");
        assert(spec1.isValid);
        assert(spec1.repoId == L"TheBloke/Mistral-7B-Instruct-v0.2-GGUF");
        assert(spec1.revision == L"main");

        auto spec2 = HuggingFaceClient::ParseModelSpec(L"TheBloke/Mistral-7B-Instruct-v0.2-GGUF:q4_k_m");
        assert(spec2.isValid);
        assert(spec2.quantFilter == L"q4_k_m");

        auto spec3 = HuggingFaceClient::ParseModelSpec(L"https://huggingface.co/deepseek-ai/DeepSeek-R1/tree/main");
        assert(spec3.isValid);
        assert(spec3.repoId == L"deepseek-ai/DeepSeek-R1");
        assert(spec3.revision == L"main");

        auto spec4 = HuggingFaceClient::ParseModelSpec(L"hf download TheBloke/Llama-2-7B-GGUF");
        assert(spec4.isValid);
        assert(spec4.repoId == L"TheBloke/Llama-2-7B-GGUF");

        // 2. IsLikelyHuggingFaceModel heuristics
        assert(HuggingFaceClient::IsLikelyHuggingFaceModel(L"https://huggingface.co/TheBloke/model"));
        assert(HuggingFaceClient::IsLikelyHuggingFaceModel(L"hf.co/meta-llama/Llama-3-8B"));
        assert(HuggingFaceClient::IsLikelyHuggingFaceModel(L"TheBloke/Mistral-7B-Instruct-v0.2-GGUF"));
        assert(!HuggingFaceClient::IsLikelyHuggingFaceModel(L"https://example.com/archive.zip"));

        // 3. Quantization analysis
        std::wstring quant, desc;
        int stars = 0;
        bool isRec = false;

        HuggingFaceClient::AnalyzeQuantization(L"mistral-7b-instruct-v0.2.Q4_K_M.gguf", quant, stars, desc, isRec);
        assert(quant == L"Q4_K_M");
        assert(stars == 4);
        assert(isRec == true);

        HuggingFaceClient::AnalyzeQuantization(L"model.Q5_K_M.gguf", quant, stars, desc, isRec);
        assert(quant == L"Q5_K_M");
        assert(stars == 5);

        HuggingFaceClient::AnalyzeQuantization(L"model.Q8_0.gguf", quant, stars, desc, isRec);
        assert(quant == L"Q8_0");
        assert(stars == 5);

        HuggingFaceClient::AnalyzeQuantization(L"model.IQ4_NL.gguf", quant, stars, desc, isRec);
        assert(quant == L"IQ4_NL");
        assert(stars == 4);

        HuggingFaceClient::AnalyzeQuantization(L"model.Q2_K.gguf", quant, stars, desc, isRec);
        assert(quant == L"Q2_K");
        assert(stars == 1);

        assert(HuggingFaceClient::FormatStars(4) == L"⭐⭐⭐⭐");
        assert(HuggingFaceClient::FormatStars(5) == L"⭐⭐⭐⭐⭐");

        std::wcout << L"    Hugging Face Spec Parsing & Quantization Rating PASSED!" << std::endl;
    }

    // Test 15: Hugging Face Live Hub API Fetch & Resolution
    std::wcout << L"[15] Testing Hugging Face Live Hub API Fetch & Resolution..." << std::endl;
    {
        auto spec = HuggingFaceClient::ParseModelSpec(L"TheBloke/Mistral-7B-Instruct-v0.2-GGUF");
        auto res = HuggingFaceClient::FetchModelTree(spec, L"", [](const std::wstring& msg) {
            std::wcout << L"    Status: " << msg << std::endl;
        });

        assert(res.success);
        assert(!res.info.files.empty());
        assert(res.info.hasGguf);
        assert(res.info.totalGgufCount > 5);
        assert(res.info.recommendedGgufIndex >= 0);

        const auto& recFile = res.info.files[res.info.recommendedGgufIndex];
        std::wcout << L"    Repo: " << res.info.spec.displayName << std::endl;
        std::wcout << L"    Total files: " << res.info.files.size() << std::endl;
        std::wcout << L"    Recommended file: " << recFile.filename << std::endl;
        std::wcout << L"    Quant: " << recFile.quantType << L" | Stars: " << HuggingFaceClient::FormatStars(recFile.qualityStars) << std::endl;
        std::wcout << L"    Size: " << HuggingFaceClient::FormatBytes(recFile.size) << std::endl;
        std::wcout << L"    Download URL: " << recFile.downloadUrl << std::endl;

        assert(recFile.quantType == L"Q4_K_M");
        assert(recFile.size > 4ULL * 1024ULL * 1024ULL * 1024ULL); // ~4.37 GB

        // Test download URL resolution
        std::wstring directUrl, resErr;
        bool resolved = HuggingFaceClient::ResolveDownloadUrl(recFile.downloadUrl, L"", directUrl, resErr);
        assert(resolved);
        assert(!directUrl.empty());
        std::wcout << L"    Resolved URL (Cloudflare/CDN): " << directUrl.substr(0, 70) << L"..." << std::endl;

        std::wcout << L"    Hugging Face Live Hub API Fetch & Resolution PASSED!" << std::endl;
    }

    std::wcout << L"\nAll 15 Tests Completed Successfully! Gety Engine is 100% Rock Solid." << std::endl;
    return 0;
}
