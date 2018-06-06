// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "peridot/lib/firebase_auth/firebase_auth_impl.h"

#include <utility>

#include <fuchsia/modular/auth/cpp/fidl.h>
#include "lib/backoff/testing/test_backoff.h"
#include "lib/callback/capture.h"
#include "lib/fidl/cpp/binding.h"
#include "lib/fxl/functional/make_copyable.h"
#include "lib/gtest/test_with_message_loop.h"
#include "peridot/lib/firebase_auth/testing/test_token_provider.h"

namespace firebase_auth {

namespace {

class MockCobaltContext : public cobalt::CobaltContext {
 public:
  ~MockCobaltContext() override = default;

  MockCobaltContext(int* called) : called_(called) {}

  void ReportObservation(cobalt::CobaltObservation observation) override {
    EXPECT_EQ(3u, observation.metric_id());
    // The value should contain the client name.
    EXPECT_TRUE(observation.ValueRepr().find("firebase-test") !=
                std::string::npos);
    *called_ += 1;
  }

 private:
  int* called_;
};

class FirebaseAuthImplTest : public gtest::TestWithMessageLoop {
 public:
  FirebaseAuthImplTest()
      : token_provider_(message_loop_.async()),
        token_provider_binding_(&token_provider_),
        firebase_auth_(
            {"api_key", "firebase-test", 1}, message_loop_.async(),
            token_provider_binding_.NewBinding().Bind(), InitBackoff(),
            std::make_unique<MockCobaltContext>(&report_observation_count_)) {}

  ~FirebaseAuthImplTest() override {}

 protected:
  std::unique_ptr<backoff::Backoff> InitBackoff() {
    auto backoff = std::make_unique<backoff::TestBackoff>();
    backoff_ = backoff.get();
    return backoff;
  }

  TestTokenProvider token_provider_;
  fidl::Binding<fuchsia::modular::auth::TokenProvider> token_provider_binding_;
  FirebaseAuthImpl firebase_auth_;
  int report_observation_count_ = 0;
  MockCobaltContext* mock_cobalt_context_;
  backoff::TestBackoff* backoff_;

