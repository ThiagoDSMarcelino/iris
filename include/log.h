#pragma once

#include <iostream>

#define PRINT(msg) do { std::cout << msg << "\n"; } while (0)
#define PRINT_ERR(msg) do { std::cerr << msg << "\n"; } while (0)

#ifndef NDEBUG
    #define LOG(msg)     do { std::cout << msg << "\n"; } while (0)
    #define LOG_ERR(msg) do { std::cerr << msg << "\n"; } while (0)
#else
    #define LOG(msg)     do {} while (0)
    #define LOG_ERR(msg) do {} while (0)
#endif
