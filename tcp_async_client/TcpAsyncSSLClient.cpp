#include "TcpAsyncSSLClient.h"

using namespace reactor;

void
TcpAsyncSSLClient::ssl_on_connect()
{
  ssl_socket_.clear();

  ssl_ctx_ = SSLContext::create_client(
      [&](const int &err_no, const std::string &err_str)
      { handle_error(err_no, err_str); });

  if (ssl_ctx_ == nullptr)
    return;

  if (ssl_ != nullptr)
    SSL_free(ssl_);

  ssl_ = SSL_new(ssl_ctx_->object());

  ssl_accept_done_ = false;

  handle_connect();

  process_ssl();
}

void
TcpAsyncSSLClient::ssl_on_disconnect()
{
  recv_buffer_.clear();
  send_buffer_.clear();
  send_buffer_infos_.clear();

  {
    std::lock_guard<std::mutex> guard(send_buffers_prepare_lock_);
    send_buffers_prepare_.clear();
    send_buffers_prepare_info_.clear();
  }

  handle_disconnect();
}

void
TcpAsyncSSLClient::ssl_on_input()
{
  if (ssl_state_ == SSL_STATE::NONE)
    ssl_state_ = SSL_STATE::READ;

  process_ssl();
}

void
TcpAsyncSSLClient::ssl_on_output()
{
  if (ssl_state_ == SSL_STATE::NONE)
    ssl_state_ = SSL_STATE::WRITE;

  process_ssl();
}

bool
TcpAsyncSSLClient::close()
{
  if (close_ ==  true || ssl_ == NULL || handler_.io_handle() == INVALID_IO_HANDLE)
    return false;

  close_ = true;
  switch (ssl_socket_.shutdown())
  {
    case SSLSocket::WANT_READ: ssl_state_ = SSL_STATE::READ; return true;
    case SSLSocket::NONE     :
    default:
      handler_.disconnect();
      return true;
  }
  handler_.disconnect();
  return true;
}

void
TcpAsyncSSLClient::process_ssl()
{
  if (ssl_accept_done_ == false)
  {
    ssl_connect();
    return;
  }

  switch (ssl_state_)
  {
    case SSL_STATE::READ   : ssl_read   (); break;
    case SSL_STATE::WRITE  : ssl_write  (); break;
    case SSL_STATE::NONE   : break;
  }
}

void
TcpAsyncSSLClient::ssl_connect()
{
  int result = ssl_socket_.connect(ssl_, handler_.io_handle());

  switch (result)
  {
    case SSLSocket::ERROR:
    case SSLSocket::ERROR_SYSCALL:
      handle_error(ssl_socket_.error_no(), to_string(ssl_state_) + ":"+ ssl_socket_.error_str());
      reactor_.remove_event_handler(&handler_);
      return;
    case SSLSocket::WANT_READ:
      return;
    case SSLSocket::WANT_WRITE:
      reactor_.register_writable(&handler_);
      return;
    default: // COMPLETE
      break;
  }

  ssl_state_ = SSL_STATE::NONE;

  handle_accept(ssl_);

  ssl_accept_done_ = true;
}

void
TcpAsyncSSLClient::ssl_read()
{
  while (true)
  {
    recv_buffer_.resize(recv_buffer_.capacity());
    int read_size = SSL_read(ssl_, recv_buffer_.data(), recv_buffer_.size());

    if (read_size <= 0)
    {
      int ssl_error = SSL_get_error(ssl_, read_size);
      switch (ssl_error)
      {
        case SSL_ERROR_WANT_WRITE: // ???
        case SSL_ERROR_WANT_READ:
          return;
        case SSL_ERROR_ZERO_RETURN:
          reactor_.remove_event_handler(&handler_);
          return;
        case SSL_ERROR_SYSCALL:
          if (read_size == 0)
          {
            reactor_.remove_event_handler(&handler_);
            return;
          }
          handle_error(errno, std::strerror(errno));
          return;
        case SSL_ERROR_NONE:
        default:
          int err_no  = ERR_get_error();
          handle_error(err_no, std::string("SSL:") + ERR_error_string(err_no, NULL));
          return;
      }
    }

    ssl_state_ = SSL_STATE::NONE;
    recv_buffer_.resize(read_size);
    handle_recv(recv_buffer_);
  }
}

bool
TcpAsyncSSLClient::send(const int32_t &id, const uint8_t *data, const size_t &size)
{
  if (size == 0 || handler_.io_handle() == INVALID_IO_HANDLE || this->is_connect() == false)
    return false;

  {
    std::lock_guard<std::mutex> guard(send_buffers_prepare_lock_);
    size_t begin = send_buffers_prepare_.size();
    send_buffers_prepare_     .insert(send_buffers_prepare_.end(), data, data + size);
    send_buffers_prepare_info_.emplace_back(id, begin, size);
  }

  if (ssl_state_ == SSL_STATE::NONE)
    ssl_state_ = SSL_STATE::WRITE;

  reactor_.register_writable(&handler_);

  return true;
}

void
TcpAsyncSSLClient::ssl_write()
{
  if (has_buffer_to_send() == false)
  {
    ssl_state_ = SSL_STATE::NONE;
    return;
  }

  int write_size = ssl_socket_.write(send_buffer_.data(), send_buffer_.size());
  switch (write_size)
  {
    case SSLSocket::ERROR:
      for (const auto &info : send_buffer_infos_)
         handle_sent_error(ssl_socket_.error_no(), "SSL:" + ssl_socket_.error_str(),
                           info.stream_id, send_buffer_.data() + info.begin, info.length);
      reactor_.remove_event_handler(&handler_);
      return;

      // syscall error인 경우 reactor에서 handle_error을 호출함.
    case SSLSocket::ERROR_SYSCALL:
      for (const auto &info : send_buffer_infos_)
         handle_sent_error(ssl_socket_.error_no(), "SSL:" + ssl_socket_.error_str(),
                           info.stream_id, send_buffer_.data() + info.begin, info.length);
      return;

    case SSLSocket::WANT_READ:
      return;

    case SSLSocket::WANT_WRITE:
      reactor_.register_writable(&handler_);
      return;

    case SSLSocket::ZERO_RETURN:
      if (SSL_get_shutdown(ssl_) == SSL_RECEIVED_SHUTDOWN)
      {
        reactor_.remove_event_handler(&handler_);
        return;
      }
      return;

    default:
      for (const auto &info : send_buffer_infos_)
        handle_sent(info.stream_id, send_buffer_.data() + info.begin, info.length);

      // goto를 뺏다. 이벤트를 이쪽에서 잡고 있으면 read 기회를 잃을 수 있기 때문에...
      send_buffer_.clear();
      send_buffer_infos_.clear();

      ssl_state_ = SSL_STATE::NONE;
  }

  if (has_buffer_to_send() == true)
    reactor_.register_writable(&handler_);
}


