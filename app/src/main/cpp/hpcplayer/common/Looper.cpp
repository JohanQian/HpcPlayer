#include "Looper.h"
#include "Handler.h"
#include "HpcLog.h"

namespace {
    constexpr char kTag[] = "Looper";
} 

Looper::Looper(std::string name) : name_(std::move(name)) {}

Looper::~Looper() {
    stop();
}

void Looper::start() {
    if (running_.exchange(true)) {
        return;
    }
    thread_ = std::thread(&Looper::run, this);
}

void Looper::stop() {
    if (!running_.exchange(false)) {
        return;
    }

    cv_.notify_one();
    if (thread_.joinable()) {
        thread_.join();
    }
}

void Looper::post(std::function<void()> task) {
    if (!running_.load()) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        task_queue_.push(std::move(task));
    }
    cv_.notify_one();
}

void Looper::run() {
    LOG_I("Looper [%s] started.", name_.c_str());
    while (running_.load()) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [this] { return !running_.load() || !task_queue_.empty(); });

            if (!running_.load() && task_queue_.empty()) {
                break;
            }

            if (!task_queue_.empty()) {
                task = std::move(task_queue_.front());
                task_queue_.pop();
            }
        }

        if (task) {
            task();
        }
    }
    LOG_I("Looper [%s] finished.", name_.c_str());
}
