/*
 * ObjectTimer.h
 *
 *  Created on: 2020. 1. 31.
 *      Author: tys
 */

#ifndef IO_REACTOR_REACTOR_OBJECTSTIMER_H_
#define IO_REACTOR_REACTOR_OBJECTSTIMER_H_

#include <reactor/DefinedType.h>
#include <reactor/trace.h>

#include <map>
#include <unordered_set>
#include <unordered_map>
#include <chrono>
#include <deque>
#include <mutex>

namespace reactor
{

template< typename T,
          typename H = std::hash<T>,
          typename P = std::equal_to<T>>
class ObjectsTimer
{
public:
  ObjectsTimer(bool using_lock = false) : using_lock_(using_lock) {}

  int32_t get_min_timeout_milliseconds();

  int32_t register_timeout(const uint32_t &msec, T object);

  bool    remove_timeout  (T object);

  std::deque<std::unordered_set<T, H, P>>
          extract_timeout_objects();

  size_t  size() const;

private:
  int32_t get_min_timeout_milliseconds_no_lock();

  bool    remove_timeout_no_lock(T object);

  class LockGuard
  {
  public:
    LockGuard(std::mutex &lock, const bool &using_lock = false)
    : lock_(lock), using_lock_(using_lock)
    { if (using_lock_ == true) lock_.lock(); }

    ~LockGuard()
    { if (using_lock_ == true) lock_.unlock(); }

  private:
    std::mutex &lock_;
    bool using_lock_ = false;
  };

private:
  std::map<std::chrono::steady_clock::time_point, std::unordered_set<T, H, P>>  time_object_;
  std::unordered_map<T, std::chrono::steady_clock::time_point, H, P>            object_time_;

  mutable std::mutex lock_;
  bool using_lock_ = false;
};

template<typename T, typename H, typename P> int32_t
ObjectsTimer<T, H, P>::get_min_timeout_milliseconds_no_lock()
{
  if (time_object_.size() == 0)
    return -1;

  int32_t min = std::chrono::duration_cast<std::chrono::milliseconds>(
      time_object_.begin()->first - std::chrono::steady_clock::now()).count();

  if (min <= 0)
    return 0;

  return min;
}

template<typename T, typename H, typename P> int32_t
ObjectsTimer<T, H, P>::get_min_timeout_milliseconds()
{
  LockGuard guard(lock_, using_lock_);
  return get_min_timeout_milliseconds_no_lock();
}

template<typename T, typename H, typename P> int32_t
ObjectsTimer<T, H, P>::register_timeout(const uint32_t &msec, T object)
{
  LockGuard guard(lock_, using_lock_);
  remove_timeout_no_lock(object);

  object_time_[object] = std::chrono::steady_clock::now() + std::chrono::milliseconds(msec);
  time_object_[object_time_[object]].insert(object);

  return get_min_timeout_milliseconds_no_lock();
}

template<typename T, typename H, typename P> bool
ObjectsTimer<T, H, P>::remove_timeout(T object)
{
  LockGuard guard(lock_, using_lock_);
  return remove_timeout_no_lock(object);
}

template<typename T, typename H, typename P>
std::deque<std::unordered_set<T, H, P>>
ObjectsTimer<T, H, P>::extract_timeout_objects()
{
  LockGuard guard(lock_, using_lock_);
  std::deque<std::unordered_set<T, H, P>> timeout_objects;

  if (time_object_.size() == 0)
    return timeout_objects;

  typename std::map<std::chrono::steady_clock::time_point,
                    std::unordered_set<T, H, P>>::iterator it = time_object_.begin();

  while (it != time_object_.end())
  {
    if (std::chrono::duration_cast<std::chrono::milliseconds>(
        time_object_.begin()->first - std::chrono::steady_clock::now()).count() > 0)
      break;

    for (T object : it->second)
      object_time_.erase(object);

    timeout_objects.emplace_back(it->second);

    it = time_object_.erase(it);
  }

  return timeout_objects;
}


template<typename T, typename H, typename P> size_t
ObjectsTimer<T, H, P>::size() const
{
  LockGuard guard(lock_, using_lock_);
  return time_object_.size();
}

template<typename T, typename H, typename P> bool
ObjectsTimer<T, H, P>::remove_timeout_no_lock(T object)
{
  if (object_time_.count(object) == 0)
    return false;

  time_object_[object_time_[object]].erase(object);

  if (time_object_[object_time_[object]].size() == 0)
    time_object_.erase(object_time_[object]);

  return object_time_.erase(object) > 0;
}

}

#endif /* IO_REACTOR_REACTOR_OBJECTSTIMER_H_ */

