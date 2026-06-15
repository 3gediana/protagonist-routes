// Protagonist Ecology — 3D replay viewer (three.js r160)
// Reads pre-dumped JSON snapshots (viewer_world.json + frames.json). Pure replay,
// never touches the running sim. All 9 mechanisms rendered; performance via
// InstancedMesh + per-frame buffer reuse. No mechanism is dropped for perf.
import * as THREE from 'three';
import { OrbitControls } from 'three/addons/OrbitControls.js';
import { GLTFLoader } from 'three/addons/GLTFLoader.js';

// ----------------------------------------------------------------------------- config
const WORLD = 600;                 // world is WORLD x WORLD
const VEXAG = 6.5;                 // vertical exaggeration of the heightmap relief
const BASE_FPS = 24;               // frames advanced per second at 1x
const SPECIES = [
  { name: 'Protagonist',   color: 0x5b8c5a }, // 0 主角 (muted green - still readable as 'the one')
  { name: 'Large grazer',  color: 0x8a6a45 }, // 1 大型食草 暖棕 (stag)
  { name: 'Small forager', color: 0xc2a878 }, // 2 小型觅食 浅沙褐
  { name: 'Apex predator', color: 0x4b4640 }, // 3 顶级掠食 炭黑
  { name: 'Pack hunter',   color: 0x7c8088 }, // 4 群猎 石板灰 (wolf)
  { name: 'Ambusher',      color: 0x9c7b4a }, // 5 伏击 黄褐 (tawny cat)
  { name: 'Scavenger',     color: 0x736a5e }, // 6 食腐 灰褐
  { name: 'Omnivore',      color: 0x6e5640 }, // 7 杂食 深棕 (boar)
  { name: 'Rabbit',        color: 0xd8cdbd }, // 8 兔 浅灰褐
];
const WEATHER = ['Clear', 'Cloudy', 'Rain', 'Storm', 'High Wind', 'Typhoon'];
const SEASON  = ['Spring', 'Summer', 'Autumn', 'Winter'];
const BUILDING = [
  { name: 'Wall',    color: 0xb9a06a },  // 0 墙
  { name: 'Shelter', color: 0xc98b4b },  // 1 棚
  { name: 'Store',   color: 0x8fa3b8 },  // 2 仓
  { name: 'Tower',   color: 0xd9d2c2 },  // 3 瞭望
];
const RESOURCE = [
  { name: 'Grass',  color: 0x5f7d4a },   // 0 草 (desaturated)
  { name: 'Shrub',  color: 0x6b6f3a },   // 1 灌木
  { name: 'Berry',  color: 0x9a4f5a },   // 2 浆果 (desaturated red)
];
const OBJECT = [
  { name: 'Stone', color: 0x9b9b9b },    // 0 石
  { name: 'Stick', color: 0x8a6a3a },    // 1 棍
  { name: 'Spear', color: 0x55463a },    // 2 矛
];

// ----------------------------------------------------------------------------- state
let renderer, scene, camera, controls, clock;
let world, frames, nFrames = 0;
let maxAgents = 0, maxObjects = 0;   // upper bounds across all frames (births grow the population)
let capAgents = 0, capObjects = 0;   // instanced-mesh capacities (with live headroom)
let liveMeta = null, liveBusy = false;  // live-append streaming state
let heightGrid, gridX, gridY, hMin = 0;
let cursor = 0;            // fractional frame index
let playing = true, speed = 1;
let sunLight, moonLight, ambient, hemi;
let ambientBase = 0.25;
const layers = {};        // name -> THREE.Object3D group
const inst = {};          // instanced meshes
let fireSprites = [], fireLights = [], worksiteNodes = [], treeNodes = [];
let weatherPoints = null, weatherKind = -1;
let lastWeather = -1, lastDayNight = -1;
const _m = new THREE.Matrix4(), _q = new THREE.Quaternion(),
      _p = new THREE.Vector3(), _s = new THREE.Vector3(), _c = new THREE.Color();
const _yAxis = new THREE.Vector3(0, 1, 0);
const _qI = new THREE.Quaternion();   // identity (flat ground decals)
const CREATURE_SCALE = 0.55;          // global animal size vs world (tuned down: were too large)
const fireTex = makeGlowTexture(), softTex = makeSoftTexture();

init();

// ----------------------------------------------------------------------------- load + boot
async function init() {
  renderer = new THREE.WebGLRenderer({ antialias: true });
  renderer.setSize(innerWidth, innerHeight);
  renderer.setPixelRatio(Math.min(2, devicePixelRatio));
  renderer.toneMapping = THREE.ACESFilmicToneMapping;
  renderer.toneMappingExposure = 1.0;
  document.getElementById('app').appendChild(renderer.domElement);

  scene = new THREE.Scene();
  scene.fog = new THREE.Fog(0x9fb6cc, 700, 1700);

  camera = new THREE.PerspectiveCamera(52, innerWidth / innerHeight, 1, 5000);
  // low angle ~25deg above horizon, looking across the terrain
  camera.position.set(0, 230, 560);
  controls = new OrbitControls(camera, renderer.domElement);
  controls.target.set(0, 40, 0);
  controls.maxPolarAngle = Math.PI * 0.49;
  controls.minDistance = 60; controls.maxDistance = 1600;
  controls.enableDamping = true; controls.dampingFactor = 0.08;

  // lights
  ambient = new THREE.AmbientLight(0xffffff, ambientBase); scene.add(ambient);
  hemi = new THREE.HemisphereLight(0xbfd4ff, 0x3a3326, 0.35); scene.add(hemi);
  sunLight = new THREE.DirectionalLight(0xfff2d6, 1.6); scene.add(sunLight);
  moonLight = new THREE.DirectionalLight(0x9fb6ff, 0.0); scene.add(moonLight);

  clock = new THREE.Clock();

  try {
    setLoad(8, 'loading world…');
    world = await fetchJSON('./data/viewer_world.json');
    setLoad(30, 'loading frames…');
    const fr = await fetchJSON('./data/frames.json');
    frames = fr.frames; nFrames = frames.length;
    normalizeFrames();
    setLoad(70, 'loading models…');
    await Promise.all([loadSpeciesModels(), loadTreeModels()]);
    setLoad(80, 'building scene…');
    buildWorld();
    buildEntities();
    setLoad(90, 'finishing…');
    setupUI();
    document.getElementById('loading').style.display = 'none';
    addEventListener('resize', onResize);
    try { liveMeta = await fetchJSON('./data/live.json?t=' + Date.now()); } catch (e) { liveMeta = null; }
    if (liveMeta) startLivePoll();
    animate();
  } catch (e) {
    setLoad(100, 'ERROR: ' + e.message);
    console.error(e);
  }
}

// The authoritative dump (TraceRecorder.cpp -> viewer_frames.jsonl, served
// verbatim by the panel) stores every entity as a NAMED object. This viewer's
// hot path reads compact positional arrays, so we adapt the data ONCE here
// rather than in every per-frame accessor. Index layouts below are kept in sync
// with the readers in buildEntities()/updateFrame():
//   agent  A: [id, archetype, x, y, energy, lifecycle, alive]
//   tree   T: [id, x, y, integrity, available]
//   object O: [x, y, type, carried, vx, vy]
//   site   W: [id, buildingType, x, y, progress, active, complete]
//   fire   F: [x, y, intensity, active]
function normalizeFrame(f) {
  f.A = (f.agents    || []).map(a => [a.id|0, a.a|0, +a.x, +a.y, +a.e || 0, a.L|0, a.al|0]);
  f.T = (f.trees     || []).map(t => [t.id|0, +t.x, +t.y, +t.iv || 0, t.av|0]);
  f.O = (f.objects   || []).map(o => [+o.x, +o.y, o.t|0, o.c|0, +o.vx || 0, +o.vy || 0]);
  f.W = (f.worksites || []).map(w => [w.id|0, w.bt|0, +w.x, +w.y, +w.p || 0, w.a|0, w.c|0]);
  f.F = (f.fires     || []).map(fr => [+fr.x, +fr.y, +fr.i || 0, fr.a|0]);
}
function normalizeFrames() {
  for (const f of frames) normalizeFrame(f);
  maxAgents  = frames.reduce((m, f) => Math.max(m, f.A.length), 0);
  maxObjects = frames.reduce((m, f) => Math.max(m, f.O.length), 0);
}

// ---- live-stream append: poll the panel for frames written after our last
// raw line; push them onto `frames` so playback continues seamlessly (no
// iframe reload, no playhead reset).
function startLivePoll() {
  setInterval(async () => {
    if (liveBusy || !liveMeta) return;
    liveBusy = true;
    try {
      const r = await fetch('/api/replay/pe_append?start=' + (liveMeta.raw_count | 0))
        .then(x => x.json());
      if (r && r.ok && !r.reload && r.frames && r.frames.length) {
        appendFrames(r.frames);
        liveMeta.raw_count = r.raw_count;
      } else if (r && r.raw_count != null && r.ok && !r.reload) {
        liveMeta.raw_count = r.raw_count;
      }
      // r.reload => the panel will swap in the new world itself; do nothing.
    } catch (e) { /* transient network error: retry next tick */ }
    finally { liveBusy = false; }
  }, 8000);
}
function appendFrames(newFrames) {
  for (const f of newFrames) { normalizeFrame(f); frames.push(f); }
  const mA = frames.reduce((m, f) => Math.max(m, f.A.length), 0);
  const mO = frames.reduce((m, f) => Math.max(m, f.O.length), 0);
  if (mA > capAgents || mO > capObjects) { location.reload(); return; }
  maxAgents = mA; maxObjects = mO;
  nFrames = frames.length;
  const seek = document.getElementById('scrub');
  if (seek) seek.max = nFrames - 1;
}
function fetchJSON(url) {
  return fetch(url).then(r => { if (!r.ok) throw new Error(url + ' ' + r.status); return r.json(); });
}
function setLoad(pct, msg) {
  // Null-safe: this overlay only has a spinner + a text label (id="loadPct"),
  // no progress bar. Referencing a missing #loadbar used to throw on the very
  // first call and freeze the viewer permanently at "loading world…".
  const bar = document.getElementById('loadbar');
  if (bar) bar.style.width = pct + '%';
  const txt = document.getElementById('loadPct') || document.getElementById('loadpct');
  if (msg && txt) txt.textContent = msg;
}

