// Deep Protagonist — 3D replay viewer
// Reads world.json + frames.json emitted by deep_protagonist_dump (read-only,
// never embedded in a training process). Renders the full 3D world: heightmap
// terrain coloured by biome, instanced trees/rocks/cacti, plants, resources,
// animals, the protagonist agent, campfires, caves, buildings and day/night.
//
// Coordinate convention: the simulation is Z-up (x,y horizontal, z height).
// three.js is Y-up, so every world point (x,y,z) maps to three (x, z, y).

import * as THREE from 'three';
import { OrbitControls } from 'three/addons/OrbitControls.js';
import { GLTFLoader } from 'three/addons/GLTFLoader.js';
import { clone as skinnedClone } from 'three/addons/SkeletonUtils.js';

const $ = (id) => document.getElementById(id);
const W = (x, y, z) => new THREE.Vector3(x, z, y);   // world -> three

// ---------------------------------------------------------------- biome colours
// Exact RGB values mirrored from DP's native WorldRenderer.cpp::biome_color.
const BIOME_COL = [
  [110,180, 80],  // 0 Plain
  [ 30,110, 40],  // 1 Forest
  [150,145,140],  // 2 Mountain
  [ 60,130,220],  // 3 River (water)
  [ 80, 90, 60],  // 4 Swamp
  [220,195,130],  // 5 Desert
];

// ---------------------------------------------------------------- load data
const loadbar = $('loadbar'), loadpct = $('loadpct');
function setProgress(p, msg) { loadbar.style.width = (p*100).toFixed(0)+'%'; if (msg) loadpct.textContent = msg; }

async function fetchJSON(url, lo, hi, label) {
  const res = await fetch(url);
  if (!res.ok) throw new Error('failed '+url+' '+res.status);
  const total = +res.headers.get('content-length') || 0;
  if (!res.body || !total) { setProgress(hi, label); return res.json(); }
  const reader = res.body.getReader();
  let recv = 0; const chunks = [];
  for (;;) {
    const { done, value } = await reader.read();
    if (done) break;
    chunks.push(value); recv += value.length;
    setProgress(lo + (hi-lo)*Math.min(1, recv/total), label);
  }
  const buf = new Uint8Array(recv); let off = 0;
  for (const c of chunks) { buf.set(c, off); off += c.length; }
  return JSON.parse(new TextDecoder().decode(buf));
}

let WORLD, FRAMES, META;
try {
  WORLD  = await fetchJSON('./data/world.json',  0.0, 0.45, 'loading world…');
  const fj = await fetchJSON('./data/frames.json', 0.45, 0.95, 'loading frames…');
  META = { stride: fj.stride||1, fixed_dt: fj.fixed_dt||0.1 };
  FRAMES = fj.frames || fj;
  setProgress(1, 'building scene…');
} catch (e) {
  loadpct.textContent = 'load error: ' + e.message;
  throw e;
}

// ---------------------------------------------------------------- world dims
const [GX, GY]   = WORLD.grid;
const CS         = WORLD.cell_scale;
const [WSX, WSY] = WORLD.world_size;
const MAXH       = WORLD.max_height;
const H          = WORLD.height;            // GX*GY row-major float metres
const BIO        = WORLD.biomes;            // GX*GY digit string
// Add fine ground ruggedness (in metres) so the surface reads textured/uneven
// instead of glassy-smooth, BUT scaled per biome — deserts (wind-smoothed sand)
// and water stay smooth, mountains are rugged, forest moderate, plains/swamp only
// gently undulate. Mutates H in place BEFORE any surfaceH()/terrain use so the
// mesh, entities, ring and camera all stay grounded on the same bumps.
(function ruggedizeH() {
  // per-biome amplitude multiplier: 0 Plain 1 Forest 2 Mountain 3 River 4 Swamp 5 Desert
  const BIOME_RUG = [0.35, 0.65, 1.0, 0.0, 0.18, 0.0];
  const h2 = (x, y) => { const n = Math.sin(x * 127.1 + y * 311.7) * 43758.5453; return (n - Math.floor(n)) * 2 - 1; };
  const vnoise = (x, y, s) => {
    const xs = x / s, ys = y / s, x0 = Math.floor(xs), y0 = Math.floor(ys);
    const tx = xs - x0, ty = ys - y0, sx = tx * tx * (3 - 2 * tx), sy = ty * ty * (3 - 2 * ty);
    const a = h2(x0, y0), b = h2(x0 + 1, y0), c = h2(x0, y0 + 1), d = h2(x0 + 1, y0 + 1);
    return (a * (1 - sx) + b * sx) * (1 - sy) + (c * (1 - sx) + d * sx) * sy;
  };
  for (let y = 0; y < GY; y++) for (let x = 0; x < GX; x++) {
    const idx = y * GX + x;
    const bi = BIO.charCodeAt(idx) - 48;
    const k = (bi >= 0 && bi < BIOME_RUG.length) ? BIOME_RUG[bi] : 0.4;
    if (k <= 0) continue;
    const fine = h2(x, y) * 0.55;           // ±0.55 m per-vertex grain
    const bump = vnoise(x, y, 3.0) * 0.95;  // ±0.95 m coherent lumps
    H[idx] += (fine + bump) * k;
  }
})();
// Carve the river network (WORLD.rivers polyline) into a real streambed BEFORE the
// terrain mesh + surfaceH sample, so water sits recessed in a channel instead of
// floating flat tiles proud of the ground bumps. Lowers H in a tapered corridor
// along each segment; River-biome ruggedness is already 0 so banks stay smooth.
(function carveRivers() {
  const segs = WORLD.rivers || [];
  if (!segs.length) return;
  const halfW = CS * 1.7;             // channel half-width (m)
  const depth = 2.4;                  // streambed depth below banks (m)
  const r2 = halfW * halfW;
  for (const s of segs) {
    const [x1, y1, x2, y2] = s;
    const dx = x2 - x1, dy = y2 - y1;
    const segLen2 = dx * dx + dy * dy || 1e-6;
    const i0 = Math.max(0, Math.floor((Math.min(x1, x2) - halfW) / CS));
    const i1 = Math.min(GX - 1, Math.ceil((Math.max(x1, x2) + halfW) / CS));
    const j0 = Math.max(0, Math.floor((Math.min(y1, y2) - halfW) / CS));
    const j1 = Math.min(GY - 1, Math.ceil((Math.max(y1, y2) + halfW) / CS));
    for (let j = j0; j <= j1; j++) for (let i = i0; i <= i1; i++) {
      const px = i * CS, py = j * CS;
      let t = ((px - x1) * dx + (py - y1) * dy) / segLen2;
      t = Math.max(0, Math.min(1, t));
      const cx = x1 + t * dx, cy = y1 + t * dy;
      const dd = (px - cx) * (px - cx) + (py - cy) * (py - cy);
      if (dd > r2) continue;
      const f = 1 - Math.sqrt(dd) / halfW;            // 1 at centre -> 0 at rim
      H[j * GX + i] -= depth * f * f;
    }
  }
})();
// Caves in nature open into rock, not flat grassland. The sim hands us cave
// footprints that can land on plains, so BEFORE the terrain mesh we raise a
// rocky knoll at each cave and tag those cells as rock — the cave then sits
// embedded in a hillside (geologically sensible) and the boulders/mouth seat on
// the mound. Pure viewer geometry, mutates H exactly like carveRivers().
const caveRock = new Float32Array(GX * GY);   // 0..1 rockiness from cave knolls
(function raiseCaveHills() {
  const caves = WORLD.caves || [];
  if (!caves.length) return;
  for (const c of caves) {
    const [cxw, cyw, fz, cz, hx, hy] = c;
    const R = Math.max(58, Math.max(hx, hy) * 3.6);   // knoll radius (m) — wide so the rock seats flush
    const peak = 7 + Math.max(hx, hy) * 0.35;         // knoll height (m, pre-VEXAG)
    const i0 = Math.max(0, Math.floor((cxw - R) / CS)), i1 = Math.min(GX - 1, Math.ceil((cxw + R) / CS));
    const j0 = Math.max(0, Math.floor((cyw - R) / CS)), j1 = Math.min(GY - 1, Math.ceil((cyw + R) / CS));
    for (let j = j0; j <= j1; j++) for (let i = i0; i <= i1; i++) {
      const dx = i * CS - cxw, dy = j * CS - cyw, d = Math.hypot(dx, dy);
      if (d > R) continue;
      const f = 0.5 + 0.5 * Math.cos(Math.PI * d / R);   // 1 centre -> 0 rim, smooth
      const idx = j * GX + i;
      H[idx] += peak * f * f;
      if (f > caveRock[idx]) caveRock[idx] = f;
    }
  }
})();
// Vertical exaggeration: the real world is ~77 m of relief over a 1024 m map
// (~7.5%), which reads as flat from a high camera. Scale height for display so
// mountains/valleys are legible. Applied at EVERY height source (surfaceH +
// terrain mesh + water tiles) so entities, ring and camera stay grounded.
const VEXAG      = 2.4;

// bilinear terrain height at world (wx,wy)
function surfaceH(wx, wy) {
  let fx = wx / CS, fy = wy / CS;
  fx = Math.max(0, Math.min(GX-1.001, fx));
  fy = Math.max(0, Math.min(GY-1.001, fy));
  const x0 = Math.floor(fx), y0 = Math.floor(fy);
  const tx = fx - x0, ty = fy - y0;
  const h00 = H[y0*GX+x0],     h10 = H[y0*GX+x0+1];
  const h01 = H[(y0+1)*GX+x0], h11 = H[(y0+1)*GX+x0+1];
  return ((h00*(1-tx)+h10*tx)*(1-ty) + (h01*(1-tx)+h11*tx)*ty) * VEXAG;
}

// ---------------------------------------------------------------- renderer / scene
const app = $('app');
const renderer = new THREE.WebGLRenderer({ antialias: true, powerPreference: 'high-performance' });
renderer.setPixelRatio(Math.min(devicePixelRatio, 2));
renderer.setSize(innerWidth, innerHeight);
renderer.outputColorSpace = THREE.SRGBColorSpace;
renderer.toneMapping = THREE.ACESFilmicToneMapping;
renderer.toneMappingExposure = 1.0;
renderer.shadowMap.enabled = false;
app.appendChild(renderer.domElement);

