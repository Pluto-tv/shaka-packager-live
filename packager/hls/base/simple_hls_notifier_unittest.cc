// Copyright 2016 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>

#include "packager/base/base64.h"
#include "packager/base/files/file_path.h"
#include "packager/hls/base/mock_media_playlist.h"
#include "packager/hls/base/simple_hls_notifier.h"
#include "packager/media/base/fixed_key_source.h"
#include "packager/media/base/protection_system_specific_info.h"
#include "packager/media/base/widevine_key_source.h"
#include "packager/media/base/widevine_pssh_data.pb.h"

namespace shaka {
namespace hls {

using ::testing::Eq;
using ::testing::InSequence;
using ::testing::Mock;
using ::testing::Return;
using ::testing::StrEq;
using ::testing::_;

namespace {
const char kMasterPlaylistName[] = "master.m3u8";
const HlsPlaylistType kVodPlaylist = HlsPlaylistType::kVod;

class MockMasterPlaylist : public MasterPlaylist {
 public:
  MockMasterPlaylist() : MasterPlaylist(kMasterPlaylistName) {}

  MOCK_METHOD1(AddMediaPlaylist, void(MediaPlaylist* media_playlist));
  MOCK_METHOD2(WriteMasterPlaylist,
               bool(const std::string& prefix, const std::string& output_dir));
};

class MockMediaPlaylistFactory : public MediaPlaylistFactory {
 public:
  MOCK_METHOD5(CreateMock,
               MediaPlaylist*(HlsPlaylistType type,
                              double time_shift_buffer_depth,
                              const std::string& file_name,
                              const std::string& name,
                              const std::string& group_id));

  std::unique_ptr<MediaPlaylist> Create(HlsPlaylistType type,
                                        double time_shift_buffer_depth,
                                        const std::string& file_name,
                                        const std::string& name,
                                        const std::string& group_id) override {
    return std::unique_ptr<MediaPlaylist>(
        CreateMock(type, time_shift_buffer_depth, file_name, name, group_id));
  }
};

const double kTestTimeShiftBufferDepth = 1800.0;
const char kTestPrefix[] = "http://testprefix.com/";
const char kAnyOutputDir[] = "anything/";

const uint64_t kAnyStartTime = 10;
const uint64_t kAnyDuration = 1000;
const uint64_t kAnySize = 2000;

MATCHER_P(SegmentTemplateEq, expected_template, "") {
  *result_listener << " which is " << arg.segment_template();
  return arg.segment_template() == expected_template;
}

const char kCencProtectionScheme[] = "cenc";
const char kSampleAesProtectionScheme[] = "cbca";

}  // namespace

//      : notifier_(profile,
//                  kTestTimeShiftBufferDepth,
//                  kTestPrefix,
//                  kAnyOutputDir,
//                  kMasterPlaylistName),

class SimpleHlsNotifierTest : public ::testing::Test {
 protected:
  SimpleHlsNotifierTest() : SimpleHlsNotifierTest(kVodPlaylist) {}

  SimpleHlsNotifierTest(HlsPlaylistType playlist_type)
      : widevine_system_id_(
            media::kWidevineSystemId,
            media::kWidevineSystemId + arraysize(media::kWidevineSystemId)),
        common_system_id_(
            media::kCommonSystemId,
            media::kCommonSystemId + arraysize(media::kCommonSystemId)) {}

  void InjectMediaPlaylistFactory(std::unique_ptr<MediaPlaylistFactory> factory,
                                  SimpleHlsNotifier* notifier) {
    notifier->media_playlist_factory_ = std::move(factory);
  }

  void InjectMasterPlaylist(std::unique_ptr<MasterPlaylist> playlist,
                            SimpleHlsNotifier* notifier) {
    notifier->master_playlist_ = std::move(playlist);
  }

  size_t NumRegisteredMediaPlaylists(const SimpleHlsNotifier& notifier) {
    return notifier.stream_map_.size();
  }

  uint32_t SetupStream(const std::string& protection_scheme,
                       MockMediaPlaylist* mock_media_playlist,
                       SimpleHlsNotifier* notifier) {
    MediaInfo media_info;
    media_info.mutable_protected_content()->set_protection_scheme(
        protection_scheme);
    std::unique_ptr<MockMasterPlaylist> mock_master_playlist(
        new MockMasterPlaylist());
    std::unique_ptr<MockMediaPlaylistFactory> factory(
        new MockMediaPlaylistFactory());

    EXPECT_CALL(
        *mock_master_playlist,
        AddMediaPlaylist(static_cast<MediaPlaylist*>(mock_media_playlist)));
    EXPECT_CALL(*mock_media_playlist, SetMediaInfo(_)).WillOnce(Return(true));
    EXPECT_CALL(*factory, CreateMock(_, _, _, _, _))
        .WillOnce(Return(mock_media_playlist));

    InjectMasterPlaylist(std::move(mock_master_playlist), notifier);
    InjectMediaPlaylistFactory(std::move(factory), notifier);
    EXPECT_TRUE(notifier->Init());
    uint32_t stream_id;
    EXPECT_TRUE(notifier->NotifyNewStream(media_info, "playlist.m3u8", "name",
                                          "groupid", &stream_id));
    return stream_id;
  }

