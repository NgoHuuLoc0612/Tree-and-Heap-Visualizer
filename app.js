/**
 * Tree & Heap Visualizer — Frontend Engine
 * 2D Canvas + Three.js 3D · WebSocket real-time · Enterprise-grade
 */

"use strict";

// ─────────────────────────────────────────────
//  Constants & Config
// ─────────────────────────────────────────────

const API = "http://localhost:5000/api";
const WS  = "http://localhost:5000";

const STRUCT_META = {
  avl:   { title:"AVL Tree",       meta:"Balanced BST · O(log n) ops · Height-balanced" },
  rbt:   { title:"Red-Black Tree", meta:"BST · O(log n) · 5 RBT properties guaranteed" },
  heap:  { title:"Binary Heap",    meta:"Min-Heap · O(log n) insert/extract · O(n) build" },
  btree: { title:"B-Tree (t=2)",   meta:"Disk-optimized · O(log n) · Multi-key nodes" },
  treap: { title:"Treap",          meta:"Randomized BST + Heap · Expected O(log n)" },
  splay: { title:"Splay Tree",     meta:"Self-adjusting BST · Amortized O(log n)" },
  trie:  { title:"Trie",           meta:"Prefix tree · O(L) ops · L = key length" },
};

// Canvas color tokens (must match CSS vars, but used in drawImage context)
const C = {
  bg:       "#0a0c0f",
  panel:    "#0f1216",
  card:     "#141820",
  border:   "#1e2430",
  borderHi: "#2d3a50",
  fg:       "#d4dde8",
  fgDim:    "#5a6a80",
  fgMid:    "#8a9bb0",
  accent:   "#00d4ff",
  accent2:  "#7c4dff",
  accent3:  "#00ff9f",
  red:      "#ff4060",
  green:    "#00e676",
  yellow:   "#ffd700",
  orange:   "#ff8c00",
  nodeAVL:  "#0077cc",
  nodeRBTr: "#cc2200",
  nodeRBTb: "#1a2030",
  nodeHeap: "#5500cc",
  nodeHL:   "#ffd700",
  nodePath: "#00ff9f",
  nodeTreap:"#004466",
  nodeSplay:"#220055",
};

// ─────────────────────────────────────────────
//  State
// ─────────────────────────────────────────────

const state = {
  sessionId:  "session_" + Math.random().toString(36).slice(2),
  struct:     "avl",
  viewMode:   "2d",
  speed:      500,
  treeData:   null,
  highlight:  new Set(),   // highlighted node ids
  pathNodes:  new Set(),   // path search nodes
  found:      new Set(),   // found node
  animating:  false,
  // 2D pan/zoom
  cam: { x:0, y:0, scale:1 },
  drag: { active:false, startX:0, startY:0, camX:0, camY:0 },
  // hovered node
  hoveredNode: null,
  // algo panel state
  algoMode: null,
};

// ─────────────────────────────────────────────
//  DOM refs
// ─────────────────────────────────────────────

const $ = id => document.getElementById(id);
const canvas   = $("viz-canvas");
const ctx      = canvas.getContext("2d");
const threeDiv = $("three-container");

// ─────────────────────────────────────────────
//  API helpers
// ─────────────────────────────────────────────

async function api(endpoint, body={}) {
  const t0 = performance.now();
  try {
    const resp = await fetch(API + endpoint, {
      method: "POST",
      headers: { "Content-Type":"application/json" },
      body: JSON.stringify({ session_id: state.sessionId, ...body })
    });
    const data = await resp.json();
    const ms = (performance.now() - t0).toFixed(1);
    $("stat-time").textContent = ms + "ms";
    return data;
  } catch(e) {
    console.warn("[API] Error:", e);
    showToast("Backend not reachable — running in demo mode", "warn");
    return null;
  }
}

async function apiGet(endpoint) {
  try {
    const resp = await fetch(API + endpoint);
    return await resp.json();
  } catch { return null; }
}

// ─────────────────────────────────────────────
//  WebSocket
// ─────────────────────────────────────────────

let socket = null;
let wsConnected = false;

function initWebSocket() {
  try {
    socket = io(WS, { transports:["websocket"] });
    socket.on("connect", () => {
      wsConnected = true;
      updateBackendBadge(true);
    });
    socket.on("disconnect", () => {
      wsConnected = false;
      updateBackendBadge(false);
    });
    socket.on("connected", d => {
      if (d.cpp) $("backend-label").textContent = "C++ Engine Active";
      else        $("backend-label").textContent = "Python Fallback";
    });
    socket.on("state_update", d => {
      if (d.state) applyState(d.state);
    });
    socket.on("animation_step", d => {
      if (d.state) applyState(d.state);
    });
    socket.on("animation_done", () => {
      state.animating = false;
    });
    socket.on("error", d => {
      console.error("[WS]", d.message);
      showToast(d.message, "error");
    });
  } catch(e) {
    console.warn("[WS] Could not connect:", e);
    updateBackendBadge(false);
  }
}

function updateBackendBadge(connected) {
  const badge = document.querySelector(".backend-badge");
  if (connected) {
    badge.classList.add("cpp");
    $("backend-label").textContent = "Connected";
  } else {
    badge.classList.remove("cpp");
    $("backend-label").textContent = "Offline";
  }
}

// ─────────────────────────────────────────────
//  State application
// ─────────────────────────────────────────────

function applyState(st) {
  state.treeData = st;
  updateStats(st);
  if (state.viewMode === "2d") render2D();
  else render3D(st);
}

function updateStats(st) {
  if (!st) return;
  const nodes = st.nodes || [];
  $("stat-nodes").textContent       = nodes.length;
  $("stat-height").textContent      = st.stats?.height ?? "—";
  $("stat-comparisons").textContent = st.stats?.comparisons ?? "—";
  $("stat-rotations").textContent   = st.stats?.rotations ?? "—";
  $("stat-root").textContent        = st.root != null && st.root !== -1 ? st.root : "—";
  $("stat-lastop").textContent      = st.stats?.last_op ?? "—";
}

// ─────────────────────────────────────────────
//  2D Canvas Renderer
// ─────────────────────────────────────────────

