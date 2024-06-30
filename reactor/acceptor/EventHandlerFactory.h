/*
 * EventHandlerFactory.h
 *
 *  Created on: 2021. 7. 25.
 *      Author: tys
 */

#ifndef IO_REACTOR_REACTOR_EVENTHANDLERFACTORY_H_
#define IO_REACTOR_REACTOR_EVENTHANDLERFACTORY_H_

#include <reactor/acceptor/Acceptor.h>
#include <reactor/Reactors.h>
#include <sys/socket.h>

namespace reactor
{

class EventHandler;

class EventHandlerFactory
{
public:
  virtual ~EventHandlerFactory() {}
  virtual EventHandler *create(const io_handle_t      &client_io_handle,
                               const sockaddr_storage &client_addr) = 0;
};

}


#endif /* OPEN_REACTOR_EVENTHANDLERFACTORY_H_ */
