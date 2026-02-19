// Copyright (c) 2026 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

#include "brave/components/local_ai/core/local_ai_service.h"

#include <utility>

#include "base/logging.h"

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

namespace {
// Timeout before closing the BackgroundWebContents. Used as a connection
// timeout (worker failed to register) and an idle timeout (no in-flight
// requests after last response).
constexpr base::TimeDelta kCloseTimeout = base::Seconds(30);
}  // namespace

// LocalAIService implementation
LocalAIService::LocalAIService(
    BackgroundContentsFactory background_contents_factory)
    : background_contents_factory_(std::move(background_contents_factory)) {
  DVLOG(3) << "LocalAIService created";
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
  close_timer_.Stop();
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
        // Close contents and reset state so next
        // GenerateEmbeddings() will reinitialize
        service->CloseBackgroundContents();
      },
      base::Unretained(this)));

  DVLOG(3) << "RegisterOnDeviceModelWorker: Bound model worker";

  ProcessPendingRequests();
}

void LocalAIService::GenerateEmbeddings(const std::string& text,
                                        GenerateEmbeddingsCallback callback) {
  // Ensure BackgroundContents exists (may have been closed due to idle)
  EnsureBackgroundContents();

  if (!model_worker_remote_.is_bound()) {
    DVLOG(3) << "Model worker not ready yet, queuing request";
    pending_requests_.emplace_back(text, std::move(callback));
    return;
  }

  // Reset idle timer since we have activity
  close_timer_.Stop();
  ForwardRequest(text, std::move(callback));
}

void LocalAIService::OnBackgroundContentsDestroyed() {
  DVLOG(1) << "LocalAIService: Background contents destroyed";
  in_flight_count_ = 0;
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
    ForwardRequest(request.text, std::move(request.callback));
  }
  MaybeStartIdleTimer();
}

void LocalAIService::ForwardRequest(const std::string& text,
                                    GenerateEmbeddingsCallback callback) {
  in_flight_count_++;
  model_worker_remote_->GenerateEmbeddings(
      text,
      base::BindOnce(&LocalAIService::OnRequestComplete,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void LocalAIService::OnRequestComplete(GenerateEmbeddingsCallback callback,
                                       const std::vector<double>& result) {
  std::move(callback).Run(result);
  in_flight_count_--;
  MaybeStartIdleTimer();
}

void LocalAIService::MaybeStartIdleTimer() {
  if (in_flight_count_ > 0) {
    return;
  }
  close_timer_.Start(FROM_HERE, kCloseTimeout,
                     base::BindOnce(&LocalAIService::CloseBackgroundContents,
                                    weak_ptr_factory_.GetWeakPtr()));
}

void LocalAIService::EnsureBackgroundContents() {
  if (background_contents_) {
    return;  // Already created
  }

  DVLOG(3) << "LocalAIService: Creating background contents";

  background_contents_ = background_contents_factory_.Run(
      base::BindOnce(&LocalAIService::OnBackgroundContentsDestroyed,
                     weak_ptr_factory_.GetWeakPtr()));

  // Start connection timeout — if the worker doesn't register within
  // kCloseTimeout, close the background contents to avoid leaking.
  close_timer_.Start(FROM_HERE, kCloseTimeout,
                     base::BindOnce(&LocalAIService::CloseBackgroundContents,
                                    weak_ptr_factory_.GetWeakPtr()));
}

void LocalAIService::CloseBackgroundContents() {
  DVLOG(3) << "LocalAIService: Closing background contents "
              "to free memory";

  close_timer_.Stop();
  in_flight_count_ = 0;
  model_worker_remote_.reset();
  std::vector<PendingRequest> requests;
  requests.swap(pending_requests_);
  for (auto& request : requests) {
    std::move(request.callback).Run({});
  }
  background_contents_.reset();
}

}  // namespace local_ai
