/*
 * Http1Protocol.h
 *
 *  Created on: 2023. 2. 23.
 *      Author: tys
 */

#ifndef IO_REACTOR_HTTP_PROTOCOL_HTTP1PROTOCOL_H_
#define IO_REACTOR_HTTP_PROTOCOL_HTTP1PROTOCOL_H_

#include <http1_protocol/HttpHeader.h>
#include <http1_protocol/HttpVersion.h>
#include <reactor/trace.h>

#include <optional>
#include <string_view>
#include <functional>

namespace https_reactor
{

using namespace reactor;

template<typename T>  // Curiously Recursing Template Pattern
class Http1Protocol
{
public:
  Http1Protocol() {}
  Http1Protocol(const int32_t     &stream_id,
                const HttpHeader  &header = {{}},
                const std::string &body = "")
  : stream_id(stream_id), header(header), body(body) {}

  int32_t       stream_id = -1;
  HTTP_VERSION  version   = HTTP_VERSION_11;
  HttpHeader    header;
  std::string   body;

  const std::string &
  raw_message() const { return raw_message_; }

  std::string
  version_str () const { return version_to_string(version); }

protected:
  static std::optional<T>
  parse(const std::string_view &message,
        std::function<std::optional<bool>(T &h1, const std::string_view &parameter_line)> parse_parameter_func);

  static std::optional<bool>
  parse_header(HttpHeader &header, const std::string_view &header_lines);

  static std::optional<bool>
  parse_body(Http1Protocol<T> &h1, const std::string_view &message);

  static std::string
  trimmed(const std::string_view &s, const char *chars = " ");

protected:
  std::string raw_message_;
};

template<typename T> std::optional<T>
Http1Protocol<T>::parse(const std::string_view &message,
                        std::function<std::optional<bool>(T &h1, const std::string_view &parameter_line)> parse_parameter_func)
{
  std::string_view::size_type parameter_end = message.find("\r\n");
  if (parameter_end == std::string::npos)
     return std::nullopt;

  T h1;
  auto result = parse_parameter_func(h1, message.substr(0, parameter_end));

  // incomplete
  if (result == std::nullopt)
    return std::nullopt;

  if (result.value() == false)
//    throw std::runtime_error("Invalid protocol parameter: " + std::string(message.substr(0, parameter_end)));
    throw std::runtime_error("Invalid protocol parameter: " + std::string(message));

  std::string_view::size_type header_end = message.find("\r\n\r\n");
  if (header_end == std::string::npos)
     return std::nullopt;

  // has header
  if (parameter_end < header_end)
  {
    auto result = Http1Protocol::parse_header(h1.header,
                                              message.substr(parameter_end+2,
                                                             header_end+2-(parameter_end+2)));
    // incomplete
    if (result == std::nullopt)
      return std::nullopt;

    if (result.value() == false)
      throw std::runtime_error("Invalid header");
  }

  result = parse_body(h1, message.substr(header_end+4));

  // incomplete
  if (result == std::nullopt)
    return std::nullopt;

  if (result.value() == false)
    throw std::runtime_error("Invalid body");

  h1.raw_message_ = message.substr(0, header_end+4+h1.body.length());

  return h1;
}

template<typename T> std::optional<bool>
Http1Protocol<T>::parse_header(HttpHeader &header, const std::string_view &header_lines)
{
  if (header_lines.length() < 2)
    return std::nullopt;

  std::string_view::size_type pos_sta = 0;
  std::string_view::size_type pos_end = 0;
  while ((pos_end = header_lines.find("\r\n", pos_sta)) != std::string_view::npos)
  {
    auto line = header_lines.substr(pos_sta, pos_end-pos_sta);

    auto name_end = line.find(": ");
    if (name_end == std::string_view::npos)
      return false;

//    auto name   = line.substr(0, name_end);
//    auto value  = line.substr(name_end+2);
//    if (values.find(',') != std::string_view::npos)
//    {
//      std::string_view::size_type values_sta = 0;
//      std::string_view::size_type values_end = 0;
//      while ((values_end = values.find(',', values_sta)) != std::string_view::npos)
//      {
//        std::string value = Http1Protocol::trimmed(std::string(values.substr(values_sta, values_end-values_sta)));
//        header.add(std::string(name), std::move(value));
//        values_sta = values_end+1;
//      }
//
//      reactor_trace << values_sta << values.length();
//      reactor_trace << values.substr(values_sta) << "]";
//
//      std::string value = Http1Protocol::trimmed(values.substr(values_sta));
//      header.add(std::string(name), std::move(value));
//
//      pos_sta = pos_end+2;
//      continue;
//    }

    header.add(std::string(line.substr(0, name_end)), std::string(line.substr(name_end+2)));
    pos_sta = pos_end+2;
  }

  return true;
}

template<typename T> std::optional<bool>
Http1Protocol<T>::parse_body(Http1Protocol<T> &h1, const std::string_view &message)
{
  if (h1.header.contains("content-length") == true)
  {
    size_t content_length = h1.header.get<size_t>("content-length");

    // not enought
    if (message.length() < content_length)
      return std::nullopt;

    h1.body = message.substr(0, content_length);
    return true;
  }

  if (h1.header.contains("transfer-encoding", "chunked", true) == true)
  {
    std::string_view::size_type min_size = 1000;
    if (message.size() < min_size) min_size = message.size();

    std::string_view::size_type body_end = message.substr(message.size()-min_size).rfind("\r\n0\r\n\r\n");
    if (body_end == std::string::npos)
      return std::nullopt;

    h1.body = message.substr(0, body_end+(message.size()-min_size)+2+1+4);
    return true;
  }

  h1.body = message;
  return true;
}

template<typename T> std::string
Http1Protocol<T>::trimmed(const std::string_view &s, const char *chars)
{
  std::string_view::size_type const first = s.find_first_not_of(chars);
  return (first == std::string::npos)
         ? std::string() : std::string(s.substr(first, s.find_last_not_of(chars)-first+1));
};

}

#endif /* IO_REACTOR_HTTP_PROTOCOL_HTTP1PROTOCOL_H_ */
