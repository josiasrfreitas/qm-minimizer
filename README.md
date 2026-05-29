# qm-minimizer

Implementação em C++ do algoritmo de **Quine–McCluskey** para minimização de funções booleanas, com suporte ao formato PLA do Espresso e foco em desempenho (multithread + operações em bitvetor).

> Projeto da disciplina **Práticas em Sistemas Digitais (Lab 06)** — Departamento de Computação, Universidade Federal de Sergipe (UFS).

## Status

🚧 **Em planejamento.** A arquitetura está sendo desenhada no estilo RFC antes da implementação. Veja `docs/` (a ser criado) para as decisões de arquitetura.

## Objetivo

Receber uma função booleana descrita em PLA, encontrar uma **soma de produtos (SOP) minimizada** equivalente e emitir o resultado em PLA. A ferramenta deve:

1. **Funcionar** nos PLAs pequenos de `data/tests/` (2–6 variáveis) — verificáveis por K-map.
2. **Escalar** nos benchmarks do IWLS 2020 em `data/benchmark/` (até 512 variáveis, 6400 termos), tratando o complemento como don't-care (`.type fr`).
3. **Reportar tempos de execução** por caso, atendendo à tarefa de casa do laboratório.

## Estrutura do repositório

```
qm-minimizer/
├── README.md                # este arquivo
├── data/
│   ├── tests/               # PLAs pequenos para verificação manual (K-map)
│   ├── benchmark/           # IWLS 2020 — training set (10 casos)
│   └── validation/          # IWLS 2020 — validation set (10 casos)
├── docs/                    # RFCs e ADRs (a criar)
├── src/                     # código-fonte C++ (a criar)
├── include/                 # headers (a criar)
└── tests/                   # testes unitários (a criar)
```

## Algoritmo (visão alta)

1. **Parse PLA** → conjunto de termos ON e OFF, com metadados (`.i`, `.o`, `.type`).
2. **Fase A — geração de implicantes primos:**
   - Agrupar termos por peso de Hamming.
   - Fundir pares de grupos adjacentes que diferem em 1 bit.
   - Repetir até estabilizar.
3. **Fase B — cobertura mínima:**
   - Identificar implicantes primos **essenciais** (única cobertura para algum mintermo).
   - Reduzir tabela via dominância de linhas/colunas.
   - Resolver o restante por heurística ou branch-and-bound.
4. **Emit PLA** minimizado.

## Build & uso

> Em desenvolvimento. A interface CLI será detalhada na RFC de arquitetura.

Skeleton previsto:

```bash
qm <input.pla> [-o output.pla] [--stats] [--threads N]
qm --bench data/benchmark/ --csv tempos.csv
```

## Referências

- Slides da disciplina e bibliografia indicada no enunciado do Lab 06.
- [IWLS 2020 LSML Contest](https://github.com/iwls2020-lsml-contest/iwls2020-lsml-contest) — origem dos benchmarks.
- [Espresso / `psksvp/espresso-ab-1.0`](https://github.com/psksvp/espresso-ab-1.0) — implementação clássica de referência.

## Licença

A definir.
