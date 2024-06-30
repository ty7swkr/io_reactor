/*
 * trace.h
 *
 *  Created on: 2020. 2. 11.
 *      Author: tys
 */

#ifndef IO_REACTOR_REACTOR_TRACE_H_
#define IO_REACTOR_REACTOR_TRACE_H_

#include <chrono>
#include <string>
#include <sstream>
#include <iomanip>
#include <iostream>

#ifndef reactor_trace
//#define reactor_trace std::cout << __FILE__ << ":" << __LINE__ << ":" << __FUNCTION__ << ":" << ": "
#define reactor_trace SimpleTrace(__FILE__, __LINE__, __FUNCTION__)
#endif

namespace reactor
{

class SimpleTrace
{
public:
  SimpleTrace(const char *filename, const int &line, const char *function)
  : filename_(filename), line_(line), function_(function)
  {
    std::string::size_type pos = filename_.rfind('/');
    if (pos != std::string::npos)
      filename_ = filename_.substr(pos+1);
  }

  virtual ~SimpleTrace()
  {
    std::cout << std::uppercase << std::setw(16) << std::setfill('0') << std::hex << (uint64_t)pthread_self() << " "
              << std::dec << current() << " "
              << filename_ << ":"
              << std::setw(3)  << std::setfill(' ') << std::right << line_ << std::left
              << std::setw(0)  << "| "
              << std::setw(15) << function_
              << std::setw(0)  << " " << message_.str() << std::endl;
  }

  template<typename T> SimpleTrace &
  operator<<(T mesg)
  {
    if (message_.str().length() > 0)
      message_ << " ";

    message_ << mesg;
    return *this;
  }

  SimpleTrace &
  operator<<(const bool &mesg)
  {
    mesg == true ? message_ << " true" : message_ << " false";
    return *this;
  }

  SimpleTrace &
  operator<<(std::ostream& (*rhs)(std::ostream&))
  {
    (void)rhs;
    return *this;
  }

protected:
  std::string current()
  {

    auto    now        = std::chrono::system_clock::now();
    auto    in_time_t  = std::chrono::system_clock::to_time_t(now);
    int64_t msecs      = std::chrono::duration_cast<std::chrono::milliseconds>
                         (now.time_since_epoch()).count() - (in_time_t * 1000);

    std::stringstream ss;
    ss << std::put_time(std::localtime(&in_time_t), "%y-%m-%d %H:%M:%S.") << std::setw(3) << std::setfill('0') << msecs;
    return ss.str();
  }

private:
  std::string filename_;
  const int   &line_;
  const char  *function_;
  std::stringstream  message_;
};

}

#endif /* IO_REACTOR_REACTOR_TRACE_H_ */
