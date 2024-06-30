/*
 * WoClient.h
 *
 *  Created on: 2021. 8. 4.
 *      Author: tys
 */

#ifndef IO_REACTOR_REACTOR_TCPASYNCCLIENT_H_
#define IO_REACTOR_REACTOR_TCPASYNCCLIENT_H_

#include <tcp_async_client/TcpAsyncEventHandler.h>

#include <vector>
#include <functional>

namespace reactor
{

class TcpAsyncClient // Reactor와 EventHandler를 제어
{
public:
  TcpAsyncClient(Reactor &reactor, const size_t &recv_buffer_size = 1024);
  virtual ~TcpAsyncClient() {}

  // disable copy
  TcpAsyncClient(const TcpAsyncClient &) = delete;
  TcpAsyncClient& operator=(const TcpAsyncClient &) = delete;

public:
  // all thread-safe
  virtual bool  connect               (const std::string  &host,
                                       const uint16_t     &port,
                                       const int32_t      &timeout_msec = 5000);
  bool          disconnect            ();
  void          set_timeout           (const uint32_t &msec);
  bool          send                  (const int32_t &stream_id, const uint8_t *data, const size_t &size);
  bool          send                  (const int32_t &stream_id, const void    *data, const size_t &size);
  bool          send                  (const int32_t &stream_id, const std::string &data);
  bool          is_connect            () const;
  void          close                 () { ::shutdown(handler_.io_handle(), SHUT_WR); }

  std::string   local_addr_port       () const { return handler_.local_addr_port(); }
  std::string   local_addr            () const { return handler_.local_addr(); }
  uint16_t      local_port            () const { return handler_.local_port(); }
  int32_t       io_handle             () { return handler_.io_handle(); }

protected:
  virtual void  handle_connect        () = 0;
  virtual void  handle_connect_error  (const int &err_no, const std::string &err_str) = 0;
  virtual void  handle_connect_timeout(const int &err_no, const std::string &err_str) = 0;
  virtual void  handle_disconnect     () = 0;
  virtual void  handle_recv           (const std::vector<uint8_t> &data) = 0;
  virtual void  handle_sent           (const int32_t &id, const uint8_t *data, const size_t &size) = 0;
  virtual void  handle_sent_error     (const int &err_no, const std::string &err_str,
                                       const int32_t &id, const uint8_t *data, const size_t &size) = 0;
  virtual void  handle_error          (const int &err_no, const std::string &err_str) = 0;
  virtual void  handle_timeout        () = 0;
  virtual void  handle_shutdown       () = 0;

protected:
  void  on_connect();
  void  on_input  ();
  void  on_output ();

private:
  bool  has_buffer_to_send();

private:
  using Bytes_ = std::vector<uint8_t>;
  Bytes_ recv_buffer_;

  struct send_buffer_info_t
  {
    send_buffer_info_t() {}
    send_buffer_info_t(const int &stream_id, const size_t &begin, const size_t &length)
    : stream_id(stream_id), begin(begin), length(length) {}
    int32_t stream_id = -1;
    size_t  begin     = 0;
    size_t  length    = 0;
  };

  Bytes_  send_buffer_;
  int     sent_size_ = 0;
  std::deque<send_buffer_info_t> send_buffer_infos_;

  std::mutex  send_buffers_prepare_lock_;
  Bytes_      send_buffers_prepare_;
  std::deque<send_buffer_info_t> send_buffers_prepare_info_;