const scene = new THREE.Scene();
const skyColor = new THREE.Color(0x9ec7ff);
scene.background = skyColor.clone();
scene.fog = new THREE.Fog(skyColor.clone(), 350, 1400);

const camera = new THREE.PerspectiveCamera(55, innerWidth/innerHeight, 0.5, 4000);
const cx = WSX*0.5, cy = WSY*0.5, ch = surfaceH(cx, cy);
// start looking at the protagonist's spawn so it (and nearby animals) are in view
const sp = FRAMES[0].ag;
const spx = sp[0], spy = sp[1], sph = surfaceH(spx, spy);
camera.position.copy(W(spx-55, spy-55, sph+38));

const controls = new OrbitControls(camera, renderer.domElement);
window.__view = { camera, controls, scene, THREE, W, WSX, WSY, surfaceH };  // DEBUG: remove before commit
controls.target.copy(W(spx, spy, sph+2));
controls.enableDamping = true;
controls.dampingFactor = 0.08;
controls.maxPolarAngle = Math.PI*0.495;   // stay above ground
controls.maxDistance = 1800;
controls.minDistance = 8;

// lights
const hemi = new THREE.HemisphereLight(0xbfd6ff, 0x4a4533, 0.55);
scene.add(hemi);
const sun = new THREE.DirectionalLight(0xfff2d6, 1.4);
sun.position.copy(W(cx+300, cy+200, ch+500));
scene.add(sun);
const ambient = new THREE.AmbientLight(0xffffff, 0.18);
scene.add(ambient);

// layer registry for toggles
const LAYERS = {};
function reg(name, obj) { (LAYERS[name] ||= []).push(obj); scene.add(obj); return obj; }
const frameHooks = [];   // GLB model drivers append here; run at end of applyFrame

// ---------------------------------------------------------------- terrain mesh
function buildTerrain() {
  const geo = new THREE.PlaneGeometry(WSX, WSY, GX-1, GY-1);
  geo.rotateX(-Math.PI/2);                     // lie flat, +Y up
  const pos = geo.attributes.position;
  const col = new Float32Array(pos.count*3);
  const c = new THREE.Color();
  const rock = new THREE.Color(0x6f6a60), snow = new THREE.Color(0xeef2f8);
  // normalize over the ACTUAL relief (not MAXH=120, which the world never reaches),
  // so the rock/snow bands and valley shading actually land on real terrain.
  let hMin = Infinity, hMax = -Infinity;
  for (let k = 0; k < H.length; k++) { if (H[k] < hMin) hMin = H[k]; if (H[k] > hMax) hMax = H[k]; }
  const range = Math.max(1e-3, hMax - hMin);
  // cheap value noise so each flat biome breaks up into mottled patches instead of
  // one solid block of paint (the main thing that read as "crude" vs PE).
  const hsh = (x, y) => { const n = Math.sin(x * 127.1 + y * 311.7) * 43758.5453; return n - Math.floor(n); };
  const vn = (x, y, s) => {
    const xs = x / s, ys = y / s, x0 = Math.floor(xs), y0 = Math.floor(ys);
    const tx = xs - x0, ty = ys - y0, sx = tx * tx * (3 - 2 * tx), sy = ty * ty * (3 - 2 * ty);
    const a = hsh(x0, y0), b2 = hsh(x0 + 1, y0), cc = hsh(x0, y0 + 1), d = hsh(x0 + 1, y0 + 1);
    return (a * (1 - sx) + b2 * sx) * (1 - sy) + (cc * (1 - sx) + d * sx) * sy;
  };
  for (let j = 0; j < GY; j++) {
    for (let i = 0; i < GX; i++) {
      const vi = j*GX + i;
      const h = H[vi];
      // PlaneGeometry verts run +x then +z(=world y); shift origin to (0,0)
      pos.setX(vi, i*CS);
      pos.setZ(vi, j*CS);
      pos.setY(vi, h*VEXAG);
      const b = BIO.charCodeAt(vi) - 48;
      const rgb = BIOME_COL[b] || BIOME_COL[0];
      c.setRGB(rgb[0]/255, rgb[1]/255, rgb[2]/255);
      const e = (h - hMin) / range;                 // 0..1 over real relief
      // micro grain + broad patches + valley darkening -> mottled natural ground
      const micro = vn(i, j, 2.2) - 0.5;
      const macro = vn(i, j, 13.0) - 0.5;
      const shade = 1 + micro*0.15 + macro*0.20 - (1 - e)*0.12;
      c.multiplyScalar(Math.max(0.62, shade));
      // rocky tint up the slopes + snow on the real peaks (skip water & desert)
      if (b !== 3 && b !== 5) {
        if (e > 0.52) c.lerp(rock, Math.min(1, (e - 0.52)/0.32) * 0.6);
        if (e > 0.80) c.lerp(snow, Math.min(1, (e - 0.80)/0.20));
      }
      // cave knolls read as bare rock regardless of underlying biome
      const cr = caveRock[vi];
      if (cr > 0.02) c.lerp(rock, Math.min(1, cr * 1.1));
      col[vi*3] = c.r; col[vi*3+1] = c.g; col[vi*3+2] = c.b;
    }
  }
  geo.setAttribute('color', new THREE.BufferAttribute(col, 3));
  geo.computeVertexNormals();
  const mat = new THREE.MeshStandardMaterial({ vertexColors: true, roughness: 0.97, metalness: 0.0, flatShading: false });
  const mesh = new THREE.Mesh(geo, mat);
  mesh.renderOrder = 0;
  return reg('terrain', mesh);
}
buildTerrain();

// water: the river network rendered as a continuous ribbon seated in the carved
// streambed (see carveRivers) + any standing River-biome cells as recessed pools.
// No more flat tiles floating above the ground bumps.
function buildWater() {
  const grp = new THREE.Group();
  const mat = new THREE.MeshStandardMaterial({ color: 0x2f6fc0, transparent: true, opacity: 0.62,
    roughness: 0.18, metalness: 0.35, side: THREE.DoubleSide });
  const segs = WORLD.rivers || [];
  if (segs.length) {
    const halfW = CS * 1.5;
    const verts = [], idxs = [];
    let base = 0;
    const wy = (x, y) => surfaceH(x, y) - 0.10;     // water surface just under the bank
    for (const s of segs) {
      const [x1, y1, x2, y2] = s;
      let dx = x2 - x1, dy = y2 - y1; const L = Math.hypot(dx, dy) || 1e-6;
      dx /= L; dy /= L; const nx = -dy * halfW, ny = dx * halfW;
      const corners = [[x1 + nx, y1 + ny], [x1 - nx, y1 - ny], [x2 + nx, y2 + ny], [x2 - nx, y2 - ny]];
      for (const [cx, cy] of corners) verts.push(cx, wy(cx, cy), cy);   // three: (x, height, z=worldY)
      idxs.push(base, base + 1, base + 2, base + 2, base + 1, base + 3);
      base += 4;
    }
    const geo = new THREE.BufferGeometry();
    geo.setAttribute('position', new THREE.Float32BufferAttribute(verts, 3));
    geo.setIndex(idxs); geo.computeVertexNormals();
    grp.add(new THREE.Mesh(geo, mat));
  }
  let cnt = 0;
  for (let j = 0; j < GY; j++) for (let i = 0; i < GX; i++) if (BIO.charCodeAt(j*GX+i) - 48 === 3) cnt++;
  if (cnt) {
    const geo = new THREE.PlaneGeometry(CS, CS); geo.rotateX(-Math.PI/2);
    const inst = new THREE.InstancedMesh(geo, mat, cnt);
    const m = new THREE.Matrix4(); let k = 0;
    for (let j = 0; j < GY; j++) for (let i = 0; i < GX; i++) {
      if (BIO.charCodeAt(j*GX+i) - 48 !== 3) continue;
      m.makeTranslation(i*CS, H[j*GX+i]*VEXAG - 0.10, j*CS); inst.setMatrixAt(k++, m);
    }
    inst.instanceMatrix.needsUpdate = true; grp.add(inst);
  }
  return reg('water', grp);
}
buildWater();

// ---------------------------------------------------------------- instancing helpers
function mat4(pos, scale, rotY) {
  const m = new THREE.Matrix4();
  const q = new THREE.Quaternion().setFromAxisAngle(new THREE.Vector3(0,1,0), rotY||0);
  m.compose(pos, q, new THREE.Vector3(scale, scale, scale));
  return m;
}

// ---------------------------------------------------------------- decorations
// kinds: 0 Tree, 1 Rock, 2 Cactus.  Real Quaternius GLB models (biome-varied),
// each normalised to unit height with its base at y=0, then instanced.
const NATURE_DIR = './assets/nature/';
const NATURE_FILES = {
  common:  ['CommonTree_1','CommonTree_2','CommonTree_3','CommonTree_4','CommonTree_5'],
  pine:    ['Pine_1','Pine_2','Pine_3','Pine_5'],
  twisted: ['TwistedTree_1','TwistedTree_2'],
  dead:    ['DeadTree_1','DeadTree_2'],
  bush:    ['Bush_1','Bush_2'],
  rock:    ['Rock_1','Rock_2','Rock_3'],
};
// natural metre height a unit-model is scaled to (before per-instance variation)
const NATURE_H = { common: 6.0, pine: 7.5, twisted: 5.5, dead: 5.0, bush: 1.5, rock: 1.6 };
const MODELS = {};                       // category -> [{prims:[{geo,mat}]}]
const _natureGltf = new GLTFLoader();
const loadNatureGLB = (url) => new Promise((res, rej) => _natureGltf.load(url, res, undefined, rej));

