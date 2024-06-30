/*
 * main.cpp
 *
 *  Created on: 2021. 7. 26.
 *      Author: tys
 */

#include <http1_client/Http1Client.h>
#include <reactor/trace.h>

#include <signal.h>
#include <netdb.h>

using namespace reactor;

std::string domain  = "127.0.0.1";
int16_t     port    = 2000;

std::string
host();

bool
is_ip(const std::string &ip_str);

std::optional<std::deque<std::string>>
host_name_to_ips(const std::string &host_name);

inline std::string
get_process_name(const char *argv0)
{
  std::string process_name = argv0;
  std::string::size_type pos = process_name.rfind("/");
  if (pos != std::string::npos)
    process_name = process_name.substr(pos+1);

  return process_name;
}

class WsClient : public Http1Client
{
public:
  WsClient(Reactor &reactor, const size_t &recv_buffer_size = 10240)
  : Http1Client(reactor, recv_buffer_size) {}
  virtual ~WsClient() {}

  bool connect()
  {
    std::string host_ip = domain;
    if (is_ip(domain) == false)
    {
      auto ips = host_name_to_ips(domain);
      if (ips.has_value() == false)
      {
        reactor_trace << host() << "has no ip";
        return false;
      }

      host_ip = ips.value().front();
    }

    return Http1Client::connect(host_ip, port);
  }

protected:
  void on_connect() override
  {
    Http1Request upgrade_websocket = Http1Request::upgrade_websocket(next_stream_id(), "/", "", {{"Host", host()}});
    this->send(upgrade_websocket);
  }

  void on_connect_error(const int &err_no, const std::string &err_str) override
  {
    reactor_trace << err_no << err_str;
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    this->connect();
  }

  void on_connect_timeout(const int &err_no, const std::string &err_str) override
  {
    reactor_trace << err_no << err_str;
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    this->connect();
  }

  void on_disconnect() override
  {
    reactor_trace;
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    this->connect();
  }

  void on_response(const Http1Response &response) override
  {
    if (is_switching_websocket(response) == false)
    {
      reactor_trace << "cannot switching websocket";
      return;
    }
    reactor_trace << "switching websocket";
    this->upgrade_websocket();

    this->set_timeout(5000);
    this->send(next_stream_id(), WebSocket::make("test", true));
  }

  void on_response(const std::deque<WebSocket> &responses) override
  {
    for (const auto &response : responses)
    {
      if (response.is_pong() == true)
      {
        reactor_trace << "pong";
        continue;
      }
      reactor_trace << response.payload_to_string();
    }
  }

  void on_sent(const Http1Request &request) override
  {
    reactor_trace << request.packet();
  }

  void on_sent_error(const int          &err_no,
                     const std::string  &err_str,
                     const Http1Request &request)
  {
    reactor_trace << err_no << err_str << request.packet();
  }

  void on_sent(const int32_t &stream_id, const WebSocket &request) override
  {
    reactor_trace << stream_id << request.payload_to_string();
  }

  void on_sent_error(const int          &err_no,
                     const std::string  &err_str,
                     const int32_t      &stream_id,
                     const WebSocket    &request)
  {
    reactor_trace << err_no << err_str << stream_id << request.payload_to_string();
  }

  void on_error_http11(const int &err_no, const std::string &err_str) override
  {
    reactor_trace << err_no << err_str;
  }

  void on_error_websocket(const int &err_no, const std::string &err_str) override
  {
    reactor_trace << err_no << err_str;
  }

  void on_timeout(const uint64_t &timeout_id) override
  {
    reactor_trace << timeout_id;
    this->send(next_stream_id(), WebSocket::make_ping(true));
    this->set_timeout(5000);
  }

  void on_shutdown() override
  {
    reactor_trace;
  }
};

int main(int argc, char **argv)
{
  signal(SIGPIPE, SIG_IGN);

  ReactorThread reactor_thread;
  reactor_thread.reactor.init(1000, 1024);
  reactor_thread.start();

  WsClient client(reactor_thread.reactor);
  reactor_trace << client.connect();

  while (true)
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

  return 0;
}

inline std::string
host() { return domain+":"+std::to_string(port); }

inline bool
is_ip(const std::string &ip_str)
{
  struct sockaddr_in sa;
  return inet_pton(AF_INET, ip_str.c_str(), &(sa.sin_addr)) != 0;
};

inline std::optional<std::deque<std::string>>
host_name_to_ips(const std::string &host_name)
{
  if (is_ip(host_name) == true)
    return {{host_name}};

  // 도메인 이름을 IP 주소로 변환합니다.
  struct hostent *hostent_ptr;
  hostent_ptr = gethostbyname(host_name.c_str());

  // 변환에 성공하면 IP 주소를 출력합니다.
  if (hostent_ptr == nullptr)
    return std::nullopt;

  std::deque<std::string> ips;
  struct in_addr **addr_list = (struct in_addr **)hostent_ptr->h_addr_list;
  for (size_t index = 0; addr_list[index] != nullptr; ++index)
    ips.emplace_back(inet_ntoa(*addr_list[index]));

  if (ips.size() == 0)
    return std::nullopt;

  return ips;
}











