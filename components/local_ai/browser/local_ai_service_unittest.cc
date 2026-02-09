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

// Fake EmbeddingGemma that returns a fixed embedding vector.
class FakeEmbeddingGemma : public mojom::EmbeddingGemmaInterface {
 public:
  void Embed(const std::string& input, EmbedCallback callback) override {
    embed_count_++;
    std::move(callback).Run(TestEmbedding());
  }

  mojo::PendingRemote<mojom::EmbeddingGemmaInterface>
  BindNewPipeAndPassRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  void Reset() { receiver_.reset(); }

  int embed_count() const { return embed_count_; }

 private:
  int embed_count_ = 0;
  mojo::Receiver<mojom::EmbeddingGemmaInterface> receiver_{this};
};

}  // namespace

class LocalAIServiceTest : public content::RenderViewHostTestHarness {
 protected:
  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    service_ = std::make_unique<LocalAIService>(browser_context());
  }

  void TearDown() override {
    static_cast<KeyedService*>(service_.get())->Shutdown();
    service_.reset();
    content::RenderViewHostTestHarness::TearDown();
  }

  // Simulate the WASM page binding its EmbeddingGemma interface.
  void BindFakeEmbeddingGemma() {
    service_->BindEmbeddingGemma(
        fake_embedding_gemma_.BindNewPipeAndPassRemote());
  }

  // Access BackgroundWebContents::Delegate methods through the
  // public base class interface.
  BackgroundWebContents::Delegate* delegate() {
    return static_cast<BackgroundWebContents::Delegate*>(service_.get());
  }

  std::unique_ptr<LocalAIService> service_;
  FakeEmbeddingGemma fake_embedding_gemma_;
};

TEST_F(LocalAIServiceTest, EmbedCreatesBackgroundContents) {
  // Calling Embed() should lazily create the BackgroundWebContents.
  base::test::TestFuture<const std::vector<double>&> future;
  service_->Embed("test", future.GetCallback());

  EXPECT_FALSE(future.IsReady());
}

TEST_F(LocalAIServiceTest, EmbedQueuesWhenNotReady) {
  base::test::TestFuture<const std::vector<double>&> future1;
  base::test::TestFuture<const std::vector<double>&> future2;

  service_->Embed("hello", future1.GetCallback());
  service_->Embed("world", future2.GetCallback());

  // Both should be queued, not resolved.
  EXPECT_FALSE(future1.IsReady());
  EXPECT_FALSE(future2.IsReady());
}

TEST_F(LocalAIServiceTest, BindEmbeddingGemmaProcessesPendingRequests) {
  base::test::TestFuture<const std::vector<double>&> future1;
  base::test::TestFuture<const std::vector<double>&> future2;

  service_->Embed("hello", future1.GetCallback());
  service_->Embed("world", future2.GetCallback());

  BindFakeEmbeddingGemma();

  EXPECT_EQ(TestEmbedding(), future1.Get());
  EXPECT_EQ(TestEmbedding(), future2.Get());
  EXPECT_EQ(2, fake_embedding_gemma_.embed_count());
}

TEST_F(LocalAIServiceTest, EmbedForwardsDirectlyWhenReady) {
  BindFakeEmbeddingGemma();

  // Now that we're ready, Embed should go directly to the remote.
  service_->Embed("test", base::DoNothing());

  base::test::TestFuture<const std::vector<double>&> future;
  service_->Embed("direct", future.GetCallback());

  EXPECT_EQ(TestEmbedding(), future.Get());
}

TEST_F(LocalAIServiceTest, ShutdownFailsPendingRequests) {
  base::test::TestFuture<const std::vector<double>&> future;
  service_->Embed("pending", future.GetCallback());

  static_cast<KeyedService*>(service_.get())->Shutdown();

  // Pending request should be resolved with empty vector.
  EXPECT_EQ(std::vector<double>{}, future.Get());
}

TEST_F(LocalAIServiceTest, OnBackgroundContentsDestroyedFailsPending) {
  base::test::TestFuture<const std::vector<double>&> future;
  service_->Embed("pending", future.GetCallback());

  delegate()->OnBackgroundContentsDestroyed();

  EXPECT_EQ(std::vector<double>{}, future.Get());
}

TEST_F(LocalAIServiceTest, ReinitializesAfterDestroyed) {
  BindFakeEmbeddingGemma();

  delegate()->OnBackgroundContentsDestroyed();

  // A new Embed() call should queue (not crash) since state
  // was reset.
  base::test::TestFuture<const std::vector<double>&> future;
  service_->Embed("after-crash", future.GetCallback());

  EXPECT_FALSE(future.IsReady());
}

TEST_F(LocalAIServiceTest, DoubleShutdownIsIdempotent) {
  base::test::TestFuture<const std::vector<double>&> future;
  service_->Embed("pending", future.GetCallback());

  auto* keyed_service = static_cast<KeyedService*>(service_.get());
  keyed_service->Shutdown();
  keyed_service->Shutdown();

  EXPECT_EQ(std::vector<double>{}, future.Get());
}

}  // namespace local_ai
