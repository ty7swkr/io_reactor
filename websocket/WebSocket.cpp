/*
 * WebSocket.cpp
 *
 *  Created on: 2023. 2. 16.
 *      Author: tys
 */
#include "WebSocket.h"
#include "sha1/sha1.h"

#include <chrono>
#include <string.h>

WebSocket
WebSocket::make(const uint8_t &op_code,
                const uint8_t *payload,
                const size_t  &payload_size,
                const bool    &masking,
                const bool    &last)
{
  WebSocket web_socket;
  web_socket.made_ = true;

  size_t header_size = 2 + (masking == false ? 0 : 4);

  // Extented data length
  if      (payload_size <= 125)   header_size += 0;
  else if (payload_size <= 65535) header_size += 2;
  else                            header_size += 8;

  web_socket.payload_pos_ = header_size;
  web_socket.payload_size_= payload_size;
  web_socket.buffer_.resize(header_size + payload_size);

  web_socket.fin_         =   (last == true ? 1: 0);
  web_socket.opcode_      =   op_code;

  web_socket.mask_        =   0;
  web_socket.mask_key_    =   0;
  if (masking == true)
  {
    web_socket.mask_      =   1;
    web_socket.mask_key_  =   std::chrono::duration_cast<std::chrono::nanoseconds>
                              (std::chrono::steady_clock::now().time_since_epoch()).count();
  }

  web_socket.buffer_[0]   =   0;
  web_socket.buffer_[0]   |=  (web_socket.fin_ << 7);
  web_socket.buffer_[0]   |=  web_socket.opcode_;

  web_socket.buffer_[1]   =   0;
  web_socket.buffer_[1]   |=  (web_socket.mask_ << 7);

  // Extented data length
  if (payload_size <= 125)
  {
    web_socket.length_    =   payload_size;
    web_socket.buffer_[1] |=  web_socket.length_;
  }
  else if (payload_size <= 65535)
  {
    web_socket.length_    =   126;
    web_socket.buffer_[1] |=  web_socket.length_;
    web_socket.buffer_[2] =   (payload_size >> 8) & 0xFF;
    web_socket.buffer_[3] =   payload_size & 0xFF;
  }
  else
  {
    web_socket.length_    =   127;
    web_socket.buffer_[1] |=  web_socket.length_;

    size_t pos = 2;
    for (int index = 7; index >= 0; --index) // 4
      web_socket.buffer_[pos++] = ((payload_size >> 8 * index) & 0xFF);
  }

  if (web_socket.mask_ == 1)
  {
    memcpy(web_socket.buffer_.data()+web_socket.payload_pos_-sizeof(web_socket.mask_key_),
           &web_socket.mask_key_,
           sizeof(web_socket.mask_key_));

    for (size_t index = 0; index < payload_size; ++index)
      web_socket.buffer_[web_socket.payload_pos_+index] =
          payload[index] ^ ((unsigned char *)(&web_socket.mask_key_))[index % 4];

    return web_socket;
  }

  memcpy(web_socket.buffer_.data()+web_socket.payload_pos_, payload, payload_size);
  return web_socket;
}

WebSocket
WebSocket::parse(const uint8_t *buffer, const size_t &size)
{
  if (size < 2)
    throw WebSocketException(WebSocketException::INCOMPLETE_HEADER);

  WebSocket web_socket;

  web_socket.payload_pos_ = 2;

  web_socket.fin_    = (buffer[0] >> 7) & 0x01;
  web_socket.opcode_ =  buffer[0]       & 0x0F;
  web_socket.mask_   = (buffer[1] >> 7) & 0x01;
  web_socket.length_ =  buffer[1]       & (~0x80);

  if (web_socket.length_ <= 125)
  {
    web_socket.payload_size_ = web_socket.length_;
  }
  else if (web_socket.length_ == 126)
  {
    web_socket.payload_pos_ += 2;

   if (web_socket.payload_pos_ < 4)
      throw WebSocketException(WebSocketException::INCOMPLETE_HEADER);

    web_socket.payload_size_ = ((buffer[2] << 8) | (buffer[3]));
  }
  else if (web_socket.length_ == 127)
  {
    web_socket.payload_pos_ += 8;

    if (size < web_socket.payload_pos_)
      throw WebSocketException(WebSocketException::INCOMPLETE_HEADER);

    web_socket.payload_size_ =
        (((uint64_t)buffer[2] << 56) | ((uint64_t)buffer[3] << 48) | ((uint64_t)buffer[4] << 40) |
         ((uint64_t)buffer[5] << 32) | ((uint64_t)buffer[6] << 24) | ((uint64_t)buffer[7] << 16) |
         ((uint64_t)buffer[8] <<  8) | ((uint64_t)buffer[9]));
  }

  if (web_socket.mask_ == 1)
  {
    web_socket.payload_pos_ += 4;

    if (size < web_socket.payload_pos_)
      throw WebSocketException(WebSocketException::INCOMPLETE_HEADER);

    web_socket.mask_key_ = *(uint32_t *)(buffer + web_socket.payload_pos_-4);
  }

  if (size < web_socket.payload_pos_ + web_socket.payload_size_)
    throw WebSocketException(WebSocketException::INCOMPLETE_PAYLOAD);

  web_socket.buffer_.clear();

  if (web_socket.mask_ == 0)
  {
    web_socket.buffer_.insert(web_socket.buffer_.end(),
                              buffer, buffer+web_socket.payload_pos_+web_socket.payload_size_);
    return web_socket;
  }

  web_socket.buffer_.insert(web_socket.buffer_.end(),
                            buffer, buffer+web_socket.payload_pos_);

  for (size_t index = 0; index < web_socket.payload_size_; ++index)
    web_socket.buffer_.push_back(buffer[web_socket.payload_pos_+index] ^
                                 ((unsigned char *)(&web_socket.mask_key()))[index % 4]);

  return web_socket;
}

std::string
WebSocket::sec_accept_key(const std::string &sec_websocket_key)
{
  std::string accept_key = sec_websocket_key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"; //RFC6544_MAGIC_KEY

  unsigned char digest[20]; // 160 bit sha1 digest
//  SHA1((const unsigned char *)accept_key.data(),
//       accept_key.size(),
//       digest);

  SHA1 sha;
  sha.Input(accept_key.data(), accept_key.size());
  sha.Result((unsigned*)digest);

  for (size_t index = 0; index < sizeof(digest); index += 4)
  {
    unsigned char c;

    c = digest[index];
    digest[index]   = digest[index+3];
    digest[index+3] = c;

    c = digest[index+1];
    digest[index+1] = digest[index+2];
    digest[index+2] = c;
  }

  return base64_encode((const unsigned char *)digest, sizeof(digest));
}

