#pragma once

#include <list>
#include <string>
#include <mysql/mysql.h>
#include <lock/locker.h>

using std::list;
using std::string;

class ConnectionPool {
public:
  static ConnectionPool* GetInstance();
  MYSQL* GetConnection();
  void
  init(const string &host, const string &user, const string &passwd, const string &db_name, int port, int max_conn);

  bool ReleaseConnection(MYSQL* conn);
  int NumFreeConnection();

private:
  list<MYSQL *> pool_;
  string host_;
  string user_;
  string passwd_;
  int port_;
  string db_name_;

  Sem sem_;
  Mutex mutex_;

  int num_free_con_;
  int num_max_con_;
};