const NODE_R  = 22;  // node circle radius px
const FONT_N  = "bold 12px 'JetBrains Mono', monospace";
const FONT_SM = "10px 'JetBrains Mono', monospace";

function resizeCanvas() {
  const rect = $("canvas-area").getBoundingClientRect();
  canvas.width  = rect.width  * devicePixelRatio;
  canvas.height = rect.height * devicePixelRatio;
  canvas.style.width  = rect.width  + "px";
  canvas.style.height = rect.height + "px";
  ctx.scale(devicePixelRatio, devicePixelRatio);
  if (state.viewMode === "2d") render2D();
}

function worldToScreen(wx, wy) {
  const W = canvas.width  / devicePixelRatio;
  const H = canvas.height / devicePixelRatio;
  return {
    x: W/2 + (wx + state.cam.x) * state.cam.scale * (W * 0.35),
    y: H * 0.12 + (wy + state.cam.y) * state.cam.scale * (H * 0.65)
  };
}

function screenToWorld(sx, sy) {
  const W = canvas.width  / devicePixelRatio;
  const H = canvas.height / devicePixelRatio;
  return {
    x: (sx - W/2)  / (state.cam.scale * W * 0.35)  - state.cam.x,
    y: (sy - H*0.12) / (state.cam.scale * H * 0.65) - state.cam.y
  };
}

function nodeColor(nd) {
  if (state.found.has(nd.id))       return C.green;
  if (state.highlight.has(nd.id))   return C.nodeHL;
  if (state.pathNodes.has(nd.id))   return C.nodePath;
  const type = nd.type || state.struct;
  if (type==="rbt")  return nd.color===1 ? C.nodeRBTr : C.nodeRBTb;
  if (type==="heap") return C.nodeHeap;
  if (type==="treap")return C.nodeTreap;
  if (type==="splay")return C.nodeSplay;
  return C.nodeAVL;
}

function render2D() {
  const W = canvas.width  / devicePixelRatio;
  const H = canvas.height / devicePixelRatio;
  ctx.clearRect(0, 0, W, H);

  // Background grid
  drawGrid(W, H);

  const st = state.treeData;
  if (!st || !st.nodes || !st.nodes.length) {
    drawEmptyHint(W, H);
    return;
  }

  const type = st.type || state.struct;

  if (type === "trie") { renderTrie(st, W, H); return; }
  if (type === "btree") { renderBTree(st, W, H); return; }

  const byId = {};
  st.nodes.forEach(n => { byId[n.id] = n; });
  const nil = st.nil ?? -1;

  // Draw edges first
  st.nodes.forEach(nd => {
    if (!nd.layout) return;
    const s = worldToScreen(nd.layout.x, nd.layout.y);
    ["left","right"].forEach(dir => {
      const cid = nd[dir];
      if (cid == null || cid === -1 || cid === nil) return;
      const child = byId[cid];
      if (!child || !child.layout) return;
      const t = worldToScreen(child.layout.x, child.layout.y);
      drawEdge(s.x, s.y, t.x, t.y, nd.id, cid);
    });
  });

  // Draw nodes
  st.nodes.forEach(nd => {
    if (!nd.layout || nd.id === nil) return;
    const s = worldToScreen(nd.layout.x, nd.layout.y);
    drawNode2D(nd, s.x, s.y, type);
  });
}

function drawGrid(W, H) {
  ctx.save();
  ctx.strokeStyle = "rgba(30,36,48,0.7)";
  ctx.lineWidth = 1;
  const spacing = 40;
  for (let x = 0; x < W; x += spacing) {
    ctx.beginPath(); ctx.moveTo(x,0); ctx.lineTo(x,H); ctx.stroke();
  }
  for (let y = 0; y < H; y += spacing) {
    ctx.beginPath(); ctx.moveTo(0,y); ctx.lineTo(W,y); ctx.stroke();
  }
  ctx.restore();
}

function drawEmptyHint(W, H) {
  ctx.save();
  ctx.fillStyle = "rgba(90,106,128,0.35)";
  ctx.font = "bold 16px 'Space Grotesk', sans-serif";
  ctx.textAlign = "center";
  ctx.textBaseline = "middle";
  ctx.fillText("Insert nodes to visualize", W/2, H/2);
  ctx.font = "12px 'JetBrains Mono', monospace";
  ctx.fillText("Use the panel above or press ⚄ to generate a random tree", W/2, H/2 + 28);
  ctx.restore();
}

function drawEdge(x1, y1, x2, y2, fromId, toId) {
  const isHL = (state.highlight.has(fromId) && state.highlight.has(toId))
             || (state.pathNodes.has(fromId) && state.pathNodes.has(toId));
  ctx.save();
  ctx.beginPath();
  ctx.moveTo(x1, y1);
  ctx.lineTo(x2, y2);
  ctx.strokeStyle = isHL ? C.accent : C.borderHi;
  ctx.lineWidth   = isHL ? 2 : 1.5;
  if (isHL) {
    ctx.shadowColor = C.accent;
    ctx.shadowBlur  = 6;
  }
  ctx.stroke();
  ctx.restore();
}

function drawNode2D(nd, cx, cy, type) {
  const r   = NODE_R;
  const col = nodeColor(nd);
  const isHL= state.highlight.has(nd.id) || state.pathNodes.has(nd.id) || state.found.has(nd.id);

  ctx.save();

  // Shadow/glow
  if (isHL) {
    ctx.shadowColor = col;
    ctx.shadowBlur  = 16;
  }

  // Circle
  ctx.beginPath();
  ctx.arc(cx, cy, r, 0, Math.PI*2);
  ctx.fillStyle = col;
  ctx.fill();

  // Border
  ctx.strokeStyle = isHL ? "#fff" : C.borderHi;
  ctx.lineWidth   = isHL ? 2 : 1.5;
  ctx.stroke();

  ctx.shadowBlur = 0;
  ctx.shadowColor = "transparent";

  // Key label
  ctx.font = FONT_N;
  ctx.fillStyle = (col === C.nodeRBTb || col === C.nodeHeap || col === C.nodeSplay) ? C.fg : (col===C.nodeAVL ? "#fff" : "#fff");
  if (col === C.nodeHL) ctx.fillStyle = C.bg;
  if (col === C.nodePath) ctx.fillStyle = C.bg;
  if (col === C.green) ctx.fillStyle = C.bg;
  ctx.textAlign = "center";
  ctx.textBaseline = "middle";
  ctx.fillText(nd.key, cx, cy);

  // Sub-labels (bf, color, priority)
  ctx.font = "bold 8px 'JetBrains Mono', monospace";
  ctx.fillStyle = "rgba(255,255,255,0.5)";
  if (type==="avl" && nd.bf != null) {
    ctx.fillText("bf:" + nd.bf, cx, cy + r + 9);
  }
  if (type==="treap" && nd.priority != null) {
    ctx.fillText("p:" + nd.priority, cx, cy + r + 9);
  }
  if (type==="rbt") {
    const dot = nd.color===1 ? "●R" : "●B";
    ctx.fillStyle = nd.color===1 ? C.red : C.fgDim;
    ctx.fillText(dot, cx, cy + r + 9);
  }

  ctx.restore();
}

