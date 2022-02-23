#pragma once

#include <list>
#include <string>
#include <mysql/mysql.h>

using std::list;
using std::string;

class ConnectionPool {
public:
  ConnectionPool* GetInstance();
  void init(const string& host, const string& user, const string& passwd, int max_conn);
private:
  list<MYSQL *> pool_;
  string host_;
  string user_;
  string passwd_;
};