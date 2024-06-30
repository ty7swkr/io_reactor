/*
 * EventHandler.h
 *
 *  Created on: 2020. 1. 31.
 *      Author: tys
 */

#ifndef IO_REACTOR_REACTOR_EVENTHANDLER_H_
#define IO_REACTOR_REACTOR_EVENTHANDLER_H_

#include <reactor/DefinedType.h>
#include <string>

namespace reactor
{

class Reactor;

class EventHandler
{
public:
  EventHandler() {}
  virtual ~EventHandler() {}

protected:
  virtual void handle_registered() = 0;

  virtual void handle_removed   () = 0;

  virtual void handle_input     () = 0;

  virtual void handle_output    () = 0;

  virtual void handle_close     () = 0;

  virtual void handle_timeout   () = 0;

  virtual void handle_error     (const int &error_no = 0, const std::string &error_str = "") = 0;

  virtual void handle_shutdown  () = 0;

protected:
  io_handle_t io_handle_  = INVALID_IO_HANDLE;
  Reactor     *reactor_   = nullptr;

protected:
  friend class Reactor;
  friend class EventHandlerAttr;
};

}

#endif /* reactor_EventHandler_h */
