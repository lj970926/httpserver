#include "log.h"

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <pthread.h>
#include <sys/time.h>



Log* Log::GetInstance() {
  static Log log;
  return &log;
}

void Log::Init(const char* file_name, int log_buf_size, int max_lines, int max_queue_size) {
  log_buf_ = new char[log_buf_size];
  
  if (max_queue_size > 0) {
    is_async_mode_ = true;
    log_queue_ = new BlockQueue<string>(max_queue_size);
    pthread_t tid;
    pthread_create(&tid, NULL, thread_write_file, NULL);
  }

  log_buf_ = new char[log_buf_size];
  memset(log_buf_, '\0', sizeof(char) * log_buf_size);
  max_lines_ = max_lines;
  char log_full_name[256] = {0};
  time_t calendar_time = time(NULL);
  struct tm* brok_time = localtime(&calendar_time);
  const char* pos_slash = strrchr(file_name, '/');

  if (!pos_slash) {
    snprintf(log_full_name, 255, "%d_%02d_%02d_%s",
             brok_time->tm_year + 1900,
             brok_time->tm_mon + 1,
             brok_time->tm_mday,
             file_name);
  } else {
    strcpy(log_name_, pos_slash + 1);
    strncpy(dir_name_, file_name, pos_slash - file_name + 1);
    snprintf(log_full_name, 255, "%s%d_%02d_%02d_%s",
             dir_name_,
             brok_time->tm_year + 1900,
             brok_time->tm_mon + 1,
             brok_time->tm_mday,
             log_name_);
  }
  fp_ = fopen(log_full_name, "a");
  today_ = brok_time->tm_mday;
  return; fp_ != NULL;
}

void* Log::thread_write_file(void *arg) {
  GetInstance()->async_write_log();
}

void Log::async_write_log() {
  string log;
  while (log_queue_->Pop(log)) {
    mutex_.Lock();
    fputs(log.c_str(), fp_);
    mutex_.Unlock();
  }
}

void Log::flush() {
  mutex_.Lock();
  fflush(fp_);
  mutex_.Unlock();
}

void Log::write(int level, const char* format, ...) {
  time_t now = time(NULL);
  struct tm* brok_tm = localtime(&now);
  struct timeval time_val = {0, 0};
  gettimeofday(&time_val, NULL);

  mutex_.Lock();

  //create new log files
  if (today_ != brok_tm->tm_mday || cur_lines_ >= max_lines_) {

    fflush(fp_);
    fclose(fp_);
    char prefix[16] = {0};
    char new_log[256] = {0};
    sprintf(prefix, "%d_%02d_%02d", brok_tm->tm_year + 1900, brok_tm->tm_mon + 1, brok_tm->tm_mday);

    if (today_ != brok_tm->tm_mday) {
      snprintf(new_log, 255, "%s%s%s", dir_name_, prefix, log_name_);
      today_ = brok_tm->tm_mday;
      cur_lines_ = 0;
    } else {
      snprintf(new_log, 255, "%s%s%s.%d", dir_name_, prefix, log_name_, cur_lines_ / max_lines_);
    }

    fp_ = fopen(new_log, "a");
  }

  mutex_.Unlock();

  char level_str[16] = {0};
  switch (level) {
    case 0:
      strcpy(level_str, "[debug]:");
      break;
    case 1:
      strcpy(level_str, "[info]:");
      break;
    case 2:
      strcpy(level_str, "[warn]:");
      break;
    case 3:
      strcpy(level_str, "[error]:");
      break;
    default:
      strcpy(level_str, "[info]:");
      break;
  }

  mutex_.Lock();
  int pre_len = sprintf(log_buf_, "%d-%02d-%02d %02d:%02d:%02d.%06ld %s ",
          brok_tm->tm_year + 1900, brok_tm->tm_mon + 1,
          brok_tm->tm_mday, brok_tm->tm_hour,
          brok_tm->tm_sec, time_val.tv_usec, level_str);

  va_list valist;
  va_start(valist, format);
  int cont_len = vsnprintf(log_buf_ + pre_len, 254 - pre_len, format, valist);
  log_buf_[pre_len + cont_len] = '\n';
  log_buf_[pre_len + cont_len + 1] = '\0';
  va_end(valist);

  string log_str = log_buf_;
  ++cur_lines_;
  mutex_.Unlock();

  if (is_async_mode_ && !log_queue_->Full()) {
    log_queue_->Push(log_str);
  } else {
    mutex_.Lock();
    fputs(log_str.c_str(), fp_);
    mutex_.Unlock();
  }
}