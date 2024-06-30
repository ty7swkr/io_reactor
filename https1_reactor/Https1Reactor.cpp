/*
 * enhanced.cpp
 *
 *  Created on: 2023. 10. 8.
 *      Author: tys
 */


#include "Https1Reactor.h"

using namespace reactor;

bool
Https1Reactor::set_reactor(const size_t             &thread_num,
                           const std::string        &cert_file,
                           const std::string        &priv_file,
                           const std::string        &chain_file,
                           const size_t             &max_clients_per_reactor,
                           const size_t             &max_events_per_reactor,
                           ReactorHandlerFactory    *factory)
{
  reactor_thread_num_       = thread_num;
  reactor_max_clients_      = max_clients_per_reactor;
  reactor_max_events_       = max_events_per_reactor;
  reactor_handler_factory_  = factory;

  ssl_cert_file_            = cert_file;
  ssl_priv_file_            = priv_file;
  ssl_chain_file_           = chain_file;
  ssl_alpn_                 = SSLAcceptorThread::alpn_http11;

  return true;
}

void
Https1Reactor::stop()
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
Https1Reactor::wait()
{
  std::unique_lock<std::mutex> lock(stop_cond_lock_);
  if (stop_ == false) stop_cond_.wait(lock);
}

bool
Https1Reactor::start()
{
  stop_ = false;

  if (session_factory_ == nullptr)
  {
    reactor_trace << "SSLSessionHandlerFactory is nullptr";
    return false;
  }

  if (reactors.init(reactor_thread_num_, reactor_max_clients_,
                    reactor_max_events_, reactor_handler_factory_) == false)
  {
    reactor_trace << "Reactors Initialize Failed";
    return false;
  }

  auto ssl_contexts_opt_ =
      SSLAcceptorThread::prepare_ssl([&](const int &err_no, const std::string &err_str)
                                     {
                                       reactor_trace << err_no << err_str;
                                     },
                                     reactors,
                                     ssl_cert_file_,
                                     ssl_priv_file_,
                                     ssl_chain_file_,
                                     ssl_alpn_);
  if (ssl_contexts_opt_.has_value() == false)
  {
    reactor_trace << "Make SSL Context Failed";
    return false;
  }

  ssl_contexts_ = ssl_contexts_opt_.value();

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

  reactors.start();

  for (size_t thread_num = 0; thread_num < acceptor_thread_num_; ++thread_num)
  {
    acceptor_threads.push_back(std::make_shared<SSLAcceptorThread>(*session_factory_, ssl_contexts_, acceptor, reactors));
    acceptor_threads.back()->start();
  }

  scope_exit.ignore = true;
  return true;
}
