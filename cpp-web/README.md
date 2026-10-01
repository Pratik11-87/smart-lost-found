# Smart Lost & Found — C++ web prototype

This version has **no browser JavaScript**. The browser receives HTML and CSS; request handling and match scoring run in C++17.

## Included
- Server-rendered landing page and report board
- HTML form to report lost or found items
- C++ matching suggestions for found reports against lost reports
- HTML escaping for submitted report text
- Responsive CSS

## Build and run
Requirements: C++17 compiler, CMake 3.16+, and internet access on first configure to fetch the pinned cpp-httplib dependency.

From the repository root:

```bash
cmake -S cpp-web -B build
cmake --build build --config Release
```

Run the executable:
- Windows Visual Studio generator: `build\Release\smart-lost-found-web.exe`
- Windows single-configuration generator: `build\smart-lost-found-web.exe`
- macOS/Linux: `./build/smart-lost-found-web`

Open http://localhost:8080. Set the PORT environment variable to use another port.

## Important limitations
This is a **prototype**, not yet a complete replacement for the original app. Reports are held in memory and disappear when the server stops. Firebase Authentication, Firestore persistence, photo uploads, user accounts, claims, and deployment integration are not implemented in this version. Do not deploy this prototype publicly or use it for real personal data.

The original React/Firebase application is retained in the repository until the C++ version is fully migrated and tested. HTML and CSS remain as browser presentation formats; application logic is in C++.

Firebase's standard browser SDK is JavaScript. A JavaScript-free version needs the C++ server to call Firebase REST APIs with secure authentication/session handling, Firestore mapping, Storage support, and deployment changes. Those pieces must be implemented and tested before this prototype can replace the existing production app.
