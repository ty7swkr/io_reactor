/*
 * Https1Client.h
 *
 *  Created on: 2023. 2. 19.
 *      Author: tys
 */

#ifndef IO_REACTOR_HTTPS_CLIENT_Https1Client_H_
#define IO_REACTOR_HTTPS_CLIENT_Https1Client_H_

#include <http1_protocol/Http1Request.h>
#include <http1_protocol/Http1Response.h>
#include <tcp_async_client/TcpAsyncSSLClient.h>
#include <reactor/ObjectsTimer.h>

using namespace reactor;
using namespace https_reactor;

class Https1Client : protected TcpAsyncSSLClient
{
public:
  Https1Client(Reactor &reactor, const size_t &recv_buffer_size = 10240)
  : TcpAsyncSSLClient(reactor, recv_buffer_size), websocket_(false) {}
  virtual ~Https1Client() {}

  bool          send                  (const Http1Request &request);
  bool          send                  (const int32_t      &stream_id,
                                       const WebSocket    &request);
  bool          connect               (const std::string  &host,
                                       const uint16_t     &port,
                                       const int32_t      &timeout_msec = 5000) override;

protected:
  bool          set_timeout           (const uint32_t &msec, const uint64_t &timeout_id = 0);
  bool          unset_timeout         (const uint64_t &id);

protected:
  virtual void  on_connect            () = 0;
  virtual void  on_connect_error      (const int &err_no, const std::string &err_str) = 0;
  virtual void  on_connect_timeout    (const int &err_no, const std::string &err_str) = 0;
  virtual void  on_disconnect         () = 0; // When detached in the reactor, on_disconnect is called.
  virtual void  on_accept             (SSL *ssl) { (void)ssl; }

  // http1.1
  virtual void  on_response           (const Http1Response &response) = 0;
  virtual void  on_sent               (const Http1Request  &request ) = 0;
  virtual void  on_sent_error         (const int &err_no, const std::string &err_str,
                                       const Http1Request  &request) = 0;
  virtual void  on_error_http11       (const int &err_no, const std::string &err_str) = 0;

  // websocket
  virtual void  on_recv               (const std::deque<WebSocket> &responses) { (void)responses; }
  virtual void  on_sent               (const int32_t &stream_id, const WebSocket   &request) { (void)stream_id; (void)request; }
  virtual void  on_sent_error         (const int     &err_no,    const std::string &err_str,
                                       const int32_t &stream_id, const WebSocket   &request) { (void)err_no; (void)err_str; (void)stream_id; (void)request; }

  virtual void  on_error_websocket    (const int &err_no, const std::string &err_str) { (void)err_no; (void)err_str; }

  virtual void  on_timeout            (const uint64_t &timeout_id) = 0;
  virtual void  on_shutdown           () = 0;

protected:
  bool          is_websocket_mode     () const { return websocket_.load(); }
  bool          is_websocket_upgrade  (const Http1Response &reponse) const;
  void          upgrade_websocket     () { websocket_ = true;  }
  void          downgrade_websocket   () { websocket_ = false; }
  int32_t       next_stream_id        ();

private:
  void          handle_connect        ();
  void          handle_recv           (const std::vector<uint8_t> &data) override;
  void          handle_recv_http1     (const std::vector<uint8_t> &data);
  void          handle_recv_ws        (const std::vector<uint8_t> &data);
  void          handle_sent           (const int32_t &id, const uint8_t *data, const size_t &size) override;
  void          handle_sent_error     (const int &err_no, const std::string &err_str,
                                       const int32_t &id, const uint8_t *data, const size_t &size) override;
  void          handle_error          (const int &err_no, const std::string &err_str) override;
  void          handle_connect_error  (const int &err_no, const std::string &err_str) override { on_connect_error  (err_no, err_str); }
  void          handle_connect_timeout(const int &err_no, const std::string &err_str) override { on_connect_timeout(err_no, err_str); }
  void          handle_disconnect     ()         override { on_disconnect();}
  void          handle_accept         (SSL *ssl) override { on_accept(ssl); }
  void          handle_timeout        ()         override;
  void          handle_shutdown       ()         override { on_shutdown();  }

private:
  std::atomic<bool> websocket_;

private:
  std::mutex  stream_id_lock_;
  int32_t     stream_id_ = 0;

private: // http1
  std::mutex  requests_h1_lock_;
  std::map<int32_t, Http1Request> requests_h1_;
  std::vector<uint8_t> buffer_h1_;

private: // websocket
  std::mutex  requests_ws_lock_;
  std::map<int32_t, WebSocket> requests_ws_;
  std::vector<uint8_t> buffer_ws_;

private:
  ObjectsTimer<int64_t> timer_;
};

