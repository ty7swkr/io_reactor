/*
 * enhanced.h
 *
 *  Created on: 2023. 10. 8.
 *      Author: tys
 */

#ifndef IO_REACTOR_TCP_REACTOR_TCPREACTOR_H_
#define IO_REACTOR_TCP_REACTOR_TCPREACTOR_H_

#include <tcp_reactor/TCPSessionHandlerFactory.h>
#include <tcp_reactor/TCPSessionHandler.h>
#include <tcp_reactor/TCPEventHandlerFactory.h>
#include <reactor/reactor.h>
#include <memory>

namespace reactor
{

using AcceptorThreadPtr = std::shared_ptr<AcceptorThread>;

class TCPReactor
{
public:
  TCPReactor() {}

  Reactors reactors;
  Acceptor acceptor;
  std::deque<AcceptorThreadPtr> acceptor_threads;

  void set_acceptor_ipv4  (TCPSessionHandlerFactory *factory,
                           const uint16_t           &port,
                           const std::string        &address    = "0.0.0.0",
                           const size_t             &thread_num = 1,
                           const int                &backlog    = 1000);

  void set_acceptor_ipv6  (TCPSessionHandlerFactory *factory,
                           const uint16_t           &port,
                           const std::string        &address    = "::0",
                           const size_t             &thread_num = 1,
                           const int                &backlog    = 1000);

  void set_acceptor_ipv46 (TCPSessionHandlerFactory *factory,
                           const uint16_t           &port,
                           const std::string        &address    = "::0",
                           const size_t             &thread_num = 1,
                           const int                &backlog    = 1000);

  void set_reactor        (const size_t             &thread_num = 1,
                           const size_t             &max_clients_per_reactor = 1000,
                           const size_t             &max_events_per_reactor  = 100,
                           ReactorHandlerFactory    *factory = nullptr);

  bool start  ();
  void stop   ();
  void wait   ();
  bool is_run () const { return !stop_; }

private:
  void set_acceptor(const int                 &type,
                    TCPSessionHandlerFactory  *factory,
                    const uint16_t            &port,
                    const std::string         &address,
                    const size_t              &thread_num,
                    const int                 &backlog);

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
  TCPSessionHandlerFactory *session_factory_ = nullptr;
  std::shared_ptr<TCPEventHandlerFactory> event_factory_;

private:
  bool                    stop_ = false;
  std::condition_variable stop_cond_;
  std::mutex              stop_cond_lock_;
};


inline void
TCPReactor::set_acceptor_ipv4(TCPSessionHandlerFactory  *factory,
                              const uint16_t            &port,
                              const std::string         &address,
                              const size_t              &thread_num,
                              const int                 &backlog)
{
  this->set_acceptor(IPV4, factory, port, address, thread_num, backlog);
}


inline void
TCPReactor::set_acceptor_ipv6(TCPSessionHandlerFactory  *factory,
                              const uint16_t            &port,
                              const std::string         &address,
                              const size_t              &thread_num,
                              const int                 &backlog)
{
  this->set_acceptor(IPV6, factory, port, address, thread_num, backlog);
}


inline void
TCPReactor::set_acceptor_ipv46(TCPSessionHandlerFactory *factory,
                               const uint16_t           &port,
                               const std::string        &address,
                               const size_t             &thread_num,
                               const int                &backlog)
{
  this->set_acceptor(IPV46, factory, port, address, thread_num, backlog);
}

inline void
TCPReactor::set_acceptor(const int                &type,
                         TCPSessionHandlerFactory *factory,
                         const uint16_t           &port,
                         const std::string        &address,
                         const size_t             &thread_num,
                         const int                &backlog)
{
  acceptor_type_        = type;
  session_factory_      = factory;

  acceptor_thread_num_  = thread_num;
  acceptor_address_     = address;
  acceptor_port_        = port;
  acceptor_backlog_     = backlog;
}

inline void
TCPReactor::set_reactor(const size_t           &thread_num,
                        const size_t           &max_clients_per_reactor,
                        const size_t           &max_events_per_reactor,
                        ReactorHandlerFactory  *factory)
{
  reactor_thread_num_       = thread_num;
  reactor_max_clients_      = max_clients_per_reactor;
  reactor_max_events_       = max_events_per_reactor;
  reactor_handler_factory_  = factory;
}

}

#endif /* IO_REACTOR_TCP_REACTOR_TCPREACTOR_H_ */
