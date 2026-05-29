# Architecture Decision Records — qm-minimizer

Decisões que afetam comportamento ou performance do minimizador. Cada ADR é um documento curto registrando o problema que motivou, o que foi decidido, e o que isso implica. Decisões aceitas não são editadas — quando algo muda, cria-se uma nova ADR substituindo a anterior.

## Índice

| # | Decisão | Impacto |
|---|---------|---------|
| [0001](0001-architecture-layers.md) | Camadas Reader → IR → Minimizer → Writer | Troca de formato/algoritmo isolada |
| [0002](0002-term-encoding.md) | Termo como `value + mask` em `BitVec` | Fusão = XOR + AND + popcount nativos |
| [0003](0003-threading-model.md) | Fase A paralela por par de grupos de Hamming | Speedup sem locks no hot path |
| [0004](0004-cover-strategy.md) | Cobertura: essenciais + guloso | Termina rápido, pode ser subótimo em núcleos cíclicos |
| [0005](0005-implicit-dont-cares.md) | DCs implícitos no IWLS não são materializados | Sempre correto; comprime pouco em funções esparsas |

## Convenções

- Notação booleana: `!A` = NOT, `+` = OR, `·` ou justaposição = AND.
- Status possíveis: **Proposta**, **Aceita**, **Substituída por NNNN**, **Obsoleta**.
