/*
 * Http1Request.h
 *
 *  Created on: 2020. 2. 20.
 *      Author: tys
 */

#ifndef HTTPPROTOCOL_HTTP1REQUEST_H_
#define HTTPPROTOCOL_HTTP1REQUEST_H_

#include <http1_protocol/Http1Protocol.h>
#include <http1_protocol/HttpRequestData.h>
#include <websocket/WebSocket.h>
#include <reactor/trace.h>

#include <string_view>
#include <optional>
#include <algorithm>
#include <string.h>

namespace https_reactor
{

using namespace reactor;

//  PUT /files/129742 HTTP/1.1\r\n
//  Host: example.com\r\n
//  User-Agent: Chrome/54.0.2803.1\r\n
//  Content-Length: 202\r\n
//  \r\n
//  This is a message body. All content in this message body should be stored under the
//  /files/129742 path, as specified by the PUT specification. The message body does
//  not have to be terminated with CRLF.\r\n

class Http1Request final : public Http1Protocol<Http1Request>
{
public:
  Http1Request () {}
  Http1Request (const int32_t     &stream_id,
                const std::string &method,
                const std::string &path,
                const HttpHeader  &header = {},
                const std::string &body   = "",
                const bool        &content_length = true)
  : Http1Protocol (stream_id, header, body),
    method_(method), path_(path), content_length_(content_length) {}

  Http1Request (const int32_t     &stream_id,
                const std::string &method,
                const std::string &path,
                const std::string &path_arg,
                const HttpHeader  &header   = {},
                const std::string &body     = "",
                const bool        &content_length = true)
  : Http1Protocol (stream_id, header, body),
    method_(method), path_(path), path_arg_(path_arg),
    content_length_(content_length) {}

  virtual ~Http1Request() {}

  static Http1Request
  GET(const int32_t     &stream_id,
      const std::string &path,
      const std::string &path_arg,
      const HttpHeader  &header = {},
      const std::string &body   = "",
      const bool        &content_length = false)
  {
    return Http1Request(stream_id, "GET", path, path_arg, header, body, content_length);
  }

  static Http1Request
  GET(const int32_t     &stream_id,
      const std::string &path,
      const std::initializer_list<std::pair<std::string, std::string>> &args,
      const HttpHeader  &header = {},
      const std::string &body   = "",
      const bool        &content_length = false)
  {
    return Http1Request(stream_id, "GET", path, make_path_arg(args), header, body, content_length);
  }

  // protocol parameter
  const std::string &path       () const { return path_;    }
  const std::string &path_arg   () const { return path_arg_;}
  const std::string &method     () const { return method_;  }

  HttpRequestData   data              () const;
  bool              should_keep_alive () const;

  std::string       get_sec_websocket_key () const;
  bool              is_upgrade_wabsocket  () const;


  std::string
  packet() const;

  std::string
  packet(const bool &with_content_length = true);

  static std::optional<Http1Request>
  parse(const std::string_view &request);

  static Http1Request
  upgrade_websocket(const int32_t     &stream_id,
                    const std::string &path,
                    const std::string &path_arg = "",
                    const HttpHeader  &header   = {});

protected:

  static std::string
  make_path_arg(const std::initializer_list<std::pair<std::string, std::string>> &args)
  {
    std::string path_arg;
    for (const auto &arg : args)
    {
      if (path_arg.size() > 0) path_arg += "&";
      path_arg += arg.first + "=" + arg.second;
    }

    return path_arg;
  }

  static std::optional<bool>
  parse_parameter(Http1Request &h1, const std::string_view &parameter_line);

protected:
  // protocol parameter
  std::string method_;
  std::string path_;
  std::string path_arg_;
  bool        content_length_ = true;

