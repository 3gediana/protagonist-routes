// Protagonist Ecology — 3D replay viewer
// Reads viewer_world.json (static terrain/water/vegetation) + frames.json
// (per-tick agents/buildings/trees/fires/projectiles/weather/season/day-night)
// and renders an interactive 3D map. Heavy use of instancing + on-demand
// rendering to stay smooth without dropping any display mechanism.
import * as THREE from 'three';
import { OrbitControls } from '../vendor/OrbitControls.js';

// ---------------------------------------------------------------- constants
const WORLD = 600, HALF = WORLD / 2;
const HVIEW = 6.5;            // vertical exaggeration so terrain reads as steep 3D relief
let hMin = 0, hMax = 1;       // heightmap range (set in buildTerrain)
const w2sx = (x) => x - HALF; // world->scene X
const w2sz = (y) => HALF - y; // world->scene Z (flip keeps map orientation)

const ARCH = [
  { c: 0x39d353, n: 'Protagonist' },
  { c: 0xffe14d, n: 'Large Herbivore' },
  { c: 0xb07a43, n: 'Small Forager' },
  { c: 0xff4d4d, n: 'Apex Predator' },
  { c: 0x9aa6b2, n: 'Pack Hunter' },
  { c: 0xff9e3d, n: 'Ambush Predator' },
  { c: 0x49e3e3, n: 'Scavenger' },
  { c: 0xff5cf0, n: 'Omnivore' },
  { c: 0xffffff, n: 'Rabbit' },
];
const BUILD = [
  { c: 0x8a8f98, n: 'Wall' },
  { c: 0xc8a86a, n: 'Shelter' },
  { c: 0x6e4d2e, n: 'Storage' },
  { c: 0x5b8fd6, n: 'Lookout' },
];
const OBJ = [{ c: 0x9a9a9a }, { c: 0x9c6b3f }, { c: 0xd8d8e0 }]; // stone / stick / spear
const SEASON = ['Spring', 'Summer', 'Autumn', 'Winter'];
const WEATHER = ['Clear', 'Cloudy', 'Rain', 'Storm', 'High Wind', 'Typhoon'];
const RES_COL = [0x39591f, 0x5f5520, 0x6e3038]; // grass / browse / fruit (muted ground cover, not species colors)
// season tint applied to terrain + vegetation (rgb multipliers)
const SEASON_TINT = [
  new THREE.Color(0.95, 1.05, 0.85), // spring
  new THREE.Color(0.92, 1.0, 0.7),   // summer
  new THREE.Color(1.18, 0.9, 0.55),  // autumn
  new THREE.Color(1.05, 1.12, 1.25), // winter (cold/snowy)
];

// ---------------------------------------------------------------- globals
let scene, camera, renderer, controls;
let world, frames, events = [];
let heightGrid, gx, gy;
let terrainMesh, terrainBaseColor;
let resMesh, waterGroup, treeMesh, agentMesh, objMesh;
let worksiteGroup, fireGroup, fireLights = [], fireSprites = [];
let sun, moon, hemi, ambient, skyMat, skyUniforms;
let rain, snow, dust;
let ambientBase = 0.1; // base ambient each frame; lightning adds on top without accumulating
const dummy = new THREE.Object3D();
const tmpColor = new THREE.Color();

let cur = 0, playing = true, speed = 1, lastT = 0;
let needsRender = true, lightningT = 0;
const MAXTREE = 256, MAXAGENT = 256, MAXOBJ = 256, MAXFIRE = 12, MAXWORK = 16;

// ---------------------------------------------------------------- bootstrap
init();
loadData();

function init() {
  scene = new THREE.Scene();
  camera = new THREE.PerspectiveCamera(55, innerWidth / innerHeight, 1, 4000);
  camera.position.set(0, 250, 540); // ~25° cinematic angle: shows relief without god-view top-down

  renderer = new THREE.WebGLRenderer({ antialias: true, powerPreference: 'high-performance' });
  renderer.setPixelRatio(Math.min(devicePixelRatio, 2));
  renderer.setSize(innerWidth, innerHeight);
  renderer.outputColorSpace = THREE.SRGBColorSpace;
  renderer.toneMapping = THREE.ACESFilmicToneMapping;
  renderer.toneMappingExposure = 1.05;
  document.getElementById('app').appendChild(renderer.domElement);

  controls = new OrbitControls(camera, renderer.domElement);
  controls.enableDamping = true;
  controls.dampingFactor = 0.08;
  controls.maxPolarAngle = Math.PI * 0.49;
  controls.minDistance = 40;
  controls.maxDistance = 1600;
  controls.target.set(0, 40, 0);
  controls.addEventListener('change', () => { needsRender = true; });

  // lights
  hemi = new THREE.HemisphereLight(0xbfd6ff, 0x40331f, 0.6);
  scene.add(hemi);
  ambient = new THREE.AmbientLight(0xffffff, 0.25);
  scene.add(ambient);
  sun = new THREE.DirectionalLight(0xffffff, 2.0);
  scene.add(sun, sun.target);
  moon = new THREE.DirectionalLight(0x9fb6ff, 0.0);
  scene.add(moon, moon.target);

  buildSky();
  addEventListener('resize', onResize);
  wireUI();
}

