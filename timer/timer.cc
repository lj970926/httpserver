//
// Created by lijin on 2022/2/27.
//

#include "timer.h"

Timer::Timer(ClientData *clnt_data, time_t expire, void (*callback)(ClientData *))
    : expire_(expire),
      client_data_(clnt_data),
      callback_(callback),
      prev(NULL),
      next(NULL){}

void Timer::Reset(time_t expire) {
  expire_ = expire;
}

time_t Timer::expire() {
  return expire_;
}

void Timer::Callback() {
  callback_(client_data_);
}

Timer::Timer(Timer *timer) {
  client_data_ = timer->client_data_;
  callback_ = timer->callback_;
  expire_ = timer->expire_;
  prev = NULL;
  next = NULL;
}

TimerList::TimerList()
    : head_(NULL),
      tail_(NULL) {}

void TimerList::AddTimer(Timer *timer) {
  if (!timer)
    return;
  if (!head_) {
    head_ = tail_ = timer;
    return;
  }

  for (auto p = head_; p; p = p->next) {
    if (timer->expire() < p->expire()) {
      if (!head_) {
        timer->next = head_;
        head_->prev = timer;
        head_ = timer;
        return;
      }
      timer->prev = p->prev;
      timer->next = p;
      timer->prev->next = timer;
      p->prev = timer;
      return;
    }
  }

  timer->prev = tail_;
  tail_->next = timer;
  tail_ = timer;
}

void TimerList::DeleteTimer(Timer *timer) {
  if (!timer)
    return;

  if (timer == head_) {
    head_ = timer->next;
    if (head_)
      head_->prev = NULL;
  }

  if (timer == tail_) {
    tail_ =timer->prev;
    if (tail_)
      tail_->next = NULL;
    delete timer;
    return;
  }
  if (timer != head_ && timer != tail_) {
    timer->prev->next = timer->next;
    timer->next->prev = timer->prev;
  }
  delete timer;
}

void TimerList::AdjustTimer(Timer *timer) {
  if (!timer)
    return;
  auto new_timer = new Timer(timer);
  DeleteTimer(timer);
  AddTimer(new_timer);
}

