#include "Bans.h"

namespace rustla2 {

Status Ban::SaveNew() {
  boost::upgrade_lock<boost::shared_mutex> read_lock(lock_);

  try {
    const auto sql = R"sql(
        INSERT INTO `{0}` (
          `id`,
          `entry_id`,
          `expiry_time`,
          `note`,
          `is_active`,
          `created_at`,
          `updated_at`
        )
        VALUES (
          ?,
          ?,
          datetime(?, 'unixepoch'),
          ?,
          ?,
          datetime(),
          datetime()
        );
      )sql";
    auto query = db_ << folly::sformat(sql, table_name_) << id_ << entry_id_
                     << expiry_time_ << note_ << is_active_;
  } catch (const sqlite::sqlite_exception& e) {
    LOG(ERROR) << "error storing ban "
               << "id: " << id_ << ", "
               << "entry_id: " << entry_id_ << ", "
               << "expiry_time: " << expiry_time_ << ", "
               << "note: " << note_ << ", "
               << "is_active: " << is_active_ << ", "
               << "error: " << e.what();

    return Status(StatusCode::DB_ENGINE_ERROR, "error saving ban", e.what());
  }

  return Status::OK;
}

Status Ban::Save() {
  boost::shared_lock<boost::shared_mutex> read_lock(lock_);

  try {
    const auto sql = R"sql(
        UPDATE `{}`
        SET `entry_id` = ?,
        `expiry_time` = ?,
        `note` = ?,
        `is_active` = ?,
        `updated_at` = datetime()
        WHERE `id` = ?
      )sql";
    db_ << folly::sformat(sql, table_name_) << entry_id_ << expiry_time_
        << note_ << is_active_ << id_;
  } catch (const sqlite::sqlite_exception& e) {
    LOG(ERROR) << "error expiring ban "
               << "entry_id: " << entry_id_ << ", "
               << "expiry_time: " << expiry_time_ << ", "
               << "note: " << note_ << ", "
               << "is_active: " << is_active_ << ", "
               << "error: " << e.what();

    return Status::ERROR;
  }

  return Status::OK;
}

void Ban::WriteJSON(rapidjson::Writer<rapidjson::StringBuffer>* writer) {
  boost::shared_lock<boost::shared_mutex> read_lock(lock_);

  writer->StartObject();
  writer->Key("id");
  writer->Uint64(id_);
  writer->Key("entry_id");
  writer->Uint64(entry_id_);
  writer->Key("expiry_time");
  writer->Uint64(expiry_time_);
  writer->Key("note");
  writer->String(note_);
  writer->Key("is_active");
  writer->Bool(is_active_);
  writer->EndObject();
}

}  // namespace rustla2