  const std::vector<uint8_t> widevine_system_id_;
  const std::vector<uint8_t> common_system_id_;
};

TEST_F(SimpleHlsNotifierTest, Init) {
  SimpleHlsNotifier notifier(kVodPlaylist, kTestTimeShiftBufferDepth,
                             kTestPrefix, kAnyOutputDir, kMasterPlaylistName);
  EXPECT_TRUE(notifier.Init());
}

// Verify that relative paths can be handled.
// For this test, since the prefix "anything/" matches, the prefix should be
// stripped.
TEST_F(SimpleHlsNotifierTest, RebaseSegmentTemplateRelative) {
  std::unique_ptr<MockMasterPlaylist> mock_master_playlist(
      new MockMasterPlaylist());
  std::unique_ptr<MockMediaPlaylistFactory> factory(
      new MockMediaPlaylistFactory());

  // Pointer released by SimpleHlsNotifier.
  MockMediaPlaylist* mock_media_playlist =
      new MockMediaPlaylist(kVodPlaylist, "playlist.m3u8", "", "");
  EXPECT_CALL(*mock_master_playlist, AddMediaPlaylist(mock_media_playlist));

  EXPECT_CALL(*mock_media_playlist,
              SetMediaInfo(SegmentTemplateEq("path/to/media$Number$.ts")))
      .WillOnce(Return(true));

  // Verify that the common prefix is stripped for AddSegment().
  EXPECT_CALL(
      *mock_media_playlist,
      AddSegment("http://testprefix.com/path/to/media1.ts", _, _, _, _));
  EXPECT_CALL(*factory, CreateMock(kVodPlaylist, Eq(kTestTimeShiftBufferDepth),
                                   StrEq("video_playlist.m3u8"), StrEq("name"),
                                   StrEq("groupid")))
      .WillOnce(Return(mock_media_playlist));

  SimpleHlsNotifier notifier(kVodPlaylist, kTestTimeShiftBufferDepth,
                             kTestPrefix, kAnyOutputDir, kMasterPlaylistName);

  InjectMasterPlaylist(std::move(mock_master_playlist), &notifier);
  InjectMediaPlaylistFactory(std::move(factory), &notifier);

  EXPECT_TRUE(notifier.Init());
  MediaInfo media_info;
  media_info.set_segment_template("anything/path/to/media$Number$.ts");
  uint32_t stream_id;
  EXPECT_TRUE(notifier.NotifyNewStream(media_info, "video_playlist.m3u8",
                                       "name", "groupid", &stream_id));

  EXPECT_TRUE(notifier.NotifyNewSegment(stream_id, "anything/path/to/media1.ts",
                                        kAnyStartTime, kAnyDuration, 0,
                                        kAnySize));
}

// Verify that when segment template's prefix and output dir match, then the
// prefix is stripped from segment template.
TEST_F(SimpleHlsNotifierTest,
       RebaseAbsoluteSegmentTemplatePrefixAndOutputDirMatch) {
  const char kAbsoluteOutputDir[] = "/tmp/something/";
  SimpleHlsNotifier test_notifier(kVodPlaylist, kTestTimeShiftBufferDepth,
                                  kTestPrefix, kAbsoluteOutputDir,
                                  kMasterPlaylistName);

  std::unique_ptr<MockMasterPlaylist> mock_master_playlist(
      new MockMasterPlaylist());
  std::unique_ptr<MockMediaPlaylistFactory> factory(
      new MockMediaPlaylistFactory());

  // Pointer released by SimpleHlsNotifier.
  MockMediaPlaylist* mock_media_playlist =
      new MockMediaPlaylist(kVodPlaylist, "playlist.m3u8", "", "");
  EXPECT_CALL(*mock_master_playlist, AddMediaPlaylist(mock_media_playlist));

  EXPECT_CALL(*mock_media_playlist,
              SetMediaInfo(SegmentTemplateEq("media$Number$.ts")))
      .WillOnce(Return(true));

  // Verify that the output_dir is stripped and then kTestPrefix is prepended.
  EXPECT_CALL(*mock_media_playlist,
              AddSegment("http://testprefix.com/media1.ts", _, _, _, _));
  EXPECT_CALL(*factory, CreateMock(kVodPlaylist, Eq(kTestTimeShiftBufferDepth),
                                   StrEq("video_playlist.m3u8"), StrEq("name"),
                                   StrEq("groupid")))
      .WillOnce(Return(mock_media_playlist));

  InjectMasterPlaylist(std::move(mock_master_playlist), &test_notifier);
  InjectMediaPlaylistFactory(std::move(factory), &test_notifier);
  EXPECT_TRUE(test_notifier.Init());
  MediaInfo media_info;
  media_info.set_segment_template("/tmp/something/media$Number$.ts");
  uint32_t stream_id;
  EXPECT_TRUE(test_notifier.NotifyNewStream(media_info, "video_playlist.m3u8",
                                            "name", "groupid", &stream_id));

  EXPECT_TRUE(
      test_notifier.NotifyNewSegment(stream_id, "/tmp/something/media1.ts",
                                     kAnyStartTime, kAnyDuration, 0, kAnySize));
}

// If the paths don't match at all and they are both absolute and completely
// different, then keep it as is.
TEST_F(SimpleHlsNotifierTest,
       RebaseAbsoluteSegmentTemplateCompletelyDifferentDirectory) {
  const char kAbsoluteOutputDir[] = "/tmp/something/";
  SimpleHlsNotifier test_notifier(kVodPlaylist, kTestTimeShiftBufferDepth,
                                  kTestPrefix, kAbsoluteOutputDir,
                                  kMasterPlaylistName);

  std::unique_ptr<MockMasterPlaylist> mock_master_playlist(
      new MockMasterPlaylist());
  std::unique_ptr<MockMediaPlaylistFactory> factory(
      new MockMediaPlaylistFactory());

  // Pointer released by SimpleHlsNotifier.
  MockMediaPlaylist* mock_media_playlist =
      new MockMediaPlaylist(kVodPlaylist, "playlist.m3u8", "", "");
  EXPECT_CALL(*mock_master_playlist, AddMediaPlaylist(mock_media_playlist));

  EXPECT_CALL(
      *mock_media_playlist,
      SetMediaInfo(SegmentTemplateEq("/var/somewhereelse/media$Number$.ts")))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_media_playlist,
              AddSegment("http://testprefix.com//var/somewhereelse/media1.ts",
                         _, _, _, _));
  EXPECT_CALL(*factory, CreateMock(kVodPlaylist, Eq(kTestTimeShiftBufferDepth),
                                   StrEq("video_playlist.m3u8"), StrEq("name"),
                                   StrEq("groupid")))
      .WillOnce(Return(mock_media_playlist));

