#pragma once

#include <stdlib.h>
#include "lock/locker.h"
template<typename T>
class BlockQueue {
public:
  BlockQueue(int max_size = 1000)
      : max_size_(max_size),
        cur_size_(0),
        front_(0),
        back_(-1) {
    if (max_size <= 0)
      exit(-1);
    array_ = new T[max_size];
  }

  ~BlockQueue() {
    mutex_.Lock();
    if (array_ != NULL)
      delete [] array_;

    mutex_.Unlock();
  }

  void Clear() {
    mutex_.Lock();
    cur_size_ = 0;
    front_ = 0;
    back_ = -1;
    mutex_.Unlock();
  }

  bool Push(const T& value) {
    mutex_.Lock();
    if (cur_size_ >= max_size_) {
      cond_.Broadcast();
      mutex_.Unlock();
      return false;
    }

    back_ = (back_ + 1) % max_size_;
    array_[back_] = value;
    ++cur_size_;
    cond_.Broadcast();
    mutex_.Unlock();
    return true;
  }

  bool Pop(T& value) {
    mutex_.Lock();
    while (cur_size_ <= 0) {
      if (!cond_.Wait(mutex_.Get())) {
        mutex_.Unlock();
        return false;
      }
    }

    value = array_[front_];
    front_ = (front_ + 1) % max_size_;
    --cur_size_;
    mutex_.Unlock();
    return true;
  }

  bool Full() {
    bool ans;
    mutex_.Lock();
    ans = (cur_size_ >= max_size_);
    mutex_.Unlock();
    return ans;
  }

  bool Empty() {
    bool ans;
    mutex_.Lock();
    ans = (cur_size_ <= 0);
    mutex_.Unlock();
    return  ans;
  }

private:
  int max_size_;
  int cur_size_;
  T* array_;
  int front_;
  int back_;
  Mutex mutex_;
  Cond cond_;
};