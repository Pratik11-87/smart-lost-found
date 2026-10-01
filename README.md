# SMART LOST & FOUND

A campus lost-and-found web application built with C++17, SQLite, HTML, and CSS.

## Features

- Account registration and sign-in with salted PBKDF2-HMAC-SHA256 password hashing
- Persistent storage for users, reports, sessions, and claims using SQLite
- Report lost or found items with JPEG, PNG, or WebP photos (maximum 5 MB)
- Browse reports and compare possible matches using an explainable C++ score
- Submit claims with an explanation
- Report owners can approve or reject submitted claims
- Secure random session tokens, HttpOnly cookies, input validation, image type checks, and HTML escaping

## Requirements

- C++17 compiler (GCC, Clang, or MSVC)
- CMake 3.16+
- SQLite3 development library
- OpenSSL development library
- Internet connection on the first build for cpp-httplib

## Build

```bash
cmake -S . -B build
cmake --build build --config Release
```

Run the generated executable:
- Windows Visual Studio: `build\Release\smart-lost-found.exe`
- Windows single-configuration generator: `build\smart-lost-found.exe`
- macOS/Linux: `./build/smart-lost-found`

Open http://localhost:8080. Set the `PORT` environment variable to choose a different port.

## Storage

The application creates `smart_lost_found.db` and an `uploads/` directory in its working directory. Keep both when backing up or moving the installation. Do not manually expose the database or upload directory as a public static directory.

## Accounts and claims

Create an account, sign in, and submit a lost or found report. Signed-in users can submit a claim on another user's found report. The report owner can review claims from the Claims page and approve or reject each pending claim.

## Production deployment

Use HTTPS and set `COOKIE_SECURE=1` when deployed behind HTTPS. Keep the database and uploads on persistent storage, restrict filesystem permissions, and back up both. For a public deployment, configure HTTPS at the reverse proxy, rate limiting, email verification, password reset, abuse reporting, and operational backups before accepting real users. The current implementation is a functional project baseline, not a security-audited production service.