  InjectMasterPlaylist(std::move(mock_master_playlist), &test_notifier);
  InjectMediaPlaylistFactory(std::move(factory), &test_notifier);
  EXPECT_TRUE(test_notifier.Init());
  MediaInfo media_info;
  media_info.set_segment_template("/var/somewhereelse/media$Number$.ts");
  uint32_t stream_id;
  EXPECT_TRUE(test_notifier.NotifyNewStream(media_info, "video_playlist.m3u8",
                                            "name", "groupid", &stream_id));
  EXPECT_TRUE(
      test_notifier.NotifyNewSegment(stream_id, "/var/somewhereelse/media1.ts",
                                     kAnyStartTime, kAnyDuration, 0, kAnySize));
}

TEST_F(SimpleHlsNotifierTest, Flush) {
  SimpleHlsNotifier notifier(kVodPlaylist, kTestTimeShiftBufferDepth,
                             kTestPrefix, kAnyOutputDir, kMasterPlaylistName);
  std::unique_ptr<MockMasterPlaylist> mock_master_playlist(
      new MockMasterPlaylist());
  EXPECT_CALL(*mock_master_playlist,
              WriteMasterPlaylist(StrEq(kTestPrefix), StrEq(kAnyOutputDir)))
      .WillOnce(Return(true));
  InjectMasterPlaylist(std::move(mock_master_playlist), &notifier);
  EXPECT_TRUE(notifier.Init());
  EXPECT_TRUE(notifier.Flush());
}

TEST_F(SimpleHlsNotifierTest, NotifyNewStream) {
  std::unique_ptr<MockMasterPlaylist> mock_master_playlist(
      new MockMasterPlaylist());
  std::unique_ptr<MockMediaPlaylistFactory> factory(
      new MockMediaPlaylistFactory());

  // Pointer released by SimpleHlsNotifier.
  MockMediaPlaylist* mock_media_playlist =
      new MockMediaPlaylist(kVodPlaylist, "playlist.m3u8", "", "");
  EXPECT_CALL(*mock_master_playlist, AddMediaPlaylist(mock_media_playlist));

  EXPECT_CALL(*mock_media_playlist, SetMediaInfo(_)).WillOnce(Return(true));
  EXPECT_CALL(*factory, CreateMock(kVodPlaylist, Eq(kTestTimeShiftBufferDepth),
                                   StrEq("video_playlist.m3u8"), StrEq("name"),
                                   StrEq("groupid")))
      .WillOnce(Return(mock_media_playlist));

  SimpleHlsNotifier notifier(kVodPlaylist, kTestTimeShiftBufferDepth,
                             kTestPrefix, kAnyOutputDir, kMasterPlaylistName);

  InjectMasterPlaylist(std::move(mock_master_playlist), &notifier);
  InjectMediaPlaylistFactory(std::move(factory), &notifier);
  EXPECT_TRUE(notifier.Init());
  MediaInfo media_info;
  uint32_t stream_id;
  EXPECT_TRUE(notifier.NotifyNewStream(media_info, "video_playlist.m3u8",
                                       "name", "groupid", &stream_id));
  EXPECT_EQ(1u, NumRegisteredMediaPlaylists(notifier));
}

TEST_F(SimpleHlsNotifierTest, NotifyNewSegment) {
  std::unique_ptr<MockMasterPlaylist> mock_master_playlist(
      new MockMasterPlaylist());
  std::unique_ptr<MockMediaPlaylistFactory> factory(
      new MockMediaPlaylistFactory());

  // Pointer released by SimpleHlsNotifier.
  MockMediaPlaylist* mock_media_playlist =
      new MockMediaPlaylist(kVodPlaylist, "playlist.m3u8", "", "");

  EXPECT_CALL(
      *mock_master_playlist,
      AddMediaPlaylist(static_cast<MediaPlaylist*>(mock_media_playlist)));
  EXPECT_CALL(*mock_media_playlist, SetMediaInfo(_)).WillOnce(Return(true));
  EXPECT_CALL(*factory, CreateMock(_, _, _, _, _))
      .WillOnce(Return(mock_media_playlist));

  const uint64_t kStartTime = 1328;
  const uint64_t kDuration = 398407;
  const uint64_t kSize = 6595840;
  const std::string segment_name = "segmentname";
  EXPECT_CALL(*mock_media_playlist,
              AddSegment(StrEq(kTestPrefix + segment_name), kStartTime,
                         kDuration, 203, kSize));

  const double kLongestSegmentDuration = 11.3;
  const uint32_t kTargetDuration = 12;  // ceil(kLongestSegmentDuration).
  EXPECT_CALL(*mock_media_playlist, GetLongestSegmentDuration())
      .WillOnce(Return(kLongestSegmentDuration));

  SimpleHlsNotifier notifier(kVodPlaylist, kTestTimeShiftBufferDepth,
                             kTestPrefix, kAnyOutputDir, kMasterPlaylistName);
  MockMasterPlaylist* mock_master_playlist_ptr = mock_master_playlist.get();
  InjectMasterPlaylist(std::move(mock_master_playlist), &notifier);
  InjectMediaPlaylistFactory(std::move(factory), &notifier);
  EXPECT_TRUE(notifier.Init());
  MediaInfo media_info;
  uint32_t stream_id;
  EXPECT_TRUE(notifier.NotifyNewStream(media_info, "playlist.m3u8", "name",
                                       "groupid", &stream_id));

  EXPECT_TRUE(notifier.NotifyNewSegment(stream_id, segment_name, kStartTime,
                                        kDuration, 203, kSize));

  Mock::VerifyAndClearExpectations(mock_master_playlist_ptr);
  Mock::VerifyAndClearExpectations(mock_media_playlist);

  EXPECT_CALL(*mock_master_playlist_ptr,
              WriteMasterPlaylist(StrEq(kTestPrefix), StrEq(kAnyOutputDir)))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_media_playlist, SetTargetDuration(kTargetDuration))
      .Times(1);
  EXPECT_CALL(*mock_media_playlist,
              WriteToFile(StrEq(
                  base::FilePath::FromUTF8Unsafe(kAnyOutputDir)
                      .Append(base::FilePath::FromUTF8Unsafe("playlist.m3u8"))
                      .AsUTF8Unsafe())))
      .WillOnce(Return(true));
  EXPECT_TRUE(notifier.Flush());
}