function renderTrie(st, W, H) {
  const by_id = {};
  st.nodes.forEach(n => { by_id[n.id] = n; });

  // Draw edges
  st.nodes.forEach(nd => {
    if (nd.parent === -1 || !by_id[nd.parent] || !nd.layout || !by_id[nd.parent].layout) return;
    const p = worldToScreen(by_id[nd.parent].layout.x, by_id[nd.parent].layout.y);
    const c = worldToScreen(nd.layout.x, nd.layout.y);
    ctx.save();
    ctx.beginPath(); ctx.moveTo(p.x,p.y); ctx.lineTo(c.x,c.y);
    ctx.strokeStyle = C.borderHi; ctx.lineWidth=1.5; ctx.stroke();
    // Edge label (char)
    ctx.font="bold 10px 'JetBrains Mono'"; ctx.fillStyle=C.accent;
    ctx.textAlign="center"; ctx.textBaseline="middle";
    ctx.fillText(nd.char, (p.x+c.x)/2, (p.y+c.y)/2);
    ctx.restore();
  });

  // Draw nodes
  st.nodes.forEach(nd => {
    if (!nd.layout) return;
    const s = worldToScreen(nd.layout.x, nd.layout.y);
    const r = 18;
    ctx.save();
    ctx.beginPath(); ctx.arc(s.x, s.y, r, 0, Math.PI*2);
    ctx.fillStyle = nd.is_end ? C.accent3 : C.card;
    ctx.fill();
    ctx.strokeStyle = nd.is_end ? C.green : C.borderHi; ctx.lineWidth=1.5; ctx.stroke();
    ctx.font="bold 11px 'JetBrains Mono'";
    ctx.fillStyle = nd.is_end ? C.bg : C.fg;
    ctx.textAlign="center"; ctx.textBaseline="middle";
    ctx.fillText(nd.char==="ROOT"?"*":nd.char, s.x, s.y);
    ctx.restore();
  });
}

function renderBTree(st, W, H) {
  const by_id = {};
  st.nodes.forEach(n => {
    const nid = Array.isArray(n.id) ? n.id[0] : n.id;
    by_id[nid] = n;
  });

  const drawBNode = (nd) => {
    if (!nd.layout) return;
    const s = worldToScreen(nd.layout.x, nd.layout.y);
    const keys = nd.keys || [];
    const bw   = 30 * keys.length + 20;
    const bh   = 36;

    ctx.save();
    ctx.fillStyle   = C.card;
    ctx.strokeStyle = C.borderHi;
    ctx.lineWidth   = 1.5;
    roundRect(ctx, s.x - bw/2, s.y - bh/2, bw, bh, 6);
    ctx.fill(); ctx.stroke();

    keys.forEach((k, i) => {
      const kx = s.x - bw/2 + 20 + i*30;
      if (i > 0) {
        ctx.strokeStyle = C.borderHi; ctx.lineWidth=1;
        ctx.beginPath(); ctx.moveTo(kx-10, s.y-bh/2+4); ctx.lineTo(kx-10, s.y+bh/2-4); ctx.stroke();
      }
      ctx.font = FONT_N; ctx.fillStyle = C.accent;
      ctx.textAlign="center"; ctx.textBaseline="middle";
      ctx.fillText(k, kx, s.y);
    });
    ctx.restore();
  };

  // Draw edges
  st.nodes.forEach(nd => {
    if (!nd.layout) return;
    const nid = Array.isArray(nd.id) ? nd.id[0] : nd.id;
    const s   = worldToScreen(nd.layout.x, nd.layout.y);
    (nd.children||[]).forEach(cid => {
      const child = by_id[cid];
      if (!child || !child.layout) return;
      const t = worldToScreen(child.layout.x, child.layout.y);
      ctx.save(); ctx.beginPath(); ctx.moveTo(s.x, s.y+18); ctx.lineTo(t.x, t.y-18);
      ctx.strokeStyle=C.borderHi; ctx.lineWidth=1.5; ctx.stroke(); ctx.restore();
    });
  });

  st.nodes.forEach(nd => drawBNode(nd));
}

function roundRect(ctx, x, y, w, h, r) {
  ctx.beginPath();
  ctx.moveTo(x+r, y);
  ctx.lineTo(x+w-r, y); ctx.quadraticCurveTo(x+w,y,x+w,y+r);
  ctx.lineTo(x+w, y+h-r); ctx.quadraticCurveTo(x+w,y+h,x+w-r,y+h);
  ctx.lineTo(x+r, y+h); ctx.quadraticCurveTo(x,y+h,x,y+h-r);
  ctx.lineTo(x, y+r); ctx.quadraticCurveTo(x,y,x+r,y);
  ctx.closePath();
}

// ─────────────────────────────────────────────
//  3D Renderer — Three.js
// ─────────────────────────────────────────────

let scene3d, camera3d, renderer3d, orbitAngle=0, orbitActive=false;
const obj3dMap = {};

