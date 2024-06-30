/*
 * SSLSessionHandler.h
 *
 *  Created on: 2022. 9. 15.
 *      Author: tys
 */

#ifndef IO_REACTOR_SSL_REACTOR_SSLSESSIONHANDLER_H_
#define IO_REACTOR_SSL_REACTOR_SSLSESSIONHANDLER_H_

#include <ssl_reactor/SSLState.h>
#include <reactor/acceptor/Acceptor.h>
#include <reactor/ObjectsTimer.h>
#include <reactor/trace.h>

#include <vector>
#include <string>
#include <memory>
#include <arpa/inet.h>

namespace reactor
{

using Bytes = std::vector<uint8_t>;

class Reactor;
class Reactors;
class SSLEventHandler;
class SSLSessionHandler;
using SSLSessionHandlerPtr = std::shared_ptr<SSLSessionHandler>;

class SSLSessionHandler
{
public:
  virtual ~SSLSessionHandler() { ssl_handler_.reset(); }

protected:
  /**
   * 클라이언트가 접속 후 reactor에 등록되었을때.
   * reactor는 SSLSessionHandler를 관리함.
   * Acceptor는 클라이언트가 접속되면 Reactor에게 SSLSessionHandler를 등록함.
   * 이때 호출되는 함수. 즉, 클라이언트가 접속시 호출됨.
   */
  virtual void handle_registered() = 0;
  /**
   * 클라이언트가 reactor에세 떨어졌을때.
   */
  virtual void handle_removed   () = 0;
  /**
   * ssl협상이 완료되면 호출됨.
   * @param alpn 상대가 협상한 alpn값.
   */
  virtual void handle_accept    (const std::string &alpn) = 0;
  /**
   * ssl 협상 완료 후 데이터가 들어왔을때. 복호화된 데이터를 수신함.
   * @param data
   * @param size
   */
  virtual void handle_input     (const uint8_t *data, const size_t  &size) = 0;
  /**
   * set_handle_output_event를 호출하고 보낼버퍼가 비어있다면 이 함수를 호출해준다.
   */
  virtual void handle_output    () {}
  /**
   * 데이터 전송완료 후 호출됨.
   * @param id SSLSessionHandler의 send시 키값.
   * @param data 보낸 데이터.
   * @param size 보낸 데이터의 크기.
   */
  virtual void handle_sent      (const int32_t &id,  const uint8_t *data, const size_t &size) = 0;
  /**
   * 접속 해제시 호출됨.
   * SSLSessionHandler의 close를 호출하거나, 상대접속이 끊긴 경우 호출되고
   * 이어서 handle_removed가 호출됨.
   */
  virtual void handle_close     () = 0;
  /**
   * SSLSessionHandler::set_timeout(ms)에 지정한 시간이 되면 호출됨.
   */
  virtual void handle_timeout   (const int64_t &timer_key) = 0;
  /**
   * 오류시 호출됨. 협상이 되지 않거나 전송등의 소켓 오류가 일어날때 호출됨.
   * @param ssl_state SSL_NONE, SSL_ACCEPT, SSL_READ, SSL_WRITE의 상태일 수 있음.
   * @param error_no 오류 코드
   * @param error_str 오류 내용
   */
  virtual void handle_error     (const SSL_STATE &ssl_state, const int &error_no, const std::string &error_str) = 0;
  virtual void handle_sent_error(const SSL_STATE &ssl_state, const int &error_no, const std::string &error_str,
                                 const int32_t &id,  const uint8_t *data, const size_t &size) = 0;
 /**
   * Reactor의 shutdown이 되었을때 호출됨.
   */
  virtual void handle_shutdown  () = 0;

public:
  const Acceptor &acceptor();
  Reactor        *reactor ();
  Reactors       &reactors();

public:
  bool send             (const int32_t  &id,    const uint8_t *data, const size_t &size);
  bool send             (const int32_t  &id,    const std::string &bytes);
  bool set_timeout      (const uint32_t &msec,  const int64_t     &key = 0);
  bool unset_timeout    (const int64_t  &key = 0);
  void handle_timeout   ();
  bool set_output_event ();
  bool close            ();

  bool is_ipv6() const { return ipv6_; }
  bool is_ipv4() const { return ipv4_; }
  bool is_uds () const { return uds_;  }

  const std::string &peer_addr()    const { return peer_addr_; }
  const uint16_t    &peer_port()    const { return peer_port_; }
  std::string       peer()          const { return peer_addr_ + ":" + std::to_string(peer_port_); }
  size_t            handler_count() const;

protected:
  int direct_send(const uint8_t     *data, const size_t &size);
  int direct_send(const std::string &data);

private:
  void set_socket_address(const struct ::sockaddr_storage &client_addr);

private:
  ObjectsTimer<int64_t> timer_;

private:
  std::string peer_addr_;
  uint16_t    peer_port_ = 0;

private:
  bool ipv6_ = false;
  bool ipv4_ = false;
  bool uds_  = false;

private:
  struct ::sockaddr_storage addr_;

private:
  std::shared_ptr<SSLEventHandler> ssl_handler_;
  friend class SSLEventHandler;
};

}

#endif /* LIBS_SSL_REACTOR_SSLSESSION_H_ */
