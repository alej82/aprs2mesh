#include <unity.h>

#include <cstring>

#include "ax25.h"

using namespace aprs2mesh;

void setUp() {}
void tearDown() {}

static Ax25Address addr(const char* call, uint8_t ssid = 0) {
  Ax25Address a;
  std::strncpy(a.call, call, kMaxCallsign);
  a.call[kMaxCallsign] = '\0';
  a.ssid = ssid;
  return a;
}

static void test_address_format_and_parse() {
  char text[12] = {};
  TEST_ASSERT_EQUAL_UINT32(4, formatAddress(addr("APRS"), text, sizeof(text)));
  TEST_ASSERT_EQUAL_STRING("APRS", text);

  TEST_ASSERT_EQUAL_UINT32(4, formatAddress(addr("AB", 5), text, sizeof(text)));
  TEST_ASSERT_EQUAL_STRING("AB-5", text);

  TEST_ASSERT_EQUAL_UINT32(9, formatAddress(addr("EA1ABC", 15), text, sizeof(text)));
  TEST_ASSERT_EQUAL_STRING("EA1ABC-15", text);

  Ax25Address parsed;
  TEST_ASSERT_TRUE(parseAddress("ea1abc-15", 9, parsed));
  TEST_ASSERT_EQUAL_STRING("EA1ABC", parsed.call);
  TEST_ASSERT_EQUAL_UINT8(15, parsed.ssid);

  TEST_ASSERT_TRUE(parseAddress("APRS", 4, parsed));
  TEST_ASSERT_EQUAL_UINT8(0, parsed.ssid);

  TEST_ASSERT_FALSE(parseAddress("EA1ABC-16", 9, parsed));
  TEST_ASSERT_FALSE(parseAddress("-1", 2, parsed));
  TEST_ASSERT_FALSE(parseAddress("ABCDEFG", 7, parsed));
  TEST_ASSERT_FALSE(parseAddress("EA1-", 4, parsed));
  TEST_ASSERT_FALSE(parseAddress("EA1-1A", 6, parsed));
}

// Hand-built AX.25 UI frame:
//   dest APRS, src EA1ABC-9, digi WIDE1-1 (last), control 0x03, pid 0xF0,
//   info "HOLA".
static const uint8_t kReferenceFrame[] = {
    0x82, 0xA0, 0xA4, 0xA6, 0x40, 0x40, 0x60,  // "APRS"
    0x8A, 0x82, 0x62, 0x82, 0x84, 0x86, 0x72,  // "EA1ABC" SSID 9
    0xAE, 0x92, 0x88, 0x8A, 0x62, 0x40, 0x63,  // "WIDE1"  SSID 1, last
    0x03, 0xF0, 'H', 'O', 'L', 'A',
};

static void test_parse_known_frame() {
  Ax25Frame frame;
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Ax25Status::Ok),
                        static_cast<int>(parseAx25(kReferenceFrame, sizeof(kReferenceFrame), frame)));
  TEST_ASSERT_EQUAL_STRING("APRS", frame.destination.call);
  TEST_ASSERT_EQUAL_UINT8(0, frame.destination.ssid);
  TEST_ASSERT_EQUAL_STRING("EA1ABC", frame.source.call);
  TEST_ASSERT_EQUAL_UINT8(9, frame.source.ssid);
  TEST_ASSERT_EQUAL_UINT8(1, frame.digipeaterCount);
  TEST_ASSERT_EQUAL_STRING("WIDE1", frame.digipeaters[0].call);
  TEST_ASSERT_EQUAL_UINT8(1, frame.digipeaters[0].ssid);
  TEST_ASSERT_EQUAL_UINT32(4, frame.infoLength);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(reinterpret_cast<const uint8_t*>("HOLA"), frame.info, 4);

  char tnc2[64] = {};
  size_t written = 0;
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Ax25Status::Ok),
                        static_cast<int>(formatTnc2(frame, true, tnc2, sizeof(tnc2), written)));
  TEST_ASSERT_EQUAL_STRING("EA1ABC-9>APRS,WIDE1-1:HOLA", tnc2);
}

static void test_build_round_trip_with_path() {
  Ax25Frame frame;
  frame.source = addr("EA1ABC", 9);
  frame.destination = addr("APRS");
  frame.digipeaters[0] = addr("WIDE1", 1);
  frame.digipeaters[1] = addr("WIDE2", 1);
  frame.digipeaterCount = 2;
  const char* info = "!4332.10N/00752.30W-Test";
  frame.infoLength = static_cast<uint16_t>(std::strlen(info));
  std::memcpy(frame.info, info, frame.infoLength);

  uint8_t bytes[128] = {};
  size_t written = 0;
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Ax25Status::Ok),
                        static_cast<int>(buildAx25(frame, bytes, sizeof(bytes), written)));
  TEST_ASSERT_EQUAL_UINT32((2 + 2) * 7 + 2 + frame.infoLength, written);

  Ax25Frame parsed;
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Ax25Status::Ok),
                        static_cast<int>(parseAx25(bytes, written, parsed)));
  TEST_ASSERT_EQUAL_UINT8(2, parsed.digipeaterCount);
  TEST_ASSERT_EQUAL_STRING("WIDE2", parsed.digipeaters[1].call);

  char tnc2[128] = {};
  size_t tnc2Len = 0;
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Ax25Status::Ok),
                        static_cast<int>(formatTnc2(parsed, true, tnc2, sizeof(tnc2), tnc2Len)));
  TEST_ASSERT_EQUAL_STRING("EA1ABC-9>APRS,WIDE1-1,WIDE2-1:!4332.10N/00752.30W-Test", tnc2);
}

