#include "thread_pool.h"
#include <iostream>
#include <utility>

ThreadPool::ThreadPool(int worker_count,std::size_t max_size):max_size(max_size)
{
    if(max_size==0){
        throw std::invalid_argument("max_size must be greater than 0");
    }
    workers.reserve(worker_count);
    for (int i = 0; i < worker_count; i++)
    {
        workers.emplace_back(&ThreadPool::work, this);
    }
}
ThreadPool::~ThreadPool()
{
    {
        std::lock_guard<std::mutex> lock(mutex);
        stop = true;
    }
    not_empty.notify_all();
    not_full.notify_all();
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
            not_empty.wait(lock, [&]()
                    { return !tasks.empty() || stop; });
            if (stop && tasks.empty())
            {
                return;
            }
            temp = std::move(tasks.front());
            tasks.pop();
        }
        not_full.notify_one();
        temp();
    }
}


