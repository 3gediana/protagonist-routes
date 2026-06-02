import * as THREE from 'three';
import { OrbitControls } from 'three/addons/OrbitControls.js';
import { GLTFLoader } from 'three/addons/GLTFLoader.js';

const renderer = new THREE.WebGLRenderer({ antialias:true });
renderer.setSize(innerWidth, innerHeight); renderer.outputColorSpace = THREE.SRGBColorSpace;
document.body.appendChild(renderer.domElement);
const scene = new THREE.Scene(); scene.background = new THREE.Color(0x202830);
const cam = new THREE.PerspectiveCamera(50, innerWidth/innerHeight, 0.1, 1000);
scene.add(new THREE.HemisphereLight(0xffffff, 0x445566, 1.1));
const d = new THREE.DirectionalLight(0xffffff, 1.4); d.position.set(5,10,7); scene.add(d);

const sets = [
  ['Animated_Animal_Pack', ['26zM1outCr','Bc97C66HKi','P1gU3Qkr9r','T6Cs7tmMHJ','a8PIIYwF7r','bCVFD48i2l','bEdE4rmZy9','qmX6nhnvp7','qvTrSG9pZF','tQdzbZ1Cmw','wcWiuEqwzq','y4wdQpg767']],
  ['Animated_Men_Pack', ['DLptRuewTn','HMnuH5geEG','fjHyMd5Wxw','mQnGoME1ez']],
];
const all = [];
for (const [pack, ids] of sets) for (const id of ids) all.push([pack, id]);

function label(text) {
  const c = document.createElement('canvas'); c.width=256; c.height=64;
  const g = c.getContext('2d'); g.fillStyle='#000a'; g.fillRect(0,0,256,64);
  g.fillStyle='#fff'; g.font='bold 26px monospace'; g.textAlign='center'; g.fillText(text,128,42);
  const t = new THREE.CanvasTexture(c);
  const s = new THREE.Sprite(new THREE.SpriteMaterial({ map:t }));
  s.scale.set(3,0.75,1); return s;
}

const loader = new GLTFLoader();
const COLS = 6, GAP = 5;
let done = 0;
all.forEach(([pack, id], i) => {
  const col = i % COLS, row = Math.floor(i / COLS);
  const px = (col - (COLS-1)/2) * GAP, pz = row * GAP;
  loader.load(`./assets/${pack}/${id}_${id}.glb`, (gltf) => {
    const obj = gltf.scene;
    obj.traverse(m => { if (m.isMesh) { m.frustumCulled = false; } });
    // measure ONLY mesh geometry (skeleton bones can extend huge and break setFromObject)
    const meshBox = (o) => {
      o.updateWorldMatrix(true, true);
      const b = new THREE.Box3();
      o.traverse(m => { if (m.isMesh) { m.geometry.computeBoundingBox(); const gb = m.geometry.boundingBox.clone(); gb.applyMatrix4(m.matrixWorld); b.union(gb); } });
      return b;
    };
    const box = meshBox(obj);
    const size = new THREE.Vector3(); box.getSize(size);
    const sc = 3.0 / Math.max(size.x, size.y, size.z);
    obj.scale.setScalar(sc);
    const box2 = meshBox(obj); const c2 = new THREE.Vector3(); box2.getCenter(c2);
    obj.position.set(px - c2.x, -box2.min.y, pz - c2.z);
    scene.add(obj);
    const lab = label(id); lab.position.set(px, 3.6, pz); scene.add(lab);
    if (++done === all.length) console.log('GALLERY_READY');
  }, undefined, (err) => { console.error('load fail', id, err); });
});

cam.position.set(0, 9, 16);
const ctr = new OrbitControls(cam, renderer.domElement);
ctr.target.set(0, 1.5, 2.5);
function loop(){ ctr.update(); renderer.render(scene, cam); requestAnimationFrame(loop); }
loop();
window.__g = { scene, cam, renderer };
