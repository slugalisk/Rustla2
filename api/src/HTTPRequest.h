#pragma once

#include <folly/String.h>
#include <uWS/uWS.h>
#include <map>
#include <string>
#include <vector>

#include "Config.h"
#include "Session.h"

namespace rustla2 {

using PostDataHandler = std::function<void(const char*, const size_t)>;

class HTTPRequest {
 public:
  explicit HTTPRequest(uWS::HttpRequest req);

  explicit HTTPRequest(rustla2::HTTPRequest&& req) noexcept;

  void OnPostData(PostDataHandler handler) { post_data_handler_ = handler; }

  void WritePostData(char* data, size_t length, size_t remaining_bytes);

  const std::map<std::string, std::string> GetQueryParams() const;

  const uWS::HttpMethod GetMethod() { return req_.getMethod(); }

  std::string GetCookie(const std::string& name);

  std::string GetSessionID();

  folly::StringPiece GetClientIPHeader();

  const std::vector<folly::StringPiece>& GetPath() const { return path_; }

  folly::StringPiece GetPathPart(size_t i) const { return path_[i]; }

 private:
  uWS::HttpRequest req_;
  std::vector<folly::StringPiece> path_;
  folly::StringPiece query_;
  std::string post_data_;
  PostDataHandler post_data_handler_;
};

}  // namespace rustla2
