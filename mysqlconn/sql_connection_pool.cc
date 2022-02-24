//
// Created by lijin on 2022/2/23.
//

#include "sql_connection_pool.h"

#include <logger/log.h>

ConnectionPool* ConnectionPool::GetInstance() {
  static ConnectionPool conn;
  return &conn;
}

void ConnectionPool::Init(const string &host, const string &user, const string &passwd, const string &db_name, int port,
                          int max_conn) {
  host_ = host;
  user_ = user;
  passwd_ = passwd;
  db_name_ = db_name;

  pool_.clear();

  for (int i = 0; i < max_conn; ++i) {
    MYSQL* con = NULL;
    con = mysql_init(NULL);

    if (con == NULL) {
      LOG_ERROR("Mysql Init error");
      exit(1);
    }

    con = mysql_real_connect(con, host_.c_str(), user_.c_str(), passwd_.c_str(),
                             db_name_.c_str(), port_, NULL, 0);

    if (con == NULL) {
      LOG_ERROR("Mysql connect error");
      exit(1);
    }

    pool_.push_back(con);
    num_max_con_ = num_free_con_ = max_conn;
    sem_ = Sem(num_free_con_);
  }
}

MYSQL* ConnectionPool::GetConnection() {
  MYSQL* conn = NULL;

  if (pool_.empty())
    return NULL;

  sem_.Wait();
  mutex_.Lock();
  conn = pool_.front();
  pool_.pop_front();
  --num_free_con_;
  mutex_.Unlock();
  return conn;

}

bool ConnectionPool::ReleaseConnection(MYSQL *conn) {
  if (!conn)
    return false;
  mutex_.Lock();
  pool_.push_back(conn);
  ++num_free_con_;
  mutex_.Unlock();
  sem_.Post();
  return true;
}

int ConnectionPool::NumFreeConnection() {
  return num_free_con_;
}

void ConnectionPool::Destroy() {
  mutex_.Lock();
  for (MYSQL* conn : pool_)
    mysql_close(conn);

  num_free_con_ = 0;
  pool_.clear();

  mutex_.Unlock();
}

ConnectionPool::~ConnectionPool() {
  Destroy();
}