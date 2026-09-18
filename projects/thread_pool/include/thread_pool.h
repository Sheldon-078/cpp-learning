#pragma once

#include <mutex>
#include <thread>
#include <queue>
#include <functional>
#include <condition_variable>
#include <vector>
#include <future>
#include <memory>
#include "move_only_function.h"


class ThreadPool
{
public:
    ThreadPool(int worker_count, std::size_t max_size);
    ~ThreadPool();
    void submit(std::function<void()> task);
    template <typename F, typename... Args>
    auto submit(F &&f, Args &&...args) -> std::future<decltype(std::forward<F>(f)(std::forward<Args>(args)...))>
    {
        using ReturnType = decltype(std::forward<F>(f)(std::forward<Args>(args)...));
        auto bound_task=std::bind(std::forward<F>(f),std::forward<Args>(args)...);
        auto wrapper=std::packaged_task<ReturnType()>(std::move(bound_task));
        std::future<ReturnType>result=wrapper.get_future();
        MoveOnlyFunction task(std::move(wrapper));
        {
            std::unique_lock<std::mutex> lock(mutex);
            not_full.wait(lock, [this]()
                          { return tasks.size() < max_size || stop; });
            if (stop)
            {
                throw std::runtime_error("submit on stopped ThreadPool");
            }
            tasks.push(std::move(task));
        }
        not_empty.notify_one();
        return result;
    }

private:
    std::vector<std::thread> workers;
    std::queue<MoveOnlyFunction> tasks;
    std::mutex mutex;
    std::condition_variable not_empty;
    std::condition_variable not_full;
    std::size_t max_size;
    bool stop = false;
    void work();
};
