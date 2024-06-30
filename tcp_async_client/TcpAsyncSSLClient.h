/*
 * WoClient.h
 *
 *  Created on: 2021. 8. 4.
 *      Author: tys
 */

#ifndef IO_REACTOR_REACTOR_TCPASYNCSSLCLIENT_H_
#define IO_REACTOR_REACTOR_TCPASYNCSSLCLIENT_H_

#include <tcp_async_client/TcpAsyncEventHandler.h>
#include <ssl_reactor/SSLContext.h>
#include <ssl_reactor/SSLSocket.h>
#include <ssl_reactor/SSLState.h>

#include <string_view>
#include <vector>
#include <functional>

namespace reactor
{

class TcpAsyncSSLClient
{
public:
  TcpAsyncSSLClient(Reactor &reactor, const size_t &recv_buffer_size = 10240);
  virtual ~TcpAsyncSSLClient();

  // disable copy
  TcpAsyncSSLClient(const TcpAsyncSSLClient &) = delete;
  TcpAsyncSSLClient& operator=(const TcpAsyncSSLClient &) = delete;

public:
  virtual bool  connect           (const std::string  &host,
                                   const uint16_t     &port,
                                   const int32_t      &timeout_msec = 5000);
  virtual bool  disconnect        ();
  bool          set_timeout       (const uint32_t &msec);
  bool          send              (const int32_t &id, const uint8_t *data, const size_t &size);
  bool          send              (const int32_t &id, const std::string_view &data);
  bool          send              (const int32_t &id, const std::string &data);
  bool          is_connect        () const;
  bool          close             ();

  uint16_t      local_port        () const { return handler_.local_port(); }
  std::string   local_addr        () const { return handler_.local_addr(); }
  std::string   local_addr_port   () const { return handler_.local_addr_port(); }

protected:
  virtual void  handle_connect        () = 0;
  virtual void  handle_connect_error  (const int &err_no, const std::string &err_str) = 0;
  virtual void  handle_connect_timeout(const int &err_no, const std::string &err_str) = 0;
  virtual void  handle_disconnect     () = 0;
  virtual void  handle_accept         (SSL *ssl) = 0;
  virtual void  handle_recv           (const std::vector<uint8_t> &data) = 0;
  virtual void  handle_sent           (const int32_t &id, const uint8_t *data, const size_t &size) = 0;
  virtual void  handle_sent_error     (const int &err_no, const std::string &err_str,
                                       const int32_t &id, const uint8_t *data, const size_t &size) = 0;
  virtual void  handle_error          (const int &err_no, const std::string &err_str) = 0;
  virtual void  handle_timeout        () = 0;
  virtual void  handle_shutdown       () = 0;

private:
  void  ssl_on_connect    ();
  void  ssl_on_output     ();
  void  ssl_on_input      ();
  void  ssl_on_disconnect ();

private:
  bool  close_ = false;

private:
  SSLContextSPtr  ssl_ctx_;
  SSL       *ssl_     = NULL;
  SSL_STATE ssl_state_=  SSL_STATE::NONE;
  SSLSocket ssl_socket_;
  bool      ssl_accept_done_ = false;

  void process_ssl();
  void ssl_connect();
  void ssl_read   ();
  void ssl_write  ();

private:
  using Bytes = std::vector<uint8_t>;
  Bytes recv_buffer_;

  struct send_buffer_info_t
  {
    send_buffer_info_t() {}
    send_buffer_info_t(const int &stream_id, const size_t &begin, const size_t &length)
    : stream_id(stream_id), begin(begin), length(length) {}
    int32_t stream_id = -1;
    size_t  begin     = 0;
    size_t  length    = 0;
  };

  Bytes send_buffer_;
  std::deque<send_buffer_info_t> send_buffer_infos_;

  std::mutex send_buffers_prepare_lock_;
  Bytes      send_buffers_prepare_;
  std::deque<send_buffer_info_t> send_buffers_prepare_info_;

  bool has_buffer_to_send()
  {
    if (send_buffer_.size() > 0)
      return true;

    std::lock_guard<std::mutex> guard(send_buffers_prepare_lock_);
    if (send_buffers_prepare_.size() == 0)
      return false;

    send_buffer_      .swap(send_buffers_prepare_);
    send_buffer_infos_.swap(send_buffers_prepare_info_);
    return true;
  }

private:
  Reactor &reactor_;
  TcpAsyncEventHandler handler_;
};

inline
TcpAsyncSSLClient::TcpAsyncSSLClient(Reactor &reactor, const size_t &recv_buffer_size)
: reactor_(reactor)
{
  recv_buffer_.reserve(recv_buffer_size);
  handler_.on_connect         = std::bind(&TcpAsyncSSLClient::ssl_on_connect,         this);
  handler_.on_input           = std::bind(&TcpAsyncSSLClient::ssl_on_input,           this);
  handler_.on_output          = std::bind(&TcpAsyncSSLClient::ssl_on_output,          this);
  handler_.on_disconnect      = std::bind(&TcpAsyncSSLClient::ssl_on_disconnect,      this);

  handler_.on_connect_timeout = std::bind(&TcpAsyncSSLClient::handle_connect_timeout, this, std::placeholders::_1, std::placeholders::_2);
  handler_.on_connect_error   = std::bind(&TcpAsyncSSLClient::handle_connect_error,   this, std::placeholders::_1, std::placeholders::_2);
  handler_.on_timeout         = std::bind(&TcpAsyncSSLClient::handle_timeout,         this);
  handler_.on_error           = std::bind(&TcpAsyncSSLClient::handle_error,           this, std::placeholders::_1, std::placeholders::_2);
  handler_.on_shutdown        = std::bind(&TcpAsyncSSLClient::handle_shutdown,        this);
}

inline bool
TcpAsyncSSLClient::set_timeout(const uint32_t &msec)
{
  return reactor_.set_timeout(&handler_, msec);
}

inline bool
TcpAsyncSSLClient::send(const int32_t &id, const std::string_view &data)
{
  return this->send(id, reinterpret_cast<const uint8_t *>(data.data()), data.size());
}

inline bool
TcpAsyncSSLClient::send(const int32_t &id, const std::string &data)
{
  return this->send(id, (uint8_t *)data.data(), data.size());
}

inline bool
TcpAsyncSSLClient::disconnect()
{
  return handler_.disconnect();
}

inline bool
TcpAsyncSSLClient::connect(const std::string  &host,
                           const uint16_t     &port,
                           const int32_t      &timeout_msec)
{
  if (handler_.is_connect() == true)
    return false;

  close_ = false;

  if (handler_.prepare_socket(host == "localhost" ? "127.0.0.1" : host, port) == false)
    return false;

  if (handler_.connect(timeout_msec) == false)
    return false;

  return reactor_.register_event_handler(&handler_, handler_.io_handle());
}

inline bool
TcpAsyncSSLClient::is_connect() const
{
  return handler_.is_connect();
}

inline
TcpAsyncSSLClient::~TcpAsyncSSLClient()
{
  if (ssl_ != NULL)
    SSL_free(ssl_);
}

}

#endif /* WOADAPTERSSLCLIENT_H_ */
