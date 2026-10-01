# Smart Lost & Found

A community lost-and-found web app built with HTML, CSS, vanilla JavaScript and Firebase.

## Features

- Email/password registration, login and logout with Firebase Authentication.
- Persistent lost and found reports stored in Cloud Firestore.
- Optional JPEG, PNG or WebP photos stored in Firebase Storage (maximum 5 MB).
- Search by item details, category, report type and location.
- Match suggestions that combine keyword overlap (55%), location similarity (30%) and time proximity (15%).
- Claim requests with a verification note and owner approval/rejection.
- Responsive interface for desktop and mobile.
- Firebase Hosting deployment.

## Project files

- `index.html` — app structure and page sections.
- `styles.css` — responsive visual design.
- `app.js` — Firebase Authentication, Firestore, Storage, filters and matching logic.
- `firebase-config.example.js` — template for your Firebase web app configuration.
- `firestore.rules` — database access rules.
- `storage.rules` — image upload access rules.
- `firebase.json` — Hosting and rules deployment configuration.

## 1. Create and configure Firebase

1. Open the [Firebase Console](https://console.firebase.google.com/) and create a project.
2. Add a Web App in Project settings and copy its configuration.
3. Enable **Authentication → Sign-in method → Email/Password**.
4. Create a **Cloud Firestore** database.
5. Enable **Storage** and create its default bucket.
6. Copy `firebase-config.example.js` to `firebase-config.js` and replace the placeholder values with your Firebase web app config.
7. Make sure `firebase-config.js` stays untracked by Git. The web config identifies the Firebase project; Firestore and Storage Security Rules are what enforce access control.

## 2. Run locally

Install Node.js and the Firebase CLI:

```bash
npm install -g firebase-tools
firebase login
firebase use --add
```

For a quick local preview, use any static web server from the project folder, such as the VS Code Live Server extension. Open the local URL it provides; do not open `index.html` directly as a `file://` URL because browser module imports may be blocked.

## 3. Deploy

From the repository root:

```bash
firebase login
firebase use --add
firebase deploy --only firestore:rules,storage
firebase deploy --only hosting
```

The first `firebase use --add` associates this checkout with your Firebase project. Choose the same project that matches the configuration in `firebase-config.js`. If the CLI says the Storage bucket is not created, create it in the Firebase Console and retry.

Firebase Hosting serves the static frontend. Authentication, database records and photos are handled by Firebase services; no always-running server is required.

## Matching approach

Each report is compared with reports of the opposite type. A score from 0–100 combines:
- **Keywords (55%)** — shared words across item name, category, description and location.
- **Location (30%)** — matching or overlapping location text.
- **Time (15%)** — reports closer together in time receive a higher score, with the time contribution tapering over 14 days.

Suggestions with a score of at least 18% are shown for reports that belong to other users. This is a lightweight heuristic, not proof that two reports describe the same item. Verify ownership before handing an item over.

## Security notes

- Publish the included Firestore and Storage rules before accepting real reports.
- Keep user contact details out of public report documents. The public report board does not display email addresses.
- Claim requests are readable by the claimant and the report owner; only the report owner can approve or reject a pending claim.
- Photo writes are restricted to a user's own Storage folder and image types/sizes are limited by rules.
- For a real deployment, review Firebase Authentication settings, usage limits, abuse prevention and the current Firebase Storage billing/plan requirements.

## Technology

HTML · CSS · JavaScript modules · Firebase Authentication · Cloud Firestore · Firebase Storage · Firebase Hosting
