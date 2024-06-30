#ifndef HTTPPROTOCOL_REACTOR_HTTPHEADER_H_
#define HTTPPROTOCOL_REACTOR_HTTPHEADER_H_

#include <reactor/trace.h>
#include <algorithm>
#include <initializer_list>
#include <unordered_map>
#include <map>
#include <list>
#include <set>
#include <string>
#include <sstream>

namespace https_reactor
{

using namespace reactor;

// not allowed duplicate header-value
class HttpHeader
{
public:
  HttpHeader() {}
  HttpHeader(const std::initializer_list<std::pair<std::string, std::string>> &rhs);
  HttpHeader(const std::map               <std::string, std::string>  &rhs);
  HttpHeader(const std::multimap          <std::string, std::string>  &rhs);
  HttpHeader(const std::unordered_map     <std::string, std::string>  &rhs);
  HttpHeader(const std::unordered_multimap<std::string, std::string>  &rhs);
  HttpHeader(const std::map<std::string, std::list<std::string>>      &rhs);

  virtual ~HttpHeader() {}

  // all names are converted to lowercase.
  size_t  count     (const std::string &name) const;
  bool    contains  (const std::string &name) const;
  bool    contains  (const std::string &name, const char  *value, const bool &value_ignore_case = false) const;
  template<typename T>
  bool    contains  (const std::string &name, const T     &value) const;

  // values can be duplicated with the same name.
  bool    add       (const std::string &name, const char        *value);
  bool    add       (const std::string &name, const std::string &value);
  template<typename T>
  bool    add       (const std::string &name, const T           &value);
  void    add       (const HttpHeader  &header);
  bool    del       (const std::string &name);
  bool    del       (const std::string &name, const std::string &value);

  // if a value for name exists it will be deleted.
  void    set       (const std::string &name, const char        *value);
  void    set       (const std::string &name, const std::string &value);
  template<typename T>
  void    set       (const std::string &name, const T           &value);
  void    set       (const std::string &name, const std::list<std::string> values);

  // get first value
  template<typename T>
  T       get       (const std::string &name) const;

  const std::string &
          ref_value (const std::string &name) const;

  std::list<std::string>
          get_values(const std::string &name) const;

  std::string
          to_string () const;

  std::string
          to_string (const std::map<std::string, std::string> &header_value_to_be_added) const;

  std::string
          to_string (std::map<std::string, std::string> &&header_value_to_be_added) const;

  size_t  size() const;

  void    clear();

public:
  static std::string to_lower(const std::string &str)
  { std::string lower; for (const auto &c : str) lower += std::tolower(c); return lower; }

  static std::string &to_lower(std::string &str)
  { for (size_t index = 0; index < str.length(); ++index) str[index] = std::tolower(str.at(index)); return str; }

  struct ci_less : std::binary_function<std::string, std::string, bool>
  {
    // case-independent (ci) compare_less binary function
    struct nocase_compare : public std::binary_function<unsigned char,unsigned char,bool>
    { bool operator() (const unsigned char& c1, const unsigned char& c2) const { return tolower (c1) < tolower (c2); } };

    // comparison
    bool operator() (const std::string & s1, const std::string & s2) const
    { return std::lexicographical_compare(s1.begin(), s1.end(), s2.begin(), s2.end(), nocase_compare()); }
  };

  // key-value
  using container_type = std::list<std::pair<std::string, std::string>>;
  const container_type  &container() const { return container_; }

protected:
  size_t  count(const std::string &name, const std::string &value) const;

protected:
  std::list<std::pair<std::string, std::string>> container_;
  // name, value-iterator
  std::map<std::string, std::map<std::string, typename container_type::iterator>, ci_less> its_;

