cmake_minimum_required(VERSION 3.19)
project(pr2)

include_directories("/usr/local/berkeley_upc/opt/include")
link_directories("/usr/local/berkeley_upc/opt/lib")

set(CMAKE_CXX_STANDARD 17)

add_executable(pr2
        main.cpp)