function buildSky() {
  skyUniforms = {
    top: { value: new THREE.Color(0x2a6cc4) },
    bottom: { value: new THREE.Color(0xbfe0ff) },
    offset: { value: 0.0 },
  };
  skyMat = new THREE.ShaderMaterial({
    side: THREE.BackSide, depthWrite: false, uniforms: skyUniforms,
    vertexShader: `varying vec3 vP; void main(){ vP=position; gl_Position=projectionMatrix*modelViewMatrix*vec4(position,1.0);} `,
    fragmentShader: `varying vec3 vP; uniform vec3 top; uniform vec3 bottom; uniform float offset;
      void main(){ float h=normalize(vP).y*0.5+0.5; h=clamp(h+offset,0.0,1.0); gl_FragColor=vec4(mix(bottom,top,pow(h,0.7)),1.0);} `,
  });
  const sky = new THREE.Mesh(new THREE.SphereGeometry(2600, 24, 16), skyMat);
  scene.add(sky);
  scene.fog = new THREE.FogExp2(0xbfe0ff, 0.00035);
}

// ---------------------------------------------------------------- data load
async function loadData() {
  const pct = document.getElementById('loadPct');
  try {
    pct.textContent = 'loading world…';
    world = await (await fetch('./data/viewer_world.json')).json();
    pct.textContent = 'loading frames…';
    frames = (await (await fetch('./data/frames.json')).json()).frames;
  } catch (e) {
    pct.textContent = 'failed to load data: ' + e;
    return;
  }
  pct.textContent = 'building scene…';
  gx = world.grid[0]; gy = world.grid[1];
  heightGrid = smoothHeight(world.height, gx, gy, 2); // soften high-freq noise into rolling hills
  buildTerrain();
  buildVegetation();
  buildWater();
  buildTrees();
  buildAgents();
  buildObjects();
  buildWorksites();
  buildFires();
  buildWeather();
  buildLegend();
  detectEvents();

  document.getElementById('scrub').max = frames.length - 1;
  applyFrame(0);
  document.getElementById('loading').style.display = 'none';
  lastT = performance.now();
  requestAnimationFrame(loop);
}

// box-blur the heightmap `passes` times so terrain reads as rolling hills, not noisy spikes
function smoothHeight(src, w, h, passes) {
  let a = Float32Array.from(src);
  let b = new Float32Array(a.length);
  for (let p = 0; p < passes; p++) {
    for (let j = 0; j < h; j++) for (let i = 0; i < w; i++) {
      let s = 0, n = 0;
      for (let dj = -1; dj <= 1; dj++) for (let di = -1; di <= 1; di++) {
        const ii = i + di, jj = j + dj;
        if (ii >= 0 && ii < w && jj >= 0 && jj < h) { s += a[jj * w + ii]; n++; }
      }
      b[j * w + i] = s / n;
    }
    [a, b] = [b, a];
  }
  return a;
}

function heightAt(x, y) {
  const u = Math.min(gx - 1, Math.max(0, (x / WORLD) * (gx - 1)));
  const v = Math.min(gy - 1, Math.max(0, (y / WORLD) * (gy - 1)));
  const i0 = Math.floor(u), j0 = Math.floor(v);
  const i1 = Math.min(gx - 1, i0 + 1), j1 = Math.min(gy - 1, j0 + 1);
  const fx = u - i0, fy = v - j0;
  const h00 = heightGrid[j0 * gx + i0], h10 = heightGrid[j0 * gx + i1];
  const h01 = heightGrid[j1 * gx + i0], h11 = heightGrid[j1 * gx + i1];
  const a = h00 * (1 - fx) + h10 * fx, b = h01 * (1 - fx) + h11 * fx;
  return ((a * (1 - fy) + b * fy) - hMin) * HVIEW; // valley floor at y=0
}

// ---------------------------------------------------------------- terrain
function buildTerrain() {
  const geo = new THREE.BufferGeometry();
  const pos = [], col = [], idx = [];
  hMin = Infinity; hMax = -Infinity;
  for (let k = 0; k < heightGrid.length; k++) { hMin = Math.min(hMin, heightGrid[k]); hMax = Math.max(hMax, heightGrid[k]); }
  const band = [
    { t: 0.00, c: new THREE.Color(0x8f8055) },
    { t: 0.18, c: new THREE.Color(0x55702f) },
    { t: 0.45, c: new THREE.Color(0x3f5a2b) },
    { t: 0.70, c: new THREE.Color(0x7d7058) },
    { t: 0.88, c: new THREE.Color(0xa7a08e) },
    { t: 1.00, c: new THREE.Color(0xeef2f6) },
  ];
  const ramp = (t) => {
    for (let b = 1; b < band.length; b++) if (t <= band[b].t) {
      const a0 = band[b - 1], a1 = band[b];
      return a0.c.clone().lerp(a1.c, (t - a0.t) / (a1.t - a0.t));
    }
    return band[band.length - 1].c.clone();
  };
  for (let j = 0; j < gy; j++) {
    for (let i = 0; i < gx; i++) {
      const wx = (i / (gx - 1)) * WORLD, wy = (j / (gy - 1)) * WORLD;
      const h = heightGrid[j * gx + i];
      pos.push(w2sx(wx), (h - hMin) * HVIEW, w2sz(wy));
      const t = (h - hMin) / Math.max(1e-3, hMax - hMin);
      const c = ramp(t);
      col.push(c.r, c.g, c.b);
    }
  }
  for (let j = 0; j < gy - 1; j++) {
    for (let i = 0; i < gx - 1; i++) {
      const a = j * gx + i, b = a + 1, c = a + gx, d = c + 1;
      idx.push(a, c, b, b, c, d);
    }
  }
  geo.setAttribute('position', new THREE.Float32BufferAttribute(pos, 3));
  geo.setAttribute('color', new THREE.Float32BufferAttribute(col, 3));
  geo.setIndex(idx);
  geo.computeVertexNormals();
  terrainBaseColor = geo.attributes.color.array.slice();
  terrainMesh = new THREE.Mesh(geo, new THREE.MeshStandardMaterial({ vertexColors: true, roughness: 0.95, metalness: 0.0, flatShading: false, side: THREE.DoubleSide }));
  scene.add(terrainMesh);
}

