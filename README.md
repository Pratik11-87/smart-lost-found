# SMART LOST & FOUND — Team Alpha NEXUS

A campus lost-and-found project. The repository currently contains both the original Firebase/React application and a new **JavaScript-free C++ web prototype**.

## C++ version (no browser JavaScript)

The C++ web prototype lives in `cpp-web/`; its match-scoring engine is in `cpp/`. The browser receives server-rendered HTML and CSS, while the web routes and matching logic execute in C++17.

Requirements: C++17 compiler, CMake 3.16+, and internet access on first build to download cpp-httplib.

From the repository root:

```bash
cmake -S cpp-web -B build
cmake --build build --config Release
```

Run the generated `smart-lost-found-web` executable (on Windows with a Visual Studio generator, `build\Release\smart-lost-found-web.exe`) and open http://localhost:8080. Set the `PORT` environment variable to change the port.

**Migration status:** this is an early prototype, not yet a full replacement for the original app. Its reports are in-memory only and are lost when the server stops. Firebase Authentication, Firestore persistence, image uploads, user accounts, claims, and production deployment are not yet implemented in the C++ version. Do not deploy it publicly or use it for real personal data. The original app is retained until the C++ replacement is completed and tested.

HTML and CSS are used for browser presentation, not JavaScript. Firebase's standard browser SDK is JavaScript-based, so a full no-JavaScript migration needs C++ server-side integration with Firebase REST APIs and secure session handling.

## Original Firebase/React version (legacy)

The existing React/Vite version is still in the repository and remains the only version with the original Firebase-backed feature set.

### Features
- Email/password Firebase Authentication
- Lost and found item reporting
- Image upload to Firebase Storage
- Firestore-backed listings and claims
- Keyword/category/location/time matching with explainable percentages
- Proof-based claim submission
- Owner/claimant workflow and status tracking
- Responsive dashboard and search
- Firestore and Storage security rules
- Firebase Hosting configuration

### Setup for the legacy version

1. Install Node.js 20+.
2. Create a Firebase project in the Firebase Console.
3. Enable **Authentication → Email/Password**, **Firestore Database**, **Storage**, and **Hosting**.
4. Copy `.env.example` to `.env` and enter your Firebase web-app configuration.
5. Install dependencies and run locally:

```bash
npm install
npm run dev
```

6. Deploy rules and hosting with Firebase CLI:

```bash
npm install -g firebase-tools
firebase login
firebase use YOUR_PROJECT_ID
firebase deploy --only firestore:rules,storage
npm run build
firebase deploy --only hosting
```

## Data model (legacy version)

- `users/{uid}` stores profile information.
- `items/{itemId}` stores `type`, `title`, `description`, `category`, `location`, `date`, `imageUrl`, `reporterId`, `reporterName`, and workflow status.
- `claims/{claimId}` stores claimant/owner IDs, proof text, evidence URL, and claim status.

## Security

Never commit a Firebase service-account JSON file or private credentials. The browser Firebase configuration is supplied through Vite environment variables in the legacy version; Firestore and Storage rules provide authorization.