function init3D() {
  scene3d = new THREE.Scene();
  scene3d.background = new THREE.Color(0x0a0c0f);
  scene3d.fog = new THREE.Fog(0x0a0c0f, 8, 30);

  const W = threeDiv.clientWidth || 800;
  const H = threeDiv.clientHeight || 500;

  camera3d = new THREE.PerspectiveCamera(60, W/H, 0.1, 100);
  camera3d.position.set(0, 2, 6);
  camera3d.lookAt(0, 0, 0);

  renderer3d = new THREE.WebGLRenderer({ antialias:true });
  renderer3d.setPixelRatio(devicePixelRatio);
  renderer3d.setSize(W, H);
  renderer3d.shadowMap.enabled = true;
  threeDiv.innerHTML = "";
  threeDiv.appendChild(renderer3d.domElement);

  // Lighting
  const amb = new THREE.AmbientLight(0x223344, 1.2);
  scene3d.add(amb);
  const dir = new THREE.DirectionalLight(0x00d4ff, 1.5);
  dir.position.set(5,8,5); dir.castShadow=true; scene3d.add(dir);
  const pt = new THREE.PointLight(0x7c4dff, 2, 15);
  pt.position.set(-3,4,-3); scene3d.add(pt);

  // Grid
  const gridHelper = new THREE.GridHelper(20, 20, 0x1e2430, 0x141820);
  gridHelper.position.y = -2;
  scene3d.add(gridHelper);

  animate3D();
}

function animate3D() {
  requestAnimationFrame(animate3D);
  if (!renderer3d) return;
  orbitAngle += 0.003;
  camera3d.position.x = Math.sin(orbitAngle) * 7;
  camera3d.position.z = Math.cos(orbitAngle) * 7;
  camera3d.lookAt(0, 0, 0);
  renderer3d.render(scene3d, camera3d);
}

function render3D(st) {
  if (!scene3d) init3D();

  // Remove old node meshes
  Object.values(obj3dMap).forEach(o => { scene3d.remove(o.mesh); scene3d.remove(o.edge); });
  for (const k in obj3dMap) delete obj3dMap[k];

  if (!st || !st.nodes || !st.nodes.length) return;

  const nil = st.nil ?? -1;
  const byId = {};
  st.nodes.forEach(n => { byId[n.id] = n; });

  const SCALE = 1.6;

  // Nodes
  st.nodes.forEach(nd => {
    if (nd.id === nil || !nd.layout3d) return;
    const l = nd.layout3d;

    const geo  = new THREE.SphereGeometry(0.28, 24, 24);
    const col  = nodeColor3d(nd);
    const mat  = new THREE.MeshStandardMaterial({
      color: col, emissive: col, emissiveIntensity: 0.3,
      roughness: 0.4, metalness: 0.7
    });
    const mesh = new THREE.Mesh(geo, mat);
    mesh.position.set(l.x * SCALE, l.y * SCALE, (l.z||0) * SCALE);
    mesh.castShadow = true;
    scene3d.add(mesh);

    // Label (canvas texture)
    const label = makeLabel3D(String(nd.key));
    label.position.set(l.x*SCALE, l.y*SCALE + 0.38, (l.z||0)*SCALE);
    scene3d.add(label);

    obj3dMap[nd.id] = { mesh, edge:null, label };
  });

  // Edges
  st.nodes.forEach(nd => {
    if (nd.id === nil || !nd.layout3d) return;
    ["left","right"].forEach(dir => {
      const cid = nd[dir];
      if (cid == null || cid === -1 || cid === nil) return;
      const child = byId[cid];
      if (!child || !child.layout3d) return;
      const l1 = nd.layout3d, l2 = child.layout3d;
      const p1 = new THREE.Vector3(l1.x*SCALE, l1.y*SCALE, (l1.z||0)*SCALE);
      const p2 = new THREE.Vector3(l2.x*SCALE, l2.y*SCALE, (l2.z||0)*SCALE);
      const dir3 = new THREE.Vector3().subVectors(p2, p1);
      const len  = dir3.length();
      const geo  = new THREE.CylinderGeometry(0.03, 0.03, len, 8);
      const mat  = new THREE.MeshStandardMaterial({ color:0x2d3a50, emissive:0x1e2430 });
      const edge = new THREE.Mesh(geo, mat);
      edge.position.copy(p1).add(p2).multiplyScalar(0.5);
      edge.quaternion.setFromUnitVectors(
        new THREE.Vector3(0,1,0),
        dir3.clone().normalize()
      );
      scene3d.add(edge);
    });
  });
}

function nodeColor3d(nd) {
  if (state.found.has(nd.id))     return 0x00e676;
  if (state.highlight.has(nd.id)) return 0xffd700;
  if (state.pathNodes.has(nd.id)) return 0x00ff9f;
  const type = nd.type || state.struct;
  if (type==="rbt")   return nd.color===1 ? 0xcc2200 : 0x1a2030;
  if (type==="heap")  return 0x5500cc;
  if (type==="treap") return 0x004466;
  if (type==="splay") return 0x220055;
  return 0x0077cc;
}

function makeLabel3D(text) {
  const c = document.createElement("canvas");
  c.width=64; c.height=32;
  const cx=c.getContext("2d");
  cx.fillStyle="rgba(0,0,0,0)"; cx.fillRect(0,0,64,32);
  cx.fillStyle="#d4dde8"; cx.font="bold 18px 'JetBrains Mono',monospace";
  cx.textAlign="center"; cx.textBaseline="middle"; cx.fillText(text,32,16);
  const tex = new THREE.CanvasTexture(c);
  const mat = new THREE.MeshBasicMaterial({ map:tex, transparent:true, depthWrite:false });
  const geo = new THREE.PlaneGeometry(0.5, 0.25);
  return new THREE.Mesh(geo, mat);
}

// ─────────────────────────────────────────────
//  Pan & Zoom (2D)
// ─────────────────────────────────────────────

