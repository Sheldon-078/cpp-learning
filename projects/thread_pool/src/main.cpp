#include "thread_pool.h"
#include <iostream>
#include <atomic>
#include <cassert>
#include <chrono>
int add(int a, int b)
{
    return a + b;
}
double adddouble(double a, double b)
{
    return a * b;
}
std::string str(std::string temp)
{
    return temp;
}
int adderror(int a)
{
    throw std::runtime_error("error");
}
struct BenchmarkResult
{
    double elapsed;
    double throughput;
    std::uint64_t checksum;
};
BenchmarkResult run_benchmark(
    int worker_count,
    int task_count,
    int loops_per_task)
{

    ThreadPool pool(worker_count, task_count);
    std::vector<std::uint64_t> results(task_count);
    std::atomic<int> completed{0};
    std::promise<void> done;
    auto done_future = done.get_future();
    auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < task_count; i++)
    {
        pool.submit([i, &results, &completed, &done, task_count, loops_per_task]()
                    {
                            std::uint64_t x = static_cast<std::uint64_t>(i + 1);

                            for (int j = 0; j < loops_per_task; ++j)
                            {
                                x = x * 1664525ULL + 1013904223ULL;
                            }

                            results[i] = x;
                            if(completed.fetch_add(1)+1==task_count){
                                done.set_value();
                            } });
    }
    done_future.wait();
    auto end = std::chrono::steady_clock::now();
    std::uint64_t checksum = 0;
    for (auto x : results)
    {
        checksum += x;
    }
    auto elapsed =
        std::chrono::duration<double>(end - start).count();
    double throughput = task_count / elapsed;
    std::cout << "elapsed = " << elapsed << " s\n";
    std::cout << "throughput = " << throughput << " tasks/s\n";
    std::cout << "checksum = " << checksum << '\n';
    BenchmarkResult result;
    result.elapsed = elapsed;
    result.throughput = throughput;
    result.checksum = checksum;
    return result;
}
int main()
{
    std::cout<<"100*1000000"<<std::endl;
    for (int i = 0; i < 3; i++)
    {
        run_benchmark(4, 100, 1000000);
        run_benchmark(8, 100, 1000000);
    }
    std::cout<<"1000*100000"<<std::endl;
    for (int i = 0; i < 3; i++)
    {
        run_benchmark(4, 1000, 100000);
        run_benchmark(8, 1000, 100000);
    }
    std::cout<<"10000*10000"<<std::endl;
    for (int i = 0; i < 3; i++)
    {
        run_benchmark(4, 10000, 10000);
        run_benchmark(8, 10000, 10000);
    }
    std::cout << "success" << std::endl;
    return 0;
}
