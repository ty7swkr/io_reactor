/*
 * WebSocket.h
 *
 *  Created on: 2023. 2. 16.
 *      Author: tys
 */

#ifndef IO_REACTOR_WEB_SOCKET_WEBSOCKET_H_
#define IO_REACTOR_WEB_SOCKET_WEBSOCKET_H_

#include <websocket/WebSocketException.h>
#include <websocket/base64/base64.h>
#include <string>

class WebSocket
{
public:
  typedef enum
  {
    OPCODE_CONTINUED = 0x0,
    OPCODE_TEXT      = 0x1,
    OPCODE_BINARY    = 0x2,
    OPCODE_CLOSE     = 0x8,
    OPCODE_PING      = 0x9,
    OPCODE_PONG      = 0xA
  } OPCODE_TYPE;

  const uint8_t   &fin()            const { return fin_;          }
  const uint8_t   &opcode()         const { return opcode_;       }
  bool            mask()            const { return mask_ == 1;    }
  const uint32_t  &mask_key()       const { return mask_key_;     }
  const uint64_t  &header_length()  const { return payload_pos_;  }
  const uint64_t  &payload_length() const { return payload_size_; }
        size_t     size()           const { return buffer_.size();}

  bool  is_continue() const { return fin_     == 0; }
  bool  is_close()    const { return opcode_  == OPCODE_CLOSE; }
  bool  is_ping ()    const { return opcode_  == OPCODE_PING;  }
  bool  is_pong ()    const { return opcode_  == OPCODE_PONG;  }

  std::string
  payload_to_string() const
  {
    if (mask() == true)
    {
      if (made_ == true)
      {
        std::string payload;
        for (size_t index = 0; index < payload_size_; ++index)
          payload.push_back(buffer_[payload_pos_+index] ^ ((uint8_t *)(&mask_key()))[index % 4]);
        return payload;
      }
    }

    return std::string(
      reinterpret_cast<const char *>(buffer_.data()+payload_pos_),
      reinterpret_cast<const char *>(buffer_.data()+payload_pos_+payload_size_));
  }

  const std::vector<uint8_t> &
  packet() const { return buffer_; }

  std::string to_string() const;

  static WebSocket
  parse     (const uint8_t *buffer, const size_t &size);

  static WebSocket
  make      (const char *payload, const bool &masking = false, const bool &last = true);

  static WebSocket
  make      (const std::string &payload, const bool &masking = false, const bool &last = true);

  static WebSocket
  make      (const std::string_view &payload, const bool &masking = false, const bool &last = true);

  static WebSocket
  make      (const uint8_t *payload, const size_t &payload_size, const bool &masking = false, const bool &last = true);

  static WebSocket
  make_ping (const bool &masking = true);

  static WebSocket
  make_pong (const bool &masking = false);

  static WebSocket
  make_close(const std::string &payload = "", const bool &masking = false);

  static WebSocket
  make      (const uint8_t  &op_code,
             const uint8_t  *payload, const size_t &payload_size,
             const bool     &mask_key = false,
             const bool     &last     = true);

  static std::string
  sec_accept_key(const std::string &sec_websocket_key);

private:
  static bool validate_opcode(const uint8_t &opcode)
  {
    switch (opcode)
    {
      case OPCODE_CONTINUED:  return true;
      case OPCODE_TEXT:       return true;
      case OPCODE_BINARY:     return true;
      case OPCODE_CLOSE:      return true;
      case OPCODE_PING:       return true;
      case OPCODE_PONG:       return true;
      default: return false;
    }
  }

private:
  uint8_t   fin_          = 0;
  uint8_t   opcode_       = OPCODE_TEXT;
  uint8_t   mask_         = 0;
  uint32_t  mask_key_     = 0;
  uint8_t   length_       = 0;
  uint64_t  payload_pos_  = 0;
  uint64_t  payload_size_ = 0;
  bool      made_         = false;

private:
  std::vector<uint8_t> buffer_;
};

inline WebSocket
WebSocket::make(const uint8_t *payload,
                const size_t  &payload_size,
                const bool    &masking,
                const bool    &last)
{
  return WebSocket::make(WebSocket::OPCODE_BINARY, payload, payload_size, masking, last);
}

inline WebSocket
WebSocket::make(const char *payload,
                const bool &masking,
                const bool &last)
{
  return WebSocket::make(std::string_view(payload),
                         masking, last);
}

inline WebSocket
WebSocket::make(const std::string  &payload,
                const bool         &masking,
                const bool         &last)
{
  return WebSocket::make(WebSocket::OPCODE_TEXT,
                         reinterpret_cast<const uint8_t *>(payload.data()),
                         payload.size(),
                         masking, last);
}

inline WebSocket
WebSocket::make(const std::string_view  &payload,
                const bool              &masking,
                const bool              &last)
{
  return WebSocket::make(WebSocket::OPCODE_TEXT,
                         reinterpret_cast<const uint8_t *>(payload.data()),
                         payload.size(),
                         masking, last);
}

inline WebSocket
WebSocket::make_ping(const bool &masking)
{
  return WebSocket::make(WebSocket::OPCODE_PING, nullptr, 0, masking, true);
}

inline WebSocket
WebSocket::make_pong(const bool &masking)
{
  return WebSocket::make(WebSocket::OPCODE_PONG, nullptr, 0, masking, true);
}

inline WebSocket
WebSocket::make_close(const std::string &payload,
                      const bool        &masking)
{
  return WebSocket::make(WebSocket::OPCODE_CLOSE,
                         reinterpret_cast<const uint8_t *>(payload.data()),
                         payload.size(), masking, true);
}

inline std::string
WebSocket::to_string() const
{
  std::string opcode_str = "UNKNOWN";
  switch (opcode_)
  {
    case OPCODE_CONTINUED:  opcode_str = "CONTINUED"; break;
    case OPCODE_TEXT:       opcode_str = "TEXT";      break;
    case OPCODE_BINARY:     opcode_str = "BINARY";    break;
    case OPCODE_CLOSE:      opcode_str = "CLOSE";     break;
    case OPCODE_PING:       opcode_str = "PING";      break;
    case OPCODE_PONG:       opcode_str = "PONG";      break;
    default: opcode_str = "UNKNOWN"; break;
  }

  std::string result;
  result += "HEADER_SIZE:"    + std::to_string(payload_pos_)  + ",";
  result += "FIN:"            + std::to_string(fin_)          + ",";
  result += "OPCODE:"         + opcode_str                    + ",";
  result += "MASK:"           + std::to_string(mask_)         + ",";
  result += "PAYLOAD_LEN:"    + std::to_string(length_);
  result += "(" + std::to_string(payload_size_)               + "),";
  result += "MASK_KEY:"       + std::to_string(mask_key_);
  return result;
}


#endif /* LIBS_IO_REACTOR_WEB_SOCKET_WEBSOCKET_H_ */