canvas.addEventListener("mousedown", e => {
  if (e.button!==0) return;
  state.drag = { active:true, startX:e.clientX, startY:e.clientY, camX:state.cam.x, camY:state.cam.y };
  canvas.style.cursor = "grabbing";
});
window.addEventListener("mousemove", e => {
  if (state.drag.active) {
    const dx = (e.clientX - state.drag.startX) / (state.cam.scale * (canvas.width/devicePixelRatio)*0.35);
    const dy = (e.clientY - state.drag.startY) / (state.cam.scale * (canvas.height/devicePixelRatio)*0.65);
    state.cam.x = state.drag.camX + dx;
    state.cam.y = state.drag.camY + dy;
    if (state.viewMode==="2d") render2D();
  }
  // Hover detection
  if (!state.drag.active && state.viewMode==="2d") detectHover(e);
});
window.addEventListener("mouseup", () => {
  state.drag.active = false;
  canvas.style.cursor = "grab";
});
canvas.addEventListener("wheel", e => {
  e.preventDefault();
  const delta = e.deltaY > 0 ? 0.9 : 1.1;
  state.cam.scale = Math.max(0.3, Math.min(5, state.cam.scale * delta));
  if (state.viewMode==="2d") render2D();
}, { passive:false });

function detectHover(e) {
  const rect = canvas.getBoundingClientRect();
  const mx = e.clientX - rect.left;
  const my = e.clientY - rect.top;
  const st = state.treeData;
  if (!st || !st.nodes) { hideTooltip(); return; }
  const nil = st.nil ?? -1;
  let hit = null;
  for (const nd of st.nodes) {
    if (nd.id === nil || !nd.layout) continue;
    const s = worldToScreen(nd.layout.x, nd.layout.y);
    const dx = mx - s.x, dy = my - s.y;
    if (Math.sqrt(dx*dx+dy*dy) < NODE_R + 4) { hit=nd; break; }
  }
  if (hit !== state.hoveredNode) {
    state.hoveredNode = hit;
    if (hit) showTooltip(hit, e.clientX, e.clientY);
    else     hideTooltip();
  }
}

function showTooltip(nd, mx, my) {
  const tt = $("node-tooltip");
  tt.classList.remove("hidden");
  const rows = [["key", nd.key], ["value", nd.value??nd.key]];
  if (nd.height != null) rows.push(["height",  nd.height]);
  if (nd.bf     != null) rows.push(["balance", nd.bf]);
  if (nd.size   != null) rows.push(["size",    nd.size]);
  if (nd.color  != null) rows.push(["color",   nd.color===1?"red":"black"]);
  if (nd.priority!=null) rows.push(["priority",nd.priority]);
  if (nd.layout) {
    rows.push(["depth", nd.layout.depth]);
  }
  tt.innerHTML = rows.map(([k,v]) =>
    `<div class="tt-row"><span class="tt-key">${k}</span><span class="tt-val">${v}</span></div>`
  ).join("");
  const W = window.innerWidth, H = window.innerHeight;
  let tx = mx + 12, ty = my - 8;
  if (tx + 180 > W) tx = mx - 190;
  if (ty + 120 > H) ty = my - 120;
  tt.style.left = tx + "px";
  tt.style.top  = ty + "px";
}
function hideTooltip() {
  $("node-tooltip").classList.add("hidden");
  state.hoveredNode = null;
}

// ─────────────────────────────────────────────
//  Operations
// ─────────────────────────────────────────────

async function doInsert() {
  const key = parseInt($("key-input").value);
  const val = parseInt($("val-input").value) || key;
  if (isNaN(key)) { showToast("Enter a valid key","warn"); return; }

  const t0 = performance.now();
  let r;

  if (state.struct === "trie") {
    const word = $("key-input").value.trim();
    r = await api(`/trie/insert`, { word });
  } else {
    r = await api(`/${state.struct}/insert`, { key, value:val });
  }

  const ms = (performance.now()-t0).toFixed(1);
  $("stat-time").textContent = ms + "ms";
  $("stat-lastop").textContent = "insert " + key;

  if (r?.state) {
    state.highlight.clear(); state.pathNodes.clear(); state.found.clear();
    applyState(r.state);
    flashNode(key);
  }
  $("key-input").value = "";
}

async function doDelete() {
  const key = parseInt($("key-input").value);
  if (isNaN(key)) { showToast("Enter a valid key","warn"); return; }
  const r = await api(`/${state.struct}/delete`, { key });
  $("stat-lastop").textContent = "delete " + key;
  if (r?.state) { state.highlight.clear(); state.pathNodes.clear(); applyState(r.state); }
  $("key-input").value = "";
}

async function doSearch() {
  const key = parseInt($("key-input").value);
  if (isNaN(key)) return;
  const r = await api(`/${state.struct}/search`, { key });
  $("stat-lastop").textContent = "search " + key;
  if (r) {
    state.pathNodes.clear(); state.found.clear();
    (r.path || []).forEach(id => state.pathNodes.add(id));
    if (r.found) {
      const st = state.treeData;
      if (st?.nodes) {
        const nd = st.nodes.find(n => n.key === key);
        if (nd) state.found.add(nd.id);
      }
    }
    if (state.viewMode==="2d") render2D();
    setTimeout(() => { state.pathNodes.clear(); state.found.clear(); render2D(); }, 2500);
    showToast(r.found ? `Found key ${key}` : `Key ${key} not found`, r.found ? "ok" : "warn");
  }
}

async function doBulkInsert() {
  const raw = $("bulk-input").value;
  const keys = raw.split(/[,\s]+/).map(s=>parseInt(s)).filter(k=>!isNaN(k));
  if (!keys.length) { showToast("Enter comma-separated integers","warn"); return; }

  if (state.struct==="trie") {
    // word by word
    for (const w of raw.split(/\s+/).filter(Boolean)) {
      await api("/trie/insert", {word:w});
    }
    const r2 = await api(`/set_structure`, {structure:"trie"});
    if (r2?.state) applyState(r2.state);
    return;
  }

  const r = await api(`/${state.struct}/bulk_insert`, { keys });
  if (r?.state) { state.highlight.clear(); state.pathNodes.clear(); applyState(r.state); }
  $("bulk-input").value = "";
}

async function doGenerate() {
  const r = await api("/generate", { structure: state.struct, n: 14 });
  if (r?.state) { state.highlight.clear(); state.pathNodes.clear(); state.found.clear(); applyState(r.state); }
}

async function doReset() {
  const r = await api("/reset", { structure: state.struct });
  if (r?.state) { state.highlight.clear(); state.pathNodes.clear(); state.found.clear(); applyState(r.state); }
}

// ─────────────────────────────────────────────
//  Flash animation for new node
// ─────────────────────────────────────────────

