#include <reactor/acceptor/Acceptor.h>
#include <reactor/trace.h>

#include <chrono>
#include <cstring>

#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/poll.h>
#include <sys/un.h>

using namespace reactor;

bool
Acceptor::bind_and_listen()
{
  err_code_ = 0;

  struct sockaddr_in6 &addr = *(reinterpret_cast<struct sockaddr_in6 *>(&addr_));

  /* don't forget your error checking for these calls: */
  if (::bind(io_handle_, (struct sockaddr *)&addr, sizeof(addr_)) < 0)
  {
    err_code_ = errno;
    reactor_trace << this->address_ << this->port_ << strerror(err_code_) << std::endl;
    return false;
  }

  if (::listen(io_handle_, backlog_) < 0)
  {
    err_code_ = errno;
//    reactor_trace << strerror(errno) << ":" << io_handle_ << std::endl;
    return false;
  }

  int io_flag = fcntl(io_handle_, F_GETFL, 0);
  fcntl(io_handle_, F_SETFL, io_flag | O_NONBLOCK);

  return true;
}

bool
Acceptor::listen_ipv4(const uint16_t     &port,
                      const std::string  &address,
                      const int          &backlog)
{
  backlog_ = backlog;
  if (set_socketopt(AF_INET) == false)
    return false;

  struct sockaddr_in &addr = *(reinterpret_cast<struct sockaddr_in *>(&addr_));

  addr.sin_family  = AF_INET;      /* host byte order */
  addr.sin_port    = htons(port);  /* short, network byte order */
  inet_pton(AF_INET, address.c_str(), &(addr.sin_addr));
  memset(&(addr.sin_zero), 0x00, sizeof(addr.sin_zero)); /* zero the rest of the struct */

  set_port_addr(port, address);

  if (bind_and_listen() == false)
    return false;

  ipv4_ = true;
  ipv6_ = false;
  uds_  = false;

  return true;
}

bool
Acceptor::listen_ipv46(const uint16_t    &port,
                       const std::string &address,
                       const int         &backlog)
{
  backlog_ = backlog;
  if (set_socketopt(AF_INET6) == false)
    return false;

  struct sockaddr_in6 &addr = *(reinterpret_cast<struct sockaddr_in6 *>(&addr_));

  //  struct sockaddr_in6 addr6_;
  addr.sin6_family = AF_INET6;     /* host byte order */
  addr.sin6_port   = htons(port);  /* short, network byte order */
  inet_pton(AF_INET6, address.c_str(), &(addr.sin6_addr));
  set_port_addr(port, address);

  if (bind_and_listen() == false)
    return false;

  ipv4_ = true;
  ipv6_ = true;
  uds_  = false;

  return true;
}

bool
Acceptor::listen_ipv6(const uint16_t     &port,
                      const std::string  &address,
                      const int          &backlog)
{
  backlog_ = backlog;
  if (set_socketopt(AF_INET6, true) == false)
    return false;

  struct sockaddr_in6 &addr = *(reinterpret_cast<struct sockaddr_in6 *>(&addr_));

  //  struct sockaddr_in6 addr6_;
  addr.sin6_family = AF_INET6;     /* host byte order */
  addr.sin6_port   = htons(port);  /* short, network byte order */
  inet_pton(AF_INET6, address.c_str(), &(addr.sin6_addr));
  set_port_addr(port, address);

  if (bind_and_listen() == false)
    return false;

  ipv4_ = false;
  ipv6_ = true;
  uds_  = false;

  return true;
}

bool
Acceptor::listen_uds(const std::string &path,
                     const bool        &reuse,
                     const int         &backlog)
{
  if (path.length() >= sizeof(sockaddr_un::sun_path))
    return false;

  if (reuse == true)
    ::remove(path.c_str());

  backlog_ = backlog;
  if (set_socketopt(AF_UNIX) == false)
    return false;

  struct sockaddr_un &addr = *(reinterpret_cast<struct sockaddr_un *>(&addr_));

  addr.sun_family = AF_UNIX;
  strcpy(addr.sun_path, path.c_str());
  set_port_addr(0, path);

  if (bind_and_listen() == false)
    return false;

  ipv4_ = false;
  ipv6_ = false;
  uds_  = true;

  return true;
}


bool
Acceptor::set_socketopt(const int &domain, const bool &ipv6only)
{
  err_code_ = 0;

  if (io_handle_ != INVALID_IO_HANDLE)
    ::close(io_handle_);

  io_handle_ = ::socket(domain, SOCK_STREAM, 0); /* do some error checking! */

  int enable = 1;
  if (setsockopt(io_handle_, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) < 0)
  {
    err_code_ = errno;
    reactor_trace << strerror(errno) << ":" << io_handle_ << std::endl;
    return false;
  }

  struct linger solinger = { 1, 0 };
  if (setsockopt(io_handle_, SOL_SOCKET, SO_LINGER, &solinger, sizeof(struct linger)) == -1)
  {
    err_code_ = errno;
    reactor_trace << strerror(errno) << ":" << io_handle_ << std::endl;
    return false;
  }

  if (ipv6only == false)
    enable = 0;

  // only ipv6_
  if (setsockopt(io_handle_, IPPROTO_IPV6, IPV6_V6ONLY, (char *)&enable, sizeof(enable)) < 0)
  {
    err_code_ = errno;
    reactor_trace << strerror(errno) << ":" << io_handle_ << std::endl;
    return false;
  }

  return true;
}

static thread_local struct pollfd fds[1];
static thread_local bool pollfd_init = false;

io_handle_t
Acceptor::accept(struct ::sockaddr_storage &client_addr, const int32_t &timeout_msec)
{
  if (pollfd_init == false)
  {
    fds[0].fd     = io_handle_;
    fds[0].events = POLLIN | POLLPRI | POLLERR | POLLHUP;
    pollfd_init   = true;
  }

  std::chrono::steady_clock::time_point timeout_point =
      std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_msec);

  int32_t remain_timeout_msec = timeout_msec;
  int32_t poll_result = 0;

  while (true)
  {
    poll_result = poll(fds, 1, remain_timeout_msec);

    if (poll_result <= 0)
      return poll_result;

    // when shutdown
    if (!(fds[0].revents & (POLLIN | POLLPRI)))
      break;

    static socklen_t sin_size = sizeof(client_addr);
    io_handle_t new_io_handle = ::accept(io_handle_, (sockaddr *)&client_addr, &sin_size);

    if (new_io_handle < 0 && errno != EWOULDBLOCK)
    {
      char buffer[100];
      reactor_trace << io_handle_ << errno << strerror_r(errno, buffer, sizeof(buffer));
      continue;
    }

    if (new_io_handle < 0 && errno == EWOULDBLOCK)
    {
      if (timeout_msec >= 0)
      {
        remain_timeout_msec =
            std::chrono::duration_cast<std::chrono::milliseconds>
        (timeout_point - std::chrono::steady_clock::now()).count();

        if (remain_timeout_msec <= 0)
          return 0;
      }
      else
      {
        remain_timeout_msec = -1;
      }

      continue;
    }

    err_code_ = 0;
    static int enable = 1;
    if (setsockopt(new_io_handle, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) < 0)
    {
      err_code_ = errno;
      reactor_trace << strerror(errno) << ":" << io_handle_ << std::endl;
      return -1;
    }

    if (new_client_to_nonblocking_ == true)
    {
      int io_flag = fcntl(new_io_handle, F_GETFL, 0);
      fcntl(new_io_handle, F_SETFL, io_flag | O_NONBLOCK);
    }

    return new_io_handle;
  }

  ::close(io_handle_);
  return -1;
}

