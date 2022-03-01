//
// Created by lijin on 2022/2/27.
//

#include "timer.h"

Timer::Timer(ClientData *clnt_data, time_t expire, void (*callback)(ClientData *)) {

}

void Timer::Reset(time_t expire) {
  expire_ = expire;
}