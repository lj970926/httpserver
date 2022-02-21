#include "log.h"
#include <string.h>


Log* Log::GetInstance() {
  static Log log;
  return &log;
}

void Log::Init(const char* file_name, int log_buf_size, int max_lines, int max_queue_size) {
  log_buf_ = new char[log_buf_size];
  
  if (max_queue_size > 0) {
    is_async_mode_ = true;
    log_queue_ = new BlockQueue<string>(max_queue_size);
  }

  log_buf_ = new char[log_buf_size];
  memset(log_buf_, '\0', sizeof(char) * log_buf_size);
  max_lines_ = max_lines;
}