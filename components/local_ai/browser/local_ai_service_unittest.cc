// Copyright (c) 2025 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

#include "brave/components/local_ai/browser/local_ai_service.h"

#include <string>
#include <vector>

#include "base/test/test_future.h"
#include "brave/components/local_ai/common/local_ai.mojom.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/test/test_renderer_host.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace local_ai {

namespace {

constexpr double kTestEmbeddingData[] = {1.0, 2.0, 3.0};

std::vector<double> TestEmbedding() {
  return {std::begin(kTestEmbeddingData), std::end(kTestEmbeddingData)};
}

// Fake model worker that returns a fixed embedding vector.
class FakeModelWorker : public mojom::OnDeviceModelWorker {
 public:
  void GenerateEmbeddings(const std::string& input,
                          GenerateEmbeddingsCallback callback) override {
    embed_count_++;
    std::move(callback).Run(TestEmbedding());
  }

  mojo::PendingRemote<mojom::OnDeviceModelWorker> BindNewPipeAndPassRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  void Reset() { receiver_.reset(); }

  int embed_count() const { return embed_count_; }

 private:
  int embed_count_ = 0;
  mojo::Receiver<mojom::OnDeviceModelWorker> receiver_{this};
};

}  // namespace

class LocalAIServiceTest : public content::RenderViewHostTestHarness {
 protected:
  LocalAIServiceTest()
      : content::RenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    service_ =
        std::make_unique<LocalAIService>(browser_context(), base::DoNothing());
  }

  void TearDown() override {
    static_cast<KeyedService*>(service_.get())->Shutdown();
    service_.reset();
    content::RenderViewHostTestHarness::TearDown();
  }

  // Simulate the model worker page registering its interface.
  void BindFakeModelWorker() {
    service_->RegisterOnDeviceModelWorker(
        fake_model_worker_.BindNewPipeAndPassRemote());
  }

  // Access BackgroundWebContents::Delegate methods through the
  // public base class interface.
  BackgroundWebContents::Delegate* delegate() {
    return static_cast<BackgroundWebContents::Delegate*>(service_.get());
  }

  std::unique_ptr<LocalAIService> service_;
  FakeModelWorker fake_model_worker_;
};

TEST_F(LocalAIServiceTest, GenerateEmbeddingsCreatesBackgroundContents) {
  // Calling GenerateEmbeddings() should lazily create the
  // BackgroundWebContents.
  base::test::TestFuture<const std::vector<double>&> future;
  service_->GenerateEmbeddings("test", future.GetCallback());

  EXPECT_FALSE(future.IsReady());
}

TEST_F(LocalAIServiceTest, GenerateEmbeddingsQueuesWhenNotReady) {
  base::test::TestFuture<const std::vector<double>&> future1;
  base::test::TestFuture<const std::vector<double>&> future2;

  service_->GenerateEmbeddings("hello", future1.GetCallback());
  service_->GenerateEmbeddings("world", future2.GetCallback());

  // Both should be queued, not resolved.
  EXPECT_FALSE(future1.IsReady());
  EXPECT_FALSE(future2.IsReady());
}

TEST_F(LocalAIServiceTest, RegisterModelWorkerProcessesPendingRequests) {
  base::test::TestFuture<const std::vector<double>&> future1;
  base::test::TestFuture<const std::vector<double>&> future2;

  service_->GenerateEmbeddings("hello", future1.GetCallback());
  service_->GenerateEmbeddings("world", future2.GetCallback());

  BindFakeModelWorker();

  EXPECT_EQ(TestEmbedding(), future1.Get());
  EXPECT_EQ(TestEmbedding(), future2.Get());
  EXPECT_EQ(2, fake_model_worker_.embed_count());
}

TEST_F(LocalAIServiceTest, GenerateEmbeddingsForwardsDirectlyWhenReady) {
  BindFakeModelWorker();

  // Now that we're ready, GenerateEmbeddings should go directly to
  // the remote.
  service_->GenerateEmbeddings("test", base::DoNothing());

  base::test::TestFuture<const std::vector<double>&> future;
  service_->GenerateEmbeddings("direct", future.GetCallback());

  EXPECT_EQ(TestEmbedding(), future.Get());
}

TEST_F(LocalAIServiceTest, ShutdownFailsPendingRequests) {
  base::test::TestFuture<const std::vector<double>&> future;
  service_->GenerateEmbeddings("pending", future.GetCallback());

  static_cast<KeyedService*>(service_.get())->Shutdown();

  // Pending request should be resolved with empty vector.
  EXPECT_EQ(std::vector<double>{}, future.Get());
}

