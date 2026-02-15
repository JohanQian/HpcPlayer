#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

class Looper : public std::enable_shared_from_this<Looper> {
public:
    explicit Looper(std::string name);
    ~Looper();

    Looper(const Looper&) = delete;
    Looper& operator=(const Looper&) = delete;
    Looper(Looper&&) = delete;
    Looper& operator=(Looper&&) = delete;

    void start();
    void stop();
    void quit();
    void post(std::function<void()> task);

private:
    void run();

    const std::string name_;
    std::thread thread_;

    std::mutex mutex_;
    std::condition_variable cv_;
    std::queue<std::function<void()>> task_queue_;

    std::atomic<bool> running_{false};
};