// ---------------------------------------------------------------- vegetation
function buildVegetation() {
  const res = world.resources;
  // flat ground patches that hug the terrain so food reads as ground cover,
  // not colored cones that look like species points
  const geo = new THREE.CircleGeometry(3.4, 7);
  geo.rotateX(-Math.PI / 2);
  resMesh = new THREE.InstancedMesh(geo, new THREE.MeshStandardMaterial({ roughness: 1, vertexColors: false, transparent: true, opacity: 0.7, side: THREE.DoubleSide }), res.length);
  resMesh.instanceMatrix.setUsage(THREE.StaticDrawUsage);
  for (let n = 0; n < res.length; n++) {
    const r = res[n];
    dummy.position.set(w2sx(r.x), heightAt(r.x, r.y) + 0.3, w2sz(r.y));
    const s = 0.8 + (n % 5) * 0.18;
    dummy.scale.set(s, 1, s);
    dummy.rotation.set(0, (n * 1.7) % Math.PI, 0);
    dummy.updateMatrix();
    resMesh.setMatrixAt(n, dummy.matrix);
    resMesh.setColorAt(n, tmpColor.setHex(RES_COL[r.k] ?? RES_COL[0]));
  }
  resMesh.instanceColor.needsUpdate = true;
  resVegBase = new Float32Array(resMesh.instanceColor.array); // remember base colors for season tint
  scene.add(resMesh);
}
let resVegBase;

// ---------------------------------------------------------------- water
function buildWater() {
  waterGroup = new THREE.Group();
  const mat = new THREE.MeshStandardMaterial({ color: 0x2b73c4, transparent: true, opacity: 0.72, roughness: 0.2, metalness: 0.3, emissive: 0x0a2540, emissiveIntensity: 0.3 });
  for (const s of world.water) {
    const disc = new THREE.Mesh(new THREE.CylinderGeometry(s.r * 1.6, s.r * 1.6, 1.2, 22), mat);
    disc.position.set(w2sx(s.x), heightAt(s.x, s.y) - 0.3, w2sz(s.y));
    waterGroup.add(disc);
  }
  scene.add(waterGroup);
}

// ---------------------------------------------------------------- trees
function buildTrees() {
  // trunk + foliage merged via a group baked into one instanced foliage + one trunk
  const folGeo = new THREE.ConeGeometry(6, 16, 7); folGeo.translate(0, 18, 0);
  treeMesh = new THREE.InstancedMesh(folGeo, new THREE.MeshStandardMaterial({ roughness: 0.9 }), MAXTREE);
  treeMesh.castShadow = false;
  treeTrunk = new THREE.InstancedMesh(new THREE.CylinderGeometry(1.4, 1.8, 10, 6), new THREE.MeshStandardMaterial({ color: 0x5a3d24, roughness: 1 }), MAXTREE);
  treeTrunk.geometry.translate(0, 5, 0);
  scene.add(treeMesh, treeTrunk);
}
let treeTrunk;

// ---------------------------------------------------------------- agents
function buildAgents() {
  const geo = new THREE.SphereGeometry(1, 12, 10);
  agentMesh = new THREE.InstancedMesh(geo, new THREE.MeshStandardMaterial({ roughness: 0.55, metalness: 0.1, emissiveIntensity: 0.35 }), MAXAGENT);
  agentMesh.instanceMatrix.setUsage(THREE.DynamicDrawUsage);
  scene.add(agentMesh);
}

// ---------------------------------------------------------------- projectiles
function buildObjects() {
  const geo = new THREE.IcosahedronGeometry(1, 0);
  objMesh = new THREE.InstancedMesh(geo, new THREE.MeshStandardMaterial({ roughness: 0.6 }), MAXOBJ);
  objMesh.instanceMatrix.setUsage(THREE.DynamicDrawUsage);
  scene.add(objMesh);
}

