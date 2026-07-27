#pragma once

#include <chrono>

namespace Eppo
{
    class Timer
    {
    public:
        Timer()
        {
            m_StartPoint = std::chrono::high_resolution_clock::now();
        }

        auto Reset() -> void
        {
            m_StartPoint = std::chrono::high_resolution_clock::now();
        }

        [[nodiscard]] auto GetElapsedMilliseconds() const
        {
            using namespace std::chrono;
            return duration_cast<nanoseconds>(high_resolution_clock::now() - m_StartPoint).count() * 0.001f * 0.001f * 0.001f;
        }

        [[nodiscard]] auto GetElapsedMicroseconds() const
        {
            using namespace std::chrono;
            return duration_cast<nanoseconds>(high_resolution_clock::now() - m_StartPoint).count() * 0.001f * 0.001f;
        }

    private:
        std::chrono::time_point<std::chrono::high_resolution_clock> m_StartPoint;
    };
}
