#include "thread_pool.h"
#include <iostream>
#include <atomic>
#include <cassert>

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
int main()
{
    std::atomic<int> counter{0};
    {
        ThreadPool pool(2, 2);
        std::thread p1([&pool, &counter]()
                      {
            for(int i=0;i<5;i++){
                pool.submit([&counter](){
                    std::this_thread::sleep_for(std::chrono::milliseconds(200));
                    counter.fetch_add(1);
                });
            } });
        std::thread p2([&]()
                      {
            for(int i=0;i<5;i++){
                pool.submit([&](){
                    std::this_thread::sleep_for(std::chrono::milliseconds(200));
                    counter.fetch_add(1);
                });
            } });
        p1.join();
        p2.join();
    }
    assert(counter == 10);
    std::cout << "success" << std::endl;
    return 0;
}
