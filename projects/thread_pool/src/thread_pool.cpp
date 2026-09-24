#include "thread_pool.h"
#include <iostream>
#include <utility>

ThreadPool::ThreadPool(int worker_count, std::size_t max_size) : max_size(max_size)
{
    if (max_size == 0)
    {
        throw std::invalid_argument("max_size must be greater than 0");
    }
    if (worker_count <= 0)
    {
        throw std::invalid_argument("worker_count must be greater than 0");
    }
    for (int i = 0; i < worker_count; ++i)
    {
        queues.push_back(
            std::make_unique<WorkerQueue>());
    }
    workers.reserve(worker_count);
    try
    {
        for (int i = 0; i < worker_count; ++i)
        {
            workers.emplace_back(
                &ThreadPool::work,
                this,
                static_cast<std::size_t>(i));
        }
    }
    catch (...)
    {
        {
            std::lock_guard<std::mutex> lock(state_mutex);
            stop = true;
        }

        not_empty.notify_all();

        for (auto &worker : workers)
        {
            if (worker.joinable())
            {
                worker.join();
            }
        }

        throw;
    }
}
ThreadPool::~ThreadPool()
{
    {
        std::lock_guard<std::mutex> lock(state_mutex);
        stop = true;
    }
    not_empty.notify_all();
    not_full.notify_all();
    for (auto &worker : workers)
    {
        worker.join();
    }
}
std::size_t ThreadPool::choose_queue()
{
    return next_queue++ % queues.size();
}
std::optional<MoveOnlyFunction> ThreadPool::try_get_task(std::size_t worker_id)
{
    auto task = queues[worker_id]->try_pop();
    if (task)
    {
        return task;
    }
    for (std::size_t offset = 1;
         offset < queues.size();
         ++offset)
    {
        std::size_t victim =
            (worker_id + offset) % queues.size();

        task = queues[victim]->try_steal();
        if (task)
        {
            return task;
        }
    }
    return std::nullopt;
}

void ThreadPool::work(std::size_t worker_id)
{
    while (true)
    {
        {
            std::unique_lock<std::mutex> lock(state_mutex);
            not_empty.wait(lock, [this]()
                           { return queued_tasks > 0 || stop; });
            if (stop && queued_tasks == 0)
            {
                return;
            }
        }
        auto temp = try_get_task(worker_id);
        if (temp)
        {
            {
                std::lock_guard<std::mutex> lock(state_mutex);
                queued_tasks--;
            }
            not_full.notify_one();
            (*temp)();
        }
    }
}