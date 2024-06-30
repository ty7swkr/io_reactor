#include "SSLContext.h"
#include <reactor/trace.h>
#include <string>

#include <pthread.h>
#include <string.h>

using namespace reactor;

#if OPENSSL_VERSION_NUMBER < 0x10101000L
/* This array will store all of the mutexes available to OpenSSL. */
static pthread_mutex_t *mutex_buf = NULL;
#endif

static std::once_flag initialize_flag;

#if OPENSSL_VERSION_NUMBER >= 0x10002000L
static int select_alpn(SSL                  *ssl,
                       const unsigned char  **out,
                       unsigned char        *outlen,
                       const unsigned char  *in,
                       unsigned int         inlen,
                       void                 *arg)
{
  (void)ssl;

  const std::string &alpn_ = *(reinterpret_cast<std::string *>(arg));

  // 무조건 ok
  if (alpn_.length() == 0)
  {
    reactor_trace << "SSL_TLSEXT_ERR_OK";
    *out = (unsigned char *)&in[1];
    *outlen = in[0];
    return SSL_TLSEXT_ERR_OK;
  }

  // 맞는 값만 ok
  unsigned int i;
  for (i = 0; i + alpn_.length() <= inlen; i += (unsigned int)(in[i] + 1))
  {
    if (memcmp(&in[i], alpn_.c_str(), alpn_.length()) == 0)
    {
      *out = (unsigned char *)&in[i + 1];
      *outlen = in[i];
      reactor_trace << "SSL_TLSEXT_ERR_OK";
      return SSL_TLSEXT_ERR_OK;
    }
  }

  reactor_trace << "SSL_TLSEXT_ERR_NOACK";
  return SSL_TLSEXT_ERR_NOACK;
}
#endif /* OPENSSL_VERSION_NUMBER >= 0x10002000L */

#if OPENSSL_VERSION_NUMBER < 0x10101000L
inline static void
locking_function(int mode, int n, const char *file, int line)
{
  (void)file;
  (void)line;

  if (n < 0 || n >= CRYPTO_num_locks())
    return;

  if (mode & CRYPTO_LOCK)
    pthread_mutex_lock(&(mutex_buf[n]));
  else
    pthread_mutex_unlock(&(mutex_buf[n]));
}

inline static unsigned long
id_function(void)
{
  return ((unsigned long)pthread_self());
}
#endif

bool
SSLContext::initialize()
{
#if OPENSSL_VERSION_NUMBER < 0x10101000L
  if (mutex_buf != NULL)
    return true;

  mutex_buf = (pthread_mutex_t *)malloc(CRYPTO_num_locks() * sizeof(pthread_mutex_t));

  if (mutex_buf == NULL)
    return false;

  for (int index = 0; index < CRYPTO_num_locks(); ++index)
    pthread_mutex_init(&(mutex_buf[index]), NULL);

  CRYPTO_set_id_callback(id_function);
  CRYPTO_set_locking_callback(locking_function);
#endif
  SSL_library_init();
  SSLeay_add_ssl_algorithms();
  SSL_load_error_strings();
  return true;
}

int
SSLContext::deinitialize()
{
#if OPENSSL_VERSION_NUMBER < 0x10101000L
  if (mutex_buf == NULL)
    return false;

  CRYPTO_set_id_callback(NULL);
  CRYPTO_set_locking_callback(NULL);

  for (int index = 0; index < CRYPTO_num_locks(); ++index)
    pthread_mutex_destroy(&(mutex_buf[index]));

  free(mutex_buf);
  mutex_buf = NULL;
#endif
  return true;
}

SSLContextSPtr
SSLContext::create(error_func_t       error_func,
                   const std::string  &cert_file,
                   const std::string  &key_file,
                   const std::string  &chain_file,
                   const std::string  &alpn)
{
  SSLContextSPtr ssl_context = SSLContext::create(error_func, cert_file, key_file, chain_file);

  if (ssl_context == nullptr)
    return nullptr;

#if OPENSSL_VERSION_NUMBER >= 0x10002000L
#pragma message "SSL_CTX_set_alpn_select_cb"
  ssl_context->alpn_ = alpn;
  SSL_CTX_set_alpn_select_cb(ssl_context->object(),
                             select_alpn,
                             (void *)&(ssl_context->alpn()));
#endif

  return ssl_context;
}

