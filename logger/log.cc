#include "log.h"
#include <string.h>


Log* Log::GetInstance() {
  static Log log;
  return &log;
}

void Log::init(const char* file_name, int log_buf_size, int max_lines, int max_queue_size) {
  log_buf_ = new char[log_buf_size];
  
  if (max_queue_size > 0) {
    is_async_mode = true;
    
  }
}