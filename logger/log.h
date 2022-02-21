#pragma once

#include <string>

#include "block_queue.h"

using std::string;

class Log {
public:
  Log* GetInstance();
  void Init(const char* file_name, int log_buf_size, int max_lines, int max_queue_size = 0);
private:
  Log();
  ~Log();
  char* log_buf_;
  bool is_async_mode_ = false;
  BlockQueue<string>* log_queue_;
  int max_lines_;
};