  void init_buffers();

private:
  Reactor &reactor_;
  TcpAsyncEventHandler handler_;
};

inline void
TcpAsyncClient::init_buffers()
{
  recv_buffer_.clear();
  send_buffer_.clear();
  std::lock_guard<std::mutex> guard(send_buffers_prepare_lock_);
  send_buffers_prepare_.clear();
  send_buffers_prepare_info_.clear();
}

inline
TcpAsyncClient::TcpAsyncClient(Reactor &reactor, const size_t &recv_buffer_size)
: reactor_(reactor)
{
  recv_buffer_.resize(recv_buffer_size);

  handler_.on_connect         = std::bind(&TcpAsyncClient::on_connect,            this);
  handler_.on_connect_error   = std::bind(&TcpAsyncClient::handle_connect_error,  this, std::placeholders::_1, std::placeholders::_2);
  handler_.on_connect_timeout = std::bind(&TcpAsyncClient::handle_connect_timeout,this, std::placeholders::_1, std::placeholders::_2);
  handler_.on_disconnect      = std::bind(&TcpAsyncClient::handle_disconnect,     this);
  handler_.on_timeout         = std::bind(&TcpAsyncClient::handle_timeout,        this);
  handler_.on_input           = std::bind(&TcpAsyncClient::on_input ,             this);
  handler_.on_output          = std::bind(&TcpAsyncClient::on_output,             this);
  handler_.on_error           = std::bind(&TcpAsyncClient::handle_error,          this, std::placeholders::_1, std::placeholders::_2);
  handler_.on_shutdown        = std::bind(&TcpAsyncClient::handle_shutdown,       this);
}

inline void
TcpAsyncClient::set_timeout(const uint32_t &msec)
{
  reactor_.set_timeout(&handler_, msec);
}

inline bool
TcpAsyncClient::disconnect()
{
  return handler_.disconnect();
}

inline bool
TcpAsyncClient::connect(const std::string  &host,
                        const uint16_t     &port,
                        const int32_t      &timeout_msec)
{
  if (handler_.prepare_socket(host == "localhost" ? "127.0.0.1" : host, port) == false)
    return false;

  if (handler_.connect(timeout_msec) == false)
    return false;

  return reactor_.register_event_handler(&handler_, handler_.io_handle());
}

inline bool
TcpAsyncClient::send(const int32_t &stream_id, const uint8_t *data, const size_t &size)
{
  if (size == 0 || handler_.io_handle() == INVALID_IO_HANDLE || this->is_connect() == false)
    return false;

  {
    std::lock_guard<std::mutex> guard(send_buffers_prepare_lock_);
    size_t begin = send_buffers_prepare_.size();
    send_buffers_prepare_     .insert(send_buffers_prepare_.end(), data, data + size);
    send_buffers_prepare_info_.emplace_back(stream_id, begin, size);
  }

  reactor_.register_writable(&handler_);
  return true;
}

inline bool
TcpAsyncClient::send(const int32_t &stream_id, const void *data, const size_t &size)
{
  return this->send(stream_id, (uint8_t *)data, size);
}

inline bool
TcpAsyncClient::send(const int32_t &stream_id, const std::string &data)
{
  return this->send(stream_id, (const uint8_t *)data.data(), data.size());
}

inline void
TcpAsyncClient::on_connect()
{
  init_buffers();
  handle_connect();
}

inline void
TcpAsyncClient::on_input()
{
  recv_buffer_.resize(recv_buffer_.capacity());

  ssize_t recv_size = ::recv(handler_.io_handle(), recv_buffer_.data(), recv_buffer_.size(), 0);

  recv_buffer_.resize(recv_size);
  handle_recv(recv_buffer_);
}

inline void
TcpAsyncClient::on_output()
{
  if (has_buffer_to_send() == false)
    return;

  int write_size = ::send(handler_.io_handle(),
                          send_buffer_.data() + sent_size_,
                          send_buffer_.size() - sent_size_, 0);
  if (write_size <= 0)
  {
    int err_no = errno;
    if (err_no == EAGAIN)
    {
      this->reactor_.register_writable(&handler_);
      return;
    }

    char str[256];
    std::string err_str = strerror_r(err_no, str, sizeof(str));

    for (const auto &info : send_buffer_infos_)
      handle_sent_error(err_no, err_str, info.stream_id, send_buffer_.data() + info.begin, info.length);

    return;
  }

  sent_size_ += write_size;
  if ((ssize_t)send_buffer_.size() < sent_size_)
    return;

  sent_size_ = 0;
  for (const auto &info : send_buffer_infos_)
    handle_sent(info.stream_id, send_buffer_.data() + info.begin, info.length);

  send_buffer_.clear();
  send_buffer_infos_.clear();

  // send compelte. 함수호출 해야함.
  if (has_buffer_to_send() == true)
    this->reactor_.register_writable(&handler_);

  return;
}

inline bool
TcpAsyncClient::is_connect() const
{
  return handler_.is_connect();
}

inline bool
TcpAsyncClient::has_buffer_to_send()
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

}

#endif /* WOADAPTERCLIENT_H_ */
