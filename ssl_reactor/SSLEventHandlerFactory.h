/*
 * SSLEventHandlerFactory.h
 *
 *  Created on: 2022. 9. 15.
 *      Author: tys
 */

#ifndef IO_REACTOR_SSL_REACTOR_SSLEVENTHANDLERFACTORY_H_
#define IO_REACTOR_SSL_REACTOR_SSLEVENTHANDLERFACTORY_H_

#include <ssl_reactor/SSLEventHandler.h>
#include <reactor/acceptor/EventHandlerFactory.h>
#include <ssl_reactor/SSLSessionHandlerFactory.h>

namespace reactor
{

class SSLEventHandlerFactory : public EventHandlerFactory
{
public:
  SSLEventHandlerFactory(SSLSessionHandlerFactory &handler_factory,
                         Acceptor                 &acceptor,
                         Reactors                 &reactors)
  : factory_(handler_factory), acceptor_(acceptor), reactors_(reactors) {}
  virtual ~SSLEventHandlerFactory() {}

  EventHandler *create(const io_handle_t      &client_io_handle,
                       const sockaddr_storage &client_addr) override
  {
    SSLSessionHandler *session = factory_.create(client_io_handle, client_addr);
    if (session == nullptr)
    {
      ::close(client_io_handle);
      return nullptr;
    }

    try
    {
      return SSLEventHandler::create(session, client_addr, acceptor_, reactors_);
    }
    catch (std::runtime_error &e)
    {
      std::cerr << e.what() << std::endl;
      ::close(client_io_handle);
    }

    return nullptr;
  }

protected:
  SSLSessionHandlerFactory &factory_;
  Acceptor  &acceptor_;
  Reactors  &reactors_;
};

}

#endif /* SSL_REACTOR_SSLEVENTHANDLERFACTORY_H_ */
