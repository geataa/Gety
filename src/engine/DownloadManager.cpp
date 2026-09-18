#include "DownloadManager.h"
#include "MediaExtractor.h"
#include "../core/Config.h"
#include "../core/JsonParser.h"
#include "../core/Crypto.h"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <chrono>

namespace Gety {

DownloadManager& DownloadManager::Instance() {
    static DownloadManager instance;
    return instance;
}

DownloadManager::DownloadManager() = default;

DownloadManager::~DownloadManager() {
    Shutdown();
}

void DownloadManager::Init(UpdateCallback onUpdate) {
    m_onUpdate = onUpdate;
    LoadTasks();

    m_running = true;
    m_timerThread = std::thread([this]() {
        while (m_running.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
            if (!m_running.load()) break;

            TickQueue();

            if (m_onUpdate) {
                m_onUpdate();
            }
        }
    });
}

void DownloadManager::Shutdown() {
    m_running = false;
    if (m_timerThread.joinable()) {
        m_timerThread.join();
    }

    PauseAll();
    SaveTasks();
}

void DownloadManager::TickQueue() {
    std::lock_guard<std::mutex> lock(m_mutex);

    int activeCount = 0;
    for (const auto& t : m_tasks) {
        DownloadState s = t->GetState();
        if (s == DownloadState::Downloading || s == DownloadState::Connecting) {
            activeCount++;
        }
    }

    int maxAllowed = Config::Instance().maxActiveDownloads;
    if (activeCount < maxAllowed) {
        for (auto& t : m_tasks) {
            if (t->GetState() == DownloadState::Queued) {
                t->Start();
                activeCount++;
                if (activeCount >= maxAllowed) {
                    break;
                }
            }
        }
    }
}

std::wstring DownloadManager::AddTask(const std::wstring& url,
                                     const std::wstring& saveDir,
                                     const std::wstring& category,
                                     int splitParts,
                                     bool startImmediately,
                                     const std::wstring& customFilename)
{
    if (MediaExtractor::IsMediaUrl(url)) {
        return AddMediaTask(url, saveDir, category.empty() ? L"Video" : category, customFilename, L"bestvideo+bestaudio/best", startImmediately);
    }

    auto task = std::make_shared<DownloadTask>(url, saveDir, category, splitParts, customFilename);
    task->SetEventCallback([this](const std::wstring& /*id*/) {
        if (m_onUpdate) m_onUpdate();
    });

    std::wstring taskId = task->GetId();

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_tasks.push_back(task);
    }

    if (startImmediately) {
        task->Start();
    }

    SaveTasks();
    if (m_onUpdate) m_onUpdate();

    return taskId;
}

std::wstring DownloadManager::AddMediaTask(const std::wstring& url,
                                          const std::wstring& saveDir,
                                          const std::wstring& category,
                                          const std::wstring& title,
                                          const std::wstring& formatSelector,
                                          bool startImmediately)
{
    std::wstring sDir = saveDir.empty() ? Config::Instance().defaultSavePath : saveDir;
    std::wstring cat = category.empty() ? L"Video" : category;
    auto task = std::make_shared<DownloadTask>(url, sDir, cat, 1, title, true, formatSelector);
    task->SetEventCallback([this](const std::wstring& /*id*/) {
        if (m_onUpdate) m_onUpdate();
    });

    std::wstring taskId = task->GetId();

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_tasks.push_back(task);
    }

    if (startImmediately) {
        task->Start();
    }

    SaveTasks();
    if (m_onUpdate) m_onUpdate();

    return taskId;
}

void DownloadManager::StartTask(const std::wstring& id) {
    std::shared_ptr<DownloadTask> target;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto& t : m_tasks) {
            if (t->GetId() == id) {
                target = t;
                break;
            }
        }
    }
    if (target) {
        target->Start();
    }
}