// ---------------------------------------------------------------- worksites
function buildWorksites() {
  worksiteGroup = new THREE.Group();
  worksiteSlots = [];
  for (let i = 0; i < MAXWORK; i++) {
    const g = new THREE.Group();
    g.visible = false;
    // persistent foundation pad so a planned site reads on the map even at 0 progress
    const pad = new THREE.Mesh(new THREE.CylinderGeometry(13, 13, 1.2, 16),
      new THREE.MeshStandardMaterial({ color: 0x3a3128, roughness: 1, transparent: true, opacity: 0.8 }));
    pad.position.y = 0.6;
    g.add(pad);
    worksiteGroup.add(g);
    worksiteSlots.push({ group: g, type: -1, mesh: null, pad });
  }
  scene.add(worksiteGroup);
}
let worksiteSlots;

function makeBuilding(bt) {
  const col = BUILD[bt] ? BUILD[bt].c : 0x999999;
  const mat = new THREE.MeshStandardMaterial({ color: col, roughness: 0.85 });
  const g = new THREE.Group();
  if (bt === 1) { // shelter / yurt: dome + base
    const base = new THREE.Mesh(new THREE.CylinderGeometry(10, 11, 8, 14), mat); base.position.y = 4;
    const dome = new THREE.Mesh(new THREE.SphereGeometry(10, 16, 10, 0, Math.PI * 2, 0, Math.PI / 2), mat); dome.position.y = 8;
    g.add(base, dome);
  } else if (bt === 2) { // storage: box + lid
    const box = new THREE.Mesh(new THREE.BoxGeometry(16, 9, 16), mat); box.position.y = 4.5;
    const lid = new THREE.Mesh(new THREE.BoxGeometry(17, 2, 17), new THREE.MeshStandardMaterial({ color: 0x4a3320 })); lid.position.y = 10;
    g.add(box, lid);
  } else if (bt === 3) { // lookout tower: posts + platform
    const post = new THREE.MeshStandardMaterial({ color: 0x6b4a2c });
    for (const [dx, dz] of [[-5, -5], [5, -5], [-5, 5], [5, 5]]) {
      const p = new THREE.Mesh(new THREE.CylinderGeometry(1, 1, 22, 6), post); p.position.set(dx, 11, dz); g.add(p);
    }
    const plat = new THREE.Mesh(new THREE.BoxGeometry(15, 3, 15), mat); plat.position.y = 23; g.add(plat);
    const roof = new THREE.Mesh(new THREE.ConeGeometry(11, 8, 4), mat); roof.position.y = 29; roof.rotation.y = Math.PI / 4; g.add(roof);
  } else { // wall segment
    const w = new THREE.Mesh(new THREE.BoxGeometry(20, 10, 5), mat); w.position.y = 5; g.add(w);
  }
  return g;
}

// ---------------------------------------------------------------- fires
function buildFires() {
  fireGroup = new THREE.Group();
  const tex = makeGlowTexture();
  for (let i = 0; i < MAXFIRE; i++) {
    const spr = new THREE.Sprite(new THREE.SpriteMaterial({ map: tex, color: 0xff7a1a, transparent: true, blending: THREE.AdditiveBlending, depthWrite: false }));
    spr.scale.set(26, 26, 1); spr.visible = false;
    fireGroup.add(spr); fireSprites.push(spr);
    const lt = new THREE.PointLight(0xff7b2a, 0, 130, 2); lt.visible = false;
    fireGroup.add(lt); fireLights.push(lt);
  }
  scene.add(fireGroup);
}
function makeGlowTexture() {
  const c = document.createElement('canvas'); c.width = c.height = 128;
  const g = c.getContext('2d');
  const grd = g.createRadialGradient(64, 64, 4, 64, 64, 64);
  grd.addColorStop(0, 'rgba(255,240,180,1)'); grd.addColorStop(0.3, 'rgba(255,150,40,0.85)');
  grd.addColorStop(1, 'rgba(255,80,0,0)');
  g.fillStyle = grd; g.fillRect(0, 0, 128, 128);
  const t = new THREE.CanvasTexture(c); t.colorSpace = THREE.SRGBColorSpace; return t;
}

// ---------------------------------------------------------------- weather FX
function buildWeather() {
  rain = makeParticles(4200, 700, 360, 0x9fd0ff, 2.2, true);
  snow = makeParticles(2400, 700, 360, 0xffffff, 4.5, false);
  dust = makeParticles(1400, 800, 120, 0xcbb487, 3.5, false);
  rain.visible = snow.visible = dust.visible = false;
  scene.add(rain, snow, dust);
}
let partTex;
function makeSoftDot() {
  if (partTex) return partTex;
  const c = document.createElement('canvas'); c.width = c.height = 32; const g = c.getContext('2d');
  const grd = g.createRadialGradient(16, 16, 0, 16, 16, 16);
  grd.addColorStop(0, 'rgba(255,255,255,1)'); grd.addColorStop(0.6, 'rgba(255,255,255,0.6)'); grd.addColorStop(1, 'rgba(255,255,255,0)');
  g.fillStyle = grd; g.beginPath(); g.arc(16, 16, 16, 0, 7); g.fill();
  partTex = new THREE.CanvasTexture(c); return partTex;
}
function makeParticles(n, spread, h, color, size, streak) {
  const pos = new Float32Array(n * 3);
  for (let i = 0; i < n; i++) {
    pos[i * 3] = (Math.random() - 0.5) * spread;
    pos[i * 3 + 1] = Math.random() * h;
    pos[i * 3 + 2] = (Math.random() - 0.5) * spread;
  }
  const g = new THREE.BufferGeometry();
  g.setAttribute('position', new THREE.BufferAttribute(pos, 3));
  const m = new THREE.PointsMaterial({ color, size, map: makeSoftDot(), alphaTest: 0.05, transparent: true, opacity: 0.8, depthWrite: false });
  const p = new THREE.Points(g, m);
  p.userData = { h, spread, streak, n };
  return p;
}

