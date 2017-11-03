#pragma once

#include <folly/String.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <sqlite_modern_cpp.h>
#include <atomic>
#include <boost/asio/ip/address.hpp>
#include <boost/icl/interval_set.hpp>
#include <boost/thread/shared_mutex.hpp>
#include <cstdint>
#include <cstring>
#include <memory>

#include "Bans.h"
#include "Status.h"

namespace rustla2 {

class IPRange {
 public:
  IPRange(const uint64_t id, const std::string& start, const std::string& end)
      : id_(id), start_(start), end_(end) {}

  uint64_t GetID() const { return id_; }

  std::string GetStart() const { return start_; }

  std::string GetEnd() const { return end_; }

  void WriteJSON(rapidjson::Writer<rapidjson::StringBuffer>* writer);

 private:
  const uint64_t id_;
  const std::string start_;
  const std::string end_;
};

class IPRanges {
 public:
  using ValueRanges = boost::icl::interval_set<unsigned __int128>;
  using Value = ValueRanges::interval_type;

  IPRanges(sqlite::database db, const std::string& table_name);

  void InitTable();

  bool Contains(const folly::StringPiece address_str);

  void WriteJSON(rapidjson::Writer<rapidjson::StringBuffer>* writer);

  std::shared_ptr<IPRange> Emplace(const std::string& address_str,
                                   const std::string& note = "",
                                   Status* status = nullptr) {
    return Emplace(address_str, address_str, note, status);
  }

  std::shared_ptr<IPRange> Emplace(const std::string& range_start_str,
                                   const std::string& range_end_str,
                                   const std::string& note = "",
                                   Status* status = nullptr);

  bool EraseByID(const uint64_t id);

  std::shared_ptr<IPRange> GetByID(uint64_t id) {
    boost::shared_lock<boost::shared_mutex> read_lock(lock_);
    const auto i = data_.find(id);
    return i == data_.end() ? nullptr : i->second;
  }

  size_t CountID(const uint64_t id) {
    boost::shared_lock<boost::shared_mutex> read_lock(lock_);
    return data_.count(id);
  }

 private:
  unsigned __int128 GetAddressValue(const folly::StringPiece address_str);

  unsigned __int128 GetAddressValue(const std::string& address_str) {
    return GetAddressValue(folly::StringPiece(address_str));
  }

  template <typename T>
  unsigned __int128 PackAddressBytes(T bytes, size_t length) {
    // prefix ipv4 addresses with 0xffff
    unsigned __int128 value = std::numeric_limits<uint16_t>::max();

    for (int i = 0; i < length; ++i) {
      value = (value << 8) | bytes[i];
    }
    return value;
  }

  uint64_t GetNextID() { return next_id_++; }

  sqlite::database db_;
  const std::string table_name_;
  boost::shared_mutex lock_;
  std::atomic<uint64_t> next_id_{0};
  ValueRanges ranges_;
  std::unordered_map<uint64_t, std::shared_ptr<IPRange>> data_;
};

class IPRangeBanMediator : public BanMediator<IPRanges> {
 public:
  static Status Unban(std::shared_ptr<rustla2::Ban> ban,
                      std::shared_ptr<IPRanges> ranges) {
    ranges->EraseByID(ban->GetEntryID());
    return Status::OK;
  }

  static Status Ban(std::shared_ptr<IPRanges> ranges,
                    std::shared_ptr<rustla2::Ban> ban) {
    return ranges->CountID(ban->GetEntryID())
               ? Status::OK
               : Status(StatusCode::ID_ERROR, "entry id not found");
  }

  using BanMediator<IPRanges>::WriteJSON;
};

}  // namespace rustla2
