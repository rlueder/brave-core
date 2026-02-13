// Copyright (c) 2026 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

#ifndef BRAVE_COMPONENTS_LOCAL_AI_CONTENT_BACKGROUND_WEB_UI_IMPL_H_
#define BRAVE_COMPONENTS_LOCAL_AI_CONTENT_BACKGROUND_WEB_UI_IMPL_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "brave/components/local_ai/content/background_web_contents.h"
#include "brave/components/local_ai/core/background_web_ui.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace local_ai {

// Content-layer adapter that owns a BackgroundWebContents and presents
// it as a BackgroundWebUI to the core/ layer.
class BackgroundWebUIImpl : public BackgroundWebUI,
                            public BackgroundWebContents::Delegate {
 public:
  // Static factory method for the content (desktop) platform.
  static std::unique_ptr<BackgroundWebUI> Create(
      content::BrowserContext* browser_context,
      const GURL& url,
      BackgroundWebUI::Delegate* delegate,
      BackgroundWebContents::WebContentsCreatedCallback
          web_contents_created_callback = {});

  ~BackgroundWebUIImpl() override;

  BackgroundWebUIImpl(const BackgroundWebUIImpl&) = delete;
  BackgroundWebUIImpl& operator=(const BackgroundWebUIImpl&) = delete;

 private:
  BackgroundWebUIImpl(content::BrowserContext* browser_context,
                      const GURL& url,
                      BackgroundWebUI::Delegate* delegate,
                      BackgroundWebContents::WebContentsCreatedCallback
                          web_contents_created_callback);

  // BackgroundWebContents::Delegate:
  void OnBackgroundContentsReady() override;
  void OnBackgroundContentsDestroyed(
      BackgroundWebContents::DestroyReason reason) override;

  raw_ptr<BackgroundWebUI::Delegate> delegate_;
  std::unique_ptr<BackgroundWebContents> background_contents_;
};

}  // namespace local_ai

#endif  // BRAVE_COMPONENTS_LOCAL_AI_CONTENT_BACKGROUND_WEB_UI_IMPL_H_
