/*
 * TCPSessionHandler.h
 *
 *  Created on: 2023. 9. 16.
 *      Author: tys
 */

#ifndef IO_REACTOR_REACTOR_ENHANCED_TCPEVENTHANDLER_H_
#define IO_REACTOR_REACTOR_ENHANCED_TCPEVENTHANDLER_H_

#include <reactor/EventHandler.h>
#include <reactor/acceptor/Acceptor.h>
#include <reactor/Reactors.h>

#include <vector>
#include <string>
#include <memory>
#include <arpa/inet.h>

namespace reactor
{

using Bytes = std::vector<uint8_t>;

class TCPSessionHandler;

class TCPEventHandler : public EventHandler
{
public:
  TCPEventHandler() = delete;
  static TCPEventHandler *create(TCPSessionHandler       *session,
                                 const sockaddr_storage  &client_addr,
                                 Acceptor                &acceptor,
                                 Reactors                &reactors)
  { return new TCPEventHandler(session, client_addr, acceptor, reactors); }
  virtual ~TCPEventHandler();

  bool send   (const int32_t &stream_id, const std::string &data);
  bool send   (const int32_t &stream_id, const void *data, const size_t &size);
  bool close  ();
  bool set_output_event();

//  int direct_send(const uint8_t *data, const size_t &size);
//  int direct_send(const void    *data, const size_t &size);

  Acceptor          &acceptor () { return acceptor_;}
  Reactor           *reactor  () { return reactor_; }
  Reactors          &reactors () { return reactors_;}
  sockaddr_storage  addr      () { return addr_;    }
  const io_handle_t &io_handle() const { return this->io_handle_; }

protected:
  TCPEventHandler(TCPSessionHandler       *session,
                  const sockaddr_storage  &client_addr,
                  Acceptor                &acceptor,
                  Reactors                &reactors);

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
  std::atomic<bool> set_output_event_;

protected:
  std::shared_ptr<TCPEventHandler> shared_from_this_;
  TCPSessionHandler *session_ = nullptr;

protected:
  struct send_buffer_info_t
  {
    send_buffer_info_t() {}
    send_buffer_info_t(const int &stream_id, const size_t &begin, const size_t &length)
    : stream_id(stream_id), begin(begin), length(length), sent_remain(length) {}
    int32_t stream_id          = -1;
    size_t  begin       = 0;
    size_t  length      = 0;
    size_t  sent_remain = 0;
  };

  Bytes send_buffer_;
  std::deque<send_buffer_info_t> send_buffer_infos_;

protected:
  std::mutex  send_buffers_prepare_lock_;
  Bytes       send_buffers_prepare_;
  std::deque<send_buffer_info_t> send_buffers_prepare_info_;

  bool has_buffer_to_send()
  {
    if (send_buffer_.size() > 0)
      return true;

    std::lock_guard<std::mutex> guard(send_buffers_prepare_lock_);
    if (send_buffers_prepare_.size() == 0)
      return false;

    send_buffer_      .swap(send_buffers_prepare_);
    send_buffer_infos_.swap(send_buffers_prepare_info_);

    return true;
  }

protected:
  sockaddr_storage  addr_;
  bool              close_ = false;

protected:
  Acceptor  &acceptor_;
  Reactors  &reactors_;
};

inline
TCPEventHandler::~TCPEventHandler()
{
}

//inline int
//TCPEventHandler::direct_send(const uint8_t *data, const size_t &size)
//{
//  int sent_size = 0;
//  while (sent_size < (int)size)
//  {
//    int write_size = ssl_socket_.write(data, size);
//    if (write_size <= 0)
//      return write_size;
//
//    sent_size += write_size;
//  }
//
//  return sent_size;
//}
//
//inline int
//TCPEventHandler::direct_send(const void *data, const size_t &size)
//{
//  return this->direct_send((uint8_t *)data, size);
//}

inline bool
TCPEventHandler::set_output_event()
{
  if (io_handle_ == INVALID_IO_HANDLE || reactor_ == nullptr || close_ == true)
    return false;

  bool comparand = false;
  bool exchanged = set_output_event_.compare_exchange_strong(comparand, true);
  if (exchanged == false)
    return false;

  return reactor_->register_writable(this);
}

inline bool
TCPEventHandler::send(const int32_t &stream_id, const std::string &str)
{
  return this->send(stream_id, str.data(), str.size());
}

inline bool
TCPEventHandler::send(const int32_t &stream_id, const void *data, const size_t &size)
{
  if (io_handle_ == INVALID_IO_HANDLE || reactor_ == nullptr || close_ == true)
    return false;

  {
    std::lock_guard<std::mutex> guard(send_buffers_prepare_lock_);
    size_t begin = send_buffers_prepare_.size();
    send_buffers_prepare_     .insert(send_buffers_prepare_.end(), (uint8_t *)data, (uint8_t *)data + size);
    send_buffers_prepare_info_.emplace_back(stream_id, begin, size);
  }

  reactor_->register_writable(this);

  return true;
}

inline bool
TCPEventHandler::close()
{
  if (close_ == true || io_handle_ == INVALID_IO_HANDLE)
    return false;

  close_ = true;

  ::shutdown(io_handle_, SHUT_RD);
  return true;
}

}

#endif /* IO_REACTOR_REACTOR_ENHANCED_TCPEVENTHANDLER_H_ */