async function loadNatureModel(file) {
  const g = await loadNatureGLB(NATURE_DIR + file + '.glb');
  g.scene.updateMatrixWorld(true);
  const prims = [];
  g.scene.traverse((o) => {
    if (!o.isMesh || !o.geometry) return;
    const geo = o.geometry.clone();
    geo.applyMatrix4(o.matrixWorld);                 // bake node transform
    let mat = Array.isArray(o.material) ? o.material[0] : o.material;
    mat = mat.clone(); mat.side = THREE.FrontSide;
    prims.push({ geo, mat });
  });
  if (!prims.length) throw new Error('no mesh in ' + file);
  // combined bbox over all primitives
  const bb = new THREE.Box3();
  prims.forEach((p) => { p.geo.computeBoundingBox(); bb.union(p.geo.boundingBox); });
  const cx = (bb.min.x + bb.max.x) * 0.5, cz = (bb.min.z + bb.max.z) * 0.5;
  const h = Math.max(1e-3, bb.max.y - bb.min.y);
  const norm = new THREE.Matrix4().makeTranslation(-cx, -bb.min.y, -cz);
  const scl  = new THREE.Matrix4().makeScale(1 / h, 1 / h, 1 / h);  // -> unit height
  prims.forEach((p) => { p.geo.applyMatrix4(norm); p.geo.applyMatrix4(scl); });
  return { prims };
}

async function loadNature() {
  for (const [cat, files] of Object.entries(NATURE_FILES)) {
    MODELS[cat] = [];
    for (const f of files) {
      try { MODELS[cat].push(await loadNatureModel(f)); }
      catch (e) { console.warn('nature load failed:', f, e.message); }
    }
  }
}

function biomeAt(wx, wy) {
  const i = Math.max(0, Math.min(GX - 1, Math.round(wx / CS)));
  const j = Math.max(0, Math.min(GY - 1, Math.round(wy / CS)));
  return BIO.charCodeAt(j * GX + i) - 48;
}
const hashi = (n) => ((Math.imul(n >>> 0, 2654435761)) >>> 0);

function buildDecos() {
  const grp = new THREE.Group();
  // biome 0 Plain 1 Forest 2 Mountain 3 River 4 Swamp 5 Desert
  const treeCat = (b) => b === 2 ? 'pine' : b === 4 ? 'twisted' : b === 5 ? 'dead' : 'common';
  // buckets[cat][variantIdx] = [matrices]
  const buckets = {};
  WORLD.decos.forEach((d, idx) => {
    const k = d[5], b = biomeAt(d[0], d[1]);
    let cat;
    if (k === 1)       cat = 'rock';
    else if (k === 2)  cat = 'dead';                       // no cactus model -> desert dead tree
    else { cat = treeCat(b); if (b === 0 && hashi(idx) % 4 === 0) cat = 'pine'; } // scatter pines in plains
    const variants = MODELS[cat];
    if (!variants || !variants.length) return;
    const vi = hashi(idx * 7 + 1) % variants.length;
    // per-instance height: base metres * deco-scale-driven variation
    const base = NATURE_H[cat] || 4.0;
    const h = (cat === 'rock' || cat === 'bush')
      ? base * (0.55 + 0.30 * d[3])
      : base * (0.55 + 0.15 * d[3]);
    const m = mat4(W(d[0], d[1], d[2]), h, -d[4]);
    ((buckets[cat] ||= {})[vi] ||= []).push(m);
  });
  for (const cat in buckets) {
    for (const vi in buckets[cat]) {
      const mats = buckets[cat][vi];
      const model = MODELS[cat][vi];
      model.prims.forEach((p) => {
        const im = new THREE.InstancedMesh(p.geo, p.mat, mats.length);
        mats.forEach((m, n) => im.setMatrixAt(n, m));
        im.instanceMatrix.needsUpdate = true;
        im.castShadow = false; im.frustumCulled = true;
        grp.add(im);
      });
    }
  }
  return reg('decos', grp);
}
await loadNature();
buildDecos();

// ---------------------------------------------------------------- plants (food)
// kinds: 0 Berry,1 Fruit,2 Cactus,3 Mushroom — small coloured spheres on the ground
const PLANT_COL = [0xdc325a, 0xffaa1e, 0x64dc96, 0xd2d25a];
const PLANT_R   = [0.13, 0.16, 0.20, 0.12];   // small berry/fruit pickups, not big balls
let plantInst, plantScale = [];
function buildPlants() {
  const grp = new THREE.Group();
  const items = WORLD.plants;
  const g = new THREE.IcosahedronGeometry(1, 1);
  const m = new THREE.MeshStandardMaterial({ vertexColors: false, roughness: 0.8 });
  plantInst = new THREE.InstancedMesh(g, m, items.length);
  plantInst.instanceColor = new THREE.InstancedBufferAttribute(new Float32Array(items.length*3), 3);
  const col = new THREE.Color();
  items.forEach((p, n) => {
    const k = p[3]; const r = PLANT_R[k] || 0.25;
    plantScale[n] = r;
    plantInst.setMatrixAt(n, mat4(W(p[0], p[1], p[2]+r), r, 0));
    col.setHex(PLANT_COL[k] || 0xffffff);
    plantInst.setColorAt(n, col);
  });
  plantInst.instanceMatrix.needsUpdate = true;
  if (plantInst.instanceColor) plantInst.instanceColor.needsUpdate = true;
  grp.add(plantInst);
  return reg('plants', grp);
}
buildPlants();

// ---------------------------------------------------------------- resources
// kinds: 0 Wood (stick), 1 Stone (cube), 2 Grass (tuft cone)
let resInst, resBaseScale = [];
function buildResources() {
  const grp = new THREE.Group();
  const items = WORLD.res;
  const wood = [], stone = [], grass = [];
  items.forEach((r, n) => { const k=r[3]; (k===0?wood:k===1?stone:grass).push([r,n]); });
  const out = { wood:null, stone:null, grass:null };
  if (wood.length) {
    const g = new THREE.CylinderGeometry(0.05, 0.07, 0.8, 5); g.rotateZ(Math.PI/2); g.translate(0,0.06,0);
    const m = new THREE.MeshStandardMaterial({ color: 0x785023, roughness: 1 });
    out.wood = new THREE.InstancedMesh(g, m, wood.length);
    wood.forEach(([r], i) => out.wood.setMatrixAt(i, mat4(W(r[0], r[1], r[2]), 1, 0)));
    out.wood.instanceMatrix.needsUpdate = true; grp.add(out.wood);
  }
  if (stone.length) {
    const g = new THREE.BoxGeometry(0.45, 0.30, 0.45); g.translate(0,0.18,0);
    const m = new THREE.MeshStandardMaterial({ color: 0xa0a0a0, roughness: 1 });
    out.stone = new THREE.InstancedMesh(g, m, stone.length);
    stone.forEach(([r], i) => out.stone.setMatrixAt(i, mat4(W(r[0], r[1], r[2]), 1, 0)));
    out.stone.instanceMatrix.needsUpdate = true; grp.add(out.stone);
  }
  if (grass.length) {
    const g = new THREE.ConeGeometry(0.10, 0.45, 5); g.translate(0,0.22,0);
    const m = new THREE.MeshStandardMaterial({ color: 0x50b450, roughness: 1 });
    out.grass = new THREE.InstancedMesh(g, m, grass.length);
    grass.forEach(([r], i) => out.grass.setMatrixAt(i, mat4(W(r[0], r[1], r[2]), 1, 0)));
    out.grass.instanceMatrix.needsUpdate = true; grp.add(out.grass);
  }
  // map global resource index -> {mesh, localIndex, basePos} so rgone can hide
  resInst = { groups: out, index: new Array(items.length) };
  const ptr = { wood:0, stone:0, grass:0 };
  items.forEach((r, n) => {
    const k = r[3]; const key = k===0?'wood':k===1?'stone':'grass';
    resInst.index[n] = { key, li: ptr[key]++, pos: W(r[0], r[1], r[2]) };
  });
  return reg('resources', grp);
}
buildResources();

// ---------------------------------------------------------------- grass carpet
// instanced grass-blade tufts scattered across grassland (Plain/Forest/Swamp) so
// the ground reads as real stubble, not flat painted green. Skips mountain/river/
// desert. Seated on the (post-ruggedize/carve) surface so blades hug the bumps.
function grassBladeCluster() {
  const geos = []; const n = 5;
  for (let i = 0; i < n; i++) {
    const a = (i / n) * Math.PI * 2 + 0.6, lean = 0.40 + (i % 2) * 0.18;
    const h = 0.26 + (i % 3) * 0.08;
    const g = new THREE.BoxGeometry(0.03, h, 0.03); g.translate(0, h / 2, 0);
    g.rotateX(Math.sin(a) * lean); g.rotateZ(-Math.cos(a) * lean);
    g.translate(Math.cos(a) * 0.05, 0, Math.sin(a) * 0.05);
    geos.push(g);
  }
  const c = new THREE.BoxGeometry(0.035, 0.34, 0.035); c.translate(0, 0.17, 0); geos.push(c);
  return mergeGeometries(geos);
}
function buildGrass() {
  const grp = new THREE.Group();
  const gMat = new THREE.MeshStandardMaterial({ roughness: 1.0, metalness: 0.0, flatShading: true });
  const GN = 34000;            // density matched to PE (12k over 600m) on DP's 1020m map
  const im = new THREE.InstancedMesh(grassBladeCluster(), gMat, GN);
  const col = new THREE.Color();
  const m = new THREE.Matrix4(), q = new THREE.Quaternion(), sc = new THREE.Vector3();
  const yAxis = new THREE.Vector3(0, 1, 0);
  let gi = 0, tries = 0;
  while (gi < GN && tries < GN * 8) {
    tries++;
    const wx = Math.random() * WSX, wy = Math.random() * WSY;
    const b = biomeAt(wx, wy);
    if (b !== 0 && b !== 1 && b !== 4) continue;     // grassland: Plain / Forest / Swamp
    const cci = Math.floor(wy / CS) * GX + Math.floor(wx / CS);
    if (caveRock[cci] > 0.3) continue;               // bare rock on cave knolls, no grass
    const s = 0.6 + Math.random() * 0.7;
    m.compose(W(wx, wy, surfaceH(wx, wy)), q.setFromAxisAngle(yAxis, Math.random() * 6.283),
              sc.set(s, s * (0.7 + Math.random() * 0.7), s));
    im.setMatrixAt(gi, m);
    const t = 0.55 + Math.random() * 0.45;
    col.setRGB(0.16 * t + 0.04, 0.38 * t + 0.10, 0.13 * t + 0.03);
    im.setColorAt(gi, col);
    gi++;
  }
  im.count = gi; im.instanceMatrix.needsUpdate = true;
  if (im.instanceColor) im.instanceColor.needsUpdate = true;
  im.castShadow = false;
  grp.add(im);
  return reg('grass', grp);
}
buildGrass();

