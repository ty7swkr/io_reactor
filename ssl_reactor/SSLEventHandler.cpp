#include "SSLEventHandler.h"
#include "SSLSessionHandler.h"

using namespace reactor;

SSLEventHandler::SSLEventHandler(SSLSessionHandler       *ssl_session,
                                 const sockaddr_storage  &client_addr,
                                 Acceptor                &acceptor,
                                 Reactors                &reactors)
: ssl_session_(ssl_session), addr_(client_addr), acceptor_(acceptor), reactors_(reactors)
{
  shared_from_this_ = std::shared_ptr<SSLEventHandler>(this);

  ssl_session_->ssl_handler_ = shared_from_this_;
  ssl_session_->set_socket_address(client_addr);

  recv_buffer_.reserve(10240);
  recv_buffer_.resize(10240);
  send_buffers_prepare_.reserve(10240);

  set_output_event_ = false;
}

bool
SSLEventHandler::init_ssl(SSL_CTX *ssl_ctx)
{
  ssl_socket_.clear();

  if (ssl_ctx_ != NULL && ssl_ctx == ssl_ctx_)
  {
    SSL_clear(ssl_);
    return true;
  }

  if (ssl_ != NULL)
    SSL_free(ssl_);

  ssl_ctx_  = ssl_ctx;
  ssl_      = SSL_new(ssl_ctx_);

  return true;
}

void
SSLEventHandler::handle_registered()
{
  ssl_state_ = SSL_STATE::NONE;
  ssl_session_->handle_registered();
}

void
SSLEventHandler::handle_removed()
{
  // 시스템으로부터 소켓이 재할당 되지 않도록 reactor에서 제거 된 후 close 한다.
  ssl_session_->handle_removed();

  ::close(io_handle_);
  this->shared_from_this_.reset();
}

void
SSLEventHandler::handle_input()
{
  if (ssl_state_ == SSL_STATE::NONE)
    ssl_state_ = SSL_STATE::READ;

  process_ssl();
}

void
SSLEventHandler::handle_output()
{
  if (ssl_state_ == SSL_STATE::NONE)
    ssl_state_ = SSL_STATE::WRITE;

  process_ssl();

  bool comparand = true;
  bool exchanged = set_output_event_.compare_exchange_strong(comparand, false);
  if (exchanged == true)
    ssl_session_->handle_output();
}

void
SSLEventHandler::handle_close()
{
  ssl_session_->handle_close();
  reactor_->remove_event_handler(this);
}

void
SSLEventHandler::handle_timeout()
{
  ssl_session_->handle_timeout();
}

void
SSLEventHandler::handle_error(const int &err_no, const std::string &err_str)
{
  ssl_session_->handle_error(ssl_state_, err_no, err_str);
  ::shutdown(io_handle_, SHUT_WR);
}

inline void
SSLEventHandler::handle_shutdown()
{
  ssl_session_->handle_shutdown();
}

inline void
SSLEventHandler::process_ssl()
{
  if (ssl_accept_done_ == false)
  {
    ssl_accept();
    return;
  }

  switch (ssl_state_)
  {
    case SSL_STATE::READ   : ssl_read (); break;
    case SSL_STATE::WRITE  : ssl_write(); break;
    case SSL_STATE::NONE   : break;
  }
}

