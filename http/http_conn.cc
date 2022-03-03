#include "http_conn.h"
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/mman.h>
#include <string>

#include <logger/log.h>

using namespace std;

const char* html_root = "./public";

int HTTPConnection::user_count_ = 0;

std::unordered_map<std::string, std::string> user;
Mutex user_lock;

int SetNonBlocking(int fd) {
  int old_flags = fcntl(fd, F_GETFL);
  fcntl(fd, F_SETFL, old_flags | O_NONBLOCK);
  return old_flags;
}

void Addfd(int epollfd, int fd, bool one_shot = true) {
  epoll_event event;
  event.data.fd = fd;
  event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;

  if (one_shot)
    event.events |= EPOLLONESHOT;

  epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
}

void Modfd(int epollfd, int fd, int ev) {
  epoll_event event;
  event.data.fd = fd;
  event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
  epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

void Removefd(int epollfd, int fd) {
  epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, NULL);
}

void HTTPConnection::__Init() {
  read_idx_ = 0;
  checked_idx_ = 0;
  start_line_ = 0;
  write_idx_ = 0;
  check_state_ = CHECK_STATE_REQUESTLINE;
  linger_ = false;
  method_ = GET;
  cgi_ = false;
  version_ = NULL;
  url_ = NULL;
  content_length_ = 0;
  host_ = NULL;
  request_content_.clear();
  sql_conn_ = NULL;
  file_address_ = NULL;
  iovcnt_ = 0;
  bytes_to_send_ = 0;

  memset(read_buf_, 0, READ_BUFFER_SIZE);
  memset(write_buf_, 0, WRITE_BUFFER_SIZE);

}

void HTTPConnection::Init(int epollfd, int sockfd, struct sockaddr_in& addr)
{
  epollfd_ = epollfd;
  sockfd_ = sockfd;
  addr_ = addr;
  __Init();
}

bool HTTPConnection::ReadOnce() {
  if (read_idx_ >= READ_BUFFER_SIZE) {
    return false;
  }

  while (true) {
    int bytes_read = recv(sockfd_, read_buf_ + read_idx_, READ_BUFFER_SIZE - read_idx_, 0);

    if (bytes_read == -1) {
      if (errno == EAGAIN || errno == EWOULDBLOCK)
        continue;
      return false;
    }

    if (bytes_read == 0) {
      return true;
    }

    read_idx_ += bytes_read;
    return true;
  }
}

void HTTPConnection::Process() {
  HTTPCode ret = __ProcessRead();

  if (ret == NO_REQUEST) {
    Modfd(epollfd_, sockfd_, EPOLLIN);
    return;
  }

  bool write_ret = __ProcessWrite(ret);
  if (!write_ret)
    CloseConnection();

  Modfd(epollfd_, sockfd_, EPOLLOUT);
}

HTTPConnection::HTTPCode HTTPConnection::__ProcessRead() {
  LineStatus line_status;
  while ((check_state_ == CHECK_STATE_CONTENT && line_status == LINE_OK) || (line_status = __ParseLine()) == LINE_OK) {
    char* text = line();
    start_line_ = checked_idx_;

    switch (check_state_) {
    case CHECK_STATE_REQUESTLINE: {
      if (__ParseRequestLine(text) == BAD_REQUEST)
        return BAD_REQUEST;
      break;
    }
    case CHECK_STATE_HEADER: {
      HTTPCode ret = __ParseHeader(text);
      if (ret == BAD_REQUEST)
        return BAD_REQUEST;

      if (ret == GET_REQUEST)   //No content
        return Response();

      line_status = LINE_OPEN;
      break;
    }

    case CHECK_STATE_CONTENT: {
      if (__ParseContent(text) == GET_REQUEST)
        return Response();
      break;
    }
    default:
      return INTERNAL_ERROR;
    }
  }
  return NO_REQUEST;
}

HTTPConnection::LineStatus HTTPConnection::__ParseLine() {
  for (; checked_idx_ < read_idx_; ++checked_idx_) {
    char temp = read_buf_[checked_idx_];
    if (temp == '\r') {
      if (checked_idx_ + 1 >= read_idx_)
        return LINE_OPEN;
      
      if (read_buf_[checked_idx_ + 1] == '\n') {
        read_buf_[checked_idx_++] = '\0';
        read_buf_[checked_idx_++] = '\0';
        return LINE_OK;
      }

      return LINE_BAD;
    }

    if (temp == '\n') {
      if (checked_idx_ - 1 > 1 && read_buf_[checked_idx_ - 1] == '\r') {
        read_buf_[checked_idx_ - 1] = '\0';
        read_buf_[checked_idx_++] = '\0';

        return LINE_OK;
      }

      return LINE_BAD;
    }
  }

  return LINE_OPEN;
}