SSLContextSPtr
SSLContext::create_client(error_func_t error_func)
{
  std::call_once(initialize_flag, []() { initialize(); });

#if OPENSSL_VERSION_NUMBER < 0x10101000L
#pragma message OPENSSL_VERSION_TEXT
  static std::mutex create_mutex;
  std::lock_guard<std::mutex> lock(create_mutex);
  SSL_CTX *ssl_ctx = SSL_CTX_new(TLSv1_2_client_method());
#else
#pragma message OPENSSL_VERSION_TEXT
  SSL_CTX *ssl_ctx = SSL_CTX_new(TLS_client_method()); // Negotiate highest available SSL/TLS version
#endif

  SSL_CTX_set_cipher_list(ssl_ctx,
#if OPENSSL_VERSION_NUMBER >= 0x10101000L // tlsv 1.3
                          TLS_DEFAULT_CIPHERSUITES
#endif
                          "ECDHE-RSA-AES256-GCM-SHA384:"
                          "ECDHE-RSA-AES256-SHA384:"
                          "ECDHE-RSA-AES256-SHA:"
                          //                          "ECDHE-RSA-AES128-SHA:"
                          //                          "ECDHE-RSA-AES128-SHA:"
                          //                          "ECDHE-RSA-AES128-GCM-SHA256:"
                          //                          "ECDHE-RSA-AES128-SHA256:"
                          //                          "ECDHE-RSA-AES128-SHA:"
                          "AES256-GCM-SHA384:"
                          "AES256-SHA256:"
                          //                          "AES128-GCM-SHA256:"
                          //                          "AES128-GCM-SHA256:"
                          //                          "AES128-SHA256:"
                          "!CAMELLIA256:"
                          "!CAMELLIA128:"
                          "!CAMELLIA"
                          "HIGH:"
                          "!LOW:"
                          "!MEDIUM:"
                          "!EXP:"
                          "!NULL:"
                          "!aNULL@STRENGTH");

  SSL_CTX_set_options(ssl_ctx,
                      (SSL_OP_ALL & ~SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS) |
                      SSL_OP_NO_SSLv2 |
                      SSL_OP_NO_SSLv3 |
                      SSL_OP_NO_COMPRESSION |
                      SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION |
                      SSL_OP_SINGLE_ECDH_USE |
                      SSL_OP_NO_TICKET |
                      SSL_OP_CIPHER_SERVER_PREFERENCE);

  SSL_CTX_set_session_cache_mode(ssl_ctx,
                                 SSL_SESS_CACHE_NO_AUTO_CLEAR |
                                 SSL_SESS_CACHE_CLIENT);

  SSL_CTX_set_mode(ssl_ctx, SSL_MODE_AUTO_RETRY);
  SSL_CTX_set_mode(ssl_ctx, SSL_MODE_RELEASE_BUFFERS);
  //  NID_secp384r1
  //  NID_X9_62_prime256v1, NID_X9_62_prime192v1, NID_X25519
  //  enables AES-128 ciphers, to get AES-256 use NID_secp384r1
#if OPENSSL_VERSION_NUMBER < 0x10101000L
  EC_KEY *ecdh = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);
#endif
#if OPENSSL_VERSION_NUMBER >= 0x10101000L // tlsv 1.3
  // erver Temp Key: X25519 253 bits
  EVP_PKEY *ecdh = NULL;
  EVP_PKEY_CTX *pctx = EVP_PKEY_CTX_new_id(NID_X25519, NULL);
  EVP_PKEY_keygen_init(pctx);
  EVP_PKEY_keygen(pctx, &ecdh);
#endif

  if (!ecdh)
  {
    //    ERR_print_errors_fp(stderr);
    ERR_print_errors_cb(error_str_callback, &error_func);
    SSL_CTX_free(ssl_ctx);
    return nullptr;
  }

  SSL_CTX_set_tmp_ecdh(ssl_ctx, ecdh);
#if OPENSSL_VERSION_NUMBER < 0x10101000L
  EC_KEY_free(ecdh);
#endif
#if OPENSSL_VERSION_NUMBER >= 0x10101000L
  EVP_PKEY_free(ecdh);
#endif

  SSLContextSPtr ssl_context(new SSLContext);
  ssl_context->ssl_ctx_ = ssl_ctx;

  return ssl_context;
}

