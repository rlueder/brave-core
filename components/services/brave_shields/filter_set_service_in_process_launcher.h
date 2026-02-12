/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_COMPONENTS_SERVICES_BRAVE_SHIELDS_FILTER_SET_SERVICE_IN_PROCESS_LAUNCHER_H_
#define BRAVE_COMPONENTS_SERVICES_BRAVE_SHIELDS_FILTER_SET_SERVICE_IN_PROCESS_LAUNCHER_H_

#include "brave/components/services/brave_shields/mojom/filter_set.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace brave_shields {

mojo::PendingRemote<filter_set::mojom::UtilParseFilterSet>
LaunchInProcessFilterSetParser();

}  // namespace brave_shields

#endif  // BRAVE_COMPONENTS_SERVICES_BRAVE_SHIELDS_FILTER_SET_SERVICE_IN_PROCESS_LAUNCHER_H_
