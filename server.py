"""
Tree & Heap Visualizer — Python Backend
Enterprise-grade Flask API server bridging C++ core_engine via pybind11
"""

from __future__ import annotations
import sys
import os
import json
import time
import math
import random
import traceback
from typing import Any, Dict, List, Optional, Tuple
from dataclasses import dataclass, asdict
from functools import wraps

from flask import Flask, request, jsonify, send_from_directory
from flask_cors import CORS
from flask_socketio import SocketIO, emit

# ── Try to import C++ core engine; fall back to pure-Python stubs ──
try:
    sys.path.insert(0, os.path.dirname(__file__))
    import core_engine as _ce
    CPP_BACKEND = True
    print("[INFO] C++ core_engine loaded successfully.")
except ImportError:
    CPP_BACKEND = False
    print("[WARN] C++ core_engine not found — using Python fallback stubs.")
    _ce = None  # type: ignore

# ─────────────────────────────────────────────
#  Pure-Python Fallback Data Structures
#  (used when C++ module is not compiled)
# ─────────────────────────────────────────────

class PyAVLNode:
    __slots__ = ("key","val","height","left","right","size","bf")
    def __init__(self, key, val=0):
        self.key=key; self.val=val; self.height=1; self.left=None; self.right=None
        self.size=1; self.bf=0

class PyAVLTree:
    def __init__(self):
        self.root = None
        self.trace: list = []
        self.total_comparisons = 0
        self.total_rotations = 0
        self._nodes_flat: list = []

    def _h(self, n): return n.height if n else 0
    def _sz(self, n): return n.size if n else 0
    def _upd(self, n):
        if not n: return
        n.height = 1 + max(self._h(n.left), self._h(n.right))
        n.bf = self._h(n.left) - self._h(n.right)
        n.size = 1 + self._sz(n.left) + self._sz(n.right)

    def _rr(self, y):
        x=y.left; T2=x.right; x.right=y; y.left=T2; self._upd(y); self._upd(x); self.total_rotations+=1; return x
    def _rl(self, x):
        y=x.right; T2=y.left; y.left=x; x.right=T2; self._upd(x); self._upd(y); self.total_rotations+=1; return y

    def _insert(self, n, key, val):
        if not n: return PyAVLNode(key, val)
        self.total_comparisons += 1
        if   key < n.key: n.left  = self._insert(n.left,  key, val)
        elif key > n.key: n.right = self._insert(n.right, key, val)
        else: n.val=val; return n
        self._upd(n)
        if n.bf > 1:
            if key < n.left.key: return self._rr(n)
            n.left = self._rl(n.left); return self._rr(n)
        if n.bf < -1:
            if key > n.right.key: return self._rl(n)
            n.right = self._rr(n.right); return self._rl(n)
        return n

    def do_insert(self, key, val=0):
        self.root = self._insert(self.root, key, val)
        self.trace.append({"op":"insert","detail":f"Insert {key}"})

    def _min_n(self, n):
        while n.left: n=n.left
        return n

    def _delete(self, n, key):
        if not n: return n
        self.total_comparisons += 1
        if   key < n.key: n.left  = self._delete(n.left,  key)
        elif key > n.key: n.right = self._delete(n.right, key)
        else:
            if not n.left or not n.right:
                n = n.left or n.right
            else:
                s = self._min_n(n.right)
                n.key=s.key; n.val=s.val
                n.right = self._delete(n.right, s.key)
        if not n: return n
        self._upd(n)
        if n.bf > 1:
            if n.left.bf >= 0: return self._rr(n)
            n.left = self._rl(n.left); return self._rr(n)
        if n.bf < -1:
            if n.right.bf <= 0: return self._rl(n)
            n.right = self._rr(n.right); return self._rl(n)
        return n

    def do_delete(self, key):
        self.root = self._delete(self.root, key)
        self.trace.append({"op":"delete","detail":f"Delete {key}"})

    def search(self, key):
        path=[]; cur=self.root
        while cur:
            path.append(cur.key)
            if key==cur.key: break
            elif key<cur.key: cur=cur.left
            else: cur=cur.right
        return path

    def serialize(self):
        res=[]; ctr=[0]
        def dfs(n, pid):
            if not n: return -1
            nid=ctr[0]; ctr[0]+=1
            left_id=dfs(n.left, nid)
            right_id=dfs(n.right, nid)
            res.append({"id":nid,"key":n.key,"value":n.val,"height":n.height,
                        "bf":n.bf,"size":n.size,"left":left_id,"right":right_id,"parent":pid})
            return nid
        dfs(self.root, -1)
        return res

    def inorder(self):
        res=[]
        def dfs(n):
            if not n: return
            dfs(n.left); res.append(n.key); dfs(n.right)
        dfs(self.root); return res

    def preorder(self):
        res=[]
        def dfs(n):
            if not n: return
            res.append(n.key); dfs(n.left); dfs(n.right)
        dfs(self.root); return res

    def postorder(self):
        res=[]
        def dfs(n):
            if not n: return
            dfs(n.left); dfs(n.right); res.append(n.key)
        dfs(self.root); return res

    def level_order(self):
        if not self.root: return []
        from collections import deque
        q=deque([self.root]); res=[]
        while q:
            lvl=[]
            for _ in range(len(q)):
                n=q.popleft(); lvl.append(n.key)
                if n.left:  q.append(n.left)
                if n.right: q.append(n.right)
            res.append(lvl)
        return res

    def kth_smallest(self, k):
        def find(n, k):
            if not n: return -1
            ls = n.left.size if n.left else 0
            if k==ls+1: return n.key
            if k<=ls:   return find(n.left, k)
            return find(n.right, k-ls-1)
        return find(self.root, k)

    def lca(self, a, b):
        cur=self.root
        while cur:
            if a<cur.key and b<cur.key: cur=cur.left
            elif a>cur.key and b>cur.key: cur=cur.right
            else: return cur.key
        return -1

    def diameter(self):
        def helper(n):
            if not n: return 0,0
            lh,ld=helper(n.left); rh,rd=helper(n.right)
            return 1+max(lh,rh), max(ld,rd,lh+rh)
        return helper(self.root)[1]

    def floor_key(self, key):
        res=None; cur=self.root
        while cur:
            if cur.key==key: return key
            elif cur.key<key: res=cur.key; cur=cur.right
            else: cur=cur.left
        return res if res is not None else -1

    def ceil_key(self, key):
        res=None; cur=self.root
        while cur:
            if cur.key==key: return key
            elif cur.key>key: res=cur.key; cur=cur.left
            else: cur=cur.right
        return res if res is not None else -1

    def clear_trace(self): self.trace=[]; self.total_comparisons=0; self.total_rotations=0


