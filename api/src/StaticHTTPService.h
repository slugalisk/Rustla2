#pragma once

#include <boost/filesystem/path.hpp>
#include <memory>
#include <string>
#include <unordered_map>

#include "HTTPRouter.h"

namespace rustla2 {

namespace fs = boost::filesystem;

class StaticCacheEntry {
 public:
  explicit StaticCacheEntry(const fs::path& path);

  const char* Data() { return data_.c_str(); }

  const size_t Size() { return data_size_; }

  const size_t HeaderSize() { return header_size_; }

 private:
  std::string data_;
  size_t data_size_;
  size_t header_size_;
};

class StaticHTTPService {
 public:
  explicit StaticHTTPService(const std::string& root_dir,
                             const std::string& index = "index.html");

  void RegisterRoutes(HTTPRouter* router);

  void ServeIndex(uWS::HttpResponse* res);

 private:
  std::unordered_map<std::string, std::shared_ptr<StaticCacheEntry>> cache_;
};

}  // namespace rustla2