function flashNode(key) {
  const st = state.treeData;
  if (!st?.nodes) return;
  const nd = st.nodes.find(n => n.key===key);
  if (!nd) return;
  state.highlight.add(nd.id);
  render2D();
  setTimeout(() => { state.highlight.delete(nd.id); render2D(); }, 1200);
}

// ─────────────────────────────────────────────
//  Traversal
// ─────────────────────────────────────────────

async function showTraversal(order="inorder") {
  const r = await api("/avl/traversal", { order });
  if (!r?.result) return;
  const keys = r.result.flat ? r.result.flat() : r.result;
  const strip = $("traversal-strip");
  strip.classList.remove("hidden");
  strip.innerHTML = `<span style="color:var(--fg-dim);margin-right:4px">${order}:</span>`;

  // Animate reveal one by one
  for (let i=0; i<keys.length; i++) {
    await delay(Math.min(state.speed * 0.4, 200));
    const badge = document.createElement("span");
    badge.className = "tr-badge";
    badge.textContent = keys[i];
    strip.appendChild(badge);
    if (i < keys.length-1) {
      const sep = document.createElement("span");
      sep.className="tr-sep"; sep.textContent="→";
      strip.appendChild(sep);
    }
    // Highlight in tree
    const nd = state.treeData?.nodes?.find(n=>n.key===keys[i]);
    if (nd) { state.pathNodes.add(nd.id); render2D(); }
  }
  setTimeout(() => {
    strip.classList.add("hidden");
    state.pathNodes.clear(); render2D();
  }, 4000);
}

// ─────────────────────────────────────────────
//  Benchmark Modal
// ─────────────────────────────────────────────

async function showBenchmark() {
  showModal("<h2>⚡ Benchmark — Inserting 1000 random keys</h2><p style='color:var(--fg-dim);font-size:12px;font-family:var(--font-mono)'>Running…</p>");
  const r = await api("/benchmark", { n:1000 });
  if (!r?.results) { showModal("<h2>Error</h2><p>Backend not available</p>"); return; }
  const res = r.results;
  const structs = Object.keys(res);
  const maxMs = Math.max(...structs.map(s => res[s].insert_ms || res[s].build_ms || 1));
  const maxCmp = Math.max(...structs.map(s => res[s].comparisons || 1));

  let html = `<h2>⚡ Benchmark — n=${r.n}</h2>
  <div class="modal-grid">
    ${structs.map(s=>`
      <div class="modal-stat">
        <div class="ms-label">${s.toUpperCase()}</div>
        <div class="ms-val">${((res[s].insert_ms||res[s].build_ms)||0).toFixed(1)}ms</div>
      </div>`).join("")}
  </div>
  <div class="bar-chart">
    <div style="font-family:var(--font-mono);font-size:10px;color:var(--fg-dim);margin-bottom:8px;letter-spacing:1px">INSERT TIME (ms)</div>
    ${structs.map((s,i)=>`
      <div class="bar-row">
        <div class="bar-label">${s}</div>
        <div class="bar-track"><div class="bar-fill ${i===1?'alt':i===2?'alt2':''}" style="width:${((res[s].insert_ms||res[s].build_ms||0)/maxMs*100).toFixed(1)}%"></div></div>
        <div class="bar-num">${((res[s].insert_ms||res[s].build_ms)||0).toFixed(2)}</div>
      </div>`).join("")}
  </div>
  <div class="bar-chart" style="margin-top:16px">
    <div style="font-family:var(--font-mono);font-size:10px;color:var(--fg-dim);margin-bottom:8px;letter-spacing:1px">COMPARISONS</div>
    ${structs.map((s,i)=>`
      <div class="bar-row">
        <div class="bar-label">${s}</div>
        <div class="bar-track"><div class="bar-fill ${i===1?'alt':i===2?'alt2':''}" style="width:${((res[s].comparisons||0)/maxCmp*100).toFixed(1)}%"></div></div>
        <div class="bar-num">${(res[s].comparisons||0).toLocaleString()}</div>
      </div>`).join("")}
  </div>`;
  showModal(html);
}

// ─────────────────────────────────────────────
//  Analytics Modal
// ─────────────────────────────────────────────

async function showAnalytics() {
  const r = await api("/analytics/avl");
  if (!r) { showModal("<h2>Analytics unavailable</h2>"); return; }
  let html = `<h2>📊 Tree Analytics</h2>
  <div class="modal-grid">
    <div class="modal-stat"><div class="ms-label">Height</div><div class="ms-val">${r.height??'—'}</div></div>
    <div class="modal-stat"><div class="ms-label">Nodes</div><div class="ms-val">${r.size??'—'}</div></div>
    <div class="modal-stat"><div class="ms-label">Diameter</div><div class="ms-val">${r.diameter??'—'}</div></div>
    <div class="modal-stat"><div class="ms-label">Avg Depth</div><div class="ms-val">${r.avg_depth?.toFixed(2)??'—'}</div></div>
    <div class="modal-stat"><div class="ms-label">Leaves</div><div class="ms-val">${r.leaf_count??'—'}</div></div>
    <div class="modal-stat"><div class="ms-label">Balanced</div><div class="ms-val" style="color:${r.is_balanced?'var(--green)':'var(--red)'}">${r.is_balanced?'✓ Yes':'✗ No'}</div></div>
    <div class="modal-stat"><div class="ms-label">Comparisons</div><div class="ms-val">${r.comparisons??'—'}</div></div>
    <div class="modal-stat"><div class="ms-label">Rotations</div><div class="ms-val">${r.rotations??'—'}</div></div>
  </div>
  <table class="modal-table">
    <tr><th>Metric</th><th>Actual</th><th>Optimal (log₂n)</th></tr>
    <tr><td>Height</td><td class="num">${r.height??'—'}</td><td class="num">${r.size?Math.ceil(Math.log2(r.size+1)):'—'}</td></tr>
    <tr><td>Rotations/n</td><td class="num">${r.size?(r.rotations/r.size).toFixed(2):'—'}</td><td class="num">&lt;2</td></tr>
    <tr><td>Comparisons/n</td><td class="num">${r.size?(r.comparisons/r.size).toFixed(2):'—'}</td><td class="num">~1.39·log₂n</td></tr>
  </table>`;
  showModal(html);
}

