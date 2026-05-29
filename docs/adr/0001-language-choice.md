# ADR 0001 — Escolha de C++17 como linguagem

- **Data:** 2026-05-29

## Contexto

O minimizador precisa rodar benchmarks IWLS 2020 com 32 a 512 variáveis e 6400 termos cada, em arquivos `.type fr`. O algoritmo de Quine-McCluskey é dominado por duas operações: (i) comparar pares de termos para ver se diferem em exatamente uma variável e (ii) marcar quais minterms cada implicante primo cobre. Ambas são naturalmente expressas como operações bit a bit sobre vetores de inteiros, e ambas escalam mal se a constante de tempo por operação for alta.

Para n=512, um termo precisa de 1024 bits (`value` + `mask`) — 16 palavras de 64 bits. Comparar dois termos vira XOR + AND + popcount sobre esses 16 words. Em C++ isso compila para uma sequência curta de instruções com `popcnt` nativo. Em Python, cada operação dessas vira chamada para o runtime, com overhead de objetos `int` boxados; em Java, há boxing/unboxing e o JIT precisa aquecer. Para um benchmark com milhões de pares candidatos, esse overhead é ordens de grandeza.

Outro ponto é paralelismo: a Fase A do QM (ver ADR 0004) é embaraçosamente paralela. Python sofre com o GIL — threading só ajuda em I/O. Java e Rust resolvem isso bem, mas C++ é o que combina threading livre com o mesmo nível de controle de memória que C, sem o overhead de aprendizado do borrow checker num projeto deste porte.

## Decisão

Implementar o minimizador em **C++17**, compilado com `g++ -O3 -std=c++17 -pthread`. Usamos a STL (`std::vector`, `std::unordered_set`, `std::thread`, `std::async`) e evitamos dependências externas. A operação central fica:

```cpp
inline bool differs_by_one(const Term& a, const Term& b) {
    if (a.mask != b.mask) return false;            // mesmas posições de '-'
    uint64_t diff = 0;
    for (size_t i = 0; i < a.value.size(); ++i)
        diff += __builtin_popcountll(a.value[i] ^ b.value[i]);
    return diff == 1;
}
```

C++17 (não C++20) porque o ambiente de build alvo ainda inclui `g++ 9.x` em algumas máquinas; C++17 é o maior denominador comum estável.

## Alternativas consideradas

| Linguagem | Por que descartada |
|-----------|--------------------|
| **Python** | GIL impede paralelismo de CPU; bit ops em `int` arbitrariamente grandes são funcionais mas lentas (alocação por operação). Faria os benchmarks de n=512 inviáveis. |
| **C** | Performance equivalente, mas perderíamos `std::vector`, RAII, templates para `BitVec`. Mais código boilerplate para o mesmo resultado. |
| **Rust** | Excelente tecnicamente. Descartado pelo custo de produtividade: borrow checker em estruturas com paralelismo + compartilhamento de tabelas de implicantes consumiria tempo que precisamos para o algoritmo em si. |
| **Java** | JVM warm-up e GC pressure prejudicam medições. Bit ops em `long[]` são razoáveis, mas threading com pool ainda assim fica mais verboso que `std::async`. |

## Consequências

**Positivas:**
- Acesso direto a `__builtin_popcountll` (`POPCNT` no x86-64), crítico para a operação dominante.
- `std::thread`/`std::async` sem GIL nem dependência externa (ver ADR 0004).
- RAII gerencia memória dos `BitVec` sem `free` manual.
- Tempo de execução comparável a C, com código mais legível.

**Limitações:**
- Build precisa de toolchain C++17 (`g++ >= 9` ou `clang++ >= 10`); documentado no README.
- Segurança de memória é responsabilidade do desenvolvedor — testes unitários e `-fsanitize=address` em CI compensam parcialmente.

## Referências

- ISO/IEC 14882:2017 (C++17).
- Intel 64 and IA-32 Architectures SDM, vol. 2, instrução `POPCNT`.
- McCluskey, E. J. *Minimization of Boolean Functions*. Bell System Technical Journal, 1956.
