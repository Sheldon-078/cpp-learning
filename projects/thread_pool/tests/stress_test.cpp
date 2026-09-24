#include "thread_pool.h"

#include <atomic>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>

bool test_future_and_exception()
{
    ThreadPool pool(1, 16);

    auto normal_future = pool.submit(
        []
        {
            return 42;
        });

    if (normal_future.get() != 42)
    {
        std::cout
            << "future return value test failed\n";
        return false;
    }

    auto bad_future = pool.submit(
        []
        {
            throw std::runtime_error("boom");
            return 0;
        });

    bool exception_received = false;

    try
    {
        bad_future.get();
    }
    catch (const std::runtime_error&)
    {
        exception_received = true;
    }

    if (!exception_received)
    {
        std::cout
            << "exception propagation test failed\n";
        return false;
    }

    // 验证前一个任务抛异常后，
    // worker 仍然能够继续执行后续任务。
    auto after_exception = pool.submit(
        []
        {
            return 100;
        });

    if (after_exception.get() != 100)
    {
        std::cout
            << "worker survival test failed\n";
        return false;
    }

    return true;
}

bool run_stress_test()
{
    constexpr int worker_count = 4;
    constexpr int producer_count = 4;
    constexpr int tasks_per_producer = 5000;

    constexpr int total_tasks =
        producer_count * tasks_per_producer;

    // 故意设置得远小于 total_tasks，
    // 强迫 producer 经历 bounded-queue backpressure。
    constexpr std::size_t max_queue_size = 128;

    std::vector<std::atomic<int>> counters(
        total_tasks);

    for (auto& counter : counters)
    {
        counter.store(0);
    }

    {
        ThreadPool pool(
            worker_count,
            max_queue_size);

        std::vector<std::thread> producers;
        producers.reserve(producer_count);

        for (int p = 0;
             p < producer_count;
             ++p)
        {
            producers.emplace_back(
                [p, &pool, &counters]
                {
                    for (int j = 0;
                         j < tasks_per_producer;
                         ++j)
                    {
                        int index =
                            p * tasks_per_producer + j;

                        // future 故意不用。
                        // 这里依赖 ThreadPool 析构时
                        // graceful drain 所有已接受任务。
                        (void)pool.submit(
                            [index, &counters]
                            {
                                counters[index]
                                    .fetch_add(1);
                            });
                    }
                });
        }

        for (auto& producer : producers)
        {
            producer.join();
        }

    } // pool destructor:
      // stop → drain queued tasks → join workers

    int missing = 0;
    int duplicated = 0;
    int correct = 0;

    for (int i = 0;
         i < total_tasks;
         ++i)
    {
        int count = counters[i].load();

        if (count == 0)
        {
            ++missing;
        }
        else if (count == 1)
        {
            ++correct;
        }
        else
        {
            ++duplicated;
        }
    }

    bool passed =
        missing == 0 &&
        duplicated == 0 &&
        correct == total_tasks;

    if (!passed)
    {
        std::cout
            << "stress test failed\n"
            << "total      = "
            << total_tasks << '\n'
            << "correct    = "
            << correct << '\n'
            << "missing    = "
            << missing << '\n'
            << "duplicated = "
            << duplicated << '\n';
    }

    return passed;
}

int main()
{
    if (!test_future_and_exception())
    {
        return 1;
    }

    constexpr int rounds = 5;

    for (int round = 1;
         round <= rounds;
         ++round)
    {
        if (!run_stress_test())
        {
            std::cout
                << "FAILED AT ROUND "
                << round
                << '\n';

            return 1;
        }

        std::cout
            << "round "
            << round
            << " passed\n";
    }

    std::cout
        << "\nALL "
        << rounds
        << " ROUNDS PASSED\n";

    return 0;
}