// ---------------------------------------------------------------- legend / events
function buildLegend() {
  const el = document.getElementById('legendList');
  el.innerHTML = ARCH.map(a => `<div class="lg"><span class="sw" style="background:#${a.c.toString(16).padStart(6, '0')}"></span>${a.n}</div>`).join('');
}

function detectEvents() {
  let pAlive = -1, pSeason = -1, pWeather = -1, pFires = -1;
  const doneSites = new Set();
  for (let f = 0; f < frames.length; f++) {
    const fr = frames[f]; const ev = [];
    const alive = fr.A.reduce((s, a) => s + a[6], 0);
    const fires = fr.F.reduce((s, x) => s + (x[3] === 1 || x[2] > 0.08 ? 1 : 0), 0);
    for (const w of fr.W) { if (w[6] === 1 && !doneSites.has(w[0])) { doneSites.add(w[0]); ev.push(`A ${BUILD[w[1]] ? BUILD[w[1]].n : 'structure'} was completed`); } }
    if (pSeason >= 0 && fr.s !== pSeason) ev.push(`Season turns to ${SEASON[fr.s]}`);
    if (pWeather >= 0 && fr.w !== pWeather) ev.push(`Weather shifts: ${WEATHER[fr.w]}`);
    if (pFires >= 0 && fires > pFires) ev.push(`A campfire is lit`);
    if (pAlive >= 0 && alive < pAlive) ev.push(`${pAlive - alive} perished`);
    if (pAlive >= 0 && alive > pAlive) ev.push(`${alive - pAlive} born into the world`);
    events[f] = ev;
    pAlive = alive; pSeason = fr.s; pWeather = fr.w; pFires = fires;
  }
}