// ---------------------------------------------------------------- caves
// Caves are a thermal + predator refuge in the sim (the agent shelters inside).
// Each cave is rendered as ONE coherent low-poly rock mound (a craggy displaced
// dome — a single solid mass, not scattered boulders) seated on the raised knoll.
// The near (south, camera-facing) face has a pocket scooped into it, and a dark
// tunnel recedes back into the hill through that pocket -> a real arched mouth at
// ground level that you could walk into, with genuine depth (not a flat black
// disc lying on top). Front of the world = -Y (scene -Z), so the mouth opens that
// way. cave footprint: [cxw, cyw, floorZ, ceilZ, hx, hy].
function buildCaves() {
  const grp = new THREE.Group();
  const caves = WORLD.caves || [];
  if (!caves.length) return reg('caves', grp);
  // cheap 3d value noise for craggy rock displacement
  const h3 = (x, y, z) => { const n = Math.sin(x*12.9898 + y*78.233 + z*37.719) * 43758.5453; return n - Math.floor(n); };
  const vn3 = (x, y, z) => {
    const xi = Math.floor(x), yi = Math.floor(y), zi = Math.floor(z);
    const xf = x-xi, yf = y-yi, zf = z-zi;
    const u = xf*xf*(3-2*xf), v = yf*yf*(3-2*yf), w = zf*zf*(3-2*zf);
    const L = (a,b,t)=>a+(b-a)*t;
    const c000=h3(xi,yi,zi),     c100=h3(xi+1,yi,zi);
    const c010=h3(xi,yi+1,zi),   c110=h3(xi+1,yi+1,zi);
    const c001=h3(xi,yi,zi+1),   c101=h3(xi+1,yi,zi+1);
    const c011=h3(xi,yi+1,zi+1), c111=h3(xi+1,yi+1,zi+1);
    return L(L(L(c000,c100,u),L(c010,c110,u),v), L(L(c001,c101,u),L(c011,c111,u),v), w);
  };
  const upY = new THREE.Vector3(0, 1, 0), backZ = new THREE.Vector3(0, 0, 1);
  caves.forEach((c, ci) => {
    const [cxw, cyw, fz, cz, hx, hy] = c;
    const span = Math.max(hx, hy);
    const Rd = Math.max(24, span * 2.3);          // mound radius (m)
    const baseY = surfaceH(cxw, cyw);             // scene height at cave centre (on the knoll)
    // ---- one solid craggy rock mound ----
    // a HEMISPHERE (widest at the base) so it seats flush on the knoll with no
    // overhang/mushroom, unlike a full sphere whose mid-bulge floats over the slope.
    const geo = new THREE.SphereGeometry(Rd, 30, 18, 0, Math.PI * 2, 0, Math.PI * 0.5);
    const pos = geo.attributes.position;
    const col = new Float32Array(pos.count * 3);
    const cc = new THREE.Color(), rA = new THREE.Color(0xc7c1b5), rB = new THREE.Color(0x8a8377);
    const openR = Math.max(7.0, span * 0.55);     // entrance half-size (m)
    const ocy = Rd * 0.05;                         // opening centre height (local, just above the base)
    for (let k = 0; k < pos.count; k++) {
      let x = pos.getX(k), y = pos.getY(k), z = pos.getZ(k);
      const n = vn3(x*0.10 + 11, y*0.10 + 4, z*0.10 + 7);
      const disp = 1 + (n - 0.5) * 0.30;          // craggy surface
      x *= disp; z *= disp; y *= disp * 1.02;     // craggy, full-height dome
      // scoop a pocket into the front (-Z) face around the opening
      const dop = Math.hypot(x, (y - ocy) * 1.05, z + Rd);
      let pocket = 0;
      if (z < 0 && dop < openR * 1.9) {
        pocket = 1 - dop / (openR * 1.9);
        z += openR * 1.55 * pocket * pocket;       // pull face deep inward -> real concavity
        x *= (1 - 0.40 * pocket);
      }
      pos.setXYZ(k, x, Math.max(0, y), z);
      cc.copy(rA).lerp(rB, n * 0.7 + pocket * 0.3);  // darker inside the pocket
      col[k*3] = cc.r; col[k*3+1] = cc.g; col[k*3+2] = cc.b;
    }
    geo.setAttribute('color', new THREE.BufferAttribute(col, 3));
    geo.computeVertexNormals();
    const mound = new THREE.Mesh(geo, new THREE.MeshStandardMaterial({ vertexColors: true, roughness: 1.0, metalness: 0.0, flatShading: true, emissive: 0x3b382f, emissiveIntensity: 0.6 }));
    const seatY = baseY - 2.0;                     // base sits just into the knoll top
    mound.position.copy(W(cxw, cyw, seatY));
    grp.add(mound);
    // ---- dark tunnel receding into the hill through the pocket ----
    const tunLen = Rd * 1.5;
    const tun = new THREE.Mesh(
      new THREE.ConeGeometry(openR, tunLen, 24, 1, true),
      new THREE.MeshStandardMaterial({ color: 0x050507, roughness: 1.0, metalness: 0.0, side: THREE.DoubleSide }));
    tun.quaternion.setFromUnitVectors(upY, backZ);   // apex points into hill (+Z), open base faces front (-Z)
    tun.scale.set(0.9, 1.0, 1.3);                    // upright, door-ish opening; long into the hill
    const openWY = cyw - Rd * 1.02;                  // mouth flush with the front face
    const openSY = seatY + openR * 0.78;             // opening centre scene height (bottom ~ground)
    tun.position.copy(W(cxw, openWY + tunLen * 0.5, openSY));
    grp.add(tun);
    // a flat dark disc deep in the throat so the tunnel never shows sky through the tip
    const cap = new THREE.Mesh(new THREE.CircleGeometry(openR * 0.6, 20),
      new THREE.MeshBasicMaterial({ color: 0x000000 }));
    cap.position.copy(W(cxw, openWY + tunLen * 0.92, openSY));
    cap.lookAt(W(cxw, openWY, openSY));
    grp.add(cap);
  });
  return reg('caves', grp);
}
buildCaves();

// ---------------------------------------------------------------- animals
// kinds: 0 Rabbit,1 Deer,2 Wolf,3 Fish,4 Eagle. One InstancedMesh per kind.
const ANIMAL_DEAD_STATE = 5;   // AnimalState::Dead (skip)
const animalKindMeshes = [];
const animalBuckets = [];      // animalBuckets[kind] = [globalAnimalIndex,...]
function makeRabbit() {
  const body = new THREE.BoxGeometry(0.35, 0.30, 0.35); body.translate(0,0.15,0);
  const ear  = new THREE.BoxGeometry(0.10, 0.30, 0.10); ear.translate(0,0.45,0);
  return mergeBoxes([[body],[ear]]);
}
function mergeBoxes(parts) {
  // simple manual merge of BoxGeometry list into one BufferGeometry
  const geos = parts.map(p => p[0]);
  return mergeGeometries(geos);
}
function mergeGeometries(geos) {
  let total = 0; for (const g of geos) total += g.attributes.position.count;
  const pos = new Float32Array(total*3), norm = new Float32Array(total*3);
  let o = 0;
  const idx = []; let base = 0;
  for (const g of geos) {
    const p = g.attributes.position, nrm = g.attributes.normal;
    for (let i=0;i<p.count;i++){ pos[o*3]=p.getX(i);pos[o*3+1]=p.getY(i);pos[o*3+2]=p.getZ(i);
      norm[o*3]=nrm.getX(i);norm[o*3+1]=nrm.getY(i);norm[o*3+2]=nrm.getZ(i); o++; }
    const gi = g.index ? g.index.array : null;
    if (gi) for (let i=0;i<gi.length;i++) idx.push(gi[i]+base);
    else for (let i=0;i<p.count;i++) idx.push(base+i);
    base += p.count;
  }
  const out = new THREE.BufferGeometry();
  out.setAttribute('position', new THREE.BufferAttribute(pos,3));
  out.setAttribute('normal', new THREE.BufferAttribute(norm,3));
  out.setIndex(idx);
  return out;
}
function makeDeer() {
  const body = new THREE.BoxGeometry(0.7, 0.5, 1.0); body.translate(0,0.55,0);
  const head = new THREE.BoxGeometry(0.4, 0.4, 0.4); head.translate(0,0.9,0.55);
  const ant  = new THREE.BoxGeometry(0.08,0.3,0.08); ant.translate(0,1.25,0.55);
  return mergeGeometries([body, head, ant]);
}
function makeWolf() {
  const body = new THREE.BoxGeometry(0.55, 0.4, 1.1); body.translate(0,0.4,0);
  const head = new THREE.BoxGeometry(0.35,0.35,0.35); head.translate(0,0.55,0.6);
  return mergeGeometries([body, head]);
}
function makeFish() { const g = new THREE.BoxGeometry(0.18,0.18,0.4); g.translate(0,0.1,0); return g; }
function makeEagle() {
  const wing = new THREE.BoxGeometry(1.6,0.05,0.2);
  const body = new THREE.BoxGeometry(0.3,0.15,0.3);
  return mergeGeometries([wing, body]);
}
const ANIMAL_GEO = [makeRabbit, makeDeer, makeWolf, makeFish, makeEagle];
const ANIMAL_COL = [0xf0f0f0, 0xa06e3c, 0x5a5a64, 0x78b4dc, 0x3c3228];
const WOLF_HUNT_COL = new THREE.Color(0xdc3c3c);
function buildAnimals() {
  const grp = new THREE.Group();
  const f0 = FRAMES[0].an;
  for (let k=0;k<5;k++) animalBuckets[k] = [];
  f0.forEach((a, i) => { const k=a[0]; if (k>=0&&k<5) animalBuckets[k].push(i); });
  for (let k=0;k<5;k++) {
    const n = animalBuckets[k].length; if (!n) { animalKindMeshes[k]=null; continue; }
    const geo = ANIMAL_GEO[k]();
    const mat = new THREE.MeshStandardMaterial({ color: ANIMAL_COL[k], roughness: 0.85 });
    const inst = new THREE.InstancedMesh(geo, mat, n);
    if (k===2) { // wolf: per-instance colour for hunt state
      inst.instanceColor = new THREE.InstancedBufferAttribute(new Float32Array(n*3), 3);
      const c = new THREE.Color(ANIMAL_COL[2]); for (let q=0;q<n;q++) inst.setColorAt(q,c);
    }
    inst.frustumCulled = false;
    animalKindMeshes[k] = inst; grp.add(inst);
  }
  return reg('animals', grp);
}
buildAnimals();

