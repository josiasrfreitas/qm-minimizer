# ADR 0003 — Codificação de termos como `value` + `mask`

- **Status:** Aceita
- **Data:** 2026-05-29

## Contexto

Um termo do QM é uma conjunção parcial: cada uma das n variáveis está em um de três estados — fixada em `0`, fixada em `1`, ou marcada como don't-care `-` (variável já foi absorvida em fusões anteriores). Por exemplo, para n=4 o termo `10-1` representa `A · !B · D` (variável C foi eliminada).

A operação central do QM é: dados dois termos do mesmo "grupo" (mesma quantidade de bits `1`), eles são fundíveis se **diferem em exatamente uma posição** e **têm os mesmos don't-cares**. Essa pergunta é feita milhões de vezes ao longo da Fase A — para n=512 e 6400 termos, são potencialmente ~20 milhões de comparações por iteração. O custo unitário dessa comparação determina o tempo total.

Uma escolha de representação ruim aqui penaliza tudo. Strings de `'0' '1' '-'` exigem um loop com branch por caractere. Arrays de "trits" (2 bits por variável) economizam memória mas ainda forçam loop com máscaras. A codificação **value + mask** explora o fato de que três estados cabem em dois bits paralelos por posição: `mask=1` significa "variável presente", `mask=0` significa "don't-care"; quando `mask=1`, `value` diz se é `0` ou `1`.

Com essa codificação, "diferem em exatamente um bit" vira: `mask_a == mask_b` E `popcount(value_a XOR value_b) == 1`. Tudo em palavras de 64 bits.

## Decisão

Cada `Term` carrega dois `BitVec` (vetores de `uint64_t`) de comprimento `ceil(n/64)`:

```cpp
using BitVec = std::vector<uint64_t>;

struct Term {
    BitVec value;   // bit i = valor da var i quando mask[i]==1
    BitVec mask;    // bit i = 1 se var i é literal; 0 se é '-'
};

inline bool fundivel(const Term& a, const Term& b) {
    if (a.mask != b.mask) return false;
    uint64_t diff = 0;
    for (size_t i = 0; i < a.value.size(); ++i)
        diff += __builtin_popcountll(a.value[i] ^ b.value[i]);
    return diff == 1;
}
```

A fusão é igualmente direta: zerar no `mask` o bit onde diferem.

Para n=512: cada `BitVec` tem 512/64 = 8 palavras = 64 bytes. Cada `Term` ocupa **128 bytes de dados úteis** (mais o overhead de `std::vector` ~24 bytes × 2 = 48 bytes). Para 6400 termos: ~6400 × 176 ≈ **1,1 MB** — cabe em L2 confortavelmente, o que ajuda muito a comparação em lote.

## Alternativas consideradas

| Codificação | Bytes/var | Bytes/Term (n=512) | Custo do `fundivel` | Veredito |
|-------------|-----------|--------------------|---------------------|----------|
| **value + mask (escolhida)** | 2 bits | 128 B | 16 ops XOR/AND + popcount | Vencedora |
| Array de trits (2 bits/var, formato custom) | 2 bits | 128 B | Loop com máscara dupla por word, ~3× mais ops | Mesmo espaço, mais lento |
| String `std::string` de `'0' '1' '-'` | 8 bits | 512 B + 24 B overhead | Loop char a char com branches | 4× espaço, 10×+ tempo |
| Dois `std::bitset<N>` | 2 bits | 128 B | Igual à escolhida, mas N fixo em compilação | Não cabe pois n varia entre PLAs |
| `std::vector<int>` com {0,1,2} | 32 bits | 2048 B | Loop com comparações | Inviável em memória e tempo |

## Consequências

**Positivas:**
- Operação dominante do QM compila para sequência curta de XOR/AND/`popcnt` — instruções de 1 ciclo no x86-64 moderno.
- Memória total para o maior benchmark (n=512, 6400 termos) cabe em ~1 MB — bom para caches.
- `std::vector<uint64_t>` se adapta a n variável (diferente de `std::bitset<N>`).
- Igualdade de `mask` vira `memcmp` de 64 bytes (vetorizado pelo compilador).

**Limitações:**
- Depuração visual exige uma função `to_string(Term, n)` que reconstrói `'0' '1' '-'` — escrita uma vez em `debug.cpp`.
- Erro fácil de cometer: setar `value` num bit onde `mask=0`. Mitigado por uma invariante `assert((value & ~mask) == 0)` em modo debug.
- A operação `fundivel` percorre o vetor inteiro mesmo quando a diferença está no primeiro word; early-exit foi medido e não compensou pelo branch.

## Referências

- McCluskey, E. J. *Minimization of Boolean Functions*. Bell System Technical Journal, 1956 — descrição original do algoritmo e da tabulação por grupos.
- Hennessy & Patterson, *Computer Architecture: A Quantitative Approach* — capítulo sobre instrução `POPCNT` e custo de operações bit a bit.
- ADR 0001 (uso de `__builtin_popcountll`), ADR 0004 (paralelismo sobre pares de grupos).
