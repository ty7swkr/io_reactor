/*
 * WsHandler.h
 *
 *  Created on: 2023. 10. 8.
 *      Author: tys
 */

#ifndef WSHANDLER_H_
#define WSHANDLER_H_

#include <http1_reactor/http1_reactor.h>

using namespace https_reactor;

class WsHandler : public Http1Handler
{
public:
  WsHandler(const sockaddr_storage &client_addr) : Http1Handler(client_addr) {}
  virtual ~WsHandler() { reactor_trace; }

protected: // method
  void  handle_registered ();

  // http 1.1  reqeust & sent
  void  handle_request    (const Http1Request  &request   ) override;
  void  handle_sent       (const Http1Response &response  ) override;
  void  handle_sent_error (const int           &err_no,
                           const std::string   &err_str,
                           const Http1Response &response  ) override {}

  // websocket request & sent
  void  handle_request    (const std::deque<WebSocket> &requests) override;
  void  handle_sent       (const WebSocket     &response  ) override;
  void  handle_sent_error (const int           &err_no,
                           const std::string   &err_str,
                           const int32_t       &id,
                           const WebSocket     &response  ) override
  { (void)err_no; (void)err_str; (void)id; (void)response; }

  // common
  void  handle_removed    () override;
  void  handle_timeout    (const int64_t       &timer_key ) override;
  void  handle_error      (const int           &err_no,
                           const std::string   &err_str   ) override
  { (void)err_no; (void)err_str; }

  void  handle_close      () override;
  void  handle_shutdown   () override {};
};


#endif /* SERVER_CLIENTHANDLER_H_ */
