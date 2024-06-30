#include "WsHandler.h"

using namespace https_reactor;

void
WsHandler::handle_registered()
{
  I_LOG << this->peer();
}

void
WsHandler::handle_request(const Http1Request &request)
{
  this->set_timeout(30000);

  if (this->is_websocket() == true)
  {
    this->close();
    return;
  }

  if (request.is_upgrade_wabsocket() == false)
  {
    this->close();
    return;
  }

  Http1Response resp =
      Http1Response::websocket_permission(next_stream_id(),
                                          request.get_sec_websocket_key());

  if (this->send(resp) == false)
    this->close();
}

void
WsHandler::handle_timeout(const int64_t &timer_key)
{
  I_LOG << "timeout session." << "send PING" << timer_key;
  this->send(WebSocket::make_ping(false));
  this->set_timeout(10000);
}

void
WsHandler::handle_sent(const Http1Response &response)
{
  D_LOG << response.packet();
}

void
WsHandler::handle_request(const std::deque<WebSocket> &requests)
{
  I_LOG << "requests" << requests.size();
  for (const auto &request : requests)
  {
    I_LOG << request.to_string();

    if (request.is_ping() == true || request.is_pong() == true)
    {
      if (request.is_pong() == true)
      {
        I_LOG << "recvied PONG";
        return;
      }

      I_LOG << "recvied PING" << requests.size();
      this->send(WebSocket::make_pong());
      continue;
    }

    if (request.is_close() == true)
    {
      I_LOG << "will be closed";
      this->close();
      return;
    }

    I_LOG << request.payload_to_string();
    this->send(WebSocket::make(request.payload_to_string()));
  }
}

void
WsHandler::handle_sent(const WebSocket &response)
{
  if (response.is_pong() == true)
  {
    I_LOG << "sent PONG";
    return;
  }

  I_LOG << response.payload_to_string();
}

void
WsHandler::handle_close()
{
  I_LOG << "closed";
}

void
WsHandler::handle_removed()
{
  I_LOG << "removed";
}

