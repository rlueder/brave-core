// Copyright (c) 2026 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

#ifndef BRAVE_COMPONENTS_LOCAL_AI_CORE_BACKGROUND_WEB_UI_H_
#define BRAVE_COMPONENTS_LOCAL_AI_CORE_BACKGROUND_WEB_UI_H_

namespace local_ai {

// Abstract interface for a background web environment that runs local AI
// model workers. The core/ layer talks only through this interface — it
// never sees WebContents, BrowserContext, or any content-layer type.
//
// Desktop: implemented by BackgroundWebUIImpl (content/).
// iOS: would be implemented by a WKWebView-based equivalent.
class BackgroundWebUI {
 public:
  class Delegate {
   public:
    // Called when the background environment has finished loading and
    // is ready to receive mojo connections.
    virtual void OnBackgroundContentsReady() = 0;

    // Called when the background environment is destroyed unexpectedly
    // (renderer crash, window.close, invalid URL). The BackgroundWebUI
    // instance is invalid after this call.
    virtual void OnBackgroundContentsDestroyed() = 0;

   protected:
    virtual ~Delegate() = default;
  };

  virtual ~BackgroundWebUI() = default;
};

}  // namespace local_ai

#endif  // BRAVE_COMPONENTS_LOCAL_AI_CORE_BACKGROUND_WEB_UI_H_
