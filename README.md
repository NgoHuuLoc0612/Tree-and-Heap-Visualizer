# 🌳 Tree & Heap Visualizer

A web-based tool for visualizing and interacting with classic data structures in real time. Built with a C++ core (via pybind11), a Python/Flask backend, and a vanilla JS frontend with 2D/3D rendering.

---

## Features

- **Interactive operations** — insert, delete, search, bulk insert on any structure
- **Step-by-step animation** with adjustable speed
- **2D canvas view** and **3D view** (Three.js)
- **Live stats** — node count, height, comparisons, rotations, last op time
- **Algorithm visualizations** — traversals, LCA, Kth smallest, floor/ceil, Huffman encoding, Dijkstra
- **Benchmark mode** — compare AVL vs Red-Black vs Heap insert performance
- **WebSocket support** for real-time updates
- **Graceful fallback** — runs in pure Python if the C++ module isn't compiled

---

## Supported Structures

| Category | Structures |
|---|---|
| Binary Trees | AVL Tree, Red-Black Tree, Treap, Splay Tree |
| Heaps | Binary Heap (min/max) |
| Advanced | B-Tree, Trie |
| Others | DSU, Segment Tree, Fenwick Tree, Persistent Segment Tree |

---

## Requirements

- Python 3.9+
- A C++ compiler with C++17 support (GCC, Clang, or MSVC)
- Node / npm not required — frontend is plain HTML/CSS/JS

```
pip install pybind11 setuptools flask flask-cors flask-socketio
```

---

## Setup

### 1. Build the C++ extension

```bash
python setup.py build_ext --inplace
```

This compiles `core_engine.cpp` into a `.so` (Linux/macOS) or `.pyd` (Windows) file next to `server.py`.

If the build fails or you want to skip it, the server will automatically fall back to pure-Python implementations for AVL Tree and Binary Heap. Other structures (RBT, Treap, Splay, BTree, Trie, etc.) require the C++ module.

### 2. Run the server

```bash
python server.py
```

The server starts on `http://localhost:5000` by default. Open it in your browser.

To use a different port:

```bash
PORT=8080 python server.py
```

---

## Project Structure

```
.
├── core_engine.cpp   # C++ implementations (AVL, RBT, Heap, Trie, …)
├── setup.py          # pybind11 build config
├── server.py         # Flask API + WebSocket server
├── index.html        # Main UI
├── app.js            # Frontend logic, canvas rendering, 3D view
└── style.css         # Dark theme UI styles
```

---

## API Overview

All endpoints accept and return JSON. A `session_id` field can be included to maintain separate state per client (defaults to `"default"`).

| Method | Endpoint | Description |
|---|---|---|
| GET | `/api/status` | Backend info and feature list |
| POST | `/api/avl/insert` | Insert key into AVL tree |
| POST | `/api/avl/delete` | Delete key from AVL tree |
| POST | `/api/avl/search` | Search for a key |
| POST | `/api/heap/insert` | Push a value onto the heap |
| POST | `/api/heap/extract` | Pop the top element |
| POST | `/api/trie/insert` | Insert a word |
| POST | `/api/trie/search` | Search for a word |
| POST | `/api/trie/autocomplete` | Get completions for a prefix |
| POST | `/api/algo/traversal` | Run in/pre/post/level-order traversal |
| POST | `/api/algo/lca` | Find lowest common ancestor |
| POST | `/api/benchmark` | Run insert benchmark across structures |
| POST | `/api/generate` | Generate a random tree |
| POST | `/api/reset` | Clear the current structure |

WebSocket events (`op`, `animate_batch`, `state_update`) are available via Socket.IO on the same port.

---

## Algorithms

- **Traversals** — inorder, preorder, postorder, level-order
- **LCA** — lowest common ancestor
- **Kth Smallest** — using subtree sizes
- **Floor & Ceil** — nearest keys in the BST
- **Huffman Encoding** — build and display the Huffman tree
- **Dijkstra / Prim / Topo Sort** — graph algorithms (C++ backend required)
- **HLD** — heavy-light decomposition (C++ backend required)

---

## Known Issues / Bugs Fixed

- `POST /api/trie/insert` — fixed a `TypeError: 'str' object cannot be interpreted as an integer` in `_serialize_trie` when the C++ pybind11 binding returns the character as a `str` instead of an `int`. The serializer now handles both types.

---

## License

MIT