// ---------------------------------------------------------------- per-frame
function applyFrame(fi) {
  const fr = frames[fi]; if (!fr) return;

  // ---- day/night lighting from tod (0.25 sunrise, 0.5 noon, 0.75 sunset)
  const tod = fr.tod, light = fr.light;
  const el = (tod - 0.25) * Math.PI * 2;      // sun elevation angle
  const az = tod * Math.PI * 2;
  const sy = Math.sin(el);
  const R = 1200, horiz = Math.cos(el) * R;
  sun.position.set(Math.cos(az) * horiz, sy * R, Math.sin(az) * horiz);
  sun.target.position.set(0, 0, 0);
  const day = Math.max(0, sy);
  const wDim = weatherDim(fr.w, fr.wp);
  sun.intensity = (0.15 + day * 2.1) * wDim;
  // warm at horizon, white at noon
  sun.color.setHex(0xffffff).lerp(tmpColor.setHex(0xff8a3a), (1 - Math.min(1, day * 1.6)) * 0.8);
  const night = 1 - Math.min(1, day * 2.2);
  moon.position.set(-Math.cos(az) * R, Math.max(120, -sy * R), -Math.sin(az) * R);
  moon.intensity = night * 0.35 * (0.5 + 0.5 * wDim);
  hemi.intensity = (0.18 + light * 0.6) * (0.5 + 0.5 * wDim);
  ambient.intensity = (0.08 + light * 0.28) * wDim;
  ambientBase = ambient.intensity; // lightning flashes add on top of this, never accumulate

  // ---- sky + fog colors
  updateSky(tod, day, fr.w);

  // ---- season tint on terrain + vegetation
  applySeasonTint(fr.s, fr.sp);

  // ---- agents (instanced)
  let an = 0;
  for (const a of fr.A) {
    if (a[6] !== 1) continue; // alive only
    const arch = a[1], x = a[2], y = a[3], life = a[5];
    const base = arch === 0 ? 3.4 : 2.1;          // protagonist larger
    const sc = base * (life === 0 ? 0.62 : life === 2 ? 1.18 : 1.0); // juvenile/elder size
    dummy.position.set(w2sx(x), heightAt(x, y) + sc, w2sz(y));
    dummy.scale.set(sc, sc, sc);
    dummy.rotation.set(0, 0, 0); dummy.updateMatrix();
    agentMesh.setMatrixAt(an, dummy.matrix);
    tmpColor.setHex(ARCH[arch] ? ARCH[arch].c : 0xffffff);
    agentMesh.setColorAt(an, tmpColor);
    const m = agentMesh.material; // (emissive set globally below)
    an++;
    if (an >= MAXAGENT) break;
  }
  hideRest(agentMesh, an);
  agentMesh.count = MAXAGENT;
  agentMesh.instanceMatrix.needsUpdate = true;
  if (agentMesh.instanceColor) agentMesh.instanceColor.needsUpdate = true;
  agentMesh.material.emissive = new THREE.Color(0x000000);

  // ---- projectiles / objects
  let on = 0;
  for (const o of fr.O) {
    const x = o[0], y = o[1], type = o[2], carried = o[3], vx = o[4], vy = o[5];
    const moving = Math.abs(vx) + Math.abs(vy) > 0.05;
    const yLift = carried ? 4 : (moving ? 6 : 0.8);
    // flying/carried projectiles read clearly; dropped ones shrink to small ground litter
    const sc = (moving || carried)
      ? (type === 2 ? 2.4 : 1.8)
      : (type === 2 ? 1.3 : 0.85);
    dummy.position.set(w2sx(x), heightAt(x, y) + yLift, w2sz(y));
    dummy.scale.set(sc, type === 2 ? sc * 1.8 : sc, sc);
    dummy.rotation.set(0, (x + y) * 0.1, moving ? 0.7 : 0); dummy.updateMatrix();
    objMesh.setMatrixAt(on, dummy.matrix);
    objMesh.setColorAt(on, tmpColor.setHex(OBJ[type] ? OBJ[type].c : 0xaaaaaa).multiplyScalar(moving ? 1.6 : 1.0));
    on++; if (on >= MAXOBJ) break;
  }
  hideRest(objMesh, on);
  objMesh.count = MAXOBJ;
  objMesh.instanceMatrix.needsUpdate = true;
  if (objMesh.instanceColor) objMesh.instanceColor.needsUpdate = true;

  // ---- trees (foliage scales with integrity; harvested -> small/brown)
  let tn = 0;
  for (const t of fr.T) {
    const x = t[1], y = t[2], iv = t[3], av = t[4];
    const h = heightAt(x, y);
    const fol = 0.35 + iv * 0.85;
    dummy.position.set(w2sx(x), h, w2sz(y));
    dummy.scale.set(fol, fol, fol); dummy.rotation.set(0, (t[0] * 1.3) % 6.28, 0); dummy.updateMatrix();
    treeMesh.setMatrixAt(tn, dummy.matrix);
    const green = new THREE.Color(0x2f7d32), dry = new THREE.Color(0x7a5a2a);
    treeMesh.setColorAt(tn, dry.clone().lerp(green, iv).multiply(SEASON_TINT[3] && false ? new THREE.Color(1, 1, 1) : new THREE.Color(1, 1, 1)));
    dummy.scale.set(1, 1, 1); dummy.updateMatrix();
    treeTrunk.setMatrixAt(tn, dummy.matrix);
    tn++; if (tn >= MAXTREE) break;
  }
  hideRest(treeMesh, tn); hideRest(treeTrunk, tn);
  treeMesh.count = MAXTREE; treeTrunk.count = MAXTREE;
  treeMesh.instanceMatrix.needsUpdate = true; treeTrunk.instanceMatrix.needsUpdate = true;
  if (treeMesh.instanceColor) treeMesh.instanceColor.needsUpdate = true;

  // ---- worksites / buildings: foundation pad always shown; structure rises with progress
  let wi = 0;
  for (const w of fr.W) {
    if (wi >= MAXWORK) break;
    const slot = worksiteSlots[wi];
    const bt = w[1], x = w[2], y = w[3], prog = w[4], active = w[5], done = w[6];
    if (slot.type !== bt) { // (re)build structure for this slot's type
      if (slot.mesh) slot.group.remove(slot.mesh);
      slot.mesh = makeBuilding(bt); slot.group.add(slot.mesh); slot.type = bt;
    }
    slot.group.visible = true;
    slot.group.position.set(w2sx(x), heightAt(x, y), w2sz(y));
    slot.pad.material.color.setHex(BUILD[bt] ? BUILD[bt].c : 0x888888).multiplyScalar(0.5);
    const p = done ? 1 : Math.max(0.06, prog); // 6% stub marks a planned/unbuilt site
    slot.mesh.scale.set(1, p, 1);
    slot.mesh.traverse(o => { if (o.material && o.material.color) { o.material.transparent = !done; o.material.opacity = done ? 1 : 0.3 + 0.6 * prog; o.material.emissive = active && !done ? new THREE.Color(0x332200) : new THREE.Color(0x000000); } });
    wi++;
  }
  for (; wi < MAXWORK; wi++) worksiteSlots[wi].group.visible = false;

  // ---- fires
  let fn = 0;
  for (const fdat of fr.F) {
    if (fn >= MAXFIRE) break;
    const x = fdat[0], y = fdat[1], inten = fdat[2];
    const active = fdat[3] === 1 || inten > 0.08; // intensity drives the flame/heat glow
    const spr = fireSprites[fn], lt = fireLights[fn];
    if (active) {
      const yy = heightAt(x, y) + 6;
      spr.visible = true; spr.position.set(w2sx(x), yy, w2sz(y));
      const flick = 0.85 + 0.15 * Math.sin(performance.now() * 0.01 + fn);
      spr.scale.setScalar((16 + Math.min(inten, 1.3) * 7) * flick);
      lt.visible = true; lt.position.set(w2sx(x), yy + 2, w2sz(y));
      lt.intensity = (0.5 + Math.min(inten, 1.3) * 0.5) * flick; lt.distance = 60; lt.decay = 2;
    } else { spr.visible = false; lt.visible = false; }
    fn++;
  }
  for (; fn < MAXFIRE; fn++) { fireSprites[fn].visible = false; fireLights[fn].visible = false; }

  // ---- weather FX visibility
  updateWeather(fr.w, fr.s);

  // ---- HUD + story
  updateHUD(fr);
  showStory(fi);
  needsRender = true;
}

