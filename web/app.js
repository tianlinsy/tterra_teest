/* =========================================================
   The Garden — front-end logic (live build)

   Same UX as the demo page; the localStorage store is replaced
   with Firestore so that:
     • every phone sees the same garden,
     • tapping a bracelet is recorded for BOTH people,
     • each bracelet's ESP32 can read its owner's connection
       count and light one LED per connection.

   Data model (Firestore)
     guests/{braceletId}                 name, answer, plant, photo, count, createdAt
     guests/{braceletId}/contacts/{id}   name, plant, metAt
========================================================= */

import { initializeApp } from "https://www.gstatic.com/firebasejs/10.12.2/firebase-app.js";
import {
    getFirestore, doc, getDoc, setDoc, deleteDoc, onSnapshot, collection, query,
    orderBy, limit, runTransaction, writeBatch, serverTimestamp, increment,
    connectFirestoreEmulator,
} from "https://www.gstatic.com/firebasejs/10.12.2/firebase-firestore.js";
import { firebaseConfig, BLOOM_AT } from "./firebase-config.js";

const app = initializeApp(firebaseConfig);
const db  = getFirestore(app);
// Local testing against `firebase emulators:start` (see SETUP_GUIDE.md §9):
if (new URLSearchParams(location.search).has("emulator")) connectFirestoreEmulator(db, "127.0.0.1", 8080);

// ---- plant palette (deterministic assignment) ----
const PLANTS = [
    {id:'lotus',    name:'Lotus',          emoji:'🪷', color:'#e48fb0'},
    {id:'rose',     name:'Rose',           emoji:'🌹', color:'#e4572e'},
    {id:'sunflower',name:'Sunflower',      emoji:'🌻', color:'#f4c430'},
    {id:'lavender', name:'Lavender',       emoji:'💜', color:'#9b7ede'},
    {id:'fern',     name:'Fern',           emoji:'🌿', color:'#4c956c'},
    {id:'orchid',   name:'Orchid',         emoji:'🌸', color:'#c77dff'},
    {id:'maple',    name:'Maple',          emoji:'🍁', color:'#d1495b'},
    {id:'cherry',   name:'Cherry Blossom', emoji:'🌸', color:'#ffb7c5'},
    {id:'cactus',   name:'Cactus',         emoji:'🌵', color:'#57a773'},
    {id:'poppy',    name:'Poppy',          emoji:'🌺', color:'#ef476f'},
    {id:'bluebell', name:'Bluebell',       emoji:'🔔', color:'#5c7aff'},
    {id:'jasmine',  name:'Jasmine',        emoji:'🤍', color:'#eae2b7'},
];
const plantById = id => PLANTS.find(p => p.id === id) || PLANTS[0];

// ---- growth stages: connections -> stage index 0..4 (BLOOM_AT = bloom) ----
const STAGES = [
    {name:'Seed',       sub:'A tiny beginning. Go meet someone.'},
    {name:'Sprout',     sub:'You broke the soil — keep connecting.'},
    {name:'Growing',    sub:"Leaves unfurling. You're halfway there."},
    {name:'Budding',    sub:'Almost there — a couple more connections.'},
    {name:'Full Bloom', sub:"You've fully bloomed. Beautiful."},
];
const T_GROW = Math.max(2, Math.round(BLOOM_AT * 0.4));   // 9 → 4
const T_BUD  = Math.max(T_GROW + 1, Math.round(BLOOM_AT * 0.7)); // 9 → 6
function stageIndex(n){
    if(n >= BLOOM_AT) return 4;
    if(n >= T_BUD)    return 3;
    if(n >= T_GROW)   return 2;
    if(n >= 1)        return 1;
    return 0;
}

// ---- local identity (which bracelet is "me" on this phone) ----
const KEY_ME = 'garden_me';
const loadMe = () => { try{return JSON.parse(localStorage.getItem(KEY_ME))}catch(e){return null} };
const saveMe = m  => { try{localStorage.setItem(KEY_ME,JSON.stringify(m))}catch(e){} };
const clearMe = () => { try{localStorage.removeItem(KEY_ME)}catch(e){} };

// ---- helpers ----
const $ = id => document.getElementById(id);
const ID_RE = /^[A-Z0-9-]{1,16}$/;
const normId = s => String(s || '').trim().toUpperCase();
function hash(s){ let h=0; for(let i=0;i<s.length;i++) h=(h*31+s.charCodeAt(i))|0; return Math.abs(h); }
function assignPlant(name, answer){ return PLANTS[ hash((name||'')+'::'+(answer||'')) % PLANTS.length ]; }
const guestRef   = id => doc(db, 'guests', id);

