#include "Config.h"

#include <folly/Uri.h>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>

namespace rustla2 {

namespace {

constexpr char kDefaultAPI[] = "/api";
constexpr char kDefaultDBPath[] = "./overrustle.sqlite";
constexpr char kDefaultGithubURL[] =
    "https://github.com/ILiedAboutCake/Rustla2";
constexpr char kDefaultJWTSecret[] = "PepoThink";
constexpr char kDefaultJWTName[] = "jwt";
constexpr time_t kDefaultJWTTTL = 60 * 60 * 24 * 7;
constexpr uint16_t kDefaultPort = 8080;
constexpr time_t kDefaultLiveCheckInterval = 60000;
constexpr char kDefaultIPAddressHeader[] = "x-client-ip";
constexpr time_t kDefaultStreamBroadcastInterval = 60000;
constexpr time_t kDefaultRustlerBroadcastInterval = 100;
constexpr char kDefaultPublicPath[] = "./public";

}  // namespace

void Config::Init(const std::string& config_path) {
  const auto config = ReadConfigFile(config_path);

  AssignString(&api_, "API", config, kDefaultAPI);
  AssignString(&api_ws_, "API_WS", config);
  AssignString(&db_db_, "DB_DB", config);
  AssignString(&db_path_, "DB_PATH", config, kDefaultDBPath);
  AssignString(&donate_do_url_, "DONATE_DO_URL", config);
  AssignString(&donate_linode_url_, "DONATE_LINODE_URL", config);
  AssignString(&donate_paypal_url_, "DONATE_PAYPAL_URL", config);
  AssignString(&github_url_, "GITHUB_URL", config, kDefaultGithubURL);
  AssignString(&jwt_secret_, "JWT_SECRET", config, kDefaultJWTSecret);
  AssignString(&jwt_name_, "JWT_NAME", config, kDefaultJWTName);
  AssignUint(&jwt_ttl_, "JWT_TTL", config, kDefaultJWTTTL);
  AssignUint(&port_, "PORT", config, kDefaultPort);
  AssignUint(&livecheck_interval_, "LIVECHECK_INTERVAL", config,
             kDefaultLiveCheckInterval);
  AssignString(&twitch_client_id_, "TWITCH_CLIENT_ID", config);
  AssignString(&twitch_client_secret_, "TWITCH_CLIENT_SECRET", config);
  AssignString(&twitch_redirect_url_, "TWITCH_REDIRECT_URI", config);
  AssignString(&google_public_api_key_, "GOOGLE_PUBLIC_API_KEY", config);
  AssignString(&ip_address_header_, "IP_ADDRESS_HEADER", config,
               kDefaultIPAddressHeader);
  AssignUint(&stream_broadcast_interval, "STREAM_BROADCAST_INTERVAL", config,
             kDefaultStreamBroadcastInterval);
  AssignUint(&rustler_broadcast_interval, "RUSTLER_BROADCAST_INTERVAL", config,
             kDefaultRustlerBroadcastInterval);
  AssignString(&ssl_cert_path_, "SSL_CERT_PATH", config);
  AssignString(&ssl_key_path_, "SSL_KEY_PATH", config);
  AssignString(&public_path_, "PUBLIC_PATH", config, kDefaultPublicPath);

  if (!ssl_cert_path_.empty() && !ssl_key_path_.empty() &&
      !AssignString(&ssl_key_password_, "SSL_KEY_PASSWORD", config)) {
    std::cout << "Enter a password for the SSL key:" << std::endl;
    std::cin >> ssl_key_password_;
  }

  jwt_secure_ = folly::Uri(twitch_redirect_url_).scheme() == "https";
}

const std::unordered_map<std::string, std::string> Config::ReadConfigFile(
    const std::string& path) {
  std::ifstream file(path);
  std::unordered_map<std::string, std::string> config;

  std::string line;
  while (std::getline(file, line)) {
    std::istringstream line_strema(line);
    std::string key;
    if (std::getline(line_strema, key, '=')) {
      std::string value;
      if (std::getline(line_strema, value)) {
        config[key] = value;
      }
    }
  }

  return config;
}

bool Config::AssignString(
    std::string* prop, const std::string& key,
    const std::unordered_map<std::string, std::string>& config,
    const std::string fallback) {
  const char* env_value = std::getenv(key.c_str());
  if (env_value != nullptr) {
    prop->assign(env_value, strlen(env_value));
    return true;
  }

  const auto config_value = config.find(key);
  if (config_value != config.end()) {
    prop->assign(config_value->second);
    return true;
  }

  if (fallback != "") {
    prop->assign(fallback);
    return true;
  }

  return false;
}

}  // namespace rustla2
