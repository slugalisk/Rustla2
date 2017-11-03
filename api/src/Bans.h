#pragma once

#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <sqlite_modern_cpp.h>
#include <algorithm>
#include <atomic>
#include <boost/thread/shared_mutex.hpp>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <unordered_set>

#include "Status.h"

namespace rustla2 {

class Ban {
 public:
  Ban(sqlite::database db, const std::string& table_name, const uint64_t id,
      const uint64_t entry_id, const time_t expiry_time,
      const std::string& note)
      : db_(db),
        table_name_(table_name),
        id_(id),
        entry_id_(entry_id),
        expiry_time_(expiry_time),
        note_(note) {}

  uint64_t GetID() {
    boost::shared_lock<boost::shared_mutex> read_lock(lock_);
    return id_;
  }

  uint64_t GetEntryID() {
    boost::shared_lock<boost::shared_mutex> read_lock(lock_);
    return entry_id_;
  }

  time_t GetExpiryTime() {
    boost::shared_lock<boost::shared_mutex> read_lock(lock_);
    return expiry_time_;
  }

  const std::string& GetNote() {
    boost::shared_lock<boost::shared_mutex> read_lock(lock_);
    return note_;
  }

  bool GetIsActive() {
    boost::shared_lock<boost::shared_mutex> read_lock(lock_);
    return is_active_;
  }

  void WriteJSON(rapidjson::Writer<rapidjson::StringBuffer>* writer);

  void SetEntryID(const uint64_t entry_id) {
    boost::unique_lock<boost::shared_mutex> write_lock(lock_);
    entry_id_ = entry_id;
  }

  void SetExpiryTime(const time_t expiry_time) {
    boost::unique_lock<boost::shared_mutex> write_lock(lock_);
    expiry_time_ = expiry_time;
  }

  void SetNote(const std::string& note) {
    boost::unique_lock<boost::shared_mutex> write_lock(lock_);
    note_ = note;
  }

  void SetActive(const bool is_active) {
    boost::unique_lock<boost::shared_mutex> write_lock(lock_);
    is_active_ = is_active;
  }

  Status SaveNew();

  Status Save();

 private:
  sqlite::database db_;
  const std::string& table_name_;
  boost::shared_mutex lock_;
  uint64_t id_{0};
  uint64_t entry_id_{0};
  time_t expiry_time_{0};
  std::string note_;
  bool is_active_{true};
};

template <typename T>
class BanMediator {
 public:
  static Status Unban(std::shared_ptr<rustla2::Ban> ban,
                      std::shared_ptr<T> collection) {
    return SetIsBanned(ban->GetEntryID(), collection, false);
  }

  static Status Ban(std::shared_ptr<T> collection,
                    std::shared_ptr<rustla2::Ban> ban) {
    return SetIsBanned(ban->GetEntryID(), collection, true);
  }

  static void WriteJSON(std::shared_ptr<T> collection,
                        std::shared_ptr<rustla2::Ban> ban,
                        rapidjson::Writer<rapidjson::StringBuffer>* writer);

 private:
  static Status SetIsBanned(const uint64_t entry_id,
                            std::shared_ptr<T> collection, const bool value);
};

template <typename TCollection,
          typename TBanMediator = BanMediator<TCollection>>
class Bans {
 public:
  Bans(sqlite::database db, const std::string& table_name,
       std::shared_ptr<TCollection> collection);

  bool Contains(const uint64_t entry_id) {
    boost::shared_lock<boost::shared_mutex> read_lock(lock_);
    return entry_ids_.count(entry_id);
  }

  size_t Size() {
    boost::shared_lock<boost::shared_mutex> read_lock(lock_);
    return data_.size();
  }

  std::shared_ptr<TCollection> GetCollection() { return collection_; }

  void WriteJSON(rapidjson::Writer<rapidjson::StringBuffer>* writer);

  std::shared_ptr<Ban> Emplace(const uint64_t entry_id,
                               const time_t expiry_time,
                               const std::string& note,
                               Status* status = nullptr);

  Status EraseByID(const uint64_t id);

  Status Erase(std::shared_ptr<Ban> ban);

  void ClearExpired();

 private:
  void InitTable();

  void LoadRecords();

  void Insert(std::shared_ptr<Ban> ban);

  uint64_t GetNextID() { return next_id_++; }

  sqlite::database db_;
  const std::string table_name_;
  std::shared_ptr<TCollection> collection_;
  std::atomic<uint64_t> next_id_{0};
  boost::shared_mutex lock_;
  std::unordered_set<uint64_t> entry_ids_;
  std::unordered_map<uint64_t, std::shared_ptr<Ban>> data_;
};

}  // namespace rustla2

#include "Bans-inl.h"