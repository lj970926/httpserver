//
// Created by lijin on 2022/2/27.
//

#ifndef HTTPSERVER_TIMER_H
#define HTTPSERVER_TIMER_H

#include <arpa/inet.h>

class Timer;
struct ClientData {
  int sockfd;
  struct sockaddr_in client_addr;
  Timer* client_timer;
};

class Timer {
public:
  void tick();

private:
  ClientData* client_data;
};

#endif //HTTPSERVER_TIMER_H
