/*
 * EventHandlerFactory.h
 *
 *  Created on: 2021. 7. 25.
 *      Author: tys
 */

#ifndef IO_REACTOR_TCP_REACTOR_TCPEVENTHANDLERFACTORY_H_
#define IO_REACTOR_TCP_REACTOR_TCPEVENTHANDLERFACTORY_H_

#include <tcp_reactor/TCPEventHandler.h>
#include <tcp_reactor/TCPSessionHandlerFactory.h>
#include <reactor/acceptor/EventHandlerFactory.h>

namespace reactor
{

class TCPEventHandlerFactory : public EventHandlerFactory
{
public:
  TCPEventHandlerFactory(TCPSessionHandlerFactory &factory,
                         Acceptor                 &acceptor,
                         Reactors                 &reactors)
  : factory_(factory), acceptor_(acceptor), reactors_(reactors) {}
  virtual ~TCPEventHandlerFactory() {}

  EventHandler *create(const io_handle_t      &client_io_handle,
                       const sockaddr_storage &client_addr) override
  {
    TCPSessionHandler *session = factory_.create(client_io_handle, client_addr);
    if (session == nullptr)
    {
      ::close(client_io_handle);
      return nullptr;
    }

    try
    {
      return TCPEventHandler::create(session, client_addr, acceptor_, reactors_);
    }
    catch (std::runtime_error &e)
    {
      std::cerr << e.what() << std::endl;
      ::close(client_io_handle);
    }

    return nullptr;
  }

protected:
  TCPSessionHandlerFactory &factory_;
  Acceptor  &acceptor_;
  Reactors  &reactors_;
};

}


#endif /* IO_REACTOR_TCP_REACTOR_TCPEVENTHANDLERFACTORY_H_ */
