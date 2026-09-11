#pragma once

#include <mutex>
#include <thread>
#include <queue>
#include <functional>
#include <condition_variable>
#include <vector>

class ThreadPool
{
public:
    ThreadPool(int worker_count);
    ~ThreadPool();
    void submit(std::function<void()> task);

private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    std::mutex mutex;
    std::condition_variable cv;
    bool stop = false;
    void work();
};
