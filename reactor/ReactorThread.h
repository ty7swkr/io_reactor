/*
 * ReactorThread.h
 *
 *  Created on: 2020. 2. 11.
 *      Author: tys
 */

#ifndef IO_REACTOR_REACTOR_REACTORTHREAD_H_
#define IO_REACTOR_REACTOR_REACTORTHREAD_H_

#include <reactor/Reactor.h>
#include <thread>
#include <mutex>
#include <condition_variable>

namespace reactor
{

class ReactorThread
{
public:
  Reactor reactor;

  ~ReactorThread()
  {
    if (thread_ != nullptr)
      delete thread_;
  }

  void start(int32_t max_clients = 1024,
             size_t  max_events  = 10240,
             ReactorHandlerFactory *factory = nullptr)
  {
    reactor.init(max_clients, max_events, factory);

    thread_ = new std::thread{&ReactorThread::run, this};
    std::unique_lock<std::mutex> lock(condition_lock_);
    if (is_run_ == true)
      return;

    condition_.wait(lock);
  }

  void stop()
  {
    reactor.stop();
    if (thread_ == nullptr)
      return;

    thread_->join();
    delete thread_;
    thread_ = nullptr;
  }

  bool is_run() const
  {
    return is_run_;
  }

protected:
  void run()
  {
    {
      std::unique_lock<std::mutex> lock(condition_lock_);
      is_run_ = true;
      condition_.notify_all();
    }

    reactor.run();
    is_run_ = false;
  }

private:
  std::thread *thread_ = nullptr;

private:
  std::condition_variable condition_;
  std::mutex condition_lock_;
  bool is_run_ = false;
};

}

#endif /* reactor_ReactorThread_h */


