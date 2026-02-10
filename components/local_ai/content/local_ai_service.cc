// Copyright (c) 2026 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

#include "brave/components/local_ai/content/local_ai_service.h"

#include "base/logging.h"
#include "brave/components/constants/webui_url_constants.h"
#include "url/gurl.h"

namespace local_ai {

LocalAIService::PendingEmbedRequest::PendingEmbedRequest() = default;
LocalAIService::PendingEmbedRequest::PendingEmbedRequest(std::string text,
                                                         EmbedCallback callback)
    : text(std::move(text)), callback(std::move(callback)) {}
LocalAIService::PendingEmbedRequest::~PendingEmbedRequest() = default;
LocalAIService::PendingEmbedRequest::PendingEmbedRequest(
    PendingEmbedRequest&&) = default;
LocalAIService::PendingEmbedRequest&
LocalAIService::PendingEmbedRequest::operator=(PendingEmbedRequest&&) = default;

// LocalAIService implementation
LocalAIService::LocalAIService(content::BrowserContext* browser_context,
                               WebContentsTagCallback web_contents_tag_callback)
    : browser_context_(browser_context),
      web_contents_tag_callback_(std::move(web_contents_tag_callback)) {
  DVLOG(3) << "LocalAIService created for browser context";

  if (!browser_context_) {
    DVLOG(0) << "LocalAIService: No browser context available";
    return;
  }
}

LocalAIService::~LocalAIService() {
  CloseWasmWebContents();
}

mojo::PendingRemote<mojom::LocalAIService> LocalAIService::MakeRemote() {
  mojo::PendingRemote<mojom::LocalAIService> remote;
  receivers_.Add(this, remote.InitWithNewPipeAndPassReceiver());
  return remote;
}

void LocalAIService::Bind(
    mojo::PendingReceiver<mojom::LocalAIService> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void LocalAIService::BindEmbeddingGemma(
    mojo::PendingRemote<mojom::EmbeddingGemmaInterface> pending_remote) {
  // Bind the single embedder remote from our WASM WebContents
  if (embedding_gemma_remote_.is_bound()) {
    DVLOG(1) << "EmbeddingGemma already bound, resetting";
    embedding_gemma_remote_.reset();
  }
  embedding_gemma_remote_.Bind(std::move(pending_remote));

  // Set up disconnect handler - this handles mojo pipe disconnections
  // that may not be renderer crashes (e.g. manual kills)
  embedding_gemma_remote_.set_disconnect_handler(base::BindOnce(
      [](LocalAIService* service) {
        DVLOG(1) << "EmbeddingGemma remote disconnected";
        // Swap-and-process to guard against re-entrancy
        std::vector<PendingEmbedRequest> requests;
        requests.swap(service->pending_embed_requests_);
        for (auto& request : requests) {
          std::move(request.callback).Run({});
        }
        // Close WebContents and reset state so next Embed() will
        // reinitialize
        service->CloseWasmWebContents();
      },
      base::Unretained(this)));

  DVLOG(3) << "BindEmbeddingGemma: Bound embedder remote";

  ProcessPendingEmbedRequests();
}

void LocalAIService::Embed(const std::string& text, EmbedCallback callback) {
  // Ensure WebContents exists
  EnsureWasmWebContents();

  if (!embedding_gemma_remote_.is_bound()) {
    DVLOG(3) << "Embedding not ready yet, queuing embed request";
    pending_embed_requests_.emplace_back(text, std::move(callback));
    return;
  }

  embedding_gemma_remote_->Embed(text, std::move(callback));
}

void LocalAIService::OnBackgroundContentsReady() {
  DVLOG(3) << "LocalAIService: Background contents ready";
}

void LocalAIService::OnBackgroundContentsDestroyed(
    BackgroundWebContents::DestroyReason reason) {
  DVLOG(1) << "LocalAIService: Background contents destroyed";
  std::vector<PendingEmbedRequest> requests;
  requests.swap(pending_embed_requests_);
  for (auto& request : requests) {
    std::move(request.callback).Run({});
  }
  background_contents_.reset();
  embedding_gemma_remote_.reset();
}

void LocalAIService::Shutdown() {
  DVLOG(3) << "LocalAIService: Shutting down";
  CloseWasmWebContents();
}

void LocalAIService::ProcessPendingEmbedRequests() {
  if (!embedding_gemma_remote_.is_bound()) {
    return;
  }

  DVLOG(3) << "Processing " << pending_embed_requests_.size()
           << " pending embed requests";

  // Swap-and-process to guard against re-entrancy
  std::vector<PendingEmbedRequest> requests;
  requests.swap(pending_embed_requests_);
  for (auto& request : requests) {
    embedding_gemma_remote_->Embed(request.text, std::move(request.callback));
  }
}

void LocalAIService::EnsureWasmWebContents() {
  if (background_contents_) {
    return;  // Already created
  }

  DVLOG(3) << "LocalAIService: Creating BackgroundWebContents";

  GURL wasm_url(kUntrustedCandleEmbeddingGemmaWasmURL);
  DVLOG(3) << "LocalAIService: Loading WASM from " << wasm_url;
  background_contents_ = std::make_unique<BackgroundWebContents>(
      browser_context_, wasm_url, this, web_contents_tag_callback_);
}

void LocalAIService::CloseWasmWebContents() {
  DVLOG(3) << "LocalAIService: Closing BackgroundWebContents "
              "to free memory";

  embedding_gemma_remote_.reset();
  std::vector<PendingEmbedRequest> requests;
  requests.swap(pending_embed_requests_);
  for (auto& request : requests) {
    std::move(request.callback).Run({});
  }
  background_contents_.reset();
}

}  // namespace local_ai