TEST_F(SimpleHlsNotifierTest, NotifyNewSegmentWithoutStreamsRegistered) {
  SimpleHlsNotifier notifier(kVodPlaylist, kTestTimeShiftBufferDepth,
                             kTestPrefix, kAnyOutputDir, kMasterPlaylistName);
  EXPECT_TRUE(notifier.Init());
  EXPECT_FALSE(notifier.NotifyNewSegment(1u, "anything", 0u, 0u, 0u, 0u));
}

TEST_F(SimpleHlsNotifierTest, NotifyEncryptionUpdateWidevine) {
  // Pointer released by SimpleHlsNotifier.
  MockMediaPlaylist* mock_media_playlist =
      new MockMediaPlaylist(kVodPlaylist, "playlist.m3u8", "", "");
  SimpleHlsNotifier notifier(kVodPlaylist, kTestTimeShiftBufferDepth,
                             kTestPrefix, kAnyOutputDir, kMasterPlaylistName);
  const uint32_t stream_id =
      SetupStream(kSampleAesProtectionScheme, mock_media_playlist, &notifier);

  const std::vector<uint8_t> iv(16, 0x45);

  media::WidevinePsshData widevine_pssh_data;
  widevine_pssh_data.set_provider("someprovider");
  widevine_pssh_data.set_content_id("contentid");
  const uint8_t kAnyKeyId[] = {
      0x11, 0x22, 0x33, 0x44, 0x11, 0x22, 0x33, 0x44,
      0x11, 0x22, 0x33, 0x44, 0x11, 0x22, 0x33, 0x44,
  };
  std::vector<uint8_t> any_key_id(kAnyKeyId, kAnyKeyId + arraysize(kAnyKeyId));
  widevine_pssh_data.add_key_id()->assign(kAnyKeyId,
                                          kAnyKeyId + arraysize(kAnyKeyId));
  std::string widevine_pssh_data_str = widevine_pssh_data.SerializeAsString();

  EXPECT_TRUE(!widevine_pssh_data_str.empty());
  std::vector<uint8_t> pssh_data(widevine_pssh_data_str.begin(),
                                 widevine_pssh_data_str.end());

  media::ProtectionSystemSpecificInfo pssh_info;
  pssh_info.set_pssh_data(pssh_data);
  pssh_info.set_system_id(widevine_system_id_.data(),
                          widevine_system_id_.size());
  pssh_info.add_key_id(any_key_id);

  const char kExpectedJson[] =
      "{"
      "\"content_id\":\"Y29udGVudGlk\","
      "\"key_ids\":[\"11223344112233441122334411223344\"],"
      "\"provider\":\"someprovider\"}";
  std::string expected_json_base64;
  base::Base64Encode(kExpectedJson, &expected_json_base64);

  std::string expected_pssh_base64;
  const std::vector<uint8_t> pssh_box = pssh_info.CreateBox();
  base::Base64Encode(std::string(pssh_box.begin(), pssh_box.end()),
                     &expected_pssh_base64);

  EXPECT_CALL(*mock_media_playlist,
              AddEncryptionInfo(
                  _, StrEq("data:text/plain;base64," + expected_json_base64),
                  StrEq(""), StrEq("0x45454545454545454545454545454545"),
                  StrEq("com.widevine"), _));
  EXPECT_CALL(*mock_media_playlist,
              AddEncryptionInfo(
                  _, StrEq("data:text/plain;base64," + expected_pssh_base64),
                  StrEq("0x11223344112233441122334411223344"),
                  StrEq("0x45454545454545454545454545454545"),
                  StrEq("urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed"), _));
  EXPECT_TRUE(notifier.NotifyEncryptionUpdate(
      stream_id, any_key_id, widevine_system_id_, iv, pssh_box));
}

// Verify that key_ids in pssh is optional.
TEST_F(SimpleHlsNotifierTest, NotifyEncryptionUpdateWidevineNoKeyidsInPssh) {
  // Pointer released by SimpleHlsNotifier.
  MockMediaPlaylist* mock_media_playlist =
      new MockMediaPlaylist(kVodPlaylist, "playlist.m3u8", "", "");
  SimpleHlsNotifier notifier(kVodPlaylist, kTestTimeShiftBufferDepth,
                             kTestPrefix, kAnyOutputDir, kMasterPlaylistName);
  const uint32_t stream_id =
      SetupStream(kSampleAesProtectionScheme, mock_media_playlist, &notifier);

  const std::vector<uint8_t> iv(16, 0x45);

  media::WidevinePsshData widevine_pssh_data;
  widevine_pssh_data.set_provider("someprovider");
  widevine_pssh_data.set_content_id("contentid");
  std::string widevine_pssh_data_str = widevine_pssh_data.SerializeAsString();
  EXPECT_TRUE(!widevine_pssh_data_str.empty());
  std::vector<uint8_t> pssh_data(widevine_pssh_data_str.begin(),
                                 widevine_pssh_data_str.end());

  const char kExpectedJson[] =
      "{"
      "\"content_id\":\"Y29udGVudGlk\","
      "\"key_ids\":[\"11223344112233441122334411223344\"],"
      "\"provider\":\"someprovider\"}";
  std::string expected_json_base64;
  base::Base64Encode(kExpectedJson, &expected_json_base64);

  media::ProtectionSystemSpecificInfo pssh_info;
  pssh_info.set_pssh_data(pssh_data);
  pssh_info.set_system_id(widevine_system_id_.data(),
                          widevine_system_id_.size());

  std::string expected_pssh_base64;
  const std::vector<uint8_t> pssh_box = pssh_info.CreateBox();
  base::Base64Encode(std::string(pssh_box.begin(), pssh_box.end()),
                     &expected_pssh_base64);

  EXPECT_CALL(*mock_media_playlist,
              AddEncryptionInfo(
                  _, StrEq("data:text/plain;base64," + expected_json_base64),
                  StrEq(""), StrEq("0x45454545454545454545454545454545"),
                  StrEq("com.widevine"), _));
  EXPECT_CALL(*mock_media_playlist,
              AddEncryptionInfo(
                  _, StrEq("data:text/plain;base64," + expected_pssh_base64),
                  StrEq("0x11223344112233441122334411223344"),
                  StrEq("0x45454545454545454545454545454545"),
                  StrEq("urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed"), _));
  const uint8_t kAnyKeyId[] = {
      0x11, 0x22, 0x33, 0x44, 0x11, 0x22, 0x33, 0x44,
      0x11, 0x22, 0x33, 0x44, 0x11, 0x22, 0x33, 0x44,
  };
  EXPECT_TRUE(notifier.NotifyEncryptionUpdate(
      stream_id,
      std::vector<uint8_t>(kAnyKeyId, kAnyKeyId + arraysize(kAnyKeyId)),
      widevine_system_id_, iv, pssh_box));
}