function hideRest(mesh, from) {
  dummy.position.set(0, -9999, 0); dummy.scale.set(0, 0, 0); dummy.rotation.set(0, 0, 0); dummy.updateMatrix();
  for (let i = from; i < mesh.instanceMatrix.count; i++) mesh.setMatrixAt(i, dummy.matrix);
}

function weatherDim(w, wp) {
  // clear bright; cloudy/rain dimmer; storm/typhoon darkest
  const base = [1.0, 0.72, 0.6, 0.42, 0.78, 0.4][w] ?? 1.0;
  return base;
}

function updateSky(tod, day, w) {
  const dayTop = new THREE.Color(0x2f74cf), dayBot = new THREE.Color(0xbfe4ff);
  const duskTop = new THREE.Color(0x3a2b5c), duskBot = new THREE.Color(0xff8b4a);
  const nightTop = new THREE.Color(0x05070f), nightBot = new THREE.Color(0x141d33);
  let top, bot;
  if (day > 0.25) { const k = Math.min(1, (day - 0.25) / 0.5); top = duskTop.clone().lerp(dayTop, k); bot = duskBot.clone().lerp(dayBot, k); }
  else { const k = Math.min(1, day / 0.25); top = nightTop.clone().lerp(duskTop, k); bot = nightBot.clone().lerp(duskBot, k); }
  // weather desaturate/darken
  const wd = weatherDim(w);
  const grey = new THREE.Color(0x55606b);
  top.lerp(grey, (1 - wd) * 0.5); bot.lerp(grey, (1 - wd) * 0.5);
  skyUniforms.top.value.copy(top); skyUniforms.bottom.value.copy(bot);
  scene.fog.color.copy(bot);
  const fogBase = 0.00032;
  scene.fog.density = fogBase * (w === 2 ? 2.6 : w === 3 ? 3.6 : w === 5 ? 4.2 : w === 1 ? 1.5 : 1.0) + (1 - Math.min(1, day * 2)) * 0.0002;
  renderer.toneMappingExposure = 0.7 + day * 0.5;
}

let lastSeason = -1, lastSp = -1;
function applySeasonTint(s, sp) {
  if (s === lastSeason && Math.abs(sp - lastSp) < 0.02) return;
  lastSeason = s; lastSp = sp;
  const tint = SEASON_TINT[s] || new THREE.Color(1, 1, 1);
  // terrain
  const arr = terrainMesh.geometry.attributes.color.array;
  const snowy = s === 3 ? Math.min(0.6, sp + 0.25) : 0; // winter whitening
  for (let i = 0; i < arr.length; i += 3) {
    let r = terrainBaseColor[i] * tint.r, g = terrainBaseColor[i + 1] * tint.g, b = terrainBaseColor[i + 2] * tint.b;
    if (snowy) { r = r * (1 - snowy) + snowy * 0.92; g = g * (1 - snowy) + snowy * 0.95; b = b * (1 - snowy) + snowy * 1.0; }
    arr[i] = r; arr[i + 1] = g; arr[i + 2] = b;
  }
  terrainMesh.geometry.attributes.color.needsUpdate = true;
  // vegetation
  if (resVegBase && resMesh.instanceColor) {
    const ca = resMesh.instanceColor.array;
    for (let i = 0; i < ca.length; i += 3) {
      ca[i] = Math.min(1, resVegBase[i] * tint.r); ca[i + 1] = Math.min(1, resVegBase[i + 1] * tint.g); ca[i + 2] = Math.min(1, resVegBase[i + 2] * tint.b);
    }
    resMesh.instanceColor.needsUpdate = true;
  }
}

function updateWeather(w, s) {
  const showFX = document.getElementById('t_weather').checked;
  rain.visible = showFX && (w === 2 || w === 3 || w === 5);
  dust.visible = showFX && (w === 4 || w === 5);
  snow.visible = showFX && s === 3;
  rain.material.opacity = w === 3 || w === 5 ? 0.95 : 0.7;
}