// ─────────────────────────────────────────────
//  Huffman Modal
// ─────────────────────────────────────────────

async function showHuffman() {
  const text = prompt("Enter text to encode:", "hello world");
  if (!text) return;
  const r = await api("/huffman/build", { text });
  if (!r) return;
  const codes = r.codes || {};
  const chars = Object.keys(codes).sort();
  let html = `<h2>Huffman Encoding — "${text.slice(0,30)}"</h2>
  <div class="huffman-grid">
    ${chars.map(c=>`
      <div class="huffman-cell">
        <div class="hc-char">${c===' '?'⎵':c}</div>
        <div class="hc-code">${codes[c]}</div>
      </div>`).join("")}
  </div>
  <p style="margin-top:14px;font-family:var(--font-mono);font-size:11px;color:var(--fg-dim)">
    Original: ${text.length*8} bits → Compressed: ${chars.reduce((acc,c)=>{
      const freq=(text.split(c).length-1); return acc+freq*(codes[c]||"").length;
    },0)} bits
  </p>`;
  showModal(html);
  if (r.nodes?.length) {
    // Temporarily show huffman tree
    applyState({ type:"avl", nodes:r.nodes, root:r.root, stats:{} });
  }
}

// ─────────────────────────────────────────────
//  Dijkstra Modal
// ─────────────────────────────────────────────

async function showDijkstra() {
  const edgesStr = prompt("Enter edges (u,v,w per line):", "0,1,4\n0,2,1\n2,1,2\n1,3,1");
  if (!edgesStr) return;
  const edges = edgesStr.split(/\n|;/).map(l => l.split(",").map(Number)).filter(e=>e.length===3);
  const n = Math.max(...edges.flat()) + 1;
  const r = await api("/algo/dijkstra", { n, src:0, edges });
  if (!r) return;
  let html = `<h2>Dijkstra — Shortest Paths from node 0</h2>
  <table class="modal-table">
    <tr><th>Node</th><th>Distance from 0</th></tr>
    ${(r.distances||[]).map((d,i)=>`<tr><td>${i}</td><td class="num">${d>1e8?"∞":d}</td></tr>`).join("")}
  </table>`;
  showModal(html);
}

// ─────────────────────────────────────────────
//  Algo Panel
// ─────────────────────────────────────────────

function showAlgoPanel(algo) {
  state.algoMode = algo;
  const panel = $("algo-panel");
  panel.style.display = "flex";

  if (algo === "traversal") {
    panel.innerHTML = `
      <span style="font-size:12px;color:var(--fg-dim);font-family:var(--font-mono)">Traversal:</span>
      <button class="btn-neutral" onclick="showTraversal('inorder')">Inorder</button>
      <button class="btn-neutral" onclick="showTraversal('preorder')">Preorder</button>
      <button class="btn-neutral" onclick="showTraversal('postorder')">Postorder</button>
      <button class="btn-neutral" onclick="showTraversal('level_order')">Level Order</button>
      <button class="btn-neutral" onclick="closeAlgoPanel()">✕</button>`;
  } else if (algo === "lca") {
    panel.innerHTML = `
      <span style="font-size:12px;color:var(--fg-dim);font-family:var(--font-mono)">LCA:</span>
      <input type="number" id="lca-a" placeholder="Node A" style="width:80px"/>
      <input type="number" id="lca-b" placeholder="Node B" style="width:80px"/>
      <button class="btn-accent2" onclick="doLCA()">Find LCA</button>
      <button class="btn-neutral" onclick="closeAlgoPanel()">✕</button>`;
  } else if (algo === "kth") {
    panel.innerHTML = `
      <span style="font-size:12px;color:var(--fg-dim);font-family:var(--font-mono)">Kth Smallest:</span>
      <input type="number" id="kth-k" placeholder="k" style="width:80px" min="1"/>
      <button class="btn-accent2" onclick="doKth()">Find</button>
      <button class="btn-neutral" onclick="closeAlgoPanel()">✕</button>`;
  } else if (algo === "floor_ceil") {
    panel.innerHTML = `
      <span style="font-size:12px;color:var(--fg-dim);font-family:var(--font-mono)">Floor/Ceil:</span>
      <input type="number" id="fc-key" placeholder="Key"/>
      <button class="btn-accent2" onclick="doFloorCeil()">Find</button>
      <button class="btn-neutral" onclick="closeAlgoPanel()">✕</button>`;
  } else if (algo === "huffman") {
    closeAlgoPanel(); showHuffman();
  } else if (algo === "dijkstra") {
    closeAlgoPanel(); showDijkstra();
  } else {
    panel.style.display = "none";
  }
}

function closeAlgoPanel() {
  $("algo-panel").style.display = "none";
  state.algoMode = null;
}

async function doLCA() {
  const a = parseInt($("lca-a")?.value), b=parseInt($("lca-b")?.value);
  if (isNaN(a)||isNaN(b)) return;
  const r = await api("/avl/lca", {a,b});
  if (r) {
    showToast(`LCA(${a}, ${b}) = ${r.lca}`, "ok");
    const nd = state.treeData?.nodes?.find(n=>n.key===r.lca);
    if (nd) { state.highlight.add(nd.id); render2D(); setTimeout(()=>{state.highlight.clear();render2D();},2500); }
  }
}

async function doKth() {
  const k = parseInt($("kth-k")?.value);
  if (isNaN(k)||k<1) return;
  const r = await api("/avl/kth", {k});
  if (r) {
    showToast(`${k}th smallest = ${r.key}`, "ok");
    const nd = state.treeData?.nodes?.find(n=>n.key===r.key);
    if (nd) { state.highlight.add(nd.id); render2D(); setTimeout(()=>{state.highlight.clear();render2D();},2500); }
  }
}

async function doFloorCeil() {
  const key = parseInt($("fc-key")?.value);
  if (isNaN(key)) return;
  const r = await api("/avl/floor_ceil", {key});
  if (r) showToast(`floor(${key})=${r.floor}  ceil(${key})=${r.ceil}`, "ok");
}

