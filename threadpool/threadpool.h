#pragma once

#include <list>
#include <pthread.h>
#include "lock/locker.h"
#include "mysqlconn//sql_connection_pool.h"

template<typename T>
class ThreadPool {
public:
  ThreadPool(ConnectionPool* pool, int thread_number = 8, int max_request = 10000);
  ~ThreadPool();
  bool Append(T* request);
private:
  static void* Worker(void* arg);
  void Run();

  int thread_number_;
  int max_requests_;
  pthread_t* threads_;
  std::list<T*> work_queue_;
  Mutex queue_locker_;
  Sem queue_stat_;
  bool stop_;
  ConnectionPool* sql_pool_;
};