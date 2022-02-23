#pragma once

#include <string>

#include <lock/locker.h>
#include "block_queue.h"

using std::string;

class Log {
public:
  static Log* GetInstance();
  void Init(const char* file_name, int log_buf_size, int max_lines, int max_queue_size = 0);
  static void* ThreadWriteFile(void* arg);
  void Flush();
  void Write(int level, const char* format, ...);
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

#define LOG_DEBUG(format, ...) {Log::GetInstance()->Write(0, format, ##__VA_ARGS__); Log::GetInstance()->Flush();}
#define LOG_INFO(format, ...) {Log::GetInstance()->Write(1, format, ##__VA_ARGS__); Log::GetInstance()->Flush();}
#define LOG_WARN(format, ...) {Log::GetInstance()->Write(2, format, ##__VA_ARGS__); Log::GetInstance()->Flush();}
#define LOG_ERROR(format, ...) {Log::GetInstance()->Write(3, format, ##__VA_ARGS__); Log::GetInstance()->Flush();}