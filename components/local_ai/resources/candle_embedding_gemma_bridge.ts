// Copyright (c) 2025 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

import {
  LocalAIService,
  EmbeddingGemmaInterfaceReceiver,
} from 'gen/brave/components/local_ai/common/local_ai.mojom.m.js'

console.log('[Candle WASM] Embedding Gemma Script loaded')

console.log('[Candle WASM] Initializing Mojo connection...')

// Initialize connection to the browser-side LocalAIService
const localAIService = LocalAIService.getRemote()

console.log('[Candle WASM] LocalAIService remote obtained:', localAIService)

// Implement the EmbeddingGemmaInterface Mojo observer
class EmbeddingGemmaInterfaceImpl {
  receiver: EmbeddingGemmaInterfaceReceiver

  constructor() {
    this.receiver = new EmbeddingGemmaInterfaceReceiver(this)
  }

  // Implementation of EmbeddingGemmaInterface::Embed
  async embed(input: string): Promise<{ output: number[] }> {
    console.log('[Candle WASM] Embed called (model not loaded yet):', input)
    // Model loading will be added in branch 3
    return { output: [] }
  }

  getPendingRemote() {
    return this.receiver.$.bindNewPipeAndPassRemote()
  }
}

// Create and register the EmbeddingGemmaInterface implementation
console.log(
  '[Candle WASM] Creating EmbeddingGemmaInterface ' + 'implementation...',
)
const embeddingGemmaImpl = new EmbeddingGemmaInterfaceImpl()
console.log(
  '[Candle WASM] Binding EmbeddingGemmaInterface ' + 'to LocalAIService...',
)
localAIService.bindEmbeddingGemma(embeddingGemmaImpl.getPendingRemote())

console.log('[Candle WASM] Embedding Gemma WASM bridge initialized!')