// ----------------------------------------------------------------------------- terrain
function smooth(grid, gx, gy, passes) {
  let cur = grid.slice();
  for (let p = 0; p < passes; p++) {
    const out = cur.slice();
    for (let y = 0; y < gy; y++) for (let x = 0; x < gx; x++) {
      let s = 0, n = 0;
      for (let dy = -1; dy <= 1; dy++) for (let dx = -1; dx <= 1; dx++) {
        const xx = x + dx, yy = y + dy;
        if (xx < 0 || yy < 0 || xx >= gx || yy >= gy) continue;
        s += cur[yy * gx + xx]; n++;
      }
      out[y * gx + x] = s / n;
    }
    cur = out;
  }
  return cur;
}

// add fine multi-octave ruggedness to a (smoothed) height grid in place so the
// ground reads bumpy/uneven instead of glassy-smooth. Mutates the same grid that
// both the terrain mesh and sampleY() read, so props stay grounded on the bumps.
function ruggedize(grid, gx, gy) {
  let mn = Infinity, mx = -Infinity;
  for (const h of grid) { if (h < mn) mn = h; if (h > mx) mx = h; }
  const range = Math.max(1e-3, mx - mn);
  const h2 = (x, y) => { const n = Math.sin(x * 127.1 + y * 311.7) * 43758.5453; return (n - Math.floor(n)) * 2 - 1; };
  const vnoise = (x, y, s) => {                       // smooth value noise at scale s (cells)
    const xs = x / s, ys = y / s, x0 = Math.floor(xs), y0 = Math.floor(ys);
    const tx = xs - x0, ty = ys - y0, sx = tx * tx * (3 - 2 * tx), sy = ty * ty * (3 - 2 * ty);
    const a = h2(x0, y0), b = h2(x0 + 1, y0), c = h2(x0, y0 + 1), d = h2(x0 + 1, y0 + 1);
    return (a * (1 - sx) + b * sx) * (1 - sy) + (c * (1 - sx) + d * sx) * sy;
  };
  const smoothstep = (a, b, x) => { const t = Math.max(0, Math.min(1, (x - a) / (b - a))); return t * t * (3 - 2 * t); };
  for (let y = 0; y < gy; y++) for (let x = 0; x < gx; x++) {
    const idx = y * gx + x;
    const e = (grid[idx] - mn) / range;     // normalized elevation
    const w = smoothstep(0.30, 0.80, e);    // lowlands/shores smooth, highlands rugged
    if (w <= 0) continue;
    const fine = h2(x, y) * 0.022;          // per-vertex grain (jagged)
    const bump = vnoise(x, y, 3.0) * 0.05;  // coherent small lumps
    grid[idx] += (fine + bump) * range * w;
  }
}

// bilinear height sample in world coords -> scene Y (after exaggeration + grounding)
function sampleY(wx, wy) {
  const fx = THREE.MathUtils.clamp(wx / WORLD * (gridX - 1), 0, gridX - 1);
  const fy = THREE.MathUtils.clamp(wy / WORLD * (gridY - 1), 0, gridY - 1);
  const x0 = Math.floor(fx), y0 = Math.floor(fy);
  const x1 = Math.min(x0 + 1, gridX - 1), y1 = Math.min(y0 + 1, gridY - 1);
  const tx = fx - x0, ty = fy - y0;
  const h00 = heightGrid[y0 * gridX + x0], h10 = heightGrid[y0 * gridX + x1];
  const h01 = heightGrid[y1 * gridX + x0], h11 = heightGrid[y1 * gridX + x1];
  const h = h00 * (1 - tx) * (1 - ty) + h10 * tx * (1 - ty)
          + h01 * (1 - tx) * ty + h11 * tx * ty;
  return (h - hMin) * VEXAG;
}
// world (x,y) -> scene vector at terrain surface (+ optional lift)
function place(wx, wy, lift = 0) {
  return _p.set(wx - WORLD / 2, sampleY(wx, wy) + lift, WORLD / 2 - wy);
}

// raw bilinear height sample (pre hMin/VEXAG) from an arbitrary grid
function sampleGridRaw(grid, gx, gy, wx, wy) {
  const fx = THREE.MathUtils.clamp(wx / WORLD * (gx - 1), 0, gx - 1);
  const fy = THREE.MathUtils.clamp(wy / WORLD * (gy - 1), 0, gy - 1);
  const x0 = Math.floor(fx), y0 = Math.floor(fy), x1 = Math.min(x0 + 1, gx - 1), y1 = Math.min(y0 + 1, gy - 1);
  const tx = fx - x0, ty = fy - y0;
  const h00 = grid[y0 * gx + x0], h10 = grid[y0 * gx + x1], h01 = grid[y1 * gx + x0], h11 = grid[y1 * gx + x1];
  return h00 * (1 - tx) * (1 - ty) + h10 * tx * (1 - ty) + h01 * (1 - tx) * ty + h11 * tx * ty;
}

// per-pond irregular shoreline: radius varies with angle via low harmonics so the
// pond is a natural blob (bays/points) instead of a perfect circle. Deterministic
// per pond index, so the carved basin and the water mesh use the SAME outline.
function pondRadiusFn(seed, baseR) {
  const p1 = seed * 1.7 + 0.5, p2 = seed * 3.1 + 1.3, p3 = seed * 5.3 + 2.7;
  return (a) => baseR * (1 + 0.22 * Math.sin(a + p1) + 0.13 * Math.sin(a * 2 + p2) + 0.08 * Math.sin(a * 3 + p3));
}
// flat horizontal water mesh whose rim follows rFn(angle) — an irregular puddle.
function irregularDiscGeo(rFn, segs = 72) {
  const pos = [0, 0, 0];
  for (let i = 0; i < segs; i++) { const a = i / segs * Math.PI * 2, r = rFn(a); pos.push(Math.cos(a) * r, 0, Math.sin(a) * r); }
  const idx = [];
  for (let i = 0; i < segs; i++) { const a = i + 1, b = (i + 1) % segs + 1; idx.push(0, b, a); }
  const g = new THREE.BufferGeometry();
  g.setAttribute('position', new THREE.Float32BufferAttribute(pos, 3));
  g.setIndex(idx); g.computeVertexNormals();
  return g;
}

// carve a depression under each pond so water sits in a real basin (terrain around
// it forms the shore) instead of a flat disc that floats/clips on the bumps. The
// basin follows each pond's irregular outline. Returns {surf, rFn} per pond.
function carveBasins(grid, gx, gy, ponds) {
  const cw = WORLD / (gx - 1);            // world units per grid cell
  const depth = 3.0, rim = cw * 2.0;      // basin depth + shore taper width
  return ponds.map((w, wi) => {
    const surf = sampleGridRaw(grid, gx, gy, w.x, w.y);   // terrain height at pond center
    const baseR = w.r * 3.2;
    const rFn = pondRadiusFn(wi + 1, baseR);
    const maxR = baseR * 1.45;                             // bound the cell scan to widest lobe
    const ci = w.x / WORLD * (gx - 1), cj = w.y / WORLD * (gy - 1), cr = (maxR + rim) / cw;
    const i0 = Math.max(0, Math.floor(ci - cr)), i1 = Math.min(gx - 1, Math.ceil(ci + cr));
    const j0 = Math.max(0, Math.floor(cj - cr)), j1 = Math.min(gy - 1, Math.ceil(cj + cr));
    // pass 1: find the LOWEST original terrain along the shore band -> that is the
    // natural water line. A flat pond on a slope must sit at its lowest bank, else
    // the disc floats above the downhill side.
    let rimMin = surf;
    for (let j = j0; j <= j1; j++) for (let i = i0; i <= i1; i++) {
      const dx = (i - ci) * cw, dy = (j - cj) * cw, d = Math.hypot(dx, dy);
      const Rang = rFn(Math.atan2(-dy, dx)) + cw * 0.8;
      if (d >= Rang && d <= Rang + rim) { const h = grid[j * gx + i]; if (h < rimMin) rimMin = h; }
    }
    const level = Math.min(surf, rimMin);                  // water surface = lowest shore
    // pass 2: carve the basin floor below that water line, ramp up to the shore.
    for (let j = j0; j <= j1; j++) for (let i = i0; i <= i1; i++) {
      const dx = (i - ci) * cw, dy = (j - cj) * cw, d = Math.hypot(dx, dy);
      const Rang = rFn(Math.atan2(-dy, dx)) + cw * 0.8;    // shoreline at this bearing (matches mesh)
      if (d > Rang + rim) continue;
      const idx = j * gx + i;
      let floor;
      if (d <= Rang) floor = level - depth;                // full basin floor
      else { const t = (d - Rang) / rim; floor = (level - depth) * (1 - t) + grid[idx] * t; }  // ramp up to shore
      grid[idx] = Math.min(grid[idx], floor);
    }
    return { surf: level, rFn };
  });
}

