#pragma once

#include <string>

#include <lock/locker.h>
#include "block_queue.h"

using std::string;

class Log {
public:
  static Log* GetInstance();
  void Init(const char* file_name, int log_buf_size, int max_lines, int max_queue_size = 0);
  static void* thread_write_file(void* arg);
  void flush();
  void write(int level, const char* format, ...);
private:
  void async_write_log();
  Log();
  ~Log();

  char* log_buf_;
  bool is_async_mode_ = false;
  BlockQueue<string>* log_queue_;
  int max_lines_;
  int cur_lines_;
  FILE* fp_;

  char dir_name_[128];
  char log_name_[128];
  Mutex mutex_;

  int today_;
};