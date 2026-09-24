#include "thread_pool.h"

#include <iostream>
#include <string>

int add(int a, int b)
{
    return a + b;
}

int main()
{
    ThreadPool pool(4, 16);

    auto future1 = pool.submit(
        []
        {
            return 42;
        });

    auto future2 = pool.submit(
        add,
        10,
        20);

    auto future3 = pool.submit(
        [](const std::string& name)
        {
            return std::string("hello, ") + name;
        },
        std::string("thread pool"));

    std::cout << "future1 = "
              << future1.get()
              << '\n';

    std::cout << "future2 = "
              << future2.get()
              << '\n';

    std::cout << "future3 = "
              << future3.get()
              << '\n';

    return 0;
}