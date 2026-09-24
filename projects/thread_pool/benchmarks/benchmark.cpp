#include "thread_pool.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <future>
#include <iostream>
#include <vector>

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
    ThreadPool pool(
        worker_count,
        static_cast<std::size_t>(task_count));

    std::vector<std::uint64_t> results(
        task_count);

    std::atomic<int> completed{0};

    std::promise<void> done;
    auto done_future = done.get_future();

    auto start =
        std::chrono::steady_clock::now();

    for (int i = 0;
         i < task_count;
         ++i)
    {
        pool.submit(
            [i,
             &results,
             &completed,
             &done,
             task_count,
             loops_per_task]
            {
                std::uint64_t x =
                    static_cast<std::uint64_t>(
                        i + 1);

                for (int j = 0;
                     j < loops_per_task;
                     ++j)
                {
                    x =
                        x * 1664525ULL +
                        1013904223ULL;
                }

                results[i] = x;

                if (completed.fetch_add(1) + 1 ==
                    task_count)
                {
                    done.set_value();
                }
            });
    }

    done_future.wait();

    auto end =
        std::chrono::steady_clock::now();

    std::uint64_t checksum = 0;

    for (auto x : results)
    {
        checksum += x;
    }

    double elapsed =
        std::chrono::duration<double>(
            end - start)
            .count();

    double throughput =
        static_cast<double>(task_count) /
        elapsed;

    return {
        elapsed,
        throughput,
        checksum};
}

void print_result(
    int worker_count,
    const BenchmarkResult& result)
{
    std::cout
        << worker_count
        << " workers"
        << " | elapsed = "
        << result.elapsed
        << " s"
        << " | throughput = "
        << result.throughput
        << " tasks/s"
        << " | checksum = "
        << result.checksum
        << '\n';
}

void run_case(
    int task_count,
    int loops_per_task,
    int rounds)
{
    std::cout
        << "\n===== "
        << task_count
        << " tasks x "
        << loops_per_task
        << " loops =====\n";

    for (int round = 1;
         round <= rounds;
         ++round)
    {
        std::cout
            << "round "
            << round
            << '\n';

        auto result4 =
            run_benchmark(
                4,
                task_count,
                loops_per_task);

        print_result(
            4,
            result4);

        auto result8 =
            run_benchmark(
                8,
                task_count,
                loops_per_task);

        print_result(
            8,
            result8);
    }
}

int main()
{
    constexpr int rounds = 3;

    run_case(
        100,
        1'000'000,
        rounds);

    run_case(
        1000,
        100'000,
        rounds);

    run_case(
        10000,
        10'000,
        rounds);

    return 0;
}