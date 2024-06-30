#include "Reactor.h"
#include <assert.h>

namespace reactor
{

void
Reactor::dispatch_reactor_event(const IoDemuxer::EventData &event,
                                bool &is_stop_event)
{
  if (event.recv_event < IoDemuxer::EVENT_USER)
    return;

  if (event.recv_event == EVENT_STOP)
  {
    is_stop_event = true;
    return;
  }

  std::unordered_map<io_handle_t, EventHandler *>::iterator it =
      handlers_.find(event.io_handle);

  if (it == handlers_.end())
    return;

  switch (event.recv_event)
  {
    case EVENT_TIMEOUT_ADD:
    {
      uint32_t timeout_msec = (uint64_t)(event.data);

      timer_.register_timeout(timeout_msec, it->second);
      return;
    }
    case EVENT_TIMEOUT_DEL:
    {
      timer_.remove_timeout(it->second);
      return;
    }
  }
}

void
Reactor::dispatch_demuxer_event_io(const IoDemuxer::EventData &event)
{
  if (event.recv_event > IoDemuxer::EVENT_CLOSE)
    return;

  std::unordered_map<io_handle_t, EventHandler *>::iterator it =
      handlers_.find(event.io_handle);

  if (it == handlers_.end())
  {
    demuxer_.remove_all_events(event.io_handle, nullptr, false);
    return;
  }

  EventHandler &handler = *(it->second);

  switch (event.recv_event)
  {
    case IoDemuxer::EVENT_READ:
    {
      handler.handle_input();
      return;
    }
    case IoDemuxer::EVENT_WRITE:
    {
      demuxer_.remove_write_event(handler.io_handle_, nullptr, false);
      handler.handle_output();
      return;
    }
    case IoDemuxer::EVENT_ERROR:
    {
      if (errno == 0)
      {
        handler.handle_error(EPIPE, std::strerror(EPIPE));
        return;
      }
      handler.handle_error(errno, std::strerror(errno));
      return;
    }
    case IoDemuxer::EVENT_CLOSE:
    {
      if (handle_close_call_.count(handler.io_handle_) > 0)
        return;

      handle_close_call_.insert(handler.io_handle_);
      handler.handle_close();
      return;
    }
  }
}

void
Reactor::dispatch_demuxer_event_result(const IoDemuxer::EventData &event)
{
  if (event.recv_event <= IoDemuxer::EVENT_CLOSE)
    return;

  EventHandler *handler = event.data;
  if (handler == nullptr)
    return;

  switch (event.recv_event)
  {
    case IoDemuxer::EVENT_REGISTER_READ:
    {
      ++handler_count_;
      handlers_.emplace(std::make_pair(event.io_handle, handler));

      handler->reactor_ = this;
      handler->handle_registered();

      if (handler != reactor_handler_ && reactor_handler_ != nullptr)
        reactor_handler_->reactor_handle_registered_handler(handler);

      return;
    }
    case IoDemuxer::EVENT_REMOVE_ALL:
    {
      if (handlers_.count(event.io_handle) == 0)
        return;

      timer_.remove_timeout(handler);
      handlers_.erase(event.io_handle);
      handle_close_call_.erase(event.io_handle);

      --handler_count_;
      // User can delete in handle_removed.
      handler->handle_removed();
      // handler->reactor_ = nullptr;

      if (handler != reactor_handler_ && reactor_handler_ != nullptr)
        reactor_handler_->reactor_handle_removed_handler(handler);

      return;
    }
  }
}

void
Reactor::run()
{
  stop_ = false;

  std::vector<IoDemuxer::EventData> events;
  events.reserve(10000);

  run_reactor_handler();

  while (true)
  {
    int32_t timeout = 0;
    while ((timeout = timer_.get_min_timeout_milliseconds()) == 0)
      run_timeout_handler();

    events.clear();
    demuxer_.wait(events, timeout);

    // timeout
    if (events.size() == 0)
    {
      run_timeout_handler();
      continue;
    }

    if (dispatch(events) == true)
      break;
  }

  run_shutdown();

  stop_ = true;
}
}