TEST_F(SimpleHlsNotifierTest, NotifyEncryptionUpdateFixedKey) {
  // Pointer released by SimpleHlsNotifier.
  MockMediaPlaylist* mock_media_playlist =
      new MockMediaPlaylist(kVodPlaylist, "playlist.m3u8", "", "");
  SimpleHlsNotifier notifier(kVodPlaylist, kTestTimeShiftBufferDepth,
                             kTestPrefix, kAnyOutputDir, kMasterPlaylistName);
  const uint32_t stream_id =
      SetupStream(kSampleAesProtectionScheme, mock_media_playlist, &notifier);

  const std::vector<uint8_t> key_id(16, 0x23);
  const std::vector<uint8_t> iv(16, 0x45);
  const std::vector<uint8_t> dummy_pssh_data(10, 'p');

  std::string expected_key_uri_base64;
  base::Base64Encode(std::string(key_id.begin(), key_id.end()),
                     &expected_key_uri_base64);

  EXPECT_CALL(*mock_media_playlist,
              AddEncryptionInfo(
                  _, StrEq("data:text/plain;base64," + expected_key_uri_base64),
                  StrEq(""), StrEq("0x45454545454545454545454545454545"),
                  StrEq("identity"), _));
  EXPECT_TRUE(notifier.NotifyEncryptionUpdate(
      stream_id, key_id, common_system_id_, iv, dummy_pssh_data));
}

// Verify that when there are multiple key IDs in PSSH, the key ID that is
// passed to NotifyEncryptionUpdate() is the first key ID in the json format.
// Also verify that content_id is optional.
TEST_F(SimpleHlsNotifierTest, WidevineMultipleKeyIdsNoContentIdInPssh) {
  // Pointer released by SimpleHlsNotifier.
  MockMediaPlaylist* mock_media_playlist =
      new MockMediaPlaylist(kVodPlaylist, "playlist.m3u8", "", "");
  SimpleHlsNotifier notifier(kVodPlaylist, kTestTimeShiftBufferDepth,
                             kTestPrefix, kAnyOutputDir, kMasterPlaylistName);
  uint32_t stream_id =
      SetupStream(kSampleAesProtectionScheme, mock_media_playlist, &notifier);

  std::vector<uint8_t> iv(16, 0x45);

  media::WidevinePsshData widevine_pssh_data;
  widevine_pssh_data.set_provider("someprovider");
  const uint8_t kFirstKeyId[] = {
      0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11,
      0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11,
  };
  const uint8_t kSecondKeyId[] = {
      0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22,
      0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22,
  };
  std::vector<uint8_t> first_keyid(kFirstKeyId,
                                   kFirstKeyId + arraysize(kFirstKeyId));
  std::vector<uint8_t> second_keyid(kSecondKeyId,
                                    kSecondKeyId + arraysize(kSecondKeyId));

  widevine_pssh_data.add_key_id()->assign(kFirstKeyId,
                                          kFirstKeyId + arraysize(kFirstKeyId));
  widevine_pssh_data.add_key_id()->assign(
      kSecondKeyId, kSecondKeyId + arraysize(kSecondKeyId));
  std::string widevine_pssh_data_str = widevine_pssh_data.SerializeAsString();
  EXPECT_TRUE(!widevine_pssh_data_str.empty());
  std::vector<uint8_t> pssh_data(widevine_pssh_data_str.begin(),
                                 widevine_pssh_data_str.end());

  media::ProtectionSystemSpecificInfo pssh_info;
  pssh_info.set_pssh_data(pssh_data);
  pssh_info.set_system_id(widevine_system_id_.data(),
                          widevine_system_id_.size());
  pssh_info.add_key_id(first_keyid);
  pssh_info.add_key_id(second_keyid);

  const char kExpectedJson[] =
      "{"
      "\"key_ids\":[\"22222222222222222222222222222222\","
      "\"11111111111111111111111111111111\"],"
      "\"provider\":\"someprovider\"}";
  std::string expected_json_base64;
  base::Base64Encode(kExpectedJson, &expected_json_base64);

  std::string expected_pssh_base64;
  const std::vector<uint8_t> pssh_box = pssh_info.CreateBox();
  base::Base64Encode(std::string(pssh_box.begin(), pssh_box.end()),
                     &expected_pssh_base64);

  EXPECT_CALL(*mock_media_playlist,
              AddEncryptionInfo(
                  _, StrEq("data:text/plain;base64," + expected_json_base64),
                  StrEq(""), StrEq("0x45454545454545454545454545454545"),
                  StrEq("com.widevine"), _));

  EXPECT_CALL(*mock_media_playlist,
              AddEncryptionInfo(
                  _, StrEq("data:text/plain;base64," + expected_pssh_base64),
                  StrEq("0x22222222222222222222222222222222"),
                  StrEq("0x45454545454545454545454545454545"),
                  StrEq("urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed"), _));

  EXPECT_TRUE(notifier.NotifyEncryptionUpdate(
      stream_id,
      // Use the second key id here so that it will be thre first one in the
      // key_ids array in the JSON.
      second_keyid, widevine_system_id_, iv, pssh_box));
}

