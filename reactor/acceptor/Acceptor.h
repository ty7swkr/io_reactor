/*
 * ListenInfo.h
 *
 *  Created on: 2020. 9. 5.
 *      Author: tys
 */

#ifndef IO_REACTOR_REACTOR_ACCEPTOR_ACCEPTOR_H_
#define IO_REACTOR_REACTOR_ACCEPTOR_ACCEPTOR_H_

#include <reactor/DefinedType.h>
#include <string>

#include <string.h>
#include <unistd.h>
#include <sys/socket.h>

namespace reactor
{

class Acceptor
{
public:
  Acceptor(const bool &new_client_to_nonblocking = true)
  : new_client_to_nonblocking_(new_client_to_nonblocking)
  { memset(&addr_, 0x00, sizeof(addr_)); }

  virtual ~Acceptor() {}

  void set_client_to_nonblocking(const bool &value) { new_client_to_nonblocking_ = value; }

  bool listen_ipv4  (const uint16_t     &port,
                     const std::string  &address_  = "0.0.0.0",
                     const int          &backlog   = 100);

  bool listen_ipv4  (const uint16_t     &port,
                     const int          &backlog,
                     const std::string  &address_  = "0.0.0.0");

  bool listen_ipv6  (const uint16_t     &port,
                     const std::string  &address_  = "::0",
                     const int          &backlog   = 100);

  bool listen_ipv6  (const uint16_t     &port,
                     const int          &backlog,
                     const std::string  &address_  = "::0");

  bool listen_ipv46 (const uint16_t     &port,
                     const std::string  &address_  = "::0",
                     const int          &backlog   = 100);

  bool listen_ipv46 (const uint16_t     &port,
                     const int          &backlog,
                     const std::string  &address_  = "::0");

  bool listen_uds   (const std::string  &path,
                     const bool         &reuse = true,
                     const int          &backlog = 100);

  void close    ();
  void shutdown (const int &how);

  bool is_ipv4  () const { return ipv4_; }
  bool is_ipv6  () const { return ipv6_; }
  bool is_uds   () const { return uds_ ; }

  const uint16_t      &port     () const { return port_; }
  const std::string   &addr     () const { return address_; }
  std::string         addr_port () const { return address_ + ":" + std::to_string(port_); }

  virtual io_handle_t accept    (struct ::sockaddr_storage  &client_addr,
                                 const int32_t              &timeout_msec = -1);

  std::string err_str () const
  {
    char error_string[240];
    return std::string(strerror_r(err_code_, error_string, sizeof(error_string)-1));
  }

  const int   &err_code() const { return err_code_; }

protected:
  const io_handle_t &io_handle() const { return io_handle_; }

private:
  bool set_socketopt(const int &domain, const bool &ipv6only = false);
  void set_port_addr(const uint16_t &port, const std::string &address)
  {
    port_    = port;
    address_ = address;
  }

  bool bind_and_listen();

private:
  uint16_t    port_     = 0;
  int         backlog_  = 100;
  std::string address_;
  int         err_code_ = 0;

private:
  bool  ipv4_  = true;
  bool  ipv6_  = false;
  bool  uds_   = false; // unix domain socket

private:
  io_handle_t io_handle_ = INVALID_IO_HANDLE;
  struct sockaddr_storage addr_;

private:
  bool  new_client_to_nonblocking_ = false;
};

inline bool
Acceptor::listen_ipv4 (const uint16_t    &port,
                       const int         &backlog,
                       const std::string &address)
{ return listen_ipv4(port, address, backlog); }

inline bool
Acceptor::listen_ipv46(const uint16_t    &port,
                       const int         &backlog,
                       const std::string &address)
{ return listen_ipv46(port, address, backlog); }

inline bool
Acceptor::listen_ipv6 (const uint16_t    &port,
                       const int         &backlog,
                       const std::string &address)
{ return listen_ipv6(port, address, backlog); }

inline void
Acceptor::close()
{
  ::close(io_handle_);
}

inline void
Acceptor::shutdown(const int &how)
{
  ::shutdown(io_handle_, how);
}

}

#endif /* IO_REACTOR_REACTOR_ACCEPTOR_ACCEPTOR_H_ */
