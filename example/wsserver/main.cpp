/*
 * main.cpp
 *
 *  Created on: 2021. 7. 26.
 *      Author: tys
 */

#include "WsHandlerFactory.h"
#include <reactor/trace.h>

#include <string.h>
#include <signal.h>

TCPReactor        listener;
WsHandlerFactory  factory;

void sig_handler(int signum)
{
  listener.stop();
}

int main(void)
{
  signal(SIGPIPE, SIG_IGN);
  signal(SIGHUP,  sig_handler);
  signal(SIGINT,  sig_handler);


  reactor_trace;
  listener.set_acceptor_ipv46(&factory, 2000, "::0", 2, 1000);

  reactor_trace << listener.set_reactor(4);
  reactor_trace << listener.start();

  listener.wait();
  reactor_trace;

  return 0;
}






