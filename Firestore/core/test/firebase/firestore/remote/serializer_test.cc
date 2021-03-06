/*
 * Copyright 2018 Google
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* NB: proto bytes were created via:
     echo 'TEXT_FORMAT_PROTO' \
       | ./build/external/protobuf/src/protobuf-build/src/protoc \
           -I./Firestore/Protos/protos \
           -I./build/external/protobuf/src/protobuf/src \
           --encode=google.firestore.v1beta1.Value \
           google/firestore/v1beta1/document.proto \
       | hexdump -C
 * where TEXT_FORMAT_PROTO is the text format of the protobuf. (go/textformat).
 *
 * Examples:
 * - For null, TEXT_FORMAT_PROTO would be 'null_value: NULL_VALUE' and would
 *   yield the bytes: { 0x58, 0x00 }.
 * - For true, TEXT_FORMAT_PROTO would be 'boolean_value: true' and would yield
 *   the bytes { 0x08, 0x01 }.
 *
 * All uses are documented below, so search for TEXT_FORMAT_PROTO to find more
 * examples.
 */

#include "Firestore/core/src/firebase/firestore/remote/serializer.h"

#include <pb.h>
#include <pb_encode.h>
#include <limits>
#include <vector>

#include "Firestore/core/src/firebase/firestore/model/field_value.h"
#include "gtest/gtest.h"

using firebase::firestore::model::FieldValue;
using firebase::firestore::remote::Serializer;

TEST(Serializer, CanLinkToNanopb) {
  // This test doesn't actually do anything interesting as far as actually using
  // nanopb is concerned but that it can run at all is proof that all the
  // libraries required for nanopb to work are actually linked correctly into
  // the test.
  pb_ostream_from_buffer(NULL, 0);
}

// Fixture for running serializer tests.
class SerializerTest : public ::testing::Test {
 public:
  SerializerTest() : serializer(/*DatabaseId("p", "d")*/) {
  }
  Serializer serializer;

  void ExpectRoundTrip(const FieldValue& model,
                       const std::vector<uint8_t>& bytes,
                       FieldValue::Type type) {
    EXPECT_EQ(type, model.type());
    std::vector<uint8_t> actual_bytes;
    serializer.EncodeFieldValue(model, &actual_bytes);
    EXPECT_EQ(bytes, actual_bytes);
    FieldValue actual_model = serializer.DecodeFieldValue(bytes);
    EXPECT_EQ(type, actual_model.type());
    EXPECT_EQ(model, actual_model);
  }
};

TEST_F(SerializerTest, EncodesNullModelToBytes) {
  FieldValue model = FieldValue::NullValue();

  // TEXT_FORMAT_PROTO: 'null_value: NULL_VALUE'
  std::vector<uint8_t> bytes{0x58, 0x00};
  ExpectRoundTrip(model, bytes, FieldValue::Type::Null);
}

TEST_F(SerializerTest, EncodesBoolModelToBytes) {
  struct TestCase {
    bool value;
    std::vector<uint8_t> bytes;
  };

  std::vector<TestCase> cases{// TEXT_FORMAT_PROTO: 'boolean_value: true'
                              {true, {0x08, 0x01}},
                              // TEXT_FORMAT_PROTO: 'boolean_value: false'
                              {false, {0x08, 0x00}}};

  for (const TestCase& test : cases) {
    FieldValue model = FieldValue::BooleanValue(test.value);
    ExpectRoundTrip(model, test.bytes, FieldValue::Type::Boolean);
  }
}

