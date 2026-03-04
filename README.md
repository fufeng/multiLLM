## Agent-based simulation code for hybrid human-AI systems


## Build

You need a C++17 compiler (e.g., `g++` or `clang++`).

```bash
g++ -O3 -std=c++17 -march=native mutation_rrg_alpha.cpp -o mutation_rrg_alpha
```

If you run into portability issues, drop `-march=native`.

## Run

```bash
./mutation_rrg_alpha [options]
```

### Common example

```bash
./mutation_rrg_alpha \
  --Ne 400 --k 4 \
  --pe 0.20 \
  --beta 0.005 \
  --mu 1e-4 \
  --burnin 1000000 \
  --sample 5000000 \
  --u 0.4 \
  --alphaA 1.0 --alphaB 0.0 \
  --nReal 10 \
  --seed 12345
```


## Citation

If you use this code in academic work, please cite:

- F. Fu, X. Chen, N. Christakis,
  *On the optimal integration of intelligent agents into network systems to steer cooperation*,
  **PNAS (2026) in press**.
  DOI: `10.1073/pnas.2537939123`
## Contact

For questions or bug reports, please open an issue or contact the authors.