  static inline const std::string             empty_value_ = "";
  static inline const std::list<std::string>  empty_values_;
};

//template<typename T>
//T HttpHeader::string_as_T(const std::string &s)
//{
//  // Convert from a std::string to a T
//  // Type T must support >> operator
//  T t;
//  std::istringstream ist(s);
//  ist >> t;
//  return t;
//}
//
///* static */
//template<> inline std::string
//HttpHeader::string_as_T<std::string>(const std::string &s)
//{
//  return s;
//}

inline size_t
HttpHeader::size() const
{
  size_t size = 0;
  for (const auto &name : container_)
    size += name.second.size();

  return size;
}

inline void
HttpHeader::add(const HttpHeader &rhs)
{
  for (const auto &[name, value] : rhs.container_)
    this->add(name, value);
}

template<> inline std::string
HttpHeader::get(const std::string &name) const
{
  auto it = its_.find(name);
  if (it == its_.end()) return "";

  const std::map<std::string, typename container_type::iterator> &values = it->second;
  if (values.size() == 0) return "";

  return values.cbegin()->first;
}

template<typename T> T
HttpHeader::get(const std::string &name) const
{
  auto it = its_.find(name);
  if (it == its_.end()) return T();

  const std::map<std::string, typename container_type::iterator> &values = it->second;
  if (values.size() == 0) return T();

  char *endptr = nullptr;
  return (T)std::strtoll(values.cbegin()->first.c_str(), &endptr, 10);
}

inline const std::string &
HttpHeader::ref_value(const std::string &name) const
{
  auto it = its_.find(name);
  if (it == its_.end()) return empty_value_;

  const std::map<std::string, typename container_type::iterator> &values = it->second;
  if (values.size() == 0) return empty_value_;

  return values.cbegin()->first;
}

inline std::list<std::string>
HttpHeader::get_values(const std::string &name) const
{
  std::list<std::string> values;

  if (auto it = its_.find(name); it != its_.end())
    for (const auto &name_value : it->second)
      values.emplace_back(name_value.first);

  return values;
}

inline void
HttpHeader::set(const std::string &name, const char *value)
{
  this->set(name, std::string(value));
}

template<typename T> inline void
HttpHeader::set(const std::string &name, const T &value)
{
  this->set(name, std::to_string(value));
}

inline void
HttpHeader::set(const std::string &name, const std::string &value)
{
  this->del(name, value);
  container_.emplace_back(name, value);
  its_[name][value] = std::prev(container_.end());
}

inline void
HttpHeader::set(const std::string &name, const std::list<std::string> values)
{
  this->del(name);
  for (const auto &value : values)
  {
    container_.emplace_back(name, value);
    its_[name][value] = std::prev(container_.end());
  }
}

inline size_t
HttpHeader::count(const std::string &name) const
{
  auto it = its_.find(name);
  if (it == its_.end()) return 0;

  return it->second.size();
}

inline bool
HttpHeader::contains(const std::string &name) const
{
  return its_.count(name) > 0;
}

inline size_t
HttpHeader::count(const std::string &name, const std::string &value) const
{
  auto it = its_.find(name);
  if (it == its_.end()) return 0;

  return it->second.count(value);
}

inline bool
HttpHeader::contains(const std::string &name, const char *value, const bool &value_ignore_case) const
{
  auto it = its_.find(name);
  if (it == its_.end()) return false;

  if (value_ignore_case == true)
  {
    for (const auto &value_it : it->second)
      if(to_lower(value_it.first) == to_lower(value))
        return true;

    return false;
  }

  return it->second.count(value) > 0;
}

template<> inline bool
HttpHeader::contains<std::string>(const std::string &name, const std::string &value) const
{
  return this->count(name, value) > 0;
}

template<typename T> bool
HttpHeader::contains(const std::string &name, const T &value) const
{
  return this->count(name, std::to_string(value)) > 0;
}

inline bool
HttpHeader::add(const std::string &name, const char *value)
{
  return this->add(name, std::string(value));
}

template<typename T> bool
HttpHeader::add(const std::string &name, const T &value)
{
  return this->add(name, std::to_string(value));
}

inline bool
HttpHeader::add(const std::string &name, const std::string &value)
{
  if (name .size() == 0 || value.size() == 0) return false;

  if (this->contains(name, value) == true)
    return false;

  container_.emplace_back(name, value);
  its_[name][value] = std::prev(container_.end());
  return true;
}

inline bool
HttpHeader::del(const std::string &name)
{
  auto it = its_.find(name);
  if (it == its_.end())
    return false;

  for (const auto &value_it : it->second)
    container_.erase(value_it.second);

  its_.erase(name);
  return true;
}

inline bool
HttpHeader::del(const std::string &name, const std::string &value)
{
  auto it1 = its_.find(name);
  if (it1 == its_.end())
    return false;

  std::map<std::string, typename container_type::iterator> &values = it1->second;

  auto it2 = values.find(value);
  if (it2 == values.end())
    return false;

  container_.erase(it2->second);
  values.erase(it2);

  if (values.size() == 0) its_.erase(name);

  return true;
}

inline std::string
HttpHeader::to_string() const
{
  if (container_.size() == 0)
    return "";

  std::string header_str;
  for (const auto &[name, value] : container_)
  {
    if (name.size() == 0 || value.size() == 0)
      continue;

    if (header_str.length() > 0)
      header_str += "\r\n";

    header_str += name + ": " + value;
  }

  return header_str;
}

inline std::string
HttpHeader::to_string(const std::map<std::string, std::string> &header_value_to_be_added) const
{
  if (container_.size() == 0 && header_value_to_be_added.size() == 0)
    return "";

  std::string header_str;
  for (const auto &[name, value] : container_)
  {
    if (auto it = header_value_to_be_added.find(name); it != header_value_to_be_added.end())
    {
      if (header_str.length() > 0)
        header_str += "\r\n";

      header_str += it->first + ": " + it->second;
      continue;
    }

    if (name.size() == 0 || value.size() == 0)
      continue;

    if (header_str.length() > 0)
      header_str += "\r\n";

    header_str += name + ": " + value;
  }

  for (const auto &[name, value] : header_value_to_be_added)
  {
    if (its_.count(name) > 0)
      continue;

    if (header_str.length() > 0)
      header_str += "\r\n";

    header_str += name + ": "  + value;
  }

  return header_str;
}

inline std::string
HttpHeader::to_string(std::map<std::string, std::string> &&header_value_to_be_added) const
{
  if (container_.size() == 0 && header_value_to_be_added.size() == 0)
    return "";

  std::string header_str;
  for (const auto &[name, value] : container_)
  {
    if (auto it = header_value_to_be_added.find(name); it != header_value_to_be_added.end())
    {
      if (header_str.length() > 0)
        header_str += "\r\n";

      header_str += it->first + ": " + it->second;
      header_value_to_be_added.erase(it);

      continue;
    }

    if (name.size() == 0 || value.size() == 0)
      continue;

    if (header_str.length() > 0)
      header_str += "\r\n";

    header_str += name + ": " + value;
  }

  for (const auto &[name, value] : header_value_to_be_added)
  {
    if (header_str.length() > 0)
      header_str += "\r\n";

    header_str += name + ": "  + value;
  }

  return header_str;
}

inline
HttpHeader::HttpHeader(const std::initializer_list<std::pair<std::string, std::string>> &rhs)
{
  for (const auto &name_value : rhs)
    this->add(name_value.first, name_value.second);
}

inline
HttpHeader::HttpHeader(const std::map<std::string, std::string> &rhs)
{
  for (const auto &name_value : rhs)
    this->add(name_value.first, name_value.second);
}

inline
HttpHeader::HttpHeader(const std::multimap<std::string, std::string> &rhs)
{
  for (const auto &name_value : rhs)
    this->add(name_value.first, name_value.second);
}

inline
HttpHeader::HttpHeader(const std::unordered_map<std::string, std::string> &rhs)
{
  for (const auto &name_value : rhs)
    this->add(name_value.first, name_value.second);
}

inline
HttpHeader::HttpHeader(const std::unordered_multimap<std::string, std::string> &rhs)
{
  for (const auto &name_value : rhs)
    this->add(name_value.first, name_value.second);
}

inline
HttpHeader::HttpHeader(const std::map<std::string, std::list<std::string>> &rhs)
{
  for (const auto &name_value : rhs)
  {
    for (const auto &value : name_value.second)
    {
      auto it = std::find(name_value.second.begin(), name_value.second.end(), value);
      if (it != name_value.second.end()) continue;

      this->add(name_value.first, value);
    }
  }
}

inline void
HttpHeader::clear()
{
  container_.clear();
}

}

#endif /* http_reactor_HttpHeader_h */
