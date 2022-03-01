//
// Created by lijin on 2022/2/27.
//

#ifndef HTTPSERVER_TIMER_H
#define HTTPSERVER_TIMER_H

#include <arpa/inet.h>
#include <sys/time.h>

class Timer;
struct ClientData {
  int sockfd;
  struct sockaddr_in client_addr;
  Timer* client_timer;
};

class Timer {
public:
  Timer(ClientData* clnt_data, time_t expire, void (*callback)(ClientData*));
  void Tick();
  void Callback(ClientData* clnt_data);

private:
  ClientData* client_data_;
  time_t expire_;
  void (*callback_)(ClientData*);
};

class TimerList {
public:
  void AddTimer(Timer* timer);
  void DeleteTimer(Timer* timer);
};

#endif //HTTPSERVER_TIMER_H
