#include "sdcard_logger.h"

#include <SD.h>

#include "esphome/core/log.h"

namespace esphome {
namespace sdcard_logger {

static const char *const TAG = "sdcard_logger";

void SDCardLogger::setup() {
  if (this->cs_pin_ == nullptr) {
    ESP_LOGE(TAG, "No CS pin configuration found.");
    this->mark_failed();
    return;
  }

  this->cs_pin_->setup();

  if (!SD.begin(this->cs_pin_->get_pin())) {
    ESP_LOGE(TAG, "Failed to mount SD card.");
    this->mark_failed();
    return;
  }

  this->mounted_ = true;
  ESP_LOGI(TAG, "SD card mounted successfully.");
}

void SDCardLogger::dump_config() {
  ESP_LOGCONFIG(TAG, "SD Card Logger:");
  LOG_PIN("  CS Pin: ", this->cs_pin_);
  ESP_LOGCONFIG(TAG, "  Base directory: %s", this->base_dir_.c_str());
}

float SDCardLogger::get_setup_priority() const { return setup_priority::HARDWARE; }

void SDCardLogger::set_base_dir(const std::string &base_dir) {
  if (base_dir.empty()) {
    this->base_dir_ = "/";
    return;
  }

  if (base_dir[0] == '/') {
    this->base_dir_ = base_dir;
  } else {
    this->base_dir_ = "/" + base_dir;
  }

  while (this->base_dir_.size() > 1 && this->base_dir_.back() == '/') {
    this->base_dir_.pop_back();
  }
}

void SDCardLogger::log_measurements(const std::string &filename, const std::vector<std::string> &values) {
  std::string line = this->make_timestamp_();
  for (const auto &value : values) {
    line += ";";
    line += value;
  }

  this->append_line_(filename, line);
}

void SDCardLogger::log_event(const std::string &event_text) {
  const std::string filename = this->make_event_filename_();
  const std::string line = this->make_timestamp_() + ";" + event_text;
  this->append_line_(filename, line);
}

std::string SDCardLogger::make_timestamp_() const {
  if (this->time_source_ == nullptr) {
    ESP_LOGW(TAG, "No time component set, using fallback timestamp.");
    return "1970-01-01 00:00:00";
  }

  auto now = this->time_source_->now();
  if (!now.is_valid()) {
    ESP_LOGW(TAG, "Time is not yet valid, using fallback timestamp.");
    return "1970-01-01 00:00:00";
  }

  return now.strftime("%Y-%m-%d %H:%M:%S");
}

std::string SDCardLogger::make_event_filename_() const {
  if (this->time_source_ == nullptr) {
    return "unknown-date.log";
  }

  auto now = this->time_source_->now();
  if (!now.is_valid()) {
    return "unknown-date.log";
  }

  return now.strftime("%Y-%m-%d") + ".log";
}

std::string SDCardLogger::build_path_(const std::string &filename) const {
  if (filename.empty()) {
    return this->base_dir_ + "/log.txt";
  }

  if (filename[0] == '/') {
    return filename;
  }

  if (this->base_dir_ == "/") {
    return "/" + filename;
  }

  return this->base_dir_ + "/" + filename;
}

bool SDCardLogger::append_line_(const std::string &filename, const std::string &line) {
  if (!this->mounted_) {
    ESP_LOGE(TAG, "SD card is not mounted.");
    return false;
  }

  const std::string path = this->build_path_(filename);

  File file = SD.open(path.c_str(), FILE_APPEND);
  if (!file) {
    if (SD.exists(path.c_str())) {
      ESP_LOGE(TAG, "Failed to open existing file for append: %s", path.c_str());
      return false;
    }

    file = SD.open(path.c_str(), FILE_WRITE);
  }

  if (!file) {
    ESP_LOGE(TAG, "Failed to open file: %s", path.c_str());
    return false;
  }

  const size_t bytes_written = file.println(line.c_str());
  file.close();

  if (bytes_written == 0) {
    ESP_LOGE(TAG, "Failed to write to file: %s", path.c_str());
    return false;
  }

  return true;
}

std::string SDCardLogger::to_string_(const char *value) const {
  if (value == nullptr) {
    return "";
  }
  return value;
}

std::string SDCardLogger::to_string_(const String &value) const { return value.c_str(); }

}  // namespace sdcard_logger
}  // namespace esphome
