// Copyright (c) 2025 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

#include "brave/components/local_ai/browser/local_ai_service.h"

#include "base/logging.h"
#include "brave/components/constants/webui_url_constants.h"
#include "url/gurl.h"

namespace local_ai {

LocalAIService::PendingRequest::PendingRequest() = default;
LocalAIService::PendingRequest::PendingRequest(
    std::string text,
    GenerateEmbeddingsCallback callback)
    : text(std::move(text)), callback(std::move(callback)) {}
LocalAIService::PendingRequest::~PendingRequest() = default;
LocalAIService::PendingRequest::PendingRequest(PendingRequest&&) = default;
LocalAIService::PendingRequest& LocalAIService::PendingRequest::operator=(
    PendingRequest&&) = default;

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
  CloseBackgroundContents();
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

void LocalAIService::RegisterOnDeviceModelWorker(
    mojo::PendingRemote<mojom::OnDeviceModelWorker> worker) {
  if (model_worker_remote_.is_bound()) {
    DVLOG(1) << "Model worker already bound, resetting";
    model_worker_remote_.reset();
  }
  model_worker_remote_.Bind(std::move(worker));

  // Set up disconnect handler - this handles mojo pipe disconnections
  // that may not be renderer crashes (e.g. manual kills)
  model_worker_remote_.set_disconnect_handler(base::BindOnce(
      [](LocalAIService* service) {
        DVLOG(1) << "Model worker remote disconnected";
        // Swap-and-process to guard against re-entrancy
        std::vector<PendingRequest> requests;
        requests.swap(service->pending_requests_);
        for (auto& request : requests) {
          std::move(request.callback).Run({});
        }
        // Close WebContents and reset state so next
        // GenerateEmbeddings() will reinitialize
        service->CloseBackgroundContents();
      },
      base::Unretained(this)));

  DVLOG(3) << "RegisterOnDeviceModelWorker: Bound model worker";

  ProcessPendingRequests();
}

void LocalAIService::GenerateEmbeddings(const std::string& text,
                                        GenerateEmbeddingsCallback callback) {
  // Ensure WebContents exists
  EnsureBackgroundContents();

  if (!model_worker_remote_.is_bound()) {
    DVLOG(3) << "Model worker not ready yet, queuing request";
    pending_requests_.emplace_back(text, std::move(callback));
    return;
  }

  model_worker_remote_->GenerateEmbeddings(text, std::move(callback));
}

void LocalAIService::OnBackgroundContentsReady() {
  DVLOG(3) << "LocalAIService: Background contents ready";
}

void LocalAIService::OnBackgroundContentsDestroyed() {
  DVLOG(1) << "LocalAIService: Background contents destroyed";
  std::vector<PendingRequest> requests;
  requests.swap(pending_requests_);
  for (auto& request : requests) {
    std::move(request.callback).Run({});
  }
  background_contents_.reset();
  model_worker_remote_.reset();
}

void LocalAIService::Shutdown() {
  DVLOG(3) << "LocalAIService: Shutting down";
  CloseBackgroundContents();
}

void LocalAIService::ProcessPendingRequests() {
  if (!model_worker_remote_.is_bound()) {
    return;
  }

  DVLOG(3) << "Processing " << pending_requests_.size() << " pending requests";

  // Swap-and-process to guard against re-entrancy
  std::vector<PendingRequest> requests;
  requests.swap(pending_requests_);
  for (auto& request : requests) {
    model_worker_remote_->GenerateEmbeddings(request.text,
                                             std::move(request.callback));
  }
}

void LocalAIService::EnsureBackgroundContents() {
  if (background_contents_) {
    return;  // Already created
  }

  DVLOG(3) << "LocalAIService: Creating BackgroundWebContents";

  GURL worker_url(kUntrustedOnDeviceModelWorkerURL);
  DVLOG(3) << "LocalAIService: Loading model worker from " << worker_url;
  background_contents_ = std::make_unique<BackgroundWebContents>(
      browser_context_, worker_url, this, web_contents_tag_callback_);
}

void LocalAIService::CloseBackgroundContents() {
  DVLOG(3) << "LocalAIService: Closing BackgroundWebContents "
              "to free memory";

  model_worker_remote_.reset();
  std::vector<PendingRequest> requests;
  requests.swap(pending_requests_);
  for (auto& request : requests) {
    std::move(request.callback).Run({});
  }
  background_contents_.reset();
}

}  // namespace local_ai
