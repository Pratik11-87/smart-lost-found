# C++ matching engine

This folder contains the lost/found similarity scoring logic in standard C++17. It mirrors the website's current explainable score factors: category (30 points), overlapping title/description keywords (up to 35), similar location (20), and nearby date (up to 15). The result is capped at 100%.

## Build with a C++ compiler

From the repository root:

```bash
g++ -std=c++17 -Wall -Wextra -pedantic cpp/matcher.cpp cpp/main.cpp -o slf-matcher
```

Run it:

- Windows: `.\slf-matcher.exe`
- macOS/Linux: `./slf-matcher`

Alternatively, use CMake:

```bash
cmake -S cpp -B build
cmake --build build
```

## Important architecture note

The existing website is a browser-based React/Vite app and uses the official Firebase JavaScript SDK for browser authentication, Firestore, and Storage. A browser cannot directly execute ordinary native C++ source. Therefore this C++ module is a standalone C++ implementation of the matching core; the existing web interface and Firebase integration remain JavaScript so the current website continues to work.

To run matching in the browser with C++, a later build step can compile the matcher to WebAssembly with Emscripten and expose it through a small JavaScript bridge. Replacing the whole UI and Firebase integration with C++ would require a separate server/API architecture and would not be a simple language substitution.

Match percentages are heuristic suggestions only, not proof of ownership. Always verify identifying details before handing over property.
