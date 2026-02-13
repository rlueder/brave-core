// Copyright (c) 2026 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

#ifndef BRAVE_COMPONENTS_LOCAL_AI_CONTENT_BACKGROUND_CONTENTS_HOST_IMPL_H_
#define BRAVE_COMPONENTS_LOCAL_AI_CONTENT_BACKGROUND_CONTENTS_HOST_IMPL_H_

#include <memory>

#include "base/functional/callback.h"
#include "brave/components/local_ai/content/background_web_contents.h"
#include "brave/components/local_ai/core/background_contents_host.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace local_ai {

// Content-layer adapter that owns a BackgroundWebContents and presents
// it as an opaque BackgroundContentsHost to the core/ layer.
class BackgroundContentsHostImpl : public BackgroundContentsHost,
                                   public BackgroundWebContents::Delegate {
 public:
  BackgroundContentsHostImpl(content::BrowserContext* browser_context,
                             const GURL& url,
                             BackgroundWebContents::WebContentsCreatedCallback
                                 web_contents_created_callback,
                             base::OnceClosure destroyed_callback);
  ~BackgroundContentsHostImpl() override;

  BackgroundContentsHostImpl(const BackgroundContentsHostImpl&) = delete;
  BackgroundContentsHostImpl& operator=(const BackgroundContentsHostImpl&) =
      delete;

 private:
  // BackgroundWebContents::Delegate:
  void OnBackgroundContentsReady() override;
  void OnBackgroundContentsDestroyed(
      BackgroundWebContents::DestroyReason reason) override;

  std::unique_ptr<BackgroundWebContents> background_contents_;
  base::OnceClosure destroyed_callback_;
};

}  // namespace local_ai

#endif  // BRAVE_COMPONENTS_LOCAL_AI_CONTENT_BACKGROUND_CONTENTS_HOST_IMPL_H_
