import { initializeApp } from 'https://www.gstatic.com/firebasejs/11.6.0/firebase-app.js';
import { getAuth, onAuthStateChanged, createUserWithEmailAndPassword, signInWithEmailAndPassword, signOut, updateProfile } from 'https://www.gstatic.com/firebasejs/11.6.0/firebase-auth.js';
import { getFirestore, collection, addDoc, doc, updateDoc, query, orderBy, onSnapshot, serverTimestamp, Timestamp, where } from 'https://www.gstatic.com/firebasejs/11.6.0/firebase-firestore.js';
import { getStorage, ref, uploadBytes, getDownloadURL } from 'https://www.gstatic.com/firebasejs/11.6.0/firebase-storage.js';
import { firebaseConfig } from './firebase-config.js';

const app = initializeApp(firebaseConfig);
const auth = getAuth(app);
const db = getFirestore(app);
const storage = getStorage(app);
const $ = (id) => document.getElementById(id);
let currentUser = null;
let reports = [];
let claims = [];
let authMode = 'login';
let unsubscribeReports = null;
let unsubscribeClaims = null;
let unsubscribeOwnerClaims = null;
let toastTimer = null;

$('year').textContent = new Date().getFullYear();
$('item-time').value = new Date(Date.now() - new Date().getTimezoneOffset() * 60000).toISOString().slice(0, 16);

