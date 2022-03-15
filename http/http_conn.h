#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <string.h>
#include <string>
#include <unordered_map>
#include <mysql/mysql.h>

#include <lock/locker.h>
#include <mysqlconn/sql_connection_pool.h>

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

  void Init(int epollfd, int sockfd, struct sockaddr_in& addr);
  void Process();
  bool ReadOnce();
  void CloseConnection();
  bool Write();
  void InitUserInfo(ConnectionPool* conn_pool);
  static int user_count();
  void set_mysql_conn(MYSQL* conn);

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
  bool __AddResponse(const char* format, ...);
  bool __AddStatusLine(int status, const char* title);
  bool __AddHeaders(const int content_length);
  bool __AddContent(const char* content);
  bool __AddLinger();
  bool __AddContentLength(const int content_length);
  bool __AddBlankLine();
  bool __AddContentType();
  void __Unmap();

  int epollfd_;
  int sockfd_;
  struct sockaddr_in addr_;
  static int user_count_;

  char read_buf_[READ_BUFFER_SIZE];
  char write_buf_[WRITE_BUFFER_SIZE];
  int read_idx_ = 0;
  int checked_idx_ = 0;
  int start_line_ = 0;

  int write_idx_ = 0;

  CheckState check_state_;
  Method method_;
  bool cgi_ = false;
  char* version_ = NULL;
  char* url_ = NULL;

  int content_length_ = 0;
  bool linger_ = false;
  char* host_ = NULL;

  std::string request_content_;
  MYSQL* sql_conn_;
  struct stat file_stat_;
  char* file_address_;
  struct iovec iov_[2];
  int iovcnt_;
  int bytes_to_send_;

};