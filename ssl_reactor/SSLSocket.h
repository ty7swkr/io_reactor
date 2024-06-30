/*
 * SSLSocket.h
 *
 *  Created on: 2020. 2. 9.
 *      Author: tys
 */

#ifndef IO_REACTOR_SSL_REACTOR_SSLSOCKET_H_
#define IO_REACTOR_SSL_REACTOR_SSLSOCKET_H_

#include <ssl_reactor/SSLInclude.h>

#include <reactor/trace.h>

#include <string>
#include <fcntl.h>
#include <cstring>

namespace reactor
{

class SSLSocket
{
public:
  enum
  {
    NULL_SSL      = -99,
    ZERO_RETURN   = -10,
    ERROR_SYSCALL = -9,
    ERROR         = -3,
    WANT_READ     = -2,
    WANT_WRITE    = -1,
    NONE          = 0
  };

  int   accept  (SSL *ssl, const int &fd);
  int   connect (SSL *ssl, const int &fd);

  int   read    (unsigned char       *buffer, const size_t &buffer_size);
  int   write   (const unsigned char *buffer, const size_t &buffer_size);
  void  clear   ();
  int   shutdown();

  const int         &error_no () const;
  const std::string &error_str() const;

private:
  int error_no_ = SSL_ERROR_NONE;
  std::string error_str_;

private:
  SSL *ssl_      = nullptr;
  int sent_size_ = 0;
};

inline const int &
SSLSocket::error_no() const
{
  return error_no_;
}

inline const std::string &
SSLSocket::error_str() const
{
  return error_str_;
}

inline int
SSLSocket::shutdown()
{
  if (ssl_ == nullptr)
    return NULL_SSL;

  int status = SSL_shutdown(ssl_);

  if (status == 0)
    return WANT_READ;

  if (status == 1)
    return NONE;

  return ERROR;
}

inline void
SSLSocket::clear()
{
  ssl_        = nullptr;
  sent_size_  = 0;
}

inline int
SSLSocket::accept(SSL *ssl, const int &fd)
{
  if (ssl == nullptr)
    return NULL_SSL;

  ERR_clear_error();

  ssl_ = ssl;
  SSL_set_fd(ssl_, fd);

  int status = SSL_accept(ssl_);

  switch (::SSL_get_error(ssl_, status))
  {
    case SSL_ERROR_NONE:
      return NONE; // To tell caller about success
    case SSL_ERROR_WANT_WRITE:
      return WANT_WRITE; // Wait for more activity
    case SSL_ERROR_WANT_READ:
      return WANT_READ; // Wait for more activity
    case SSL_ERROR_ZERO_RETURN:
      return ZERO_RETURN;
    case SSL_ERROR_SYSCALL:
      // The peer has notified us that it is shutting down via
      // the SSL "close_notify" message so we need to
      // shutdown, too.
//      printf("Peer closed connection during SSL handshake,status:%d", status);
      error_no_  = errno;
      error_str_ = std::strerror(error_no_);
      return ERROR_SYSCALL;
    default:
      // printf("Unexpected error during SSL handshake,status:%d", status);
      error_no_  = ERR_get_error();
      error_str_ = ERR_error_string(error_no_, NULL);
      return ERROR;
  }
}

inline int
SSLSocket::connect(SSL *ssl, const int &fd)
{
  if (ssl == nullptr)
    return NULL_SSL;

  ERR_clear_error();

  ssl_ = ssl;
  SSL_set_fd(ssl_, fd);

  int status = SSL_connect(ssl_);

  switch (::SSL_get_error(ssl_, status))
  {
    case SSL_ERROR_NONE:
      return NONE; // To tell caller about success
    case SSL_ERROR_WANT_WRITE:
      return WANT_WRITE; // Wait for more activity
    case SSL_ERROR_WANT_READ:
      return WANT_READ; // Wait for more activity
    case SSL_ERROR_ZERO_RETURN:
      return ZERO_RETURN;
    case SSL_ERROR_SYSCALL:
      // The peer has notified us that it is shutting down via
      // the SSL "close_notify" message so we need to
      // shutdown, too.
//      printf("Peer closed connection during SSL handshake,status:%d", status);
      error_no_  = errno;
      error_str_ = std::strerror(error_no_);
      return ERROR_SYSCALL;
    default:
      // printf("Unexpected error during SSL handshake,status:%d", status);
      error_no_  = ERR_get_error();
      error_str_ = ERR_error_string(error_no_, NULL);
      return ERROR;
  }
}

inline int
SSLSocket::read(unsigned char *buffer, const size_t &buffer_size)
{
  if (ssl_ == nullptr)
    return NULL_SSL;

  ERR_clear_error();

  int read_size = SSL_read(ssl_, buffer, buffer_size);
  int ssl_error = SSL_get_error(ssl_, read_size);
  //reactor_trace << ssl_error << read_size;

  // 끊겼을때 0을 리턴함. ??
  if (read_size == 0)
    return ZERO_RETURN;

  if (read_size < 0)
  {
    switch (ssl_error)
    {
      case SSL_ERROR_NONE:
        return NONE; // To tell caller about success
      case SSL_ERROR_WANT_WRITE:
        return WANT_WRITE;
      case SSL_ERROR_WANT_READ:
        return WANT_READ;
      case SSL_ERROR_ZERO_RETURN:
        return ZERO_RETURN;
      case SSL_ERROR_SYSCALL:
        error_no_  = errno;
        error_str_ = std::strerror(error_no_);
        return ERROR_SYSCALL;
      default:
//          long error = ERR_get_error();
//          const char* error_string = ERR_error_string(error, NULL);
//          printf("could not SSL_read (returned -1) %s\n", error_string);
        error_no_  = ERR_get_error();
        error_str_ = ERR_error_string(error_no_, NULL);
        return ERROR;
    }
  }

  // COMPLETE
  return read_size;
}

inline int
SSLSocket::write(const unsigned char *buffer, const size_t &buffer_size)
{
  if (ssl_ == nullptr)
    return NULL_SSL;

//  NonblockSetter set_nonblock(fd);
  ERR_clear_error();
  int write_size = SSL_write(ssl_, buffer, buffer_size - sent_size_);
  if (write_size == 0)
  {
    // 끊겼을때 0을 리턴함.
//      long error = ERR_get_error();
//      const char* error_str = ERR_error_string(error, NULL);
//      printf("could not SSL_write (returned 0): %d %s\n", SSL_get_error(ssl_, write_size), error_str);
//      printf("could not SSL_write (returned 0): %d %s\n", buffer_size, strerror(errno));
    return ZERO_RETURN;
  }

  if (write_size < 0)
  {
    switch (SSL_get_error(ssl_, write_size))
    {
      case SSL_ERROR_WANT_WRITE:
        return WANT_WRITE;
      case SSL_ERROR_WANT_READ:
        return WANT_READ;
      case SSL_ERROR_ZERO_RETURN:
        return ZERO_RETURN;
      case SSL_ERROR_NONE:
        return NONE;
      case SSL_ERROR_SYSCALL:
        error_no_  = errno;
        error_str_ = std::strerror(error_no_);
        return ERROR_SYSCALL;
      default:
//          long error = ERR_get_error();
//          const char* error_string = ERR_error_string(error, NULL);
//          printf("could not SSL_write (returned -1) %s\n", error_string);
        error_no_  = ERR_get_error();
        error_str_ = ERR_error_string(error_no_, NULL);
        sent_size_ = 0;
        return ERROR;
    }
  }

  sent_size_ += write_size;

  // COMPLETE
  if ((int)buffer_size >= sent_size_)
  {
    int sent_size = sent_size_;
    sent_size_ = 0;
    return sent_size;
  }

  return WANT_WRITE;
}

}

#endif /* IO_REACTOR_SSL_REACTOR_SSLSOCKET_H_ */
