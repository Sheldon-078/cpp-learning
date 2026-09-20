#include "thread_pool.h"
#include <iostream>
#include <utility>

ThreadPool::ThreadPool(int worker_count, std::size_t max_size) : max_size(max_size)
{
    if (max_size == 0)
    {
        throw std::invalid_argument("max_size must be greater than 0");
    }
    if (worker_count == 0 || worker_count < 0)
    {
        throw std::invalid_argument("worker_count must be greater than 0");
    }
    workers.reserve(worker_count);
    for (int i = 0; i < worker_count; i++)
    {
        workers.emplace_back(&ThreadPool::work, this, i);
    }
    for (int i = 0; i < worker_count; ++i)
    {
        queues.push_back(
            std::make_unique<WorkerQueue>());
    }
    for (int i = 0; i < worker_count; ++i)
    {
        workers.emplace_back(
            &ThreadPool::work,
            this,
            i);
    }
}
ThreadPool::~ThreadPool()
{
    {
        std::lock_guard<std::mutex> lock(state_mutex);
        stop = true;
    }
    start_cv.notify_all();
    not_empty.notify_all();
    not_full.notify_all();
    for (auto &worker : workers)
    {
        worker.join();
    }
}
std::size_t ThreadPool::choose_queue()
{
    return next_queue.fetch_add(1) % queues.size();
}
std::optional<MoveOnlyFunction> ThreadPool::try_get_task(std::size_t worker_id)
{
    auto task = queues[worker_id]->try_pop();
    if (task)
    {
        local_pop_count.fetch_add(1); //
        return task;
    }
    for (std::size_t offset = 1;
         offset < queues.size();
         ++offset)
    {
        std::size_t victim =
            (worker_id + offset) % queues.size();

        task = queues[victim]->try_steal();
        steal_attempt_count.fetch_add(1);
        if (task)
        {
            steal_count.fetch_add(1); //
            return task;
        }
    }
    if (task)
    {
        steal_count.fetch_add(1); //
        return task;
    }

    task_fail_count.fetch_add(1);
    return std::nullopt;
}
std::size_t ThreadPool::get_steal_count() const
{
    return steal_count.load();
}
std::size_t ThreadPool::get_local_pop_count() const
{
    return local_pop_count.load();
}
std::size_t ThreadPool::get_task_fail_count() const
{
    return task_fail_count.load();
}
std::size_t ThreadPool::get_steal_attempt_count() const
{
    return steal_attempt_count.load();
}
void ThreadPool::start(){
    {
        std::lock_guard<std::mutex>lock(state_mutex);
        workers_enabled=true;
    }
    start_cv.notify_all();
}
void ThreadPool::work(std::size_t worker_id)
{
    {
        std::unique_lock<std::mutex>lock(state_mutex);
        start_cv.wait(lock,[this](){
            return workers_enabled||stop;
        });
    }
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