TEST_F(LocalAIServiceTest, OnBackgroundContentsDestroyedFailsPending) {
  base::test::TestFuture<const std::vector<double>&> future;
  service_->GenerateEmbeddings("pending", future.GetCallback());

  delegate()->OnBackgroundContentsDestroyed();

  EXPECT_EQ(std::vector<double>{}, future.Get());
}

TEST_F(LocalAIServiceTest, ReinitializesAfterDestroyed) {
  BindFakeModelWorker();

  delegate()->OnBackgroundContentsDestroyed();

  // A new GenerateEmbeddings() call should queue (not crash) since
  // state was reset.
  base::test::TestFuture<const std::vector<double>&> future;
  service_->GenerateEmbeddings("after-crash", future.GetCallback());

  EXPECT_FALSE(future.IsReady());
}

TEST_F(LocalAIServiceTest, CloseTimeoutClosesWebContents) {
  BindFakeModelWorker();

  // Generate embeddings — starts the idle timer after forwarding.
  base::test::TestFuture<const std::vector<double>&> future;
  service_->GenerateEmbeddings("test", future.GetCallback());
  EXPECT_EQ(TestEmbedding(), future.Get());

  // Fast-forward past the close timeout (30 seconds).
  task_environment()->FastForwardBy(base::Seconds(30));

  // The idle timer should have closed the WebContents and reset the
  // remote. A new call should queue (not resolve) since it needs to
  // reinitialize.
  base::test::TestFuture<const std::vector<double>&> future2;
  service_->GenerateEmbeddings("after-idle", future2.GetCallback());
  EXPECT_FALSE(future2.IsReady());
}

TEST_F(LocalAIServiceTest, GenerateEmbeddingsResetsCloseTimeout) {
  BindFakeModelWorker();

  // First request starts the idle timer.
  base::test::TestFuture<const std::vector<double>&> future1;
  service_->GenerateEmbeddings("first", future1.GetCallback());
  EXPECT_EQ(TestEmbedding(), future1.Get());

  // Advance 20 seconds (not enough to trigger close).
  task_environment()->FastForwardBy(base::Seconds(20));

  // Second request should reset the timer.
  base::test::TestFuture<const std::vector<double>&> future2;
  service_->GenerateEmbeddings("second", future2.GetCallback());
  EXPECT_EQ(TestEmbedding(), future2.Get());

  // Advance another 20 seconds — 40s total since start, but only
  // 20s since last request. Timer should NOT have fired.
  task_environment()->FastForwardBy(base::Seconds(20));

  // Should still work without reinitializing.
  base::test::TestFuture<const std::vector<double>&> future3;
  service_->GenerateEmbeddings("third", future3.GetCallback());
  EXPECT_EQ(TestEmbedding(), future3.Get());
}

TEST_F(LocalAIServiceTest, ConnectionTimeoutFailsPendingRequests) {
  base::test::TestFuture<const std::vector<double>&> future;
  service_->GenerateEmbeddings("pending", future.GetCallback());

  // Worker never registers. Fast-forward past the connection timeout.
  task_environment()->FastForwardBy(base::Seconds(30));

  // Pending request should be resolved with empty vector.
  EXPECT_EQ(std::vector<double>{}, future.Get());

  // A new call should queue again (reinitializes).
  base::test::TestFuture<const std::vector<double>&> future2;
  service_->GenerateEmbeddings("retry", future2.GetCallback());
  EXPECT_FALSE(future2.IsReady());
}

TEST_F(LocalAIServiceTest, IdleTimeoutFiresWhenNoPendingRequests) {
  BindFakeModelWorker();

  // No requests were pending when the worker registered. The idle timer
  // should still close the WebContents after the timeout.
  task_environment()->FastForwardBy(base::Seconds(30));

  base::test::TestFuture<const std::vector<double>&> future;
  service_->GenerateEmbeddings("after-idle", future.GetCallback());
  EXPECT_FALSE(future.IsReady());
}

TEST_F(LocalAIServiceTest, DoubleShutdownIsIdempotent) {
  base::test::TestFuture<const std::vector<double>&> future;
  service_->GenerateEmbeddings("pending", future.GetCallback());

  auto* keyed_service = static_cast<KeyedService*>(service_.get());
  keyed_service->Shutdown();
  keyed_service->Shutdown();

  EXPECT_EQ(std::vector<double>{}, future.Get());
}

}  // namespace local_ai
