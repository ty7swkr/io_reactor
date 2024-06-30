/*
 * ReactorHandler.h
 *
 *  Created on: 2020. 6. 26.
 *      Author: tys
 */

#ifndef IO_REACTOR_REACTOR_REACTORHANDLER_H_
#define IO_REACTOR_REACTOR_REACTORHANDLER_H_

#include <reactor/EventHandler.h>

#include <string>
#include <sys/socket.h>
#include <unistd.h>

namespace reactor
{

class Reactors;
class Reactor;

class ReactorHandler : public EventHandler
{
public:
  ReactorHandler() {}

protected:
  virtual void reactor_handle_registered() = 0;
  virtual void reactor_handle_timeout   () = 0;
  virtual void reactor_handle_shutdown  () {}
  virtual void reactor_handle_error     (const int         &error_no,
                                         const std::string &error_str)
  { (void)error_no, (void)error_str; }

  virtual void reactor_handle_registered_handler(EventHandler *handler)
  { (void)handler; }

  virtual void reactor_handle_removed_handler   (EventHandler *handler)
  { (void)handler; }

protected:
  bool set_timeout  (const uint32_t &msec);
  void unset_timeout();

private:
  bool init();
  void handle_registered() override;
  void handle_removed   () override;
  void handle_timeout   () override;
  void handle_shutdown  () override;
  void handle_input     () override;
  void handle_output    () override;
  void handle_error     (const int &error_no, const std::string &error_str) override;
  void handle_close     () override;

private:
  uint8_t buffer_[100];
  int     pipe_send_= INVALID_IO_HANDLE;

  friend class Reactors;
  friend class Reactor;
};

inline bool
ReactorHandler::init()
{
  int pipe_fd[2];
  if (pipe(pipe_fd) < 0)
    return false;

  io_handle_  = pipe_fd[0];
  pipe_send_  = pipe_fd[1];

  return true;
}

inline void
ReactorHandler::handle_registered()
{
  this->reactor_handle_registered();
}

inline void
ReactorHandler::handle_removed()
{
}

inline void
ReactorHandler::handle_timeout()
{
  this->reactor_handle_timeout();
}

inline void
ReactorHandler::handle_input()
{
  auto ignore_result = [&](const size_t &)->void {};
  ignore_result(::read(io_handle_, buffer_, sizeof(buffer_)));
}

inline void
ReactorHandler::handle_shutdown()
{
  ::close(io_handle_);
  ::close(pipe_send_);
  this->reactor_handle_shutdown();
}

inline void
ReactorHandler::handle_output()
{
}

inline void
ReactorHandler::handle_close()
{
  ::close(io_handle_);
  ::close(pipe_send_);
}

inline void
ReactorHandler::handle_error(const int &error_no, const std::string &error_str)
{
  this->reactor_handle_error(error_no, error_str);
}

}

#endif /* SRC_HTTP_REACTOR_REACTOR_REACTORHANDLER_H_ */




