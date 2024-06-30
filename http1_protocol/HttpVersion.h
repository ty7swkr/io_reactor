/*
 * HttpVersion.h
 *
 *  Created on: 2020. 9. 24.
 *      Author: tys
 */

#ifndef HTTPPROTOCOL_REACTOR_HTTPVERSION_H_
#define HTTPPROTOCOL_REACTOR_HTTPVERSION_H_

namespace https_reactor
{

typedef enum
{
  HTTP_VERSION_NONE = 0,
  HTTP_VERSION_11   = 1,  // 1.1
  HTTP_VERSION_2    = 2,  // 2
} HTTP_VERSION;

inline
std::string version_to_string(const HTTP_VERSION &version)
{
  if (version == HTTP_VERSION_11) return "HTTP/1.1";
  if (version == HTTP_VERSION_2)  return "HTTP/2";
  return "http/?";
}

}

#endif /* http_reactor_HttpVersion_h */
