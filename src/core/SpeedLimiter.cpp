#include "SpeedLimiter.h"
#include <thread>
#include <algorithm>

namespace Gety {

SpeedLimiter& SpeedLimiter::Instance() {
    static SpeedLimiter instance;
    return instance;
}

SpeedLimiter::SpeedLimiter() {
    m_lastCheck = std::chrono::steady_clock::now();
}

void SpeedLimiter::SetLimit(SpeedMode mode, int manualKbps, int backgroundKbps) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_mode = mode;
    if (m_mode == SpeedMode::Unlimited) {
        m_targetBps = 0;
    } else if (m_mode == SpeedMode::ManualLimit) {
        m_targetBps = std::max(1, manualKbps) * 1024;
    } else if (m_mode == SpeedMode::Background) {
        int bg = (backgroundKbps > 0) ? backgroundKbps : 256;
        m_targetBps = bg * 1024;
    }
    m_tokens = (double)m_targetBps;
    m_lastCheck = std::chrono::steady_clock::now();
}

static inline int backgroundSpeedLimitKbps(int val) {
    return (val > 0) ? val : 256;
}

void SpeedLimiter::Throttle(size_t bytesToRead) {
    if (m_mode == SpeedMode::Unlimited || m_targetBps <= 0) {
        return;
    }

    std::unique_lock<std::mutex> lock(m_mutex);
    while (true) {
        auto now = std::chrono::steady_clock::now();
        double elapsedSec = std::chrono::duration<double>(now - m_lastCheck).count();
        m_lastCheck = now;

        m_tokens += elapsedSec * m_targetBps;
        if (m_tokens > m_targetBps) {
            m_tokens = (double)m_targetBps;
        }

        if (m_tokens >= (double)bytesToRead) {
            m_tokens -= (double)bytesToRead;
            break;
        }

        // Need to wait for tokens
        double missingTokens = (double)bytesToRead - m_tokens;
        double waitSec = missingTokens / (double)m_targetBps;
        if (waitSec > 0.5) waitSec = 0.5;
        int waitMs = std::max(1, (int)(waitSec * 1000.0));

        lock.unlock();
        std::this_thread::sleep_for(std::chrono::milliseconds(waitMs));
        lock.lock();
    }
}

} // namespace Gety
