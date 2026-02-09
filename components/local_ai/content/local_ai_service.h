// Copyright (c) 2026 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

#ifndef BRAVE_COMPONENTS_LOCAL_AI_CONTENT_LOCAL_AI_SERVICE_H_
#define BRAVE_COMPONENTS_LOCAL_AI_CONTENT_LOCAL_AI_SERVICE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "brave/components/local_ai/content/background_web_contents.h"
#include "brave/components/local_ai/core/local_ai.mojom.h"
#include "components/keyed_service/core/keyed_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace local_ai {

// LocalAIService provides on-device machine learning capabilities, currently
// using the Candle ML framework for execution via WebAssembly.
//
// This service manages:
// - A BackgroundWebContents that loads and runs the WASM-based ML model
// - Communication between the browser process and the WASM renderer via Mojo
// - Request queueing while the model initializes
// - Cleanup on shutdown and renderer crash
//
// The service is currently implemented using Candle (see candle_embedding_gemma
// resources), but the API is framework-agnostic to allow future flexibility.
class LocalAIService : public KeyedService,
                       public mojom::LocalAIService,
                       public BackgroundWebContents::Delegate {
 public:
  explicit LocalAIService(content::BrowserContext* browser_context);
  ~LocalAIService() override;

  LocalAIService(const LocalAIService&) = delete;
  LocalAIService& operator=(const LocalAIService&) = delete;

  mojo::PendingRemote<mojom::LocalAIService> MakeRemote();
  void Bind(mojo::PendingReceiver<mojom::LocalAIService> receiver);

  void BindEmbeddingGemma(
      mojo::PendingRemote<mojom::EmbeddingGemmaInterface>) override;

  void Embed(const std::string& text, EmbedCallback callback) override;

 private:
  // KeyedService:
  void Shutdown() override;

  // BackgroundWebContents::Delegate:
  void OnBackgroundContentsReady() override;
  void OnBackgroundContentsDestroyed() override;

  void EnsureWasmWebContents();
  void CloseWasmWebContents();

  raw_ptr<content::BrowserContext> browser_context_ = nullptr;

  // Background WebContents that loads the WASM and maintains the model
  std::unique_ptr<BackgroundWebContents> background_contents_;

  mojo::ReceiverSet<mojom::LocalAIService> receivers_;

  // Single embedder remote (shared by all callers)
  mojo::Remote<mojom::EmbeddingGemmaInterface> embedding_gemma_remote_;

  // Holds an Embed() call that arrived before the WASM model was ready.
  // Requests are drained in FIFO order once the model is initialized.
  struct PendingEmbedRequest {
    PendingEmbedRequest();
    PendingEmbedRequest(std::string text, EmbedCallback callback);
    ~PendingEmbedRequest();
    PendingEmbedRequest(PendingEmbedRequest&&);
    PendingEmbedRequest& operator=(PendingEmbedRequest&&);

    std::string text;
    EmbedCallback callback;
  };
  std::vector<PendingEmbedRequest> pending_embed_requests_;

  void ProcessPendingEmbedRequests();

  base::WeakPtrFactory<LocalAIService> weak_ptr_factory_{this};
};

}  // namespace local_ai

#endif  // BRAVE_COMPONENTS_LOCAL_AI_CONTENT_LOCAL_AI_SERVICE_H_
