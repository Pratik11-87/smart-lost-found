# C++ Web Application

This folder contains the server-rendered web application for SMART LOST & FOUND. The interface is delivered as HTML and CSS; application logic and request handling run in C++17.

## Features

- Campus landing page and report board
- Form for reporting lost and found items
- C++ matching suggestions for found reports against lost reports
- Explanation of matching factors
- HTML escaping for submitted report content
- Responsive layout

## Requirements

- C++17 compiler
- CMake 3.16 or newer
- Internet access on the first configure to download the pinned cpp-httplib dependency

## Build

Run from the repository root:

```bash
cmake -S cpp-web -B build
cmake --build build --config Release
```

Run the generated executable:

- Windows Visual Studio generator: `build\Release\smart-lost-found-web.exe`
- Windows single-configuration generator: `build\smart-lost-found-web.exe`
- macOS/Linux: `./build/smart-lost-found-web`

Open http://localhost:8080. Set the `PORT` environment variable to use a different port.

## Limitations

This is a demonstration prototype. Reports are held in memory and disappear when the server stops. Accounts, persistent database storage, image uploads, claims, and production deployment are not implemented. Do not deploy publicly or use the prototype for real personal data until these features and appropriate security controls have been implemented and tested.
