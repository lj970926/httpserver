# CMakeList.txt : CMake project for httpserver, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

project ("httpserver")

include_directories(.)

include(FindPkgConfig)
pkg_check_modules(LIBMYSQLCLIENT REQUIRED mysqlclient)

foreach(FLAG ${LIBMYSQLCLIENT_CFLAGS_OTHER})
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${FLAG}")
endforeach()

link_directories(${LIBMYSQLCLIENT_LIBRARY_DIRS})

#find pthread and mysqlclient
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# Add source to this project's executable.
add_executable (httpserver "httpserver.cpp"
        "lock/locker.h"
        "threadpool/threadpool.h" "mysqlconn/sql_connection_pool.h"
        "http/http_conn.h" "http/http_conn.cc"
        "logger/block_queue.h" "logger/log.h"
        "logger/log.cc" "mysqlconn/sql_connection_pool.cc"
        "timer/timer.cc")

#link pthread and mysqlclient
target_link_libraries(httpserver Threads::Threads)
target_include_directories(httpserver PRIVATE ${LIBMYSQLCLIENT_INCLUDE_DIRS})
target_link_libraries(httpserver ${LIBMYSQLCLIENT_LIBRARIES})

# TODO: Add tests and install targets if needed.