// ---------------------------------------------------------------- agent (protagonist)
let agentGroup;
function buildAgent() {
  const grp = new THREE.Group();
  const bodyM = new THREE.MeshStandardMaterial({ color: 0x2bd1c4, roughness: 0.5, metalness: 0.1, emissive: 0x0a3b38, emissiveIntensity: 0.5 });
  const headM = new THREE.MeshStandardMaterial({ color: 0xffe0b0, roughness: 0.6 });
  const body = new THREE.Mesh(new THREE.CapsuleGeometry(0.45, 1.0, 6, 12), bodyM); body.position.y = 1.0;
  const head = new THREE.Mesh(new THREE.SphereGeometry(0.35, 16, 12), headM);     head.position.y = 1.95;
  // facing indicator
  const nose = new THREE.Mesh(new THREE.ConeGeometry(0.12, 0.3, 8), headM); nose.position.set(0,1.95,0.35); nose.rotation.x = Math.PI/2;
  grp.add(body, head, nose);
  // (locator ring is a separate terrain-conforming mesh, built below — not a flat
  //  child disc, which on slopes gets buried by the ground.)
  agentGroup = grp;
  return reg('agent', grp);
}
buildAgent();

// ---- terrain-conforming locator ring --------------------------------------
// A flat ring parented to the agent gets occluded on any slope. Instead this is
// an independent band whose vertices are re-projected onto the heightfield every
// frame, so it hugs hills/valleys and (depthWrite off + renderOrder) is never
// swallowed by the terrain it sits on.
const RING_SEG = 64, RING_RIN = 1.35, RING_ROUT = 1.7, RING_OFF = 0.12;
const ringGeo = new THREE.BufferGeometry();
const ringPos = new Float32Array((RING_SEG + 1) * 2 * 3);
ringGeo.setAttribute('position', new THREE.BufferAttribute(ringPos, 3));
{ const idx = [];
  for (let s = 0; s < RING_SEG; s++) {
    const a = s*2, b = s*2+1, c = (s+1)*2, d = (s+1)*2+1;
    idx.push(a, b, c, b, d, c);
  }
  ringGeo.setIndex(idx);
}
const agentRing = new THREE.Mesh(ringGeo, new THREE.MeshBasicMaterial({
  color: 0x2bd1c4, transparent: true, opacity: 0.8, side: THREE.DoubleSide,
  depthWrite: false }));
agentRing.renderOrder = 5;          // draw over terrain so a hillside can't hide it
reg('agent', agentRing);            // toggles with the Protagonist layer
function updateRing(ax, ay) {
  for (let s = 0; s <= RING_SEG; s++) {
    const th = s / RING_SEG * Math.PI * 2, c = Math.cos(th), sn = Math.sin(th);
    for (let e = 0; e < 2; e++) {
      const r = e ? RING_ROUT : RING_RIN;
      const sx = ax + c*r, sy = ay + sn*r;
      const w = W(sx, sy, surfaceH(sx, sy) + RING_OFF);
      const vi = (s*2 + e) * 3;
      ringPos[vi] = w.x; ringPos[vi+1] = w.y; ringPos[vi+2] = w.z;
    }
  }
  ringGeo.attributes.position.needsUpdate = true;
  ringGeo.computeBoundingSphere();
}

// beacon column above the agent (kept on its own toggle with agent)
const beacon = new THREE.Mesh(new THREE.CylinderGeometry(0.08,0.08,40,6),
  new THREE.MeshBasicMaterial({ color: 0x2bd1c4, transparent:true, opacity:0.22 }));
agentGroup.add(beacon); beacon.position.y = 20;

// ---------------------------------------------------------------- fire / buildings / farm / shelter (dynamic groups)
const fireGroup  = reg('fire',  new THREE.Group());
const buildGroup = reg('builds', new THREE.Group());
const farmGroup  = reg('farm',  new THREE.Group());
const firePool = []; // {sprite, light}
const FIRE_MAX = 8;
const fireTex = (() => {
  const c = document.createElement('canvas'); c.width=c.height=64; const g=c.getContext('2d');
  const grd = g.createRadialGradient(32,32,2,32,32,30);
  grd.addColorStop(0,'rgba(255,240,180,1)'); grd.addColorStop(0.4,'rgba(255,150,40,0.9)');
  grd.addColorStop(1,'rgba(255,80,0,0)'); g.fillStyle=grd; g.fillRect(0,0,64,64);
  const t = new THREE.CanvasTexture(c); return t;
})();
for (let i=0;i<FIRE_MAX;i++) {
  const sprite = new THREE.Sprite(new THREE.SpriteMaterial({ map: fireTex, transparent:true, depthWrite:false, blending: THREE.AdditiveBlending }));
  sprite.scale.set(2.5,3.0,1); sprite.visible=false;
  const light = new THREE.PointLight(0xff8030, 0, 40, 2); light.visible=false;
  fireGroup.add(sprite); fireGroup.add(light);
  firePool.push({ sprite, light });
}

const BUILD_COL = [0xb48246, 0xa06e3c, 0xa0a0a5, 0xaa9678];
const BUILD_FOOT = [1.5, 2.0, 2.0, 2.8];

// ---------------------------------------------------------------- HUD refs
const elClock=$('clock'), elTick=$('tickNum'), elPhase=$('phaseChip').querySelector('b'),
      elAnim=$('animalCount'), elCave=$('caveChip').querySelector('b'),
      vH=$('vHunger'),vT=$('vThirst'),vS=$('vStamina'),vP=$('vProtein'),vV=$('vVitamin'),
      deadMsg=$('deadMsg');

// ---------------------------------------------------------------- playback state
let playing = true, speed = 1, playhead = 0; // playhead in frame units (float)
let dayNightOn = true;
const NF = FRAMES.length;
const seek = $('seek'); seek.max = String(NF-1);
const tlabel = $('tlabel');

$('play').onclick = () => { playing = !playing; $('play').textContent = playing ? '⏸ Pause' : '▶ Play'; };
seek.oninput = () => { playhead = +seek.value; playing = false; $('play').textContent='▶ Play'; applyFrame(playhead); };
document.querySelectorAll('.speeds button').forEach(b => b.onclick = () => {
  speed = +b.dataset.spd; document.querySelectorAll('.speeds button').forEach(x=>x.classList.remove('on')); b.classList.add('on');
});
document.querySelectorAll('#layers input').forEach(inp => inp.onchange = () => {
  if (inp.dataset.layer === 'daynight') { dayNightOn = inp.checked; return; }
  const arr = LAYERS[inp.dataset.layer]; if (arr) arr.forEach(o => o.visible = inp.checked);
});

// ---------------------------------------------------------------- per-frame apply
const _m = new THREE.Matrix4(), _p = new THREE.Vector3(), _hide = mat4(new THREE.Vector3(0,-9999,0), 0.0001, 0);
const _wolfHunt = new THREE.Color(0xdc3c3c), _wolfCalm = new THREE.Color(ANIMAL_COL[2]);
let lastResGone = new Set(), lastPlantGone = new Set();

function lerp(a,b,t){ return a+(b-a)*t; }
function angLerp(a,b,t){ let d=b-a; while(d>Math.PI)d-=2*Math.PI; while(d<-Math.PI)d+=2*Math.PI; return a+d*t; }

