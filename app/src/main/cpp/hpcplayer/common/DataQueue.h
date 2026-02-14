#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>
#include <optional>
#include <chrono>

/**
 * A thread-safe queue that blocks on Pop operations when the queue is empty
 * and can be aborted to unblock waiting threads.
 * Supports a maximum size to prevent unbounded growth.
 * @tparam T The type of the items in the queue.
 */
template <typename T>
class DataQueue {
public:
    /**
     * Constructs a DataQueue with an optional maximum size.
     * @param max_size The maximum number of items the queue can hold. 0 means unlimited.
     */
    explicit DataQueue(size_t max_size = 0) : max_size_(max_size), abort_(false) {}

    /**
     * Pushes an item to the back of the queue.
     * If the queue is full, this operation blocks until space is available or aborted.
     * If the queue has been aborted, this operation is a no-op.
     * @param item The item to push.
     */
    void Push(T item) {
        std::unique_lock<std::mutex> lock(mutex_);
        cond_full_.wait(lock, [this] { return (max_size_ == 0 || queue_.size() < max_size_) || abort_; });
        
        if (abort_) {
            return;
        }
        queue_.push(std::move(item));
        cond_empty_.notify_one();
    }

    /**
     * Emplaces an item to the back of the queue.
     * If the queue is full, this operation blocks until space is available or aborted.
     * If the queue has been aborted, this operation is a no-op.
     */
    template <typename... Args>
    void Emplace(Args&&... args) {
        std::unique_lock<std::mutex> lock(mutex_);
        cond_full_.wait(lock, [this] { return (max_size_ == 0 || queue_.size() < max_size_) || abort_; });

        if (abort_) {
            return;
        }
        queue_.emplace(std::forward<Args>(args)...);
        cond_empty_.notify_one();
    }

    /**
     * Waits until the queue is not empty or has been aborted, then pops an item.
     * @return std::optional<T> The popped item, or std::nullopt if aborted.
     */
    std::optional<T> WaitAndPop() {
        std::unique_lock<std::mutex> lock(mutex_);
        cond_empty_.wait(lock, [this] { return !queue_.empty() || abort_; });

        if (abort_ && queue_.empty()) {
            return std::nullopt;
        }

        auto item = std::move(queue_.front());
        queue_.pop();
        cond_full_.notify_one();
        return item;
    }

    /**
     * Waits until the queue is not empty or has been aborted, then pops an item.
     * @param out_item Pointer to store the popped item.
     * @return true if an item was popped, false if the queue was aborted and is empty.
     */
    bool WaitAndPop(T* out_item) {
        if (out_item == nullptr) {
            return false;
        }
        std::unique_lock<std::mutex> lock(mutex_);
        cond_empty_.wait(lock, [this] { return !queue_.empty() || abort_; });

        if (abort_ && queue_.empty()) {
            return false;
        }

        *out_item = std::move(queue_.front());
        queue_.pop();
        cond_full_.notify_one();
        return true;
    }

    /**
     * Waits for a specified duration for an item.
     * @param timeout The maximum duration to wait.
     * @return std::optional<T> The popped item, or std::nullopt if timeout or aborted.
     */
    template <class Rep, class Period>
    std::optional<T> WaitAndPopFor(const std::chrono::duration<Rep, Period>& timeout) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (!cond_empty_.wait_for(lock, timeout, [this] { return !queue_.empty() || abort_; })) {
            return std::nullopt; // Timeout
        }

        if (abort_ && queue_.empty()) {
            return std::nullopt;
        }

        auto item = std::move(queue_.front());
        queue_.pop();
        cond_full_.notify_one();
        return item;
    }

    /**
     * Tries to pop an item without blocking.
     * @return std::optional<T> The popped item, or std::nullopt if empty.
     */
    std::optional<T> TryPop() {
        std::unique_lock<std::mutex> lock(mutex_);
        if (queue_.empty()) {
            return std::nullopt;
        }
        auto item = std::move(queue_.front());
        queue_.pop();
        cond_full_.notify_one();
        return item;
    }

    /**
     * Wakes up all waiting threads and prevents any further operations.
     * After being aborted, the queue will drain any remaining items but will
     * not accept new ones.
     */
    void Abort() {
        std::unique_lock<std::mutex> lock(mutex_);
        abort_ = true;
        cond_empty_.notify_all();
        cond_full_.notify_all();
    }

    /**
     * Resets the abort state, allowing the queue to be reused.
     */
    void Reset() {
        std::unique_lock<std::mutex> lock(mutex_);
        abort_ = false;
    }

    /**
     * Removes all items from the queue. Does not reset the abort state.
     */
    void Flush() {
        std::unique_lock<std::mutex> lock(mutex_);
        std::queue<T> empty_queue;
        queue_.swap(empty_queue);
        cond_full_.notify_all(); // Notify producers that space is available
    }

    /**
     * Gets the current number of items in the queue.
     * @return The size of the queue.
     */
    size_t Size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

    /**
     * Checks if the queue is empty.
     * @return true if empty, false otherwise.
     */
    bool Empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }

private:
    std::queue<T> queue_;
    mutable std::mutex mutex_;
    std::condition_variable cond_empty_; // Wait here if queue is empty (for consumers)
    std::condition_variable cond_full_;  // Wait here if queue is full (for producers)
    size_t max_size_;
    bool abort_ = false;
};
