
#include <iostream>
#include <mutex>

#include <absl/base/log_severity.h>
#include <absl/log/globals.h>
#include <absl/log/initialize.h>
#include <absl/log/log.h>
#include <absl/log/log_sink_registry.h>
#include <absl/strings/str_format.h>
#include <packager/live_packager_logging.h>

namespace shaka::pluto::live {

void LogCollectorSink::Send(const absl::LogEntry& entry) {
  if (messages_.size() <= max_message_count_) {
    const auto msg =
        absl::StrFormat("(%s): %s", absl::LogSeverityName(entry.log_severity()),
                        entry.text_message());
    messages_.push_back(msg);
  }
}

const std::vector<std::string>& LogCollectorSink::GetMessages() const {
  return messages_;
}

void InitializeLog(absl::LogSeverityAtLeast sev) {
  static std::once_flag initialized;
  std::call_once(initialized, []() {
    // It is an error to call this function twice.
    absl::InitializeLog();
  });

  absl::SetStderrThreshold(absl::LogSeverityAtLeast::kInfinity);
  absl::SetMinLogLevel(sev);
}

void InstallCustomLogSink(absl::LogSink& sink) {
  absl::AddLogSink(&sink);
}

void RemoveCustomLogSink(absl::LogSink& sink) {
  absl::RemoveLogSink(&sink);
}

}  // namespace shaka::pluto::live