function applyFrame(ph) {
  const i0 = Math.min(NF-1, Math.floor(ph));
  const i1 = Math.min(NF-1, i0+1);
  const t = ph - i0;
  const fa = FRAMES[i0], fb = FRAMES[i1];

  // ---- day / night ----
  const sunH = lerp(fa.sun, fb.sun, t);       // -1..1
  const tod  = lerp(fa.t, fb.t, t);           // 0..1
  if (dayNightOn) {
    const day = Math.max(0, sunH);
    sun.intensity = 0.35 + 1.4*day;
    sun.color.setHSL(0.10 + 0.04*day, 0.6, 0.55 + 0.15*day);
    hemi.intensity = 0.40 + 0.5*day;
    ambient.intensity = 0.28 + 0.18*day;
    // sun arc across the sky from time-of-day
    const ang = (tod - 0.25) * Math.PI*2;
    sun.position.copy(W(cx + Math.cos(ang)*600, cy + 200, ch + Math.max(40,Math.sin(ang)*700)));
    const dayCol = new THREE.Color(0x9ec7ff), duskCol = new THREE.Color(0x20283c);
    const sky = duskCol.clone().lerp(dayCol, Math.max(0, Math.min(1, day*1.4)));
    scene.background = sky; scene.fog.color = sky;
    renderer.toneMappingExposure = 0.75 + 0.45*day;
  }

  // ---- agent ----
  const a0 = fa.ag, a1 = fb.ag;
  const ax = lerp(a0[0],a1[0],t), ay = lerp(a0[1],a1[1],t), az = lerp(a0[2],a1[2],t);
  const ayaw = angLerp(a0[3],a1[3],t);
  // agent z is reported ~5m above terrain (eye height); ground it to the
  // surface so the model's feet touch the ground (use raw z inside caves).
  const groundZ = (a0[11] === 1) ? az : surfaceH(ax, ay);
  agentGroup.position.copy(W(ax, ay, groundZ));
  agentGroup.rotation.y = -ayaw;
  updateRing(ax, ay);
  const alive = a0[10] === 1;
  agentGroup.visible = (LAYERS.agent[0]?.parent ? true : true);
  deadMsg.style.display = alive ? 'none' : 'block';
  // vitals (0..100)
  const setBar = (el, val, hue) => { el.style.width = Math.max(0,Math.min(100,val)).toFixed(0)+'%'; };
  setBar(vH, a0[4]); setBar(vT, a0[5]); setBar(vS, a0[6]); setBar(vP, a0[8]); setBar(vV, a0[9]);

  // ---- animals ----
  for (let k=0;k<5;k++) {
    const inst = animalKindMeshes[k]; if (!inst) continue;
    const bucket = animalBuckets[k];
    for (let q=0;q<bucket.length;q++) {
      const gi = bucket[q];
      const an0 = fa.an[gi], an1 = (fb.an && fb.an[gi]) ? fb.an[gi] : an0;
      if (!an0) { inst.setMatrixAt(q, _hide); continue; }
      const dead = an0[1] === ANIMAL_DEAD_STATE || an0[6] <= 0;
      if (dead) { inst.setMatrixAt(q, _hide); continue; }
      const x = lerp(an0[2],an1[2],t), y = lerp(an0[3],an1[3],t), z = lerp(an0[4],an1[4],t);
      const yaw = angLerp(an0[5],an1[5],t);
      inst.setMatrixAt(q, mat4(W(x,y,z), 1, -yaw + Math.PI/2));
      if (k===2 && inst.instanceColor) inst.setColorAt(q, an0[1]===3 ? _wolfHunt : _wolfCalm); // state 3 = Hunt
    }
    inst.instanceMatrix.needsUpdate = true;
    if (inst.instanceColor) inst.instanceColor.needsUpdate = true;
  }

  // ---- resources hide-on-collected (rgone) ----
  const rgone = new Set(fa.rgone);
  if (rgone.size || lastResGone.size) {
    // restore previously hidden that are now back (rare) + hide newly gone
    const touch = new Set([...rgone, ...lastResGone]);
    for (const gi of touch) {
      const info = resInst.index[gi]; if (!info) continue;
      const mesh = resInst.groups[info.key]; if (!mesh) continue;
      mesh.setMatrixAt(info.li, rgone.has(gi) ? _hide : mat4(info.pos, 1, 0));
      mesh.instanceMatrix.needsUpdate = true;
    }
    lastResGone = rgone;
  }
  // ---- plants hide-on-unripe (pgone) ----
  const pgone = new Set(fa.pgone);
  if (pgone.size || lastPlantGone.size) {
    const touch = new Set([...pgone, ...lastPlantGone]);
    for (const gi of touch) {
      const p = WORLD.plants[gi]; if (!p) continue; const r = plantScale[gi];
      plantInst.setMatrixAt(gi, pgone.has(gi) ? _hide : mat4(W(p[0],p[1],p[2]+r), r, 0));
    }
    plantInst.instanceMatrix.needsUpdate = true;
    lastPlantGone = pgone;
  }

  // ---- campfires ----
  const fires = fa.fire || [];
  for (let i=0;i<FIRE_MAX;i++) {
    const f = fires[i], slot = firePool[i];
    if (f && f[4] === 1) {
      const p = W(f[0], f[1], f[2]+1.2);
      slot.sprite.position.copy(p); slot.sprite.visible = true;
      const flick = 0.85 + 0.3*Math.sin(performance.now()*0.01 + i);
      slot.sprite.scale.set(2.5*flick, 3.2*flick, 1);
      slot.light.position.copy(W(f[0], f[1], f[2]+2)); slot.light.visible = true;
      slot.light.intensity = 6*flick;
    } else { slot.sprite.visible = false; slot.light.visible = false; }
  }

  // ---- buildings + sites ----
  rebuildStructures(fa);

  // ---- HUD ----
  const hh = Math.floor(tod*24), mm = Math.floor((tod*24-hh)*60);
  elClock.textContent = String(hh).padStart(2,'0')+':'+String(mm).padStart(2,'0');
  elTick.textContent = fa.tk;
  elPhase.textContent = sunH > 0.15 ? 'Day' : (sunH > -0.1 ? 'Dusk' : 'Night');
  let nAlive=0; for (const a of fa.an) if (a[1]!==ANIMAL_DEAD_STATE && a[6]>0) nAlive++;
  elAnim.textContent = nAlive;
  elCave.textContent = a0[11] === 1 ? 'yes' : 'no';

  // ---- GLB model drivers (animals + protagonist, progressive enhancement) ----
  for (let h = 0; h < frameHooks.length; h++) frameHooks[h](fa, fb, t);
}

let structKey = '';
function rebuildStructures(f) {
  const key = JSON.stringify(f.site)+JSON.stringify(f.bld)+JSON.stringify(f.farm)+JSON.stringify(f.sh);
  if (key === structKey) return; structKey = key;
  for (const g of [buildGroup, farmGroup]) { while(g.children.length) { const c=g.children.pop(); c.geometry?.dispose(); } }
  // sites: [type,x,y,z,progress,completed]
  for (const s of (f.site||[])) {
    const [ty,x,y,z,prog] = s; const w = (BUILD_FOOT[ty]||2)*2;
    const foot = new THREE.Mesh(new THREE.BoxGeometry(w,0.1,w), new THREE.MeshStandardMaterial({color:0xcccccc, transparent:true, opacity:0.4}));
    foot.position.copy(W(x,y,z+0.05)); buildGroup.add(foot);
    if (prog >= 0.5) {
      const fullH=[1.6,2.6,2.6,3.4][ty]||2.4; const ch2=fullH*Math.min(1,(prog-0.5)/0.5);
      if (ch2>0.05){ const wall=new THREE.Mesh(new THREE.BoxGeometry(w*0.95,ch2,w*0.95), new THREE.MeshStandardMaterial({color:BUILD_COL[ty]}));
        wall.position.copy(W(x,y,z+0.35+ch2*0.5)); buildGroup.add(wall); }
    }
  }
  // finished buildings: [type,x,y,z,coverage]
  for (const b of (f.bld||[])) {
    const [ty,x,y,z] = b; const w=(BUILD_FOOT[ty]||2)*2; const h=[1.6,2.6,2.6,3.4][ty]||2.4;
    const box=new THREE.Mesh(new THREE.BoxGeometry(w,h,w), new THREE.MeshStandardMaterial({color:BUILD_COL[ty], roughness:0.9}));
    box.position.copy(W(x,y,z+h*0.5)); buildGroup.add(box);
    const roof=new THREE.Mesh(new THREE.ConeGeometry(w*0.8,h*0.5,4), new THREE.MeshStandardMaterial({color:0x803020}));
    roof.position.copy(W(x,y,z+h+ h*0.25)); roof.rotation.y=Math.PI/4; buildGroup.add(roof);
  }
  // farm plots: [x,y,z,kind,growth,water,ripe]
  for (const p of (f.farm||[])) {
    const [x,y,z,kind,growth,,ripe] = p;
    const soil=new THREE.Mesh(new THREE.BoxGeometry(1.4,0.12,1.4), new THREE.MeshStandardMaterial({color:0x5a3c28}));
    soil.position.copy(W(x,y,z+0.06)); farmGroup.add(soil);
    if (growth>0.05){ const ch=0.1+0.6*growth;
      const crop=new THREE.Mesh(new THREE.ConeGeometry(0.3,ch,6), new THREE.MeshStandardMaterial({color: ripe?0xe0c040:0x60b050}));
      crop.position.copy(W(x,y,z+0.12+ch*0.5)); farmGroup.add(crop); }
  }
  // shelter: [x,y,z,r]
  while(LAYERS.builds && false){break;}
  if (f.sh && f.sh.length===4){ const [x,y,z,r]=f.sh;
    const dome=new THREE.Mesh(new THREE.SphereGeometry(r,16,10,0,Math.PI*2,0,Math.PI/2), new THREE.MeshStandardMaterial({color:0x8a6a4a, transparent:true, opacity:0.55}));
    dome.position.copy(W(x,y,z)); buildGroup.add(dome); }
}

// ---------------------------------------------------------------- real GLB models
// Progressive enhancement: the scene already renders with low-poly geometry so it
// works instantly and never breaks. Here we load real animated GLB models
// (Quaternius) and, once each is ready, swap it in for the protagonist + animals.
// Skinned meshes MUST be cloned with SkeletonUtils.clone — a plain .clone() shares
// one skeleton across all copies and collapses the mesh (the invisible-model bug).
const gltfLoader = new GLTFLoader();
const _clock = new THREE.Clock();
const _allMixers = [];
function loadGLB(url){ return new Promise((res,rej)=> gltfLoader.load(url, g=>res(g), undefined, rej)); }
// scale a model so its largest dimension == targetH metres, then drop feet to y=0.
// Measure ONLY mesh geometry after updating world matrices: skinned meshes carry
// their real size in node scales (not the raw verts), so Box3.setFromObject on an
// un-posed clone collapses to a point. This mirrors the working gallery sizing.
function meshBox(o){
  o.updateWorldMatrix(true, true);
  const b = new THREE.Box3();
  o.traverse(m=>{ if(m.isMesh){ m.geometry.computeBoundingBox(); const gb=m.geometry.boundingBox.clone(); gb.applyMatrix4(m.matrixWorld); b.union(gb); } });
  return b;
}
function fitModel(obj, targetH){
  const sz = new THREE.Vector3(); meshBox(obj).getSize(sz);
  const s = targetH / (Math.max(sz.x, sz.y, sz.z) || 1); obj.scale.setScalar(s);
  obj.position.y -= meshBox(obj).min.y;
}
function pickClip(clips, prefer){
  for (const want of prefer){ const c = clips.find(k=>k.name.toLowerCase().includes(want)); if (c) return c; }
  return clips[0] || null;
}

