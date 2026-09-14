#include "thread_pool.h"
#include <iostream>
#include <utility>

ThreadPool::ThreadPool(int worker_count)
{
    workers.reserve(worker_count);
    for (int i = 0; i < worker_count; i++)
    {
        workers.emplace_back(&ThreadPool::work, this);
    }
};
ThreadPool::~ThreadPool()
{
    {
        std::lock_guard<std::mutex> lock(mutex);
        stop = true;
    }
    cv.notify_all();
    for (auto &worker : workers)
    {
        worker.join();
    }
}
void ThreadPool::work()
{

    while (true)
    {
        std::function<void()> temp;
        {
            std::unique_lock<std::mutex> lock(mutex);
            cv.wait(lock, [&]()
                    { return !tasks.empty() || stop; });
            if (stop && tasks.empty())
            {
                return;
            }
            temp = std::move(tasks.front());
            tasks.pop();
        }
        temp();
    }
}


