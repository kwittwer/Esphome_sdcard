#pragma once

#include <sstream>
#include <string>
#include <vector>

#include "esphome/components/time/real_time_clock.h"
#include "esphome/core/component.h"
#include "esphome/core/gpio.h"

#ifdef USE_WEBSERVER
#include "esphome/components/web_server_base/web_server_base.h"
#endif

namespace esphome {
namespace sdcard_logger {

class SDCardLogger : public Component {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;

  void set_cs_pin(GPIOPin *cs_pin) { this->cs_pin_ = cs_pin; }
  void set_time_source(time::RealTimeClock *time_source) { this->time_source_ = time_source; }
  void set_base_dir(const std::string &base_dir);

  void log_measurements(const std::string &filename, const std::vector<std::string> &values);
  void log_event(const std::string &event_text);

  const std::string &get_base_dir() const { return this->base_dir_; }
  bool is_mounted() const { return this->mounted_; }

  template<typename... Args> void log_measurements(const std::string &filename, Args... values) {
    std::vector<std::string> string_values;
    string_values.reserve(sizeof...(values));
    (string_values.push_back(this->to_string_(values)), ...);
    this->log_measurements(filename, string_values);
  }

 protected:
  GPIOPin *cs_pin_{nullptr};
  time::RealTimeClock *time_source_{nullptr};
  std::string base_dir_{"/"};
  bool mounted_{false};

  std::string make_timestamp_() const;
  std::string make_event_filename_() const;
  std::string build_path_(const std::string &filename) const;
  bool append_line_(const std::string &filename, const std::string &line);

#ifdef USE_WEBSERVER
  void setup_web_handler_();
#endif

  template<typename T> std::string to_string_(const T &value) const {
    std::ostringstream stream;
    stream << value;
    return stream.str();
  }

  std::string to_string_(const std::string &value) const { return value; }
  std::string to_string_(const char *value) const;
  std::string to_string_(const String &value) const;
};

#ifdef USE_WEBSERVER

class SDCardFileHandler : public AsyncWebHandler {
 public:
  explicit SDCardFileHandler(SDCardLogger *logger) : logger_(logger) {}

  bool canHandle(AsyncWebServerRequest *request) const override;
  void handleRequest(AsyncWebServerRequest *request) override;
  bool isRequestHandlerTrivial() const override { return false; }

 protected:
  SDCardLogger *logger_;

  std::string build_list_html_() const;
  std::string format_size_(size_t bytes) const;
  bool is_valid_path_(const std::string &name) const;
  static std::string html_escape_(const std::string &s);

  void handle_list_(AsyncWebServerRequest *request);
  void handle_download_(AsyncWebServerRequest *request);
  void handle_delete_(AsyncWebServerRequest *request);
};

#endif  // USE_WEBSERVER

}  // namespace sdcard_logger
}  // namespace esphome
