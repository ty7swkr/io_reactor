#include "TcpAsyncEventHandler.h"

using namespace reactor;

bool
TcpAsyncEventHandler::set_addr_ipv4(struct sockaddr_storage &server_addr,
                                    const std::string &host, const uint16_t &port)
{
  memset(&server_addr, 0, sizeof(struct sockaddr_storage));
  struct sockaddr_in &in_addr = *((struct sockaddr_in *)(&server_addr));

  in_addr.sin_family = AF_INET;
  in_addr.sin_port   = htons(port);
  if (inet_pton(AF_INET, host.c_str(), &in_addr.sin_addr.s_addr) != 1)
    return false;

  ipv6_ = false;
  return true;
}

bool
TcpAsyncEventHandler::set_addr_ipv6(struct sockaddr_storage &server_addr,
                                    const std::string &host, const uint16_t &port)
{
  memset(&server_addr, 0, sizeof(struct sockaddr_storage));
  struct sockaddr_in6 &in6_addr = *((struct sockaddr_in6 *)(&server_addr));

  in6_addr.sin6_family = AF_INET6;
  in6_addr.sin6_port   = htons(port);
  if (inet_pton(AF_INET6, host.c_str(), &in6_addr.sin6_addr) != 1)
    return false;

  ipv6_ = true;
  return true;
}

bool
TcpAsyncEventHandler::prepare_socket(const std::string  &host,
                                     const uint16_t     &port)
{
  if (io_handle_ != INVALID_IO_HANDLE)
    ::close(io_handle_);

  if      (set_addr_ipv4(server_addr_, host, port) == true)
    io_handle_ = socket(AF_INET,  SOCK_STREAM, 0);
  else if (set_addr_ipv6(server_addr_, host, port) == true)
    io_handle_ = socket(AF_INET6, SOCK_STREAM, 0);
  else
    return false;

  if (io_handle_ < 0)
    return false;

  static int enable = 1;
  if (setsockopt(io_handle_, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
    return false;

  struct linger solinger = { 1, 0 };
  if (setsockopt(io_handle_, SOL_SOCKET, SO_LINGER, &solinger, sizeof(struct linger)) < 0)
    return false;

  io_handle_fcntl_ = fcntl(io_handle_, F_GETFL, 0);
  if (fcntl(io_handle_, F_SETFL, io_handle_fcntl_ | O_NONBLOCK) < 0)
    return false;

  return true;
}

bool
TcpAsyncEventHandler::connect(const int32_t &timeout_msec)
{
  if (io_handle_ == INVALID_IO_HANDLE)
    return false;

  connect_timeout_ = timeout_msec;

  if (server_addr_.ss_family == AF_INET)
    ::connect(io_handle_, (struct sockaddr *)&server_addr_, sizeof(sockaddr_in));
  else
    ::connect(io_handle_, (struct sockaddr *)&server_addr_, sizeof(sockaddr_in6));

  return true;
}

















