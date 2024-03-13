//
// Created by Oleksandr Khodos on 13.03.2024.
//

#ifndef PACKAGER_LIVE_PACKAGER_EXPORT_H
#define PACKAGER_LIVE_PACKAGER_EXPORT_H

#ifdef __cplusplus
extern "C"
{
#endif

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

#define KEY_IV_LEN 16

typedef struct LivePackagerConfig {
    OutputFormat_t format;
    TrackType_t track_type;

    uint8_t iv    [KEY_IV_LEN];
    uint8_t key   [KEY_IV_LEN];
    uint8_t key_id[KEY_IV_LEN];
    EncryptionScheme_t protection_scheme;

    uint32_t segment_number;
    int32_t m2ts_offset_ms;
    int64_t timed_text_decode_time;
} LivePackagerConfig_t;

typedef struct LivePackager_instance_s* LivePackager_t;

LivePackager_t livepackager_new(LivePackagerConfig_t cfg);
void livepackager_free(LivePackager_t lp);

size_t livepackager_package_init(LivePackager_t lp, uint8_t* init, size_t init_len, uint8_t* dest);
size_t livepackager_package(LivePackager_t lp, uint8_t* init, size_t init_len, uint8_t* seg, size_t seg_len, uint8_t* dest);
size_t livepackager_package_timedtext(LivePackager_t lp, uint8_t* seg, size_t seg_len, uint8_t* dest);

#ifdef __cplusplus
}
#endif


#endif //PACKAGER_LIVE_PACKAGER_EXPORT_H