// Personal link: anyone who opens it becomes this guest's connection.
const linkFor = id => location.origin + location.pathname + '?met=' + encodeURIComponent(id);

// Random, unambiguous 6-character code for guests without a bracelet (no 0/O/1/I).
const CODE_ALPHABET = 'ABCDEFGHJKMNPQRSTUVWXYZ23456789';
function randomCode(){
    const a = new Uint8Array(6); crypto.getRandomValues(a);
    return [...a].map(b => CODE_ALPHABET[b % CODE_ALPHABET.length]).join('');
}
async function freshCode(){
    for(let i = 0; i < 6; i++){
        const c = randomCode();
        if(!(await getDoc(guestRef(c))).exists()) return c;
    }
    throw new Error('could not find a free code');
}
const contactRef = (owner, other) => doc(db, 'guests', owner, 'contacts', other);

// Downscale the selfie to a 160px square JPEG (~10 KB) so it fits in the doc.
function shrinkPhoto(file, size = 160){
    return new Promise(res => {
        const img = new Image();
        img.onload = () => {
            const c = document.createElement('canvas'); c.width = c.height = size;
            const s = Math.min(img.width, img.height);
            c.getContext('2d').drawImage(img, (img.width-s)/2, (img.height-s)/2, s, s, 0, 0, size, size);
            URL.revokeObjectURL(img.src);
            res(c.toDataURL('image/jpeg', .72));
        };
        img.onerror = () => res('');
        img.src = URL.createObjectURL(file);
    });
}

let photoData  = '';
let pendingMet = null;      // bracelet tapped before onboarding finished
let meDoc      = null;      // live copy of guests/{me}
let contacts   = [];        // live copy of guests/{me}/contacts
let unsubs     = [];

// ---- onboarding ----
$('photo').addEventListener('change', async e => {
    const f = e.target.files[0]; if(!f) return;
    photoData = await shrinkPhoto(f);
    const p = $('photoPreview'); p.src = photoData; p.style.display = photoData ? 'block' : 'none';
});
$('code').addEventListener('input', e => { e.target.value = normId(e.target.value); });

$('plantMe').addEventListener('click', async () => {
    const name = $('name').value.trim();
    let   id   = normId($('code').value);
    if(!name){ toast('Please enter your name 🌱'); return; }
    if(id && !ID_RE.test(id)){ toast('Bracelet code is letters/numbers, e.g. A1'); return; }
    const answer = $('q1').value;
    const btn = $('plantMe'); btn.disabled = true;

    try {
        if(!id) id = await freshCode();                      // no bracelet → personal code
        const snap = await getDoc(guestRef(id));
        if(snap.exists()){
            // Someone already claimed this bracelet. If it's the same name, treat it as
            // "rejoin from a new phone"; otherwise refuse.
            const g = snap.data();
            if(g.name.trim().toLowerCase() !== name.toLowerCase()){
                toast('Bracelet ' + id + ' is already planted by ' + g.name + ' 🌿');
                btn.disabled = false; return;
            }
            saveMe({ id, name: g.name, plant: g.plant });
        } else {
            const plant = assignPlant(name, answer);
            const data  = { name, answer, plant: plant.id, count: 0, createdAt: serverTimestamp() };
            if(photoData) data.photo = photoData;
            await setDoc(guestRef(id), data);            // ⇢ guests/{braceletId}
            saveMe({ id, name, plant: plant.id });
        }
        $('onboarding').hidden = true;
        enterApp();
        if(pendingMet){ const m = pendingMet; pendingMet = null; addContact(m); }
    } catch(err){
        console.error(err);
        toast('Could not reach the garden — check your connection.');
        btn.disabled = false;
    }
});

// ---- add a contact (from a scan or a demo tap) — recorded for BOTH people ----
async function addContact(rawId){
    const me = loadMe();
    if(!me){ pendingMet = rawId; return; }
    const otherId = normId(rawId);
    if(!ID_RE.test(otherId)) return;
    if(otherId === me.id){ toast("That's your own bracelet 🌱"); return; }

    try {
        const otherSnap = await getDoc(guestRef(otherId));           // ⇢ fetch guests/{id}
        if(!otherSnap.exists()){ toast('Bracelet ' + otherId + " hasn't been planted yet"); return; }
        const other = otherSnap.data();

        const added = await runTransaction(db, async tx => {
            const mine   = await tx.get(contactRef(me.id, otherId));
            const theirs = await tx.get(contactRef(otherId, me.id));
            if(mine.exists()) return false;
            tx.set(contactRef(me.id, otherId), { name: other.name, plant: other.plant, metAt: serverTimestamp() });
            tx.update(guestRef(me.id), { count: increment(1) });
            if(!theirs.exists()){
                tx.set(contactRef(otherId, me.id), { name: me.name, plant: me.plant, metAt: serverTimestamp() });
                tx.update(guestRef(otherId), { count: increment(1) });
            }
            return true;
        });

        const pl = plantById(other.plant);
        if(!added){ toast('You already met ' + other.name + ' 🌿'); return; }
        const total = contacts.length + 1;
        toast('🌸 You met ' + other.name + ' — a ' + pl.name + '!' +
            (total >= BLOOM_AT ? "  You've fully bloomed!" : '  (' + total + '/' + BLOOM_AT + ')'));
    } catch(err){
        console.error(err);
        toast('Could not record the connection — try again.');
    }
}

