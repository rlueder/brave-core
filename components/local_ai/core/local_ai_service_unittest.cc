// Copyright (c) 2026 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

#include "brave/components/local_ai/core/local_ai_service.h"

#include <string>
#include <vector>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "brave/components/local_ai/core/background_contents_host.h"
#include "brave/components/local_ai/core/local_ai.mojom.h"
#include "brave/components/local_ai/core/local_models_updater.h"
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

// Fake BackgroundContentsHost that stores the destroyed callback so
// tests can trigger it manually.
class FakeBackgroundContentsHost : public BackgroundContentsHost {
 public:
  explicit FakeBackgroundContentsHost(base::OnceClosure destroyed_callback)
      : destroyed_callback_(std::move(destroyed_callback)) {}

  void SimulateDestroyed() {
    if (destroyed_callback_) {
      std::move(destroyed_callback_).Run();
    }
  }

 private:
  base::OnceClosure destroyed_callback_;
};

}  // namespace

class LocalAIServiceTest : public testing::Test {
 protected:
  void SetUp() override {
    service_ = std::make_unique<LocalAIService>(base::BindRepeating(
        &LocalAIServiceTest::CreateFakeHost, base::Unretained(this)));
  }

  void TearDown() override {
    static_cast<KeyedService*>(service_.get())->Shutdown();
    service_.reset();
    // Clear the singleton state for test isolation.
    LocalModelsUpdaterState::GetInstance()->SetInstallDir(base::FilePath());
  }

  std::unique_ptr<BackgroundContentsHost> CreateFakeHost(
      LocalAIService::BackgroundContentsDestroyedCallback destroyed_callback) {
    auto host = std::make_unique<FakeBackgroundContentsHost>(
        std::move(destroyed_callback));
    last_created_host_ = host.get();
    return host;
  }

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

  // Simulate both readiness conditions being met:
  // 1. Component ready (OnLocalModelsReady via SetInstallDir)
  // 2. Mojo remote bound (RegisterOnDeviceModelWorker — also sets
  //    wasm_page_loaded_)
  // After these, TryLoadModel() will trigger Init() on the fake.
  void MakeFullyReady() {
    // Trigger GenerateEmbeddings to create BackgroundContents.
    base::test::TestFuture<const std::vector<double>&> discard;
    service_->GenerateEmbeddings("warmup", discard.GetCallback());

    // 1. Component ready (sets up files + notifies).
    SetUpModelFiles();
    // 2. Bind the mojo remote (sets wasm_page_loaded_ and triggers
    //    TryLoadModel).
    BindFakeModelWorker();

    // TestFuture::Wait() runs a RunLoop until the callback fires,
    // which covers the thread-pool hop for LoadModelFiles and the
    // mojo round-trip through Init() -> ProcessPendingRequests.
    ASSERT_TRUE(discard.Wait());
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<LocalAIService> service_;
  FakeModelWorker fake_model_worker_;
  raw_ptr<FakeBackgroundContentsHost> last_created_host_ = nullptr;
  base::ScopedTempDir temp_dir_;
};

TEST_F(LocalAIServiceTest, GenerateEmbeddingsCreatesBackgroundContents) {
  base::test::TestFuture<const std::vector<double>&> future;
  service_->GenerateEmbeddings("test", future.GetCallback());

  EXPECT_TRUE(last_created_host_);
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

  ASSERT_TRUE(last_created_host_);
  last_created_host_->SimulateDestroyed();

  EXPECT_EQ(std::vector<double>{}, future.Get());
}

TEST_F(LocalAIServiceTest, ReinitializesAfterDestroyed) {
  MakeFullyReady();

  ASSERT_TRUE(last_created_host_);
  last_created_host_->SimulateDestroyed();

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
  task_environment_.FastForwardBy(base::Seconds(30));

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
  task_environment_.FastForwardBy(base::Seconds(20));

  // Second request should reset the timer.
  base::test::TestFuture<const std::vector<double>&> future2;
  service_->GenerateEmbeddings("second", future2.GetCallback());
  EXPECT_EQ(TestEmbedding(), future2.Get());

  // Advance another 20 seconds — 40s total since start, but only
  // 20s since last request. Timer should NOT have fired.
  task_environment_.FastForwardBy(base::Seconds(20));

  // Should still work without reinitializing.
  base::test::TestFuture<const std::vector<double>&> future3;
  service_->GenerateEmbeddings("third", future3.GetCallback());
  EXPECT_EQ(TestEmbedding(), future3.Get());
}

TEST_F(LocalAIServiceTest, ConnectionTimeoutFailsPendingRequests) {
  base::test::TestFuture<const std::vector<double>&> future;
  service_->GenerateEmbeddings("pending", future.GetCallback());

  // Worker never registers. Fast-forward past the connection timeout.
  task_environment_.FastForwardBy(base::Seconds(30));

  // Pending request should be resolved with empty vector.
  EXPECT_EQ(std::vector<double>{}, future.Get());

  // A new call should queue again (reinitializes).
  base::test::TestFuture<const std::vector<double>&> future2;
  service_->GenerateEmbeddings("retry", future2.GetCallback());
  EXPECT_FALSE(future2.IsReady());
}

TEST_F(LocalAIServiceTest, IdleTimeoutFiresWhenNoPendingRequests) {
  MakeFullyReady();

  // No requests were pending after warmup. The idle timer
  // should still close the WebContents after the timeout.
  task_environment_.FastForwardBy(base::Seconds(30));

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

TEST_F(LocalAIServiceTest, TryLoadModelWaitsForBothConditions) {
  // Queue a request.
  base::test::TestFuture<const std::vector<double>&> future;
  service_->GenerateEmbeddings("test", future.GetCallback());

  // Only component ready - not enough (no worker).
  SetUpModelFiles();
  EXPECT_FALSE(future.IsReady());

  // Both conditions met - now Init() + GenerateEmbeddings() should complete.
  BindFakeModelWorker();
  EXPECT_EQ(TestEmbedding(), future.Get());
  EXPECT_EQ(1, fake_model_worker_.init_count());
}

TEST_F(LocalAIServiceTest, TryLoadModelDoesNotLoadIfAlreadyInitialized) {
  MakeFullyReady();

  // Triggering models ready again should not re-init.
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

TEST_F(LocalAIServiceTest, ComponentReadyBeforeWorkerTriggersLoad) {
  base::test::TestFuture<const std::vector<double>&> future;
  service_->GenerateEmbeddings("test", future.GetCallback());

  // Component ready first, then worker registers.
  SetUpModelFiles();
  BindFakeModelWorker();

  EXPECT_EQ(TestEmbedding(), future.Get());
  EXPECT_EQ(1, fake_model_worker_.init_count());
}

}  // namespace local_ai
