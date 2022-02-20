#include <stdlib.h>
#include "lock/locker.h"
template<typename T>
class BlockQueue {
public:
  BlockQueue(int max_size = 1000)
      : max_size_(max_size) {
    if (max_size <= 0)
      exit(-1);
    array_ = new T[max_size];
  }

  bool push(const T& value) {
    mutex_.Lock();
    if (cur_size_ >= max_size_) {
      
    }
  }
private:
  int max_size_;
  int cur_size_;
  T* array_;
  int front_;
  int back_;
  Mutex mutex_;
};