// ---- live subscriptions once we know who "me" is ----
function enterApp(){
    const me = loadMe(); if(!me) return;
    linkRenderedFor = '';
    $('app').hidden = false;
    unsubs.forEach(u => u()); unsubs = [];

    unsubs.push(onSnapshot(guestRef(me.id), snap => {
        if(!snap.exists()){            // garden was reset from another phone
            clearMe(); location.href = location.pathname; return;
        }
        meDoc = snap.data();
        renderMe();
    }));

    unsubs.push(onSnapshot(query(collection(db, 'guests', me.id, 'contacts'), orderBy('metAt', 'desc')), snap => {
        contacts = snap.docs.map(d => ({ id: d.id, ...d.data(), pending: d.metadata.hasPendingWrites }));
        renderMe(); renderContacts(); renderChips();
    }));

    // Everyone in the garden, for demo chips.
    unsubs.push(onSnapshot(query(collection(db, 'guests'), orderBy('createdAt', 'desc'), limit(40)), snap => {
        roster = snap.docs.map(d => ({ id: d.id, name: d.data().name, plant: d.data().plant }));
        renderChips();
    }));
}
let roster = [];

// ---- render ----
function renderMe(){
    const me = loadMe(); if(!me || !meDoc) return;
    const pl = plantById(meDoc.plant);

    document.documentElement.style.setProperty('--plant', pl.color);
    document.documentElement.style.setProperty('--plant-soft', hexToRGBA(pl.color, .28));

    $('meAvatar').innerHTML = meDoc.photo ? '<img src="'+meDoc.photo+'" alt="">' : pl.emoji;
    $('meName').textContent = meDoc.name;
    $('mePlant').innerHTML  = 'You are a <b style="color:'+pl.color+'">'+pl.name+'</b> '+pl.emoji;
    $('meCode').textContent = me.id;

    const n  = contacts.length;
    const st = STAGES[stageIndex(n)];
    $('meBadge').textContent = st.name;
    $('flower').setAttribute('data-stage', stageIndex(n));
    $('stageName').textContent = st.name;
    $('stageSub').textContent  = st.sub;

    $('barFill').style.width = (Math.min(n, BLOOM_AT) / BLOOM_AT * 100) + '%';
    $('barLabel').textContent = n >= BLOOM_AT ? 'Fully bloomed 🌸'
        : n + ' / ' + BLOOM_AT + ' connections to full bloom';

    renderLink(me.id, meDoc.name);

    // LED mirror — same thing the bracelet shows
    const leds = $('leds');
    if(leds.children.length !== BLOOM_AT) leds.innerHTML = '<i></i>'.repeat(BLOOM_AT);
    [...leds.children].forEach((el, i) => el.classList.toggle('on', i < n));
}

let linkRenderedFor = '';
function qrSvg(text, cellPx){
    if(typeof qrcode !== 'function') return '';
    const q = qrcode(0, 'M'); q.addData(text); q.make();
    return q.createSvgTag({ cellSize: cellPx, margin: 0, scalable: true });
}
function renderLink(id, name){
    if(linkRenderedFor === id) return;
    linkRenderedFor = id;
    const url = linkFor(id);
    $('myUrl').textContent = url;
    $('qr').innerHTML    = qrSvg(url, 4);
    $('qrBig').innerHTML = qrSvg(url, 8);
    $('qrBigName').textContent = name;
}
$('qr').addEventListener('click', () => { $('qrOverlay').hidden = false; });
$('qrOverlay').addEventListener('click', () => { $('qrOverlay').hidden = true; });
$('copyBtn').addEventListener('click', async () => {
    const me = loadMe(); if(!me) return;
    try { await navigator.clipboard.writeText(linkFor(me.id)); toast('Link copied 🌿'); }
    catch(e){ toast(linkFor(me.id)); }
});
$('shareBtn').addEventListener('click', async () => {
    const me = loadMe(); if(!me) return;
    const url = linkFor(me.id);
    if(navigator.share){
        try { await navigator.share({ title: 'Connect with me in The Garden', text: 'Open this to become my connection 🌸', url }); } catch(e){}
    } else {
        try { await navigator.clipboard.writeText(url); toast('Sharing not available here — link copied instead'); } catch(e){ toast(url); }
    }
});

