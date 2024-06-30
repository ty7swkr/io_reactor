/*
 * SSLAcceptorThread.h
 *
 *  Created on: 2022. 9. 8.
 *      Author: tys
 */

#ifndef IO_REACTOR_SSL_REACTOR_SSLACCEPTORTHREAD_H_
#define IO_REACTOR_SSL_REACTOR_SSLACCEPTORTHREAD_H_

#include <ssl_reactor/SSLSessionHandlerFactory.h>
#include <ssl_reactor/SSLEventHandlerFactory.h>
#include <ssl_reactor/SSLContext.h>

#include <reactor/acceptor/AcceptorThread.h>

#include <optional>
#include <string>
#include <memory>

namespace reactor
{

#pragma GCC diagnostic ignored "-Wreorder"

using SSLContexts = std::vector<SSLContextSPtr>;

class SSLAcceptorThread : public AcceptorThread
{
public:
  static inline const std::string alpn_http11 = SSLContext::alpn_http11;
  static inline const std::string alpn_http2  = SSLContext::alpn_http2;

public:
  static std::optional<SSLContexts>
  prepare_ssl       (SSLContext::error_func_t     error_func,
                     Reactors                     &reactors,
                     const std::string            &cert_file,
                     const std::string            &priv_file,
                     const std::string            &chain_file = "",
                     const std::string            &alpn = "");

  static std::optional<SSLContexts>
  prepare_ssl       (Reactors                     &reactors,
                     const std::string            &cert_file,
                     const std::string            &priv_file,
                     const std::string            &chain_file = "",
                     const std::string            &alpn = "");

public:
  SSLAcceptorThread(SSLSessionHandlerFactory      &handler_factory,
                    SSLContexts                   &ssl_ctx_ptrs,
                    Acceptor                      &acceptor,
                    Reactors                      &reactors,
                    AcceptorThreadHandlerFactory  *acceptor_thread_handler_factory = nullptr);
  virtual ~SSLAcceptorThread() {}

protected:
  bool on_created_handler(EventHandler *, Reactor &, Reactors &, const size_t &) override;

private:
  SSLEventHandlerFactory  ssl_handler_factory_;
  SSLContexts             ssl_ctx_ptrs_;
};

inline
SSLAcceptorThread::SSLAcceptorThread(SSLSessionHandlerFactory     &handler_factory,
                                     SSLContexts                  &ssl_ctx_ptrs,
                                     Acceptor                     &acceptor,
                                     Reactors                     &reactors,
                                     AcceptorThreadHandlerFactory *acceptor_thread_handler_factory)
: ssl_handler_factory_(handler_factory, acceptor, reactors),
  ssl_ctx_ptrs_       (ssl_ctx_ptrs),
  // ssl_handler_factory_가 초기화 되어야 하므로 AcceptorThread가 이후에 호출된다.
  AcceptorThread      (acceptor,
                       reactors,
                       ssl_handler_factory_,
                       acceptor_thread_handler_factory)
{
}

#pragma GCC diagnostic pop

inline std::optional<SSLContexts>
SSLAcceptorThread::prepare_ssl(SSLContext::error_func_t  error_func,
                               Reactors                  &reactors,
                               const std::string         &cert_file,
                               const std::string         &priv_file,
                               const std::string         &chain_file,
                               const std::string         &alpn)
{
  size_t reactors_size = reactors.get_reactors().size();

  SSLContexts ssl_ctxs;
  for (size_t index = 0; index < reactors_size; ++index)
  {
    SSLContextSPtr ssl_ctx = SSLContext::create(error_func,
                                                cert_file,
                                                priv_file,
                                                chain_file,
                                                alpn);

    if (ssl_ctx == nullptr)
      return std::nullopt;

    ssl_ctxs.emplace_back(ssl_ctx);
  }

  return ssl_ctxs;
}

inline std::optional<SSLContexts>
SSLAcceptorThread::prepare_ssl(Reactors          &reactors,
                               const std::string &cert_file,
                               const std::string &priv_file,
                               const std::string &chain_file,
                               const std::string &alpn)
{
  size_t reactors_size = reactors.get_reactors().size();

  //using error_func_t = std::function<void(const uint64_t &, const std::string &)>;
  SSLContext::error_func_t error_func = [](const uint64_t &code, const std::string &str)
  {
    throw std::pair<int, std::string>(code, str);
  };

  SSLContexts ssl_ctx_sptrs;
  for (size_t index = 0; index < reactors_size; ++index)
  {
    ssl_ctx_sptrs.emplace_back(SSLContext::create(error_func,
                                                  cert_file,
                                                  priv_file,
                                                  chain_file,
                                                  alpn));
  }

  return ssl_ctx_sptrs;
}

inline bool
SSLAcceptorThread::on_created_handler(EventHandler  *event_handler,
                                      Reactor       &reactor,
                                      Reactors      &reactors,
                                      const size_t  &reactors_index)
{
  (void)reactors; (void)reactor;
  SSL_CTX *ssl_ctx = ssl_ctx_ptrs_[reactors_index]->object();

  SSLEventHandler *ssl_event_handler = static_cast<SSLEventHandler *>(event_handler);
  return ssl_event_handler->init_ssl(ssl_ctx);
}

}

#endif /* SSL_REACTOR_SSLACCEPTORTHREAD_H_ */
