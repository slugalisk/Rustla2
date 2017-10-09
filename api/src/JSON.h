#pragma once

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <memory>
#include <string>

namespace rustla2 {
namespace json {

template <typename T>
std::string Serialize(T* model) {
  rapidjson::StringBuffer buf;
  rapidjson::Writer<rapidjson::StringBuffer> writer(buf);
  model->WriteJSON(&writer);
  return buf.GetString();
}

template <typename T>
std::string Serialize(T& model) {
  return Serialize(&model);
}

template <typename T>
std::string Serialize(std::shared_ptr<T> model) {
  return Serialize(model.get());
}

struct StringRef {
  template <typename T>
  explicit StringRef(const T& value)
      : string_(value.GetString()), size_(value.GetStringLength()) {}

  inline operator std::string() const { return std::string(string_, size_); }

  bool operator==(const std::string& rhs) const {
    return rhs.size() == size_ && rhs == string_;
  }

  const char* string_;
  const size_t size_;
};

}  // namespace json
}  // namespace rustla2
