#include "TCPSessionHandler.h"
#include "TCPEventHandler.h"
#include <sys/un.h>

using namespace reactor;

const io_handle_t &
TCPSessionHandler::io_handle() const
{
  return event_handler_->io_handle();
}

const Acceptor &
TCPSessionHandler::acceptor()
{
  return event_handler_->acceptor();
}

size_t
TCPSessionHandler::handler_count() const
{
  return event_handler_->reactors().handler_count();
}

bool
TCPSessionHandler::send(const int32_t &id, const uint8_t *data, const size_t &size)
{
  return event_handler_->send(id, data, size);
}

bool
TCPSessionHandler::send(const int32_t &id, const std::string &data)
{
  return event_handler_->send(id, (const uint8_t *)data.data(), data.size());
}

//int
//TCPSessionHandler::direct_send(const uint8_t *data, const size_t &size)
//{
//  return event_handler_->direct_send(data, size);
//}
//
//int
//TCPSessionHandler::direct_send(const std::string &data)
//{
//  return event_handler_->direct_send(data.data(), data.size());
//}

bool
TCPSessionHandler::set_timeout(const uint32_t &msec, const int64_t &key)
{
  int32_t min_msec = timer_.register_timeout(msec, key);
  //  reactor_trace << key << min_msec;
  if (min_msec < 0)
    return false;

  return event_handler_->reactor()->set_timeout(event_handler_.get(), min_msec);
}

bool
TCPSessionHandler::unset_timeout(const int64_t &key)
{
  if (timer_.remove_timeout(key) == false)
    return false;

  if (timer_.size() == 0)
    return event_handler_->reactor()->unset_timeout(event_handler_.get());

  int32_t min_msec = timer_.get_min_timeout_milliseconds();
  if (min_msec < 0)
    return true;

  event_handler_->reactor()->set_timeout(event_handler_.get(), min_msec);
  return true;
}

void
TCPSessionHandler::handle_timeout()
{
  int32_t min_msec = 0;
  while ((min_msec = timer_.get_min_timeout_milliseconds()) == 0)
  {
    std::deque<std::unordered_set<int64_t>> timeouts = timer_.extract_timeout_objects();
    for (const auto &keys : timeouts)
      for (const auto &key : keys)
        this->handle_timeout(key);
  }

  //  reactor_trace << min_msec;
  if (min_msec < 0)
    return;

  event_handler_->reactor()->set_timeout(event_handler_.get(), min_msec);
}

bool
TCPSessionHandler::set_output_event()
{
  return event_handler_->set_output_event();
}

bool
TCPSessionHandler::close()
{
  return event_handler_->close();
}

void
TCPSessionHandler::set_socket_address(const struct sockaddr_storage &addr)
{
  addr_ = addr;
  char addr_str[1024] = { 0, };

  ipv6_ = false;
  ipv4_ = false;
  uds_  = false;

  if (addr.ss_family == AF_INET)
  {
    ipv4_ = true;
    ::inet_ntop(addr.ss_family,
                &((struct sockaddr_in *)&addr)->sin_addr ,
                addr_str, sizeof(addr_str));
    peer_addr_ = addr_str;
    peer_port_ = htons(((struct sockaddr_in *)&addr)->sin_port);
    return;
  }

  if (addr.ss_family == AF_INET6)
  {
    ipv6_ = true;
    ::inet_ntop(addr.ss_family,
                &((struct sockaddr_in6 *)&addr)->sin6_addr ,
                addr_str, sizeof(addr_str));
    peer_addr_ = addr_str;
    if (peer_addr_.length() > 7)
      if (peer_addr_.substr(0, 7).compare("::ffff:") == 0)
        peer_addr_ = peer_addr_.substr(7);
    peer_port_ = htons(((struct sockaddr_in6 *)&addr)->sin6_port);
    return;
  }

  uds_ = true;
  peer_addr_ = (const char *)((const struct sockaddr_un *)&addr)->sun_path;
  peer_port_ = 0;
}