static void test_tnc2_parse_and_info_with_colon() {
  const char* text = "EA1ABC-9>APRS::EA1ABC-9 :hola{13";
  Ax25Frame frame;
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Ax25Status::Ok),
                        static_cast<int>(parseTnc2(text, std::strlen(text), frame)));
  TEST_ASSERT_EQUAL_STRING("EA1ABC", frame.source.call);
  TEST_ASSERT_EQUAL_UINT8(9, frame.source.ssid);
  TEST_ASSERT_EQUAL_STRING("APRS", frame.destination.call);
  TEST_ASSERT_EQUAL_UINT8(0, frame.digipeaterCount);
  TEST_ASSERT_EQUAL_UINT32(18, frame.infoLength);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(reinterpret_cast<const uint8_t*>(":EA1ABC-9 :hola{13"),
                                frame.info, frame.infoLength);
}

static void test_tnc2_star_markers_are_ignored() {
  const char* text = "EA1ABC-9>APRS,WIDE1-1*,WIDE2-1*:HOLA";
  Ax25Frame frame;
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Ax25Status::Ok),
                        static_cast<int>(parseTnc2(text, std::strlen(text), frame)));
  TEST_ASSERT_EQUAL_UINT8(2, frame.digipeaterCount);
  TEST_ASSERT_EQUAL_STRING("WIDE1", frame.digipeaters[0].call);

  char out[64] = {};
  size_t written = 0;
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Ax25Status::Ok),
                        static_cast<int>(formatTnc2(frame, true, out, sizeof(out), written)));
  TEST_ASSERT_EQUAL_STRING("EA1ABC-9>APRS,WIDE1-1,WIDE2-1:HOLA", out);
}

static void test_strip_path() {
  const char* text = "EA1ABC-9>APRS,WIDE1-1,WIDE2-1:HOLA";
  char out[64] = {};
  size_t written = 0;
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(Ax25Status::Ok),
      static_cast<int>(stripTnc2Path(text, std::strlen(text), out, sizeof(out), written)));
  TEST_ASSERT_EQUAL_STRING("EA1ABC-9>APRS:HOLA", out);
  TEST_ASSERT_EQUAL_UINT32(18, written);
}

static void test_tnc2_without_path_round_trip() {
  const char* text = "EA1ABC-9>APRS:HOLA";
  Ax25Frame frame;
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Ax25Status::Ok),
                        static_cast<int>(parseTnc2(text, std::strlen(text), frame)));
  TEST_ASSERT_EQUAL_UINT8(0, frame.digipeaterCount);

  char out[64] = {};
  size_t written = 0;
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Ax25Status::Ok),
                        static_cast<int>(formatTnc2(frame, false, out, sizeof(out), written)));
  TEST_ASSERT_EQUAL_STRING(text, out);
}

static void test_malformed_inputs() {
  Ax25Frame frame;
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Ax25Status::ShortFrame),
                        static_cast<int>(parseAx25(kReferenceFrame, 10, frame)));

  uint8_t badControl[sizeof(kReferenceFrame)] = {};
  std::memcpy(badControl, kReferenceFrame, sizeof(kReferenceFrame));
  badControl[21] = 0x13;
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Ax25Status::BadControl),
                        static_cast<int>(parseAx25(badControl, sizeof(badControl), frame)));

  uint8_t zeros[32] = {};
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Ax25Status::BadAddress),
                        static_cast<int>(parseAx25(zeros, sizeof(zeros), frame)));

  TEST_ASSERT_EQUAL_INT(static_cast<int>(Ax25Status::BadTnc2),
                        static_cast<int>(parseTnc2("EA1ABC-9>APRS", 13, frame)));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Ax25Status::BadTnc2),
                        static_cast<int>(parseTnc2("EA1ABC-9 APRS:HOLA", 19, frame)));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Ax25Status::BadCallsign),
                        static_cast<int>(parseTnc2("EA1ABC-99>APRS:HOLA", 18, frame)));
}

static void test_build_rejects_bad_input() {
  Ax25Frame frame;
  frame.source = addr("A B");
  frame.destination = addr("APRS");
  uint8_t bytes[64] = {};
  size_t written = 0;
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Ax25Status::BadCallsign),
                        static_cast<int>(buildAx25(frame, bytes, sizeof(bytes), written)));

  frame.source = addr("EA1ABC");
  frame.digipeaterCount = 9;
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(Ax25Status::TooManyDigipeaters),
      static_cast<int>(buildAx25(frame, bytes, sizeof(bytes), written)));

  frame.digipeaterCount = 0;
  frame.infoLength = 300;
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Ax25Status::InfoTooLong),
                        static_cast<int>(buildAx25(frame, bytes, sizeof(bytes), written)));

  frame.infoLength = 10;
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Ax25Status::BufferTooSmall),
                        static_cast<int>(buildAx25(frame, bytes, 10, written)));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_address_format_and_parse);
  RUN_TEST(test_parse_known_frame);
  RUN_TEST(test_build_round_trip_with_path);
  RUN_TEST(test_tnc2_parse_and_info_with_colon);
  RUN_TEST(test_tnc2_star_markers_are_ignored);
  RUN_TEST(test_strip_path);
  RUN_TEST(test_tnc2_without_path_round_trip);
  RUN_TEST(test_malformed_inputs);
  RUN_TEST(test_build_rejects_bad_input);
  return UNITY_END();
}
