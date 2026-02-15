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
    // Capture shared_from_this to keep the Looper alive until the thread finishes.
    // This prevents the Looper from being destroyed while the thread is still running,
    // which is crucial if the Looper is stopped from within its own thread.
    auto self = shared_from_this();
    thread_ = std::thread([this, self]() {
        this->run();
    });
}

void Looper::stop() {
    running_.store(false);
    cv_.notify_one();

    if (thread_.joinable()) {
        if (thread_.get_id() == std::this_thread::get_id()) {
            thread_.detach();
        } else {
            thread_.join();
        }
    }
}

void Looper::quit() {
    running_.store(false);
    cv_.notify_one();
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
