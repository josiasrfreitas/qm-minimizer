# ADR 0005 — Don't-cares implícitos no IWLS não são materializados

- **Status:** Aceita
- **Data:** 2026-05-29

## Contexto

PLAs `.type fr` listam on_set e off_set; **tudo que não está listado é don't-care implícito**. Para os benchmarks IWLS:

| n | ON+OFF listados | DCs implícitos |
|---|---|---|
| 32 | 6400 | ~4.3 × 10⁹ |
| 64 | 6400 | ~1.8 × 10¹⁹ |
| 512 | 6400 | ~10¹⁵⁴ |

Materializar essa massa de DCs é impossível a partir de n ≥ 64.

## Decisão

A Fase A opera **só** sobre `on_set ∪ dc_set` explícito. DCs implícitos não são enumerados.

A invariante de segurança: termos fundidos a partir do on_set cobrem **só** inputs do on_set (cada fusão de tamanho 2 cobre os 2 inputs originais; por indução, fusão de 2^k cobre os 2^k originais). O cover gerado nunca viola o off_set.

## Consequências

- **Corretude preservada**: cobre on, evita off, sempre.
- **Compressão pode ser subótima**: primos não crescem usando DCs implícitos. Em funções esparsas o resultado tem tantos primos quanto minterms (ex.: `ex00.train.pla`: 3251 ON → 3251 primos no PR atual).
- **Comprimir esses casos exigiria algoritmo tipo Espresso** (`expand` aproveita DCs implícitos) — família diferente, fora do QM clássico.
