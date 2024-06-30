/*
 * enhanced.cpp
 *
 *  Created on: 2023. 10. 8.
 *      Author: tys
 */


#include "TCPReactor.h"

using namespace reactor;

void
TCPReactor::stop()
{
  {
    std::unique_lock<std::mutex> lock(stop_cond_lock_);
    if (stop_ == true)
      return;
  }

  for (const auto &accept : acceptor_threads)
    accept->stop();

  reactors.stop();

  std::unique_lock<std::mutex> lock(stop_cond_lock_);
  stop_     = true;
  stop_cond_ .notify_all();
}

void
TCPReactor::wait()
{
  std::unique_lock<std::mutex> lock(stop_cond_lock_);
  if (stop_ == false) stop_cond_.wait(lock);
}

bool
TCPReactor::start()
{
  stop_ = false;

  if (reactors.init(reactor_thread_num_, reactor_max_clients_,
                    reactor_max_events_, reactor_handler_factory_) == false)
    return false;

  reactors.start();

  struct scope_exit_t
  {
    scope_exit_t(std::function<void()> exit_func) : exit_func(exit_func) {}
    ~scope_exit_t() { if (ignore == true) return; exit_func(); }

    bool ignore = false;
    std::function<void()> exit_func;
  };

  scope_exit_t scope_exit([&](){ reactors.stop(); });

  switch (acceptor_type_)
  {
    case IPV4 : if (acceptor.listen_ipv4  (acceptor_port_, acceptor_address_, acceptor_backlog_) == false) return false; else break;
    case IPV6 : if (acceptor.listen_ipv6  (acceptor_port_, acceptor_address_, acceptor_backlog_) == false) return false; else break;
    case IPV46: if (acceptor.listen_ipv46 (acceptor_port_, acceptor_address_, acceptor_backlog_) == false) return false; else break;
    default: return false;
  }

  event_factory_ =
      std::make_shared<TCPEventHandlerFactory>(*session_factory_,
                                               acceptor,
                                               reactors);

  for (size_t thread_num = 0; thread_num < acceptor_thread_num_; ++thread_num)
  {
    acceptor_threads.push_back(
        std::make_shared<AcceptorThread>(acceptor,
                                         reactors,
                                         *(event_factory_.get())));
    acceptor_threads.back()->start();
  }

  scope_exit.ignore = true;
  return true;
}
