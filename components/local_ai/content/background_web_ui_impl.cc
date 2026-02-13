// Copyright (c) 2026 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

#include "brave/components/local_ai/content/background_web_ui_impl.h"

#include <utility>

#include "base/logging.h"
#include "base/memory/ptr_util.h"

namespace local_ai {

// static
std::unique_ptr<BackgroundWebUI> BackgroundWebUIImpl::Create(
    content::BrowserContext* browser_context,
    const GURL& url,
    BackgroundWebUI::Delegate* delegate,
    BackgroundWebContents::WebContentsCreatedCallback
        web_contents_created_callback) {
  return base::WrapUnique(
      new BackgroundWebUIImpl(browser_context, url, delegate,
                              std::move(web_contents_created_callback)));
}

BackgroundWebUIImpl::BackgroundWebUIImpl(
    content::BrowserContext* browser_context,
    const GURL& url,
    BackgroundWebUI::Delegate* delegate,
    BackgroundWebContents::WebContentsCreatedCallback
        web_contents_created_callback)
    : delegate_(delegate) {
  background_contents_ = std::make_unique<BackgroundWebContents>(
      browser_context, url, this, std::move(web_contents_created_callback));
}

BackgroundWebUIImpl::~BackgroundWebUIImpl() = default;

void BackgroundWebUIImpl::OnBackgroundContentsReady() {
  DVLOG(3) << "BackgroundWebUIImpl: Background contents ready";
  delegate_->OnBackgroundContentsReady();
}

void BackgroundWebUIImpl::OnBackgroundContentsDestroyed(
    BackgroundWebContents::DestroyReason reason) {
  DVLOG(1) << "BackgroundWebUIImpl: Background contents destroyed";
  background_contents_.reset();
  delegate_->OnBackgroundContentsDestroyed();
  // |this| may be deleted.
}

}  // namespace local_ai
