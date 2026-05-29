# ADR 0002 — Termo como `value + mask` em `BitVec`

- **Status:** Aceita
- **Data:** 2026-05-29

## Contexto

Cada termo tem n posições em um de três estados: `0`, `1`, `-`. A pergunta-chave do QM — *"esses dois termos diferem em exatamente um bit?"* — é feita dezenas de milhões de vezes por execução. Custo unitário dela = custo total.

## Decisão

Dois `BitVec` por termo (vetores de `uint64_t` de largura `ceil(n/64)`):

```cpp
struct Term {
    BitVec value;  // 0 ou 1 onde mask=1
    BitVec mask;   // 1 onde a variável é literal, 0 onde é don't-care
};
```

A fusão vira: `t1.mask == t2.mask && popcount((t1.value ^ t2.value) & t1.mask) == 1` — tudo XOR + AND + popcount sobre `uint64_t`.

## Consequências

- Para n=512, cada termo ocupa 128 B; 6400 termos cabem em cache L1.
- Strings de `'0'/'1'/'-'` seriam ~10x mais lentas (sem popcount nativo). Trits empacotados (2 bits/var) economizam memória mas exigem máscara por par e somam instruções.
- Cada fusão precisa restaurar o invariante `value & ~mask == 0` (um AND extra).
