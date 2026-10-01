# SMART LOST & FOUND

A campus lost-and-found application built with C++17, HTML, and CSS. Students can submit lost or found item reports and review possible matches ranked by a transparent scoring system.

## Features

- Create lost-item and found-item reports
- Browse all reports on a campus noticeboard
- Rank possible matches by category, keywords, location, and date
- See the reasons behind each match score
- Responsive interface for desktop and mobile screens
- Server-side input validation and HTML escaping

## Requirements

- C++17 compiler (GCC, Clang, or MSVC)
- CMake 3.16 or later
- Internet connection on the first build to download the cpp-httplib dependency

## Build

Run these commands from the repository root:

```bash
cmake -S . -B build
cmake --build build --config Release
```

Run the executable:
- Windows with Visual Studio: `build\Release\smart-lost-found.exe`
- Windows with a single-configuration generator: `build\smart-lost-found.exe`
- macOS/Linux: `./build/smart-lost-found`

Open http://localhost:8080 in your browser. To use a different port, set the `PORT` environment variable before launching the program.

## Project structure

```text
cpp/
  matcher.hpp       Matching data structures and public API
  matcher.cpp       C++ matching score implementation
cpp-web/
  server.cpp        HTTP routes, forms, and server-rendered pages
CMakeLists.txt      Build configuration
README.md           Project overview and setup
```

## Matching score

Possible matches receive up to 100 points:
- Same category: 30 points
- Shared keywords in title and description: up to 35 points
- Similar location: 20 points
- Dates within one day: 15 points; within seven days: 8 points

Scores help prioritize review; they do not establish ownership. Verify an item before returning it.

## Current data storage

This starter build stores reports in the running server's memory. Reports are cleared when the server stops. Persistent database storage, user accounts, image uploads, and claim workflows can be added as later project modules.

## Safety

Do not publish sensitive personal information in item descriptions. The current build is intended for local demonstration and should not be exposed to the public internet without persistent storage, authentication, request protection, and deployment hardening.
