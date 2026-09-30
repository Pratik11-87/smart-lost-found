# SMART LOST & FOUND

A Firebase-backed campus lost-and-found web application. Users can report lost/found items, attach evidence, search listings, see explainable match scores, submit proof-based claims, and update the handover status.

## Features

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

## Setup

1. Install Node.js 20+.
2. Create a Firebase project at Firebase Console.
3. Enable **Authentication → Email/Password**, **Firestore Database**, **Storage**, and **Hosting**.
4. Copy `.env.example` to `.env` and enter your Firebase web-app configuration.
5. Install dependencies:

```bash
npm install
```

6. Run locally:

```bash
npm run dev
```

7. Deploy rules and hosting with Firebase CLI:

```bash
npm install -g firebase-tools
firebase login
firebase use YOUR_PROJECT_ID
firebase deploy --only firestore:rules,storage
npm run build
firebase deploy --only hosting
```

## Data model

`users/{uid}` stores profile information.

`items/{itemId}` stores `type`, `title`, `description`, `category`, `location`, `date`, `imageUrl`, `reporterId`, `reporterName`, and workflow status.

`claims/{claimId}` stores claimant/owner IDs, proof text, evidence URL, and claim status.

## Security

Never put a Firebase service-account JSON file or private credentials in the repository. The browser Firebase configuration is intentionally supplied through Vite environment variables; Firestore and Storage rules provide authorization.
