#include <iostream>
#include "calculator.h"


int main()
{

    int a;
    int b;


    std::cout << "Please enter a: ";
    std::cin >> a;


    std::cout << "Please enter b: ";
    std::cin >> b;



    std::cout << "sum = "
              << add(a,b)
              << std::endl;


    std::cout << "difference = "
              << subtract(a,b)
              << std::endl;


    std::cout << "product = "
              << multiply(a,b)
              << std::endl;



    return 0;
}