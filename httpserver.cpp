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
#define TIME_SLOT 5

using namespace std;

extern int SetNonBlocking(int fd);
extern void Addfd(int epollfd, int fd, bool one_shot = true);
extern void Modfd(int epollfd, int fd, int ev);
void Removefd(int epollfd, int fd);

//global variables
int pipefd[2];
int epollfd;
TimerList timer_list;

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

void SendErrorMsg(int connfd, const char* info) {
  printf("%s\n", info);
  write(connfd, info, strlen(info));
  close(connfd);
}

void Callback(ClientData* clnt_data) {
  if (clnt_data) {

    Removefd(epollfd, clnt_data->sockfd);
    close(clnt_data->sockfd);
    LOG_INFO("close fd %d", clnt_data->sockfd);
  }
}

void TimeoutHandler() {
  timer_list.Tick();
  alarm(TIME_SLOT);
}

int main(int argc,  char* argv[])
{
  if (argc != 3) {
    printf("Usage: %s <ip> <port>\n", basename(argv[0]));
    return -1;
  }
  //init
  Log::GetInstance()->Init("Servlog", 2000, 80000, 8);
  ConnectionPool *conn_pool = ConnectionPool::GetInstance();
  conn_pool->Init("localhost", "http","lj970926", "httpdb", 3306, 8);
  ThreadPool<HTTPConnection> thread_pool(conn_pool);
  auto* http_conns = new HTTPConnection[MAX_FD_NUMS];
  http_conns->InitUserInfo(conn_pool);
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

  if (listen(serv_sock, 5) < 0) {
    printf("Listen error\n");
    return -1;
  }

  //create pipe
  if (socketpair(PF_UNIX, SOCK_STREAM, 0, pipefd) < 0) {
    printf("Fail to create pipe\n");
    return -1;
  }

  //crate epoll and add events
  epoll_event events[MAX_EVENT_NUMS];
  epollfd = epoll_create(1);

  Addfd(epollfd, serv_sock, false);

  SetNonBlocking(pipefd[1]);
  Addfd(epollfd, pipefd[0], false);

  //add sig handler
  AddSigHandler(SIGALRM, SigHandler, false);
  AddSigHandler(SIGTERM, SigHandler, false);

  //create client data and timer
  auto client_data = new ClientData[MAX_FD_NUMS];
  bool time_out = false;
  bool stop_server = false;

  //main loop

  while (!stop_server) {
    int event_number = epoll_wait(epollfd, events, MAX_EVENT_NUMS, -1);
    if (event_number < 0 && errno != EINTR) {
      LOG_ERROR("epoll failure");
      break;
    }

    for (int i = 0; i < event_number; ++i) {
      int sockfd = events[i].data.fd;
      //new request
      if (sockfd == serv_sock) {
        struct sockaddr_in clnt_addr;
        socklen_t clnt_addr_size = sizeof(clnt_addr);
        int connfd = accept(serv_sock, (struct sockaddr*)&clnt_addr, &clnt_addr_size);
        if (connfd < 0) {
          LOG_ERROR("accept error, errno: %d", errno);
          continue;
        }

        if (HTTPConnection::user_count() >= MAX_FD_NUMS) {
          LOG_ERROR("Internal server busy");
          SendErrorMsg(connfd, "Internal server busy");
        }

        http_conns[connfd].Init(epollfd, connfd, clnt_addr);
        client_data[connfd].sockfd = connfd;
        client_data[connfd].client_addr = clnt_addr;
        auto timer = new Timer(&client_data[connfd], time(NULL) + 3 * TIME_SLOT, Callback);
        client_data[connfd].client_timer = timer;
        timer_list.AddTimer(timer);
      } else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
        Timer* timer = client_data[i].client_timer;
        timer->Callback();

        if (timer) {
          timer_list.DeleteTimer(timer);
        }
      } else if (sockfd == pipefd[0] && (events[i].events & EPOLLIN)) {
        //signal handler
        char signals[1024];
        int ret = read(pipefd[0], signals, sizeof(signals));
        if (ret <= 0)
          continue;
        for (int i = 0; i < ret; ++i) {
          switch (signals[i]) {
            case SIGALRM:
              time_out = true;
              break;
            case SIGTERM:
              stop_server = true;
          }
        }
      } else if (events[i].events & EPOLLIN) {
        Timer* timer = client_data[sockfd].client_timer;
        if (http_conns[sockfd].ReadOnce()) {
          LOG_INFO("Deal with client(%s)", inet_ntoa(client_data[sockfd].client_addr.sin_addr));
          thread_pool.Append(&http_conns[sockfd]);

          if (timer) {
            timer->Reset(time(NULL) + 3 * TIME_SLOT);
            LOG_INFO("Adjust timer once");
            timer_list.AdjustTimer(timer);
          }
        } else {
          if (timer) {
            timer->Callback();
            timer_list.DeleteTimer(timer);
          }
        }
      } else if (events[i].events & EPOLLOUT) {
        Timer* timer = client_data[sockfd].client_timer;
        if (http_conns[sockfd].Write()) {
          LOG_INFO("send data to client(%s)", inet_ntoa(client_data[sockfd].client_addr.sin_addr));

          if (timer) {
            LOG_INFO("Adjust timer once");
            timer->Reset(time(NULL) + 3 * TIME_SLOT);
            timer_list.AddTimer(timer);
          }
        } else {
          if (timer) {
            timer->Callback();
            timer_list.DeleteTimer(timer);
          }
        }
      }
    }
    if (time_out) {
      TimeoutHandler();
      time_out = false;
    }
  }
  close(epollfd);
  close(serv_sock);
  close (pipefd[1]);
  close(pipefd[0]);
  delete[] http_conns;
  delete[] client_data;
  return 0;
}
