# ADR 0004 — Cobertura: essenciais + guloso, sem Branch-and-Bound

- **Status:** Aceita
- **Data:** 2026-05-29

## Contexto

Escolher o menor conjunto de primos que cobre todo o on_set é Set Cover — NP-difícil. Existe um espectro: greedy puro (rápido, subótimo), B&B (exato, exponencial no pior caso), Petrick (exato, explode em memória).

## Decisão

1. **Essenciais**: para cada minterm coberto por exatamente 1 primo, esse primo entra na solução.
2. **Guloso** no resto: enquanto houver minterm descoberto, pega o primo que cobre mais minterms ainda descobertos.

## Consequências

- Termina em O(P × M); sempre cabe no orçamento de tempo.
- Pode emitir solução subótima em **núcleos cíclicos** (estrutura simétrica onde nenhum primo é essencial) — comum em funções de paridade.
- B&B é encaixável como uma `MinimizerOptions::CoverStrategy` adicional sem mexer no pipeline. Trabalho futuro.
