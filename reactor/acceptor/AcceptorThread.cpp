/*
 * AcceptorThread.cpp
 *
 *  Created on: 2020. 2. 11.
 *      Author: tys
 */

#include "AcceptorThread.h"
#include <reactor/EventHandlerAttr.h>
#include <reactor/trace.h>

// tcp nodelay
#include <linux/socket.h>
#include <netinet/tcp.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

namespace reactor
{

void
AcceptorThread::run()
{
  io_handle_t               client_io_handle  = INVALID_IO_HANDLE;
  struct ::sockaddr_storage client_addr;

  Reactor       *reactor        = nullptr;
  size_t        reactors_index  = 0;
  EventHandler  *event_handler  = nullptr;

  if (acceptor_thread_handler_factory_ != nullptr)
  {
    acceptor_thread_handler_ = acceptor_thread_handler_factory_->create();
    if (acceptor_thread_handler_ != nullptr)
      acceptor_thread_handler_->handle_registered();
  }

  {
    std::unique_lock<std::mutex> lock(run_cond_lock_);
    is_run_ = true;
    run_cond_.notify_all();
  }

  while (true)
  {
    int32_t min_timeout = -1;
    if (acceptor_thread_handler_ != nullptr)
    {
      min_timeout = acceptor_thread_handler_->get_msec_remaining_until_timeout();
      if (min_timeout == 0)
      {
        acceptor_thread_handler_->handle_timeout();
        while ((min_timeout = acceptor_thread_handler_->get_msec_remaining_until_timeout()) == 0);
      }
    }

    client_io_handle = acceptor_.accept(client_addr, min_timeout);

    // timeout
    if (client_io_handle == 0)
      continue;

    if (client_io_handle < 0)
      break;

    static int enable = 1;
    setsockopt(client_io_handle, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
    setsockopt(client_io_handle, IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(int));

    reactor = reactors_.get_reactor();

    event_handler = handler_factory_.create(client_io_handle, client_addr);
    if (event_handler == nullptr)
      continue;

    EventHandlerAttr::io_handle(event_handler) = client_io_handle;
    if (on_created_handler(event_handler, *reactor, reactors_, reactors_index) == false)
    {
      ::close(client_io_handle);
      continue;
    }

    reactor->register_event_handler(event_handler, client_io_handle);
  }

  if (acceptor_thread_handler_ != nullptr)
    acceptor_thread_handler_->handle_shutdown();
}

}
