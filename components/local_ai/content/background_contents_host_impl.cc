// Copyright (c) 2026 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

#include "brave/components/local_ai/content/background_contents_host_impl.h"

#include <utility>

#include "base/logging.h"

namespace local_ai {

BackgroundContentsHostImpl::BackgroundContentsHostImpl(
    content::BrowserContext* browser_context,
    const GURL& url,
    BackgroundWebContents::WebContentsCreatedCallback
        web_contents_created_callback,
    base::OnceClosure destroyed_callback)
    : destroyed_callback_(std::move(destroyed_callback)) {
  background_contents_ = std::make_unique<BackgroundWebContents>(
      browser_context, url, this, std::move(web_contents_created_callback));
}

BackgroundContentsHostImpl::~BackgroundContentsHostImpl() = default;

void BackgroundContentsHostImpl::OnBackgroundContentsReady() {
  DVLOG(3) << "BackgroundContentsHostImpl: Background contents ready";
}

void BackgroundContentsHostImpl::OnBackgroundContentsDestroyed(
    BackgroundWebContents::DestroyReason reason) {
  DVLOG(1) << "BackgroundContentsHostImpl: Background contents destroyed";
  background_contents_.reset();
  if (destroyed_callback_) {
    std::move(destroyed_callback_).Run();
    // |this| may be deleted.
  }
}

}  // namespace local_ai