// ─────────────────────────────────────────────
//  Modal helpers
// ─────────────────────────────────────────────

function showModal(html) {
  $("modal-content").innerHTML = html;
  $("modal-overlay").classList.remove("hidden");
}
function closeModal() {
  $("modal-overlay").classList.add("hidden");
}
$("modal-close").addEventListener("click", closeModal);
$("modal-overlay").addEventListener("click", e => { if (e.target===$("modal-overlay")) closeModal(); });

// ─────────────────────────────────────────────
//  Toast notifications
// ─────────────────────────────────────────────

function showToast(msg, type="ok") {
  const existing = document.querySelector(".toast");
  if (existing) existing.remove();
  const t = document.createElement("div");
  t.className = "toast";
  const col = type==="ok"?"var(--green)":type==="warn"?"var(--orange)":"var(--red)";
  t.style.cssText = `position:fixed;bottom:60px;right:20px;background:var(--bg-card);
    border:1px solid ${col};border-radius:6px;padding:10px 16px;font-family:var(--font-mono);
    font-size:12px;color:${col};z-index:2000;animation:slideIn 0.2s ease;max-width:320px;
    box-shadow:0 4px 20px rgba(0,0,0,0.4)`;
  t.textContent = msg;
  document.body.appendChild(t);
  setTimeout(() => t.style.opacity="0", 2500);
  setTimeout(() => t.remove(), 3000);
}

// ─────────────────────────────────────────────
//  Event listeners
// ─────────────────────────────────────────────

$("btn-insert").addEventListener("click", doInsert);
$("btn-delete").addEventListener("click", doDelete);
$("btn-search").addEventListener("click", doSearch);
$("btn-bulk").addEventListener("click", doBulkInsert);
$("btn-reset").addEventListener("click", doReset);
$("btn-generate").addEventListener("click", doGenerate);
$("btn-benchmark").addEventListener("click", showBenchmark);
$("btn-analytics").addEventListener("click", showAnalytics);

$("key-input").addEventListener("keydown", e => { if(e.key==="Enter") doInsert(); });
$("bulk-input").addEventListener("keydown", e => { if(e.key==="Enter") doBulkInsert(); });

// Speed slider
$("speed-slider").addEventListener("input", e => {
  state.speed = parseInt(e.target.value);
  $("speed-val").textContent = state.speed + "ms";
});

// View toggle
document.querySelectorAll(".tgl").forEach(btn => {
  btn.addEventListener("click", () => {
    document.querySelectorAll(".tgl").forEach(b=>b.classList.remove("active"));
    btn.classList.add("active");
    state.viewMode = btn.dataset.view;
    if (state.viewMode==="2d") {
      canvas.style.display="block"; threeDiv.style.display="none";
      render2D();
    } else {
      canvas.style.display="none"; threeDiv.style.display="block";
      if (state.treeData) render3D(state.treeData);
    }
  });
});

// Struct nav
document.querySelectorAll(".nav-btn[data-struct]").forEach(btn => {
  btn.addEventListener("click", async () => {
    document.querySelectorAll(".nav-btn[data-struct]").forEach(b=>b.classList.remove("active"));
    btn.classList.add("active");
    state.struct = btn.dataset.struct;
    const meta = STRUCT_META[state.struct] || { title:state.struct, meta:"" };
    $("struct-title").textContent = meta.title;
    $("struct-meta").textContent  = meta.meta;

    // Show/hide trie word insert
    $("btn-trie-word").style.display = state.struct==="trie" ? "inline-block" : "none";

    state.highlight.clear(); state.pathNodes.clear(); state.found.clear();
    const r = await api("/set_structure", { structure: state.struct });
    if (r?.state) applyState(r.state);
    else { state.treeData=null; render2D(); }

    closeAlgoPanel();
  });
});

// Algo nav
document.querySelectorAll(".nav-btn[data-algo]").forEach(btn => {
  btn.addEventListener("click", () => {
    showAlgoPanel(btn.dataset.algo);
  });
});

// ─────────────────────────────────────────────
//  Heap-specific: extract button
// ─────────────────────────────────────────────

async function heapExtract() {
  const r = await api("/heap/extract");
  if (r?.extracted) {
    showToast(`Extracted top: ${r.extracted.key}`, "ok");
    $("stat-lastop").textContent = "extract " + r.extracted.key;
    if (r.state) applyState(r.state);
  } else if (r?.error) {
    showToast(r.error, "warn");
  }
}

// Patch delete button for heap → extract
const origDelete = $("btn-delete").onclick;
$("btn-delete").addEventListener("click", () => {
  if (state.struct==="heap") { heapExtract(); return; }
});

// ─────────────────────────────────────────────
//  Resize observer
// ─────────────────────────────────────────────

const ro = new ResizeObserver(() => {
  resizeCanvas();
  if (renderer3d) {
    const W=threeDiv.clientWidth, H=threeDiv.clientHeight;
    renderer3d.setSize(W, H);
    camera3d.aspect = W/H;
    camera3d.updateProjectionMatrix();
  }
});
ro.observe($("canvas-area"));

// ─────────────────────────────────────────────
//  Utility
// ─────────────────────────────────────────────

const delay = ms => new Promise(r => setTimeout(r, ms));

// Add slide-in keyframe
const styleEl = document.createElement("style");
styleEl.textContent = `@keyframes slideIn { from{transform:translateX(40px);opacity:0} to{transform:translateX(0);opacity:1} }`;
document.head.appendChild(styleEl);

// ─────────────────────────────────────────────
//  Init
// ─────────────────────────────────────────────

async function init() {
  resizeCanvas();
  initWebSocket();

  // Check backend
  try {
    const status = await apiGet("/status");
    if (status?.ok) {
      $("backend-label").textContent = status.cpp_backend ? "C++ Engine" : "Python Fallback";
      document.querySelector(".backend-badge").classList.add(status.cpp_backend ? "cpp" : "py");
    }
  } catch(e) {
    $("backend-label").textContent = "Offline (Demo)";
  }

  // Set initial struct label
  const meta = STRUCT_META["avl"];
  $("struct-title").textContent = meta.title;
  $("struct-meta").textContent  = meta.meta;

  // Generate initial tree
  await doGenerate();
}

init();
