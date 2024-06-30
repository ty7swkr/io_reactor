#include <tcp_async_client/TcpAsyncClient.h>
#include <signal.h>

using namespace reactor;


class MyTcpClient : public TcpAsyncClient
{
public:
  MyTcpClient() : TcpAsyncClient(reactor_thread.reactor) {}

  bool start()
  {
    reactor_thread.reactor.init(100, 1000);
    reactor_thread.start();
    reactor_trace << reactor_thread.is_run();
    return this-connect();
  }

  bool connect()
  {
    return TcpAsyncClient::connect("localhost", 2000, 1000);
  }

  void  on_connect()
  {
    reactor_trace << this->io_handle();
    reactor_trace << "on_connect" << this->send(stream_id_++, "test", strlen("test"));
  }

  void  on_connect_error(const int &err_no, const std::string &err_str)
  {
    reactor_trace << "on_connect_error" << this->is_connect() << err_no << err_str;
  }

  void  on_connect_timeout(const int &err_no, const std::string &err_str)
  {
    reactor_trace << "on_connect_timeout" << this->is_connect() << err_no << err_str;
  }

  void  on_disconnect()
  {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    this->connect();
  }

  void  on_recv(const std::vector<uint8_t> &data)
  {
    reactor_trace << "on_recv" << data.size() << (const uint8_t *)&(data[0]);
  }

  void  on_sent(const int32_t &stream_id, const uint8_t *data, const size_t &size)
  {
    (void)data;
    reactor_trace << "on_sent" << stream_id << size;
  }

  void  on_sent_error     (const int &err_no, const std::string &err_str,
                           const int32_t &id, const uint8_t *data, const size_t &size)
  {
    (void)data;
    reactor_trace << "on_sent_error" << err_no << err_str << id << data << size;
  }

  void  on_error(const int &err_no, const std::string &err_str)
  {
    reactor_trace << "on_error" << this->is_connect() << err_no << err_str;
  }

  void  on_timeout()
  {
    reactor_trace << "on_timeout" ;
  }

  void  on_shutdown()
  {
    reactor_trace << "on_shutdown" ;
  }

private:
  ReactorThread reactor_thread;
  int32_t stream_id_ = 0;
};

int main(void)
{
  signal(SIGPIPE, SIG_IGN);

  MyTcpClient client;
  reactor_trace << client.start();

  while (true)
    std::this_thread::sleep_for(std::chrono::seconds(1));

  return 0;
}
