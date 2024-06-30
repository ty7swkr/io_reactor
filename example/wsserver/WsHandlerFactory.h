/*
 * WsHandlerFactory.h
 *
 *  Created on: 2023. 10. 8.
 *      Author: tys
 */

#ifndef WSHANDLERFACTORY_H_
#define WSHANDLERFACTORY_H_

#include "WsHandler.h"
#include <memory>

using namespace reactor;

class WsHandlerFactory : public TCPSessionHandlerFactory
{
public:
  WsHandlerFactory() {}
  virtual ~WsHandlerFactory() {}

  TCPSessionHandler *
  create(const io_handle_t      &client_io_handle,
         const sockaddr_storage &client_addr) override
  {
    try
    {
      return new WsHandler(client_addr);
    }
    catch (std::runtime_error &e)
    {
      std::cerr << e.what() << std::endl;
      ::close(client_io_handle);
    }

    return nullptr;
  }
};



#endif /* SERVER_CLIENTHANDLERFACTORY_H_ */
