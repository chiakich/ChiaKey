/*
Copyright (c) 2012, Yahoo! Inc.  All rights reserved.
Copyrights licensed under the New BSD License. See the accompanying LICENSE
file for terms.
*/

#ifndef StringVectorHelper_h
#define StringVectorHelper_h

#include <string>
#include <vector>

namespace Manjusri {
using namespace std;

typedef vector<string> StringVector;

class StringVectorHelper {
 public:
  static const StringVector SubVector(const StringVector& vec, size_t from,
                                      size_t size);
  static const StringVector SubVector(StringVector::const_iterator begin,
                                      StringVector::const_iterator end);
  static const string CombineIntoString(const StringVector& vec);
  static const string CombineIntoString(const StringVector& vec, size_t from,
                                        size_t size);
  static const string CombineIntoString(StringVector::const_iterator begin,
                                        StringVector::const_iterator end);
  static const string Join(const StringVector& vec,
                           const string& separator = "");
  static const string Join(const StringVector& vec, size_t from, size_t size,
                           const string& separator = "");
  static const string Join(StringVector::const_iterator begin,
                           StringVector::const_iterator end,
                           const string& separator = "");
};

typedef StringVectorHelper SVH;

inline const StringVector StringVectorHelper::SubVector(
    StringVector::const_iterator begin, StringVector::const_iterator end) {
  StringVector result;
  result.insert(result.begin(), begin, end);
  return result;
}

inline const StringVector StringVectorHelper::SubVector(const StringVector& vec,
                                                        size_t from,
                                                        size_t size) {
  return SubVector(vec.begin() + from, vec.begin() + from + size);
}

inline const string StringVectorHelper::CombineIntoString(
    const StringVector& vec) {
  return CombineIntoString(vec.begin(), vec.end());
}

inline const string StringVectorHelper::CombineIntoString(
    const StringVector& vec, size_t from, size_t size) {
  return CombineIntoString(vec.begin() + from, vec.begin() + from + size);
}

inline const string StringVectorHelper::CombineIntoString(
    StringVector::const_iterator begin, StringVector::const_iterator end) {
  string result;
  for (StringVector::const_iterator iter = begin; iter != end; ++iter)
    result += *iter;
  return result;
}

inline const string StringVectorHelper::Join(const StringVector& vec,
                                             const string& separator) {
  return Join(vec.begin(), vec.end(), separator);
}

inline const string StringVectorHelper::Join(const StringVector& vec,
                                             size_t from, size_t size,
                                             const string& separator) {
  return Join(vec.begin() + from, vec.begin() + from + size, separator);
}

inline const string StringVectorHelper::Join(StringVector::const_iterator begin,
                                             StringVector::const_iterator end,
                                             const string& separator) {
  string result;
  for (StringVector::const_iterator iter = begin; iter != end;) {
    result += *iter;
    if (++iter != end) result += separator;
  }
  return result;
}
};  // namespace Manjusri

#endif