inline bool
Https1Client::set_timeout(const uint32_t &msec, const uint64_t &id)
{
  int32_t min_msec = timer_.register_timeout(msec, id);

  if (min_msec < 0)
    return false;

  TcpAsyncSSLClient::set_timeout(min_msec);
  return true;
}

inline bool
Https1Client::unset_timeout(const uint64_t &id)
{
  return timer_.remove_timeout(id);
}

inline void
Https1Client::handle_timeout()
{
  int32_t min_msec = 0;

  while ((min_msec = timer_.get_min_timeout_milliseconds()) == 0)
  {
    std::deque<std::unordered_set<int64_t>> timeouts = timer_.extract_timeout_objects();
    for (const auto &keys : timeouts)
      for (const auto &key : keys)
        on_timeout(key);
  }

  if (min_msec <= 0)
    return;

  TcpAsyncSSLClient::set_timeout(min_msec);
}

inline void
Https1Client::handle_connect()
{
  {
    std::lock_guard<std::mutex> guard(requests_h1_lock_);
    requests_h1_.clear();
    buffer_h1_.clear();
  }
  {
    std::lock_guard<std::mutex> guard(requests_ws_lock_);
    requests_ws_.clear();
    buffer_ws_.clear();
  }

  this->on_connect();
}

inline bool
Https1Client::is_websocket_upgrade(const Http1Response &response) const
{
  if (response.status() != 101)
    return false;

  if (response.header.contains("sec-webSocket-key") == false)
    return false;

  if (HttpHeader::to_lower(response.header.get<std::string>("upgrade")) != "websocket")
    return false;

  if (HttpHeader::to_lower(response.header.get<std::string>("connection")) != "upgrade")
    return false;

  return true;
}

inline int32_t
Https1Client::next_stream_id()
{
  std::lock_guard<std::mutex> guard(stream_id_lock_);
  return ++stream_id_;
}

inline bool
Https1Client::send(const Http1Request &request)
{
  if (websocket_.load() == true) return false;

  std::lock_guard<std::mutex> guard(requests_h1_lock_);
  requests_h1_.insert(std::make_pair(request.stream_id, request));
  return TcpAsyncSSLClient::send(request.stream_id, request.packet());
}

inline bool
Https1Client::connect(const std::string &host,
                      const uint16_t    &port,
                      const int32_t     &timeout_msec)
{
  websocket_ = false;
  {
    std::lock_guard<std::mutex> guard(stream_id_lock_);
    stream_id_ = 0;
  }
  {
    std::lock_guard<std::mutex> guard(requests_h1_lock_);
    requests_h1_.clear();
    buffer_h1_.clear();
  }
  {
    std::lock_guard<std::mutex> guard(requests_ws_lock_);
    requests_ws_.clear();
    buffer_ws_.clear();
  }

  return TcpAsyncSSLClient::connect(host, port, timeout_msec);
}

inline bool
Https1Client::send(const int32_t &stream_id, const WebSocket &request)
{
  if (websocket_.load() == false) return false;

  std::lock_guard<std::mutex> guard(requests_ws_lock_);
  requests_ws_.insert(std::make_pair(stream_id, request));
  return TcpAsyncSSLClient::send(stream_id, request.packet().data(), request.packet().size());
}