TEST_F(SerializerTest, EncodesIntegersModelToBytes) {
  // NB: because we're calculating the bytes via protoc (instead of
  // libprotobuf) we have to look at values from numeric_limits<T> ahead of
  // time. So these test cases make the following assumptions about
  // numeric_limits: (linking libprotobuf is starting to sound like a better
  // idea. :)
  //   -9223372036854775808
  //     == -8000000000000000
  //     == numeric_limits<int64_t>::min()
  //   9223372036854775807
  //     == 7FFFFFFFFFFFFFFF
  //     == numeric_limits<int64_t>::max()
  //
  // For now, lets at least verify these values:
  EXPECT_EQ(-9223372036854775807 - 1, std::numeric_limits<int64_t>::min());
  EXPECT_EQ(9223372036854775807, std::numeric_limits<int64_t>::max());

  struct TestCase {
    int64_t value;
    std::vector<uint8_t> bytes;
  };

  std::vector<TestCase> cases{
      // TEXT_FORMAT_PROTO: 'integer_value: 0'
      TestCase{0, {0x10, 0x00}},
      // TEXT_FORMAT_PROTO: 'integer_value: 1'
      TestCase{1, {0x10, 0x01}},
      // TEXT_FORMAT_PROTO: 'integer_value: -1'
      TestCase{
          -1,
          {0x10, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x01}},
      // TEXT_FORMAT_PROTO: 'integer_value: 100'
      TestCase{100, {0x10, 0x64}},
      // TEXT_FORMAT_PROTO: 'integer_value: -100'
      TestCase{
          -100,
          {0x10, 0x9c, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x01}},
      // TEXT_FORMAT_PROTO: 'integer_value: -9223372036854775808'
      TestCase{
          -9223372036854775807 - 1,
          {0x10, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x01}},
      // TEXT_FORMAT_PROTO: 'integer_value: 9223372036854775807'
      TestCase{9223372036854775807,
               {0x10, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f}}};

  for (const TestCase& test : cases) {
    FieldValue model = FieldValue::IntegerValue(test.value);
    ExpectRoundTrip(model, test.bytes, FieldValue::Type::Integer);
  }
}

TEST_F(SerializerTest, EncodesStringModelToBytes) {
  struct TestCase {
    std::string value;
    std::vector<uint8_t> bytes;
  };

  std::vector<TestCase> cases{
      // TEXT_FORMAT_PROTO: 'string_value: ""'
      {"", {0x8a, 0x01, 0x00}},
      // TEXT_FORMAT_PROTO: 'string_value: "a"'
      {"a", {0x8a, 0x01, 0x01, 0x61}},
      // TEXT_FORMAT_PROTO: 'string_value: "abc def"'
      {"abc def", {0x8a, 0x01, 0x07, 0x61, 0x62, 0x63, 0x20, 0x64, 0x65, 0x66}},
      // TEXT_FORMAT_PROTO: 'string_value: "æ"'
      {"æ", {0x8a, 0x01, 0x02, 0xc3, 0xa6}},
      // TEXT_FORMAT_PROTO: 'string_value: "\0\ud7ff\ue000\uffff"'
      // Note: Each one of the three embedded universal character names
      // (\u-escaped) maps to three chars, so the total length of the string
      // literal is 10 (ignoring the terminating null), and the resulting string
      // literal is the same as '\0\xed\x9f\xbf\xee\x80\x80\xef\xbf\xbf'". The
      // size of 10 must be added, or else std::string will see the \0 at the
      // start and assume that's the end of the string.
      {{"\0\ud7ff\ue000\uffff", 10},
       {0x8a, 0x01, 0x0a, 0x00, 0xed, 0x9f, 0xbf, 0xee, 0x80, 0x80, 0xef, 0xbf,
        0xbf}},
      {{"\0\xed\x9f\xbf\xee\x80\x80\xef\xbf\xbf", 10},
       {0x8a, 0x01, 0x0a, 0x00, 0xed, 0x9f, 0xbf, 0xee, 0x80, 0x80, 0xef, 0xbf,
        0xbf}},
      // TEXT_FORMAT_PROTO: 'string_value: "(╯°□°）╯︵ ┻━┻"'
      {"(╯°□°）╯︵ ┻━┻",
       {0x8a, 0x01, 0x1e, 0x28, 0xe2, 0x95, 0xaf, 0xc2, 0xb0, 0xe2, 0x96,
        0xa1, 0xc2, 0xb0, 0xef, 0xbc, 0x89, 0xe2, 0x95, 0xaf, 0xef, 0xb8,
        0xb5, 0x20, 0xe2, 0x94, 0xbb, 0xe2, 0x94, 0x81, 0xe2, 0x94, 0xbb}}};

  for (const TestCase& test : cases) {
    FieldValue model = FieldValue::StringValue(test.value);
    ExpectRoundTrip(model, test.bytes, FieldValue::Type::String);
  }
}

// TODO(rsgowman): Test [en|de]coding multiple protos into the same output
// vector.

// TODO(rsgowman): Death test for decoding invalid bytes.
