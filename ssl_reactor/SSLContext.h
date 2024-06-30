/*
 * SSLContext.h
 *
 *  Created on: 2020. 2. 6.
 *      Author: tys
 */

#ifndef IO_REACTOR_SSL_REACTOR_SSLCONTEXT_H_
#define IO_REACTOR_SSL_REACTOR_SSLCONTEXT_H_

#include <ssl_reactor/SSLInclude.h>

#include <string>
#include <functional>
#include <memory>
#include <mutex>

namespace reactor
{

class SSLContext;
using SSLContextSPtr = std::shared_ptr<SSLContext>;

class SSLContext
{
public:
  using error_func_t = std::function<void(const int &, const std::string &)>;

  static inline const std::string alpn_http11 = "\x8http/1.1";
  static inline const std::string alpn_http2  = "\x2h2";

  static bool initialize  ();

  static int  deinitialize();

  static SSLContextSPtr create_client(error_func_t  error_func);

  static SSLContextSPtr create  (error_func_t       error_func,
                                 const std::string  &cert_file,
                                 const std::string  &key_file,
                                 const std::string  &chain_file = "");

  // alpn == ""      : 아무거나 들어와도 ok.
  // alpn != ""      : 해당값인 경우만 ok.
  static SSLContextSPtr create  (error_func_t       error_func,
                                 const std::string  &cert_file,
                                 const std::string  &key_file,
                                 const std::string  &chain_file,
                                 const std::string  &alpn);

  SSLContext()   {}
  ~SSLContext() { if (ssl_ctx_ != NULL) SSL_CTX_free(ssl_ctx_); }

  const std::string &alpn  () const { return alpn_; }
  SSL_CTX           *object() { return ssl_ctx_; }

protected:
  static int error_str_callback(const char *str, size_t len, void *arg)
  {
    error_func_t &error_func = *(reinterpret_cast<error_func_t *>(arg));
    if (error_func != nullptr)
      error_func(ERR_get_error(), std::string(str, str+len));
    return 1;
  }

private:
  SSL_CTX     *ssl_ctx_ = nullptr;
  std::string alpn_     = "";
};

}

#endif /* SSLContext_h */