SSLContextSPtr
SSLContext::create(error_func_t       error_func,
                   const std::string  &cert_file,
                   const std::string  &key_file,
                   const std::string  &chain_file)
{
  std::call_once(initialize_flag, []() { initialize(); });

#if OPENSSL_VERSION_NUMBER < 0x10101000L
#pragma message OPENSSL_VERSION_TEXT
  static std::mutex create_mutex;
  std::lock_guard<std::mutex> lock(create_mutex);
  SSL_CTX *ssl_ctx = SSL_CTX_new(TLSv1_2_server_method());
#else
#pragma message OPENSSL_VERSION_TEXT
  SSL_CTX *ssl_ctx = SSL_CTX_new(TLS_server_method());
#endif

  SSL_CTX_set_cipher_list(ssl_ctx,
#if OPENSSL_VERSION_NUMBER >= 0x10101000L // tlsv 1.3
                          TLS_DEFAULT_CIPHERSUITES
#endif
                          "ECDHE-RSA-AES256-GCM-SHA384:"
                          "ECDHE-RSA-AES256-SHA384:"
                          "ECDHE-RSA-AES256-SHA:"
                          //                          "ECDHE-RSA-AES128-SHA:"
                          //                          "ECDHE-RSA-AES128-SHA:"
                          //                          "ECDHE-RSA-AES128-GCM-SHA256:"
                          //                          "ECDHE-RSA-AES128-SHA256:"
                          //                          "ECDHE-RSA-AES128-SHA:"
                          "AES256-GCM-SHA384:"
                          "AES256-SHA256:"
                          //                          "AES128-GCM-SHA256:"
                          //                          "AES128-GCM-SHA256:"
                          //                          "AES128-SHA256:"
                          "!CAMELLIA256:"
                          "!CAMELLIA128:"
                          "!CAMELLIA"
                          "HIGH:"
                          "!LOW:"
                          "!MEDIUM:"
                          "!EXP:"
                          "!NULL:"
                          "!aNULL@STRENGTH");

  SSL_CTX_set_options(ssl_ctx,
                      (SSL_OP_ALL & ~SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS) |
                      SSL_OP_NO_SSLv2 |
                      SSL_OP_NO_SSLv3 |
                      SSL_OP_NO_COMPRESSION |
                      SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION |
                      SSL_OP_SINGLE_ECDH_USE |
                      SSL_OP_NO_TICKET |
                      SSL_OP_CIPHER_SERVER_PREFERENCE);

  SSL_CTX_set_session_cache_mode(ssl_ctx,
                                 SSL_SESS_CACHE_NO_AUTO_CLEAR |
                                 SSL_SESS_CACHE_SERVER);

  SSL_CTX_set_mode(ssl_ctx, SSL_MODE_AUTO_RETRY | SSL_MODE_RELEASE_BUFFERS);
  //  NID_secp384r1
  //  NID_X9_62_prime256v1, NID_X9_62_prime192v1, NID_X25519
  //  enables AES-128 ciphers, to get AES-256 use NID_secp384r1
#if OPENSSL_VERSION_NUMBER < 0x10101000L
  EC_KEY *ecdh = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);
#endif
#if OPENSSL_VERSION_NUMBER >= 0x10101000L // tlsv 1.3
  // erver Temp Key: X25519 253 bits
  EVP_PKEY *ecdh = NULL;
  EVP_PKEY_CTX *pctx = EVP_PKEY_CTX_new_id(NID_X25519, NULL);
  EVP_PKEY_keygen_init(pctx);
  EVP_PKEY_keygen(pctx, &ecdh);
#endif

  if (!ecdh)
  {
    //    ERR_print_errors_fp(stderr);
    ERR_print_errors_cb(error_str_callback, &error_func);
    SSL_CTX_free(ssl_ctx);
    return nullptr;
  }

  SSL_CTX_set_tmp_ecdh(ssl_ctx, ecdh);
#if OPENSSL_VERSION_NUMBER < 0x10101000L
  EC_KEY_free(ecdh);
#endif
#if OPENSSL_VERSION_NUMBER >= 0x10101000L
  EVP_PKEY_free(ecdh);
#endif

  // 인증서를 파일로 부터 로딩할때 사용함.
  if (SSL_CTX_use_certificate_file(ssl_ctx, cert_file.c_str(), SSL_FILETYPE_PEM) <= 0)
  {
    ERR_print_errors_cb(error_str_callback, &error_func);
    SSL_CTX_free(ssl_ctx);
    return nullptr;
  }

  /* 암호화 통신을 위해서 이용하는 개인 키를 설정한다. */
  if (SSL_CTX_use_PrivateKey_file(ssl_ctx, key_file.c_str(), SSL_FILETYPE_PEM) <= 0)
  {
    ERR_print_errors_cb(error_str_callback, &error_func);
    SSL_CTX_free(ssl_ctx);
    return nullptr;
  }

  /* 개인 키가 사용 가능한 것인지 확인한다. */
  if (SSL_CTX_check_private_key(ssl_ctx) == 0)
  {
    ERR_print_errors_cb(error_str_callback, &error_func);
    SSL_CTX_free(ssl_ctx);
    return nullptr;
  }

  if (chain_file.length() > 0)
  {
    if (SSL_CTX_load_verify_locations(ssl_ctx, chain_file.c_str(), NULL) == 0)
    {
      ERR_print_errors_cb(error_str_callback, &error_func);
      SSL_CTX_free(ssl_ctx);
      return nullptr;
    }
  }

  SSLContextSPtr ssl_context(new SSLContext);
  ssl_context->ssl_ctx_ = ssl_ctx;

  return ssl_context;
}
