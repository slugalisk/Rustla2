#pragma once

#include <rapidjson/document.h>
#include <string>

#include "Curl.h"
#include "Status.h"

namespace rustla2 {

class APIResult {
 public:
  virtual ~APIResult() = default;

  const rapidjson::Document& GetData() const { return data_; }

  virtual std::string GetSchema() = 0;

  Status SetData(const char* data, size_t length);

 private:
  rapidjson::Document data_;
};

}  // namespace rustla2
