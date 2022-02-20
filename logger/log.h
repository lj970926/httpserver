class Log {
public:
  Log* GetInstance();
  void init(const char* file_name, int log_buf_size, int max_lines, int max_queue_size = 0);
private:
  Log();
  ~Log();
  char* log_buf_;
  bool is_async_mode = false;
};