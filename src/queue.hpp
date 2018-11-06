#pragma once

#include <mutex>
#include <atomic>
#include <condition_variable>
#include <queue>

namespace threadsafe
{

template<class T>
class queue
{
public:
    queue() = default;
    ~queue() = default;

    void push(T data);
    T blocking_pop();

    std::queue<T> release();

private:
    void throw_if_released();
    T pop_front();

    std::mutex mutex_;
    std::atomic<bool> released_ {false};
    std::condition_variable on_push_or_release_;

    std::queue<T> queue_;
};

template<typename T>
void queue<T>::push(T data)
{
    std::lock_guard<std::mutex> l{mutex_};

    throw_if_released();

    queue_.push(std::move(data));
    on_push_or_release_.notify_one();
}

template<typename T>
T queue<T>::blocking_pop()
{
    std::unique_lock<std::mutex> lock{mutex_};

    on_push_or_release_.wait(lock, [this]{
        throw_if_released();
        return !queue_.empty();
    });

    return pop_front();
}

template<typename T>
std::queue<T> queue<T>::release()
{
    std::lock_guard<std::mutex> l{mutex_};

    released_ = true;
    on_push_or_release_.notify_all();

    return std::move(queue_);
}

template<typename T>
void queue<T>::throw_if_released()
{
    if (released_)
        throw std::runtime_error("Queue released");
}

template<typename T>
T queue<T>::pop_front()
{
    auto data = std::move(queue_.front());

    queue_.pop();

    return data;
}

} // namespace threadsafe
