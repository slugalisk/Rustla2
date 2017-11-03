#pragma once

#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <sqlite_modern_cpp.h>
#include <boost/thread/shared_mutex.hpp>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

#include "Channel.h"
#include "JSON.h"

namespace rustla2 {

class User {
 public:
  User(sqlite::database db, const uint64_t id, const std::string &name,
       const Channel &channel, const std::string &last_ip,
       const time_t last_seen, const bool left_chat, const bool is_admin,
       const bool is_banned)
      : db_(db),
        id_(id),
        name_(name),
        channel_(std::shared_ptr<Channel>(channel)),
        last_ip_(last_ip),
        last_seen_(last_seen),
        left_chat_(left_chat),
        is_admin_(is_admin),
        is_banned_(is_banned) {}

  User(sqlite::database db, const std::string &name, const Channel &channel,
       const std::string &last_ip)
      : db_(db),
        id_(std::hash<std::string>{}(name)&json::kMaxIntSize),
        name_(name),
        channel_(std::shared_ptr<Channel>(channel)),
        last_ip_(last_ip),
        last_seen_(time(nullptr)) {}

  uint64_t GetID() { return id_; }

  const std::string &GetName() { return name_; }

  std::shared_ptr<Channel> GetChannel() {
    boost::shared_lock<boost::shared_mutex> read_lock(lock_);
    return channel_;
  }

  std::string GetLastIP() {
    boost::shared_lock<boost::shared_mutex> read_lock(lock_);
    return last_ip_;
  }

  time_t GetLastSeen() {
    boost::shared_lock<boost::shared_mutex> read_lock(lock_);
    return last_seen_;
  }

  bool GetLeftChat() {
    boost::shared_lock<boost::shared_mutex> read_lock(lock_);
    return left_chat_;
  }

  bool GetIsAdmin() {
    boost::shared_lock<boost::shared_mutex> read_lock(lock_);
    return is_admin_;
  }

  bool GetIsBanned() {
    boost::shared_lock<boost::shared_mutex> read_lock(lock_);
    return is_banned_;
  }

  std::string GetStreamJSON();

  std::string GetProfileJSON();

  void WriteJSON(rapidjson::Writer<rapidjson::StringBuffer> *writer);

  void SetChannel(const Channel &channel) {
    boost::unique_lock<boost::shared_mutex> write_lock(lock_);
    channel_ = std::make_shared<Channel>(channel);
  }

  void SetLastIP(const std::string &last_ip) {
    boost::unique_lock<boost::shared_mutex> write_lock(lock_);
    last_ip_ = last_ip;
  }

  void SetLastSeen(const time_t last_seen) {
    boost::unique_lock<boost::shared_mutex> write_lock(lock_);
    last_seen_ = last_seen;
  }

  void SetLeftChat(bool left_chat) {
    boost::unique_lock<boost::shared_mutex> write_lock(lock_);
    left_chat_ = left_chat;
  }

  void SetIsAdmin(const bool is_admin) {
    boost::unique_lock<boost::shared_mutex> write_lock(lock_);
    is_admin_ = is_admin;
  }

  void SetIsBanned(const bool is_banned) {
    boost::unique_lock<boost::shared_mutex> write_lock(lock_);
    is_banned_ = is_banned;
  }

  bool Save();

  bool SaveNew();

 private:
  sqlite::database db_;
  boost::shared_mutex lock_;
  const uint64_t id_;
  const std::string name_;
  std::shared_ptr<Channel> channel_;
  std::string last_ip_;
  time_t last_seen_;
  bool left_chat_{false};
  bool is_admin_{false};
  bool is_banned_{false};
};

class Users {
 public:
  Users(sqlite::database db);

  void InitTable();

  std::shared_ptr<User> GetByID(const uint64_t id) {
    boost::shared_lock<boost::shared_mutex> read_lock(lock_);
    const auto i = data_by_id_.find(id);
    return i == data_by_id_.end() ? nullptr : i->second;
  }

  size_t CountID(const uint64_t id) {
    boost::shared_lock<boost::shared_mutex> read_lock(lock_);
    return data_by_id_.count(id);
  }

  std::shared_ptr<User> GetByName(const std::string &name) {
    boost::shared_lock<boost::shared_mutex> read_lock(lock_);
    const auto i = data_by_name_.find(name);
    return i == data_by_name_.end() ? nullptr : i->second;
  }

  std::shared_ptr<User> Emplace(const std::string &name, const Channel &channel,
                                const std::string &ip = "");

  void WriteJSON(rapidjson::Writer<rapidjson::StringBuffer> *writer);

 private:
  sqlite::database db_;
  boost::shared_mutex lock_;
  std::unordered_map<uint64_t, std::shared_ptr<User>> data_by_id_;
  std::unordered_map<std::string, std::shared_ptr<User>> data_by_name_;
};

}  // namespace rustla2
