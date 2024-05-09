#ifndef PACKAGER_LIVE_PACKAGER_LOGGING_H_
#define PACKAGER_LIVE_PACKAGER_LOGGING_H_

#include <vector>
#include <string>
#include <absl/log/log.h>
#include <absl/log/log_sink.h>

namespace shaka::pluto::live {

class LogCollectorSink : public absl::LogSink {
  public:
    LogCollectorSink() = default;
    ~LogCollectorSink() = default;

   virtual void Send(const absl::LogEntry& entry) override;

   const std::vector<std::string>& GetMessages() const;

  private:
   std::vector<std::string> messages_;
   std::vector<absl::LogSeverity> severities_;
   size_t max_message_count_ = 1000;
};

void InitializeLog(absl::LogSeverityAtLeast sev);
void InstallCustomLogSink(absl::LogSink& sink);
void RemoveCustomLogSink(absl::LogSink& sink);

} // namespace shaka::pluto::live


#endif  // PACKAGER_LIVE_PACKAGER_LOGGING_H_