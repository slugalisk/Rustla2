#pragma once

#include <memory>

#include "DB.h"
#include "HTTPRequest.h"
#include "HTTPResponseWriter.h"
#include "HTTPRouter.h"

namespace rustla2 {

class AdminHTTPService {
 public:
  explicit AdminHTTPService(std::shared_ptr<DB> db);

  void RegisterRoutes(HTTPRouter *router);

  void GetUsers(uWS::HttpResponse *res, HTTPRequest *req);

 private:
  template <typename T>
  HTTPRouteHandler GetHandler(T get_bans);

  template <typename T>
  HTTPRouteHandler CreateBanHandler(T get_bans);

  void CreateIPBan(uWS::HttpResponse *res, HTTPRequest *req);

  template <typename T>
  void CreateBan(const uint64_t entry_id, const rapidjson::Document &input,
                 T collection, HTTPResponseWriter *writer);

  template <typename T>
  HTTPRouteHandler DeleteBanHandler(T get_bans);

  bool RejectUnauthorized(uWS::HttpResponse *res, HTTPRequest *req);

  std::shared_ptr<DB> db_;
};

}  // namespace rustla2
