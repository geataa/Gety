#pragma once

#include "Types.h"
#include <mutex>
#include <chrono>

namespace Gety {

class SpeedLimiter {
public:
    static SpeedLimiter& Instance();

    void SetLimit(SpeedMode mode, int manualKbps, int backgroundKbps);
    void Throttle(size_t bytesToRead);

private:
    SpeedLimiter();
    ~SpeedLimiter() = default;

    std::mutex m_mutex;
    SpeedMode m_mode = SpeedMode::Unlimited;
    int m_targetBps = 0; // Bytes per second

    double m_tokens = 0.0;
    std::chrono::steady_clock::time_point m_lastCheck;
};

} // namespace Gety