void DownloadManager::PauseTask(const std::wstring& id) {
    std::shared_ptr<DownloadTask> target;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto& t : m_tasks) {
            if (t->GetId() == id) {
                target = t;
                break;
            }
        }
    }
    if (target) {
        target->Pause();
    }
}

void DownloadManager::StopTask(const std::wstring& id) {
    PauseTask(id);
}

void DownloadManager::RedownloadTask(const std::wstring& id) {
    std::shared_ptr<DownloadTask> target;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto& t : m_tasks) {
            if (t->GetId() == id) {
                target = t;
                break;
            }
        }
    }
    if (target) {
        target->Redownload();
    }
}

void DownloadManager::DeleteTask(const std::wstring& id, bool deleteFile) {
    std::shared_ptr<DownloadTask> target;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto it = m_tasks.begin(); it != m_tasks.end(); ++it) {
            if ((*it)->GetId() == id) {
                target = *it;
                m_tasks.erase(it);
                break;
            }
        }
    }
    if (target) {
        std::thread([target, deleteFile]() {
            target->DeleteTask(deleteFile);
        }).detach();
    }
    SaveTasks();
    if (m_onUpdate) m_onUpdate();
}

bool DownloadManager::UpdateTaskUrl(const std::wstring& id, const std::wstring& newUrl) {
    std::shared_ptr<DownloadTask> target;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto& t : m_tasks) {
            if (t && t->GetId() == id) {
                target = t;
                break;
            }
        }
    }
    if (target) {
        bool res = target->UpdateUrl(newUrl);
        SaveTasks();
        if (m_onUpdate) m_onUpdate();
        return res;
    }
    return false;
}

void DownloadManager::MoveTaskUp(const std::wstring& id) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (size_t i = 1; i < m_tasks.size(); ++i) {
        if (m_tasks[i]->GetId() == id) {
            std::swap(m_tasks[i], m_tasks[i - 1]);
            break;
        }
    }
    SaveTasks();
    if (m_onUpdate) m_onUpdate();
}

void DownloadManager::MoveTaskDown(const std::wstring& id) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (size_t i = 0; i + 1 < m_tasks.size(); ++i) {
        if (m_tasks[i]->GetId() == id) {
            std::swap(m_tasks[i], m_tasks[i + 1]);
            break;
        }
    }
    SaveTasks();
    if (m_onUpdate) m_onUpdate();
}

void DownloadManager::PauseAll() {
    std::vector<std::shared_ptr<DownloadTask>> copyList;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        copyList = m_tasks;
    }
    for (auto& t : copyList) {
        t->Pause();
    }
}

void DownloadManager::ResumeAll() {
    std::vector<std::shared_ptr<DownloadTask>> copyList;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        copyList = m_tasks;
    }
    for (auto& t : copyList) {
        if (t->GetState() == DownloadState::Paused) {
            t->Start();
        }
    }
}

std::vector<DownloadTaskInfo> DownloadManager::GetAllSnapshots() const {
    std::vector<DownloadTaskInfo> list;
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& t : m_tasks) {
        list.push_back(t->GetSnapshot());
    }
    return list;
}

bool DownloadManager::GetSnapshot(const std::wstring& id, DownloadTaskInfo& outInfo) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& t : m_tasks) {
        if (t->GetId() == id) {
            outInfo = t->GetSnapshot();
            return true;
        }
    }
    return false;
}

int DownloadManager::GetActiveCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    int count = 0;
    for (const auto& t : m_tasks) {
        DownloadState s = t->GetState();
        if (s == DownloadState::Downloading || s == DownloadState::Connecting) {
            count++;
        }
    }
    return count;
}

int DownloadManager::GetCompletedCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    int count = 0;
    for (const auto& t : m_tasks) {
        if (t->GetState() == DownloadState::Completed) {
            count++;
        }
    }
    return count;
}

