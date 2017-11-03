#include "JSON.h"

#include <rapidjson/error/en.h>
#include <rapidjson/schema.h>
#include <sstream>

namespace rustla2 {
namespace json {

std::string Serialize(WriterFunction writer_func) {
  rapidjson::StringBuffer buf;
  rapidjson::Writer<rapidjson::StringBuffer> writer(buf);
  writer_func(&writer);
  return buf.GetString();
}

std::ostream& operator<<(std::ostream& stream, const StringRef& status) {
  stream << status.GetString();

  return stream;
}

rapidjson::Document Parse(const char* data, const size_t length,
                          const std::string& schema_json, Status* status) {
  rapidjson::Document input;
  input.Parse(data, length);

  if (input.HasParseError()) {
    new (status) Status(StatusCode::JSON_PARSE_ERROR, "malformed json",
                        rapidjson::GetParseError_En(input.GetParseError()));

    return input;
  }

  if (!schema_json.empty()) {
    rapidjson::Document schema;
    schema.Parse(schema_json);
    if (schema.HasParseError()) {
      new (status) Status(StatusCode::JSON_SCHEMA_ERROR, "invalid json schema",
                          rapidjson::GetParseError_En(schema.GetParseError()));
      return input;
    }

    rapidjson::SchemaDocument schema_document(schema);
    rapidjson::SchemaValidator validator(schema_document);

    if (!input.Accept(validator)) {
      rapidjson::StringBuffer doc_uri;
      rapidjson::StringBuffer schema_uri;
      validator.GetInvalidDocumentPointer().StringifyUriFragment(doc_uri);
      validator.GetInvalidSchemaPointer().StringifyUriFragment(schema_uri);

      std::stringstream error_details;
      error_details << "invalid " << validator.GetInvalidSchemaKeyword() << ", "
                    << "document at " << doc_uri.GetString() << " "
                    << "does not match schema at " << schema_uri.GetString();

      new (status) Status(StatusCode::VALIDATION_ERROR,
                          "json validation failed", error_details.str());
      return input;
    }
  }

  if (status) *status = Status::OK;
  return input;
}

}  // namespace json
}  // namespace rustla2