// Verify that the encryption scheme set in MediaInfo is passed to
// MediaPlaylist::AddEncryptionInfo().
TEST_F(SimpleHlsNotifierTest, EncryptionScheme) {
  // Pointer released by SimpleHlsNotifier.
  MockMediaPlaylist* mock_media_playlist =
      new MockMediaPlaylist(kVodPlaylist, "playlist.m3u8", "", "");
  SimpleHlsNotifier notifier(kVodPlaylist, kTestTimeShiftBufferDepth,
                             kTestPrefix, kAnyOutputDir, kMasterPlaylistName);
  const uint32_t stream_id =
      SetupStream(kCencProtectionScheme, mock_media_playlist, &notifier);

  const std::vector<uint8_t> key_id(16, 0x23);
  const std::vector<uint8_t> iv(16, 0x45);
  const std::vector<uint8_t> dummy_pssh_data(10, 'p');

  std::string expected_key_uri_base64;
  base::Base64Encode(std::string(key_id.begin(), key_id.end()),
                     &expected_key_uri_base64);

  EXPECT_CALL(
      *mock_media_playlist,
      AddEncryptionInfo(
          MediaPlaylist::EncryptionMethod::kSampleAesCenc,
          StrEq("data:text/plain;base64," + expected_key_uri_base64), StrEq(""),
          StrEq("0x45454545454545454545454545454545"), StrEq("identity"), _));
  EXPECT_TRUE(notifier.NotifyEncryptionUpdate(
      stream_id, key_id, common_system_id_, iv, dummy_pssh_data));
}

// If using 'cenc' with Widevine, don't output the json form.
TEST_F(SimpleHlsNotifierTest, WidevineCencEncryptionScheme) {
  // Pointer released by SimpleHlsNotifier.
  MockMediaPlaylist* mock_media_playlist =
      new MockMediaPlaylist(kVodPlaylist, "playlist.m3u8", "", "");
  SimpleHlsNotifier notifier(kVodPlaylist, kTestTimeShiftBufferDepth,
                             kTestPrefix, kAnyOutputDir, kMasterPlaylistName);
  const uint32_t stream_id =
      SetupStream(kCencProtectionScheme, mock_media_playlist, &notifier);

  const std::vector<uint8_t> iv(16, 0x45);

  media::WidevinePsshData widevine_pssh_data;
  widevine_pssh_data.set_provider("someprovider");
  widevine_pssh_data.set_content_id("contentid");
  const uint8_t kAnyKeyId[] = {
      0x11, 0x22, 0x33, 0x44, 0x11, 0x22, 0x33, 0x44,
      0x11, 0x22, 0x33, 0x44, 0x11, 0x22, 0x33, 0x44,
  };
  std::vector<uint8_t> any_key_id(kAnyKeyId, kAnyKeyId + arraysize(kAnyKeyId));
  widevine_pssh_data.add_key_id()->assign(kAnyKeyId,
                                          kAnyKeyId + arraysize(kAnyKeyId));
  std::string widevine_pssh_data_str = widevine_pssh_data.SerializeAsString();

  EXPECT_TRUE(!widevine_pssh_data_str.empty());
  std::vector<uint8_t> pssh_data(widevine_pssh_data_str.begin(),
                                 widevine_pssh_data_str.end());

  media::ProtectionSystemSpecificInfo pssh_info;
  pssh_info.set_pssh_data(pssh_data);
  pssh_info.set_system_id(widevine_system_id_.data(),
                          widevine_system_id_.size());
  pssh_info.add_key_id(any_key_id);

  std::string expected_pssh_base64;
  const std::vector<uint8_t> pssh_box = pssh_info.CreateBox();
  base::Base64Encode(std::string(pssh_box.begin(), pssh_box.end()),
                     &expected_pssh_base64);

  EXPECT_CALL(*mock_media_playlist,
              AddEncryptionInfo(
                  _, StrEq("data:text/plain;base64," + expected_pssh_base64),
                  StrEq("0x11223344112233441122334411223344"),
                  StrEq("0x45454545454545454545454545454545"),
                  StrEq("urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed"), _));
  EXPECT_TRUE(notifier.NotifyEncryptionUpdate(
      stream_id, any_key_id, widevine_system_id_, iv, pssh_box));
}

