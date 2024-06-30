/*
 * main.cpp
 *
 *  Created on: 2021. 7. 26.
 *      Author: tys
 */

#include <https1_reactor/https1_reactor.h>
#include <reactor/trace.h>

#include <string.h>
#include <signal.h>

using namespace https_reactor;

// client handler
class WssClientHandler : public Https1Handler
{
public:
  WssClientHandler(const struct sockaddr_storage &addr)
  : addr_(addr) {}
  virtual ~WssClientHandler() {}

protected:
  void handle_registered() override
  {
    //reactor_trace << this->handler_count();
  }

  void handle_accept(const std::string &alpn)
  {
    //reactor_trace << alpn;
  }

  // Called when data arrives.
  void handle_timeout(const int64_t &timer_key) override
  {
    //reactor_trace << timer_key;
    this->close();
  }

  void  handle_error(const SSL_STATE     &ssl_state,
                     const int           &err_no,
                     const std::string   &err_str) override
  {
    (void)ssl_state;
    //reactor_trace << err_no << err_str;
  }

  void  handle_request(const Http1Request &request) override
  {
    //reactor_trace << request.packet();
    HttpHeader header;
    header.set("Server",        "nginx/1.18.0 (Ubuntu)");
    header.set("Content-Type",  "text/html");
    header.set("ETag",          "\"6523f885-0\"");
    header.set("Accept-Ranges", "bytes");
    header.set("Content-Length","0");
    header.set("Connection",   "close");

    this->send(Http1Response(next_stream_id(), 200, header));
  }

  // Called by Reactor when the client is disconnected.
  void  handle_sent(const Http1Response &response) override
  {
    //reactor_trace << response.packet();
//    //reactor_trace << this->close();
//    std::this_thread::sleep_for(std::chrono::milliseconds(10));
//    this->close();
  }

  void  handle_sent_error(const SSL_STATE     &ssl_state,
                          const int           &err_no,
                          const std::string   &err_str,
                          const Http1Response &response) override
  {
    (void)ssl_state;
    //reactor_trace << response.packet();
    //reactor_trace << err_no << err_str;
  }

  void  handle_request(const std::deque<WebSocket> &requests) override
  {
    for (auto const &request : requests)
    {
      if (request.is_ping() == true)
      {
        //reactor_trace << "ping";
        this->send(WebSocket::make_pong());
        continue;
      }

      //reactor_trace << request.payload_to_string();
      this->send(WebSocket::make(request.payload_to_string()));
    }
  }

  void  handle_sent(const WebSocket &response) override
  {
    //reactor_trace << response.payload_to_string();
  }

  void  handle_sent_error(const SSL_STATE     &ssl_state,
                          const int           &err_no,
                          const std::string   &err_str,
                          const int32_t       &stream_id,
                          const WebSocket     &response)
  {
    (void)ssl_state;
    //reactor_trace << stream_id << err_no << err_str;
    //reactor_trace << response.payload_to_string();
  }

  void  handle_close()
  {
    //reactor_trace;
  }

  void handle_shutdown() override
  {
    //reactor_trace;
  }

  void handle_removed() override
  {
    //reactor_trace << handler_count();
    delete this;
  }

protected:
  const struct ::sockaddr_storage addr_;

protected:
  char buff_[1024];
};

class ClientHandlerFactory : public SSLSessionHandlerFactory
{
public:
  ClientHandlerFactory() {}
  virtual ~ClientHandlerFactory() {}

  SSLSessionHandler *create(const io_handle_t      &client_io_handle,
                            const sockaddr_storage &client_addr) override
  {
    try
    {
      return new WssClientHandler(client_addr);
    }
    catch (std::runtime_error &e)
    {
      std::cerr << e.what() << std::endl;
      ::close(client_io_handle);
    }

    reactor_trace;
    return nullptr;
  }
};

Https1Reactor https1_reactor;

void sig_handler(int signum)
{
  https1_reactor.stop();
}

int main(void)
{
  signal(SIGPIPE, SIG_IGN);
  signal(SIGHUP,  sig_handler);
  signal(SIGINT,  sig_handler);

  reactor_trace;
  ClientHandlerFactory factory;

  reactor_trace;
  https1_reactor.set_acceptor_ipv46(&factory, 2000, "::0", 2, 1000);

  reactor_trace << https1_reactor.set_reactor(4, "./server.crt", "./server.key", "", 1000, 100);
  reactor_trace << https1_reactor.start();

  reactor_trace;
  https1_reactor.wait();

  reactor_trace;
  https1_reactor.stop();

  return 0;
}