inline void
Https1Client::handle_recv(const std::vector<uint8_t> &data)
{
  if (websocket_.load() == false)
  {
    handle_recv_http1(data);
    return;
  }

  handle_recv_ws(data);
}

inline void
Https1Client::handle_recv_http1(const std::vector<uint8_t> &data)
{
  std::optional<Http1Response> h1res;

  buffer_h1_.insert(buffer_h1_.end(), data.begin(), data.end());
  try
  {
    h1res = Http1Response::parse(std::string_view(
        reinterpret_cast<const char *>(buffer_h1_.data()), buffer_h1_.size()));

    if (h1res == std::nullopt)
      return;

    buffer_h1_.clear();
  }
  catch (std::runtime_error &e)
  {
    on_error_http11(EINVAL, e.what());
    return;
  }

  on_response(h1res.value());
}

inline void
Https1Client::handle_recv_ws(const std::vector<uint8_t> &data)
{
  buffer_ws_.insert(buffer_ws_.end(), data.begin(), data.end());
  std::deque<WebSocket> requests;
  while (true)
  {
    try
    {
      WebSocket request =
          WebSocket::parse(buffer_ws_.data(), buffer_ws_.size());

      buffer_ws_.erase(buffer_ws_.begin(), buffer_ws_.begin()+request.size());
      requests.emplace_back(request);
    }
    catch (const WebSocketException &e)
    {
      // incomplete
      if (e.code != WebSocketException::WRONG_OPCODE &&
          e.code != WebSocketException::UNKNOWN)
        break;

      // error
      if (requests.size() > 0)
        this->on_recv(requests);

      on_error_websocket(EINVAL, e.what());
      return;
    }
  }

  if (requests.size() > 0)
    this->on_recv(requests);
}

inline void
Https1Client::handle_sent(const int32_t &id, const uint8_t *data, const size_t &size)
{
  (void)data; (void)size;
  if (websocket_.load() == false)
  {
    Http1Request request;
    {
      std::lock_guard<std::mutex> guard(requests_h1_lock_);
      auto it = requests_h1_.find(id);
      if (it == requests_h1_.end())
        return;

      request = std::move(it->second);
      requests_h1_.erase(it);
    }

    on_sent(request);
    return;
  }

  WebSocket request;
  int32_t   stream_id;
  {
    std::lock_guard<std::mutex> guard(requests_ws_lock_);
    auto it = requests_ws_.find(id);
    if (it == requests_ws_.end())
      return;

    stream_id = it->first;
    request   = std::move(it->second);
    requests_ws_.erase(it);
  }

  on_sent(stream_id, request);
}

inline void
Https1Client::handle_sent_error(const int &err_no, const std::string &err_str,
                                const int32_t &id, const uint8_t *data, const size_t &size)
{
  (void)data; (void)size;
  if (websocket_.load() == false)
  {
    Http1Request request;
    {
      std::lock_guard<std::mutex> guard(requests_h1_lock_);
      auto it = requests_h1_.find(id);
      if (it == requests_h1_.end())
        return;

      request = std::move(it->second);
      requests_h1_.erase(it);
    }

    on_sent_error(err_no, err_str, request);
    return;
  }

  WebSocket request;
  int32_t   stream_id;
  {
    std::lock_guard<std::mutex> guard(requests_ws_lock_);
    auto it = requests_ws_.find(id);
    if (it == requests_ws_.end())
      return;

    stream_id = it->first;
    request   = std::move(it->second);
    requests_ws_.erase(it);
  }

  on_sent_error(err_no, err_str, stream_id, request);
}

inline void
Https1Client::handle_error(const int &err_no, const std::string &err_str)
{
  if (websocket_.load() == false)
  {
    on_error_http11(err_no, err_str);
    return;
  }

  on_error_websocket(err_no, err_str);
}


#endif /* IO_REACTOR_HTTPS_CLIENT_Https1Client_H_ */























