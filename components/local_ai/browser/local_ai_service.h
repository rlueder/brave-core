// Copyright (c) 2025 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

#ifndef BRAVE_COMPONENTS_LOCAL_AI_BROWSER_LOCAL_AI_SERVICE_H_
#define BRAVE_COMPONENTS_LOCAL_AI_BROWSER_LOCAL_AI_SERVICE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "brave/components/local_ai/browser/background_web_contents.h"
#include "brave/components/local_ai/common/local_ai.mojom.h"
#include "components/keyed_service/core/keyed_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace local_ai {

// LocalAIService provides on-device machine learning capabilities.
//
// This service manages:
// - A BackgroundWebContents that loads and runs the ML model worker
// - Communication between the browser process and the renderer via Mojo
// - Request queueing while the model initializes
// - Automatic cleanup after idle timeout to free memory
class LocalAIService : public KeyedService,
                       public mojom::LocalAIService,
                       public BackgroundWebContents::Delegate {
 public:
  using WebContentsTagCallback =
      base::RepeatingCallback<void(content::WebContents*)>;

  // |web_contents_tag_callback| is called after creating a
  // BackgroundWebContents to tag it for the task manager. Injected by
  // the browser layer since task_manager is in chrome/.
  LocalAIService(content::BrowserContext* browser_context,
                 WebContentsTagCallback web_contents_tag_callback);
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
  // KeyedService:
  void Shutdown() override;

  // BackgroundWebContents::Delegate:
  void OnBackgroundContentsReady() override;
  void OnBackgroundContentsDestroyed() override;

  void EnsureBackgroundContents();
  void CloseBackgroundContents();

  raw_ptr<content::BrowserContext> browser_context_ = nullptr;

  // Background WebContents that loads and maintains the model worker
  std::unique_ptr<BackgroundWebContents> background_contents_;

  mojo::ReceiverSet<mojom::LocalAIService> receivers_;

  // Single model worker remote (shared by all callers)
  mojo::Remote<mojom::OnDeviceModelWorker> model_worker_remote_;

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

  void ProcessPendingRequests();
  void ForwardRequest(const std::string& text,
                      GenerateEmbeddingsCallback callback);
  void OnRequestComplete(GenerateEmbeddingsCallback callback,
                         const std::vector<double>& result);
  void MaybeStartIdleTimer();

  WebContentsTagCallback web_contents_tag_callback_;

  // Number of requests dispatched to the model worker awaiting response.
  int in_flight_count_ = 0;

  // Closes BackgroundWebContents after a timeout. Used as a connection
  // timeout (worker failed to register) and an idle timeout (no in-flight
  // requests after last response).
  base::OneShotTimer close_timer_;

  base::WeakPtrFactory<LocalAIService> weak_ptr_factory_{this};
};

}  // namespace local_ai

#endif  // BRAVE_COMPONENTS_LOCAL_AI_BROWSER_LOCAL_AI_SERVICE_H_
