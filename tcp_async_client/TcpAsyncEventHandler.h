#ifndef IO_REACTOR_TCP_ASYNC_CLIENT_TCPASYNCEVENTHANDLER_H_
#define IO_REACTOR_TCP_ASYNC_CLIENT_TCPASYNCEVENTHANDLER_H_

#include <reactor/Reactors.h>
#include <reactor/EventHandler.h>
#include <reactor/trace.h>

#include <functional>

#include <string.h>
#include <arpa/inet.h>
#include <stdlib.h>

namespace reactor
{

class TcpAsyncEventHandler : public EventHandler
{
public:
  TcpAsyncEventHandler() { memset(local_addr_, 0x00, sizeof(local_addr_)); }
  virtual ~TcpAsyncEventHandler() {}

  virtual bool  prepare_socket  (const std::string  &host,
                                 const uint16_t     &port);

  virtual bool  connect         (const int32_t      &timeout_msec);

  virtual bool  disconnect      ();
  bool          is_connect      () const { return is_connect_;}
  io_handle_t   io_handle       () const { return io_handle_; }

  uint16_t      local_port      () const { return local_port_;}
  std::string   local_addr      () const { return local_addr_;}
  std::string   local_addr_port () const { return local_addr() + " " + std::to_string(local_port()); }

  void          wait_registered () const;

public:
  std::function<void()> on_connect    = nullptr;
  std::function<void()> on_disconnect = nullptr;
  std::function<void()> on_timeout    = nullptr;
  std::function<void()> on_input      = nullptr;
  std::function<void()> on_output     = nullptr;
  std::function<void()> on_shutdown   = nullptr;

  std::function<void(const int &, const std::string &)> on_connect_error    = nullptr;
  std::function<void(const int &, const std::string &)> on_connect_timeout  = nullptr;
  std::function<void(const int &, const std::string &)> on_error            = nullptr;

protected:
  void handle_connect   ();
  void handle_disconnect();

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
  bool set_addr_ipv4(struct sockaddr_storage &peer_addr,
                     const std::string &host, const uint16_t &port);

  bool set_addr_ipv6(struct sockaddr_storage &peer_addr,
                     const std::string &host, const uint16_t &port);

protected:
  mutable std::mutex              registered_mutex_;
  mutable std::condition_variable registered_contition_;
  bool                            registered_ = false;

protected:
  int32_t   connect_timeout_          = -1;
  bool      is_connect_               = false;
  bool      call_on_connect_timeout_  = false;
  int       io_handle_fcntl_          = 0;
  uint16_t  local_port_               = 0;
  char      local_addr_[INET6_ADDRSTRLEN+1];
  bool      ipv6_                     = false;

protected:
  struct sockaddr_storage server_addr_;
};

inline void
TcpAsyncEventHandler::wait_registered() const
{
  std::unique_lock<std::mutex> lock(registered_mutex_);
  if (registered_ == true)
    return;

  registered_contition_.wait(lock);
}

inline bool
TcpAsyncEventHandler::disconnect()
{
  if (is_connect_ == false)
    return false;

  if (reactor_ == nullptr)
    return true;

  reactor_->remove_event_handler(this);

  return true;
}

inline void
TcpAsyncEventHandler::handle_connect()
{
  if (ipv6_ == false)
  {
    sockaddr_storage  addr;
    socklen_t         addr_size = sizeof(struct sockaddr_in);
    getsockname(io_handle_, (struct sockaddr *)&addr, &addr_size);
    local_port_ = ntohs(((struct sockaddr_in *)&addr)->sin_port);
    inet_ntop(AF_INET, &((struct sockaddr_in *)&addr)->sin_addr, local_addr_, sizeof(local_addr_));
  }
  else
  {
    sockaddr_storage  addr;
    socklen_t         addr_size = sizeof(struct sockaddr_in);
    getsockname(io_handle_, (struct sockaddr *)&addr, &addr_size);
    local_port_  = ntohs(((struct sockaddr_in6 *)&addr)->sin6_port);
    inet_ntop(AF_INET6, &((struct sockaddr_in6 *)&addr)->sin6_addr, local_addr_, sizeof(local_addr_));
  }

  if (on_connect == nullptr)
    return;

  on_connect();
}

inline void
TcpAsyncEventHandler::handle_disconnect()
{
  if (on_disconnect == nullptr)
    return;

  on_disconnect();
}

// Called when registered in Reactor.
// There is no need to register with Reactor as it is already registered with Reactor.
inline void
TcpAsyncEventHandler::handle_registered()
{
  if (is_connect_ == false)
    reactor_->set_timeout(this, connect_timeout_);

  reactor_->register_writable(this); // connect시 output으로 리턴함.

  {
    std::unique_lock<std::mutex> lock(registered_mutex_);
    registered_ = true;
  }

  // 대기 중인 스레드에게 시그널 전달
  registered_contition_.notify_all();
}

// Called when removed from Reactor.
inline void
TcpAsyncEventHandler::handle_removed()
{
  ::close(io_handle_);
  io_handle_  = INVALID_IO_HANDLE;
  registered_ = false;

  if (is_connect_ == false)
  {
    if (call_on_connect_timeout_ == true)
    {
      call_on_connect_timeout_ = false;
      if (on_connect_timeout != nullptr)
        on_connect_timeout(errno, std::strerror(errno));

      return;
    }

    if (on_connect_error != nullptr)
      on_connect_error(errno, std::strerror(errno));

    return;
  }

  is_connect_ = false;
  this->handle_disconnect();
}

// Called when data arrives.
inline void
TcpAsyncEventHandler::handle_input()
{
  if (on_input == nullptr)
    return;

  on_input();
}

// Called when data can be written.
inline void
TcpAsyncEventHandler::handle_output()
{
  if (is_connect_ == false)
  {
    reactor_->unset_timeout(this);

    is_connect_ = true;
    handle_connect();
    return;
  }

  if (on_output == nullptr)
    return;

  on_output();
}

// Called by Reactor when the client is disconnected.
// handle_close() is still called until user explicitly
// call close(io_handle_) or shutdown(io_handle_, ...).
inline void
TcpAsyncEventHandler::handle_close()
{
  reactor_->remove_event_handler(this);
}

// When Reactor's set_timeout(msec) is called,
// Reactor calls handle_timeout() after the timeout.
// set_timeout is one-time. To call handle_timeout again,
// you must call Reactor::set_timeout(msec) each time.
inline void
TcpAsyncEventHandler::handle_timeout()
{
  if (is_connect_ == false)
  {
    call_on_connect_timeout_ = true;
    reactor_->remove_event_handler(this);
    return;
  }

  if (on_timeout == nullptr)
    return;

  on_timeout();
}

// Called on error. The error is set to the standard errno.
inline void
TcpAsyncEventHandler::handle_error(const int &error_no, const std::string &error_str)
{
  if (on_error == nullptr || is_connect_ == false)
    return;

  on_error(error_no, error_str);
}

// Called when the reactor is shutting down. The reactor is set to nullptr.
inline void
TcpAsyncEventHandler::handle_shutdown()
{
  if (on_shutdown == nullptr)
    return;

  on_shutdown();
}

}

#endif /* TCPASYNCCLIENTHANDLER_H_ */
