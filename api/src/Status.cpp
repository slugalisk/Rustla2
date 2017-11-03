#include "Status.h"

namespace rustla2 {

const Status& Status::OK = Status(StatusCode::OK, "");
const Status& Status::ERROR = Status(StatusCode::ERROR, "");

void Status::WriteJSON(rapidjson::Writer<rapidjson::StringBuffer>* writer) {
  writer->StartObject();
  writer->Key("code");
  writer->Int(code_);
  writer->Key("error");
  writer->String(error_message_);
  writer->Key("details");
  writer->String(error_details_);
  writer->EndObject();
}

std::ostream& operator<<(std::ostream& stream, const Status& status) {
  stream << "Status("
         << "code: " << status.GetCode() << ", "
         << "message: " << status.GetErrorMessage();

  auto details = status.GetErrorDetails();
  if (!details.empty()) stream << ", details: " << details;

  stream << ")";

  return stream;
}

}  // namespace rustla2
