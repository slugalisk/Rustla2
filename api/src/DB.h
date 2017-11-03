#pragma once

#include <sqlite_modern_cpp.h>
#include <memory>

#include "Bans.h"
#include "Config.h"
#include "IPRanges.h"
#include "Streams.h"
#include "Users.h"

namespace rustla2 {

class DB {
  using UserBans = Bans<Users>;
  using StreamBans = Bans<Streams>;
  using IPBans = Bans<IPRanges, IPRangeBanMediator>;

 public:
  DB()
      : db_(Config::Get().GetDBPath()),
        users_(std::make_shared<Users>(db_)),
        streams_(std::make_shared<Streams>(db_)),
        banned_ips_(std::make_shared<IPRanges>(db_, "banned_ip_ranges")),
        user_bans_(std::make_shared<UserBans>(db_, "user_bans", users_)),
        stream_bans_(
            std::make_shared<StreamBans>(db_, "stream_bans", streams_)),
        ip_bans_(std::make_shared<IPBans>(db_, "ip_bans", banned_ips_)) {}

  std::shared_ptr<Users> GetUsers() { return users_; }

  std::shared_ptr<Streams> GetStreams() { return streams_; }

  std::shared_ptr<IPRanges> GetBannedIPs() { return banned_ips_; }

  std::shared_ptr<UserBans> GetUserBans() { return user_bans_; }

  std::shared_ptr<StreamBans> GetStreamBans() { return stream_bans_; }

  std::shared_ptr<IPBans> GetIPBans() { return ip_bans_; }

 private:
  sqlite::database db_;
  std::shared_ptr<Users> users_;
  std::shared_ptr<IPRanges> banned_ips_;
  std::shared_ptr<Streams> streams_;
  std::shared_ptr<UserBans> user_bans_;
  std::shared_ptr<StreamBans> stream_bans_;
  std::shared_ptr<IPBans> ip_bans_;
};

}  // namespace rustla2
