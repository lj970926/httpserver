#pragma once

#include <list>
#include <exception>
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

template<typename T>
ThreadPool<T>::ThreadPool(ConnectionPool* pool, int thread_number, int max_requests)
  : sql_pool_(pool),
  thread_number_(thread_number),
  stop_(false),
  threads_(NULL),
  max_requests_(max_requests) {

  if (thread_number_ <= 0 || max_requests_ <= 0)
    throw std::exception();

  threads_ = new pthread_t[thread_number_];

  if (!threads_)
    throw std::exception();

  for (int i = 0; i < thread_number_; ++i) {
    if (pthread_create(threads_ + i, NULL, Worker, this) != 0) {
      delete[] threads_;
      throw std::exception();
    }

    if (pthread_detach(threads_[i]) != 0) {
      delete[] threads_;
      throw std::exception();
    }
  }
}

template<typename T>
ThreadPool<T>::~ThreadPool() {
  delete[] threads_;
  stop_ = true;
}

template<typename T>
bool ThreadPool<T>::Append(T* request) {
  queue_locker_.Lock();

  if (work_queue_.size() > max_requests_) {
    queue_locker_.Unlock();
    return false;
  }

  work_queue_.push_back(request);
  queue_locker_.Unlock();
  queue_stat_.Post();
  return true;
}

template<typename T>
void* ThreadPool<T>::Worker(void* arg) {
  ThreadPool* pool = (ThreadPool*)arg;
  pool->Run();
  return pool;
}

template<typename T>
void ThreadPool<T>::Run() {
  while (!stop_) {
    queue_stat_.Wait();
    queue_locker_.Lock();
    T* request = work_queue_.front();
    queue_locker_.Unlock();
    MYSQL* conn;
    ConnectionRAII mysql_conn(&conn, sql_pool_);
    request->set_mysql_conn(conn);
    request->Process();
  }
}