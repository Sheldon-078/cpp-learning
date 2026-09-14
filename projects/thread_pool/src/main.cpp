#include "thread_pool.h"
#include <iostream>
#include <atomic>
#include <cassert>

int add(int a,int b){
    return a+b;
}
double adddouble(double a,double b){
    return a*b;
}
std::string str(std::string temp){
    return temp;
}
int adderror(int a){
    throw std::runtime_error("error");
}
int main()
{
    ThreadPool pool(1);

    auto f4=pool.submit(adderror, 5);
    try{
        std::cout << f4.get() << '\n';
    }
    catch(const std::exception &e){
        std::cout << e.what() << '\n';
    }
    auto f1=pool.submit(add, 10, 20);
    std::cout << f1.get() << '\n';
    auto f2=pool.submit(adddouble, 5.684, 4.269);
    std::cout << f2.get() << '\n';
    auto f3=pool.submit(str, "hello,ustc");
    std::cout << f3.get() << '\n';
    
    return 0;
}