// ----------------------------------------------------------------------------- creature meshes
// Distinct low-poly silhouettes per species so they read as animals (not balls).
// Built from boxes merged into one geometry => one InstancedMesh / draw call per
// species, scaling to the whole population. All face +Z (head toward +Z), feet at
// y=0. Per-species colour comes from the mesh material; size from life-stage scale.
function mergeBoxes(parts) {
  const geos = [];
  for (const p of parts) {
    const g = new THREE.BoxGeometry(p.w, p.h, p.d);
    if (p.rx || p.ry || p.rz) {
      const e = new THREE.Euler(p.rx || 0, p.ry || 0, p.rz || 0);
      g.applyMatrix4(new THREE.Matrix4().makeRotationFromEuler(e));
    }
    g.translate(p.x || 0, p.y || 0, p.z || 0);
    geos.push(g.toNonIndexed());
  }
  return mergeBoxesFromGeos(geos);
}
function mergeBoxesFromGeos(geosIn) {
  const geos = geosIn.map(g => g.index ? g.toNonIndexed() : g);
  let tot = 0; for (const g of geos) tot += g.attributes.position.count;
  const pos = new Float32Array(tot * 3), nrm = new Float32Array(tot * 3);
  let o = 0;
  for (const g of geos) {
    const P = g.attributes.position, N = g.attributes.normal;
    for (let i = 0; i < P.count; i++) {
      pos[o * 3] = P.getX(i); pos[o * 3 + 1] = P.getY(i); pos[o * 3 + 2] = P.getZ(i);
      nrm[o * 3] = N.getX(i); nrm[o * 3 + 1] = N.getY(i); nrm[o * 3 + 2] = N.getZ(i); o++;
    }
  }
  const out = new THREE.BufferGeometry();
  out.setAttribute('position', new THREE.BufferAttribute(pos, 3));
  out.setAttribute('normal', new THREE.BufferAttribute(nrm, 3));
  return out;
}
// generic quadruped; o overrides defaults (lengths in PE world units, ~adult ≈ 9 long)
function quad(o = {}) {
  const bL = o.bodyLen ?? 7, bH = o.bodyH ?? 3, bW = o.bodyW ?? 2.6;
  const cy = o.bodyY ?? 4.2, legL = o.legLen ?? 4.2, legW = o.legW ?? 0.9;
  const hS = o.headSize ?? 2.2, hUp = o.headUp ?? 1.6, neck = o.neck ?? 1.4;
  const parts = [];
  parts.push({ w: bW, h: bH, d: bL, x: 0, y: cy, z: 0 });                 // body
  if (o.hump) parts.push({ w: bW * 0.9, h: bH * 0.9, d: bL * 0.42, x: 0, y: cy + bH * 0.5, z: -bL * 0.05 });
  const lx = bW * 0.5 - legW * 0.5, lz = bL * 0.5 - legW * 0.7;
  for (const sx of [-1, 1]) for (const sz of [-1, 1])
    parts.push({ w: legW, h: legL, d: legW, x: sx * lx, y: legL * 0.5, z: sz * lz });
  parts.push({ w: legW * 0.9, h: neck, d: legW * 0.9, x: 0, y: cy + hUp * 0.5, z: bL * 0.5, rx: -0.5 }); // neck
  parts.push({ w: hS * 0.9, h: hS * 0.8, d: hS, x: 0, y: cy + hUp, z: bL * 0.5 + hS * 0.45 });           // head
  if (o.tailLen) parts.push({ w: legW * 0.6, h: legW * 0.6, d: o.tailLen, x: 0, y: cy, z: -bL * 0.5 - o.tailLen * 0.4, rx: o.tailUp ?? 0.5 });
  if (o.ears) for (const sx of [-1, 1]) parts.push({ w: 0.5, h: o.ears, d: 0.5, x: sx * hS * 0.3, y: cy + hUp + hS * 0.4 + o.ears * 0.4, z: bL * 0.5 + hS * 0.3 });
  if (o.antler) for (const sx of [-1, 1]) {
    parts.push({ w: 0.4, h: 3.0, d: 0.4, x: sx * hS * 0.3, y: cy + hUp + hS * 0.6, z: bL * 0.5 + hS * 0.3, rz: sx * 0.35 });
    parts.push({ w: 0.35, h: 1.6, d: 0.35, x: sx * (hS * 0.3 + 1.0), y: cy + hUp + hS * 0.6 + 1.6, z: bL * 0.5 + hS * 0.3, rz: sx * 0.8 });
  }
  return mergeBoxes(parts);
}
function biped() { // upright protagonist
  const parts = [];
  for (const sx of [-1, 1]) parts.push({ w: 1.2, h: 4.2, d: 1.2, x: sx * 1.1, y: 2.1, z: 0 });   // legs
  parts.push({ w: 3.0, h: 4.4, d: 2.0, x: 0, y: 6.3, z: 0 });                                      // torso
  for (const sx of [-1, 1]) parts.push({ w: 1.0, h: 3.8, d: 1.0, x: sx * 2.2, y: 6.2, z: 0 });     // arms
  parts.push({ w: 2.2, h: 2.2, d: 2.2, x: 0, y: 9.5, z: 0 });                                      // head
  parts.push({ w: 1.0, h: 1.0, d: 1.0, x: 0, y: 9.5, z: 1.2 });                                    // nose/face (facing +Z)
  return mergeBoxes(parts);
}
function bunny() { // small body, big haunches, long ears
  const parts = [];
  parts.push({ w: 2.2, h: 2.4, d: 3.4, x: 0, y: 2.0, z: 0 });           // body
  for (const sx of [-1, 1]) parts.push({ w: 0.9, h: 2.2, d: 1.6, x: sx * 0.8, y: 1.1, z: -1.0 }); // hind haunches
  for (const sx of [-1, 1]) parts.push({ w: 0.6, h: 1.6, d: 0.6, x: sx * 0.7, y: 0.8, z: 1.1 });  // front legs
  parts.push({ w: 1.8, h: 1.8, d: 1.8, x: 0, y: 3.4, z: 1.6 });          // head
  for (const sx of [-1, 1]) parts.push({ w: 0.5, h: 3.0, d: 0.6, x: sx * 0.5, y: 5.4, z: 1.4, rx: -0.2 }); // ears
  return mergeBoxes(parts);
}
// ---- resource props (replace flat colour discs with small volumetric models)
function grassTuft() { // a fanning clump of blades
  const parts = [], n = 7;
  for (let i = 0; i < n; i++) {
    const a = (i / n) * Math.PI * 2, lean = 0.34;
    parts.push({ w: 0.32, h: 2.9 + (i % 3) * 0.5, d: 0.32,
      x: Math.cos(a) * 0.6, y: 1.5, z: Math.sin(a) * 0.6,
      rx: Math.sin(a) * lean, rz: -Math.cos(a) * lean });
  }
  parts.push({ w: 0.4, h: 3.6, d: 0.4, x: 0, y: 1.8, z: 0 });
  return mergeBoxes(parts);
}
function grassBladeCluster() { // a few thin blades fanning out -> reads as ground stubble
  const parts = [], n = 5;
  for (let i = 0; i < n; i++) {
    const a = (i / n) * Math.PI * 2 + 0.6, lean = 0.40 + (i % 2) * 0.18;
    parts.push({ w: 0.16, h: 2.0 + (i % 3) * 0.7, d: 0.16,
      x: Math.cos(a) * 0.32, y: 0.9, z: Math.sin(a) * 0.32,
      rx: Math.sin(a) * lean, rz: -Math.cos(a) * lean });
  }
  parts.push({ w: 0.18, h: 2.6, d: 0.18, x: 0, y: 1.2, z: 0 });
  return mergeBoxes(parts);
}
function shrubBlob() { const g = new THREE.IcosahedronGeometry(2.5, 0); g.scale(1.0, 0.72, 1.0); g.translate(0, 1.8, 0); return g.toNonIndexed(); }
function berryBush() { const g = new THREE.IcosahedronGeometry(2.2, 0); g.scale(1.0, 0.80, 1.0); g.translate(0, 1.7, 0); return g.toNonIndexed(); }
function berryDots() { // ring of small spheres sitting on the berry bush
  const geos = [], n = 7;
  for (let i = 0; i < n; i++) {
    const a = (i / n) * Math.PI * 2, r = 1.4;
    const s = new THREE.IcosahedronGeometry(0.45, 0);
    s.translate(Math.cos(a) * r, 2.5 + ((i * 1.7) % 1.0), Math.sin(a) * r);
    geos.push(s.toNonIndexed());
  }
  return mergeBoxesFromGeos(geos);
}
// per-species geometry (index = species id in SPECIES[])
const SPECIES_GEO = [
  biped(),                                                                   // 0 Protagonist
  quad({ bodyLen: 9, bodyH: 3.6, bodyW: 3.0, bodyY: 5.2, legLen: 5.2, headSize: 2.4, headUp: 2.6, antler: true, tailLen: 1.5 }), // 1 Large grazer (stag)
  quad({ bodyLen: 4.5, bodyH: 2.2, bodyW: 2.0, bodyY: 2.8, legLen: 2.6, headSize: 1.7, headUp: 1.0, ears: 1.6, tailLen: 1.4, tailUp: 0.9 }), // 2 Small forager
  quad({ bodyLen: 8, bodyH: 3.0, bodyW: 2.8, bodyY: 4.0, legLen: 4.0, headSize: 2.4, headUp: 0.4, tailLen: 3.5, tailUp: 0.2 }), // 3 Apex predator (low head, long tail)
  quad({ bodyLen: 6.5, bodyH: 2.6, bodyW: 2.3, bodyY: 3.8, legLen: 4.0, headSize: 2.0, headUp: 0.8, ears: 1.3, tailLen: 3.0 }), // 4 Pack hunter (wolf)
  quad({ bodyLen: 6, bodyH: 2.0, bodyW: 2.0, bodyY: 2.6, legLen: 2.6, headSize: 1.8, headUp: 0.2, tailLen: 3.2, tailUp: 0.1 }), // 5 Ambusher (crouched cat)
  quad({ bodyLen: 6.5, bodyH: 2.6, bodyW: 2.2, bodyY: 4.0, legLen: 4.4, headSize: 2.3, headUp: 1.2, hump: true, tailLen: 1.4 }), // 6 Scavenger (hunched)
  quad({ bodyLen: 7.5, bodyH: 4.0, bodyW: 3.4, bodyY: 4.0, legLen: 3.6, headSize: 2.6, headUp: 0.6, hump: true, tailLen: 1.0 }), // 7 Omnivore (bulky)
  bunny(),                                                                   // 8 Rabbit
];

