// Copyright (c) 2025 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

import {
  LocalAIService,
  OnDeviceModelWorkerReceiver,
} from 'gen/brave/components/local_ai/common/local_ai.mojom.m.js'

console.log('[Candle WASM] On-device model worker script loaded')

console.log('[Candle WASM] Initializing Mojo connection...')

// Initialize connection to the browser-side LocalAIService
const localAIService = LocalAIService.getRemote()

console.log('[Candle WASM] LocalAIService remote obtained:', localAIService)

// Implement the OnDeviceModelWorker Mojo interface
class OnDeviceModelWorkerImpl {
  receiver: OnDeviceModelWorkerReceiver

  constructor() {
    this.receiver = new OnDeviceModelWorkerReceiver(this)
  }

  // Implementation of OnDeviceModelWorker::GenerateEmbeddings
  async generateEmbeddings(input: string): Promise<{ output: number[] }> {
    console.log(
      '[Candle WASM] GenerateEmbeddings called' + ' (model not loaded yet):',
      input,
    )
    // Model loading will be added in branch 3
    return { output: [] }
  }

  getPendingRemote() {
    return this.receiver.$.bindNewPipeAndPassRemote()
  }
}

// Create and register the OnDeviceModelWorker implementation
console.log('[Candle WASM] Creating OnDeviceModelWorker implementation...')
const modelWorkerImpl = new OnDeviceModelWorkerImpl()
console.log(
  '[Candle WASM] Registering OnDeviceModelWorker' + ' with LocalAIService...',
)
localAIService.registerOnDeviceModelWorker(modelWorkerImpl.getPendingRemote())

console.log('[Candle WASM] On-device model worker bridge initialized!')