class PyBinaryHeap:
    def __init__(self, min_heap=True):
        self.data: list = []
        self.is_min = min_heap
        self.trace: list = []
        self.comparisons = 0

    def _cmp(self, i, j):
        self.comparisons += 1
        return self.data[i]["key"] < self.data[j]["key"] if self.is_min else self.data[i]["key"] > self.data[j]["key"]

    def _up(self, i):
        while i>0:
            p=(i-1)//2
            if self._cmp(i,p): self.data[i],self.data[p]=self.data[p],self.data[i]; i=p
            else: break
        self.trace.append({"op":"heapify_up","detail":f"Bubble up to {i}"})

    def _down(self, i):
        n=len(self.data)
        while True:
            best=i; l=2*i+1; r=2*i+2
            if l<n and self._cmp(l,best): best=l
            if r<n and self._cmp(r,best): best=r
            if best==i: break
            self.data[i],self.data[best]=self.data[best],self.data[i]; i=best
        self.trace.append({"op":"heapify_down","detail":f"Sink to {i}"})

    def insert(self, key, val=0):
        self.data.append({"key":key,"value":val}); self._up(len(self.data)-1)

    def extract_top(self):
        if not self.data: raise RuntimeError("Heap empty")
        top=self.data[0]; self.data[0]=self.data[-1]; self.data.pop()
        if self.data: self._down(0)
        return top

    def peek_top(self): return self.data[0]["key"] if self.data else -1

    def build_heap(self, keys):
        self.data=[{"key":k,"value":0} for k in keys]
        for i in range(len(self.data)//2-1,-1,-1): self._down(i)

    def heap_sort(self):
        import copy; tmp=copy.deepcopy(self); res=[]
        while tmp.data: res.append(tmp.extract_top()["key"])
        return res

    def serialize(self):
        n=len(self.data)
        return [{"id":i,"key":self.data[i]["key"],"value":self.data[i]["value"],
                 "left":2*i+1 if 2*i+1<n else -1,
                 "right":2*i+2 if 2*i+2<n else -1,
                 "parent":(i-1)//2 if i>0 else -1} for i in range(n)]

    def clear_trace(self): self.trace=[]; self.comparisons=0


# ─────────────────────────────────────────────
#  Session State: one tree/heap per connection
# ─────────────────────────────────────────────

@dataclass
class SessionState:
    avl:    Any = None
    rbt:    Any = None
    heap:   Any = None
    treap:  Any = None
    splay:  Any = None
    btree:  Any = None
    trie:   Any = None
    dsu:    Any = None
    segtree: Any = None
    fenwick: Any = None
    huffman: Any = None
    persistent_seg: Any = None
    current_struct: str = "avl"
    animation_speed: float = 500.0  # ms per step
    active_algo: str = ""

_sessions: Dict[str, SessionState] = {}

def get_session(sid: str) -> SessionState:
    if sid not in _sessions:
        _sessions[sid] = SessionState()
        _init_session(_sessions[sid])
    return _sessions[sid]

def _init_session(s: SessionState):
    if CPP_BACKEND:
        s.avl   = _ce.AVLTree()
        s.rbt   = _ce.RBTree()
        s.heap  = _ce.BinaryHeap(True)
        s.treap = _ce.Treap()
        s.splay = _ce.SplayTree()
        s.btree = _ce.BTree(2)
        s.trie  = _ce.Trie()
        s.dsu   = _ce.DSU(20)
        s.huffman = _ce.HuffmanTree()
        s.persistent_seg = _ce.PersistentSegTree(100)
    else:
        s.avl  = PyAVLTree()
        s.heap = PyBinaryHeap(True)
    s.current_struct = "avl"

# ─────────────────────────────────────────────
#  Layout Engine — Reingold-Tilford Algorithm
# ─────────────────────────────────────────────

def layout_tree(nodes_data: list, root_id: int, nil_id: int = -1) -> dict:
    """
    Compute (x, y, depth) for every node using the Reingold-Tilford
    tidy-tree algorithm for a visually balanced layout.
    """
    by_id = {n["id"]: n for n in nodes_data if n["id"] != nil_id}
    if not by_id or root_id not in by_id or root_id == nil_id:
        return {}

    # Build child relationships
    children = {nid: [] for nid in by_id}
    for nid, nd in by_id.items():
        l = nd.get("left",  -1)
        r = nd.get("right", -1)
        if l != -1 and l != nil_id and l in by_id:
            children[nid].append(("left",  l))
        if r != -1 and r != nil_id and r in by_id:
            children[nid].append(("right", r))

    # Depth assignment
    depth: Dict[int,int] = {}
    stack = [(root_id, 0)]
    while stack:
        nid, d = stack.pop()
        depth[nid] = d
        for _, cid in children.get(nid, []):
            stack.append((cid, d+1))

    # x-coordinate via inorder rank
    inorder: List[int] = []
    def _inorder(nid):
        if nid not in by_id: return
        kids = {k: v for k,v in children.get(nid, [])}
        if "left" in kids: _inorder(kids["left"])
        inorder.append(nid)
        if "right" in kids: _inorder(kids["right"])
    _inorder(root_id)

    x_rank = {nid: i for i, nid in enumerate(inorder)}
    # Normalize
    total = max(x_rank.values()) + 1 if x_rank else 1
    layout = {}
    for nid in by_id:
        layout[nid] = {
            "x": (x_rank.get(nid, 0) / max(total - 1, 1)) * 2 - 1,  # [-1, 1]
            "y": -depth.get(nid, 0) * 0.18,                           # downward
            "depth": depth.get(nid, 0)
        }
    return layout

def layout_heap(nodes_data: list) -> dict:
    """Binary heap array-layout: compute tree (x,y) positions from index."""
    layout = {}
    for nd in nodes_data:
        i   = nd["id"]
        dep = int(math.log2(i + 1)) if i > 0 else 0
        pos_in_level = i - (2**dep - 1)
        count_in_level = 2**dep
        layout[i] = {
            "x": (pos_in_level / max(count_in_level - 1, 1)) * 2 - 1,
            "y": -dep * 0.18,
            "depth": dep
        }
    return layout

def layout_btree(nodes_data: list, root_id: int) -> dict:
    """Simple BFS horizontal layout for B-Tree."""
    by_id = {n["id"][0]: n for n in nodes_data}
    layout = {}
    if root_id not in by_id: return layout
    from collections import deque
    q = deque([(root_id, 0, 0.0)])
    level_counts: Dict[int, int] = {}
    level_offset: Dict[int, int] = {}
    # first pass: count nodes per level
    level_nodes: Dict[int, list] = {}
    bfs_q = deque([(root_id, 0)])
    visited = set()
    while bfs_q:
        nid, dep = bfs_q.popleft()
        if nid in visited or nid not in by_id: continue
        visited.add(nid)
        level_nodes.setdefault(dep, []).append(nid)
        for cid in by_id[nid]["children"]:
            if cid not in visited: bfs_q.append((cid, dep+1))
    for dep, nids in level_nodes.items():
        for i, nid in enumerate(nids):
            layout[nid] = {"x": (i/(max(len(nids)-1,1)))*2-1, "y": -dep*0.22, "depth": dep}
    return layout

# ─────────────────────────────────────────────
#  3D Coordinates  (spherical / cylindrical)
# ─────────────────────────────────────────────

def layout_3d(layout_2d: dict, nodes_data: list, mode: str="spiral") -> dict:
    """
    Generate 3D positions.
    mode='spiral'    → each depth level rotated by golden angle
    mode='radial'    → BFS radial sphere
    mode='layered'   → flat layers along Z axis
    """
    phi = (1 + math.sqrt(5)) / 2  # golden ratio
    layout_3d: Dict[int, dict] = {}
    max_depth = max((v["depth"] for v in layout_2d.values()), default=0)

    for nid, pos2d in layout_2d.items():
        d   = pos2d["depth"]
        x2d = pos2d["x"]
        y2d = pos2d["y"]
        if mode == "spiral":
            angle = d * 2 * math.pi / phi
            r     = d * 0.3 + 0.1
            layout_3d[nid] = {
                "x": x2d * 1.5,
                "y": y2d * 1.5,
                "z": d * 0.2 * math.cos(angle),
                "rx": 0, "ry": angle
            }
        elif mode == "radial":
            theta = math.pi * (d / max(max_depth,1))
            phi_a = 2 * math.pi * (nid % 12) / 12
            r     = d * 0.4 + 0.5
            layout_3d[nid] = {
                "x": r * math.sin(theta) * math.cos(phi_a),
                "y": r * math.cos(theta),
                "z": r * math.sin(theta) * math.sin(phi_a),
                "rx":theta, "ry": phi_a
            }
        else:  # layered
            layout_3d[nid] = {"x": x2d * 2, "y": y2d * 2, "z": d * 0.25, "rx":0,"ry":0}

    return layout_3d

# ─────────────────────────────────────────────
#  Serialization helpers
# ─────────────────────────────────────────────

def _serialize_avl(s: SessionState) -> dict:
    nodes = s.avl.serialize()
    root  = s.avl.root if CPP_BACKEND else _get_root_id(nodes)
    layout_2d = layout_tree(nodes, root)
    layout3d  = layout_3d(layout_2d, nodes)
    for nd in nodes:
        nid = nd["id"]
        nd["layout"] = layout_2d.get(nid, {"x":0,"y":0,"depth":0})
        nd["layout3d"] = layout3d.get(nid, {"x":0,"y":0,"z":0,"rx":0,"ry":0})
        nd["type"] = "avl"
    return {
        "type": "avl", "nodes": nodes, "root": root,
        "stats": {
            "comparisons": s.avl.total_comparisons,
            "rotations":   s.avl.total_rotations,
            "height":      _tree_height(nodes, root),
            "size":        len(nodes)
        }
    }

def _serialize_rbt(s: SessionState) -> dict:
    if not CPP_BACKEND: return {"type":"rbt","nodes":[],"root":-1,"stats":{}}
    nodes = s.rbt.serialize()
    root  = s.rbt.get_root()
    nil   = s.rbt.get_nil()
    layout_2d = layout_tree(nodes, root, nil)
    layout3d  = layout_3d(layout_2d, nodes)
    for nd in nodes:
        nid=nd["id"]
        nd["layout"]   = layout_2d.get(nid, {"x":0,"y":0,"depth":0})
        nd["layout3d"] = layout3d.get(nid, {"x":0,"y":0,"z":0,"rx":0,"ry":0})
        nd["type"] = "rbt"
        nd["color_name"] = "red" if nd["color"]==1 else "black"
    return {"type":"rbt","nodes":nodes,"root":root,"nil":nil,
            "stats":{"comparisons":s.rbt.total_comparisons,"rotations":s.rbt.total_rotations,"size":len(nodes)}}

def _serialize_heap(s: SessionState) -> dict:
    nodes = s.heap.serialize()
    layout_2d = layout_heap(nodes)
    layout3d  = layout_3d(layout_2d, nodes, "layered")
    for nd in nodes:
        i=nd["id"]
        nd["layout"]   = layout_2d.get(i, {"x":0,"y":0,"depth":0})
        nd["layout3d"] = layout3d.get(i, {"x":0,"y":0,"z":0,"rx":0,"ry":0})
        nd["type"] = "heap"
    top = s.heap.peek_top()
    return {"type":"heap","nodes":nodes,"top":top,
            "is_min": True,
            "stats":{"comparisons": s.heap.comparisons if CPP_BACKEND else s.heap.comparisons,"size":len(nodes)}}

def _serialize_btree(s: SessionState) -> dict:
    if not CPP_BACKEND: return {"type":"btree","nodes":[],"root":-1}
    nodes  = s.btree.serialize()
    root   = s.btree.root
    layout_2d = layout_btree(nodes, root)
    for nd in nodes:
        nid = nd["id"][0]
        nd["layout"] = layout_2d.get(nid, {"x":0,"y":0,"depth":0})
    return {"type":"btree","nodes":nodes,"root":root}

def _serialize_treap(s: SessionState) -> dict:
    if not CPP_BACKEND: return {"type":"treap","nodes":[],"root":-1}
    nodes  = s.treap.serialize()
    root   = s.treap.root
    layout_2d = layout_tree(nodes, root)
    layout3d  = layout_3d(layout_2d, nodes)
    for nd in nodes:
        nid=nd["id"]
        nd["layout"]   = layout_2d.get(nid, {"x":0,"y":0,"depth":0})
        nd["layout3d"] = layout3d.get(nid, {"x":0,"y":0,"z":0,"rx":0,"ry":0})
        nd["type"] = "treap"
    return {"type":"treap","nodes":nodes,"root":root}

def _serialize_splay(s: SessionState) -> dict:
    if not CPP_BACKEND: return {"type":"splay","nodes":[],"root":-1}
    nodes  = s.splay.serialize()
    root   = s.splay.root
    layout_2d = layout_tree(nodes, root)
    layout3d  = layout_3d(layout_2d, nodes)
    for nd in nodes:
        nid=nd["id"]
        nd["layout"]   = layout_2d.get(nid,{"x":0,"y":0,"depth":0})
        nd["layout3d"] = layout3d.get(nid,{"x":0,"y":0,"z":0,"rx":0,"ry":0})
        nd["type"] = "splay"
    return {"type":"splay","nodes":nodes,"root":root}

def _serialize_trie(s: SessionState) -> dict:
    if not CPP_BACKEND: return {"type":"trie","nodes":[],"root":0}
    raw = s.trie.serialize()  # list of (id, parent, char, is_end, count)
    nodes=[]
    for item in raw:
        nid,pid,ch,is_end,cnt = item
        if isinstance(ch, int):
            char = "ROOT" if ch == 32 else chr(ch)
        else:
            # C++ binding returned a str directly
            char = ch if ch not in ("", None) else "ROOT"
        nodes.append({"id":nid,"parent":pid,"char":char,
                       "is_end":bool(is_end),"count":cnt})
    # layout
    by_id={n["id"]:n for n in nodes}
    children_map: Dict[int,list]={n["id"]:[] for n in nodes}
    for n in nodes:
        pid=n["parent"]
        if pid!=-1 and pid in children_map: children_map[pid].append(n["id"])
    def _x_assign(nid, depth, x_start, x_end):
        by_id[nid]["layout"]={"x":(x_start+x_end)/2,"y":-depth*0.15,"depth":depth}
        kids=children_map.get(nid,[])
        if not kids: return
        slot=(x_end-x_start)/len(kids)
        for i,cid in enumerate(kids):
            _x_assign(cid,depth+1,x_start+i*slot,x_start+(i+1)*slot)
    if nodes: _x_assign(0,0,-1.0,1.0)
    return {"type":"trie","nodes":nodes,"root":0}

def _get_root_id(nodes):
    if not nodes: return -1
    ids={n["id"] for n in nodes}
    parents={n.get("parent",-1) for n in nodes}
    for nid in ids:
        if nid not in parents: return nid
    return nodes[0]["id"]

def _tree_height(nodes, root, nil=-1):
    by_id={n["id"]:n for n in nodes}
    def h(nid):
        if nid==-1 or nid==nil or nid not in by_id: return 0
        nd=by_id[nid]
        return 1+max(h(nd.get("left",-1)),h(nd.get("right",-1)))
    return h(root)

SERIALIZERS = {
    "avl":   _serialize_avl,
    "rbt":   _serialize_rbt,
    "heap":  _serialize_heap,
    "btree": _serialize_btree,
    "treap": _serialize_treap,
    "splay": _serialize_splay,
    "trie":  _serialize_trie,
}

def serialize_current(s: SessionState) -> dict:
    fn = SERIALIZERS.get(s.current_struct)
    if fn:
        try:
            return fn(s)
        except Exception as e:
            return {"type": s.current_struct, "nodes": [], "error": str(e)}
    return {"type": s.current_struct, "nodes": []}

# ─────────────────────────────────────────────
#  Flask App & SocketIO
# ─────────────────────────────────────────────

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
app = Flask(__name__, static_folder=BASE_DIR, static_url_path="")
CORS(app)
socketio = SocketIO(app, cors_allowed_origins="*", async_mode="threading")

def api_response(data=None, error=None, status=200):
    body = {"ok": error is None}
    if data:  body.update(data)
    if error: body["error"] = error
    return jsonify(body), status

def require_json(f):
    @wraps(f)
    def wrapper(*args, **kwargs):
        if not request.is_json:
            return api_response(error="Content-Type must be application/json", status=400)
        return f(*args, **kwargs)
    return wrapper

# ─── Structure management ───

@app.route("/api/set_structure", methods=["POST"])
@require_json
def set_structure():
    sid  = request.json.get("session_id", "default")
    name = request.json.get("structure", "avl")
    s    = get_session(sid)
    s.current_struct = name
    return api_response({"structure": name, "state": serialize_current(s)})

@app.route("/api/reset", methods=["POST"])
@require_json
def reset():
    sid  = request.json.get("session_id", "default")
    name = request.json.get("structure", "avl")
    min_heap = request.json.get("min_heap", True)
    s = _sessions.get(sid, SessionState())
    _init_session(s)
    s.current_struct = name
    _sessions[sid] = s
    return api_response({"structure": name, "state": serialize_current(s)})

# ─── AVL Operations ───

@app.route("/api/avl/insert", methods=["POST"])
@require_json
def avl_insert():
    sid = request.json.get("session_id","default")
    key = int(request.json["key"]); val = int(request.json.get("value", key))
    s = get_session(sid); s.current_struct="avl"
    s.avl.do_insert(key, val)
    return api_response({"state": _serialize_avl(s)})

@app.route("/api/avl/delete", methods=["POST"])
@require_json
def avl_delete():
    sid = request.json.get("session_id","default")
    key = int(request.json["key"])
    s = get_session(sid)
    s.avl.do_delete(key)
    return api_response({"state": _serialize_avl(s)})

@app.route("/api/avl/search", methods=["POST"])
@require_json
def avl_search():
    sid = request.json.get("session_id","default")
    key = int(request.json["key"])
    s = get_session(sid)
    path = s.avl.search(key)
    return api_response({"path": list(path), "found": bool(path and ((CPP_BACKEND and s.avl.nodes[path[-1]].key==key) or (not CPP_BACKEND and path[-1]==key))), "state": _serialize_avl(s)})

@app.route("/api/avl/traversal", methods=["POST"])
@require_json
def avl_traversal():
    sid   = request.json.get("session_id","default")
    order = request.json.get("order","inorder")
    s = get_session(sid)
    fn_map = {"inorder":s.avl.inorder,"preorder":s.avl.preorder,"postorder":s.avl.postorder,"level_order":s.avl.level_order}
    fn = fn_map.get(order, s.avl.inorder)
    result = fn()
    return api_response({"order":order,"result":result})

@app.route("/api/avl/kth", methods=["POST"])
@require_json
def avl_kth():
    sid = request.json.get("session_id","default")
    k   = int(request.json["k"])
    s = get_session(sid)
    return api_response({"k":k,"key":s.avl.kth_smallest(k)})

@app.route("/api/avl/lca", methods=["POST"])
@require_json
def avl_lca():
    sid = request.json.get("session_id","default")
    a = int(request.json["a"]); b = int(request.json["b"])
    s = get_session(sid)
    return api_response({"a":a,"b":b,"lca":s.avl.lca(a,b)})

@app.route("/api/avl/floor_ceil", methods=["POST"])
@require_json
def avl_floor_ceil():
    sid = request.json.get("session_id","default")
    key = int(request.json["key"])
    s   = get_session(sid)
    return api_response({"key":key,"floor":s.avl.floor_key(key),"ceil":s.avl.ceil_key(key)})

@app.route("/api/avl/bulk_insert", methods=["POST"])
@require_json
def avl_bulk_insert():
    sid  = request.json.get("session_id","default")
    keys = request.json["keys"]
    s = get_session(sid)
    for k in keys: s.avl.do_insert(int(k), int(k))
    return api_response({"count":len(keys),"state":_serialize_avl(s)})

# ─── RBT Operations ───

@app.route("/api/rbt/insert", methods=["POST"])
@require_json
def rbt_insert():
    sid = request.json.get("session_id","default")
    key = int(request.json["key"]); val=int(request.json.get("value",key))
    s = get_session(sid); s.current_struct="rbt"
    if CPP_BACKEND: s.rbt.do_insert(key,val)
    return api_response({"state":_serialize_rbt(s)})

@app.route("/api/rbt/delete", methods=["POST"])
@require_json
def rbt_delete():
    sid=request.json.get("session_id","default"); key=int(request.json["key"])
    s=get_session(sid)
    if CPP_BACKEND: s.rbt.do_delete(key)
    return api_response({"state":_serialize_rbt(s)})

# ─── Heap Operations ───

@app.route("/api/heap/insert", methods=["POST"])
@require_json
def heap_insert():
    sid=request.json.get("session_id","default"); key=int(request.json["key"])
    s=get_session(sid); s.current_struct="heap"
    s.heap.insert(key)
    return api_response({"state":_serialize_heap(s)})

@app.route("/api/heap/extract", methods=["POST"])
@require_json
def heap_extract():
    sid=request.json.get("session_id","default")
    s=get_session(sid)
    try:
        top = s.heap.extract_top()
        return api_response({"extracted": top if isinstance(top,dict) else {"key":top.key,"value":top.value},"state":_serialize_heap(s)})
    except Exception as e:
        return api_response(error=str(e),status=400)

@app.route("/api/heap/build", methods=["POST"])
@require_json
def heap_build():
    sid=request.json.get("session_id","default"); keys=request.json["keys"]
    min_heap=request.json.get("min_heap",True)
    s=get_session(sid)
    if CPP_BACKEND: s.heap=_ce.BinaryHeap(bool(min_heap))
    else:           s.heap=PyBinaryHeap(bool(min_heap))
    s.heap.build_heap([int(k) for k in keys])
    return api_response({"state":_serialize_heap(s)})

@app.route("/api/heap/sort", methods=["POST"])
@require_json
def heap_sort():
    sid=request.json.get("session_id","default")
    s=get_session(sid)
    sorted_keys = s.heap.heap_sort()
    return api_response({"sorted": list(sorted_keys)})

# ─── B-Tree ───

@app.route("/api/btree/insert", methods=["POST"])
@require_json
def btree_insert():
    sid=request.json.get("session_id","default"); key=int(request.json["key"])
    s=get_session(sid); s.current_struct="btree"
    if CPP_BACKEND: s.btree.insert(key)
    return api_response({"state":_serialize_btree(s)})

# ─── Treap ───

@app.route("/api/treap/insert", methods=["POST"])
@require_json
def treap_insert():
    sid=request.json.get("session_id","default"); key=int(request.json["key"])
    s=get_session(sid); s.current_struct="treap"
    if CPP_BACKEND: s.treap.insert(key)
    return api_response({"state":_serialize_treap(s)})

@app.route("/api/treap/delete", methods=["POST"])
@require_json
def treap_delete():
    sid=request.json.get("session_id","default"); key=int(request.json["key"])
    s=get_session(sid)
    if CPP_BACKEND: s.treap.erase(key)
    return api_response({"state":_serialize_treap(s)})

# ─── Splay Tree ───

@app.route("/api/splay/insert", methods=["POST"])
@require_json
def splay_insert():
    sid=request.json.get("session_id","default"); key=int(request.json["key"])
    s=get_session(sid); s.current_struct="splay"
    if CPP_BACKEND: s.splay.insert(key)
    return api_response({"state":_serialize_splay(s)})

@app.route("/api/splay/search", methods=["POST"])
@require_json
def splay_search():
    sid=request.json.get("session_id","default"); key=int(request.json["key"])
    s=get_session(sid)
    found = s.splay.search(key) if CPP_BACKEND else False
    return api_response({"found":bool(found),"state":_serialize_splay(s)})

# ─── Trie ───

@app.route("/api/trie/insert", methods=["POST"])
@require_json
def trie_insert():
    sid=request.json.get("session_id","default"); word=request.json["word"]
    s=get_session(sid); s.current_struct="trie"
    if CPP_BACKEND: s.trie.insert(str(word))
    return api_response({"state":_serialize_trie(s)})

@app.route("/api/trie/search", methods=["POST"])
@require_json
def trie_search():
    sid=request.json.get("session_id","default"); word=request.json["word"]
    s=get_session(sid)
    found = s.trie.search(str(word)) if CPP_BACKEND else False
    return api_response({"found":bool(found)})

@app.route("/api/trie/autocomplete", methods=["POST"])
@require_json
def trie_autocomplete():
    sid=request.json.get("session_id","default"); prefix=request.json["prefix"]
    s=get_session(sid)
    results = list(s.trie.autocomplete(str(prefix))) if CPP_BACKEND else []
    return api_response({"prefix":prefix,"results":results})

# ─── DSU ───

@app.route("/api/dsu/unite", methods=["POST"])
@require_json
def dsu_unite():
    sid=request.json.get("session_id","default")
    x=int(request.json["x"]); y=int(request.json["y"])
    s=get_session(sid)
    ok = s.dsu.unite(x,y) if CPP_BACKEND else False
    parent = list(s.dsu.get_parent()) if CPP_BACKEND else []
    return api_response({"merged":bool(ok),"parent":parent,"components": s.dsu.components if CPP_BACKEND else 0})

@app.route("/api/dsu/connected", methods=["POST"])
@require_json
def dsu_connected():
    sid=request.json.get("session_id","default")
    x=int(request.json["x"]); y=int(request.json["y"])
    s=get_session(sid)
    conn = s.dsu.connected(x,y) if CPP_BACKEND else False
    return api_response({"x":x,"y":y,"connected":bool(conn)})

# ─── Segment Tree ───

@app.route("/api/segtree/build", methods=["POST"])
@require_json
def segtree_build():
    sid=request.json.get("session_id","default"); arr=request.json["array"]
    s=get_session(sid)
    ia=[int(x) for x in arr]
    if CPP_BACKEND: s.segtree=_ce.SegmentTree(ia)
    return api_response({"internal": list(s.segtree.get_internal()) if CPP_BACKEND else [], "array":ia})

@app.route("/api/segtree/query", methods=["POST"])
@require_json
def segtree_query():
    sid=request.json.get("session_id","default"); l=int(request.json["l"]); r=int(request.json["r"])
    s=get_session(sid)
    result = s.segtree.query(l,r) if (CPP_BACKEND and s.segtree) else 0
    return api_response({"l":l,"r":r,"sum":result})

@app.route("/api/segtree/update", methods=["POST"])
@require_json
def segtree_update():
    sid=request.json.get("session_id","default")
    l=int(request.json["l"]); r=int(request.json["r"]); val=int(request.json["val"])
    s=get_session(sid)
    if CPP_BACKEND and s.segtree: s.segtree.update(l,r,val)
    return api_response({"updated":True})

# ─── Huffman ───

@app.route("/api/huffman/build", methods=["POST"])
@require_json
def huffman_build():
    sid=request.json.get("session_id","default"); text=request.json["text"]
    s=get_session(sid)
    from collections import Counter
    freq = dict(Counter(text))
    if CPP_BACKEND:
        s.huffman.build(freq)
        nodes = s.huffman.serialize()
        codes = {str(k):v for k,v in s.huffman.get_codes().items()}
        layout_2d = layout_tree(nodes, s.huffman.root)
        for nd in nodes: nd["layout"]=layout_2d.get(nd["id"],{"x":0,"y":0,"depth":0})
        return api_response({"nodes":nodes,"codes":codes,"root":s.huffman.root})
    return api_response({"nodes":[],"codes":{},"root":-1})

# ─── Graph Algorithms ───

@app.route("/api/algo/dijkstra", methods=["POST"])
@require_json
def algo_dijkstra():
    sid=request.json.get("session_id","default")
    n   = int(request.json["n"])
    src = int(request.json["src"])
    edges = request.json["edges"]  # [[u,v,w],...]
    if not CPP_BACKEND:
        return api_response(error="C++ backend required", status=503)
    adj: list = [[] for _ in range(n)]
    for e in edges: adj[e[0]].append((e[1],e[2])); adj[e[1]].append((e[0],e[2]))
    dist = list(_ce.GraphAlgo.dijkstra(n, src, adj))
    return api_response({"distances":dist})

@app.route("/api/algo/prim", methods=["POST"])
@require_json
def algo_prim():
    sid=request.json.get("session_id","default")
    n=int(request.json["n"]); edges=request.json["edges"]
    if not CPP_BACKEND: return api_response(error="C++ backend required",status=503)
    adj: list=[[] for _ in range(n)]
    for e in edges: adj[e[0]].append((e[1],e[2])); adj[e[1]].append((e[0],e[2]))
    mst=_ce.GraphAlgo.prim_mst(n,adj)
    return api_response({"mst":[(u,v) for u,v in mst]})

@app.route("/api/algo/topo", methods=["POST"])
@require_json
def algo_topo():
    sid=request.json.get("session_id","default")
    n=int(request.json["n"]); edges=request.json["edges"]
    if not CPP_BACKEND: return api_response(error="C++ backend required",status=503)
    adj: list=[[] for _ in range(n)]
    for u,v in edges: adj[u].append(v)
    order=list(_ce.GraphAlgo.topo_sort(n,adj))
    return api_response({"order":order})

# ─── Persistent Segment Tree ───

@app.route("/api/persistent/add", methods=["POST"])
@require_json
def persistent_add():
    sid=request.json.get("session_id","default")
    pos=int(request.json["pos"]); val=int(request.json["val"])
    s=get_session(sid)
    if CPP_BACKEND: s.persistent_seg.add_version(pos,val)
    return api_response({"versions": s.persistent_seg.version_count() if CPP_BACKEND else 0})

@app.route("/api/persistent/query", methods=["POST"])
@require_json
def persistent_query():
    sid=request.json.get("session_id","default")
    version=int(request.json["version"]); l=int(request.json["l"]); r=int(request.json["r"])
    s=get_session(sid)
    result = s.persistent_seg.query_version(version,l,r) if CPP_BACKEND else 0
    return api_response({"version":version,"l":l,"r":r,"sum":result})

# ─── Benchmarking ───

@app.route("/api/benchmark", methods=["POST"])
@require_json
def benchmark():
    sid=request.json.get("session_id","default")
    struct=request.json.get("structure","avl"); n=int(request.json.get("n",1000))
    keys=[random.randint(1,10000) for _ in range(n)]
    results={}
    for name in ["avl","rbt","heap"]:
        s2=SessionState(); _init_session(s2); s2.current_struct=name
        t0=time.perf_counter()
        try:
            if name=="avl":
                for k in keys: s2.avl.do_insert(k,k)
                elapsed=time.perf_counter()-t0
                results[name]={"insert_ms":elapsed*1000,"comparisons":s2.avl.total_comparisons,"rotations":s2.avl.total_rotations}
            elif name=="rbt" and CPP_BACKEND:
                for k in keys: s2.rbt.do_insert(k,k)
                elapsed=time.perf_counter()-t0
                results[name]={"insert_ms":elapsed*1000,"comparisons":s2.rbt.total_comparisons,"rotations":s2.rbt.total_rotations}
            elif name=="heap":
                s2.heap.build_heap(keys)
                elapsed=time.perf_counter()-t0
                results[name]={"build_ms":elapsed*1000,"comparisons":s2.heap.comparisons}
        except Exception as e:
            results[name]={"error":str(e)}
    return api_response({"n":n,"results":results})

# ─── Analytics ───

@app.route("/api/analytics/avl", methods=["POST"])
@require_json
def analytics_avl():
    sid=request.json.get("session_id","default")
    s=get_session(sid)
    if not CPP_BACKEND:
        nodes=s.avl.serialize()
        root_id=_get_root_id(nodes)
        h=_tree_height(nodes,root_id)
        return api_response({"height":h,"size":len(nodes),"comparisons":s.avl.total_comparisons,"rotations":s.avl.total_rotations})
    stats=_ce.compute_avl_stats(s.avl)
    return api_response({"height":stats.height,"diameter":stats.diameter,"avg_depth":stats.avg_depth,
                          "leaf_count":stats.leaf_count,"is_balanced":stats.is_balanced,
                          "comparisons":stats.total_comparisons,"rotations":stats.total_rotations,"size":stats.node_count})

# ─── Random Tree Generation ───

@app.route("/api/generate", methods=["POST"])
@require_json
def generate():
    sid=request.json.get("session_id","default")
    struct=request.json.get("structure","avl"); n=int(request.json.get("n",12))
    keys=random.sample(range(1,200),min(n,50))
    s=get_session(sid); s.current_struct=struct
    if struct=="avl":
        if CPP_BACKEND: s.avl=_ce.AVLTree()
        else: s.avl=PyAVLTree()
        for k in keys: s.avl.do_insert(k,k)
        return api_response({"state":_serialize_avl(s),"keys":keys})
    elif struct=="rbt" and CPP_BACKEND:
        s.rbt=_ce.RBTree()
        for k in keys: s.rbt.do_insert(k,k)
        return api_response({"state":_serialize_rbt(s),"keys":keys})
    elif struct=="heap":
        if CPP_BACKEND: s.heap=_ce.BinaryHeap(True)
        else: s.heap=PyBinaryHeap(True)
        s.heap.build_heap(keys)
        return api_response({"state":_serialize_heap(s),"keys":keys})
    elif struct=="btree" and CPP_BACKEND:
        s.btree=_ce.BTree(2)
        for k in keys: s.btree.insert(k)
        return api_response({"state":_serialize_btree(s),"keys":keys})
    elif struct=="treap" and CPP_BACKEND:
        s.treap=_ce.Treap()
        for k in keys: s.treap.insert(k)
        return api_response({"state":_serialize_treap(s),"keys":keys})
    elif struct=="splay" and CPP_BACKEND:
        s.splay=_ce.SplayTree()
        for k in keys: s.splay.insert(k)
        return api_response({"state":_serialize_splay(s),"keys":keys})
    return api_response({"keys":keys,"state":serialize_current(s)})

# ─── Status / Health ───

@app.route("/api/status")
def status():
    return api_response({"cpp_backend":CPP_BACKEND,"version":"2.0.0",
                          "structures":["avl","rbt","heap","btree","treap","splay","trie","dsu","segtree","fenwick","huffman","persistent_seg"],
                          "algorithms":["inorder","preorder","postorder","level_order","kth_smallest","lca","floor_ceil","dijkstra","prim","topo_sort","hld"],
                          "features":["2d_viz","3d_viz","realtime","animation","benchmark","analytics","persistent"]})

# ─── Serve frontend ───

@app.route("/")
def index():
    return send_from_directory(BASE_DIR, "index.html")

@app.route("/<path:filename>")
def static_files(filename):
    return send_from_directory(BASE_DIR, filename)

# ─────────────────────────────────────────────
#  WebSocket Events (real-time)
# ─────────────────────────────────────────────

@socketio.on("connect")
def on_connect():
    sid = request.sid
    get_session(sid)
    emit("connected", {"session_id": sid, "cpp": CPP_BACKEND})

@socketio.on("disconnect")
def on_disconnect():
    sid = request.sid
    _sessions.pop(sid, None)

@socketio.on("op")
def on_op(data):
    sid = request.sid
    s   = get_session(sid)
    struct = data.get("structure", s.current_struct)
    s.current_struct = struct
    op  = data.get("op")
    key = data.get("key")
    val = data.get("value", key)

    try:
        if struct == "avl":
            if op == "insert": s.avl.do_insert(int(key), int(val))
            elif op == "delete": s.avl.do_delete(int(key))
            state = _serialize_avl(s)
        elif struct == "rbt" and CPP_BACKEND:
            if op == "insert": s.rbt.do_insert(int(key), int(val))
            elif op == "delete": s.rbt.do_delete(int(key))
            state = _serialize_rbt(s)
        elif struct == "heap":
            if op == "insert": s.heap.insert(int(key))
            elif op == "extract": s.heap.extract_top()
            state = _serialize_heap(s)
        elif struct == "treap" and CPP_BACKEND:
            if op == "insert": s.treap.insert(int(key))
            elif op == "delete": s.treap.erase(int(key))
            state = _serialize_treap(s)
        elif struct == "splay" and CPP_BACKEND:
            if op == "insert": s.splay.insert(int(key))
            state = _serialize_splay(s)
        else:
            state = serialize_current(s)
        emit("state_update", {"state": state, "op": op, "key": key})
    except Exception as e:
        emit("error", {"message": str(e), "trace": traceback.format_exc()})

@socketio.on("animate_batch")
def on_animate_batch(data):
    """Send a sequence of states with delay for animation."""
    sid    = request.sid
    s      = get_session(sid)
    ops    = data.get("ops", [])
    speed  = data.get("speed_ms", 500)
    struct = data.get("structure", s.current_struct)
    s.current_struct = struct

    for op_data in ops:
        try:
            op  = op_data.get("op")
            key = op_data.get("key")
            val = op_data.get("value", key)
            if struct == "avl":
                if op == "insert": s.avl.do_insert(int(key), int(val))
                elif op == "delete": s.avl.do_delete(int(key))
                state = _serialize_avl(s)
            elif struct == "heap":
                if op == "insert": s.heap.insert(int(key))
                elif op == "extract": s.heap.extract_top()
                state = _serialize_heap(s)
            else:
                state = serialize_current(s)
            socketio.emit("animation_step", {"state": state, "op": op, "key": key}, to=sid)
            time.sleep(speed / 1000.0)
        except Exception as e:
            socketio.emit("error", {"message": str(e)}, to=sid)
            break

    socketio.emit("animation_done", {}, to=sid)

# ─────────────────────────────────────────────
#  Entry Point
# ─────────────────────────────────────────────

if __name__ == "__main__":
    port = int(os.environ.get("PORT", 5000))
    print(f"[INFO] Tree & Heap Visualizer backend starting on port {port}")
    print(f"[INFO] C++ backend: {CPP_BACKEND}")
    socketio.run(app, host="0.0.0.0", port=port, debug=False)
