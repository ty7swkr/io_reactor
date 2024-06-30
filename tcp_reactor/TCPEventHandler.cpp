#include "TCPEventHandler.h"
#include "TCPSessionHandler.h"
#include <functional>

using namespace reactor;

TCPEventHandler::TCPEventHandler(TCPSessionHandler       *session,
                                 const sockaddr_storage  &client_addr,
                                 Acceptor                &acceptor,
                                 Reactors                &reactors)
: session_(session), addr_(client_addr), acceptor_(acceptor), reactors_(reactors)
{
  shared_from_this_         = std::shared_ptr<TCPEventHandler>(this);
  session_->event_handler_  = shared_from_this_;

  send_buffer_         .reserve(10240);
  send_buffers_prepare_.reserve(10240);

  set_output_event_ = false;
}

void
TCPEventHandler::handle_registered()
{
  send_buffer_.clear();
  send_buffer_.reserve(10240);

  session_->handle_registered();
}

void
TCPEventHandler::handle_removed()
{
  // 시스템으로부터 소켓이 재할당 되지 않도록 reactor에서 제거 된 후 close 한다.
  session_->handle_removed();

  ::close(io_handle_);
  this->shared_from_this_.reset();
}

void
TCPEventHandler::handle_input()
{
  session_->handle_input();
}

void
TCPEventHandler::handle_output()
{
  bool comparand = true;
  bool exchanged = set_output_event_.compare_exchange_strong(comparand, false);
  if (exchanged == true)
    session_->handle_output();

//  while (true)
//  {
  if (has_buffer_to_send() == false)
    return;

  int sent_size =::send(this->io_handle_, send_buffer_.data(), send_buffer_.size(), 0);
  if (sent_size <= 0)
  {
    int err_no = errno;
    if (err_no == EAGAIN)
    {
      reactor_->register_writable(this);
      return;
    }

    char str[256];
    std::string err_str = ::strerror_r(err_no, str, sizeof(str));

    for (const send_buffer_info_t &info : send_buffer_infos_)
      session_->handle_sent_error(err_no, err_str, info.stream_id, send_buffer_.data() + info.begin, info.length);

    ::shutdown(io_handle_, SHUT_RD);
    return;
  }

  struct ScopeExit
  {
    ScopeExit(std::function<void()> exit_func) : exit_func_(exit_func) {}
    ~ScopeExit() { exit_func_(); }
    std::function<void()> exit_func_;
  };

  ScopeExit scope_exit([&]()
  { send_buffer_.erase(send_buffer_.begin(), send_buffer_.begin() + sent_size); });

  ssize_t sent_info_size = sent_size;
  for (auto it = send_buffer_infos_.begin(); it != send_buffer_infos_.end();)
  {
    send_buffer_info_t &info = *it;

    sent_info_size -= info.sent_remain;
    if (sent_info_size < 0)
    {
      info.sent_remain = -sent_info_size;
      break;
    }

    session_->handle_sent(info.stream_id, send_buffer_.data() + info.begin, info.length);
    it = send_buffer_infos_.erase(it);
  }

  // 다른 event기회를 주기위해  while문으로 처리 안하고register_writable를 호출함.
  if (has_buffer_to_send() == true)
    reactor_->register_writable(this);

  return;
}

void
TCPEventHandler::handle_close()
{
  session_->handle_close();
  reactor_->remove_event_handler(this);
}

void
TCPEventHandler::handle_timeout()
{
  session_->handle_timeout();
}

void
TCPEventHandler::handle_error(const int &err_no, const std::string &err_str)
{
  session_->handle_error(err_no, err_str);
  ::shutdown(io_handle_, SHUT_RD);
}

inline void
TCPEventHandler::handle_shutdown()
{
  session_->handle_shutdown();
}
