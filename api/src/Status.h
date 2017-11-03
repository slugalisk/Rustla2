#pragma once

#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <ostream>
#include <string>

namespace rustla2 {

enum StatusCode {
  UNKNOWN = 0,
  OK = 1,
  ERROR = 2,
  HTTP_ERROR = 3,
  JSON_PARSE_ERROR = 4,
  JSON_SCHEMA_ERROR = 5,
  VALIDATION_ERROR = 6,
  API_ERROR = 7,
  DB_ENGINE_ERROR = 8,
  ID_ERROR = 9,
  INVALID_ARGUMENT = 10,
};

class Status {
 public:
  Status() : code_(StatusCode::UNKNOWN) {}

  Status(StatusCode code, const std::string& error_message)
      : code_(code), error_message_(error_message) {}

  Status(StatusCode code, const std::string& error_message,
         const std::string& error_details)
      : code_(code),
        error_message_(error_message),
        error_details_(error_details) {}

  static const Status& OK;
  static const Status& ERROR;

  bool Ok() const { return code_ == StatusCode::OK; }

  StatusCode GetCode() const { return code_; }

  std::string GetErrorMessage() const { return error_message_; }

  std::string GetErrorDetails() const { return error_details_; }

  void WriteJSON(rapidjson::Writer<rapidjson::StringBuffer>* writer);

 private:
  StatusCode code_;
  std::string error_message_;
  std::string error_details_;
};

std::ostream& operator<<(std::ostream& stream, const Status& status);

}  // namespace rustla2
