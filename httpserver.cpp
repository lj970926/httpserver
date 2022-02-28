// httpserver.cpp : Defines the entry point for the application.
//

#include <csignal>
#include <cstring>
#include <cerrno>
#include <cstdio>
#include <unistd.h>
#include <libgen.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <http/http_conn.h>
#include <timer/timer.h>
#include <logger/log.h>
#include <threadpool/threadpool.h>
#include <mysqlconn/sql_connection_pool.h>

#define MAX_FD_NUMS  65536
#define MAX_EVENT_NUMS 10000

using namespace std;

extern int SetNonBlocking(int fd);
extern void Addfd(int epollfd, int fd, bool one_shot = true);
extern void Modfd(int epollfd, int fd, int ev);
void Removefd(int epollfd, int fd);

//global variables
int pipefd[2];

void AddSigHandler(int sig, void (*handler)(int), bool restart = true) {
  struct sigaction act;
  memset(&act, 0, sizeof(act));
  act.sa_handler = handler;
  sigfillset(&act.sa_mask);
  if (restart)
    act.sa_flags |= SA_RESTART;
  if (sigaction(sig, &act, NULL) != 0)
    exit(1);
}

void SigHandler(int sig) {
  int old_errno = errno;
  write(pipefd[1], &sig, 1);
  errno = old_errno;
}

int main(int argc,  char* argv[])
{
  if (argc != 3) {
    printf("Usage: %s <ip> <port>\n", argv[0]);
    return -1;
  }
  //init
  Log::GetInstance()->Init("Servlog", 2000, 80000, 8);
  ConnectionPool *conn_pool = ConnectionPool::GetInstance();
  conn_pool->Init("localhost", "http","lj970926", "httpdb", 3306, 8);
  ThreadPool<HTTPConnection> thread_pool(conn_pool);
  auto* http_conns = new HTTPConnection[MAX_FD_NUMS];

  //create socket
  int serv_sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (serv_sock < 0) {
    printf("Fail to create serv sock\n");
    return -1;
  }

  struct sockaddr_in serv_addr;
  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  serv_addr.sin_port = htons(atoi(argv[2]));

  int flag = 1;
  setsockopt(serv_sock, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));

  if (bind(serv_sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
    printf("Bind error\n");
    return -1;
  }

  //create pipe
  if (socketpair(PF_UNIX, SOCK_STREAM, 0, pipefd) < 0) {
    printf("Fail to create pipe\n");
    return -1;
  }

  //crate epoll and add events
  epoll_event events[MAX_EVENT_NUMS];
  int epollfd = epoll_create(1);

  Addfd(epollfd, serv_sock, false);

  SetNonBlocking(pipefd[1]);
  Addfd(epollfd, pipefd[0], false);

  //add sig handler
  AddSigHandler(SIGALRM, SigHandler, false);
  AddSigHandler(SIGTERM, SigHandler, false);

  return 0;
}
