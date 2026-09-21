/* =========================================================
   The Garden — front-end logic
   Runs standalone on demo data + localStorage.
   Firebase hook-in points are marked with  // ⇢ FIREBASE
========================================================= */

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

// ---- growth stages: connections -> stage index 0..4 (7 = bloom) ----
const STAGES = [
    {name:'Seed',       sub:'A tiny beginning. Go meet someone.'},
    {name:'Sprout',     sub:'You broke the soil — keep connecting.'},
    {name:'Growing',    sub:"Leaves unfurling. You're halfway there."},
    {name:'Budding',    sub:'Almost there — a couple more connections.'},
    {name:'Full Bloom', sub:"You've fully bloomed. Beautiful."},
];
const BLOOM_AT = 7;
function stageIndex(n){
    if(n >= 7) return 4;
    if(n >= 5) return 3;
    if(n >= 3) return 2;
    if(n >= 1) return 1;
    return 0;
}

// ---- demo roster (stands in for the Firestore `guests` collection) ----
const GUESTS = {
    jenny:{id:'jenny',name:'Jenny',plant:'lotus'},   maya:{id:'maya',name:'Maya',plant:'sunflower'},
    alex: {id:'alex', name:'Alex', plant:'fern'},     sam: {id:'sam', name:'Sam', plant:'orchid'},
    noah: {id:'noah', name:'Noah', plant:'maple'},    lily:{id:'lily',name:'Lily',plant:'bluebell'},
    theo: {id:'theo', name:'Theo', plant:'cactus'},   zoe: {id:'zoe', name:'Zoe', plant:'cherry'},
};

// ---- storage (⇢ FIREBASE: replace with Firestore reads/writes) ----
const KEY_ME='garden_me', KEY_C='garden_contacts';
const loadMe = () => { try{return JSON.parse(localStorage.getItem(KEY_ME))}catch(e){return null} };
const saveMe = m  => { try{localStorage.setItem(KEY_ME,JSON.stringify(m))}catch(e){} };
const loadC  = () => { try{return JSON.parse(localStorage.getItem(KEY_C))||[]}catch(e){return []} };
const saveC  = c  => { try{localStorage.setItem(KEY_C,JSON.stringify(c))}catch(e){} };

// ---- helpers ----
const $ = id => document.getElementById(id);
function hash(s){ let h=0; for(let i=0;i<s.length;i++) h=(h*31+s.charCodeAt(i))|0; return Math.abs(h); }
function assignPlant(name, answer){ return PLANTS[ hash((name||'')+'::'+(answer||'')) % PLANTS.length ]; }

let photoData = '';
let pendingMet = null;

// ---- onboarding ----
$('photo').addEventListener('change', e => {
    const f = e.target.files[0]; if(!f) return;
    const r = new FileReader();
    r.onload = () => { photoData = r.result; const p=$('photoPreview'); p.src=photoData; p.style.display='block'; };
    r.readAsDataURL(f);
});

$('plantMe').addEventListener('click', () => {
    const name = $('name').value.trim();
    if(!name){ toast('Please enter your name 🌱'); return; }
    const answer = $('q1').value;
    const plant  = assignPlant(name, answer);
    // ⇢ FIREBASE: create guests/{uid} doc { name, answer, plant, photoURL }
    saveMe({ name, answer, photo:photoData, plant:plant.id });
    $('onboarding').hidden = true;
    $('app').hidden = false;
    render();
    if(pendingMet){ addContact(pendingMet); pendingMet = null; }
});

