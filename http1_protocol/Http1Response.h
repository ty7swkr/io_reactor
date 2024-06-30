#ifndef HTTPPROTOCOL_HTTP1RESPONSE_H_
#define HTTPPROTOCOL_HTTP1RESPONSE_H_

#include <http1_protocol/Http1Protocol.h>
#include <http1_protocol/HttpHeader.h>
#include <http1_protocol/HttpStatus.h>
#include <http1_protocol/HttpVersion.h>
#include <websocket/WebSocket.h>
#include <reactor/trace.h>

#include <string_view>
#include <optional>
#include <cctype>

#include <string.h>

namespace https_reactor
{

using namespace reactor;

//  HTTP/1.1 200 OK\r\n
//  Date: Mon, 27 Jul 2009 12:28:53 GMT\r\n
//  Server: Apache/2.2.14 (Win32)\r\n
//  Last-Modified: Wed, 22 Jul 2009 19:15:56 GMT\r\n
//  Content-Length: 88\r\n
//  Content-Type: text/html\r\n
//  Connection: Closed\r\n
//  \r\n
//  This is a message body. All content in this message body should be stored under the
//  /files/129742 path, as specified by the PUT specification. The message body does
//  not have to be terminated with CRLF.\r\n

class Http1Response final : public Http1Protocol<Http1Response>
{
public:
  Http1Response() {}
  Http1Response(const int32_t     &stream_id,
                const size_t      &status,
                const HttpHeader  &header = {},
                const std::string &body = "")
  : Http1Protocol(stream_id, header, body), status_(status) {}

//  Http1Response(int32_t           &&stream_id,
//                size_t            &&status,
//                HttpHeader        &&header,
//                std::string       &&body)
//  : Http1Protocol(stream_id, header, body), status_(std::move(status)) {}
  virtual ~Http1Response() {}

  // protocol parameter
  const int32_t &status     () const { return status_; }
  std::string   status_str  () const { return status_code_to_string(status_); }

  bool
  is_websocket_upgrade() const;

  std::string
  packet_without_body() const;

  std::string
  packet() const;

  std::string
  packet(const bool &with_content_length = true);

  static Http1Response  // 웹소켓으로 업그레이드 응답을 만들어줌.
  websocket_permission(const int32_t     &stream_id,
                       const std::string &sec_websocket_key,
                       const HttpHeader  &response_header = {{}},
                       const std::string &response_body = "");

  static std::optional<Http1Response>
  parse(const std::string_view &message);

protected:
  static std::optional<bool>
  parse_parameter(Http1Response &h1, const std::string_view &parameter_line);

protected:
  int32_t status_ = 0;

  friend class Http1Protocol<Http1Response>;
};

inline bool
Http1Response::is_websocket_upgrade() const
{
  if (status_ != 101)
    return false;

  if (HttpHeader::to_lower(header.get<std::string>("upgrade")) != "websocket")
    return false;

  return true;
}

inline Http1Response
Http1Response::websocket_permission(const int32_t     &stream_id,
                                    const std::string &sec_websocket_key,
                                    const HttpHeader  &response_header,
                                    const std::string &response_body)
{
  Http1Response h1(stream_id, 101, response_header, response_body);

  h1.header.set("upgrade",              "websocket");
  h1.header.set("connection",           "Upgrade");
  h1.header.set("sec-websocket-accept", WebSocket::sec_accept_key(sec_websocket_key));

  return h1;
}

inline std::string
Http1Response::packet_without_body() const
{
  std::string packet =
      version_str() + " " + std::to_string(status_) + " " + status_code_to_string(status_) + "\r\n" +
      header.to_string() + "\r\n";

  if (header.size() > 0)
    packet += "\r\n";

  if (body.length() == 0)
    return packet;

  return packet;
}

inline std::string
Http1Response::packet() const
{
  std::string packet =
      version_str() + " " + std::to_string(status_) + " " + status_code_to_string(status_) + "\r\n" +
      header.to_string() + "\r\n";

  if (header.size() > 0)
    packet += "\r\n";

  if (body.length() == 0)
    return packet;

//  if (header.contains("content-length") == false)
//    return packet + body + "\r\n\r\n";

  return packet + body;
}

inline std::string
Http1Response::packet(const bool &with_content_length)
{
  if (with_content_length == true && header.count("content-length") == 0)
    header.set("content-length", std::to_string(body.length()));

  std::string packet =
      version_str() + " " + std::to_string(status_) + " " + status_code_to_string(status_) + "\r\n" +
      header.to_string() + "\r\n";

  if (header.size() > 0)
    packet += "\r\n";

  if (body.length() == 0)
    return packet;

  if (with_content_length == false)
    return packet + body + "\r\n\r\n";

  return packet + body;
}

inline std::optional<Http1Response>
Http1Response::parse(const std::string_view &message)
{
  return Http1Protocol<Http1Response>::parse(message, Http1Response::parse_parameter);
}

inline std::optional<bool>
Http1Response::parse_parameter(Http1Response &h1, const std::string_view &parameter_line)
{
  if (parameter_line.length() > 256)
    return false;

  //  HTTP/1.1 200 OK
  std::string_view::size_type version_end = parameter_line.find(' ');
  if (version_end == std::string::npos)
    return false;

  std::string_view::size_type status_end  = parameter_line.find(' ', version_end+1);
  if (status_end == std::string::npos)
    return false;

  if (parameter_line.substr(0, version_end) != "HTTP/1.1" && parameter_line.substr(0, version_end) != "HTTP/1.0")
    return false;

  std::string_view status = parameter_line.substr(version_end+1, status_end-(version_end+1));
  for (size_t index = 0; index < status.length(); ++index)
    if (std::isdigit(status.at(index)) == 0)
      return false;

  h1.status_ = std::atoi(std::string(status).c_str());

  return true;
}

}

#endif /* IO_REACTOR_HTTPS_REACTOR_HTTP1RESPONSE_H_ */