function renderContacts(){
    $('contactCount').textContent = contacts.length;
    const box = $('contacts');
    if(!contacts.length){
        box.innerHTML = '<div class="empty">No connections yet —<br>go tap someone\'s bracelet.</div>';
        return;
    }
    box.innerHTML = contacts.map(c => {
        const p = plantById(c.plant);
        const t = c.metAt && c.metAt.toDate ? c.metAt.toDate().toLocaleTimeString([], {hour:'numeric', minute:'2-digit'}) : 'just now';
        return '<div class="contact' + (c.pending ? ' pending' : '') + '">'
            + '<div class="c-av" style="background:'+hexToRGBA(p.color,.22)+'">'+p.emoji+'</div>'
            + '<div><div class="c-name">'+esc(c.name)+'</div>'
            + '<div class="c-meta">'+p.name+' · '+esc(c.id)+' · met at '+t+'</div></div></div>';
    }).join('');
}

function renderChips(){
    const me = loadMe(); if(!me) return;
    const met = new Set(contacts.map(c => c.id));
    const remaining = roster.filter(g => g.id !== me.id && !met.has(g.id));
    $('demoChips').innerHTML = remaining.length
        ? remaining.map(g => { const p = plantById(g.plant);
            return '<span class="chip" data-id="'+esc(g.id)+'">'+p.emoji+' '+esc(g.name)+'</span>'; }).join('')
        : '<span class="c-meta">' + (roster.length > 1 ? "You've met everyone in the garden 🌸" : 'Nobody else has planted themselves yet.') + '</span>';
    document.querySelectorAll('.chip').forEach(ch =>
        ch.addEventListener('click', () => addContact(ch.dataset.id)));
}

// ---- reset: remove my connections everywhere, free the bracelet, forget this phone ----
$('reset').addEventListener('click', async () => {
    const me = loadMe(); if(!me) return;
    if(!confirm('Clear your garden and start over? This also removes you from the people you met.')) return;
    try {
        const batch = writeBatch(db);
        for(const c of contacts){
            batch.delete(contactRef(me.id, c.id));
            batch.delete(contactRef(c.id, me.id));
            batch.update(guestRef(c.id), { count: increment(-1) });
        }
        batch.delete(guestRef(me.id));
        await batch.commit();
    } catch(err){ console.error(err); }
    unsubs.forEach(u => u()); unsubs = [];
    clearMe();
    location.href = location.pathname;
});

// ---- misc ----
let toastT;
function toast(msg){
    const t = $('toast'); t.textContent = msg; t.classList.add('show');
    clearTimeout(toastT); toastT = setTimeout(() => t.classList.remove('show'), 3200);
}
function esc(s){ return String(s).replace(/[&<>"]/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c])); }
function hexToRGBA(hex, a){
    const h = hex.replace('#',''); const b = parseInt(h,16);
    return 'rgba('+((b>>16)&255)+','+((b>>8)&255)+','+(b&255)+','+a+')';
}

// ---- boot ----
(async function init(){
    const params = new URLSearchParams(location.search);
    const met = normId(params.get('met'));
    if(met){                                   // avoid re-adding on refresh
        const keep = params.has('emulator') ? '?emulator' : '';
        history.replaceState({}, '', location.pathname + keep);
    }

    const me = loadMe();
    if(me){
        $('onboarding').hidden = true;
        enterApp();
        if(met) addContact(met);
        return;
    }

    // First visit on this phone. If the tapped bracelet is unclaimed, it's
    // probably the visitor's own — offer its code. Otherwise remember the tap
    // and apply it after sign-up.
    if(met){
        try {
            const snap = await getDoc(guestRef(met));
            if(!snap.exists()){
                $('code').value = met;
                $('codeHint').textContent = 'Tapped your own bracelet? Its code ' + met + ' is filled in. Tapped a friend\'s? Clear it (or enter your own bracelet code).';
            } else {
                pendingMet = met;
                $('codeHint').textContent = 'You tapped ' + snap.data().name + '\'s bracelet — plant yourself and they\'ll be your first connection.';
            }
        } catch(e){ pendingMet = met; }
    }
})();
