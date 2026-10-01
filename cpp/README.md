# C++ Matching Engine

This folder contains the lost-and-found similarity scoring logic in standard C++17. The engine compares item category, title and description keywords, location, and date. Its result is capped at 100 points and includes explanations for the factors that contributed to the score.

## Build with a C++ compiler

From the repository root:

```bash
g++ -std=c++17 -Wall -Wextra -pedantic cpp/matcher.cpp cpp/main.cpp -o slf-matcher
```

Run the program:

- Windows: `.\slf-matcher.exe`
- macOS/Linux: `./slf-matcher`

## Build with CMake

```bash
cmake -S cpp -B matcher-build
cmake --build matcher-build --config Release
```

## Scoring factors

| Signal | Maximum points |
|---|---:|
| Same category | 30 |
| Shared title/description keywords | 35 |
| Similar location | 20 |
| Nearby date | 15 |
| **Maximum** | **100** |

Match percentages are heuristic suggestions only, not proof of ownership. Verify identifying details before returning property.
