#pragma once

#include <chrono>

class Clock {
public:
    virtual ~Clock() = default;
    virtual int64_t GetPosition() = 0; // in microseconds
};

class SystemClock : public Clock {
public:
    SystemClock() : start_time_(std::chrono::steady_clock::now()) {}

    int64_t GetPosition() override {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::microseconds>(now - start_time_).count();
    }

private:
    std::chrono::steady_clock::time_point start_time_;
};
