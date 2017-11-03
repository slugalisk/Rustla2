#pragma once

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <cmath>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include "Status.h"

namespace rustla2 {
namespace json {

const uint64_t kMaxIntSize = 0x1fffffffffffff;

using Writer = rapidjson::Writer<rapidjson::StringBuffer>;
using WriterFunction = std::function<void(Writer* writer)>;

std::string Serialize(WriterFunction writer_func);

template <typename T>
std::string Serialize(T* model) {
  return Serialize([&](Writer* writer) { model->WriteJSON(writer); });
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
  StringRef(){};

  StringRef(const char* string, const size_t size)
      : string_(string), size_(size){};

  template <typename T>
  explicit StringRef(const T& value)
      : StringRef(value.GetString(), value.GetStringLength()) {}

  std::string GetString() const { return std::string(string_, size_); }

  operator std::string() const { return GetString(); }

  bool operator==(const std::string& rhs) const {
    return rhs.size() == size_ && rhs == string_;
  }

  StringRef operator=(const StringRef& ref) const {
    return StringRef(ref.string_, ref.size_);
  }

  const char* string_{""};
  const size_t size_{0};
};

std::ostream& operator<<(std::ostream& stream, const StringRef& status);

rapidjson::Document Parse(const char* data, const size_t length,
                          const std::string& schema_json = "",
                          Status* status = nullptr);

}  // namespace json
}  // namespace rustla2