// ---- real low-poly animal models (Quaternius pack) baked into the instanced pipeline.
// We bake each GLB's rest-pose geometry (position+normal only) into a single BufferGeometry,
// normalize it to face +Z (PE heading convention) with feet on the ground, and scale to a
// target size in PE box-model units. Species we have no good model for keep their box mesh.
const _gltf = new GLTFLoader();
// species idx -> { url, targetH (height in box-model units, pre lifeScale), yaw (offset to face +Z) }
const SPECIES_GLB = {
  0: { url: './assets/animals/DLptRuewTn_DLptRuewTn.glb', targetH: 10.0, yaw: 0 },   // Protagonist -> human
  1: { url: './assets/animals/tQdzbZ1Cmw_tQdzbZ1Cmw.glb', targetH: 9.5,  yaw: 0 },   // Large grazer -> Stag
  2: { url: './assets/animals/Bc97C66HKi_Bc97C66HKi.glb', targetH: 4.2,  yaw: 0 },   // Small forager -> Fox
  3: { url: './assets/animals/P1gU3Qkr9r_P1gU3Qkr9r.glb', targetH: 6.6,  yaw: 0 },   // Apex predator -> Wolf
  4: { url: './assets/animals/y4wdQpg767_y4wdQpg767.glb', targetH: 5.0,  yaw: 0 },   // Pack hunter -> Shiba (dog)
  6: { url: './assets/animals/qmX6nhnvp7_qmX6nhnvp7.glb', targetH: 7.0,  yaw: 0 },   // Scavenger -> Donkey
  7: { url: './assets/animals/26zM1outCr_26zM1outCr.glb', targetH: 7.5,  yaw: 0 },   // Omnivore -> Cow
};
function bakeGLBGeometry(gltf, { targetH = 6, yaw = 0 }) {
  const geos = [];
  gltf.scene.updateMatrixWorld(true);
  gltf.scene.traverse(o => {
    if (!o.isMesh || !o.geometry) return;
    let g = o.geometry.index ? o.geometry.toNonIndexed() : o.geometry.clone();
    g.applyMatrix4(o.matrixWorld);                 // bind/rest pose into world space
    if (!g.attributes.normal) g.computeVertexNormals();
    const ng = new THREE.BufferGeometry();
    ng.setAttribute('position', g.attributes.position.clone());
    ng.setAttribute('normal', g.attributes.normal.clone());
    geos.push(ng);
  });
  if (!geos.length) return null;
  const merged = mergeBoxesFromGeos(geos);
  // models are Y-up (DP renders them standing as-is). Align the long body axis to Z (forward).
  merged.computeBoundingBox();
  let bb = merged.boundingBox;
  if ((bb.max.x - bb.min.x) > (bb.max.z - bb.min.z) * 1.15) merged.rotateY(Math.PI / 2);
  if (yaw) merged.rotateY(yaw);
  // scale to target height, then drop feet to y=0 and centre on x/z
  merged.computeBoundingBox(); bb = merged.boundingBox;
  const h = (bb.max.y - bb.min.y) || 1;
  const s = targetH / h; merged.scale(s, s, s);
  merged.computeBoundingBox(); bb = merged.boundingBox;
  merged.translate(-(bb.max.x + bb.min.x) / 2, -bb.min.y, -(bb.max.z + bb.min.z) / 2);
  merged.computeVertexNormals();
  return merged;
}
async function loadSpeciesModels() {
  const jobs = Object.entries(SPECIES_GLB).map(async ([si, spec]) => {
    try {
      const gltf = await _gltf.loadAsync(spec.url);
      const baked = bakeGLBGeometry(gltf, spec);
      if (baked) SPECIES_GEO[+si] = baked;
    } catch (e) { console.warn('species model load failed', si, spec.url, e); }
  });
  await Promise.all(jobs);
}

// ---- real low-poly trees (Quaternius nature pack). Only 80 trees, so each is an
// individual cloned node (keeps full original textures/materials + variety). Each
// template is normalized to unit height with its base at y=0; per-tree scale gives
// natural height + integrity-driven growth.
const hashi = (n) => ((Math.imul(n >>> 0, 2654435761)) >>> 0);
const _treeGltf = new GLTFLoader();
const TREE_DIR = './assets/nature/';
// weighted toward conifers; duplicates raise frequency for a believable mixed forest
const TREE_FILES = ['Pine_1', 'Pine_3', 'Pine_1', 'CommonTree_2', 'CommonTree_4', 'TwistedTree_1', 'DeadTree_1', 'Bush_2'];
let TREE_TEMPLATES = [];   // [{ grp, baseH }]
async function loadTreeModels() {
  TREE_TEMPLATES = [];
  for (const f of TREE_FILES) {
    try {
      const g = await _treeGltf.loadAsync(TREE_DIR + f + '.glb');
      g.scene.updateMatrixWorld(true);
      const prims = [];
      g.scene.traverse(o => {
        if (!o.isMesh || !o.geometry) return;
        const geo = o.geometry.clone(); geo.applyMatrix4(o.matrixWorld);
        let m = Array.isArray(o.material) ? o.material[0] : o.material;
        m = m.clone(); m.side = THREE.FrontSide;
        prims.push({ geo, mat: m });
      });
      if (!prims.length) continue;
      const bb = new THREE.Box3();
      prims.forEach(p => { p.geo.computeBoundingBox(); bb.union(p.geo.boundingBox); });
      const cx = (bb.min.x + bb.max.x) / 2, cz = (bb.min.z + bb.max.z) / 2;
      const h = Math.max(1e-3, bb.max.y - bb.min.y);
      const norm = new THREE.Matrix4().makeTranslation(-cx, -bb.min.y, -cz);
      const scl = new THREE.Matrix4().makeScale(1 / h, 1 / h, 1 / h);
      const grp = new THREE.Group();
      prims.forEach(p => { p.geo.applyMatrix4(norm); p.geo.applyMatrix4(scl); grp.add(new THREE.Mesh(p.geo, p.mat)); });
      TREE_TEMPLATES.push({ grp, baseH: f.startsWith('Bush') ? 10 : 30 });
    } catch (e) { console.warn('tree model load failed', f, e); }
  }
}

