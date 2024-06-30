/*
 * main.cpp
 *
 *  Created on: 2021. 7. 26.
 *      Author: tys
 */

#include <reactor/acceptor/AcceptorThread.h>
#include <reactor/Reactors.h>
#include <reactor/trace.h>

#include <string.h>

using namespace reactor;

// reactor thread monitoring handler
class MyReactorHandler : public ReactorHandler
{
public:
  virtual ~MyReactorHandler() {}

protected:
  void reactor_handle_registered() override
  {
    reactor_->set_timeout(this, 1000);
    reactor_trace;
  }

  void reactor_handle_timeout() override
  {
    reactor_->set_timeout(this, 1000);
    reactor_trace;
  }

  void reactor_handle_shutdown() override
  {
    reactor_trace;
  }

  void reactor_handle_error(const int         &error_no,
                            const std::string &error_str) override
  {
    reactor_trace << error_no << error_str;
  }

  void reactor_handle_registered_handler(EventHandler *handler) override
  {
    (void)handler;
    reactor_trace;
  }

  void reactor_handle_removed_handler(EventHandler *handler) override
  {
    (void)handler;
    reactor_trace;
  }
};

// reactor thread monitoring handler factory
class MyReactorHandlerFactory : public ReactorHandlerFactory
{
public:
  virtual ~MyReactorHandlerFactory() {}
  ReactorHandler *create(Reactor *reactor) override
  {
    (void)reactor;
    return new MyReactorHandler;
  }

  void destroy(Reactor        *reactor,
               ReactorHandler *handler) override
  {
    (void)reactor;
    delete handler;
  }
};

class MyAcceptorThreadHandler : public AcceptorThreadHandler
{
protected:
  virtual ~MyAcceptorThreadHandler() {}
  void handle_registered() override
  {
    this->set_timeout(1000);
    reactor_trace;
  }

  void handle_timeout() override
  {
    this->set_timeout(1000);
    reactor_trace;
  }

  void handle_shutdown() override
  {
    reactor_trace; delete this;
  }
};

// client handler
class ClientHandler : public EventHandler
{
public:
  ClientHandler(const struct sockaddr_storage &addr,
                Acceptor &acceptor,
                Reactors &reactors)
  : addr_(addr), acceptor_(acceptor), reactors_(reactors) {}
  virtual ~ClientHandler() {}

protected:
  // When a new connection is created and registered with the reactor.
  void handle_registered() override
  {
    reactor_trace << io_handle_ << reactor_->handler_count();
  }

  // Called when data arrives.
  void handle_input() override
  {
    memset(buff_, 0x00, sizeof(buff_));

    reactor_trace << ::recv(io_handle_, buff_, sizeof(buff_), 0);
    reactor_trace << buff_;

    // Register to write to the reactor.
    reactor_->register_writable(this);
  }

  // Called when writing is possible.
  void handle_output() override
  {
    reactor_trace << ::send(io_handle_, buff_, strlen(buff_), 0);
  }

  // Called by Reactor when the client is disconnected.
  void handle_close() override
  {
    reactor_trace;
    reactor_->remove_event_handler(this);
  }

  // When Reactor's set_timeout(msec) is called,
  // Reactor calls handle_timeout() after the timeout.
  // set_timeout is one-time. If you want to call handle_timeout repeatedly,
  // you must call Reactor::set_timeout(msec) each time.
  void handle_timeout() override
  {
    reactor_trace;
  }

  // Called on error. The error is set to the standard errno.
  void handle_error(const int &error_no = 0, const std::string &error_str = "") override
  {
    reactor_trace << error_no << error_str;
  }

  // Called when the reactor is shutting down. The reactor is set to nullptr.
  void handle_shutdown() override
  {
    reactor_trace;
  }

  // Called when removed from Reactor.
  void handle_removed() override
  {
    reactor_trace;
    ::close(io_handle_);
    delete this;
  }

protected:
  const struct ::sockaddr_storage addr_;
  Acceptor  &acceptor_;
  Reactors  &reactors_;

protected:
  char buff_[1024];
};

class ClientHandlerFactory : public EventHandlerFactory
{
public:
  ClientHandlerFactory(Acceptor &acceptor, Reactors &reactors)
  : acceptor_(acceptor), reactors_(reactors) {}
  virtual ~ClientHandlerFactory() {}
  
  EventHandler *create(const io_handle_t      &client_io_handle,
                       const sockaddr_storage &client_addr) override
  {
    try
    {
      return new ClientHandler(client_addr, acceptor_, reactors_);
    }
    catch (std::runtime_error &e)
    {
      std::cerr << e.what() << std::endl;
      ::close(client_io_handle);
    }

    return nullptr;
  }

protected:
  Acceptor  &acceptor_;
  Reactors  &reactors_;
};

int main(void)
{
  Reactors reactors;
  reactors.init(5, new MyReactorHandlerFactory);
  reactors.start();

  Acceptor acceptor;
  acceptor.listen_ipv46(2000);

  ClientHandlerFactory client_handler_factory(acceptor, reactors);
  AcceptorThreadHandlerFactoryTemplate<MyAcceptorThreadHandler> acceptor_thread_handler_factory;

  std::deque<AcceptorThread *> acceptors;
  for (size_t thread_num = 0; thread_num < 1; ++thread_num)
  {
    acceptors.emplace_back(new AcceptorThread(acceptor,
                                              reactors,
                                              client_handler_factory,
                                              &acceptor_thread_handler_factory));
    acceptors.back()->start();
  }

  while (true)
    std::this_thread::sleep_for(std::chrono::seconds(1));

  for (auto acceptor_thread : acceptors)
  {
    acceptor_thread->stop();
    delete acceptor_thread;
  }

  reactors.stop();
  return 0;
}
