# CMakeList.txt : CMake project for httpserver, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

project ("httpserver")

include_directories(.)

# Add source to this project's executable.
add_executable (httpserver "httpserver.cpp" "httpserver.h" "lock/locker.h" "threadpool/threadpool.h" "CGImysql/sql_connection_pool.h" "threadpool/threadpool.cc" "http/http_conn.h" "http/http_conn.cc")

# TODO: Add tests and install targets if needed.