function buildWorld() {
  [gridX, gridY] = world.grid;
  heightGrid = smooth(world.height, gridX, gridY, 2);
  ruggedize(heightGrid, gridX, gridY);          // bumpy ground, not glassy-smooth
  const waterSurf = carveBasins(heightGrid, gridX, gridY, world.water);  // sink ponds into basins
  hMin = Math.min(...heightGrid);

  // ---- terrain mesh with per-vertex elevation colour band
  const geo = new THREE.PlaneGeometry(WORLD, WORLD, gridX - 1, gridY - 1);
  const pos = geo.attributes.position;
  const colors = new Float32Array(pos.count * 3);
  let hMax = -1e9; for (const h of heightGrid) if (h > hMax) hMax = h;
  const range = Math.max(1e-3, hMax - hMin);
  for (let i = 0; i < pos.count; i++) {
    const px = pos.getX(i), py = pos.getY(i);
    const wx = px + WORLD / 2, wy = py + WORLD / 2;     // plane local -> world
    const y = sampleY(wx, wy);
    pos.setZ(i, y);                                     // displace before rotation
    const e = ((y / VEXAG)) / range;                    // 0..1 normalized elevation
    bandColor(e, _c);
    colors[i * 3] = _c.r; colors[i * 3 + 1] = _c.g; colors[i * 3 + 2] = _c.b;
  }
  geo.setAttribute('color', new THREE.BufferAttribute(colors, 3));
  geo.rotateX(-Math.PI / 2);                            // lie flat, Y up
  geo.computeVertexNormals();
  const mat = new THREE.MeshStandardMaterial({ vertexColors: true, roughness: 0.95,
    metalness: 0.0, side: THREE.DoubleSide, flatShading: false });
  const terrain = new THREE.Mesh(geo, mat);
  terrain.name = 'terrain';
  layers.terrain = group('terrain'); layers.terrain.add(terrain);

  // ---- water: irregular puddles seated in their carved basins
  layers.water = group('water');
  const wmat = new THREE.MeshStandardMaterial({ color: 0x2f6fb0, transparent: true,
    opacity: 0.80, roughness: 0.22, metalness: 0.12, side: THREE.DoubleSide });
  world.water.forEach((w, wi) => {
    const { surf, rFn } = waterSurf[wi];
    const disc = new THREE.Mesh(irregularDiscGeo(rFn), wmat);   // horizontal, irregular rim
    const v = place(w.x, w.y, 0);
    const surfY = (surf - hMin) * VEXAG;            // pond surface sits in the carved basin
    disc.position.set(v.x, surfY + 0.3, v.z);
    layers.water.add(disc);
  });

  // ---- resources: small volumetric props per kind + soft ground shadow
  layers.resources = group('resources');
  const byKind = [[], [], []];
  for (const r of world.resources) { const k = (r.k >= 0 && r.k < 3) ? r.k : 0; byKind[k].push(r); }
  const resGeo = [grassTuft(), shrubBlob(), berryBush()];
  const resCol = [0x6b8f4e, 0x6f7438, 0x4f7a42];           // grass / shrub / berry-bush
  const yawOf = r => (r.x * 0.7 + r.y * 1.3) % 6.283;
  const scOf  = r => 0.8 + ((Math.abs(r.x) * 13 + Math.abs(r.y) * 7) % 100) / 100 * 0.6;
  inst.res = [];
  for (let k = 0; k < 3; k++) {
    const mat = new THREE.MeshStandardMaterial({ color: resCol[k], roughness: 0.95, metalness: 0.0, flatShading: true });
    const im = new THREE.InstancedMesh(resGeo[k], mat, Math.max(1, byKind[k].length));
    byKind[k].forEach((r, j) => {
      const sc = scOf(r);
      _m.compose(place(r.x, r.y, 0), _q.setFromAxisAngle(_yAxis, yawOf(r)), _s.set(sc, sc, sc));
      im.setMatrixAt(j, _m);
    });
    im.count = byKind[k].length; im.instanceMatrix.needsUpdate = true;
    layers.resources.add(im); inst.res.push(im);
  }
  // red berries sitting on the berry bushes
  const berries = byKind[2];
  const bdi = new THREE.InstancedMesh(berryDots(),
    new THREE.MeshStandardMaterial({ color: 0xb5424f, roughness: 0.55, metalness: 0.0 }),
    Math.max(1, berries.length));
  berries.forEach((r, j) => {
    const sc = scOf(r);
    _m.compose(place(r.x, r.y, 0), _q.setFromAxisAngle(_yAxis, yawOf(r)), _s.set(sc, sc, sc));
    bdi.setMatrixAt(j, _m);
  });
  bdi.count = berries.length; bdi.instanceMatrix.needsUpdate = true;
  layers.resources.add(bdi); inst.resBerry = bdi;
  // soft ground shadow under every resource (grounds them, no longer flat colour blobs)
  const shGeo = new THREE.PlaneGeometry(1, 1); shGeo.rotateX(-Math.PI / 2);
  const shMat = new THREE.MeshBasicMaterial({ color: 0x0a0d08, map: softTex, transparent: true, opacity: 0.26, depthWrite: false });
  const shR = new THREE.InstancedMesh(shGeo, shMat, world.resources.length);
  shR.renderOrder = -1;
  world.resources.forEach((r, i) => {
    const sc = scOf(r);
    _m.compose(place(r.x, r.y, 0.16), _qI, _s.set(5.5 * sc, 1, 5.5 * sc));
    shR.setMatrixAt(i, _m);
  });
  shR.instanceMatrix.needsUpdate = true;
  layers.resources.add(shR); inst.resShadow = shR;

  // ---- grass stubble: instanced tufts carpet the grassland elevation band so the
  // ground reads as real grass, not flat green paint. Skips shore/sand, rock/snow
  // uplands, and the carved water basins.
  layers.grass = group('grass');
  const gMat = new THREE.MeshStandardMaterial({ roughness: 1.0, metalness: 0.0, flatShading: true });
  const GN = 12000;
  const grassIM = new THREE.InstancedMesh(grassBladeCluster(), gMat, GN);
  const gcol = new THREE.Color();
  const pondR2 = world.water.map(w => { const r = w.r * 3.2 + 7; return r * r; });  // squared skip radius
  let gi = 0, tries = 0;
  while (gi < GN && tries < GN * 6) {
    tries++;
    const wx = Math.random() * WORLD, wy = Math.random() * WORLD;
    const y = sampleY(wx, wy);
    const e = (y / VEXAG) / range;                 // normalized elevation
    if (e < 0.10 || e > 0.66) continue;            // grassland band only
    let inPond = false;
    for (let p = 0; p < world.water.length; p++) {
      const dx = wx - world.water[p].x, dy = wy - world.water[p].y;
      if (dx * dx + dy * dy < pondR2[p]) { inPond = true; break; }
    }
    if (inPond) continue;
    const s = 0.7 + Math.random() * 0.9;
    _m.compose(place(wx, wy, 0), _q.setFromAxisAngle(_yAxis, Math.random() * 6.283),
               _s.set(s, s * (0.7 + Math.random() * 0.7), s));
    grassIM.setMatrixAt(gi, _m);
    const t = 0.55 + Math.random() * 0.45;         // per-tuft green shade variation
    gcol.setRGB(0.18 * t + 0.04, 0.40 * t + 0.10, 0.14 * t + 0.03);
    grassIM.setColorAt(gi, gcol);
    gi++;
  }
  grassIM.count = gi;
  grassIM.instanceMatrix.needsUpdate = true;
  if (grassIM.instanceColor) grassIM.instanceColor.needsUpdate = true;
  layers.grass.add(grassIM); inst.grass = grassIM;
}

function bandColor(e, out) {
  // sand -> grass -> rock -> snow, tuned so the common mid-elevation band reads
  // as green land instead of washing out to pale rock/snow.
  const sand   = new THREE.Color(0xcdb98a),
        grassL = new THREE.Color(0x6f9a52),   // low grassland (brighter green)
        grassH = new THREE.Color(0x4d6e36),   // upland grass (deeper green)
        rock   = new THREE.Color(0x6d6a5c),   // earthy rock
        snow   = new THREE.Color(0xeef2f7);
  if (e < 0.10) out.copy(sand).lerp(grassL, e / 0.10);
  else if (e < 0.66) out.copy(grassL).lerp(grassH, (e - 0.10) / 0.56);
  else if (e < 0.86) out.copy(grassH).lerp(rock, (e - 0.66) / 0.20);
  else out.copy(rock).lerp(snow, Math.min(1, (e - 0.86) / 0.14));
  return out;
}

