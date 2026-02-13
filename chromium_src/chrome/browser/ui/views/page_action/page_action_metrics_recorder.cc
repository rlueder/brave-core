// Copyright (c) 2026 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

#include "chrome/browser/ui/views/page_action/page_action_metrics_recorder.h"

#define RecordIconShown RecordIconShown_Chromium
#define RecordChipShown RecordChipShown_Chromium
#define RecordIconClick RecordIconClick_Chromium
#define RecordChipClick RecordChipClick_Chromium

#include <chrome/browser/ui/views/page_action/page_action_metrics_recorder.cc>

#undef RecordChipClick
#undef RecordIconClick
#undef RecordChipShown
#undef RecordIconShown
