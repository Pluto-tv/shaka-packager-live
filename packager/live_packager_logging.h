#ifndef PACKAGER_LIVE_PACKAGER_LOGGING_H_
#define PACKAGER_LIVE_PACKAGER_LOGGING_H_

#include <absl/log/log.h>
#include <absl/log/log_sink.h>
#include <functional>
#include <utility>

namespace shaka::pluto::live {

class LogCollectorSink : public absl::LogSink {
 public:
  LogCollectorSink(std::function<void(const absl::LogEntry&)> logSink) : logSink_(std::move(logSink)) {}
  ~LogCollectorSink() override = default;

  void Send(const absl::LogEntry& entry) override;

 private:
  std::function<void(const absl::LogEntry&)> logSink_;
  size_t max_message_count_ = 1000;
};

void InitializeLog(absl::LogSeverityAtLeast sev);
void InstallCustomLogSink(absl::LogSink& sink);
void RemoveCustomLogSink(absl::LogSink& sink);

}  // namespace shaka::pluto::live

#endif  // PACKAGER_LIVE_PACKAGER_LOGGING_H_