// animal kinds: 0 Rabbit,1 Deer,2 Wolf,3 Fish,4 Eagle.
// Eagle has no GLB in the pack -> keeps its low-poly glider geometry.
const ANIMAL_GLB = [
  { url:'./assets/Animated_Animal_Pack/y4wdQpg767_y4wdQpg767.glb', h:0.6, rot:0, clip:['gallop','walk','idle'] }, // rabbit -> small dog (ShibaInu)
  { url:'./assets/Animated_Animal_Pack/T6Cs7tmMHJ_T6Cs7tmMHJ.glb', h:1.7, rot:0, clip:['gallop','walk','idle'] }, // deer
  { url:'./assets/Animated_Animal_Pack/P1gU3Qkr9r_P1gU3Qkr9r.glb', h:1.1, rot:0, clip:['gallop','walk','idle'] }, // wolf
  { url:'./assets/Animated_Fish_Bundle/1tycAcvSVW_1tycAcvSVW.glb', h:0.7, rot:0, clip:['swim','idle'] },          // fish -> Tuna
  null,
];
const animalGLBGroup = reg('animals', new THREE.Group());
const animalGLB = [];   // animalGLB[k][q] = {pivot, mixer} aligned to animalBuckets[k]
async function buildAnimalGLB(){
  for (let k=0;k<5;k++){
    const spec = ANIMAL_GLB[k], bucket = animalBuckets[k];
    if (!spec || !bucket || !bucket.length) continue;
    let gltf; try { gltf = await loadGLB(spec.url); } catch(e){ console.warn('GLB load failed', spec.url, e); continue; }
    const clip = pickClip(gltf.animations||[], spec.clip);
    const arr = [];
    for (let q=0;q<bucket.length;q++){
      const obj = skinnedClone(gltf.scene);
      obj.traverse(o=>{ if(o.isMesh){ o.frustumCulled=false; o.castShadow=false; } });
      fitModel(obj, spec.h);
      const pivot = new THREE.Group(); pivot.add(obj); pivot.userData.rot = spec.rot;
      animalGLBGroup.add(pivot);
      let mixer=null;
      if (clip){ mixer=new THREE.AnimationMixer(obj); const a=mixer.clipAction(clip); a.play(); a.time=Math.random()*clip.duration; _allMixers.push(mixer); }
      arr.push({ pivot, mixer });
    }
    animalGLB[k] = arr;
    if (animalKindMeshes[k]) animalKindMeshes[k].visible = false;   // retire low-poly for this kind
  }
}
frameHooks.push((fa, fb, t)=>{
  const dt = _clock.getDelta();
  for (let i=0;i<_allMixers.length;i++) _allMixers[i].update(dt);
  for (let k=0;k<5;k++){
    const arr = animalGLB[k]; if (!arr) continue; const bucket = animalBuckets[k];
    for (let q=0;q<bucket.length;q++){
      const ent = arr[q]; if (!ent) continue; const gi = bucket[q];
      const an0 = fa.an[gi], an1 = (fb.an && fb.an[gi]) ? fb.an[gi] : an0;
      const dead = !an0 || an0[1]===ANIMAL_DEAD_STATE || an0[6]<=0;
      if (dead){ ent.pivot.visible=false; continue; }
      ent.pivot.visible = true;
      const x=lerp(an0[2],an1[2],t), y=lerp(an0[3],an1[3],t), z=lerp(an0[4],an1[4],t);
      const yaw=angLerp(an0[5],an1[5],t);
      ent.pivot.position.copy(W(x,y,z));
      ent.pivot.rotation.y = -yaw + Math.PI/2 + ent.pivot.userData.rot;
    }
  }
});
// ---- protagonist action visualisation: clip + held tool + floating label ----
// BaseHuman ships 11 clips (Idle/Walk/Run/Punch/SwordSlash/Sitting/Clapping/
// Jump/Standing/Death...). There is no bespoke "fish"/"sew"/"plant" clip, so
// each of the 18 sim actions maps to the best-matching body clip PLUS a
// distinct hand-held prop (spear/rod/axe/hoe/bucket/torch/...), and a label
// names the action. The discrete action id is emitted per frame as fa.act.
const PROP_MAT = {
  wood : new THREE.MeshStandardMaterial({color:0x6b4a2b, roughness:0.95}),
  metal: new THREE.MeshStandardMaterial({color:0x9aa3ab, roughness:0.4, metalness:0.7}),
  green: new THREE.MeshStandardMaterial({color:0x4f9e3a, roughness:0.9}),
  fur  : new THREE.MeshStandardMaterial({color:0x8a6a44, roughness:1}),
  food : new THREE.MeshStandardMaterial({color:0xff5522, roughness:0.6}),
  line : new THREE.MeshBasicMaterial({color:0xffffff}),
  flame: new THREE.MeshBasicMaterial({color:0xff9020}),
};
function makeProp(kind){
  const g=new THREE.Group(), M=PROP_MAT;
  const cyl=(rt,rb,h,m)=>new THREE.Mesh(new THREE.CylinderGeometry(rt,rb,h,8),m);
  const box=(x,y,z,m)=>new THREE.Mesh(new THREE.BoxGeometry(x,y,z),m);
  if(kind==='spear'){ const s=cyl(0.022,0.022,1.6,M.wood); s.position.y=0.45; const t=new THREE.Mesh(new THREE.ConeGeometry(0.055,0.22,8),M.metal); t.position.y=1.35; g.add(s,t); }
  else if(kind==='rod'){ const r=cyl(0.01,0.022,1.5,M.wood); r.position.y=0.55; r.rotation.z=-0.45; const ln=cyl(0.004,0.004,1.0,M.line); ln.position.set(0.62,0.5,0); g.add(r,ln); }
  else if(kind==='axe'){ const h=cyl(0.026,0.026,0.95,M.wood); h.position.y=0.25; const hd=box(0.26,0.16,0.05,M.metal); hd.position.set(0.09,0.66,0); g.add(h,hd); }
  else if(kind==='hoe'){ const h=cyl(0.022,0.022,1.05,M.wood); h.position.y=0.3; const hd=box(0.2,0.06,0.12,M.metal); hd.position.set(0,0.78,0.07); g.add(h,hd); }
  else if(kind==='bucket'){ const b=cyl(0.13,0.1,0.2,M.metal); b.position.y=0.32; const hh=cyl(0.005,0.005,0.26,M.metal); hh.rotation.z=Math.PI/2; hh.position.y=0.45; g.add(b,hh); }
  else if(kind==='torch'){ const s=cyl(0.018,0.018,0.6,M.wood); s.position.y=0.2; const f=new THREE.Mesh(new THREE.SphereGeometry(0.08,8,8),M.flame); f.position.y=0.55; g.add(s,f); }
  else if(kind==='food'){ const b=new THREE.Mesh(new THREE.SphereGeometry(0.07,10,8),M.food); b.position.y=0.05; g.add(b); }
  else if(kind==='grass'){ for(let i=0;i<5;i++){ const bl=new THREE.Mesh(new THREE.ConeGeometry(0.02,0.4,4),M.green); bl.position.set((i-2)*0.035,0.22,0); bl.rotation.z=(i-2)*0.16; g.add(bl);} }
  else if(kind==='fur'){ const p=box(0.32,0.05,0.42,M.fur); p.position.y=0.12; g.add(p); }
  else if(kind==='log'){ const l=cyl(0.06,0.06,0.42,M.wood); l.rotation.z=Math.PI/2; l.position.y=0.12; g.add(l); }
  return g;
}
const ACT = {
  0:{c:'sitting',    p:'food',  l:'Eating'},
  1:{c:'sitting',    p:'rod',   l:'Drinking / Fishing'},
  2:{c:'punch',      p:'spear', l:'Attacking'},
  3:{c:'swordslash', p:'axe',   l:'Gathering'},
  4:{c:'clapping',   p:'log',   l:'Building shelter'},
  5:{c:'clapping',   p:'spear', l:'Crafting spear'},  // hand assembly, not a slash
  6:{c:'sitting',    p:null,    l:'Sleeping'},
  7:{c:'clapping',   p:'grass', l:'Weaving grass dress'},
  8:{c:'clapping',   p:'fur',   l:'Sewing fur cloak'},
  9:{c:'standing',   p:null,    l:'Wearing clothes', wear:1},
  10:{c:'clapping',  p:'log',   l:'Placing blueprint'},
  11:{c:'standing',  p:null,    l:'Switching build type'},
  12:{c:'clapping',  p:'log',   l:'Depositing materials'},
  13:{c:'clapping',  p:'hoe',   l:'Planting seed'},
  14:{c:'clapping',  p:'bucket',l:'Watering crop'},
  15:{c:'clapping',  p:'food',  l:'Feeding / taming'},
  16:{c:null,        p:null,    l:''},
  17:{c:'clapping',  p:'torch', l:'Tending fire'},
};
function makeActionLabel(){
  const cv=document.createElement('canvas'); cv.width=512; cv.height=96;
  const tex=new THREE.CanvasTexture(cv);
  const spr=new THREE.Sprite(new THREE.SpriteMaterial({map:tex, transparent:true, depthTest:false, depthWrite:false}));
  spr.scale.set(6,1.15,1); spr.position.y=3.0; spr.visible=false; spr.userData={cv,tex,cur:'__'};
  return spr;
}
function setLabel(spr,text){
  if(spr.userData.cur===text) return; spr.userData.cur=text;
  if(!text){ spr.visible=false; return; }
  const {cv,tex}=spr.userData, x=cv.getContext('2d');
  x.clearRect(0,0,cv.width,cv.height);
  x.font='bold 46px system-ui,Arial'; x.textAlign='center'; x.textBaseline='middle';
  x.lineWidth=8; x.strokeStyle='rgba(0,0,0,0.85)'; x.strokeText(text,256,48);
  x.fillStyle='#ffe27a'; x.fillText(text,256,48);
  tex.needsUpdate=true; spr.visible=true;
}
let agentMixer=null, agentActions={}, agentCur=null, agentPropHolder=null, agentProps={}, agentLabel=null, agentWorn=false, agentCloak=null;
async function buildAgentGLB(){
  let gltf; try { gltf = await loadGLB('./assets/Animated_Men_Pack/DLptRuewTn_DLptRuewTn.glb'); }
  catch(e){ console.warn('human GLB failed', e); return; }
  const human = skinnedClone(gltf.scene);
  human.traverse(o=>{ if(o.isMesh){ o.frustumCulled=false; } });
  fitModel(human, 1.85);
  const pivot = new THREE.Group(); pivot.add(human); pivot.rotation.y = Math.PI;
  agentGroup.add(pivot);
  agentGroup.children.slice(0,3).forEach(c=>{ if(c!==pivot) c.visible=false; });  // hide capsule/head/nose

  // all clips -> named actions (key = lowercased suffix after 'Man_')
  agentMixer = new THREE.AnimationMixer(human);
  for (const clip of (gltf.animations||[])){
    const key = clip.name.toLowerCase().replace(/^.*man_/,'').replace(/[^a-z]/g,'');
    agentActions[key] = agentMixer.clipAction(clip);
  }
  _allMixers.push(agentMixer);

  // right-hand bone -> prop holder, counter-scaled so props read in true metres
  let bone=null;
  human.traverse(o=>{ if(o.isBone && !bone && /(palm\.?r|hand\.?r|righthand|hand_r|fingers\.?r)/i.test(o.name)) bone=o; });
  if(!bone) human.traverse(o=>{ if(o.isBone && !bone && /hand/i.test(o.name)) bone=o; });
  // torso (for the worn garment) + feet (for per-frame grounding) bones
  let torsoBone=null, footL=null, footR=null;
  human.traverse(o=>{ if(o.isBone){
    if(!torsoBone && /^torso$/i.test(o.name)) torsoBone=o;
    if(!footL && /^footl$/i.test(o.name)) footL=o;
    if(!footR && /^footr$/i.test(o.name)) footR=o;
  }});
  if(!torsoBone) human.traverse(o=>{ if(o.isBone && !torsoBone && /(torso|chest|spine|abdomen)/i.test(o.name)) torsoBone=o; });
  agentPropHolder = new THREE.Group();
  if(bone){
    bone.updateWorldMatrix(true,false);
    const sc=new THREE.Vector3(); bone.matrixWorld.decompose(new THREE.Vector3(), new THREE.Quaternion(), sc);
    agentPropHolder.scale.setScalar(1/(sc.x||1));
    agentPropHolder.rotation.set(0,0,0);  // tuned in-browser: shaft reads cleanly as held-in-hand
    bone.add(agentPropHolder);
  } else {
    agentPropHolder.position.set(0.32,1.15,0.18); pivot.add(agentPropHolder);  // fallback near right hand
  }
  for (const kind of ['spear','rod','axe','hoe','bucket','torch','food','grass','fur','log']){
    const pr=makeProp(kind); pr.visible=false; agentPropHolder.add(pr); agentProps[kind]=pr;
  }
  // garment that appears (and persists) once clothes are worn. A straight-sided
  // low-segment cylinder reads as an ugly frustum/"棱台"; instead build a smooth
  // lathed robe with a curved silhouette + a rolled collar so it looks like cloth.
  {
    const cloth = new THREE.MeshStandardMaterial({ color:0x6b4326, roughness:0.98, metalness:0.0, side:THREE.DoubleSide });
    // A poncho/cape authored in the TORSO bone's local frame: origin (y=0) sits at
    // the mid-chest bone, the collar is just above it and the hem flares down to the
    // hips. Each frame the cloak is snapped onto the torso bone (see hook below) so
    // it is genuinely worn — it lowers/leans with the body when sitting or bending
    // instead of hanging at a fixed height and detaching.
    const prof = [                              // (radius, y-from-torso) collar -> hip hem
      new THREE.Vector2(0.120, 0.22), new THREE.Vector2(0.165, 0.10),
      new THREE.Vector2(0.205,-0.06), new THREE.Vector2(0.245,-0.26),
      new THREE.Vector2(0.275,-0.46),
    ];
    const cape = new THREE.Mesh(new THREE.LatheGeometry(prof, 48), cloth);   // 48 seg = smooth
    const collar = new THREE.Mesh(new THREE.TorusGeometry(0.125, 0.04, 10, 30), cloth);
    collar.rotation.x = Math.PI/2; collar.position.y = 0.22;
    agentCloak = new THREE.Group(); agentCloak.add(cape, collar);
    agentCloak.visible = false; pivot.add(agentCloak);
  }

  agentLabel = makeActionLabel(); agentGroup.add(agentLabel);
  if (agentActions.idle){ agentActions.idle.play(); agentCur='idle'; }

  // per-frame driver: clip + prop + label from fa.act (locomotion when noop)
  frameHooks.push((fa, fb, t)=>{
    if(!agentMixer) return;
    const aliveNow = fa.ag[10]===1;
    const id = (typeof fa.act==='number') ? fa.act : 16;
    const spec = ACT[id] || ACT[16];
    let clipKey = spec.c;
    if(!clipKey){
      const dx=(fb.ag?fb.ag[0]:fa.ag[0])-fa.ag[0], dz=(fb.ag?fb.ag[2]:fa.ag[2])-fa.ag[2];
      const spd=Math.hypot(dx,dz)/((META.stride*META.fixed_dt)||1);
      clipKey = spd>2.4 ? 'run' : (spd>0.25 ? 'walk' : 'idle');
    }
    if(!aliveNow && agentActions.death) clipKey='death';
    if(!agentActions[clipKey]) clipKey = agentActions.idle ? 'idle' : Object.keys(agentActions)[0];
    if(clipKey && clipKey!==agentCur && agentActions[clipKey]){
      const next=agentActions[clipKey]; next.reset(); next.fadeIn(0.18); next.play();
      if(agentCur && agentActions[agentCur]) agentActions[agentCur].fadeOut(0.18);
      agentCur=clipKey;
    }
    const want = spec.p;
    for(const k in agentProps) agentProps[k].visible = (k===want);
    if(spec.wear) agentWorn=true;
    if(agentCloak) agentCloak.visible=agentWorn;
    // ground the model by its lowest foot so non-standing poses (sitting to drink/
    // fish) rest on the terrain instead of floating. The shift is exact in one step
    // because moving human.y translates the foot's local-y by the same amount.
    human.updateWorldMatrix(true,true);
    if(footL || footR){
      const tmp=new THREE.Vector3(); let minY=Infinity;
      for(const fb of [footL,footR]) if(fb){ fb.getWorldPosition(tmp); agentGroup.worldToLocal(tmp); if(tmp.y<minY) minY=tmp.y; }
      if(isFinite(minY) && Math.abs(minY)>1e-4){ human.position.y -= minY; human.updateWorldMatrix(true,true); }
    }
    // keep the garment worn on the torso bone (follows the body every frame)
    if(agentWorn && agentCloak && torsoBone){
      const wp=new THREE.Vector3(); torsoBone.getWorldPosition(wp);
      pivot.worldToLocal(wp); agentCloak.position.copy(wp); agentCloak.rotation.set(0,0,0);
    }
    setLabel(agentLabel, aliveNow ? spec.l : 'Died');
  });
}
buildAnimalGLB();
buildAgentGLB();

