#include "AdminHTTPService.h"

#include <glog/logging.h>
#include <rapidjson/document.h>
#include <rapidjson/schema.h>
#include <cstring>

#include "Config.h"
#include "JSON.h"

namespace rustla2 {

AdminHTTPService::AdminHTTPService(std::shared_ptr<DB> db) : db_(db) {}

void AdminHTTPService::RegisterRoutes(HTTPRouter *router) {
  const auto &api = Config::Get().GetAPI() + "/admin";

  router->Get(api + "/users", GetHandler(db_->GetUsers()));
  router->Get(api + "/streams", GetHandler(db_->GetStreams()));
  router->Get(api + "/banned-ips", GetHandler(db_->GetBannedIPs()));

  auto user_bans = db_->GetUserBans();
  router->Get(api + "/user-bans", GetHandler(user_bans));
  router->Post(api + "/user-bans", CreateBanHandler(user_bans));
  router->Delete(api + "/user-bans/*", DeleteBanHandler(user_bans));

  auto stream_bans = db_->GetStreamBans();
  router->Get(api + "/stream-bans", GetHandler(stream_bans));
  router->Post(api + "/stream-bans", CreateBanHandler(stream_bans));
  router->Delete(api + "/stream-bans/*", DeleteBanHandler(stream_bans));

  auto ip_bans = db_->GetIPBans();
  router->Get(api + "/ip-bans", GetHandler(ip_bans));
  router->Post(api + "/ip-bans", &AdminHTTPService::CreateIPBan, this);
  router->Delete(api + "/ip-bans/*", DeleteBanHandler(ip_bans));
}

void AdminHTTPService::GetUsers(uWS::HttpResponse *res, HTTPRequest *req) {
  if (RejectUnauthorized(res, req)) {
    return;
  }

  HTTPResponseWriter writer(res);
  writer.Status(200, "OK");
  writer.JSON(json::Serialize(db_->GetUsers()));
}

template <typename T>
HTTPRouteHandler AdminHTTPService::GetHandler(T collection) {
  return [=](uWS::HttpResponse *res, HTTPRequest *req) {
    if (RejectUnauthorized(res, req)) {
      return;
    }

    HTTPResponseWriter writer(res);
    writer.Status(200, "OK");
    writer.JSON(json::Serialize(collection));
  };
}

template <typename T>
HTTPRouteHandler AdminHTTPService::CreateBanHandler(T collection) {
  return [=](uWS::HttpResponse *res, HTTPRequest *req) {
    if (RejectUnauthorized(res, req)) {
      return;
    }

    req->OnPostData([=](const char *data, const size_t length) {
      HTTPResponseWriter writer(res);
      Status status;

      const auto schema = R"json(
          {
            "type": "object",
            "properties": {
              "id": {"type": "integer"},
              "expiry_time": {"type": "integer"},
              "reason": {"type": "string"}
            },
            "required": [
              "id",
              "expiry_time"
            ]
          }
        )json";
      const auto input = json::Parse(data, length, schema, &status);
      if (!status.Ok()) {
        LOG(ERROR) << "AdminHTTPService::CreateBanHandler " << status;

        writer.Status(400, "Invalid Request");
        writer.JSON(json::Serialize(status));
        return;
      }

      CreateBan(input["id"].GetUint64(), input, collection, &writer);
    });
  };
}

