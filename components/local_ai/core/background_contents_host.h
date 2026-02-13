// Copyright (c) 2026 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

#ifndef BRAVE_COMPONENTS_LOCAL_AI_CORE_BACKGROUND_CONTENTS_HOST_H_
#define BRAVE_COMPONENTS_LOCAL_AI_CORE_BACKGROUND_CONTENTS_HOST_H_

namespace local_ai {

// Opaque handle to a background WebContents. Destroying the handle tears
// down the underlying contents. The core/ layer never sees the actual
// WebContents — it only holds this handle.
class BackgroundContentsHost {
 public:
  virtual ~BackgroundContentsHost() = default;
};

}  // namespace local_ai

#endif  // BRAVE_COMPONENTS_LOCAL_AI_CORE_BACKGROUND_CONTENTS_HOST_H_
