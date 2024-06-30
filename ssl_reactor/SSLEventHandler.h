/*
 * SSLEventHandler.h
 *
 *  Created on: 2022. 9. 7.
 *      Author: tys
 */

#ifndef IO_REACTOR_SSL_REACTOR_SSLEVENTHANDLER_H_
#define IO_REACTOR_SSL_REACTOR_SSLEVENTHANDLER_H_

#include <ssl_reactor/SSLSocket.h>
#include <ssl_reactor/SSLState.h>

#include <reactor/acceptor/Acceptor.h>
#include <reactor/Reactors.h>
#include <reactor/EventHandler.h>
#include <reactor/trace.h>

#include <string>
#include <arpa/inet.h>

namespace reactor
{

class SSLSessionHandler;

using Bytes = std::vector<uint8_t>;

class SSLEventHandler : public EventHandler
{
public:
  SSLEventHandler() = delete;
  static SSLEventHandler *create(SSLSessionHandler       *ssl_session,
                                 const sockaddr_storage  &client_addr,
                                 Acceptor                &acceptor,
                                 Reactors                &reactors)
  {
    return new SSLEventHandler(ssl_session, client_addr, acceptor, reactors);
  }

  virtual ~SSLEventHandler();

  bool init_ssl(SSL_CTX *ssl_ctx);
  bool send    (const int32_t &id, const uint8_t *data, const size_t &size);
  bool close   ();
  bool set_output_event();

  int direct_send(const uint8_t *data, const size_t &size);
  int direct_send(const void    *data, const size_t &size);

  Acceptor        &acceptor () { return acceptor_; }
  Reactor         *reactor  () { return reactor_; }
  Reactors        &reactors () { return reactors_; }
  sockaddr_storage addr     () { return addr_; }

protected:
  SSLEventHandler(SSLSessionHandler       *ssl_session,
                  const sockaddr_storage  &client_addr,
                  Acceptor                &acceptor,
                  Reactors                &reactors);

protected:
  void handle_registered() override;
  void handle_removed   () override;
  void handle_input     () override;
  void handle_output    () override;
  void handle_close     () override;
  void handle_timeout   () override;
  void handle_error     (const int &error_no = 0, const std::string &error_str = "") override;
  void handle_shutdown  () override;

protected:
  std::atomic<bool> set_output_event_;

protected:
  std::shared_ptr<SSLEventHandler> shared_from_this_;
  SSLSessionHandler *ssl_session_ = nullptr;

protected:
  SSL_CTX *ssl_ctx_ = NULL;
  SSL     *ssl_     = NULL;

  SSL_STATE ssl_state_ = SSL_STATE::NONE;
  SSLSocket ssl_socket_;
  bool      ssl_accept_done_ = false;

  void process_ssl();
  void ssl_accept ();
  void ssl_read   ();
  void ssl_write  ();

protected:
  Bytes recv_buffer_;

  struct send_buffer_info_t
  {
    send_buffer_info_t() {}
    send_buffer_info_t(const int &id, const size_t &begin, const size_t &length)
    : id(id), begin(begin), length(length) {}
    int32_t id      = -1;
    size_t  begin   = 0;
    size_t  length  = 0;
  };

  Bytes send_buffer_;
  std::deque<send_buffer_info_t> send_buffer_infos_;

  std::mutex  send_buffers_prepare_lock_;
  Bytes       send_buffers_prepare_;
  std::deque<send_buffer_info_t> send_buffers_prepare_info_;

  bool has_buffer_to_send()
  {
    if (send_buffer_.size() > 0)
      return true;

    std::lock_guard<std::mutex> guard(send_buffers_prepare_lock_);
    if (send_buffers_prepare_.size() == 0)
      return false;

    send_buffer_.swap(send_buffers_prepare_);
    send_buffer_infos_.swap(send_buffers_prepare_info_);

    return true;
  }

protected:
  bool close_ = false;
  sockaddr_storage addr_;
  Acceptor &acceptor_;
  Reactors &reactors_;
};

inline
SSLEventHandler::~SSLEventHandler()
{
  if (ssl_ == NULL)
    return;

  SSL_clear(ssl_);
  SSL_free(ssl_);
}

inline int
SSLEventHandler::direct_send(const uint8_t *data, const size_t &size)
{
  int sent_size = 0;
  while (sent_size < (int)size)
  {
    int write_size = ssl_socket_.write(data, size);
    if (write_size <= 0)
      return write_size;

    sent_size += write_size;
  }

  return sent_size;
}

inline int
SSLEventHandler::direct_send(const void *data, const size_t &size)
{
  return this->direct_send((uint8_t *)data, size);
}

inline bool
SSLEventHandler::set_output_event()
{
  if (io_handle_ == INVALID_IO_HANDLE || reactor_ == nullptr || close_ == true)
    return false;

  bool comparand = false;
  bool exchanged = set_output_event_.compare_exchange_strong(comparand, true);
  if (exchanged == false)
    return false;

  return reactor_->register_writable(this);
}

inline bool
SSLEventHandler::send(const int32_t &id, const uint8_t *data, const size_t &size)
{
  if (io_handle_ == INVALID_IO_HANDLE || reactor_ == nullptr || close_ == true)
    return false;

  {
    std::lock_guard<std::mutex> guard(send_buffers_prepare_lock_);
    size_t begin = send_buffers_prepare_.size();
    send_buffers_prepare_     .insert(send_buffers_prepare_.end(), data, data + size);
    send_buffers_prepare_info_.emplace_back(id, begin, size);
  }

  reactor_->register_writable(this);

  return true;
}

inline bool
SSLEventHandler::close()
{
  if (close_ == true || io_handle_ == INVALID_IO_HANDLE)
    return false;

  close_ = true;
  ssl_socket_.shutdown();

//  WANT_READ의 경우 상대방이 close_notify를 send하여야 하나 그렇지 않은 경우
//  가 존재하므로 아래 코드는 주석 처리하였다.
//  switch (ssl_socket_.shutdown())
//  {
//    case SSLSocket::WANT_READ: ssl_state_ = SSL_STATE::READ; return true;
//    case SSLSocket::NONE     :
//    default:
//      ::shutdown(io_handle_, SHUT_RD); return true;
//  }
//  ::shutdown(io_handle_, SHUT_RD);
  ::shutdown(io_handle_, SHUT_WR);
  return true;
}

}


#endif /* SSLEVENTHANDLER_H_ */
