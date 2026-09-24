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
#include "worker_queue.h"

class ThreadPool
{
public:
    ThreadPool(int worker_count, std::size_t max_size);
    ~ThreadPool();
    void submit(std::function<void()> task);
    template <typename F, typename... Args>
    auto submit(F &&f, Args &&...args) -> std::future<decltype(std::forward<F>(f)(std::forward<Args>(args)...))>
    {
        auto bound_task = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
        using ReturnType = decltype(std::forward<F>(f)(std::forward<Args>(args)...));
        auto wrapper = std::packaged_task<ReturnType()>(std::move(bound_task));
        std::future<ReturnType> result = wrapper.get_future();
        MoveOnlyFunction task(std::move(wrapper));
        {
            std::unique_lock<std::mutex> lock(state_mutex);
            not_full.wait(lock, [this]()
                          { return queued_tasks < max_size || stop; });
            if (stop)
            {
                throw std::runtime_error("submit on stopped ThreadPool");
            }
            queued_tasks++;
            try
            {
                queues[choose_queue()]->push(std::move(task));
            }
            catch (...)
            {
                --queued_tasks;
                lock.unlock();
                not_full.notify_one();
                throw;
            }
        }
        not_empty.notify_one();
        return result;
    }

private:
    std::vector<std::thread> workers;
    std::vector<std::unique_ptr<WorkerQueue>> queues;
    std::size_t next_queue{0};
    std::size_t queued_tasks{0};
    std::size_t choose_queue();
    std::mutex state_mutex;
    std::condition_variable not_empty;
    std::condition_variable not_full;
    std::size_t max_size;
    bool stop = false;
    void work(std::size_t worker_id);
    std::optional<MoveOnlyFunction> try_get_task(std::size_t worker_id);
};