// ----------------------------------------------------------------------------- entities
function buildEntities() {
  const f0 = frames[0];
  // headroom so live-appended frames with population growth fit without rebuild
  capAgents  = Math.ceil(maxAgents * 1.5) + 8;
  capObjects = Math.max(1, Math.ceil(maxObjects * 1.5) + 8);

  // trees: real low-poly GLB models, one cloned node per tree (only 80 -> cheap).
  // Falls back to a cone mesh per tree if the GLB templates failed to load.
  layers.trees = group('trees');
  treeNodes = [];
  for (let i = 0; i < f0.T.length; i++) {
    const id = f0.T[i][0];
    let node;
    if (TREE_TEMPLATES.length) {
      const tpl = TREE_TEMPLATES[hashi(id * 7 + 1) % TREE_TEMPLATES.length];
      node = tpl.grp.clone();
      node.userData.baseH = tpl.baseH;
    } else {
      const tgeo = new THREE.ConeGeometry(0.32, 1, 7); tgeo.translate(0, 0.5, 0);  // unit height
      node = new THREE.Mesh(tgeo, new THREE.MeshStandardMaterial({ color: 0x2f7a37, roughness: 0.9 }));
      node.userData.baseH = 22;
    }
    node.rotation.y = (id % 628) / 100;
    node.visible = false;
    layers.trees.add(node);
    treeNodes.push(node);
  }

  // species: one InstancedMesh per species using a distinct animal silhouette,
  // oriented to heading, scaled by life stage. Each mesh is sized to hold the whole
  // population (upper bound); unused instances are parked off-screen each frame.
  // ground-shadow blobs: one flat soft decal under every animal so they read as
  // standing ON the terrain rather than hovering. Single InstancedMesh sized to
  // the whole population (shadow is species-agnostic); unused parked each frame.
  layers.shadows = group('shadows');
  const shGeo = new THREE.PlaneGeometry(1, 1); shGeo.rotateX(-Math.PI / 2);
  const shMat = new THREE.MeshBasicMaterial({ color: 0x0a0d08, map: softTex,
    transparent: true, opacity: 0.34, depthWrite: false });
  inst.shadows = new THREE.InstancedMesh(shGeo, shMat, capAgents);
  inst.shadows.count = capAgents; inst.shadows.frustumCulled = false;
  inst.shadows.renderOrder = -1;
  layers.shadows.add(inst.shadows);

  layers.agents = group('agents');
  inst.species = SPECIES.map((s, si) => {
    const m = new THREE.MeshStandardMaterial({ color: s.color, roughness: 0.7, metalness: 0.03 });
    const im = new THREE.InstancedMesh(SPECIES_GEO[si] || SPECIES_GEO[0], m, capAgents);
    im.count = capAgents; im.frustumCulled = false;
    layers.agents.add(im);
    return im;
  });

  // projectiles / objects: distinct prop per type (0 Stone, 1 Stick, 2 Spear).
  layers.objects = group('objects');
  const objGeo = [
    new THREE.IcosahedronGeometry(1.7, 0),                                  // stone
    (() => { const g = new THREE.CylinderGeometry(0.35, 0.35, 5, 6); g.rotateX(Math.PI / 2); return g; })(),  // stick (lies along +Z)
    (() => {                                                                // spear: shaft + head
      const shaft = new THREE.CylinderGeometry(0.28, 0.28, 7, 6); shaft.rotateX(Math.PI / 2);
      const head = new THREE.ConeGeometry(0.7, 1.6, 6); head.rotateX(Math.PI / 2); head.translate(0, 0, 4.3);
      return mergeBoxesFromGeos([shaft, head]);
    })(),
  ];
  inst.objects = objGeo.map((g, ti) => {
    const m = new THREE.MeshStandardMaterial({ color: OBJECT[ti] ? OBJECT[ti].color : 0x999999, roughness: 0.7 });
    const im = new THREE.InstancedMesh(g, m, capObjects);
    im.count = capObjects; im.frustumCulled = false;
    layers.objects.add(im);
    return im;
  });

  // worksites / buildings: a staged timber hut per site (assembles as progress rises)
  layers.worksites = group('worksites');
  for (const w of f0.W) {
    const g = makeHut(w[1]);
    layers.worksites.add(g); worksiteNodes.push(g);
  }

  // fire: one small campfire model per source (shown on demand when lit / intensity>0)
  layers.fire = group('fire');
  for (let i = 0; i < f0.F.length; i++) {
    const cf = makeCampfire(); cf.visible = false;
    const lt = new THREE.PointLight(0xff7a20, 0, 60, 2);
    layers.fire.add(cf); layers.fire.add(lt);
    fireSprites.push(cf); fireLights.push(lt);
  }

  // ---- caves: rocky outcrops + dark sheltered mouths from the terrain cave mask
  // (a thermal + predator refuge in the sim). Interior cells render as grey
  // boulder mounds that cluster into rocky hills; edge cells (c.e===1) get a
  // recessed near-black opening so entrances read as real cave mouths. Absent
  // in older dumps (no world.caves) -> nothing drawn, viewer still works.
  layers.caves = group('caves');
  const caveCells = world.caves || [];
  if (caveCells.length) {
    const cYaw = c => (c.x * 0.9 + c.y * 1.7) % 6.283;
    const cScale = c => 3.2 + ((Math.abs(c.x) * 11 + Math.abs(c.y) * 5) % 100) / 100 * 3.2;
    const _ccol = new THREE.Color();
    const rockMat = new THREE.MeshStandardMaterial({ color: 0xffffff, roughness: 1.0, metalness: 0.0, flatShading: true });
    const rocks = new THREE.InstancedMesh(new THREE.IcosahedronGeometry(1, 0), rockMat, caveCells.length);
    caveCells.forEach((c, j) => {
      const s = cScale(c);
      _m.compose(place(c.x, c.y, s * 0.42 - 1.4), _q.setFromAxisAngle(_yAxis, cYaw(c)), _s.set(s, s * 0.82, s));
      rocks.setMatrixAt(j, _m);
      rocks.setColorAt(j, _ccol.setHex((Math.abs(c.x * 7 + c.y * 3) % 2) ? 0x6d6b64 : 0x595751));
    });
    rocks.instanceMatrix.needsUpdate = true;
    if (rocks.instanceColor) rocks.instanceColor.needsUpdate = true;
    layers.caves.add(rocks);
    const mouths = caveCells.filter(c => c.e);
    if (mouths.length) {
      const mMat = new THREE.MeshStandardMaterial({ color: 0x07070a, roughness: 1.0, metalness: 0.0 });
      const mim = new THREE.InstancedMesh(new THREE.SphereGeometry(1, 12, 8), mMat, mouths.length);
      mouths.forEach((c, j) => {
        _m.compose(place(c.x, c.y, 1.1), _q.setFromAxisAngle(_yAxis, cYaw(c)), _s.set(2.6, 1.9, 2.6));
        mim.setMatrixAt(j, _m);
      });
      mim.instanceMatrix.needsUpdate = true;
      layers.caves.add(mim);
    }
  }

  // weather particles
  layers.weather = group('weather');
}

function group(name) { const g = new THREE.Group(); g.name = name; scene.add(g); return g; }

// a small, human-scale campfire: ring of stones + a teepee of logs + a flickering
// flame and a tight glow. Replaces the old map-wide additive glow blob. The flame /
// halo / light are exposed on userData so updateFrame can flicker them per frame.
function makeCampfire() {
  const g = new THREE.Group();
  const logParts = [];
  for (let i = 0; i < 5; i++) {
    const a = i / 5 * Math.PI * 2;
    logParts.push({ w: 0.34, h: 2.6, d: 0.34, x: Math.cos(a) * 0.55, y: 1.0, z: Math.sin(a) * 0.55,
                    rx: Math.cos(a) * 0.5, rz: Math.sin(a) * 0.5 });
  }
  g.add(new THREE.Mesh(mergeBoxes(logParts),
        new THREE.MeshStandardMaterial({ color: 0x4a2f1c, roughness: 1 })));
  const stoneParts = [];
  for (let i = 0; i < 8; i++) {
    const a = i / 8 * Math.PI * 2;
    stoneParts.push({ w: 0.7, h: 0.55, d: 0.7, x: Math.cos(a) * 1.55, y: 0.25, z: Math.sin(a) * 1.55, ry: a });
  }
  g.add(new THREE.Mesh(mergeBoxes(stoneParts),
        new THREE.MeshStandardMaterial({ color: 0x6f6962, roughness: 1 })));
  const outer = new THREE.Mesh(new THREE.ConeGeometry(0.9, 2.8, 7),
        new THREE.MeshBasicMaterial({ color: 0xff6a18, transparent: true, opacity: 0.8,
          blending: THREE.AdditiveBlending, depthWrite: false }));
  outer.position.y = 1.9;
  const inner = new THREE.Mesh(new THREE.ConeGeometry(0.5, 1.8, 7),
        new THREE.MeshBasicMaterial({ color: 0xffd24a, transparent: true, opacity: 0.95,
          blending: THREE.AdditiveBlending, depthWrite: false }));
  inner.position.y = 1.5;
  g.add(outer); g.add(inner);
  const halo = new THREE.Sprite(new THREE.SpriteMaterial({ map: fireTex, color: 0xff8a30,
        transparent: true, blending: THREE.AdditiveBlending, depthWrite: false }));
  halo.scale.set(6, 6, 1); halo.position.y = 1.8;
  g.add(halo);
  g.userData = { outer, inner, halo };
  return g;
}

// a small timber hut built in distinct stages so construction can be *watched* rising:
//   site pad (always) -> 4 corner posts -> top frame beams -> walls -> thatched roof.
// updateHut(node, progress) reveals/grows each stage from the build-progress value, so a
// worksite literally assembles itself piece by piece as progress goes 0 -> 1.
function makeHut(typeIdx) {
  const col   = BUILDING[typeIdx] ? BUILDING[typeIdx].color : 0xc98b4b;
  const wood = 0x6b4a2b, woodDark = 0x553a22, thatch = 0x9c7b3f, dirt = 0x6b5a44;
  const FW = 11, half = FW / 2, postH = 8;
  const g = new THREE.Group();

  // 0) cleared site pad (always shown — a staked-out building site)
  const pad = new THREE.Mesh(new THREE.BoxGeometry(FW + 2, 0.6, FW + 2),
        new THREE.MeshStandardMaterial({ color: dirt, roughness: 1 }));
  pad.position.y = 0.3; g.add(pad);

  // 1) corner posts (grow up from ground)
  const posts = new THREE.Group(); posts.name = 'posts';
  const postGeo = new THREE.CylinderGeometry(0.42, 0.52, postH, 6);
  const pmat = new THREE.MeshStandardMaterial({ color: wood, roughness: 1 });
  for (const sx of [-1, 1]) for (const sz of [-1, 1]) {
    const p = new THREE.Mesh(postGeo, pmat); p.position.set(sx * half, postH / 2, sz * half); posts.add(p);
  }
  g.add(posts);

  // 2) top frame beams
  const frame = new THREE.Group(); frame.name = 'frame';
  const beamMat = new THREE.MeshStandardMaterial({ color: woodDark, roughness: 1 });
  const bx = new THREE.BoxGeometry(FW + 1, 0.6, 0.6), bz = new THREE.BoxGeometry(0.6, 0.6, FW + 1);
  for (const sz of [-1, 1]) { const b = new THREE.Mesh(bx, beamMat); b.position.set(0, postH, sz * half); frame.add(b); }
  for (const sx of [-1, 1]) { const b = new THREE.Mesh(bz, beamMat); b.position.set(sx * half, postH, 0); frame.add(b); }
  g.add(frame);

  // 3) walls (grow up); front wall split around a central doorway
  const walls = new THREE.Group(); walls.name = 'walls';
  const wmat = new THREE.MeshStandardMaterial({ color: col, roughness: 0.92 });
  const wallX = new THREE.BoxGeometry(FW, postH, 0.5), wallZ = new THREE.BoxGeometry(0.5, postH, FW);
  const addWall = (geo, x, z) => { const m = new THREE.Mesh(geo, wmat); m.position.set(x, postH / 2, z); walls.add(m); };
  addWall(wallX, 0, -half);
  addWall(wallZ, -half, 0);
  addWall(wallZ, half, 0);
  const fpw = (FW - 4) / 2, wallF = new THREE.BoxGeometry(fpw, postH, 0.5);
  for (const sx of [-1, 1]) addWall(wallF, sx * (fpw / 2 + 2), half);
  g.add(walls);

  // 4) thatched gable roof
  const roof = new THREE.Group(); roof.name = 'roof';
  const rm = new THREE.Mesh(new THREE.ConeGeometry(half * 1.5, 5, 4),
        new THREE.MeshStandardMaterial({ color: thatch, roughness: 1 }));
  rm.rotation.y = Math.PI / 4; rm.position.y = postH + 2.5; roof.add(rm);
  g.add(roof);

  g.userData = { posts, frame, walls, roof, postH };
  return g;
}
const _seg = (p, a, b) => THREE.MathUtils.clamp((p - a) / (b - a), 0, 1);
function updateHut(node, prog) {
  const ud = node.userData, p = THREE.MathUtils.clamp(prog, 0, 1);
  const pp = Math.max(0.06, _seg(p, 0.0, 0.25));   // posts (min stub so an idle site reads as staked)
  ud.posts.scale.y = pp;
  const ff = _seg(p, 0.25, 0.45);                  // frame beams settle on once posts are up
  ud.frame.visible = ff > 0.01; ud.frame.scale.set(ff, 1, ff); ud.frame.position.y = (1 - ff) * ud.postH * 0.2;
  const wg = _seg(p, 0.45, 0.82);                  // walls rise
  ud.walls.visible = wg > 0.01; ud.walls.scale.y = Math.max(0.001, wg);
  const rg = _seg(p, 0.82, 1.0);                   // roof lowers into place
  ud.roof.visible = rg > 0.01; ud.roof.scale.setScalar(Math.max(0.001, rg)); ud.roof.position.y = (1 - rg) * 4;
}

