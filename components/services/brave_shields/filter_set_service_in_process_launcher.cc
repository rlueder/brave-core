/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "brave/components/services/brave_shields/filter_set_service_in_process_launcher.h"

#include <memory>
#include <utility>

#include "base/task/thread_pool.h"
#include "brave/components/services/brave_shields/filter_set_service.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace brave_shields {

namespace {
void BindInProcessFilterSetParser(
    mojo::PendingReceiver<filter_set::mojom::UtilParseFilterSet> receiver) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<brave_shields::FilterSetService>(), std::move(receiver));
}
}  // namespace

mojo::PendingRemote<filter_set::mojom::UtilParseFilterSet>
LaunchInProcessFilterSetParser() {
  mojo::PendingRemote<filter_set::mojom::UtilParseFilterSet> remote;
  base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::WithBaseSyncPrimitives()})
      ->PostTask(FROM_HERE,
                 base::BindOnce(&BindInProcessFilterSetParser,
                                remote.InitWithNewPipeAndPassReceiver()));
  return remote;
}

}  // namespace brave_shields
