/*
 * Https1Handler.h
 *
 *  Created on: 2023. 2. 3.
 *      Author: tys
 */

#ifndef HTTPS_REACTOR_HTTPS1HANDLER_H_
#define HTTPS_REACTOR_HTTPS1HANDLER_H_

#include <http1_protocol/Http1Request.h>
#include <http1_protocol/Http1Response.h>
#include <ssl_reactor/ssl_reactor.h>

namespace https_reactor
{

using namespace reactor;

class Https1Handler : public SSLSessionHandler
{
public:
  Https1Handler() : websocket_(false) {}
  virtual ~Https1Handler() {}

  bool            send              (const Http1Response  &response);
  int32_t         send              (const WebSocket      &response);
  bool            is_websocket      () const { return websocket_.load(); }

protected:
  virtual void    handle_registered () {};
  virtual void    handle_accept     (const std::string    &alpn)       { (void)alpn; }
  virtual void    handle_timeout    (const int64_t        &timer_key)  { (void)timer_key; }
  virtual void    handle_error      (const SSL_STATE      &ssl_state,
                                     const int            &err_no,
                                     const std::string    &err_str)    { (void)ssl_state; (void)err_no; (void)err_str; }

  // http 1.1  reqeust & sent
  virtual void    handle_request    (const Http1Request   &request )  = 0;
  virtual void    handle_sent       (const Http1Response  &response)  = 0;
  virtual void    handle_sent_error (const SSL_STATE      &ssl_state,
                                     const int            &err_no,
                                     const std::string    &err_str,
                                     const Http1Response  &response)  = 0;

  // websocket request & sent
  virtual void    handle_request    (const std::deque<WebSocket> &requests) { (void)requests; }
  virtual void    handle_sent       (const int32_t        &stream_id,
                                     const WebSocket      &response)  { (void)stream_id; (void)response; }
  virtual void    handle_sent_error (const SSL_STATE      &ssl_state,
                                     const int            &err_no,
                                     const std::string    &err_str,
                                     const int32_t        &stream_id,
                                     const WebSocket      &response)  { (void)ssl_state; (void)err_no; (void)err_str; (void)stream_id; (void)response; }

  virtual void    handle_close      () {};
  virtual void    handle_removed    () {};
  virtual void    handle_shutdown   () {};

protected:
  int32_t         next_stream_id    ();
  const Acceptor  &acceptor         () const { return this->acceptor(); }

private:
  void            handle_input      (const uint8_t  *data, const size_t &size) override;
  void            handle_input_ws   (const uint8_t  *data, const size_t &size);

  void            handle_sent       (const int32_t  &stream_id,
                                     const uint8_t  *data, const size_t &size) override;

  bool            handle_sent_http1 (const int32_t  &stream_id,
                                     const uint8_t  *data, const size_t &size);

  void            handle_sent_error (const SSL_STATE      &ssl_state,
                                     const int            &err_no,
                                     const std::string    &err_str,
                                     const int32_t        &stream_id,
                                     const uint8_t *data, const size_t  &size);

private:
  std::vector<char>     buffer_http1_;
  std::vector<uint8_t>  buffer_websocket_;

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

#endif /* REACTOR_HTTPS_REACTOR_Https1Handler_H_ */














