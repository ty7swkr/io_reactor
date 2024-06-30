/*
 * Http1Handler.h
 *
 *  Created on: 2023. 2. 3.
 *      Author: tys
 */

#ifndef HTTP_REACTOR_HTTP1HANDLER_H_
#define HTTP_REACTOR_HTTP1HANDLER_H_

#include <http1_protocol/Http1Request.h>
#include <http1_protocol/Http1Response.h>
#include <tcp_reactor/TCPSessionHandler.h>
#include <reactor/reactor.h>

#include <deque>
#include <atomic>
#include <mutex>

namespace https_reactor
{

using namespace reactor;

class Http1Handler : public TCPSessionHandler
{
public: // interface
  Http1Handler(const sockaddr_storage &client_addr,
               const size_t           &buffer_http1_size      = 10240,
               const size_t           &buffer_websocket_size  = 65535*10)
  : TCPSessionHandler(client_addr), websocket_(false)
  {
    buffer_http1_     .resize(buffer_http1_size);
    buffer_websocket_ .resize(buffer_websocket_size);
  }
  virtual ~Http1Handler() {}

  bool          send              (const Http1Response  &response);
  int32_t       send              (const WebSocket      &response);
  bool          is_websocket      () const { return websocket_.load(); }

protected: // virtual method
  // http 1.1  reqeust & sent
  virtual void  handle_request    (const Http1Request   &request )  = 0;
  virtual void  handle_sent       (const Http1Response  &response)  = 0;
  virtual void  handle_sent_error (const int            &err_no,
                                   const std::string    &err_str,
                                   const Http1Response  &response)  = 0;

  // websocket request & sent
  virtual void  handle_request    (const std::deque<WebSocket> &requests) { (void)requests; }
  virtual void  handle_sent       (const int32_t        &stream_id,
                                   const WebSocket      &response) { (void)stream_id; (void)response; }
  virtual void  handle_sent_error (const int            &err_no,
                                   const std::string    &err_str,
                                   const int32_t        &id,
                                   const WebSocket      &response) { (void)err_no; (void)err_str; (void)id; (void)response; }

  // common
  virtual void  handle_registered () {};
  virtual void  handle_removed    () {};
  virtual void  handle_timeout    (const int64_t        &timer_key)  { (void)timer_key; }
  virtual void  handle_error      (const int            &err_no,
                                   const std::string    &err_str)    { (void)err_no; (void)err_str; }
  virtual void  handle_close      () {};
  virtual void  handle_shutdown   () {};

protected: // interface
  int32_t       next_stream_id    ();

private:
  void          handle_input      () override;
  void          handle_input_ws   ();
  void          handle_sent       (const int32_t        &stream_id,
                                   const uint8_t        *data,
                                   const size_t         &size) override;

  bool          handle_sent_http1 (const int32_t        &stream_id,
                                   const uint8_t        *data,
                                   const size_t         &size);

  void          handle_sent_error (const int            &err_no,
                                   const std::string    &err_str,
                                   const int32_t        &stream_id,
                                   const uint8_t        *data,
                                   const size_t         &size);

private:
  std::vector<char>     buffer_http1_;
  ssize_t               buffer_http1_recvd_ = 0;
  std::vector<uint8_t>  buffer_websocket_;
  ssize_t               buffer_websocket_recvd_ = 0;

private:
  std::mutex  stream_id_lock_;
  int32_t     stream_id_ = -1;

private:
  std::mutex  sent_res_http1_lock_;
  std::map<int32_t, Http1Response> sent_res_http1_;

private:
  std::mutex sent_res_ws_lock_;
  std::map<int32_t, WebSocket> sent_res_ws_;

private:
  std::atomic<bool> websocket_;
};

}

#endif /* REACTOR_HTTPS_REACTOR_HTTP1HANDLER_H_ */