// ---------------------------------------------------------------- main loop
$('loading').style.display = 'none';
let lastT = performance.now();
const FPS_FRAME = 1/ (META.fixed_dt*META.stride*5); // playback ~5x sim-time by default feel

// follow-cam: keep the protagonist in frame (toggle with F). Snapping target +
// camera by the same delta tracks the agent while preserving orbit angle/zoom.
let followCam = true, followInit = false;
addEventListener('keydown', e => { if (e.key === 'f' || e.key === 'F') followCam = !followCam; });
const _ap = new THREE.Vector3();
function updateFollow() {
  if (!followCam || window.__noFollow || !agentGroup) return;
  agentGroup.getWorldPosition(_ap); _ap.y += 1.2;
  if (!followInit) {                       // first frame: high angled view, clears tree canopy
    camera.position.copy(_ap).add(new THREE.Vector3(8.5, 13, 8.5));
    controls.target.copy(_ap); followInit = true; return;
  }
  const dx = _ap.x - controls.target.x, dy = _ap.y - controls.target.y, dz = _ap.z - controls.target.z;
  controls.target.set(_ap.x, _ap.y, _ap.z);
  camera.position.set(camera.position.x + dx, camera.position.y + dy, camera.position.z + dz);
}

function tick() {
  const now = performance.now(); const dt = Math.min(0.05, (now-lastT)/1000); lastT = now;
  if (playing) {
    playhead += dt * speed * 12;   // 12 replay-frames per second at 1x
    if (playhead >= NF-1) playhead = 0;
    seek.value = String(Math.floor(playhead));
  }
  tlabel.textContent = Math.floor(playhead) + ' / ' + (NF-1);
  applyFrame(playhead);
  updateFollow();
  if (agentGroup && beacon) {              // fade the locator column out in close-ups
    agentGroup.getWorldPosition(_ap);
    const t = THREE.MathUtils.clamp((camera.position.distanceTo(_ap) - 22) / 38, 0, 1);
    beacon.material.opacity = 0.22 * t;
    beacon.visible = t > 0.01;
  }
  controls.update();
  renderer.render(scene, camera);
  requestAnimationFrame(tick);
}
try { applyFrame(0); } catch (e) { console.error('applyFrame(0) failed:', e); }
tick();

addEventListener('resize', () => {
  camera.aspect = innerWidth/innerHeight; camera.updateProjectionMatrix();
  renderer.setSize(innerWidth, innerHeight);
});

// expose for debugging
window.__dp = { WORLD, FRAMES, scene, camera, controls, get agent(){ return agentGroup; } };
