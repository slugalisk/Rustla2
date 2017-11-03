#include "IPRanges.h"

#include <folly/Conv.h>
#include <folly/Format.h>
#include <glog/logging.h>
#include <rapidjson/document.h>
#include <limits>
#include <vector>

namespace rustla2 {

void IPRange::WriteJSON(rapidjson::Writer<rapidjson::StringBuffer>* writer) {
  writer->StartObject();
  writer->Key("id");
  writer->Int(id_);
  writer->Key("start");
  writer->String(start_);
  writer->Key("end");
  writer->String(end_);
  writer->EndObject();
}

IPRanges::IPRanges(sqlite::database db, const std::string& table_name)
    : db_(db), table_name_(table_name) {
  InitTable();

  db_ << folly::sformat("SELECT MAX(id) + 1 FROM {}", table_name_) >>
      [&](uint64_t next_id) { next_id_ = next_id; };

  auto sql = folly::sformat("SELECT `start`, `end` FROM `{}`", table_name_);
  auto query = db_ << sql;

  query >> [&](const std::string start, const std::string end) {
    ranges_.insert(Value::closed(GetAddressValue(start), GetAddressValue(end)));
  };

  LOG(INFO) << "read " << ranges_.size() << " ip ranges from " << table_name;
}

void IPRanges::InitTable() {
  auto sql = R"sql(
      CREATE TABLE IF NOT EXISTS `{}` (
        `id` INT PRIMARY KEY,
        `start` VARCHAR(39),
        `end` VARCHAR(39),
        `note` VARCHAR(255),
        `created_at` DATETIME NOT NULL,
        `updated_at` DATETIME NOT NULL,
        UNIQUE (`start`, `end`)
      )
    )sql";
  db_ << folly::sformat(sql, table_name_);
}

bool IPRanges::Contains(const folly::StringPiece address_str) {
  const auto value = GetAddressValue(address_str);
  if (value == 0) {
    return false;
  }

  boost::shared_lock<boost::shared_mutex> read_lock(lock_);
  return ranges_.find(value) != ranges_.end();
}

void IPRanges::WriteJSON(rapidjson::Writer<rapidjson::StringBuffer>* writer) {
  boost::shared_lock<boost::shared_mutex> read_lock(lock_);

  writer->StartArray();
  for (const auto& it : data_) {
    it.second->WriteJSON(writer);
  }
  writer->EndArray();
}

std::shared_ptr<IPRange> IPRanges::Emplace(const std::string& range_start_str,
                                           const std::string& range_end_str,
                                           const std::string& note,
                                           Status* status) {
  const auto range_start = GetAddressValue(range_start_str);
  const auto range_end = GetAddressValue(range_end_str);
  if (range_start == 0 || range_end == 0) {
    new (status) Status(StatusCode::VALIDATION_ERROR, "invalid ip format");
    return nullptr;
  }

  uint64_t id = GetNextID();

  try {
    DLOG(INFO) << "IPRanges::Emplace inserting record "
               << "table_name: " << table_name_ << ", "
               << "id: " << id << ", "
               << "range_start_str : " << range_start_str << ", "
               << "range_end_str : " << range_end_str << ", "
               << "note: " << note;

    const auto sql = R"sql(
        INSERT INTO `{0}` (
          `id`,
          `start`,
          `end`,
          `note`,
          `created_at`,
          `updated_at`
        )
        VALUES (
          ?,
          ?,
          ?,
          ?,
          datetime(),
          datetime()
        );
      )sql";
    db_ << folly::sformat(sql, table_name_) << id << range_start_str
        << range_end_str << note;
  } catch (const sqlite::sqlite_exception& e) {
    LOG(ERROR) << "error storing ip range "
               << "start: " << range_start_str << ", "
               << "end: " << range_end_str << ", "
               << "note: " << note << ", "
               << "table: " << table_name_ << ", "
               << "error: " << e.what();

    new (status)
        Status(StatusCode::DB_ENGINE_ERROR, "error saving ip range", e.what());
    return nullptr;
  }

  DLOG(INFO) << "IPRanges::Emplace indexing range "
             << "table_name: " << table_name_ << ", "
             << "id: " << id;

  auto range = std::make_shared<IPRange>(id, range_start_str, range_end_str);

  boost::unique_lock<boost::shared_mutex> write_lock(lock_);
  data_[id] = range;
  ranges_.insert(Value::closed(range_start, range_end));

  if (status) *status = Status::OK;
  return range;
}

bool IPRanges::EraseByID(const uint64_t id) {
  db_ << folly::sformat("DELETE FROM `{0}` WHERE id = ?", table_name_) << id;

  boost::upgrade_lock<boost::shared_mutex> read_lock(lock_);
  auto it = data_.find(id);
  if (it == data_.end()) {
    return false;
  }

  const auto range_start = GetAddressValue(it->second->GetStart());
  const auto range_end = GetAddressValue(it->second->GetEnd());

  boost::upgrade_to_unique_lock<boost::shared_mutex> write_lock(read_lock);
  ranges_.erase(Value::closed(range_start, range_end));
  data_.erase(id);

  return true;
}

unsigned __int128 IPRanges::GetAddressValue(
    const folly::StringPiece address_str) {
  if (address_str.size() == 0) {
    return 0;
  }

  boost::system::error_code error;
  auto address =
      boost::asio::ip::address::from_string(address_str.toString(), error);
  if (error) {
    return 0;
  }

  if (address.is_v6()) {
    return PackAddressBytes(address.to_v6().to_bytes(), 16);
  } else if (address.is_v4()) {
    return PackAddressBytes(address.to_v4().to_bytes(), 4);
  }

  return 0;
}

}  // namespace rustla2