void AdminHTTPService::CreateIPBan(uWS::HttpResponse *res, HTTPRequest *req) {
  if (RejectUnauthorized(res, req)) {
    return;
  }

  req->OnPostData([=](const char *data, const size_t length) {
    HTTPResponseWriter writer(res);
    Status status;

    const auto schema = R"json(
        {
          "type": "object",
          "properties": {
            "ip_range_start": {
              "anyOf": [
                {"format": "ipv4"},
                {"format": "ipv6"}
              ]
            },
            "ip_range_end": {
              "anyOf": [
                {"format": "ipv4"},
                {"format": "ipv6"}
              ]
            },
            "expiry_time": {"type": "integer"},
            "reason": {"type": "string"}
          },
          "required": [
            "ip_range_start",
            "ip_range_end",
            "expiry_time"
          ]
        }
      )json";
    const auto input = json::Parse(data, length, schema, &status);
    if (!status.Ok()) {
      LOG(ERROR) << "AdminHTTPService::CreateIPBan " << status;

      writer.Status(400, "Invalid Request");
      writer.JSON(json::Serialize(status));
      return;
    }

    auto range_start = json::StringRef(input["ip_range_start"]);
    auto range_end = json::StringRef(input["ip_range_end"]);
    auto reason = input.HasMember("reason")
                      ? std::string(json::StringRef(input["reason"]))
                      : "";

    DLOG(INFO) << "AdminHTTPService::CreateIPBan "
               << "ip_range_start: " << range_start << ", "
               << "ip_range_end: " << range_end << ", "
               << "reason: " << reason;

    auto range =
        db_->GetBannedIPs()->Emplace(range_start, range_end, reason, &status);
    if (!status.Ok()) {
      LOG(ERROR) << "AdminHTTPService::CreateIPBan " << status;

      writer.Status(400, "Invalid Request");
      writer.JSON(json::Serialize(status));
      return;
    }

    CreateBan(range->GetID(), input, db_->GetIPBans(), &writer);
  });
}

template <typename T>
void AdminHTTPService::CreateBan(const uint64_t entry_id,
                                 const rapidjson::Document &input, T collection,
                                 HTTPResponseWriter *writer) {
  auto expiry_time = input["expiry_time"].GetUint64();
  auto reason = input.HasMember("reason")
                    ? std::string(json::StringRef(input["reason"]))
                    : "";

  DLOG(INFO) << "AdminHTTPService::CreateBan "
             << "entry_id: " << entry_id << ", "
             << "expiry_time: " << expiry_time << ", "
             << "reason: " << reason;

  Status status;
  auto ban = collection->Emplace(entry_id, expiry_time, reason, &status);
  if (!status.Ok()) {
    LOG(ERROR) << "AdminHTTPService::CreateBan " << status;

    writer->Status(500, "Internal Error");
    writer->JSON(json::Serialize(status));
    return;
  }

  writer->Status(200, "OK");
  writer->JSON(json::Serialize([&](json::Writer *writer) {
    writer->StartObject();
    writer->Key("entry");

    auto model = collection->GetCollection()->GetByID(ban->GetEntryID());
    if (model == nullptr) {
      writer->Null();
    } else {
      model->WriteJSON(writer);
    }

    writer->Key("ban");
    ban->WriteJSON(writer);
    writer->EndObject();
  }));
}

template <typename T>
HTTPRouteHandler AdminHTTPService::DeleteBanHandler(T collection) {
  return [=](uWS::HttpResponse *res, HTTPRequest *req) {
    HTTPResponseWriter writer(res);
    Status status;

    auto id = std::stoull(req->GetPathPart(3).toString());
    if (!id) {
      LOG(ERROR) << "AdminHTTPService::DeleteBanHandler ";

      writer.Status(400, "Invalid Request");
      writer.JSON("{\"error\": \"invalid id\"}");
      return;
    }

    collection->EraseByID(id);

    writer.Status(200, "OK");
    writer.JSON("{}");
  };
}

bool AdminHTTPService::RejectUnauthorized(uWS::HttpResponse *res,
                                          HTTPRequest *req) {
  // const auto name = req->GetSessionID();
  // auto user = name == "" ? nullptr : db_->GetUsers()->GetByName(name);

  // if (user == nullptr || !user->GetIsAdmin()) {
  //   HTTPResponseWriter writer(res);
  //   writer.Status(401, "Unauthorized");
  //   writer.JSON("{\"error\": \"unauthorized\"}");
  //   return true;
  // }

  return false;
}

}  // namespace rustla2
