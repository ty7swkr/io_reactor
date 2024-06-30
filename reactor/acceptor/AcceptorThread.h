/*
 * AcceptorThread.h
 *
 *  Created on: 2020. 2. 11.
 *      Author: tys
 */

#ifndef IO_REACTOR_REACTOR_ACCEPTOR_ACCEPTTHREAD_H_
#define IO_REACTOR_REACTOR_ACCEPTOR_ACCEPTTHREAD_H_

#include <reactor/acceptor/AcceptorThreadHandlerFactory.h>
#include <reactor/acceptor/Acceptor.h>
#include <reactor/acceptor/EventHandlerFactory.h>

#include <reactor/ReactorHandlerFactory.h>
#include <reactor/Reactors.h>
#include <reactor/ObjectsTimer.h>

#include <cstdio>
#include <thread>
#include <unordered_map>
#include <mutex>
#include <condition_variable>
#include <functional>

namespace reactor
{

class AcceptorThread
{
public:
  AcceptorThread(Acceptor &acceptor,
                 Reactors &reactors,
                 EventHandlerFactory &event_handler_factory,
                 AcceptorThreadHandlerFactory *acceptor_thread_handler_factory = nullptr);

  virtual ~AcceptorThread();

  void start();
  void stop ();

  const Acceptor &get_acceptor() const { return acceptor_; }

protected:
  virtual bool on_created_handler(EventHandler  *,
                                  Reactor       &,
                                  Reactors      &,
                                  const size_t  &reactor_index) { (void)reactor_index; return true; }

protected:
  void    run();

private:
  AcceptorThreadHandler        *acceptor_thread_handler_         = nullptr;
  AcceptorThreadHandlerFactory *acceptor_thread_handler_factory_ = nullptr;

private:
  Acceptor &acceptor_;
  Reactors &reactors_;
  EventHandlerFactory &handler_factory_;

private:
  std::thread *thread_  = nullptr;

private:
  std::condition_variable  run_cond_;
  std::mutex               run_cond_lock_;
  bool is_run_ = false;
};

inline
AcceptorThread::AcceptorThread(Acceptor &acceptor,
                               Reactors &reactors,
                               EventHandlerFactory          &event_handler_factory,
                               AcceptorThreadHandlerFactory *acceptor_thread_handler_factory)
: acceptor_thread_handler_factory_(acceptor_thread_handler_factory),
  acceptor_       (acceptor),
  reactors_       (reactors),
  handler_factory_(event_handler_factory)
{
}

inline
AcceptorThread::~AcceptorThread()
{
  this->stop();
}

inline void
AcceptorThread::start()
{
  if (thread_ != nullptr)
    delete thread_;

  thread_ = new std::thread{&AcceptorThread::run, this};

  std::unique_lock<std::mutex> lock(run_cond_lock_);
  if (is_run_ == true)
    return;

  run_cond_.wait(lock);
}

inline void
AcceptorThread::stop()
{
  acceptor_.shutdown(SHUT_RDWR);
  if (thread_ == nullptr)
    return;

  thread_->join();
  delete thread_;
  thread_ = nullptr;
}

}

#endif /* OPEN_REACTOR_TCP_ACCEPTOR_ACCEPTTHREAD_H_ */