TEST_F(SimpleHlsNotifierTest, WidevineNotifyEncryptionUpdateEmptyIv) {
  // Pointer released by SimpleHlsNotifier.
  MockMediaPlaylist* mock_media_playlist =
      new MockMediaPlaylist(kVodPlaylist, "playlist.m3u8", "", "");
  SimpleHlsNotifier notifier(kVodPlaylist, kTestTimeShiftBufferDepth,
                             kTestPrefix, kAnyOutputDir, kMasterPlaylistName);
  const uint32_t stream_id =
      SetupStream(kSampleAesProtectionScheme, mock_media_playlist, &notifier);

  media::WidevinePsshData widevine_pssh_data;
  widevine_pssh_data.set_provider("someprovider");
  widevine_pssh_data.set_content_id("contentid");
  const uint8_t kAnyKeyId[] = {
      0x11, 0x22, 0x33, 0x44, 0x11, 0x22, 0x33, 0x44,
      0x11, 0x22, 0x33, 0x44, 0x11, 0x22, 0x33, 0x44,
  };
  std::vector<uint8_t> any_key_id(kAnyKeyId, kAnyKeyId + arraysize(kAnyKeyId));
  widevine_pssh_data.add_key_id()->assign(kAnyKeyId,
                                          kAnyKeyId + arraysize(kAnyKeyId));
  std::string widevine_pssh_data_str = widevine_pssh_data.SerializeAsString();
  EXPECT_TRUE(!widevine_pssh_data_str.empty());
  std::vector<uint8_t> pssh_data(widevine_pssh_data_str.begin(),
                                 widevine_pssh_data_str.end());

  const char kExpectedJson[] =
      "{"
      "\"content_id\":\"Y29udGVudGlk\","
      "\"key_ids\":[\"11223344112233441122334411223344\"],"
      "\"provider\":\"someprovider\"}";
  std::string expected_json_base64;
  base::Base64Encode(kExpectedJson, &expected_json_base64);

  media::ProtectionSystemSpecificInfo pssh_info;
  pssh_info.set_pssh_data(pssh_data);
  pssh_info.set_system_id(widevine_system_id_.data(),
                          widevine_system_id_.size());
  pssh_info.add_key_id(any_key_id);

  EXPECT_CALL(*mock_media_playlist,
              AddEncryptionInfo(
                  _, StrEq("data:text/plain;base64," + expected_json_base64),
                  StrEq(""), StrEq(""), StrEq("com.widevine"), StrEq("1")));

  EXPECT_CALL(
      *mock_media_playlist,
      AddEncryptionInfo(
          _,
          StrEq("data:text/plain;base64,"
                "AAAAS3Bzc2gAAAAA7e+"
                "LqXnWSs6jyCfc1R0h7QAAACsSEBEiM0QRIjNEESIzRBEiM0QaDHNvb"
                "WVwcm92aWRlciIJY29udGVudGlk"),
          StrEq("0x11223344112233441122334411223344"), StrEq(""),
          StrEq("urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed"), StrEq("1")));
  std::vector<uint8_t> pssh_as_vec = pssh_info.CreateBox();
  std::string pssh_in_string(pssh_as_vec.begin(), pssh_as_vec.end());
  std::string base_64_encoded_pssh;
  base::Base64Encode(pssh_in_string, &base_64_encoded_pssh);
  LOG(INFO) << base_64_encoded_pssh;

  std::vector<uint8_t> empty_iv;
  EXPECT_TRUE(notifier.NotifyEncryptionUpdate(
      stream_id,
      std::vector<uint8_t>(kAnyKeyId, kAnyKeyId + arraysize(kAnyKeyId)),
      widevine_system_id_, empty_iv, pssh_info.CreateBox()));
}

TEST_F(SimpleHlsNotifierTest, NotifyEncryptionUpdateWithoutStreamsRegistered) {
  std::vector<uint8_t> system_id;
  std::vector<uint8_t> iv;
  std::vector<uint8_t> pssh_data;
  std::vector<uint8_t> key_id;
  SimpleHlsNotifier notifier(kVodPlaylist, kTestTimeShiftBufferDepth,
                             kTestPrefix, kAnyOutputDir, kMasterPlaylistName);
  EXPECT_TRUE(notifier.Init());
  EXPECT_FALSE(
      notifier.NotifyEncryptionUpdate(1238u, key_id, system_id, iv, pssh_data));
}

class LiveOrEventSimpleHlsNotifierTest
    : public SimpleHlsNotifierTest,
      public ::testing::WithParamInterface<HlsPlaylistType> {
 protected:
  LiveOrEventSimpleHlsNotifierTest() : SimpleHlsNotifierTest(GetParam()) {
    expected_playlist_type_ = GetParam();
  }

  HlsPlaylistType expected_playlist_type_;
};

TEST_P(LiveOrEventSimpleHlsNotifierTest, NotifyNewSegment) {
  std::unique_ptr<MockMasterPlaylist> mock_master_playlist(
      new MockMasterPlaylist());
  std::unique_ptr<MockMediaPlaylistFactory> factory(
      new MockMediaPlaylistFactory());

  // Pointer released by SimpleHlsNotifier.
  MockMediaPlaylist* mock_media_playlist =
      new MockMediaPlaylist(expected_playlist_type_, "playlist.m3u8", "", "");

  EXPECT_CALL(
      *mock_master_playlist,
      AddMediaPlaylist(static_cast<MediaPlaylist*>(mock_media_playlist)));
  EXPECT_CALL(*mock_media_playlist, SetMediaInfo(_)).WillOnce(Return(true));
  EXPECT_CALL(*factory, CreateMock(expected_playlist_type_, _, _, _, _))
      .WillOnce(Return(mock_media_playlist));

  const uint64_t kStartTime = 1328;
  const uint64_t kDuration = 398407;
  const uint64_t kSize = 6595840;
  const std::string segment_name = "segmentname";
  EXPECT_CALL(*mock_media_playlist,
              AddSegment(StrEq(kTestPrefix + segment_name), kStartTime,
                         kDuration, _, kSize));

  const double kLongestSegmentDuration = 11.3;
  const uint32_t kTargetDuration = 12;  // ceil(kLongestSegmentDuration).
  EXPECT_CALL(*mock_media_playlist, GetLongestSegmentDuration())
      .WillOnce(Return(kLongestSegmentDuration));

  EXPECT_CALL(*mock_master_playlist,
              WriteMasterPlaylist(StrEq(kTestPrefix), StrEq(kAnyOutputDir)))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_media_playlist, SetTargetDuration(kTargetDuration))
      .Times(1);
  EXPECT_CALL(*mock_media_playlist,
              WriteToFile(StrEq(
                  base::FilePath::FromUTF8Unsafe(kAnyOutputDir)
                      .Append(base::FilePath::FromUTF8Unsafe("playlist.m3u8"))
                      .AsUTF8Unsafe())))
      .WillOnce(Return(true));

  SimpleHlsNotifier notifier(GetParam(),
                             kTestTimeShiftBufferDepth, kTestPrefix,
                             kAnyOutputDir, kMasterPlaylistName);
  InjectMasterPlaylist(std::move(mock_master_playlist), &notifier);
  InjectMediaPlaylistFactory(std::move(factory), &notifier);
  EXPECT_TRUE(notifier.Init());
  MediaInfo media_info;
  uint32_t stream_id;
  EXPECT_TRUE(notifier.NotifyNewStream(media_info, "playlist.m3u8", "name",
                                       "groupid", &stream_id));

  EXPECT_TRUE(notifier.NotifyNewSegment(stream_id, segment_name, kStartTime,
                                        kDuration, 0, kSize));
}

