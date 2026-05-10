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

#ifdef USE_WEBSERVER
  this->setup_web_handler_();
#endif
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

// ---------------------------------------------------------------------------
// Web file manager (only compiled when web_server is present in the project)
// ---------------------------------------------------------------------------
#ifdef USE_WEBSERVER

void SDCardLogger::setup_web_handler_() {
  auto *wsb = web_server_base::global_web_server_base;
  if (wsb == nullptr) {
    return;
  }
  // WebServerBase::add_handler stores the pointer and manages its lifetime.
  wsb->add_handler(new SDCardFileHandler(this));
  ESP_LOGI(TAG, "SD card web file manager enabled – open /sd in the browser");
}

// --- SDCardFileHandler ---

bool SDCardFileHandler::canHandle(AsyncWebServerRequest *request) const {
  const auto &url = request->url();
  return url == "/sd" || url == "/sd/download" || url == "/sd/delete";
}

void SDCardFileHandler::handleRequest(AsyncWebServerRequest *request) {
  const auto &url = request->url();
  if (url == "/sd") {
    this->handle_list_(request);
  } else if (url == "/sd/download") {
    this->handle_download_(request);
  } else if (url == "/sd/delete") {
    this->handle_delete_(request);
  } else {
    request->send(404, "text/plain", "Not found");
  }
}

void SDCardFileHandler::handle_list_(AsyncWebServerRequest *request) {
  String html = this->build_list_html_().c_str();
  request->send(200, "text/html; charset=utf-8", html);
}

void SDCardFileHandler::handle_download_(AsyncWebServerRequest *request) {
  if (!request->hasParam("path")) {
    request->send(400, "text/plain", "Missing path parameter");
    return;
  }
  const std::string name = request->getParam("path")->value().c_str();
  if (!this->is_valid_path_(name)) {
    request->send(400, "text/plain", "Invalid path");
    return;
  }
  const std::string &base = this->logger_->get_base_dir();
  std::string full_path = (base == "/") ? "/" + name : base + "/" + name;

  if (!SD.exists(full_path.c_str())) {
    request->send(404, "text/plain", "File not found");
    return;
  }
  request->send(SD, full_path.c_str(), "application/octet-stream", true);
}

void SDCardFileHandler::handle_delete_(AsyncWebServerRequest *request) {
  if (!request->hasParam("path")) {
    request->send(400, "text/plain", "Missing path parameter");
    return;
  }
  const std::string name = request->getParam("path")->value().c_str();
  if (!this->is_valid_path_(name)) {
    request->send(400, "text/plain", "Invalid path");
    return;
  }
  const std::string &base = this->logger_->get_base_dir();
  std::string full_path = (base == "/") ? "/" + name : base + "/" + name;

  if (!SD.exists(full_path.c_str())) {
    request->send(404, "text/plain", "File not found");
    return;
  }
  if (SD.remove(full_path.c_str())) {
    request->redirect("/sd");
  } else {
    request->send(500, "text/plain", "Failed to delete file");
  }
}

bool SDCardFileHandler::is_valid_path_(const std::string &name) const {
  if (name.empty()) {
    return false;
  }
  // Reject absolute paths and any directory traversal attempts
  if (name[0] == '/' || name.find("..") != std::string::npos) {
    return false;
  }
  return true;
}

std::string SDCardFileHandler::html_escape_(const std::string &s) {
  std::string out;
  out.reserve(s.size());
  for (char c : s) {
    switch (c) {
      case '&':
        out += "&amp;";
        break;
      case '<':
        out += "&lt;";
        break;
      case '>':
        out += "&gt;";
        break;
      case '"':
        out += "&quot;";
        break;
      case '\'':
        out += "&#39;";
        break;
      default:
        out += c;
        break;
    }
  }
  return out;
}

std::string SDCardFileHandler::format_size_(size_t bytes) const {  char buf[32];
  if (bytes < 1024) {
    snprintf(buf, sizeof(buf), "%u B", (unsigned) bytes);
  } else if (bytes < 1024 * 1024) {
    snprintf(buf, sizeof(buf), "%.1f KiB", bytes / 1024.0f);
  } else {
    snprintf(buf, sizeof(buf), "%.1f MiB", bytes / (1024.0f * 1024.0f));
  }
  return buf;
}

std::string SDCardFileHandler::build_list_html_() const {
  const std::string &base = this->logger_->get_base_dir();

  std::string html =
      "<!DOCTYPE html><html lang=\"en\"><head>"
      "<meta charset=\"utf-8\">"
      "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
      "<title>SD Card Files</title>"
      "<style>"
      "*{box-sizing:border-box}"
      "body{font-family:sans-serif;margin:0;padding:20px;background:#f4f4f4}"
      ".card{background:#fff;border-radius:8px;padding:24px;box-shadow:0 2px 6px rgba(0,0,0,.12);max-width:860px;margin:0 auto}"
      "h1{margin:0 0 4px;color:#333;font-size:1.4em}"
      ".dir{color:#666;font-size:.9em;margin:0 0 16px}"
      "table{width:100%;border-collapse:collapse}"
      "th{background:#3c8dbc;color:#fff;padding:10px 12px;text-align:left;font-weight:600}"
      "td{padding:9px 12px;border-bottom:1px solid #eee}"
      "tr:last-child td{border-bottom:none}"
      "tr:hover td{background:#f9f9f9}"
      "a.dl{color:#3c8dbc;text-decoration:none}"
      "a.dl:hover{text-decoration:underline}"
      "a.del{color:#c0392b;text-decoration:none}"
      "a.del:hover{text-decoration:underline}"
      ".empty{color:#999;font-style:italic;text-align:center;padding:20px}"
      "</style></head><body><div class=\"card\">"
      "<h1>&#128193; SD Card File Manager</h1>"
      "<p class=\"dir\">Directory: <code>";
  html += SDCardFileHandler::html_escape_(base);
  html +=
      "</code></p>"
      "<table><tr><th>File</th><th>Size</th><th>Download</th><th>Delete</th></tr>";

  if (!this->logger_->is_mounted()) {
    html += "<tr><td colspan=\"4\" class=\"empty\">SD card is not mounted.</td></tr>";
  } else {
    File dir = SD.open(base.c_str());
    bool found = false;
    if (dir && dir.isDirectory()) {
      File entry = dir.openNextFile();
      while (entry) {
        if (!entry.isDirectory()) {
          // entry.name() may return the full path on some SDK versions – extract basename
          std::string full_name = entry.name();
          size_t slash = full_name.rfind('/');
          std::string name = (slash != std::string::npos) ? full_name.substr(slash + 1) : full_name;

          std::string size_str = this->format_size_(entry.size());
          std::string safe_name = SDCardFileHandler::html_escape_(name);
          found = true;

          html += "<tr><td>";
          html += safe_name;
          html += "</td><td>";
          html += size_str;
          html += "</td><td><a class=\"dl\" href=\"/sd/download?path=";
          html += safe_name;
          html += "\">&#11015; Download</a></td><td><a class=\"del\" href=\"/sd/delete?path=";
          html += safe_name;
          html += "\" onclick=\"return confirm('Delete ";
          html += safe_name;
          html += "?')\">&#128465; Delete</a></td></tr>";
        }
        entry.close();
        entry = dir.openNextFile();
      }
      dir.close();
    }
    if (!found) {
      html += "<tr><td colspan=\"4\" class=\"empty\">No files found.</td></tr>";
    }
  }

  html += "</table></div></body></html>";
  return html;
}

#endif  // USE_WEBSERVER

}  // namespace sdcard_logger
}  // namespace esphome
