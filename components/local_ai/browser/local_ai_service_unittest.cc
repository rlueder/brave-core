// Copyright (c) 2025 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

#include "brave/components/local_ai/browser/local_ai_service.h"

#include <string>
#include <vector>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/test_future.h"
#include "brave/components/local_ai/browser/local_models_updater.h"
#include "brave/components/local_ai/common/local_ai.mojom.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/test/test_renderer_host.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace local_ai {

namespace {

constexpr double kTestEmbeddingData[] = {1.0, 2.0, 3.0};

std::vector<double> TestEmbedding() {
  return {std::begin(kTestEmbeddingData), std::end(kTestEmbeddingData)};
}

// Fake model worker that accepts Init() and returns a fixed
// embedding vector from GenerateEmbeddings().
class FakeModelWorker : public mojom::OnDeviceModelWorker {
 public:
  void Init(mojom::ModelFilesPtr model_files, InitCallback callback) override {
    init_count_++;
    std::move(callback).Run(init_success_);
  }

  void GenerateEmbeddings(const std::string& input,
                          GenerateEmbeddingsCallback callback) override {
    embed_count_++;
    std::move(callback).Run(TestEmbedding());
  }

  mojo::PendingRemote<mojom::OnDeviceModelWorker> BindNewPipeAndPassRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  void Reset() { receiver_.reset(); }

  void set_init_success(bool success) { init_success_ = success; }
  int init_count() const { return init_count_; }
  int embed_count() const { return embed_count_; }

 private:
  bool init_success_ = true;
  int init_count_ = 0;
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
    // Clear the singleton state for test isolation.
    LocalModelsUpdaterState::GetInstance()->SetInstallDir(base::FilePath());
    content::RenderViewHostTestHarness::TearDown();
  }

  // Simulate the model worker page registering its interface.
  void BindFakeModelWorker() {
    service_->RegisterOnDeviceModelWorker(
        fake_model_worker_.BindNewPipeAndPassRemote());
  }

  // Set up dummy model files on disk and notify the updater state.
  void SetUpModelFiles() {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    base::FilePath dir = temp_dir_.GetPath();
    base::FilePath model_dir = dir.AppendASCII(kEmbeddingGemmaModelDir);
    ASSERT_TRUE(base::CreateDirectory(model_dir));
    ASSERT_TRUE(
        base::CreateDirectory(model_dir.AppendASCII(kEmbeddingGemmaDense1Dir)));
    ASSERT_TRUE(
        base::CreateDirectory(model_dir.AppendASCII(kEmbeddingGemmaDense2Dir)));

    // Create dummy model files matching expected paths.
    ASSERT_TRUE(
        base::WriteFile(model_dir.AppendASCII(kEmbeddingGemmaModelFile), "w"));
    ASSERT_TRUE(base::WriteFile(model_dir.AppendASCII(kEmbeddingGemmaDense1Dir)
                                    .AppendASCII(kEmbeddingGemmaDenseModelFile),
                                "d1"));
    ASSERT_TRUE(base::WriteFile(model_dir.AppendASCII(kEmbeddingGemmaDense2Dir)
                                    .AppendASCII(kEmbeddingGemmaDenseModelFile),
                                "d2"));
    ASSERT_TRUE(base::WriteFile(
        model_dir.AppendASCII(kEmbeddingGemmaTokenizerFile), "t"));
    ASSERT_TRUE(
        base::WriteFile(model_dir.AppendASCII(kEmbeddingGemmaConfigFile), "c"));

    // This fires OnLocalModelsReady on the service.
    LocalModelsUpdaterState::GetInstance()->SetInstallDir(dir);
  }

  // Simulate all three readiness conditions being met:
  // 1. WASM page loaded (OnBackgroundContentsReady)
  // 2. Component ready (OnLocalModelsReady via SetInstallDir)
  // 3. Mojo remote bound (RegisterOnDeviceModelWorker)
  // After these, TryLoadModel() will trigger Init() on the fake.
  void MakeFullyReady() {
    // Trigger GenerateEmbeddings to create BackgroundWebContents.
    base::test::TestFuture<const std::vector<double>&> discard;
    service_->GenerateEmbeddings("warmup", discard.GetCallback());

    // 1. WASM loaded.
    delegate()->OnBackgroundContentsReady();
    // 2. Component ready (sets up files + notifies).
    SetUpModelFiles();
    // 3. Bind the mojo remote (triggers TryLoadModel).
    BindFakeModelWorker();

    // TestFuture::Wait() runs a RunLoop until the callback fires,
    // which covers the thread-pool hop for LoadModelFiles and the
    // mojo round-trip through Init() → ProcessPendingRequests.
    ASSERT_TRUE(discard.Wait());
  }