// ----------------------------------------------------------------------------- per-frame update
function updateFrame(fi, alpha) {
  const f = frames[fi], fn = frames[Math.min(fi + 1, nFrames - 1)];

  // ---- day / night
  const tod = lerp(f.tod, fn.tod, alpha, true), light = lerp(f.light, fn.light, alpha);
  updateDayNight(tod, light, f.w);

  // ---- agents (species): route each into its species mesh, oriented to heading
  const A = f.A, An = fn.A; let alive = 0;
  const spN = inst.species.map(() => 0);
  let shN = 0;
  for (let i = 0; i < A.length; i++) {
    const a = A[i];
    if (a[6] !== 1) continue;
    alive++;
    const an = (An[i] && An[i][0] === a[0]) ? An[i] : a;
    const wx = lerp(a[2], an[2], alpha), wy = lerp(a[3], an[3], alpha);
    const lifeScale = (a[5] === 0 ? 0.62 : (a[5] === 2 ? 1.35 : 1.0)) * CREATURE_SCALE;
    const dx = an[2] - a[2], dz = -(an[3] - a[3]);             // scene-space heading
    const speed = Math.hypot(dx, dz);
    const yaw = (speed > 0.02) ? Math.atan2(dx, dz) : (a[0] % 628) / 100;
    _q.setFromAxisAngle(_yAxis, yaw);
    // gait: moving creatures get a small body bounce (legs aren't animated under
    // instancing, so a vertical bob reads as a walk/trot). Shadow stays grounded.
    const bob = speed > 0.04
      ? Math.abs(Math.sin(performance.now() * 0.006 + (a[0] % 64) * 0.2)) * 1.6 * lifeScale
      : 0;
    _m.compose(place(wx, wy, bob), _q, _s.set(lifeScale, lifeScale, lifeScale));
    const si = (a[1] >= 0 && a[1] < inst.species.length) ? a[1] : 0;
    inst.species[si].setMatrixAt(spN[si]++, _m);
    // soft shadow on the ground (slightly larger than footprint, lifts to avoid z-fight)
    const foot = (si === 0 ? 5.0 : 8.0) * lifeScale;
    _m.compose(place(wx, wy, 0.35), _qI, _s.set(foot, 1, foot));
    inst.shadows.setMatrixAt(shN++, _m);
  }
  for (let s = 0; s < inst.species.length; s++) {
    for (let k = spN[s]; k < inst.species[s].count; k++) hideInstance(inst.species[s], k);
    inst.species[s].instanceMatrix.needsUpdate = true;
  }
  for (let k = shN; k < inst.shadows.count; k++) hideInstance(inst.shadows, k);
  inst.shadows.instanceMatrix.needsUpdate = true;

  // ---- trees (real GLB nodes; height scales with integrity, only alive shown)
  for (let i = 0; i < f.T.length; i++) {
    const t = f.T[i], node = treeNodes[i];
    if (!node) continue;
    if (t[4] !== 1) { node.visible = false; continue; }
    node.visible = true;
    const h = node.userData.baseH * (0.6 + 0.4 * THREE.MathUtils.clamp(t[3], 0, 1));
    node.position.copy(place(t[1], t[2], 0));
    node.scale.set(h, h, h);
  }

  // ---- objects / projectiles: routed by type (stone/stick/spear), oriented to flight
  const O = f.O, On = fn.O;
  const objN = inst.objects.map(() => 0);
  for (let i = 0; i < O.length; i++) {
    const o = O[i];
    const onx = On[i] || o;
    const wx = lerp(o[0], onx[0], alpha), wy = lerp(o[1], onx[1], alpha);
    const moving = Math.hypot(o[4], o[5]) > 0.05 || o[3] === 1;
    const sc = moving ? 1.25 : 1.0;
    const lift = moving ? 9 : 1.5;
    const dx = o[4], dz = -o[5];                                // velocity heading
    const yaw = (Math.hypot(dx, dz) > 0.02) ? Math.atan2(dx, dz) : 0;
    _q.setFromAxisAngle(_yAxis, yaw);
    _m.compose(place(wx, wy, lift), _q, _s.set(sc, sc, sc));
    const ti = (o[2] >= 0 && o[2] < inst.objects.length) ? o[2] : 0;
    inst.objects[ti].setMatrixAt(objN[ti]++, _m);
  }
  for (let t = 0; t < inst.objects.length; t++) {
    for (let k = objN[t]; k < inst.objects[t].count; k++) hideInstance(inst.objects[t], k);
    inst.objects[t].instanceMatrix.needsUpdate = true;
  }

  // ---- worksites: site pad always; posts->frame->walls->roof assemble with progress
  for (let i = 0; i < f.W.length; i++) {
    const w = f.W[i], node = worksiteNodes[i];
    if (!node) continue;
    node.position.copy(place(w[2], w[3], 0));
    // prefer explicit progress (w[4]); a built flag (w[6]) forces a finished hut
    const prog = (w[6] === 1) ? 1 : THREE.MathUtils.clamp(w[4], 0, 1);
    updateHut(node, prog);
  }

  // ---- fire: campfire sits on the ground; flame + glow + light flicker with intensity
  for (let i = 0; i < f.F.length; i++) {
    const fr = f.F[i], cf = fireSprites[i], lt = fireLights[i];
    if (!cf || !lt) continue;
    const inten = fr[2];
    if (inten > 0.08) {
      const v = place(fr[0], fr[1], 0);
      cf.position.copy(v); cf.visible = true;
      const flick = 0.82 + 0.18 * Math.sin(performance.now() * 0.018 + i * 1.7);
      const s = Math.min(1.4, 0.7 + inten * 0.5);          // bigger fire -> taller flame
      const ud = cf.userData;
      ud.outer.scale.set(1, s * flick, 1); ud.outer.material.opacity = 0.7 * flick;
      ud.inner.scale.set(1, s * flick, 1);
      ud.halo.scale.setScalar((4 + inten * 2) * flick);
      lt.position.set(v.x, v.y + 2, v.z); lt.intensity = Math.min(2.2, inten * 1.4) * flick;
      lt.distance = 55 + inten * 30;
    } else { cf.visible = false; lt.intensity = 0; }
  }

  // ---- weather particles
  updateWeather(f.w, light);

  // ---- HUD
  setText('clockTime', todClock(tod));
  setText('phaseName', light < 0.3 ? 'night' : 'day');
  setText('seasonV', SEASON[f.s] || '—');
  setText('weatherV', WEATHER[f.w] || '—');
  setText('popV', alive);
  let nBld = 0; for (let i = 0; i < f.W.length; i++) if (f.W[i][6] === 1) nBld++;
  setText('bldV', nBld);
  let nFire = 0; for (let i = 0; i < f.F.length; i++) if (f.F[i][2] > 0.08) nFire++;
  setText('fireV', nFire);
  setText('tickV', f.tick ?? '—');
  pushEvents(f);
}

function hideInstance(mesh, i) { _m.compose(_p.set(0, -9999, 0), _q.identity(), _s.set(0.0001, 0.0001, 0.0001)); mesh.setMatrixAt(i, _m); }

