#include <https1_reactor/Https1Handler.h>

using namespace https_reactor;

int32_t
Https1Handler::next_stream_id()
{
  std::lock_guard<std::mutex> guard(stream_id_lock_);

  if (++stream_id_ < 0)
    stream_id_ = 0;

  return stream_id_;
}

void
Https1Handler::handle_input(const uint8_t *data, const size_t &size)
{
  if (websocket_ == true)
  {
    this->handle_input_ws(data, size);
    return;
  }

  buffer_http1_.insert(buffer_http1_.end(), data, data+size);
  try
  {
    // HTTP 1.1규약상 한번에 하나의 메세지만 온다.
    std::optional<Http1Request> h1req_opt =
        Http1Request::parse(std::string_view(buffer_http1_.data(), buffer_http1_.size()));

    // incomplete
    if (h1req_opt.has_value() == false)
      return;

    buffer_http1_.clear();

    h1req_opt->stream_id = next_stream_id();
    this->handle_request(h1req_opt.value());
  }
  catch (const std::exception &e)
  {
    buffer_http1_.clear();
    this->handle_error(SSL_STATE::READ, EINVAL, e.what());
  }
}

void
Https1Handler::handle_input_ws(const uint8_t *data, const size_t &size)
{
  buffer_websocket_.insert(buffer_websocket_.end(), data, data+size);
  std::deque<WebSocket> requests;
  while (true)
  {
    try
    {
      WebSocket request = WebSocket::parse(buffer_websocket_.data(), buffer_websocket_.size());

      buffer_websocket_.erase(buffer_websocket_.begin(), buffer_websocket_.begin()+request.size());
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
        this->handle_request(requests);

      this->handle_error(SSL_STATE::READ, EINVAL, e.what());

      buffer_websocket_.clear();
      return;
    }
  }

  if (requests.size() > 0)
    this->handle_request(requests);
}

bool
Https1Handler::send(const Http1Response &response)
{
  if (websocket_.load() == true)
  {
    this->handle_error(SSL_STATE::NONE, EPERM,
                       "Https1Handler::send(const Http1Response &response) : Already in the websocket state.");
    return -1;
  }

  {
    std::lock_guard<std::mutex> guard(stream_id_lock_);
    stream_id_ = response.stream_id;
  }
  {
    std::lock_guard<std::mutex> guard(sent_res_http1_lock_);
    sent_res_http1_[response.stream_id] = response;
  }

  return SSLSessionHandler::send(response.stream_id, response.packet());
}

int32_t
Https1Handler::send(const WebSocket &response)
{
  if (websocket_.load() == false)
  {
    this->handle_error(SSL_STATE::NONE, EPERM,
                       "Https1Handler::send(const WebSocketResponse &response) : No websocket state.");
    return -1;
  }

  int32_t stream_id = next_stream_id();
  {
    std::lock_guard<std::mutex> guard(sent_res_ws_lock_);
    sent_res_ws_[stream_id] = response;
  }

  if (SSLSessionHandler::send(stream_id, response.packet().data(), response.packet().size()) == false)
    return -1;

  return stream_id;
}

void
Https1Handler::handle_sent_error(const SSL_STATE     &ssl_state,
                                 const int           &err_no,
                                 const std::string   &err_str,
                                 const int32_t &stream_id, const uint8_t *data, const size_t  &size)
{
  (void)data; (void)size;
  std::function<bool()> http1_send_error = [&]() -> bool
  {
    Http1Response response;
    {
      std::lock_guard<std::mutex> guard(sent_res_http1_lock_);
      auto it = sent_res_http1_.find(stream_id);
      if (it == sent_res_http1_.end())
        return false;

      response = std::move(it->second);
      sent_res_http1_.erase(it);
    }

    this->handle_sent_error(ssl_state, err_no, err_str, response);

    if (response.is_websocket_upgrade() == true)
      websocket_ = true;

    return true;
  };

  if (http1_send_error() == true)
    return;

  WebSocket response;
  {
    std::lock_guard<std::mutex> guard(sent_res_ws_lock_);
    auto it = sent_res_ws_.find(stream_id);
    if (it == sent_res_ws_.end())
      return;

    response = it->second;
    sent_res_ws_.erase(it);
  }

  this->handle_sent_error(ssl_state, err_no, err_str, stream_id, response);
}

void
Https1Handler::handle_sent(const int32_t &stream_id, const uint8_t *data, const size_t &size)
{
  if (handle_sent_http1(stream_id, data, size) == true)
    return;

  WebSocket response;
  {
    std::lock_guard<std::mutex> guard(sent_res_ws_lock_);
    auto it = sent_res_ws_.find(stream_id);
    if (it == sent_res_ws_.end())
      return;

    response = it->second;
    sent_res_ws_.erase(it);
  }

  this->handle_sent(stream_id, response);
}

bool
Https1Handler::handle_sent_http1(const int32_t &stream_id, const uint8_t *data, const size_t  &size)
{
  (void)data; (void)size;
  Http1Response response;
  {
    std::lock_guard<std::mutex> guard(sent_res_http1_lock_);
    auto it = sent_res_http1_.find(stream_id);
    if (it == sent_res_http1_.end())
      return false;

    response = std::move(it->second);
    sent_res_http1_.erase(it);
  }

  if (response.is_websocket_upgrade() == true)
    websocket_ = true;

  this->handle_sent(response);

  return true;
}