  friend class Http1Protocol<Http1Request>;
};

inline std::optional<Http1Request>
Http1Request::parse(const std::string_view &request)
{
  return Http1Protocol<Http1Request>::parse(request, Http1Request::parse_parameter);
}

inline std::string
Http1Request::packet() const
{
  std::string path = path_;
  if (path_arg_.length() > 0)
    path += "?" + path_arg_;

  std::string packet =
      method_ + " " + path + " " + version_to_string(version) + "\r\n" +
      header.to_string() + "\r\n";

  if (content_length_ == true && header.count("content-length") == 0)
    packet += "content-length: " + std::to_string(body.length()) + "\r\n";

  if (header.size() > 0)
    packet += "\r\n";

  if (body.length() == 0)
    return packet;

  return packet + body;
}

inline std::string
Http1Request::packet(const bool &with_content_length)
{
  std::string path = path_;
  if (path_arg_.length() > 0)
    path += "?" + path_arg_;

  if (with_content_length == true && header.count("content-length") == 0)
    header.set("content-length", std::to_string(body.length()));

  std::string packet =
      method_ + " " + path + " " + version_to_string(version) + "\r\n" +
      header.to_string() + "\r\n";

  if (header.size() > 0)
    packet += "\r\n";

  if (body.length() == 0)
    return packet;

//  if (with_content_length == false)
//    return packet + body + "\r\n\r\n";

  return packet + body;
}

inline Http1Request
Http1Request::upgrade_websocket(const int32_t     &stream_id,
                                const std::string &path,
                                const std::string &path_arg,
                                const HttpHeader  &header)
{
  uint8_t bytes[16];
  uint64_t *bytes1 = (uint64_t *) bytes;
  uint64_t *bytes2 = (uint64_t *)(bytes+8);

  *bytes1 = std::chrono::duration_cast<std::chrono::nanoseconds>
            (std::chrono::steady_clock::now().time_since_epoch()).count();
  *bytes2 = std::chrono::duration_cast<std::chrono::nanoseconds>
            (std::chrono::steady_clock::now().time_since_epoch()).count();

  std::string sec_websocket_key = base64_encode(bytes, sizeof(bytes));

  Http1Request request(stream_id, "GET", path, path_arg);
  request.header = header;
  request.header.set("connection",            "Upgrade");
  request.header.set("upgrade",               "websocket");
  request.header.set("sec-websocket-version", "13");
  request.header.set("sec-websocket-key",     sec_websocket_key);

  return request;
}

inline std::string
Http1Request::get_sec_websocket_key() const
{
  return header.get<std::string>("Sec-WebSocket-Key");
}

inline bool
Http1Request::is_upgrade_wabsocket() const
{
  if (HttpHeader::to_lower(header.get<std::string>("connection")) != "upgrade")   return false;
  if (HttpHeader::to_lower(header.get<std::string>("upgrade"))    != "websocket") return false;
  if (header.get<std::string>("sec-websocket-version")            != "13")        return false;
  if (header.contains("sec-websocket-key")                        == false)       return false;
  return true;
}

inline bool
Http1Request::should_keep_alive() const
{
  return header.contains("connection", "close", true);
}

inline HttpRequestData
Http1Request::data() const
{
  HttpRequestData data;
  data.version        = version;
  data.version_string = version_to_string(data.version);
  data.path           = path();
  data.path_arg       = path_arg();
  data.method         = method();
  data.header         = header;
  data.body           = body;

  return data;
}

inline std::optional<bool>
Http1Request::parse_parameter(Http1Request &h1, const std::string_view &parameter_line)
{
  if (parameter_line.length() > 256)
    return false;

  //  PUT /files/129742 HTTP/1.1
  std::string_view::size_type method_end  = parameter_line.find(' ');
  if (method_end == std::string::npos)
    return false;

  std::string_view::size_type path_end    = parameter_line.find(' ', method_end+1);
  if (path_end == std::string::npos)
    return false;

  h1.method_ = parameter_line.substr(0, method_end);
  h1.path_   = parameter_line.substr(method_end+1, path_end-(method_end+1));

  std::string_view version = parameter_line.substr(path_end+1,
                                                   parameter_line.length()-(path_end+1));

  if (version != "HTTP/1.1" && version != "HTTP/1.0")
    return false;

  std::string::size_type pos = h1.path_.find('?');
  if (pos != std::string::npos)
  {
    h1.path_arg_= h1.path_.substr(pos+1);
    h1.path_    = h1.path_.substr(0, pos);
  }

  return true;
}

}

#endif /* http_rserver_HttpRequest_h */