// ----------------------------------------------------------------------------- day/night
function updateDayNight(tod, light, weather) {
  // sun arc: noon overhead, night below horizon
  const ang = (tod - 0.25) * Math.PI * 2;        // tod 0.25 -> sunrise-ish
  const sx = Math.cos(ang), sy = Math.sin(ang);
  sunLight.position.set(sx * 600, Math.max(-200, sy * 700), 220);
  sunLight.intensity = Math.max(0, sy) * 1.9 * (0.4 + 0.6 * light);
  const warm = THREE.MathUtils.clamp(1 - sy, 0, 1);    // warmer near horizon
  sunLight.color.setRGB(1, 0.92 - 0.18 * warm, 0.84 - 0.4 * warm);

  moonLight.position.set(-sx * 600, Math.max(-200, -sy * 600), -180);
  moonLight.intensity = Math.max(0, -sy) * 0.5;

  hemi.intensity = 0.18 + 0.34 * light;
  // ambient: base scaled by light + storm darkening; lightning flash added per-frame (reset, never accumulated)
  const stormDark = (weather === 3 || weather === 5) ? 0.6 : 1.0;
  let amb = (0.10 + 0.30 * light) * stormDark;
  if ((weather === 3 || weather === 5) && Math.random() < 0.02) amb += 1.3;  // lightning flash, transient
  ambient.intensity = amb;

  renderer.toneMappingExposure = 0.55 + 0.75 * light;

  // sky / fog colour blends night<->day, dimmed in storm
  const day = new THREE.Color(0x9fc3e8), night = new THREE.Color(0x0c1322),
        dusk = new THREE.Color(0xc98a5a);
  _c.copy(night).lerp(day, light);
  if (warm > 0.5 && light > 0.15) _c.lerp(dusk, (warm - 0.5) * 0.7 * light);
  if (weather === 3 || weather === 5) _c.multiplyScalar(0.6);
  if (weather === 1 || weather === 2) _c.multiplyScalar(0.85);
  scene.background = _c.clone();
  scene.fog.color.copy(_c);
}

// ----------------------------------------------------------------------------- weather
function updateWeather(w, light) {
  // 0 Clear,1 Cloudy,2 Rain,3 Storm,4 HighWind,5 Typhoon
  const want = (w === 2 || w === 3) ? 'rain' : (w === 5 ? 'rain' : (w === 4 ? 'dust' : 'none'));
  const isWinter = false; // (season-driven snow could be added; rain used for wet weather)
  if (w !== lastWeather) { rebuildWeather(want); lastWeather = w; }
  if (!weatherPoints) return;
  // animate falling particles
  const pos = weatherPoints.geometry.attributes.position;
  const arr = pos.array, n = arr.length / 3;
  const fall = weatherKind === 'dust' ? 1.5 : (weatherKind === 'rain' ? 11 : 4);
  for (let i = 0; i < n; i++) {
    arr[i * 3 + 1] -= fall;
    if (weatherKind === 'dust') arr[i * 3] += 3.0;
    if (arr[i * 3 + 1] < 0) arr[i * 3 + 1] = 360 + Math.random() * 60;
    if (arr[i * 3] > WORLD / 2) arr[i * 3] -= WORLD;
  }
  pos.needsUpdate = true;
}

function rebuildWeather(kind) {
  if (weatherPoints) { layers.weather.remove(weatherPoints); weatherPoints.geometry.dispose(); weatherPoints.material.dispose(); weatherPoints = null; }
  weatherKind = kind;
  if (kind === 'none') return;
  const n = kind === 'rain' ? 3500 : 1800;
  const arr = new Float32Array(n * 3);
  for (let i = 0; i < n; i++) {
    arr[i * 3] = (Math.random() - 0.5) * WORLD * 1.2;
    arr[i * 3 + 1] = Math.random() * 420;
    arr[i * 3 + 2] = (Math.random() - 0.5) * WORLD * 1.2;
  }
  const g = new THREE.BufferGeometry(); g.setAttribute('position', new THREE.BufferAttribute(arr, 3));
  const color = kind === 'dust' ? 0xccb487 : (kind === 'rain' ? 0x9fc0e0 : 0xffffff);
  const size = kind === 'rain' ? 3.2 : (kind === 'dust' ? 5 : 6);
  const mat = new THREE.PointsMaterial({ color, size, map: softTex, transparent: true,
    opacity: kind === 'rain' ? 0.5 : 0.7, depthWrite: false, blending: THREE.NormalBlending });
  weatherPoints = new THREE.Points(g, mat);
  layers.weather.add(weatherPoints);
}

// ----------------------------------------------------------------------------- events ticker
let evState = { w: -1, night: -1 };
function pushEvents(f) {
  const night = f.light < 0.3 ? 1 : 0;
  const out = [];
  if (f.w !== evState.w && evState.w !== -1) out.push('Weather → ' + (WEATHER[f.w] || '?'));
  if (night !== evState.night && evState.night !== -1) out.push(night ? 'Night falls' : 'Day breaks');
  evState.w = f.w; evState.night = night;
  if (!out.length) return;
  const list = document.getElementById('storyLog');
  if (!list) return;
  for (const s of out) {
    const d = document.createElement('div'); d.className = 'ev';
    d.textContent = '· ' + s; list.prepend(d);
  }
  while (list.childNodes.length > 8) list.removeChild(list.lastChild);
}

// ----------------------------------------------------------------------------- UI
function setupUI() {
  // legend
  const ll = document.getElementById('legendList');
  SPECIES.forEach(s => {
    const row = document.createElement('div'); row.className = 'sp';
    row.innerHTML = `<span class="sw" style="background:#${s.color.toString(16).padStart(6,'0')}"></span>${s.name}`;
    ll.appendChild(row);
  });
  // play/pause
  const pb = document.getElementById('playBtn');
  pb.onclick = () => { playing = !playing; pb.textContent = playing ? '⏸︎ Pause' : '► Play'; };
  // seek (range slider id is 'scrub' in the markup)
  const seek = document.getElementById('scrub');
  seek.max = nFrames - 1;
  seek.oninput = () => { cursor = +seek.value; playing = false; pb.textContent = '► Play'; renderOnce(); };
  // speed (a <select id="speed"> in the markup)
  const spSel = document.getElementById('speed');
  if (spSel) { speed = +spSel.value || 1; spSel.onchange = () => { speed = +spSel.value || 1; }; }
  // layer toggles: checkbox id 't_<x>' maps to a layer group key
  const TOGGLE_LAYER = { t_terrain: 'terrain', t_resources: 'resources', t_water: 'water',
    t_trees: 'trees', t_agents: 'agents', t_objects: 'objects', t_worksites: 'worksites',
    t_fires: 'fire', t_weather: 'weather' };
  for (const [id, key] of Object.entries(TOGGLE_LAYER)) {
    const cb = document.getElementById(id);
    if (!cb) continue;
    cb.onchange = () => { const g = layers[key]; if (g) g.visible = cb.checked; };
  }
}

// ----------------------------------------------------------------------------- loop
function animate() {
  requestAnimationFrame(animate);
  const dt = Math.min(0.1, clock.getDelta());
  if (playing && nFrames > 1) {
    cursor += dt * BASE_FPS * speed;
    // live mode: hold at the newest frame and wait for appended ones (直播);
    // replay mode: loop back to the start
    if (cursor >= nFrames - 1) cursor = liveMeta ? nFrames - 1 : 0;
    document.getElementById('scrub').value = Math.floor(cursor);
  }
  const fi = Math.floor(cursor), alpha = cursor - fi;
  updateFrame(fi, alpha);
  document.getElementById('frameLbl').textContent = (fi + 1) + ' / ' + nFrames;
  controls.update();
  renderer.render(scene, camera);
}
function renderOnce() { const fi = Math.floor(cursor); updateFrame(fi, 0); controls.update(); renderer.render(scene, camera); }

// ----------------------------------------------------------------------------- helpers
function lerp(a, b, t, wrap) {
  if (wrap) { let d = b - a; if (d > 0.5) d -= 1; if (d < -0.5) d += 1; let v = a + d * t; return (v + 1) % 1; }
  return a + (b - a) * t;
}
function todClock(tod) {
  const mins = Math.floor(tod * 24 * 60); const h = Math.floor(mins / 60) % 24, m = mins % 60;
  return String(h).padStart(2, '0') + ':' + String(m).padStart(2, '0');
}
function setText(id, val) { const el = document.getElementById(id); if (el) el.textContent = val; }
function onResize() { camera.aspect = innerWidth / innerHeight; camera.updateProjectionMatrix(); renderer.setSize(innerWidth, innerHeight); }

function makeGlowTexture() {
  const c = document.createElement('canvas'); c.width = c.height = 64;
  const g = c.getContext('2d'); const grd = g.createRadialGradient(32, 32, 0, 32, 32, 32);
  grd.addColorStop(0, 'rgba(255,240,180,1)'); grd.addColorStop(0.4, 'rgba(255,150,40,0.8)');
  grd.addColorStop(1, 'rgba(255,80,0,0)'); g.fillStyle = grd; g.fillRect(0, 0, 64, 64);
  const t = new THREE.CanvasTexture(c); return t;
}
function makeSoftTexture() {
  const c = document.createElement('canvas'); c.width = c.height = 32;
  const g = c.getContext('2d'); const grd = g.createRadialGradient(16, 16, 0, 16, 16, 16);
  grd.addColorStop(0, 'rgba(255,255,255,1)'); grd.addColorStop(1, 'rgba(255,255,255,0)');
  g.fillStyle = grd; g.fillRect(0, 0, 32, 32); return new THREE.CanvasTexture(c);
}