HTTPConnection::HTTPCode HTTPConnection::__ParseRequestLine(char* text) {
  char* method = strtok(text, "\t");
  if (strcasecmp(method, "GET") == 0)
    method_ = GET;
  else if (strcasecmp(method, "POST") == 0) {
    method_ = POST;
    cgi_ = true;
  }
  else
    return BAD_REQUEST;

  version_ = strtok(NULL, "\t");
  if (!version_ || !strcasecmp(version_, "HTTP/1.1"))
    return BAD_REQUEST;

  url_ = strtok(NULL, "\t");

  if (strncasecmp(url_, "http://", 7) == 0) {
    url_ += 7;
    url_ = strchr(url_, '/');
  }

  if (strncasecmp(url_, "https://", 8) == 0) {
    url_ += 8;
    url_ = strchr(url_, '/');
  }

  if (!url_ || url_[0] != '/')
    return BAD_REQUEST;

  if (strlen(url_) == 1)
    strcat(url_, "judge.html");

  check_state_ = CHECK_STATE_HEADER;
  return NO_REQUEST;
}

HTTPConnection::HTTPCode HTTPConnection::__ParseHeader(char* text) {
  if (text[0] == '\0') {
    //find empty line
    if (content_length_ == 0)
      return GET_REQUEST;
    else {
      check_state_ = CHECK_STATE_CONTENT;
      return NO_REQUEST;
    }
  }

  if (strncasecmp(text, "Connection:", 11) == 0) {
    text += 11;
    text += strspn(text, "\t");
    if (strcasecmp(text, "keep-alive") == 0)
      linger_ = true;
  }
  else if (strncasecmp(text, "Content-length:", 15) == 0) {
    text += 15;
    text += strspn(text, "\t");

    content_length_ = atol(text);
  }
  else if (strncasecmp(text, "Host:", 5) == 0) {
    text += 5;
    text += strspn(text, "\t");
    host_ = text;
  }
  else {
    exit(1);
  }

  return NO_REQUEST;
}

