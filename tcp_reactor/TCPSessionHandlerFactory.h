/*
 * SessionFactory.h
 *
 *  Created on: 2022. 9. 15.
 *      Author: tys
 */

#ifndef IO_REACTOR_TCP_REACTOR_TCPSESSIONHANDLERFACTORY_H_
#define IO_REACTOR_TCP_REACTOR_TCPSESSIONHANDLERFACTORY_H_

#include <reactor/DefinedType.h>
#include <sys/socket.h>

namespace reactor
{

class TCPSessionHandler;

class TCPSessionHandlerFactory
{
public:
  virtual ~TCPSessionHandlerFactory() {}
  virtual TCPSessionHandler *create(const io_handle_t      &client_io_handle,
                                    const sockaddr_storage &client_addr) = 0;
};

}

#endif /* IO_REACTOR_TCP_REACTOR_TCPSESSIONHANDLERFACTORY_H_ */
