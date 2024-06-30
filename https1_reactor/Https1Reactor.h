/*
 * enhanced.h
 *
 *  Created on: 2023. 10. 8.
 *      Author: tys
 */

#ifndef IO_REACTOR_HTTPS_REACTOR_TCPREACTOR_H_
#define IO_REACTOR_HTTPS_REACTOR_TCPREACTOR_H_

#include <ssl_reactor/SSLEventHandlerFactory.h>
#include <ssl_reactor/SSLAcceptorThread.h>
#include <reactor/Reactors.h>
#include <memory>

namespace reactor
{

using SSLAcceptorThreadPtr = std::shared_ptr<SSLAcceptorThread>;

class Https1Reactor
{
public:
  Https1Reactor() {}

  Reactors reactors;
  Acceptor acceptor;
  std::deque<SSLAcceptorThreadPtr> acceptor_threads;


  void set_acceptor_ipv4  (SSLSessionHandlerFactory *factory,
                           const uint16_t           &port,
                           const std::string        &address    = "0.0.0.0",
                           const size_t             &thread_num = 1,
                           const int                &backlog    = 1000);

  void set_acceptor_ipv6  (SSLSessionHandlerFactory *factory,
                           const uint16_t           &port,
                           const std::string        &address    = "::0",
                           const size_t             &thread_num = 1,
                           const int                &backlog    = 1000);

  void set_acceptor_ipv46 (SSLSessionHandlerFactory *factory,
                           const uint16_t           &port,
                           const std::string        &address    = "::0",
                           const size_t             &thread_num = 1,
                           const int                &backlog    = 1000);

  bool set_reactor        (const size_t             &thread_num,
                           const std::string        &cert_file,
                           const std::string        &priv_file,
                           const std::string        &chain_file = "",
                           const size_t             &max_clients_per_reactor = 1000,
                           const size_t             &max_events_per_reactor  = 100,
                           ReactorHandlerFactory    *factory = nullptr);

  bool start();
  void stop ();
  void wait ();

private:
  void set_acceptor(const int                 &type,
                    SSLSessionHandlerFactory  *factory,
                    const uint16_t            &port,
                    const std::string         &address,
                    const size_t              &thread_num,
                    const int                 &backlog);

private:
  std::string ssl_cert_file_;
  std::string ssl_priv_file_;
  std::string ssl_chain_file_;
  std::string ssl_alpn_;
  SSLContexts ssl_contexts_;

private:
  size_t      reactor_thread_num_   = 1;
  size_t      reactor_max_clients_  = 10000;
  size_t      reactor_max_events_   = 100;
  ReactorHandlerFactory *reactor_handler_factory_ = nullptr;

private:
  enum { IPV4, IPV6, IPV46 };
  int         acceptor_type_        = IPV4;
  size_t      acceptor_thread_num_  = 1;
  std::string acceptor_address_;
  uint16_t    acceptor_port_        = 0;
  int         acceptor_backlog_     = 100;

private:
  SSLSessionHandlerFactory  *session_factory_ = nullptr;

private:
  bool                    stop_ = false;
  std::condition_variable stop_cond_;
  std::mutex              stop_cond_lock_;
};


inline void
Https1Reactor::set_acceptor_ipv4(SSLSessionHandlerFactory  *factory,
                                 const uint16_t            &port,
                                 const std::string         &address,
                                 const size_t              &acceptor_thread_num,
                                 const int                 &backlog)
{
  this->set_acceptor(IPV4, factory, port, address, acceptor_thread_num, backlog);
}


inline void
Https1Reactor::set_acceptor_ipv6(SSLSessionHandlerFactory  *factory,
                                 const uint16_t            &port,
                                 const std::string         &address,
                                 const size_t              &acceptor_thread_num,
                                 const int                 &backlog)
{
  this->set_acceptor(IPV6, factory, port, address, acceptor_thread_num, backlog);
}


inline void
Https1Reactor::set_acceptor_ipv46(SSLSessionHandlerFactory  *factory,
                                  const uint16_t            &port,
                                  const std::string         &address,
                                  const size_t              &acceptor_thread_num,
                                  const int                 &backlog)
{
  this->set_acceptor(IPV46, factory, port, address, acceptor_thread_num, backlog);
}

inline void
Https1Reactor::set_acceptor(const int                 &type,
                            SSLSessionHandlerFactory  *factory,
                            const uint16_t            &port,
                            const std::string         &address,
                            const size_t              &acceptor_thread_num,
                            const int                 &backlog)
{
  acceptor_type_        = type;
  session_factory_      = factory;

  acceptor_thread_num_  = acceptor_thread_num;
  acceptor_address_     = address;
  acceptor_port_        = port;
  acceptor_backlog_     = backlog;
}

}

#endif /* IO_REACTOR_TCP_REACTOR_TCPREACTOR_H_ */
