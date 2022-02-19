#include "threadpool.h"
#include <exception>

template<typename T>
ThreadPool<T>::ThreadPool(ConnectionPool* pool, int thread_number, int max_requests)
    : sql_pool_(pool),
      thread_number_(thread_number),
      stop_(false),
      threads_(NULL) {

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
  //TODO
}