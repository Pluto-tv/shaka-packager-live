#ifndef PACKAGER_PUBLIC_ID3_TAG_H_
#define PACKAGER_PUBLIC_ID3_TAG_H_

#include <cstdint>
#include <list>
#include <memory>
#include <string>
#include <vector>

namespace shaka {

struct Id3TagData {
  // Presentation timestamp of the event, in stream timescale.
  int64_t pts = 0;
  // emsg scheme_id_uri, e.g. "https://aomedia.org/emsg/ID3".
  std::string scheme_id_uri;
  // emsg "value" string. Often empty.
  std::string value;
  // Unique emsg id; caller-supplied (no auto-generation).
  uint32_t id = 0;
  // emsg event_duration, in stream timescale.
  uint32_t event_duration = 0;
  // The ID3v2 payload that becomes emsg message_data.
  std::vector<uint8_t> data;
};

using Id3TagList = std::list<Id3TagData>;
using Id3TagListPtr = std::shared_ptr<Id3TagList>;

}  // namespace shaka

#endif  // PACKAGER_PUBLIC_ID3_TAG_H_