// ---- add a contact (from a scan or a demo tap) ----
function addContact(id){
    const me = loadMe();
    if(!me){ pendingMet = id; return; }
    // ⇢ FIREBASE: fetch guests/{id}; here we read the demo roster
    const g = GUESTS[id];
    if(!g) return;                                   // unknown id
    const list = loadC();
    if(list.some(c => c.id === id)){ toast('You already met ' + g.name + ' 🌿'); return; }
    const pl = plantById(g.plant);
    const rec = { id:g.id, name:g.name, plant:g.plant,
        metAt:new Date().toLocaleTimeString([], {hour:'numeric', minute:'2-digit'}) };
    list.push(rec);
    saveC(list);                                     // ⇢ FIREBASE: write guests/{me}/contacts/{id}
    render();
    const total = list.length;
    toast('🌸 You met ' + g.name + ' — a ' + pl.name + '!' +
        (total >= BLOOM_AT ? "  You've fully bloomed!" : '  (' + total + '/' + BLOOM_AT + ')'));
}

// ---- render everything ----
function render(){
    const me = loadMe(); if(!me) return;
    const pl = plantById(me.plant);

    // theme the whole page with the guest's plant color
    document.documentElement.style.setProperty('--plant', pl.color);
    document.documentElement.style.setProperty('--plant-soft', hexToRGBA(pl.color, .28));

    // avatar
    const av = $('meAvatar');
    av.innerHTML = me.photo ? '<img src="'+me.photo+'" alt="">' : pl.emoji;

    $('meName').textContent  = me.name;
    $('mePlant').innerHTML   = 'You are a <b style="color:'+pl.color+'">'+pl.name+'</b> '+pl.emoji;

    const n  = loadC().length;
    const si = stageIndex(n);
    const st = STAGES[si];

    $('meBadge').textContent = st.name;
    $('flower').setAttribute('data-stage', si);
    $('stageName').textContent = st.name;
    $('stageSub').textContent  = st.sub;

    const pct = Math.min(n, BLOOM_AT) / BLOOM_AT * 100;
    $('barFill').style.width = pct + '%';
    $('barLabel').textContent = n >= BLOOM_AT ? 'Fully bloomed 🌸'
        : n + ' / ' + BLOOM_AT + ' connections to full bloom';

    // contact list (newest first)
    const contacts = loadC();
    $('contactCount').textContent = contacts.length;
    const box = $('contacts');
    if(!contacts.length){
        box.innerHTML = '<div class="empty">No connections yet —<br>go tap someone\'s bracelet.</div>';
    } else {
        box.innerHTML = contacts.slice().reverse().map(c => {
            const p = plantById(c.plant);
            return '<div class="contact">'
                + '<div class="c-av" style="background:'+hexToRGBA(p.color,.22)+'">'+p.emoji+'</div>'
                + '<div><div class="c-name">'+esc(c.name)+'</div>'
                + '<div class="c-meta">'+p.name+' · met at '+c.metAt+'</div></div></div>';
        }).join('');
    }

    // demo chips (skip people already met)
    const met = new Set(contacts.map(c => c.id));
    const remaining = Object.values(GUESTS).filter(g => !met.has(g.id));
    $('demoChips').innerHTML = remaining.length
        ? remaining.map(g => { const p=plantById(g.plant);
            return '<span class="chip" data-id="'+g.id+'">'+p.emoji+' '+g.name+'</span>'; }).join('')
        : '<span class="c-meta">You\'ve met everyone in the demo roster 🌸</span>';
    document.querySelectorAll('.chip').forEach(ch =>
        ch.addEventListener('click', () => addContact(ch.dataset.id)));
}

// ---- misc ----
$('reset').addEventListener('click', () => {
    if(confirm('Clear your garden and start over?')){
        localStorage.removeItem(KEY_ME); localStorage.removeItem(KEY_C);
        location.href = location.pathname;
    }
});

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
(function init(){
    const met = new URLSearchParams(location.search).get('met');
    if(met) history.replaceState({}, '', location.pathname);   // avoid re-adding on refresh

    const me = loadMe();
    if(me){
        $('onboarding').hidden = true; $('app').hidden = false;
        render();
        if(met) addContact(met);
    } else {
        pendingMet = met;   // onboarding shows by default; apply tap after sign-up
    }
})();