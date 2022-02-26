#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <string.h>
#include <string>
#include <unordered_map>
#include <mysql/mysql.h>

#include <lock/locker.h>

class HTTPConnection {
public:

  static const int FILENAME_LEN = 200;
  static const int READ_BUFFER_SIZE = 2048;
  static const int WRITE_BUFFER_SIZE = 1024;

  enum Method {
    GET = 0,
    POST,
    HEAD,
    PUT,
    DELETE,
    TRACE,
    OPTIONS,
    CONNECT,
    PATH
  };

  enum CheckState {
    CHECK_STATE_REQUESTLINE = 0,
    CHECK_STATE_HEADER,
    CHECK_STATE_CONTENT
  };

  enum LineStatus {
    LINE_OK = 0,
    LINE_BAD,
    LINE_OPEN
  };

  enum HTTPCode {
    NO_REQUEST = 0,
    GET_REQUEST,
    BAD_REQUEST,
    INTERNAL_ERROR,
    NO_RESOURCE,
    FORBIDDEN_REQUEST,
    FILE_REQUEST
  };

  HTTPConnection() {}
  ~HTTPConnection() {}

  void Init(int sockfd, struct sockaddr_in& addr);
  void Process();
  bool ReadOnce();
  void CloseConnection();
  bool Write();

  int epollfd_;

private:
  void __Init();
  HTTPCode __ParseRequestLine(char* text);
  HTTPCode __ParseHeader(char* text);
  HTTPCode __ParseContent(char* text);
  HTTPCode __ProcessRead();
  bool __ProcessWrite(HTTPCode ret);
  char* line() { return read_buf_ + start_line_; }
  LineStatus __ParseLine();
  HTTPCode Response();

  int sockfd_;
  struct sockaddr_in addr_;

  char read_buf_[READ_BUFFER_SIZE];
  char write_buf_[WRITE_BUFFER_SIZE];
  int read_idx_ = 0;
  int checked_idx_ = 0;
  int start_line_ = 0;

  CheckState check_state_;
  Method method_;
  bool cgi_ = false;
  char* version_;
  char* url_;

  int content_length_;
  bool linger_;
  char* host_;

  std::string request_content_;
  std::unordered_map<std::string, std::string> user_;
  Mutex user_lock_;
  MYSQL* sql_conn_;
  struct stat file_stat_;
  char* file_address_;
};