void
SSLEventHandler::ssl_accept()
{
  int result = ssl_socket_.accept(ssl_, io_handle_);
  switch (result)
  {
    case SSLSocket::ERROR:
      // 에러 처리해야함.
      ssl_session_->handle_error(ssl_state_, ssl_socket_.error_no(), ssl_socket_.error_str());
      ::shutdown(io_handle_, SHUT_WR);
      return;

    case SSLSocket::ERROR_SYSCALL:
      if (ssl_socket_.error_no() != 0)
        ssl_session_->handle_error(ssl_state_, ssl_socket_.error_no(), ssl_socket_.error_str());
      return;

    case SSLSocket::WANT_READ:
      return;

    case SSLSocket::WANT_WRITE:
      reactor_->register_writable(this);
      return;

    default: // COMPLETE
    {
      ssl_state_        = SSL_STATE::NONE;
      ssl_accept_done_  = true;

      const unsigned char *ssl_alpn_bytes = NULL;
      unsigned int         ssl_alpn_len   = 0;

#ifndef OPENSSL_NO_NEXTPROTONEG
      SSL_get0_next_proto_negotiated(ssl_, &ssl_alpn_bytes, &ssl_alpn_len);
#endif /* !OPENSSL_NO_NEXTPROTONEG */
#if OPENSSL_VERSION_NUMBER >= 0x10002000L
      SSL_get0_alpn_selected(ssl_, &ssl_alpn_bytes, &ssl_alpn_len);
#endif /* OPENSSL_VERSION_NUMBER >= 0x10002000L */

      std::string ssl_alpn(ssl_alpn_bytes, ssl_alpn_bytes+ssl_alpn_len);
      ssl_session_->handle_accept(ssl_alpn);
    }
  }
}

void
SSLEventHandler::ssl_read()
{
  read_ssl_socket:
  int read_size = ssl_socket_.read(recv_buffer_.data(), recv_buffer_.size());

  switch (read_size)
  {
    case SSLSocket::ERROR:
      ::shutdown(io_handle_, SHUT_WR);
      return;

    // syscall error인 경우 reactor에서 handle_error을 호출함. 테스트됨.
    case SSLSocket::ERROR_SYSCALL:
      ssl_session_->handle_error(ssl_state_, ssl_socket_.error_no(), ssl_socket_.error_str());
      return;

    case SSLSocket::WANT_READ:
      return;

    case SSLSocket::WANT_WRITE:
      reactor_->register_writable(this);
      return;

    case SSLSocket::NONE:
    case SSLSocket::ZERO_RETURN:
      if (SSL_get_shutdown(ssl_) == SSL_RECEIVED_SHUTDOWN)
      {
        // close socket
        ::shutdown(io_handle_, SHUT_WR);
        return;
      }
      return;
    default: // 받음.
    {
      ssl_state_ = SSL_STATE::NONE;
      ssl_session_->handle_input(recv_buffer_.data(), read_size);

      // 더 읽을게 있는지 체크.
      if (read_size >= (int)recv_buffer_.size())
        goto read_ssl_socket;

      return;
    }
  }
}

void
SSLEventHandler::ssl_write()
{
  if (has_buffer_to_send() == false)
  {
    ssl_state_ = SSL_STATE::NONE;
    return;
  }

  int write_size = ssl_socket_.write(send_buffer_.data(), send_buffer_.size());

  // 부하테스트 해봐야 함.
  switch (write_size)
  {
    case SSLSocket::ERROR:
      ::shutdown(io_handle_, SHUT_WR);
      return;

      // syscall error인 경우 reactor에서 handle_error을 호출함. 테스트됨.
    case SSLSocket::ERROR_SYSCALL:
      for (const auto &info : send_buffer_infos_)
        ssl_session_->handle_sent_error(ssl_state_, ssl_socket_.error_no(), ssl_socket_.error_str(),
                                        info.id, send_buffer_.data() + info.begin, info.length);
      return;

    case SSLSocket::WANT_READ:
      return;

    case SSLSocket::WANT_WRITE:
      reactor_->register_writable(this);
      return;

    case SSLSocket::ZERO_RETURN:
    {
      if (SSL_get_shutdown(ssl_) == SSL_RECEIVED_SHUTDOWN)
      {
        ::shutdown(io_handle_, SHUT_WR);
        return;
      }
      return;
    }
    default:
    {
      for (const auto &info : send_buffer_infos_)
        ssl_session_->handle_sent(info.id, send_buffer_.data() + info.begin, info.length);

      // goto를 뺏다. 이벤트를 이쪽에서 잡고 있으면 read 기회를 잃을 수 있기 때문에...
      send_buffer_.clear();
      send_buffer_infos_.clear();

      ssl_state_ = SSL_STATE::NONE;
      return;
    }
  }
}