 private:
  FXL_DISALLOW_COPY_AND_ASSIGN(FirebaseAuthImplTest);
};

TEST_F(FirebaseAuthImplTest, GetFirebaseToken) {
  token_provider_.Set("this is a token", "some id", "me@example.com");

  AuthStatus auth_status;
  std::string firebase_token;
  firebase_auth_.GetFirebaseToken(
      callback::Capture(MakeQuitTask(), &auth_status, &firebase_token));
  EXPECT_FALSE(RunLoopWithTimeout());
  EXPECT_EQ(AuthStatus::OK, auth_status);
  EXPECT_EQ("this is a token", firebase_token);
  EXPECT_EQ(0, report_observation_count_);
}

TEST_F(FirebaseAuthImplTest, GetFirebaseTokenRetryOnError) {
  token_provider_.Set("this is a token", "some id", "me@example.com");

  AuthStatus auth_status;
  std::string firebase_token;
  token_provider_.error_to_return.status =
      fuchsia::modular::auth::Status::NETWORK_ERROR;
  backoff_->SetOnGetNext(MakeQuitTask());
  bool called = false;
  firebase_auth_.GetFirebaseToken(
      [this, &called, &auth_status, &firebase_token](auto status, auto token) {
        called = true;
        auth_status = status;
        firebase_token = std::move(token);
        message_loop_.PostQuitTask();
      });
  EXPECT_FALSE(RunLoopWithTimeout());
  EXPECT_FALSE(called);
  EXPECT_EQ(1, backoff_->get_next_count);
  EXPECT_EQ(0, backoff_->reset_count);
  EXPECT_EQ(0, report_observation_count_);

  token_provider_.error_to_return.status = fuchsia::modular::auth::Status::OK;
  EXPECT_FALSE(RunLoopWithTimeout());
  EXPECT_TRUE(called);
  EXPECT_EQ(AuthStatus::OK, auth_status);
  EXPECT_EQ("this is a token", firebase_token);
  EXPECT_EQ(1, backoff_->get_next_count);
  EXPECT_EQ(1, backoff_->reset_count);
  EXPECT_EQ(0, report_observation_count_);
}

TEST_F(FirebaseAuthImplTest, GetFirebaseUserId) {
  token_provider_.Set("this is a token", "some id", "me@example.com");

  AuthStatus auth_status;
  std::string firebase_user_id;
  firebase_auth_.GetFirebaseUserId(
      callback::Capture(MakeQuitTask(), &auth_status, &firebase_user_id));
  EXPECT_FALSE(RunLoopWithTimeout());
  EXPECT_EQ(AuthStatus::OK, auth_status);
  EXPECT_EQ("some id", firebase_user_id);
  EXPECT_EQ(0, report_observation_count_);
}

TEST_F(FirebaseAuthImplTest, GetFirebaseUserIdRetryOnError) {
  token_provider_.Set("this is a token", "some id", "me@example.com");

  AuthStatus auth_status;
  std::string firebase_id;
  token_provider_.error_to_return.status =
      fuchsia::modular::auth::Status::NETWORK_ERROR;
  backoff_->SetOnGetNext(MakeQuitTask());
  bool called = false;
  firebase_auth_.GetFirebaseUserId(
      [this, &called, &auth_status, &firebase_id](auto status, auto id) {
        called = true;
        auth_status = status;
        firebase_id = std::move(id);
        message_loop_.PostQuitTask();
      });
  EXPECT_FALSE(RunLoopWithTimeout());
  EXPECT_FALSE(called);
  EXPECT_EQ(1, backoff_->get_next_count);
  EXPECT_EQ(0, backoff_->reset_count);
  EXPECT_EQ(0, report_observation_count_);

  token_provider_.error_to_return.status = fuchsia::modular::auth::Status::OK;
  EXPECT_FALSE(RunLoopWithTimeout());
  EXPECT_TRUE(called);
  EXPECT_EQ(AuthStatus::OK, auth_status);
  EXPECT_EQ("some id", firebase_id);
  EXPECT_EQ(1, backoff_->get_next_count);
  EXPECT_EQ(1, backoff_->reset_count);
  EXPECT_EQ(0, report_observation_count_);
}

TEST_F(FirebaseAuthImplTest, GetFirebaseUserIdMaxRetry) {
  token_provider_.Set("this is a token", "some id", "me@example.com");

  AuthStatus auth_status;
  token_provider_.error_to_return.status =
      fuchsia::modular::auth::Status::NETWORK_ERROR;
  backoff_->SetOnGetNext(MakeQuitTask());
  bool called = false;
  firebase_auth_.GetFirebaseUserId(
      [this, &called, &auth_status](auto status, auto id) {
        called = true;
        auth_status = status;
        message_loop_.PostQuitTask();
      });
  EXPECT_FALSE(RunLoopWithTimeout());
  EXPECT_FALSE(called);
  EXPECT_EQ(1, backoff_->get_next_count);
  EXPECT_EQ(0, backoff_->reset_count);
  EXPECT_EQ(0, report_observation_count_);

  // Exceeding the maximum number of retriable errors returns an error.
  token_provider_.error_to_return.status =
      fuchsia::modular::auth::Status::INTERNAL_ERROR;
  EXPECT_FALSE(RunLoopWithTimeout());
  EXPECT_TRUE(called);
  EXPECT_EQ(AuthStatus::ERROR, auth_status);
  EXPECT_EQ(1, backoff_->get_next_count);
  EXPECT_EQ(1, backoff_->reset_count);
  EXPECT_EQ(1, report_observation_count_);
}

TEST_F(FirebaseAuthImplTest, GetFirebaseUserIdNonRetriableError) {
  token_provider_.Set("this is a token", "some id", "me@example.com");

  AuthStatus auth_status;
  token_provider_.error_to_return.status =
      fuchsia::modular::auth::Status::BAD_REQUEST;
  backoff_->SetOnGetNext(MakeQuitTask());
  bool called = false;
  firebase_auth_.GetFirebaseUserId(
      [this, &called, &auth_status](auto status, auto id) {
        called = true;
        auth_status = status;
        message_loop_.PostQuitTask();
      });
  EXPECT_FALSE(RunLoopWithTimeout());
  EXPECT_TRUE(called);
  EXPECT_EQ(AuthStatus::ERROR, auth_status);
  EXPECT_EQ(0, backoff_->get_next_count);
  EXPECT_EQ(1, backoff_->reset_count);
  EXPECT_EQ(1, report_observation_count_);
}

}  // namespace

}  // namespace firebase_auth
