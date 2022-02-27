// httpserver.cpp : Defines the entry point for the application.
//

#include <csignal>
#include <cstring>
#include <http/http_conn.h>

using namespace std;

extern int SetNonBlocking(int fd);
extern void Addfd(int epollfd, int fd, bool one_shot = true);
extern void Modfd(int epollfd, int fd, int ev);
void Removefd(int epollfd, int fd);

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

int main()
{

  return 0;
}
