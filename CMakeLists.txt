# CMakeList.txt : CMake project for httpserver, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

project ("httpserver")

include_directories(.)

#find pthread and mysqlclient
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# Add source to this project's executable.
add_executable (httpserver "httpserver.cpp"
        "lock/locker.h"
        "threadpool/threadpool.h" "mysqlconn/sql_connection_pool.h"
        "threadpool/threadpool.cc" "http/http_conn.h" "http/http_conn.cc"
        "logger/block_queue.h" "logger/log.h"
        "logger/log.cc" "mysqlconn/sql_connection_pool.cc"
        "timer/timer.cc")

#link pthread and mysqlclient
target_link_libraries(httpserver Threads::Threads)

# TODO: Add tests and install targets if needed.
