// Copyright (c) 2026 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

#ifndef BRAVE_COMPONENTS_LOCAL_AI_CORE_LOCAL_AI_SERVICE_H_
#define BRAVE_COMPONENTS_LOCAL_AI_CORE_LOCAL_AI_SERVICE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "brave/components/local_ai/core/background_contents_host.h"
#include "brave/components/local_ai/core/local_ai.mojom.h"
#include "brave/components/local_ai/core/local_models_updater.h"
#include "components/keyed_service/core/keyed_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace local_ai {

// LocalAIService provides on-device machine learning capabilities.
//
// This service manages:
// - A BackgroundContentsHost that owns the ML model worker
// - Communication between the browser process and the renderer via Mojo
// - Request queueing while the model initializes
// - Cleanup on shutdown and renderer crash
// - Automatic cleanup after idle timeout to free memory
class LocalAIService : public KeyedService,
                       public mojom::LocalAIService,
                       public LocalModelsUpdaterState::Observer {
 public:
  // Called when the background contents is destroyed unexpectedly
  // (crash, invalid URL, window.close). The service uses this to
  // reset state and fail pending requests.
  using BackgroundContentsDestroyedCallback = base::OnceClosure;

  // Factory that creates a BackgroundContentsHost. The destroyed
  // callback fires if the contents dies unexpectedly.
  using BackgroundContentsFactory =
      base::RepeatingCallback<std::unique_ptr<BackgroundContentsHost>(
          BackgroundContentsDestroyedCallback)>;

  explicit LocalAIService(
      BackgroundContentsFactory background_contents_factory);
  ~LocalAIService() override;

  LocalAIService(const LocalAIService&) = delete;
  LocalAIService& operator=(const LocalAIService&) = delete;

  mojo::PendingRemote<mojom::LocalAIService> MakeRemote();
  void Bind(mojo::PendingReceiver<mojom::LocalAIService> receiver);

  // mojom::LocalAIService:
  void RegisterOnDeviceModelWorker(
      mojo::PendingRemote<mojom::OnDeviceModelWorker> worker) override;
  void GenerateEmbeddings(const std::string& text,
                          GenerateEmbeddingsCallback callback) override;

 private:
  // LocalModelsUpdaterState::Observer:
  void OnLocalModelsReady(const base::FilePath& install_dir) override;

  // KeyedService:
  void Shutdown() override;

  void OnBackgroundContentsDestroyed();

  void LoadModelFiles();
  void OnModelFilesLoaded(mojom::ModelFilesPtr model_files);
  void OnModelInitialized(bool success);
  void RetryLoadModel();
  void EnsureBackgroundContents();
  void CloseBackgroundContents();

  // Background contents host that owns the model worker page
  std::unique_ptr<BackgroundContentsHost> background_contents_;

  BackgroundContentsFactory background_contents_factory_;

  mojo::ReceiverSet<mojom::LocalAIService> receivers_;

  // Single model worker remote (shared by all callers)
  mojo::Remote<mojom::OnDeviceModelWorker> model_worker_remote_;

  // Model loading state
  base::FilePath pending_model_path_;
  int model_load_retry_count_ = 0;
  static constexpr int kMaxModelLoadRetries = 10;

  // Track readiness conditions
  bool wasm_page_loaded_ = false;
  bool models_ready_ = false;
  bool model_initialized_ = false;

  // Holds a GenerateEmbeddings() call that arrived before the model was
  // ready. Requests are drained in FIFO order once the model is
  // initialized.
  struct PendingRequest {
    PendingRequest();
    PendingRequest(std::string text, GenerateEmbeddingsCallback callback);
    ~PendingRequest();
    PendingRequest(PendingRequest&&);
    PendingRequest& operator=(PendingRequest&&);

    std::string text;
    GenerateEmbeddingsCallback callback;
  };
  std::vector<PendingRequest> pending_requests_;

  void TryLoadModel();
  void ProcessPendingRequests();
  void ForwardRequest(const std::string& text,
                      GenerateEmbeddingsCallback callback);
  void OnRequestComplete(GenerateEmbeddingsCallback callback,
                         const std::vector<double>& result);
  void MaybeStartIdleTimer();

  // Number of requests dispatched to the model worker awaiting response.
  int in_flight_count_ = 0;

  // Closes BackgroundWebContents after a timeout. Used as a connection
  // timeout (worker failed to register) and an idle timeout (no in-flight
  // requests after last response).
  base::OneShotTimer close_timer_;

  base::WeakPtrFactory<LocalAIService> weak_ptr_factory_{this};
};

}  // namespace local_ai

#endif  // BRAVE_COMPONENTS_LOCAL_AI_CORE_LOCAL_AI_SERVICE_H_
