#include "AngelThumpClient.h"

#include "Curl.h"
#include "JSON.h"

namespace rustla2 {
namespace angelthump {

rapidjson::Document ChannelResult::GetSchema() {
  rapidjson::Document schema;
  schema.Parse(R"json(
      {
        "type": "object",
        "properties": {
          "live": {"type": "boolean"},
          "thumbnail": {
            "type": "string",
            "format": "uri"
          },
          "viewers": {"type": "integer"}
        },
        "required": ["live", "thumbnail", "viewers"]
      }
    )json");
  return schema;
}

bool ChannelResult::GetLive() const { return GetData()["live"].GetBool(); }

std::string ChannelResult::GetThumbnail() const {
  return json::StringRef(GetData()["thumbnail"]);
}

uint64_t ChannelResult::GetViewers() const {
  return GetData()["viewers"].GetUint64();
}

Status Client::GetChannelByName(const std::string& name,
                                ChannelResult* result) {
  CurlRequest req("https://api.angelthump.com/" + name);
  req.Submit();

  if (!req.Ok()) {
    return Status(StatusCode::HTTP_ERROR, req.GetErrorMessage());
  }
  if (req.GetResponseCode() != 200) {
    return Status(
        StatusCode::API_ERROR, "received non 200 response",
        "api returned status code " + std::to_string(req.GetResponseCode()));
  }

  const auto& response = req.GetResponse();
  return result->SetData(response.c_str(), response.size());
}

}  // namespace angelthump
}  // namespace rustla2