HTTPConnection::HTTPCode HTTPConnection::Response() {
  char flag = url_[1];
  string file_path = html_root;
  if (cgi_ && (flag == '2' || flag == '3')) {
    string name, passwd;
    int delim = request_content_.find('&');
    name = request_content_.substr(5, delim - 5);
    passwd = request_content_.substr(delim + 1);
    if (flag == '3') {
      if (user.find(name) == user.end()) {
        string sql_insert = "INSERT INTO user(username, passwd) VALUES('";
        sql_insert += name;
        sql_insert += "', '";
        sql_insert += passwd;
        sql_insert += "')";

        user_lock.Lock();
        int res = mysql_query(sql_conn_, sql_insert.c_str());
        user[name] = passwd;
        user_lock.Unlock();
        strcpy(url_, res == 0 ? "log.html" : "registerError.html");
      } else {
        strcpy(url_, "registerError.html");
      }
    } else {
      if (user.find(name) != user.end() && user[name] == passwd)
        strcpy(url_, "welcome.html");
      else
        strcpy(url_, "logError.html");
    }
  }

    switch (flag) {
    case '0':
      file_path += "/register.html";
      break;
    case '1':
      file_path += "/log.html";
      break;
    case '5':
      file_path += "/picture.html";
      break;
    case '6':
      file_path += "/video.html";
      break;
    case '7':
      file_path += "/fans.html";
      break;
    default:
      file_path += url_;
    }

    if (stat(file_path.c_str(), &file_stat_) < 0)
      return NO_RESOURCE;
    if (!(file_stat_.st_mode & S_IROTH))
      return FORBIDDEN_REQUEST;
    if (!S_ISREG(file_stat_.st_mode))
      return BAD_REQUEST;
    int fd = open(file_path.c_str(), O_RDONLY);
    file_address_ = (char*)mmap(0, file_stat_.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    return FILE_REQUEST;
}

HTTPConnection::HTTPCode HTTPConnection::__ParseContent(char* text) {
  if (read_idx_ >= checked_idx_ + content_length_) {
    text[content_length_] = '\0';
    request_content_ = text;
    return GET_REQUEST;
  }

  return NO_REQUEST;
}

bool HTTPConnection::__ProcessWrite(HTTPCode ret) {
  // response desp
  const char* error_500_title = "Internal Error";
  const char* error_500_form = "Internal Server Error.\n";
  const char* error_404_title = "Not Found";
  const char* error_404_form = "The requested file was not found in the server.\n";
  const char* error_400_title = "Bad Request";
  const char* error_400_form = "Your request has bad syntax.\n";
  const char* error_403_title = "Forbiden";
  const char* error_403_form = "Permission denied from the server.\n";
  const char* ok_200_title = "OK";

  switch (ret) {
    case INTERNAL_ERROR:
      __AddStatusLine(500, error_500_title);
      __AddHeaders(strlen(error_500_form));
      if (!__AddContent(error_500_form))
        return false;
      break;
    case NO_RESOURCE:
      __AddStatusLine(404, error_404_title);
      __AddHeaders(strlen(error_404_form));
      if (!__AddContent(error_404_form))
        return false;
      break;
    case BAD_REQUEST:
      __AddStatusLine(400, error_400_title);
      __AddHeaders(strlen(error_400_form));
      if (!__AddContent(error_400_form))
        return false;
      break;
    case FORBIDDEN_REQUEST:
      __AddStatusLine(403, error_403_title);
      __AddHeaders(strlen(error_403_form));
      if (!__AddContent(error_403_form))
        return false;
      break;
    case FILE_REQUEST:
      __AddStatusLine(200, ok_200_title);
      if (file_stat_.st_size) {
        __AddHeaders(file_stat_.st_size);
        iov_[1].iov_base = file_address_;
        iov_[1].iov_len = file_stat_.st_size;
        ++iovcnt_;
        bytes_to_send_ += file_stat_.st_size;
      } else {
        const char* ok_form = "<html><body></body></html>";
        __AddHeaders(strlen(ok_form));
        if (!__AddContent(ok_form))
          return false;
      }
      break;
    default:
      return false;
  }
  iov_[0].iov_base = write_buf_;
  iov_[0].iov_len = write_idx_;
  ++iovcnt_;
  bytes_to_send_ += write_idx_;
  return true;
}

bool HTTPConnection::__AddStatusLine(int status, const char* title) {
  return __AddResponse("%s %d %s\r\n", "HTTP/1.1", status, title);
}

bool HTTPConnection::__AddHeaders(const int content_length) {
  if (!__AddContentLength(content_length))
    return false;
  if (!__AddLinger())
    return false;
  return __AddBlankLine();
}

bool HTTPConnection::__AddContent(const char* content) {
  return __AddResponse(content);
}

bool HTTPConnection::__AddResponse(const char* format, ...) {
  if (write_idx_ >= WRITE_BUFFER_SIZE)
    return false;
  va_list valist;
  va_start(valist, format);
  int len = snprintf(write_buf_ + write_idx_, WRITE_BUFFER_SIZE - write_idx_ - 1, format, valist);
  va_end(valist);
  if (write_idx_ + len >= WRITE_BUFFER_SIZE - 1)
    return false;
  write_idx_ += len;
  LOG_INFO("request: %s", write_buf_);
  return true;
}

bool HTTPConnection::__AddLinger() {
  return __AddResponse("Connection:%s\r\n", (linger_ ? "keep-alive" : "close"));
}

bool HTTPConnection::__AddContentLength(const int content_length) {
  return __AddResponse("Content-Length:%d\r\n", content_length);
}

bool HTTPConnection::__AddBlankLine() {
  return __AddResponse("\r\n");
}

bool HTTPConnection::__AddContentType() {
  return __AddResponse("Content-Type:text/html\r\n");
}

void HTTPConnection::InitUserInfo(ConnectionPool* conn_pool) {
  MYSQL* conn;
  ConnectionRAII con_raii(&conn, conn_pool);
  if (mysql_query(conn, "SELECT username, passwd from user")) {
    LOG_ERROR("SELECT error: %s", mysql_error(conn));
  }

  MYSQL_RES* result = mysql_store_result(conn);

  while (MYSQL_ROW row = mysql_fetch_row(result)) {
    string name = row[0];
    string passwd = row[1];
    user[name] = passwd;
  }
}

void HTTPConnection::CloseConnection() {
  Removefd(epollfd_, sockfd_);
  close(sockfd_);
  --user_count_;
}

void HTTPConnection::set_mysql_conn(MYSQL *conn) {
  sql_conn_ = conn;
}

bool HTTPConnection::Write() {

  if (bytes_to_send_ == 0) {
    Modfd(epollfd_, sockfd_, EPOLLIN);
    __Init();
    return true;
  }

  while (true) {
    int write_cnt = writev(sockfd_, iov_, iovcnt_);
    if (write_cnt < 0) {
      if (errno == EAGAIN) {
        Modfd(epollfd_, sockfd_, EPOLLOUT);
        return true;
      }
      return false;
    }
    bytes_to_send_ -= write_cnt;

    if (write_cnt >= iov_[0].iov_len) {
      iov_[1].iov_base = (char*)iov_[1].iov_base + (write_cnt - iov_[0].iov_len);
      iov_[1].iov_len = bytes_to_send_;
      iov_[0].iov_len = 0;
    } else {
      iov_[0].iov_base  = (char*)iov_[0].iov_base + write_cnt;
      iov_[0].iov_len -=write_cnt;
    }

    if (bytes_to_send_ == 0) {
      Modfd(epollfd_, sockfd_, EPOLLIN);
      if (linger_) {
        __Init();
        return true;
      } else {
        return false;
      }
    }
  }
}

