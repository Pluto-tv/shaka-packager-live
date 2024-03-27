//
// Created by Oleksandr Khodos on 13.03.2024.
//

#ifndef PACKAGER_LIVE_PACKAGER_EXPORT_H
#define PACKAGER_LIVE_PACKAGER_EXPORT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

typedef enum OutputFormat {
  OUTPUT_FORMAT_FMP4,
  OUTPUT_FORMAT_TS,
  OUTPUT_FORMAT_VTTMP4,
  OUTPUT_FORMAT_TTMLMP4,
  OUTPUT_FORMAT_TTML,
} OutputFormat_t;

typedef enum TrackType {
  TRACK_TYPE_AUDIO,
  TRACK_TYPE_VIDEO,
  TRACK_TYPE_TEXT,
} TrackType_t;

typedef enum EncryptionScheme {
  ENCRYPTION_SCHEME_NONE,
  ENCRYPTION_SCHEME_SAMPLE_AES,
  ENCRYPTION_SCHEME_AES_128,
  ENCRYPTION_SCHEME_CBCS,
  ENCRYPTION_SCHEME_CENC,
} EncryptionScheme_t;

static const size_t kIvMaxSize = 16;
static const size_t kKeySize = 16;
static const size_t kKeyIdSize = 16;

typedef struct LivePackagerConfig {
  OutputFormat_t format;
  TrackType_t track_type;

  size_t iv_size;
  uint8_t iv[kIvMaxSize];
  uint8_t key[kKeySize];
  uint8_t key_id[kKeyIdSize];
  EncryptionScheme_t protection_scheme;

  /// User-specified segment number.
  /// For FMP4 output:
  ///   It can be used to set the moof header sequence number if > 0.
  /// For M2TS output:
  ///   It is be used to set the continuity counter.
  uint32_t segment_number;

  /// The offset to be applied to transport stream (e.g. MPEG2-TS, HLS packed
  /// audio) timestamps to compensate for possible negative timestamps in the
  /// input.
  int32_t m2ts_offset_ms;

  /// Used for timed text packaging to set the fragment decode time when the
  /// output format is either VTT in MP4 or TTML in MP4.
  int64_t timed_text_decode_time;
} LivePackagerConfig_t;

typedef struct LivePackager_buffer_s* LivePackagerBuffer_t;

LivePackagerBuffer_t livepackager_buf_new();
void livepackager_buf_free(LivePackagerBuffer_t buf);

const uint8_t* livepackager_buf_data(LivePackagerBuffer_t buf);
size_t livepackager_buf_size(LivePackagerBuffer_t buf);

typedef struct LivePackager_instance_s* LivePackager_t;

LivePackager_t livepackager_new(LivePackagerConfig_t cfg);
void livepackager_free(LivePackager_t lp);

bool livepackager_package_init(LivePackager_t lp,
                               uint8_t* init,
                               size_t init_len,
                               LivePackagerBuffer_t dest);
bool livepackager_package(LivePackager_t lp,
                          uint8_t* init,
                          size_t init_len,
                          uint8_t* media,
                          size_t media_len,
                          LivePackagerBuffer_t dest);
bool livepackager_package_timedtext(LivePackager_t lp,
                                    uint8_t* seg,
                                    size_t seg_len,
                                    LivePackagerBuffer_t dest);

#ifdef __cplusplus
}
#endif

#endif  // PACKAGER_LIVE_PACKAGER_EXPORT_H
