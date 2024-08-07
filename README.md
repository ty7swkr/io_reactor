## 설명
- 의존성 없는 C++ 멀티쓰레드 고성능 네트워크 I/O를 목표로 작성(epoll)
- 오픈 소스의 제약조건(boost버전 혹은 openssl버전 혹은 메소드 제약) 문제로 작성
- 기타 오픈 소스의 고급 기능 사용시 비용 문제로 작성
- HTTP 1.1, WEBSOCKET, SSL, Non-SSL 지원
- 비동기 멀티쓰레드 서버 및 비동기 클라이언트 프레임웍
- HTTP 1.1에서 WEBSOCKET으로의 전환
- 코드 호환을 위헤 C++17은 부분적으로 사용함

## 요구사항
- g++ - c++ 17지원되는 컴파일러
- CentOS 7이상, Ubuntu 18이상
- OpenSSL 1.0.x or 1.1.x

## 디렉토리
- http1_client      : HTTP 1.1 클라이언트 프레임웍
- http1_protocol    : HTTP 1.1 파서&빌더
- http1_reactor     : HTTP 1.1 비보안 멀티쓰레드 Async 서버 프레임웍
- https1_client     : HTTP 1.1 보안(SSL) Async 클라이언트
- https1_rector     : HTTP 1.1 보안(SSL) 멀티쓰레드 Async 서버 프레임웍
- reactor           : TCP/IP REACTOR패턴 적용 프레임웍(IO 멀티플렉싱)
- ssl_reactor       : REACTOR + ACCEPTOR 보안(SSL) TCP/IP 적용 서버 프레임웍
- tcp_reactor       : REACTOR + ACCEPTOR 비보안    TCP/IP 적용 서버 프레임웍
- tcp_async_client  : TCP/IP 보안(SSL) 및 비보안 ASYNC Client 프레임웍
- websocket         : 웹소켓 v13 파서&빌더 - 오픈소스 수정.
- example
  - async_client    : 비동기 기반 TCP/IP ASYNC CLIENT
  - complex         : TCP/IP 서버
  - simple          : 간단한 TCP/IP 서버
  - wsclient        : 비보안 Websocket Client
  - wssclient       : 보안(SSL) Websocket Client
  - wsserver        : 비보안 Websocket Server
  - wssserver       : 보안(SSL) Websocket Server