function updateHUD(fr) {
  const tod = fr.tod, hh = Math.floor(tod * 24), mm = Math.floor((tod * 24 % 1) * 60);
  document.getElementById('clockTime').textContent = String(hh).padStart(2, '0') + ':' + String(mm).padStart(2, '0');
  const isDay = tod > 0.25 && tod < 0.75;
  document.getElementById('phaseName').textContent = tod < 0.25 ? 'night' : tod < 0.3 ? 'dawn' : tod < 0.7 ? 'day' : tod < 0.78 ? 'dusk' : 'night';
  const dot = document.getElementById('clockDot');
  dot.style.background = isDay ? '#ffd86b' : '#9fb6ff';
  dot.style.boxShadow = `0 0 10px 2px ${isDay ? 'rgba(255,200,80,.7)' : 'rgba(120,150,255,.6)'}`;
  document.getElementById('seasonV').textContent = SEASON[fr.s] + ' ';
  document.getElementById('seasonBar').style.width = (fr.sp * 100).toFixed(0) + '%';
  document.getElementById('weatherV').textContent = WEATHER[fr.w];
  const alive = fr.A.reduce((s, a) => s + a[6], 0);
  document.getElementById('popV').textContent = alive + ' / ' + fr.A.length;
  const done = fr.W.reduce((s, w) => s + w[6], 0);
  const built = fr.W.reduce((s, w) => s + (w[6] || w[4] > 0.02 ? 1 : 0), 0);
  document.getElementById('bldV').textContent = built + ' / ' + fr.W.length;
  document.getElementById('fireV').textContent = fr.F.reduce((s, x) => s + (x[3] === 1 || x[2] > 0.08 ? 1 : 0), 0) + ' / ' + fr.F.length;
  document.getElementById('tickV').textContent = fr.tick;
  document.getElementById('frameLbl').textContent = (cur | 0) + ' / ' + (frames.length - 1);
  document.getElementById('scrub').value = cur | 0;
}

const logged = [];
function showStory(fi) {
  const ev = events[fi]; if (!ev || !ev.length) return;
  const cap = document.getElementById('storyCap');
  cap.textContent = ev[0]; cap.style.opacity = '1';
  clearTimeout(showStory._t); showStory._t = setTimeout(() => { cap.style.opacity = '0'; }, 2200);
  const log = document.getElementById('storyLog');
  for (const e of ev) {
    const d = document.createElement('div'); d.className = 'new'; d.textContent = '› ' + e; log.prepend(d);
    setTimeout(() => d.classList.remove('new'), 1500);
  }
  while (log.children.length > 7) log.removeChild(log.lastChild);
}

// ---------------------------------------------------------------- loop
function loop(now) {
  requestAnimationFrame(loop);
  const dt = Math.min(0.05, (now - lastT) / 1000); lastT = now;
  controls.update();

  if (playing) {
    cur += dt * 18 * speed; // ~18 frames/sec at 1x
    if (cur >= frames.length) cur = 0;
    applyFrame(cur | 0);
  }
  // animate particles every frame when visible
  animateParticles(rain, dt, 220 * speed);
  animateParticles(snow, dt, 40 * speed, true);
  animateParticles(dust, dt, 0, false, 120 * speed);

  // storm lightning
  const fr = frames[cur | 0];
  if (playing && fr && fr.w === 3 && Math.random() < 0.006) { lightningT = 1; }
  if (lightningT > 0) lightningT -= dt * 3;
  const flash = Math.max(0, lightningT) * 1.5;
  ambient.intensity = ambientBase + flash; // set, never accumulate (fixes wash-out while paused)
  if (flash > 0) needsRender = true;

  if (needsRender || playing || rain.visible || snow.visible || dust.visible) {
    renderer.render(scene, camera);
    needsRender = false;
  }
}

function animateParticles(p, dt, fall, drift, wind) {
  if (!p.visible) return;
  const a = p.geometry.attributes.position.array, h = p.userData.h, sp = p.userData.spread;
  const camx = controls.target.x, camz = controls.target.z;
  for (let i = 0; i < a.length; i += 3) {
    if (fall) a[i + 1] -= fall * dt;
    if (drift) { a[i] += Math.sin((a[i + 1] + i) * 0.05) * 6 * dt; a[i + 2] += Math.cos(i) * 4 * dt; }
    if (wind) { a[i] += wind * dt; }
    if (a[i + 1] < 0) { a[i + 1] = h; a[i] = camx + (Math.random() - 0.5) * sp; a[i + 2] = camz + (Math.random() - 0.5) * sp; }
    if (wind && a[i] > camx + sp / 2) a[i] = camx - sp / 2;
  }
  p.geometry.attributes.position.needsUpdate = true;
  p.position.x = 0; p.position.z = 0;
}

// ---------------------------------------------------------------- UI
function wireUI() {
  document.getElementById('playBtn').onclick = (e) => {
    playing = !playing; e.target.textContent = playing ? 'Pause' : 'Play'; lastT = performance.now();
  };
  document.getElementById('scrub').oninput = (e) => { cur = +e.target.value; playing = false; document.getElementById('playBtn').textContent = 'Play'; applyFrame(cur | 0); };
  document.getElementById('speed').onchange = (e) => { speed = +e.target.value; };
  const tg = (id, fn) => document.getElementById(id).onchange = (e) => { fn(e.target.checked); needsRender = true; };
  tg('t_terrain', v => terrainMesh.visible = v);
  tg('t_resources', v => resMesh.visible = v);
  tg('t_water', v => waterGroup.visible = v);
  tg('t_trees', v => { treeMesh.visible = v; treeTrunk.visible = v; });
  tg('t_agents', v => agentMesh.visible = v);
  tg('t_objects', v => objMesh.visible = v);
  tg('t_worksites', v => worksiteGroup.visible = v);
  tg('t_fires', v => fireGroup.visible = v);
  tg('t_weather', v => { if (!v) rain.visible = snow.visible = dust.visible = false; });
}

function onResize() {
  camera.aspect = innerWidth / innerHeight; camera.updateProjectionMatrix();
  renderer.setSize(innerWidth, innerHeight); needsRender = true;
}
