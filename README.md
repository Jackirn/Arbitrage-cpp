# Arbitrage-cpp

A modular C++ implementation of an **energy futures statistical arbitrage pipeline**.  
The project replicates and extends a MATLAB workflow, providing efficient execution with C++ (Boost, NLopt, CMake).  

Core features:
- Data loading, trimming, and outlier filtering  
- Ornstein–Uhlenbeck (OU) model calibration and bootstrap  
- Transaction cost estimation  
- Optimal trading bands computation with stop-loss & leverage sweep  
- Results exported to CSV for further analysis  

---

## Project Structure

The repository is organized into modular components:

### `include/utilities/`
Header-only utilities used across the project:
- **DataOrdering.hpp** – Functions for data trimming and splitting  
- **Loaders.hpp** – CSV loader and preprocessing  
- **StatisticalBootstrap.hpp** – OU model bootstrap estimation  
- **OptimalBands.hpp** – Optimal trading bands computation  

---

### `src/`
C++ source code implementations:
- **main.cpp** – Main pipeline entry point  
- **DataOrdering.cpp** – Implementation of data ordering utilities  
- **Loaders.cpp** – CSV loader implementation  
- **StatisticalBootstrap.cpp** – OU model estimation & bootstrap logic  
- **OptimalBands.cpp** – Optimal bands optimization (NLopt + Boost)  

---

### Root files
- **CMakeLists.txt** – Build configuration (CMake)  
- **HO-LGO.csv** – Example dataset (*not included in repo*)  

---

### `outputs/`
Generated results and logs after running the pipeline.  

---

## Dependencies

This project requires:

- **C++17 or later**  
- **CMake ≥ 3.15**  
- [Boost](https://www.boost.org/) – Math & statistics utilities  
- [NLopt](https://nlopt.readthedocs.io/) – Nonlinear optimization library  

On macOS (Homebrew):  
```bash
brew install boost nlopt cmake
