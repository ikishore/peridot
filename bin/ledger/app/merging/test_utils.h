// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PERIDOT_BIN_LEDGER_APP_MERGING_TEST_UTILS_H_
#define PERIDOT_BIN_LEDGER_APP_MERGING_TEST_UTILS_H_

#include <functional>
#include <memory>

#include <lib/backoff/backoff.h>
#include <lib/fit/function.h>
#include <lib/gtest/test_loop_fixture.h>

#include "gtest/gtest.h"
#include "peridot/bin/ledger/coroutine/coroutine_impl.h"
#include "peridot/bin/ledger/encryption/fake/fake_encryption_service.h"
#include "peridot/bin/ledger/storage/public/journal.h"
#include "peridot/bin/ledger/storage/public/page_storage.h"
#include "peridot/bin/ledger/storage/public/types.h"
#include "peridot/bin/ledger/testing/test_with_environment.h"
#include "peridot/lib/scoped_tmpfs/scoped_tmpfs.h"

namespace ledger {

class TestWithPageStorage : public TestWithEnvironment {
 public:
  TestWithPageStorage();
  ~TestWithPageStorage() override;

 protected:
  virtual storage::PageStorage* page_storage() = 0;

  // Returns a function that, when executed, adds the provided key and object to
  // a journal.
  fit::function<void(storage::Journal*)> AddKeyValueToJournal(
      const std::string& key, std::string value);

  // Returns a function that, when executed, deleted the provided key from a
  // journal.
  fit::function<void(storage::Journal*)> DeleteKeyFromJournal(
      const std::string& key);

  ::testing::AssertionResult GetValue(
      storage::ObjectIdentifier object_identifier, std::string* value);

  ::testing::AssertionResult CreatePageStorage(
      std::unique_ptr<storage::PageStorage>* page_storage);

  fit::closure MakeQuitTaskOnce();

 private:
  scoped_tmpfs::ScopedTmpFS tmpfs_;
  encryption::FakeEncryptionService encryption_service_;
};

}  // namespace ledger

#endif  // PERIDOT_BIN_LEDGER_APP_MERGING_TEST_UTILS_H_
