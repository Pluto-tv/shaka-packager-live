#ifndef PACKAGER_PUBLIC_ID3_TAG_H_
#define PACKAGER_PUBLIC_ID3_TAG_H_

#include <cstdint>
#include <list>
#include <memory>
#include <vector>

namespace shaka {

struct Id3TagData {
  int64_t pts = 0;
  std::vector<uint8_t> data;
};

using Id3TagList = std::list<Id3TagData>;
using Id3TagListPtr = std::shared_ptr<Id3TagList>;

}  // namespace shaka

#endif  // PACKAGER_PUBLIC_ID3_TAG_H_
