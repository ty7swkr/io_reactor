/*
 * EventHandlerAssistant.h
 *
 *  Created on: 2022. 9. 10.
 *      Author: tys
 */

#ifndef IO_REACTOR_REACTOR_EVENTHANDLERATTR_H_
#define IO_REACTOR_REACTOR_EVENTHANDLERATTR_H_

#include <reactor/EventHandler.h>

namespace reactor
{

class EventHandlerAttr
{
public:
  static io_handle_t &io_handle(EventHandler *handler) { return handler->io_handle_; }
};

}

#endif /* THIRD_PARTY_IO_REACTOR_REACTOR_EVENTHANDLERASSISTANT_H_ */
