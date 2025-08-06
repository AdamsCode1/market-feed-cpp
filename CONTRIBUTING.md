# Contributing to Market Feed

Thank you for your interest in contributing to this high-performance market data feed handler! This project serves as a demonstration of production-level C++ and financial technology skills.

## Code Style

This project follows strict coding standards to maintain professional quality:

### C++ Guidelines
- **Standard**: C++20 with modern features
- **Formatting**: 4-space indentation, Google style with clang-format
- **Naming**: 
  - Variables: `snake_case`
  - Functions: `snake_case`
  - Classes: `PascalCase`
  - Constants: `UPPER_CASE`
  - Private members: trailing underscore `member_`

### Performance Requirements
- **Zero-allocation** in hot paths
- **Cache-friendly** data structures
- **RAII** for all resource management
- **Move semantics** for large objects
- **constexpr** where applicable

## Testing Standards

All contributions must include:
- **Unit tests** for new functionality
- **Integration tests** for system-level changes
- **Benchmarks** for performance-critical code
- **Memory safety** verification

## Performance Benchmarks

Changes affecting performance must maintain:
- **Throughput**: ≥4M messages/second
- **Latency**: p99 <20µs for core operations
- **Memory**: No leaks or excessive allocation

## Build Requirements

- C++20 compatible compiler (GCC 11+, Clang 13+)
- CMake 3.22+
- Google Test & Benchmark frameworks
- Ninja build system (recommended)

## Submission Process

1. **Fork** the repository
2. **Create** feature branch: `git checkout -b feature/description`
3. **Implement** with tests and documentation
4. **Verify** all tests pass: `make test`
5. **Benchmark** performance impact: `make bench`
6. **Submit** pull request with detailed description

## Questions?

This project demonstrates professional-level development practices. For questions about implementation details or career opportunities, feel free to reach out!
