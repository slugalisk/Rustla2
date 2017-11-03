#pragma once

#include <folly/Conv.h>
#include <folly/Format.h>
#include <glog/logging.h>
#include <rapidjson/document.h>
#include <boost/asio/ip/address.hpp>
#include <chrono>
#include <limits>
#include <sstream>
#include <vector>

#include "Status.h"

namespace rustla2 {

template <typename T>
void BanMediator<T>::WriteJSON(
    std::shared_ptr<T> collection, std::shared_ptr<rustla2::Ban> ban,
    rapidjson::Writer<rapidjson::StringBuffer>* writer) {
  writer->StartObject();
  writer->Key("ban");
  ban->WriteJSON(writer);
  writer->Key("entry");

  auto model = collection->GetByID(ban->GetID());
  if (model == nullptr) {
    writer->Null();
  } else {
    model->WriteJSON(writer);
  }

  writer->EndObject();
}

template <typename T>
Status BanMediator<T>::SetIsBanned(const uint64_t entry_id,
                                   std::shared_ptr<T> collection,
                                   const bool value) {
  auto model = collection->GetByID(entry_id);
  if (model == nullptr) {
    return Status(StatusCode::ERROR, "invalid entry id",
                  folly::sformat("no entry found with id {}", entry_id));
  }

  if (model->GetIsBanned() == value) {
    return Status(
        StatusCode::ERROR, "invalid entry",
        value ? "entry is already banned" : "entry is already unbanned");
  }

  model->SetIsBanned(value);
  model->Save();

  return Status::OK;
}

template <typename TCollection, typename TBanMediator>
Bans<TCollection, TBanMediator>::Bans(sqlite::database db,
                                      const std::string& table_name,
                                      std::shared_ptr<TCollection> collection)
    : db_(db), table_name_(table_name), collection_(collection) {
  InitTable();

  db_ << folly::sformat("SELECT MAX(id) + 1 FROM {}", table_name_) >>
      [&](uint64_t next_id) { next_id_ = next_id; };

  const auto sql = R"sql(
      SELECT
        `id`,
        `entry_id`,
        strftime('%s', `expiry_time`),
        `note`
      FROM `{}`
      WHERE `is_active` = 1
      ORDER BY `expiry_time` DESC
    )sql";
  auto query = db_ << folly::sformat(sql, table_name_);

  query >> [&](const uint64_t id, const uint64_t entry_id,
               const uint64_t expiry_time, const std::string& note) {
    Insert(std::make_shared<Ban>(db_, table_name_, id, entry_id, expiry_time,
                                 note));
  };

  LOG(INFO) << "read " << Size() << " bans from " << table_name_;
}

template <typename TCollection, typename TBanMediator>
void Bans<TCollection, TBanMediator>::InitTable() {
  boost::shared_lock<boost::shared_mutex> read_lock(lock_);

  auto sql = R"sql(
      CREATE TABLE IF NOT EXISTS `` (
        `id` INT PRIMARY KEY ASC,
        `entry_id` INT,
        `expiry_time` DATETIME NOT NULL,
        `is_active` TINYINT(1) DEFAULT 1,
        `note` VARCHAR(255),
        `created_at` DATETIME NOT NULL,
        `updated_at` DATETIME NOT NULL
      )
    )sql";
  db_ << folly::sformat(sql, table_name_);
}

template <typename TCollection, typename TBanMediator>
void Bans<TCollection, TBanMediator>::WriteJSON(
    rapidjson::Writer<rapidjson::StringBuffer>* writer) {
  boost::shared_lock<boost::shared_mutex> read_lock(lock_);

  writer->StartArray();
  for (const auto& it : data_) {
    it.second->WriteJSON(writer);
  }
  writer->EndArray();
}

template <typename TCollection, typename TBanMediator>
std::shared_ptr<Ban> Bans<TCollection, TBanMediator>::Emplace(
    const uint64_t entry_id, const time_t expiry_time, const std::string& note,
    Status* status) {
  auto ban = std::make_shared<Ban>(db_, table_name_, GetNextID(), entry_id,
                                   expiry_time, note);

  auto ban_status = TBanMediator::Ban(collection_, ban);
  if (!ban_status.Ok()) {
    LOG(ERROR) << "Bans::Emplace " << ban_status;
    if (status) *status = ban_status;
    return nullptr;
  }

  auto save_status = ban->SaveNew();
  if (!save_status.Ok()) {
    LOG(ERROR) << "Bans::Emplace " << save_status;
    if (status) *status = save_status;
    return nullptr;
  }

  boost::unique_lock<boost::shared_mutex> write_lock(lock_);
  Insert(ban);

  if (status) *status = Status::OK;
  return ban;
}

template <typename TCollection, typename TBanMediator>
Status Bans<TCollection, TBanMediator>::EraseByID(const uint64_t id) {
  auto ban = data_.find(id);
  if (ban == data_.end()) {
    LOG(ERROR) << "Bans::EraseByID id does not exist: " << id;
    return Status::ERROR;
  }
  return Erase(ban->second);
}

template <typename TCollection, typename TBanMediator>
Status Bans<TCollection, TBanMediator>::Erase(std::shared_ptr<Ban> ban) {
  TBanMediator::Unban(ban, collection_);

  ban->SetActive(false);
  ban->Save();

  boost::unique_lock<boost::shared_mutex> write_lock(lock_);
  data_.erase(ban->GetID());
  entry_ids_.erase(ban->GetEntryID());

  return Status::OK;
}

template <typename TCollection, typename TBanMediator>
void Bans<TCollection, TBanMediator>::Insert(std::shared_ptr<Ban> ban) {
  entry_ids_.insert(ban->GetEntryID());
  data_[ban->GetID()] = ban;
}

template <typename TCollection, typename TBanMediator>
void Bans<TCollection, TBanMediator>::ClearExpired() {
  std::vector<std::shared_ptr<Ban>> expired_bans;
  auto now = std::chrono::duration_cast<std::chrono::seconds>(
                 std::chrono::system_clock::now().time_since_epoch())
                 .count();

  {
    boost::shared_lock<boost::shared_mutex> read_lock(lock_);
    for (const auto& it : data_) {
      if (it.second->GetExpiryTime() < now) {
        expired_bans.push_back(it.second);
      }
    }
  }

  for (const auto ban : expired_bans) {
    Erase(ban);
  }

  if (expired_bans.size()) {
    LOG(INFO) << "Bans::ClearExpired "
              << "table_name: " << table_name_ << ", "
              << "expired " << expired_bans.size() << " ban(s)";
  }
}

}  // namespace rustla2