  BackgroundWebContents::Delegate* delegate() {
    return static_cast<BackgroundWebContents::Delegate*>(service_.get());
  }

  LocalModelsUpdaterState::Observer* observer() {
    return static_cast<LocalModelsUpdaterState::Observer*>(service_.get());
  }

  std::unique_ptr<LocalAIService> service_;
  FakeModelWorker fake_model_worker_;
  base::ScopedTempDir temp_dir_;
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

  EXPECT_FALSE(future1.IsReady());
  EXPECT_FALSE(future2.IsReady());
}

TEST_F(LocalAIServiceTest, ShutdownFailsPendingRequests) {
  base::test::TestFuture<const std::vector<double>&> future;
  service_->GenerateEmbeddings("pending", future.GetCallback());

  static_cast<KeyedService*>(service_.get())->Shutdown();

  EXPECT_EQ(std::vector<double>{}, future.Get());
}

TEST_F(LocalAIServiceTest, OnBackgroundContentsDestroyedFailsPending) {
  base::test::TestFuture<const std::vector<double>&> future;
  service_->GenerateEmbeddings("pending", future.GetCallback());

  delegate()->OnBackgroundContentsDestroyed();

  EXPECT_EQ(std::vector<double>{}, future.Get());
}

TEST_F(LocalAIServiceTest, ReinitializesAfterDestroyed) {
  MakeFullyReady();

  delegate()->OnBackgroundContentsDestroyed();

  base::test::TestFuture<const std::vector<double>&> future;
  service_->GenerateEmbeddings("after-crash", future.GetCallback());

  EXPECT_FALSE(future.IsReady());
}

TEST_F(LocalAIServiceTest, CloseTimeoutClosesWebContents) {
  MakeFullyReady();

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
  MakeFullyReady();

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

TEST_F(LocalAIServiceTest, TryLoadModelWaitsForAllThreeConditions) {
  // Queue a request.
  base::test::TestFuture<const std::vector<double>&> future;
  service_->GenerateEmbeddings("test", future.GetCallback());

  // Only WASM loaded - not enough.
  delegate()->OnBackgroundContentsReady();
  EXPECT_FALSE(future.IsReady());

  // WASM + component - still not enough (no mojo remote).
  SetUpModelFiles();
  EXPECT_FALSE(future.IsReady());

  // All three - now Init() + GenerateEmbeddings() should complete.
  BindFakeModelWorker();
  EXPECT_EQ(TestEmbedding(), future.Get());
  EXPECT_EQ(1, fake_model_worker_.init_count());
}

TEST_F(LocalAIServiceTest, TryLoadModelDoesNotLoadIfAlreadyInitialized) {
  MakeFullyReady();

  delegate()->OnBackgroundContentsReady();
  EXPECT_EQ(1, fake_model_worker_.init_count());
}

TEST_F(LocalAIServiceTest, FullReadinessProcessesPendingRequests) {
  base::test::TestFuture<const std::vector<double>&> future1;
  base::test::TestFuture<const std::vector<double>&> future2;

  service_->GenerateEmbeddings("hello", future1.GetCallback());
  service_->GenerateEmbeddings("world", future2.GetCallback());

  MakeFullyReady();

  EXPECT_EQ(TestEmbedding(), future1.Get());
  EXPECT_EQ(TestEmbedding(), future2.Get());
}

TEST_F(LocalAIServiceTest, GenerateEmbeddingsForwardsDirectlyWhenReady) {
  MakeFullyReady();

  base::test::TestFuture<const std::vector<double>&> future;
  service_->GenerateEmbeddings("direct", future.GetCallback());

  EXPECT_EQ(TestEmbedding(), future.Get());
}

TEST_F(LocalAIServiceTest, RegisterModelWorkerAloneIsNotReady) {
  base::test::TestFuture<const std::vector<double>&> future;
  service_->GenerateEmbeddings("test", future.GetCallback());

  BindFakeModelWorker();

  EXPECT_FALSE(future.IsReady());
  EXPECT_EQ(0, fake_model_worker_.init_count());
}

TEST_F(LocalAIServiceTest, ComponentReadyBeforeWasmDoesNotTriggerLoad) {
  base::test::TestFuture<const std::vector<double>&> future;
  service_->GenerateEmbeddings("test", future.GetCallback());

  SetUpModelFiles();
  BindFakeModelWorker();

  EXPECT_FALSE(future.IsReady());
  EXPECT_EQ(0, fake_model_worker_.init_count());

  delegate()->OnBackgroundContentsReady();
  EXPECT_EQ(TestEmbedding(), future.Get());
  EXPECT_EQ(1, fake_model_worker_.init_count());
}

}  // namespace local_ai