int DownloadManager::GetPausedCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    int count = 0;
    for (const auto& t : m_tasks) {
        if (t->GetState() == DownloadState::Paused) {
            count++;
        }
    }
    return count;
}

double DownloadManager::GetTotalSpeedBps() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    double total = 0.0;
    for (const auto& t : m_tasks) {
        if (t->GetState() == DownloadState::Downloading) {
            total += t->GetSnapshot().currentSpeedBps;
        }
    }
    return total;
}

static std::wstring GetCurrentIsoDateTime() {
    SYSTEMTIME st;
    GetLocalTime(&st);
    wchar_t buf[64];
    swprintf_s(buf, L"%04d-%02d-%02d %02d:%02d:%02d", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    return buf;
}

void DownloadManager::SaveTasks() {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::ostringstream innerJson;
    innerJson << "{\n";
    innerJson << "  \"version\": 4,\n";
    innerJson << "  \"savedAt\": \"" << JsonParser::WideToUtf8(GetCurrentIsoDateTime()) << "\",\n";
    innerJson << "  \"taskCount\": " << m_tasks.size() << ",\n";
    innerJson << "  \"tasks\": [\n";

    for (size_t i = 0; i < m_tasks.size(); ++i) {
        const auto& t = m_tasks[i];
        auto snap = t->GetSnapshot();
        auto logs = t->GetLogs();

        innerJson << "    {\n";
        innerJson << "      \"id\": \"" << JsonParser::EscapeString(JsonParser::WideToUtf8(snap.id)) << "\",\n";
        innerJson << "      \"url\": \"" << JsonParser::EscapeString(JsonParser::WideToUtf8(snap.url)) << "\",\n";
        innerJson << "      \"filename\": \"" << JsonParser::EscapeString(JsonParser::WideToUtf8(snap.filename)) << "\",\n";
        innerJson << "      \"saveDirectory\": \"" << JsonParser::EscapeString(JsonParser::WideToUtf8(snap.saveDirectory)) << "\",\n";
        innerJson << "      \"category\": \"" << JsonParser::EscapeString(JsonParser::WideToUtf8(snap.category)) << "\",\n";
        innerJson << "      \"splitCount\": " << snap.splitCount << ",\n";
        innerJson << "      \"state\": " << (int)snap.state << ",\n";
        innerJson << "      \"addedDate\": \"" << JsonParser::EscapeString(JsonParser::WideToUtf8(snap.addedDate)) << "\",\n";
        innerJson << "      \"completedDate\": \"" << JsonParser::EscapeString(JsonParser::WideToUtf8(snap.completedDate)) << "\",\n";
        innerJson << "      \"isMedia\": " << (t->IsMediaTask() ? "true" : "false") << ",\n";
        innerJson << "      \"mediaFormat\": \"" << JsonParser::EscapeString(JsonParser::WideToUtf8(t->GetMediaFormat())) << "\",\n";

        // Task logs array
        innerJson << "      \"logs\": [\n";
        for (size_t j = 0; j < logs.size(); ++j) {
            innerJson << "        {\n";
            innerJson << "          \"timestamp\": \"" << JsonParser::EscapeString(JsonParser::WideToUtf8(logs[j].timestamp)) << "\",\n";
            innerJson << "          \"level\": " << logs[j].level << ",\n";
            innerJson << "          \"message\": \"" << JsonParser::EscapeString(JsonParser::WideToUtf8(logs[j].message)) << "\"\n";
            innerJson << "        }" << (j + 1 < logs.size() ? "," : "") << "\n";
        }
        innerJson << "      ]\n";

        innerJson << "    }" << (i + 1 < m_tasks.size() ? "," : "") << "\n";
    }
    innerJson << "  ]\n";
    innerJson << "}\n";

    std::string innerStr = innerJson.str();

    // Encrypt payload with Windows DPAPI
    std::string encryptedPayload = Crypto::EncryptDPAPI(innerStr, L"GetyTaskHistory");

    // Write secure JSON container
    std::ostringstream envelope;
    envelope << "{\n";
    envelope << "  \"format\": \"gety_tasks\",\n";
    envelope << "  \"version\": 4,\n";
    envelope << "  \"encrypted\": true,\n";
    envelope << "  \"cipher\": \"Windows-DPAPI\",\n";
    envelope << "  \"taskCount\": " << m_tasks.size() << ",\n";
    envelope << "  \"payload\": \"" << encryptedPayload << "\"\n";
    envelope << "}\n";

    std::wstring jsonPath = Config::Instance().GetPortableDataDir() + L"\\tasks.json";
    std::ofstream out(std::filesystem::path(jsonPath), std::ios::out | std::ios::trunc);
    if (out.is_open()) {
        out << envelope.str();
        out.close();
    }
}

void DownloadManager::LoadTasks() {
    std::wstring jsonPath = Config::Instance().GetPortableDataDir() + L"\\tasks.json";
    std::filesystem::path fsJsonPath(jsonPath);

    if (std::filesystem::exists(fsJsonPath)) {
        std::ifstream in(fsJsonPath);
        if (in.is_open()) {
            std::stringstream ss;
            ss << in.rdbuf();
            in.close();

            std::string fileContent = ss.str();
            JsonParser outerParser(fileContent);
            JValue outerDoc = outerParser.Parse();

            std::string innerJsonStr;
            bool isEncrypted = outerDoc.getBool("encrypted", false);
            if (isEncrypted) {
                std::string payloadB64 = outerDoc.getString("payload");
                innerJsonStr = Crypto::DecryptDPAPI(payloadB64);
            } else {
                if (outerDoc.has("tasks")) {
                    innerJsonStr = fileContent;
                }
            }

            if (!innerJsonStr.empty()) {
                JsonParser innerParser(innerJsonStr);
                JValue doc = innerParser.Parse();
                const auto& tasksArr = doc.getArray("tasks");

                std::lock_guard<std::mutex> lock(m_mutex);
                m_tasks.clear();

                for (const auto& taskVal : tasksArr) {
                    std::wstring wId = JsonParser::Utf8ToWide(taskVal.getString("id"));
                    std::wstring wUrl = JsonParser::Utf8ToWide(taskVal.getString("url"));
                    std::wstring wFn = JsonParser::Utf8ToWide(taskVal.getString("filename"));
                    std::wstring wSdir = JsonParser::Utf8ToWide(taskVal.getString("saveDirectory"));
                    std::wstring wCat = JsonParser::Utf8ToWide(taskVal.getString("category"));
                    int splitCount = (int)taskVal.getInt("splitCount", 10);
                    int stateVal = (int)taskVal.getInt("state", 0);
                    std::wstring wAdded = JsonParser::Utf8ToWide(taskVal.getString("addedDate"));
                    std::wstring wCompleted = JsonParser::Utf8ToWide(taskVal.getString("completedDate"));
                    bool isMedia = taskVal.getBool("isMedia", false);
                    std::wstring mediaFmt = JsonParser::Utf8ToWide(taskVal.getString("mediaFormat"));

                    auto task = std::make_shared<DownloadTask>(wUrl, wSdir, wCat, splitCount, wFn, isMedia, mediaFmt);
                    task->RestoreMetadata(wId, (DownloadState)stateVal, wAdded, wCompleted, wCat);

                    // Restore logs
                    const auto& logsArr = taskVal.getArray("logs");
                    std::vector<TaskLogEntry> restoredLogs;
                    for (const auto& logItem : logsArr) {
                        TaskLogEntry entry;
                        entry.timestamp = JsonParser::Utf8ToWide(logItem.getString("timestamp"));
                        entry.level = (int)logItem.getInt("level", 0);
                        entry.message = JsonParser::Utf8ToWide(logItem.getString("message"));
                        restoredLogs.push_back(entry);
                    }
                    task->RestoreLogs(restoredLogs);

                    task->SetEventCallback([this](const std::wstring&) {
                        if (m_onUpdate) m_onUpdate();
                    });

                    m_tasks.push_back(task);
                }
                return;
            }
        }
    }

    // Legacy fallback: tasks.dat
    std::wstring legacyPath = Config::Instance().GetPortableDataDir() + L"\\tasks.dat";
    std::filesystem::path fsLegacy(legacyPath);
    if (std::filesystem::exists(fsLegacy)) {
        std::ifstream in(fsLegacy);
        if (in.is_open()) {
            auto readTrimmedLine = [&in](std::string& line) -> bool {
                if (!std::getline(in, line)) return false;
                if (!line.empty() && line.back() == '\r') line.pop_back();
                return true;
            };

            std::string firstLine;
            bool shouldMigrate = false;
            if (readTrimmedLine(firstLine)) {
                bool isVersion2Or3 = false;
                size_t count = 0;
                if (firstLine == "VERSION 3" || firstLine == "VERSION 2") {
                    isVersion2Or3 = true;
                    std::string countLine;
                    if (readTrimmedLine(countLine)) {
                        try { count = std::stoull(countLine); } catch (...) {}
                    }
                } else {
                    try { count = std::stoull(firstLine); } catch (...) {}
                }

                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    m_tasks.clear();

                    for (size_t i = 0; i < count; ++i) {
                        std::string sId, sUrl, sFn, sSdir, sCat, sSplit, sState, sAdded, sCompleted;
                        if (!readTrimmedLine(sId) || !readTrimmedLine(sUrl) || !readTrimmedLine(sFn) ||
                            !readTrimmedLine(sSdir) || !readTrimmedLine(sCat) || !readTrimmedLine(sSplit) ||
                            !readTrimmedLine(sState) || !readTrimmedLine(sAdded) || !readTrimmedLine(sCompleted)) {
                            break;
                        }

                        int splitCount = 10;
                        try { splitCount = std::stoi(sSplit); } catch (...) {}
                        int stateVal = 0;
                        try { stateVal = std::stoi(sState); } catch (...) {}

                        bool isMedia = false;
                        std::wstring mediaFmt;
                        if (isVersion2Or3) {
                            std::string sMediaFlag, sMediaFmt;
                            if (readTrimmedLine(sMediaFlag) && readTrimmedLine(sMediaFmt)) {
                                isMedia = (sMediaFlag == "1");
                                if (sMediaFmt != "-") mediaFmt = MediaExtractor::Utf8ToWide(sMediaFmt);
                            }
                        }

                        std::wstring wId = MediaExtractor::Utf8ToWide(sId);
                        std::wstring wUrl = MediaExtractor::Utf8ToWide(sUrl);
                        std::wstring wSdir = MediaExtractor::Utf8ToWide(sSdir);
                        std::wstring wCat = MediaExtractor::Utf8ToWide(sCat);
                        std::wstring wFn = MediaExtractor::Utf8ToWide(sFn);
                        std::wstring wAdded = MediaExtractor::Utf8ToWide(sAdded);
                        std::wstring wCompleted = MediaExtractor::Utf8ToWide(sCompleted);

                        auto task = std::make_shared<DownloadTask>(wUrl, wSdir, wCat, splitCount, wFn, isMedia, mediaFmt);
                        task->RestoreMetadata(wId, (DownloadState)stateVal, wAdded, wCompleted, wCat);
                        task->SetEventCallback([this](const std::wstring&) {
                            if (m_onUpdate) m_onUpdate();
                        });
                        m_tasks.push_back(task);
                    }
                    shouldMigrate = !m_tasks.empty();
                }
            }
            in.close();

            // Migrate legacy tasks.dat to encrypted tasks.json immediately
            if (shouldMigrate) {
                SaveTasks();
            }
        }
    }
}

} // namespace Gety