TEST_P(LiveOrEventSimpleHlsNotifierTest, NotifyNewSegmentsWithMultipleStreams) {
  const uint64_t kStartTime = 1328;
  const uint64_t kDuration = 398407;
  const uint64_t kSize = 6595840;

  InSequence in_sequence;

  std::unique_ptr<MockMasterPlaylist> mock_master_playlist(
      new MockMasterPlaylist());
  std::unique_ptr<MockMediaPlaylistFactory> factory(
      new MockMediaPlaylistFactory());

  // Pointer released by SimpleHlsNotifier.
  MockMediaPlaylist* mock_media_playlist1 =
      new MockMediaPlaylist(expected_playlist_type_, "playlist1.m3u8", "", "");
  MockMediaPlaylist* mock_media_playlist2 =
      new MockMediaPlaylist(expected_playlist_type_, "playlist2.m3u8", "", "");

  EXPECT_CALL(*factory, CreateMock(_, _, StrEq("playlist1.m3u8"), _, _))
      .WillOnce(Return(mock_media_playlist1));
  EXPECT_CALL(*mock_media_playlist1, SetMediaInfo(_)).WillOnce(Return(true));
  EXPECT_CALL(
      *mock_master_playlist,
      AddMediaPlaylist(static_cast<MediaPlaylist*>(mock_media_playlist1)));
  EXPECT_CALL(*factory, CreateMock(_, _, StrEq("playlist2.m3u8"), _, _))
      .WillOnce(Return(mock_media_playlist2));
  EXPECT_CALL(*mock_media_playlist2, SetMediaInfo(_)).WillOnce(Return(true));
  EXPECT_CALL(
      *mock_master_playlist,
      AddMediaPlaylist(static_cast<MediaPlaylist*>(mock_media_playlist2)));

  SimpleHlsNotifier notifier(GetParam(),
                             kTestTimeShiftBufferDepth, kTestPrefix,
                             kAnyOutputDir, kMasterPlaylistName);
  MockMasterPlaylist* mock_master_playlist_ptr = mock_master_playlist.get();
  InjectMasterPlaylist(std::move(mock_master_playlist), &notifier);
  InjectMediaPlaylistFactory(std::move(factory), &notifier);
  EXPECT_TRUE(notifier.Init());

  MediaInfo media_info;
  uint32_t stream_id1;
  EXPECT_TRUE(notifier.NotifyNewStream(media_info, "playlist1.m3u8", "name",
                                        "groupid", &stream_id1));
  uint32_t stream_id2;
  EXPECT_TRUE(notifier.NotifyNewStream(media_info, "playlist2.m3u8", "name",
                                        "groupid", &stream_id2));

  EXPECT_CALL(*mock_media_playlist1, AddSegment(_, _, _, _, _)).Times(1);
  const double kLongestSegmentDuration = 11.3;
  const uint32_t kTargetDuration = 12;  // ceil(kLongestSegmentDuration).
  EXPECT_CALL(*mock_media_playlist1, GetLongestSegmentDuration())
      .WillOnce(Return(kLongestSegmentDuration));

  EXPECT_CALL(*mock_master_playlist_ptr, WriteMasterPlaylist(_, _))
      .WillOnce(Return(true));
  // SetTargetDuration and update all playlists as target duration is updated.
  EXPECT_CALL(*mock_media_playlist1, SetTargetDuration(kTargetDuration))
      .Times(1);
  EXPECT_CALL(*mock_media_playlist1,
              WriteToFile(StrEq(
                  base::FilePath::FromUTF8Unsafe(kAnyOutputDir)
                      .Append(base::FilePath::FromUTF8Unsafe("playlist1.m3u8"))
                      .AsUTF8Unsafe())))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_media_playlist2, SetTargetDuration(kTargetDuration))
      .Times(1);
  EXPECT_CALL(*mock_media_playlist2,
              WriteToFile(StrEq(
                  base::FilePath::FromUTF8Unsafe(kAnyOutputDir)
                      .Append(base::FilePath::FromUTF8Unsafe("playlist2.m3u8"))
                      .AsUTF8Unsafe())))
      .WillOnce(Return(true));
  EXPECT_TRUE(notifier.NotifyNewSegment(stream_id1, "segment_name", kStartTime,
                                        kDuration, 0, kSize));

  EXPECT_CALL(*mock_media_playlist2, AddSegment(_, _, _, _, _)).Times(1);
  EXPECT_CALL(*mock_media_playlist2, GetLongestSegmentDuration())
      .WillOnce(Return(kLongestSegmentDuration));
  EXPECT_CALL(*mock_master_playlist_ptr, WriteMasterPlaylist(_, _))
      .WillOnce(Return(true));
  // Not updating other playlists as target duration does not change.
  EXPECT_CALL(*mock_media_playlist2,
              WriteToFile(StrEq(
                  base::FilePath::FromUTF8Unsafe(kAnyOutputDir)
                      .Append(base::FilePath::FromUTF8Unsafe("playlist2.m3u8"))
                      .AsUTF8Unsafe())))
      .WillOnce(Return(true));
  EXPECT_TRUE(notifier.NotifyNewSegment(stream_id2, "segment_name", kStartTime,
                                        kDuration, 0, kSize));
}

INSTANTIATE_TEST_CASE_P(PlaylistTypes,
                        LiveOrEventSimpleHlsNotifierTest,
                        ::testing::Values(HlsPlaylistType::kLive,
                                          HlsPlaylistType::kEvent));
}  // namespace hls
}  // namespace shaka