function showToast(message) {
  $('toast').textContent = message;
  $('toast').classList.remove('hidden');
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => $('toast').classList.add('hidden'), 3600);
}
function setMessage(id, message, success) {
  const el = $(id);
  el.textContent = message || '';
  el.style.color = success ? '#36744a' : '';
}
function formatDate(value) {
  const date = value && typeof value.toDate === 'function' ? value.toDate() : value instanceof Date ? value : value ? new Date(value) : null;
  if (!date || Number.isNaN(date.getTime())) return 'Time not specified';
  return date.toLocaleString([], { dateStyle: 'medium', timeStyle: 'short' });
}
function escapeHtml(value) {
  return String(value == null ? '' : value).replace(/[&<>"']/g, (ch) => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[ch]));
}
function reportTime(report) {
  if (report.occurredAt && typeof report.occurredAt.toDate === 'function') return report.occurredAt.toDate();
  if (report.occurredAt) return new Date(report.occurredAt);
  return new Date(0);
}
function searchableText(report) {
  return [report.title, report.category, report.description, report.location].join(' ').toLowerCase();
}
function keywordSet(report) {
  const stop = new Set(['the','and','with','for','from','that','this','was','were','have','has','its','near','lost','found','item','into','your','you','at','in','on','a','an','of','to','is','my']);
  return searchableText(report).split(/[^a-z0-9]+/).filter((word) => word.length > 2 && !stop.has(word));
}
function scoreMatch(a, b) {
  if (!a || !b || a.id === b.id || a.type === b.type) return 0;
  const left = new Set(keywordSet(a));
  const right = new Set(keywordSet(b));
  let shared = 0;
  left.forEach((word) => { if (right.has(word)) shared += 1; });
  const keywordScore = shared / Math.max(1, Math.min(left.size, right.size));
  const locA = String(a.location || '').toLowerCase().replace(/[^a-z0-9 ]/g, '').trim();
  const locB = String(b.location || '').toLowerCase().replace(/[^a-z0-9 ]/g, '').trim();
  let locationScore = 0;
  if (locA && locB && (locA.includes(locB) || locB.includes(locA))) locationScore = 1;
  else {
    const wordsA = new Set(locA.split(/\s+/).filter(Boolean));
    const wordsB = new Set(locB.split(/\s+/).filter(Boolean));
    wordsA.forEach((word) => { if (wordsB.has(word)) locationScore = Math.max(locationScore, 0.6); });
  }
  const timeDiffHours = Math.abs(reportTime(a).getTime() - reportTime(b).getTime()) / 3600000;
  const timeScore = Math.max(0, 1 - timeDiffHours / (24 * 14));
  const score = (keywordScore * 0.55) + (locationScore * 0.30) + (timeScore * 0.15);
  return Math.round(score * 100);
}
function reportCard(report, matchScore) {
  const isLost = report.type === 'lost';
  const img = report.photoURL ? '<img src="' + escapeHtml(report.photoURL) + '" alt="Photo of ' + escapeHtml(report.title) + '" loading="lazy">' : '<div class="image-placeholder" aria-hidden="true">' + (isLost ? '⌕' : '✳') + '</div>';
  const score = typeof matchScore === 'number' ? '<span class="match-score">' + matchScore + '% match</span>' : '';
  const canClaim = currentUser && !isLost && report.ownerId !== currentUser.uid;
  const ownerText = currentUser && report.ownerId === currentUser.uid ? '<span>♧ Your report</span>' : '';
  const claimButton = canClaim ? '<button class="button button-primary claim-item" data-id="' + escapeHtml(report.id) + '">Claim item</button>' : '';
  return '<article class="report-card"><div class="report-image">' + img + '<span class="status-pill ' + (isLost ? 'status-lost' : 'status-found') + '">' + (isLost ? 'Lost item' : 'Found item') + '</span></div><div class="report-content"><span class="category-label">' + escapeHtml(report.category || 'Other') + '</span><h3>' + escapeHtml(report.title) + '</h3><p class="report-description">' + escapeHtml(report.description || '') + '</p>' + score + '<div class="report-meta"><span>⌖ ' + escapeHtml(report.location || 'Location not given') + '</span><span>◷ ' + escapeHtml(formatDate(report.occurredAt)) + '</span>' + ownerText + '</div>' + (claimButton ? '<div class="card-actions">' + claimButton + '</div>' : '') + '</div></article>';
}
function renderReports() {
  const search = $('search-input').value.trim().toLowerCase();
  const type = $('type-filter').value;
  const location = $('location-filter').value.trim().toLowerCase();
  const sort = $('sort-filter').value;
  let list = reports.filter((item) => {
    const typeOkay = type === 'all' || item.type === type;
    const textOkay = !search || searchableText(item).includes(search);
    const locationOkay = !location || String(item.location || '').toLowerCase().includes(location);
    return typeOkay && textOkay && locationOkay;
  });
  if (sort === 'oldest') list.sort((a, b) => reportTime(a) - reportTime(b));
  else if (sort === 'matches' && currentUser) {
    const mine = reports.filter((item) => item.ownerId === currentUser.uid);
    list.sort((a, b) => Math.max(0, ...mine.map((m) => scoreMatch(m, b))) - Math.max(0, ...mine.map((m) => scoreMatch(m, a))));
  } else list.sort((a, b) => reportTime(b) - reportTime(a));
  $('report-grid').innerHTML = list.length ? list.map((item) => reportCard(item)).join('') : '';
  $('empty-state').classList.toggle('hidden', list.length > 0);
  $('report-grid').classList.toggle('hidden', list.length === 0);
  $('browse-notice').classList.add('hidden');
  bindClaimButtons();
  renderMatches();
}
function renderMatches() {
  if (!currentUser) {
    $('matches-grid').innerHTML = '<div class="empty-match">Log in and publish a report to see possible matches.</div>';
    return;
  }
  const mine = reports.filter((item) => item.ownerId === currentUser.uid);
  if (!mine.length) {
    $('matches-grid').innerHTML = '<div class="empty-match">Publish a lost or found report to get matching suggestions.</div>';
    return;
  }
  const suggestions = reports.filter((item) => item.ownerId !== currentUser.uid).map((item) => ({item, score: Math.max(0, ...mine.map((mineItem) => scoreMatch(mineItem, item)))})).filter((entry) => entry.score >= 18).sort((a, b) => b.score - a.score).slice(0, 3);
  $('matches-grid').innerHTML = suggestions.length ? suggestions.map((entry) => reportCard(entry.item, entry.score)).join('') : '<div class="empty-match">No close matches yet. New community reports may create a match later.</div>';
  bindClaimButtons();
}
function bindClaimButtons() {
  document.querySelectorAll('.claim-item').forEach((button) => {
    button.addEventListener('click', async () => {
      if (!currentUser) { openAuth('login'); return; }
      const report = reports.find((item) => item.id === button.dataset.id);
      if (!report) return;
      const reason = window.prompt('Describe a detail that helps verify this item belongs to you. Avoid sharing sensitive personal information.');
      if (reason === null) return;
      if (!reason.trim()) { showToast('Please add a short verification detail.'); return; }
      button.disabled = true;
      try {
        const existing = claims.find((claim) => claim.reportId === report.id && claim.claimantId === currentUser.uid && claim.status === 'pending');
        if (existing) { showToast('You already have a pending claim for this item.'); return; }
        await addDoc(collection(db, 'claims'), {reportId: report.id, reportOwnerId: report.ownerId, claimantId: currentUser.uid, claimantEmail: currentUser.email || '', reason: reason.trim().slice(0, 600), status: 'pending', createdAt: serverTimestamp()});
        showToast('Claim submitted. The person who found it can review your request.');
      } catch (error) {
        showToast(error.message || 'Could not submit claim. Please try again.');
      } finally { button.disabled = false; }
    });
  });
}
function renderClaims() {
  if (!currentUser) { $('claims-list').innerHTML = '<p class="muted">Log in to view your claims.</p>'; return; }
  if (!claims.length) { $('claims-list').innerHTML = '<p class="muted">No claims yet.</p>'; return; }
  $('claims-list').innerHTML = claims.map((claim) => {
    const report = reports.find((item) => item.id === claim.reportId);
    const title = report ? report.title : 'Item report';
    const isOwner = claim.reportOwnerId === currentUser.uid;
    const status = ['pending','approved','rejected'].includes(claim.status) ? claim.status : 'pending';
    const buttons = isOwner && status === 'pending' ? '<div class="claim-buttons"><button class="button button-primary claim-decision" data-id="' + escapeHtml(claim.id) + '" data-status="approved">Approve</button><button class="button button-outline claim-decision" data-id="' + escapeHtml(claim.id) + '" data-status="rejected">Reject</button></div>' : '';
    return '<div class="claim-row"><div><h4>' + escapeHtml(title) + '</h4><p>' + (isOwner ? 'Claim from ' + escapeHtml(claim.claimantEmail || 'community member') : 'Your claim') + ' · ' + escapeHtml(formatDate(claim.createdAt)) + '</p><p>' + escapeHtml(claim.reason || '') + '</p></div><div><span class="claim-status ' + status + '">' + status + '</span>' + buttons + '</div></div>';
  }).join('');
  document.querySelectorAll('.claim-decision').forEach((button) => button.addEventListener('click', async () => {
    button.disabled = true;
    try {
      await updateDoc(doc(db, 'claims', button.dataset.id), {status: button.dataset.status, reviewedAt: serverTimestamp()});
      showToast('Claim ' + button.dataset.status + '.');
    } catch (error) { showToast(error.message || 'Could not update claim.'); }
    finally { button.disabled = false; }
  }));
}
function openAuth(mode) {
  authMode = mode;
  $('auth-eyebrow').textContent = mode === 'signup' ? 'JOIN THE COMMUNITY' : 'WELCOME BACK';
  $('auth-title').textContent = mode === 'signup' ? 'Create account' : 'Log in';
  $('auth-description').textContent = mode === 'signup' ? 'Create an account to publish reports and manage claims.' : 'Pick up where you left off.';
  $('auth-submit').textContent = mode === 'signup' ? 'Create account' : 'Log in';
  $('auth-switch').innerHTML = mode === 'signup' ? 'Already registered? <button class="inline-button" id="switch-auth">Log in</button>' : 'New here? <button class="inline-button" id="switch-auth">Create an account</button>';
  $('auth-password').autocomplete = mode === 'signup' ? 'new-password' : 'current-password';
  setMessage('auth-message', '');
  $('auth-dialog').showModal();
  $('switch-auth').addEventListener('click', () => openAuth(authMode === 'signup' ? 'login' : 'signup'));
}
$('open-login').addEventListener('click', () => openAuth('login'));
$('open-signup').addEventListener('click', () => openAuth('signup'));
$('close-auth').addEventListener('click', () => $('auth-dialog').close());
$('auth-dialog').addEventListener('click', (event) => { if (event.target === $('auth-dialog')) $('auth-dialog').close(); });
$('auth-form').addEventListener('submit', async (event) => {
  event.preventDefault();
  const email = $('auth-email').value.trim();
  const password = $('auth-password').value;
  const button = $('auth-submit');
  button.disabled = true;
  setMessage('auth-message', '');
  try {
    if (authMode === 'signup') {
      const credential = await createUserWithEmailAndPassword(auth, email, password);
      await updateProfile(credential.user, {displayName: email.split('@')[0]});
    } else await signInWithEmailAndPassword(auth, email, password);
    $('auth-dialog').close();
    $('auth-form').reset();
    showToast(authMode === 'signup' ? 'Account created. Welcome!' : 'You are now logged in.');
  } catch (error) {
    const messages = {'auth/email-already-in-use':'An account with this email already exists.','auth/invalid-email':'Enter a valid email address.','auth/weak-password':'Use a password with at least 6 characters.','auth/invalid-credential':'Email or password is incorrect.','auth/too-many-requests':'Too many attempts. Please wait and try again.'};
    setMessage('auth-message', messages[error.code] || error.message || 'Authentication failed.');
  } finally { button.disabled = false; }
});
$('logout-button').addEventListener('click', async () => { try { await signOut(auth); showToast('You have logged out.'); } catch (error) { showToast(error.message); } });
$('report-form').addEventListener('submit', async (event) => {
  event.preventDefault();
  if (!currentUser) { openAuth('login'); setMessage('report-message', 'Log in to publish your report.'); return; }
  const button = $('report-form').querySelector('button[type="submit"]');
  const photo = $('item-photo').files[0];
  if (photo && photo.size > 5 * 1024 * 1024) { setMessage('report-message', 'Photo must be 5 MB or smaller.'); return; }
  if (photo && !['image/jpeg','image/png','image/webp'].includes(photo.type)) { setMessage('report-message', 'Choose a JPEG, PNG or WebP image.'); return; }
  button.disabled = true;
  button.textContent = 'Publishing…';
  setMessage('report-message', '');
  try {
    const occurredAt = new Date($('item-time').value);
    if (Number.isNaN(occurredAt.getTime())) throw new Error('Choose a valid date and time.');
    const reportData = {type:$('report-type').value, title:$('item-title').value.trim(), category:$('item-category').value, description:$('item-description').value.trim(), location:$('item-location').value.trim(), occurredAt:Timestamp.fromDate(occurredAt), ownerId:currentUser.uid, ownerName:currentUser.displayName || '', photoURL:'', photoPath:'', status:'open', createdAt:serverTimestamp()};
    const created = await addDoc(collection(db, 'reports'), reportData);
    if (photo) {
      const photoPath = 'report-photos/' + currentUser.uid + '/' + created.id + '-' + Date.now() + '-' + photo.name.replace(/[^a-zA-Z0-9._-]/g, '_');
      const photoRef = ref(storage, photoPath);
      await uploadBytes(photoRef, photo, {contentType:photo.type});
      reportData.photoURL = await getDownloadURL(photoRef);
      reportData.photoPath = photoPath;
      await updateDoc(doc(db, 'reports', created.id), {photoURL:reportData.photoURL, photoPath:photoPath});
    }
    $('report-form').reset();
    $('item-time').value = new Date(Date.now() - new Date().getTimezoneOffset() * 60000).toISOString().slice(0, 16);
    setMessage('report-message', 'Report published successfully.', true);
    showToast('Your report is live on the community board.');
    document.getElementById('browse').scrollIntoView({behavior:'smooth'});
  } catch (error) {
    setMessage('report-message', error.message || 'Could not publish report. Check Firebase setup and try again.');
  } finally { button.disabled = false; button.innerHTML = 'Publish report <span>→</span>'; }
});
['search-input','type-filter','location-filter','sort-filter'].forEach((id) => $(id).addEventListener('input', renderReports));
$('refresh-button').addEventListener('click', () => { renderReports(); showToast('Reports refreshed.'); });

onAuthStateChanged(auth, (user) => {
  currentUser = user;
  $('auth-actions').classList.toggle('hidden', !!user);
  $('user-actions').classList.toggle('hidden', !user);
  $('user-label').textContent = user ? (user.displayName || user.email || 'Signed in') : '';
  if (unsubscribeClaims) unsubscribeClaims();
  if (unsubscribeOwnerClaims) unsubscribeOwnerClaims();
  unsubscribeClaims = null;
  unsubscribeOwnerClaims = null;
  if (user) {
    let ownClaims = [];
    let ownerClaims = [];
    const mergeClaims = () => {
      const byId = new Map();
      ownClaims.concat(ownerClaims).forEach((claim) => byId.set(claim.id, claim));
      claims = Array.from(byId.values());
      renderClaims();
    };
    unsubscribeClaims = onSnapshot(query(collection(db, 'claims'), where('claimantId', '==', user.uid)), (snapshot) => {
      ownClaims = snapshot.docs.map((item) => ({id:item.id, ...item.data()}));
      mergeClaims();
    }, (error) => {
      console.error('Your claims:', error);
      $('claims-list').innerHTML = '<p class="muted">Could not load your claims. Check Firestore rules.</p>';
    });
    unsubscribeOwnerClaims = onSnapshot(query(collection(db, 'claims'), where('reportOwnerId', '==', user.uid)), (snapshot) => {
      ownerClaims = snapshot.docs.map((item) => ({id:item.id, ...item.data()}));
      mergeClaims();
    }, (error) => {
      console.error('Claims on your reports:', error);
      ownerClaims = [];
      mergeClaims();
    });
  } else {
    claims = [];
    renderClaims();
  }
  renderReports();
});

unsubscribeReports = onSnapshot(query(collection(db, 'reports'), orderBy('createdAt', 'desc')), (snapshot) => {
  reports = snapshot.docs.map((item) => ({id:item.id, ...item.data()}));
  renderReports();
}, (error) => {
  console.error('Reports listener:', error);
  $('report-grid').innerHTML = '';
  $('browse-notice').textContent = 'Could not load reports. Configure Firebase and publish the Firestore rules before using the app.';
  $('browse-notice').classList.remove('hidden');
  $('empty-state').classList.add('hidden');
});
