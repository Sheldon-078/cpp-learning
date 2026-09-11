#include "thread_pool.h"
#include <iostream>
#include <atomic>
#include <cassert>

int main()
{
    const int N = 1000;

    std::vector<std::atomic<int>> counts(N);

    for (auto &count : counts)
    {
        count.store(0);
    }
    {
        ThreadPool pool(4);
        for (int i = 0; i < 1000; i++)
        {
            pool.submit([i,&counts]()
                        { counts[i] += 1; });
        }
    }
    for(int i=0;i<1000;i++){
        assert(counts[i] == 1);
    }
    std::cout << "test passed\n";
    return 0;
}
