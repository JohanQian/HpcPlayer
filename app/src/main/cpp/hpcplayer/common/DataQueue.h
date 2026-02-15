#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <queue>
#include <type_traits>

template <typename T>
class DataQueue {
public:
    explicit DataQueue(size_t maxSize = 0) : maxSize(maxSize) {}

    ~DataQueue() {
        abort();
    }

    DataQueue(const DataQueue&) = delete;
    DataQueue& operator=(const DataQueue&) = delete;
    DataQueue(DataQueue&&) = delete;
    DataQueue& operator=(DataQueue&&) = delete;

    void push(T item) {
        std::unique_lock<std::mutex> lock(mutex);
        condFull.wait(lock, [this] { return isAborted || maxSize == 0 || queue.size() < maxSize; });

        if (isAborted) {
            return;
        }

        queue.push(std::move(item));
        condEmpty.notify_one();
    }

    template <typename... Args>
    void emplace(Args&&... args) {
        std::unique_lock<std::mutex> lock(mutex);
        condFull.wait(lock, [this] { return isAborted || maxSize == 0 || queue.size() < maxSize; });

        if (isAborted) {
            return;
        }

        queue.emplace(std::forward<Args>(args)...);
        condEmpty.notify_one();
    }

    std::optional<T> waitAndPop() {
        std::unique_lock<std::mutex> lock(mutex);
        condEmpty.wait(lock, [this] { return isAborted || !queue.empty(); });

        if (isAborted && queue.empty()) {
            return std::nullopt;
        }

        if (queue.empty()) {
            return std::nullopt;
        }

        T item = std::move(queue.front());
        queue.pop();
        condFull.notify_one();
        return item;
    }

    bool waitAndPop(T* outItem) {
        if (!outItem) {
            return false;
        }
        std::unique_lock<std::mutex> lock(mutex);
        condEmpty.wait(lock, [this] { return isAborted || !queue.empty(); });

        if (isAborted && queue.empty()) {
            return false;
        }

        if (queue.empty()) {
            return false;
        }

        *outItem = std::move(queue.front());
        queue.pop();
        condFull.notify_one();
        return true;
    }

    template <class Rep, class Period>
    std::optional<T> waitAndPopFor(const std::chrono::duration<Rep, Period>& timeout) {
        std::unique_lock<std::mutex> lock(mutex);
        if (!condEmpty.wait_for(lock, timeout, [this] { return isAborted || !queue.empty(); })) {
            return std::nullopt;
        }

        if (isAborted && queue.empty()) {
            return std::nullopt;
        }

        if (queue.empty()) {
            return std::nullopt;
        }

        T item = std::move(queue.front());
        queue.pop();
        condFull.notify_one();
        return item;
    }

    std::optional<T> tryPop() {
        std::lock_guard<std::mutex> lock(mutex);
        if (queue.empty()) {
            return std::nullopt;
        }

        T item = std::move(queue.front());
        queue.pop();
        condFull.notify_one();
        return item;
    }

    void abort() {
        std::lock_guard<std::mutex> lock(mutex);
        isAborted = true;
        condEmpty.notify_all();
        condFull.notify_all();
    }

    void reset() {
        std::lock_guard<std::mutex> lock(mutex);
        isAborted = false;
    }

    void flush() {
        std::lock_guard<std::mutex> lock(mutex);
        std::queue<T> empty;
        std::swap(queue, empty);
        condFull.notify_all();
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex);
        return queue.size();
    }

    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex);
        return queue.empty();
    }

private:
    std::queue<T> queue;
    mutable std::mutex mutex;
    std::condition_variable condEmpty;
    std::condition_variable condFull;
    size_t maxSize;
    bool isAborted = false;
};
