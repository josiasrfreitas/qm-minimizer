# qm-minimizer

Implementação em C++ do algoritmo de **Quine–McCluskey** para minimização de funções booleanas, com suporte ao formato PLA do Espresso e foco em desempenho (multithread + operações em bitvetor de largura dinâmica).

## Status

🚧 **Em desenvolvimento ativo.** A arquitetura está documentada em `docs/adr/` (Architecture Decision Records). A implementação está sendo construída em partes lógicas — veja o histórico de commits.

## Objetivo

Receber uma função booleana descrita em PLA, encontrar uma **soma de produtos (SOP) minimizada** equivalente e emitir o resultado em PLA. A ferramenta:

1. Funciona em PLAs pequenos (2–6 variáveis) — verificáveis manualmente.
2. Escala em problemas grandes do benchmark **IWLS 2020 LSML Contest** (até 512 variáveis, 6400 termos), tratando o complemento como don't-care (`.type fr`).
3. Reporta tempos de execução por caso e estatísticas (número de primos gerados, selecionados, literais).

## Arquitetura

Camadas desacopladas:

```
Reader (PLA) → Function (IR comum) → Minimizer (QM) → Solution → Writer (PLA)
```

Cada camada é uma interface abstrata com uma implementação concreta. Para adicionar suporte a outro formato (BLIF, JSON, truth table), basta implementar um novo `Reader` ou `Writer` — o núcleo do algoritmo não é afetado.

Detalhamento e justificativas em `docs/adr/`.

## Estrutura do repositório

```
qm-minimizer/
├── README.md
├── CMakeLists.txt
├── docs/adr/                # Architecture Decision Records
├── include/qm/              # Headers públicos (interfaces e tipos)
├── src/                     # Implementações concretas
├── tests/                   # Testes unitários
└── data/
    ├── tests/               # PLAs pequenos para verificação manual
    ├── benchmark/           # IWLS 2020 — training set (10 casos)
    └── validation/          # IWLS 2020 — validation set (10 casos)
```

## Build

Requer apenas um compilador com suporte a **C++17** (testado com Apple Clang 17 e GCC 11+). Não tem dependências externas.

```bash
make            # build/qm em modo release (-O3 -march=native)
make test       # smoke tests nos PLAs pequenos
make bench      # roda o IWLS, gera tempos.csv
make clean
```

Alternativamente, build manual em uma linha:

```bash
clang++ -std=c++17 -O2 -Iinclude src/*.cc -pthread -o qm
```

Para CMake (opcional, mesmo resultado):

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j
```

## Uso

```bash
# Minimização básica (lê PLA, escreve PLA minimizado no stdout)
./build/qm data/tests/ex02_funcao4var.pla

# Salva em arquivo + estatísticas no stderr
./build/qm data/benchmark/ex00.train.pla -o ex00.min.pla --stats

# Roda em diretório inteiro, gera CSV de tempos
./build/qm --bench data/benchmark/ --csv tempos.csv

# Controla número de threads (default = hardware concurrency)
./build/qm data/benchmark/ex08.train.pla --threads 4
```

Flags disponíveis (resumo):

| Flag | Significado |
|---|---|
| `-o, --output <file>` | Arquivo de saída (default: stdout) |
| `--stats` | Imprime estatísticas em stderr |
| `--threads N` | Número de threads (default: hardware concurrency) |
| `--bench <dir>` | Modo benchmark sobre um diretório de PLAs |
| `--csv <file>` | Saída CSV para o modo `--bench` |

## Algoritmo (visão alta)

1. **Parse PLA** → conjunto de termos ON/OFF/DC, com metadados (`.i`, `.o`, `.type`).
2. **Fase A — geração de implicantes primos:**
   - Agrupar termos por peso de Hamming.
   - Fundir pares de grupos adjacentes que diferem em 1 bit (em paralelo).
   - Repetir até estabilizar.
3. **Fase B — cobertura mínima:**
   - Identificar implicantes primos **essenciais**.
   - Greedy: escolher iterativamente o primo que cobre mais mintermos descobertos.
4. **Emit PLA** minimizado.

A Fase A é paralelizada por par de grupos de Hamming. A Fase B é sequencial. Detalhes em `docs/adr/0004-threading-model.md` e `docs/adr/0005-cover-strategy.md`.

## Benchmark

O diretório `data/benchmark/` contém os 10 casos do IWLS 2020 LSML Contest (cada par `exNN.train.pla` / `exNN.valid.pla` representa a mesma função, com amostras distintas):

| Caso | `.i` (vars) | `.p` (termos) |
|---|:-:|:-:|
| ex00, ex01 | 32 | 6400 |
| ex02, ex03 | 64 | 6400 |
| ex04, ex05 | 128 | 6400 |
| ex06, ex07 | 256 | 6400 |
| ex08, ex09 | 512 | 6400 |

## Referências

- E. J. McCluskey. *Minimization of Boolean Functions.* Bell System Technical Journal, 1956.
- W. V. Quine. *The Problem of Simplifying Truth Functions.* American Mathematical Monthly, 1952.
- R. K. Brayton et al. *Logic Minimization Algorithms for VLSI Synthesis* (Espresso). Kluwer, 1984.
- [IWLS 2020 LSML Contest](https://github.com/iwls2020-lsml-contest/iwls2020-lsml-contest)
- [Espresso heuristic logic minimizer (`psksvp/espresso-ab-1.0`)](https://github.com/psksvp/espresso-ab-1.0)

## Licença

MIT. Veja `LICENSE`.
