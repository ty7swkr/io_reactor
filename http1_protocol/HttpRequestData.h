/*
 * HttpRequest.h
 *
 *  Created on: 2021. 1. 11.
 *      Author: tys
 */

#ifndef HTTPPROTOCOL_HTTPREQUESTDATA_H_
#define HTTPPROTOCOL_HTTPREQUESTDATA_H_

#include <http1_protocol/HttpHeader.h>
#include <http1_protocol/HttpVersion.h>
#include <unordered_map>
#include <string>

namespace https_reactor
{

class HttpRequestData
{
public:
  virtual ~HttpRequestData() {}

  HTTP_VERSION  version;
  std::string   version_string;
  std::string   path;     // path?arg....
  std::string   path_arg; // arg....
  std::string   method;
  HttpHeader    header;
  std::string   body;
};

}

#endif /* http_rserver_HttpRequest_h */
