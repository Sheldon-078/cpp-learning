#pragma once

#include <mutex>
#include <thread>
#include <queue>
#include <functional>
#include <condition_variable>
#include <vector>
#include <future>
#include <memory>

class ThreadPool
{
public:
    ThreadPool(int worker_count);
    ~ThreadPool();
    void submit(std::function<void()> task);
    template<typename F,typename... Args>
    auto submit(F&& f,Args &&...args)->std::future<decltype(std::forward<F>(f)(std::forward<Args>(args)...))>{
        using ReturnType=decltype(std::forward<F>(f)(std::forward<Args>(args)...));
        auto bound_task=std::bind(std::forward<F>(f),std::forward<Args>(args)...);
        auto task_ptr=std::make_shared<std::packaged_task<ReturnType()>>(std::move(bound_task));
        std::future<ReturnType>result=task_ptr->get_future();
        std::function<void()>wrapper=[task_ptr](){
            (*task_ptr)();
        };
        {
            std::lock_guard<std::mutex>lock(mutex);
            if(stop){
                throw std::runtime_error("submit on stopped ThreadPool");
            }
            tasks.push(std::move(wrapper));
        }
        cv.notify_one();
        return result;
    }
private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    std::mutex mutex;
    std::condition_variable cv;
    bool stop = false;
    void work();
};
