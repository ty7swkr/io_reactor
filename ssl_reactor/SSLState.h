/*
 * SSLState.h
 *
 *  Created on: 2022. 9. 15.
 *      Author: tys
 */

#ifndef IO_REACTOR_SSL_REACTOR_SSLSTATE_H_
#define IO_REACTOR_SSL_REACTOR_SSLSTATE_H_

#include <string>

namespace reactor
{

enum class SSL_STATE : int
{
  NONE   = 0,
  READ,
  WRITE
};

inline std::string
to_string(const SSL_STATE &state)
{
  switch (state)
  {
    case SSL_STATE::NONE  : return "SSL_STATE:NONE";
    case SSL_STATE::READ  : return "SSL_STATE:READ";
    case SSL_STATE::WRITE : return "WRITE:NONE";
    default: return "SSL_STATE:UNKNOWN";
  }
}

}

#endif /* SSL_REACTOR_SSLSTATE_H_ */
