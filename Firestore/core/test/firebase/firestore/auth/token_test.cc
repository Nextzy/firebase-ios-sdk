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

#include "Firestore/core/src/firebase/firestore/auth/token.h"

#include "gtest/gtest.h"

namespace firebase {
namespace firestore {
namespace auth {

TEST(Token, Getter) {
  Token token("token", User("abc"));
  EXPECT_EQ("token", token.token());
  EXPECT_EQ(User("abc"), token.user());
  EXPECT_TRUE(token.is_valid());
}

TEST(Token, InvalidToken) {
  const Token& token = Token::Invalid();
  EXPECT_ANY_THROW(token.token());
  EXPECT_EQ(User::Unauthenticated(), token.user());
  EXPECT_FALSE(token.is_valid());
}

}  // namespace auth
}  // namespace firestore
}  // namespace firebase
