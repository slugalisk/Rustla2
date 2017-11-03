#include "Users.h"

#include <glog/logging.h>

namespace rustla2 {

std::string User::GetStreamJSON() {
  boost::shared_lock<boost::shared_mutex> read_lock(lock_);

  rapidjson::StringBuffer buf;
  rapidjson::Writer<rapidjson::StringBuffer> writer(buf);

  writer.StartObject();
  writer.Key("service");
  writer.String(channel_->GetService());
  writer.Key("channel");
  writer.String(channel_->GetChannel());
  writer.EndObject();

  return buf.GetString();
}

std::string User::GetProfileJSON() {
  boost::shared_lock<boost::shared_mutex> read_lock(lock_);

  rapidjson::StringBuffer buf;
  rapidjson::Writer<rapidjson::StringBuffer> writer(buf);

  writer.StartObject();
  writer.Key("username");
  writer.String(name_);
  writer.Key("service");
  writer.String(channel_->GetService());
  writer.Key("channel");
  writer.String(channel_->GetChannel());
  writer.Key("left_chat");
  writer.Bool(left_chat_);
  writer.EndObject();

  return buf.GetString();
}

void User::WriteJSON(rapidjson::Writer<rapidjson::StringBuffer> *writer) {
  boost::shared_lock<boost::shared_mutex> read_lock(lock_);

  writer->StartObject();
  writer->Key("id");
  writer->Uint64(id_);
  writer->Key("username");
  writer->String(name_);
  writer->Key("channel");
  channel_->WriteJSON(writer);
  writer->Key("left_chat");
  writer->Bool(left_chat_);
  writer->Key("last-ip");
  writer->String(last_ip_);
  writer->Key("last_seen");
  writer->Int(last_seen_);
  writer->Key("is_admin");
  writer->Bool(is_admin_);
  writer->Key("is_banned");
  writer->Bool(is_banned_);
  writer->EndObject();
}

bool User::Save() {
  boost::shared_lock<boost::shared_mutex> read_lock(lock_);
  try {
    const auto sql = R"sql(
        UPDATE `users` SET
          `service` = ?,
          `channel` = ?,
          `last_ip` = ?,
          `last_seen` = datetime(?, 'unixepoch'),
          `left_chat` = ?,
          `is_admin` = ?,
          `is_banned` = ?,
          `updated_at` = datetime()
        WHERE `id` = ?
      )sql";
    db_ << sql << channel_->GetService() << channel_->GetChannel() << last_ip_
        << last_seen_ << left_chat_ << is_admin_ << is_banned_ << id_;
  } catch (const sqlite::sqlite_exception &e) {
    LOG(ERROR) << "error updating user "
               << "id: " << id_ << ", "
               << "service: " << channel_->GetService() << ", "
               << "channel: " << channel_->GetChannel() << ", "
               << "last_ip: " << last_ip_ << ", "
               << "last_seen: " << last_seen_ << ", "
               << "left_chat: " << left_chat_ << ", "
               << "is_admin: " << is_admin_ << ", "
               << "is_banned: " << is_banned_ << ", "
               << "error: " << e.what() << ", "
               << "code: " << e.get_extended_code();

    return false;
  }
  return true;
}

bool User::SaveNew() {
  boost::shared_lock<boost::shared_mutex> read_lock(lock_);
  try {
    const auto sql = R"sql(
        INSERT INTO `users` (
          `id`,
          `name`,
          `service`,
          `channel`,
          `last_ip`,
          `last_seen`,
          `left_chat`,
          `is_admin`,
          `is_banned`,
          `created_at`,
          `updated_at`
        )
        VALUES (
          ?,
          ?,
          ?,
          ?,
          ?,
          datetime(?, 'unixepoch'),
          ?,
          ?,
          ?,
          datetime(),
          datetime()
        )
      )sql";
    db_ << sql << id_ << name_ << channel_->GetService()
        << channel_->GetChannel() << last_ip_ << last_seen_ << left_chat_
        << is_admin_ << is_banned_;
  } catch (const sqlite::sqlite_exception &e) {
    LOG(ERROR) << "error creating user "
               << "id: " << id_ << ", "
               << "name: " << name_ << ", "
               << "service: " << channel_->GetService() << ", "
               << "channel: " << channel_->GetChannel() << ", "
               << "last_ip: " << last_ip_ << ", "
               << "last_seen: " << last_seen_ << ", "
               << "left_chat: " << left_chat_ << ", "
               << "is_admin: " << is_admin_ << ", "
               << "is_banned: " << is_banned_ << ", "
               << "error: " << e.what() << ", "
               << "code: " << e.get_extended_code();

    return false;
  }
  return true;
}

Users::Users(sqlite::database db) : db_(db) {
  InitTable();

  auto sql = R"sql(
      SELECT
        `id`,
        `name`,
        `service`,
        `channel`,
        `last_ip`,
        strftime('%s', `last_seen`),
        `left_chat`,
        `is_admin`,
        `is_banned`
      FROM `users`
    )sql";
  auto query = db_ << sql;

  query >> [&](const uint64_t id, const std::string &name,
               const std::string &service, const std::string &channel,
               const std::string &last_ip, const time_t last_seen,
               const bool left_chat, const bool is_admin,
               const bool is_banned) {
    auto user = std::make_shared<User>(
        db_, id, name, Channel::Create(channel, service), last_ip, last_seen,
        left_chat, is_admin, is_banned);

    data_by_id_[id] = user;
    data_by_name_[name] = user;
  };

  LOG(INFO) << "read " << data_by_name_.size() << " users";
}

void Users::InitTable() {
  auto sql = R"sql(
      CREATE TABLE IF NOT EXISTS `users` (
        `id` INT PRIMARY KEY,
        `name` VARCHAR(255) NOT NULL,
        `service` VARCHAR(255) NOT NULL,
        `channel` VARCHAR(255) NOT NULL,
        `last_ip` VARCHAR(255) NOT NULL,
        `last_seen` DATETIME NOT NULL,
        `left_chat` TINYINT(1) DEFAULT 0,
        `is_banned` TINYINT(1) NOT NULL DEFAULT 0,
        `created_at` DATETIME NOT NULL,
        `updated_at` DATETIME NOT NULL,
        `is_admin` TINYINT(1) DEFAULT 0,
        UNIQUE (`name`)
      );
    )sql";
  db_ << sql;
}

std::shared_ptr<User> Users::Emplace(const std::string &name,
                                     const Channel &channel,
                                     const std::string &ip) {
  auto user = std::make_shared<User>(db_, name, channel, ip);

  {
    boost::unique_lock<boost::shared_mutex> write_lock(lock_);
    auto it = data_by_name_.find(user->GetName());
    if (it != data_by_name_.end()) {
      return it->second;
    }

    data_by_id_[user->GetID()] = user;
    data_by_name_[user->GetName()] = user;
  }

  user->SaveNew();

  return user;
}

void Users::WriteJSON(rapidjson::Writer<rapidjson::StringBuffer> *writer) {
  boost::shared_lock<boost::shared_mutex> read_lock(lock_);

  writer->StartArray();
  for (const auto &it : data_by_name_) {
    it.second->WriteJSON(writer);
  }
  writer->EndArray();
}

}  // namespace rustla2
