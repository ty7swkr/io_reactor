/*
 * SSLSessionFactory.h
 *
 *  Created on: 2022. 9. 15.
 *      Author: tys
 */

#ifndef IO_REACTOR_SSL_REACTOR_SSLSESSIONHANDLERFACTORY_H_
#define IO_REACTOR_SSL_REACTOR_SSLSESSIONHANDLERFACTORY_H_

#include <reactor/DefinedType.h>
#include <sys/socket.h>

namespace reactor
{

class SSLSessionHandler;

class SSLSessionHandlerFactory
{
public:
  virtual ~SSLSessionHandlerFactory() {}
  virtual SSLSessionHandler *create(const io_handle_t      &client_io_handle,
                                    const sockaddr_storage &client_addr) = 0;
};

}

#endif /* SSL_REACTOR_SSLSESSIONFACTORY_H_ */
