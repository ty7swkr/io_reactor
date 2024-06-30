/*
 * WebSocket.h
 *
 *  Created on: 2021. 11. 29.
 *      Author: tys
 */

#ifndef IO_REACTOR_WEB_SOCKET_WEBSOCKETEXCEPTION_H_
#define IO_REACTOR_WEB_SOCKET_WEBSOCKETEXCEPTION_H_

#include <vector>
#include <string>
#include <stdint.h>

class WebSocketException
{
public:
  enum
  {
    INCOMPLETE_HEADER   = -1,
    INCOMPLETE_PAYLOAD  = -2,
    WRONG_OPCODE        = -10,
    UNKNOWN             = -999
  };

  WebSocketException(const int &code, const std::string &reason = "")
  : code(code), reason_(reason) {}

  int code;
  std::string what() const
  {
    std::string code_string = "Unknown";
    switch (code)
    {
      case INCOMPLETE_HEADER  : code_string = "Incomplete Header";  break;
      case INCOMPLETE_PAYLOAD : code_string = "Incomplete Payload"; break;
      case WRONG_OPCODE       : code_string = "Wrong opcode";       break;
      case UNKNOWN            : code_string = "Unknown";            break;
    }

    if (reason_.length() == 0)
      return code_string + "(" + std::to_string(code) + ") ";

    return code_string + "(" + std::to_string(code) + ") " + reason_;
  }

private:
  std::string reason_;
};


#endif /* WEB_SOCKET_WEBSOCKET_H_ */
