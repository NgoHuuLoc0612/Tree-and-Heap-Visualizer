#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/functional.h>
#include <vector>
#include <queue>
#include <stack>
#include <unordered_map>
#include <unordered_set>
#include <map>
#include <set>
#include <algorithm>
#include <optional>
#include <variant>
#include <functional>
#include <cmath>
#include <string>
#include <sstream>
#include <memory>
#include <random>
#include <limits>
#include <numeric>
#include <tuple>
#include <chrono>

namespace py = pybind11;
using namespace std;

// ─────────────────────────────────────────────
//  Node Structures
// ─────────────────────────────────────────────

struct TreeNode {
    int key;
    int value;
    int height;       // AVL
    int bf;           // balance factor
    int color;        // 0=black,1=red  (RBT)
    int size;         // subtree size (Order-stat)
    int lazy;         // lazy propagation tag
    int seg_val;      // segment tree value
    bool is_null;     // sentinel null node
    int parent;
    int left;
    int right;
    // Treap
    int priority;
    // B-Tree  / B+ Tree
    vector<int> keys;
    vector<int> children;
    bool is_leaf;
    // Splay
    int splay_size;
    // Thread Binary Tree
    bool left_thread;
    bool right_thread;

    TreeNode() : key(0), value(0), height(1), bf(0), color(1), size(1),
        lazy(0), seg_val(0), is_null(false), parent(-1), left(-1), right(-1),
        priority(0), is_leaf(true), splay_size(1), left_thread(false), right_thread(false) {}
};

struct HeapNode {
    int key;
    int value;
    int degree;       // binomial / fibonacci
    int mark;         // fibonacci mark
    int parent;
    int child;
    int sibling;
    double priority;  // for priority queue variant
    HeapNode() : key(0), value(0), degree(0), mark(0), parent(-1), child(-1), sibling(-1), priority(0.0) {}
};

// ─────────────────────────────────────────────
//  Step / Animation Trace
// ─────────────────────────────────────────────

struct StepRecord {
    string op;             // "insert","delete","search","rotate","merge","split"
    string detail;
    vector<int> highlight; // node ids highlighted
    vector<int> path;      // traversal path
    vector<pair<int,int>> edges; // edges to highlight
    string tree_state_json;// full serialized tree at this step
    double timestamp_ms;
    int comparisons;
    int rotations;
    string auxiliary;      // extra data
};

// ─────────────────────────────────────────────
//  Utility
// ─────────────────────────────────────────────

static mt19937 rng(42);

static int rand_priority() {
    return (int)(rng() % 1000000);
}

// ─────────────────────────────────────────────
//  AVL Tree
// ─────────────────────────────────────────────

class AVLTree {
public:
    vector<TreeNode> nodes;
    int root = -1;
    int node_count = 0;
    vector<StepRecord> trace;
    int total_comparisons = 0;
    int total_rotations = 0;

    int new_node(int key, int val) {
        nodes.emplace_back();
        int id = nodes.size() - 1;
        nodes[id].key = key;
        nodes[id].value = val;
        nodes[id].height = 1;
        nodes[id].size = 1;
        node_count++;
        return id;
    }

    int height(int n) { return n == -1 ? 0 : nodes[n].height; }
    int size(int n)   { return n == -1 ? 0 : nodes[n].size; }

    void update(int n) {
        if (n == -1) return;
        nodes[n].height = 1 + max(height(nodes[n].left), height(nodes[n].right));
        nodes[n].bf     = height(nodes[n].left) - height(nodes[n].right);
        nodes[n].size   = 1 + size(nodes[n].left) + size(nodes[n].right);
    }

    int rotate_right(int y) {
        int x = nodes[y].left;
        int T2 = nodes[x].right;
        nodes[x].right = y;
        nodes[y].left  = T2;
        if (T2 != -1) nodes[T2].parent = y;
        nodes[x].parent = nodes[y].parent;
        nodes[y].parent = x;
        update(y); update(x);
        total_rotations++;
        return x;
    }

    int rotate_left(int x) {
        int y = nodes[x].right;
        int T2 = nodes[y].left;
        nodes[y].left  = x;
        nodes[x].right = T2;
        if (T2 != -1) nodes[T2].parent = x;
        nodes[y].parent = nodes[x].parent;
        nodes[x].parent = y;
        update(x); update(y);
        total_rotations++;
        return y;
    }

    int insert(int n, int key, int val, vector<int>& path) {
        if (n == -1) {
            int id = new_node(key, val);
            path.push_back(id);
            return id;
        }
        path.push_back(n);
        total_comparisons++;
        if (key < nodes[n].key)
            nodes[n].left  = insert(nodes[n].left,  key, val, path);
        else if (key > nodes[n].key)
            nodes[n].right = insert(nodes[n].right, key, val, path);
        else {
            nodes[n].value = val;
            return n;
        }
        update(n);
        int bf = nodes[n].bf;
        // LL
        if (bf > 1 && key < nodes[nodes[n].left].key)  return rotate_right(n);
        // RR
        if (bf < -1 && key > nodes[nodes[n].right].key) return rotate_left(n);
        // LR
        if (bf > 1 && key > nodes[nodes[n].left].key) {
            nodes[n].left = rotate_left(nodes[n].left);
            return rotate_right(n);
        }
        // RL
        if (bf < -1 && key < nodes[nodes[n].right].key) {
            nodes[n].right = rotate_right(nodes[n].right);
            return rotate_left(n);
        }
        return n;
    }

    int min_node(int n) {
        while (nodes[n].left != -1) n = nodes[n].left;
        return n;
    }

    int delete_node(int n, int key, vector<int>& path) {
        if (n == -1) return n;
        path.push_back(n);
        total_comparisons++;
        if (key < nodes[n].key)
            nodes[n].left  = delete_node(nodes[n].left,  key, path);
        else if (key > nodes[n].key)
            nodes[n].right = delete_node(nodes[n].right, key, path);
        else {
            if (nodes[n].left == -1 || nodes[n].right == -1) {
                int tmp = (nodes[n].left != -1) ? nodes[n].left : nodes[n].right;
                n = tmp;
            } else {
                int s = min_node(nodes[n].right);
                nodes[n].key   = nodes[s].key;
                nodes[n].value = nodes[s].value;
                nodes[n].right = delete_node(nodes[n].right, nodes[s].key, path);
            }
        }
        if (n == -1) return n;
        update(n);
        int bf = nodes[n].bf;
        if (bf > 1 && nodes[nodes[n].left].bf >= 0)  return rotate_right(n);
        if (bf > 1 && nodes[nodes[n].left].bf < 0) {
            nodes[n].left = rotate_left(nodes[n].left);
            return rotate_right(n);
        }
        if (bf < -1 && nodes[nodes[n].right].bf <= 0) return rotate_left(n);
        if (bf < -1 && nodes[nodes[n].right].bf > 0) {
            nodes[n].right = rotate_right(nodes[n].right);
            return rotate_left(n);
        }
        return n;
    }

    vector<int> search(int key) {
        vector<int> path;
        int cur = root;
        while (cur != -1) {
            path.push_back(cur);
            total_comparisons++;
            if      (key == nodes[cur].key) break;
            else if (key <  nodes[cur].key) cur = nodes[cur].left;
            else                            cur = nodes[cur].right;
        }
        return path;
    }

    // Serialize to dict list for Python
    vector<map<string,int>> serialize() {
        vector<map<string,int>> result;
        for (int i = 0; i < (int)nodes.size(); i++) {
            auto& nd = nodes[i];
            map<string,int> m;
            m["id"]     = i;
            m["key"]    = nd.key;
            m["value"]  = nd.value;
            m["height"] = nd.height;
            m["bf"]     = nd.bf;
            m["size"]   = nd.size;
            m["left"]   = nd.left;
            m["right"]  = nd.right;
            m["parent"] = nd.parent;
            result.push_back(m);
        }
        return result;
    }

    void do_insert(int key, int val) {
        vector<int> path;
        root = insert(root, key, val, path);
        StepRecord sr;
        sr.op = "insert";
        sr.detail = "Insert key=" + to_string(key);
        sr.path = path;
        sr.highlight = {(int)nodes.size()-1};
        sr.comparisons = total_comparisons;
        sr.rotations = total_rotations;
        trace.push_back(sr);
    }

    void do_delete(int key) {
        vector<int> path;
        root = delete_node(root, key, path);
        StepRecord sr;
        sr.op = "delete";
        sr.detail = "Delete key=" + to_string(key);
        sr.path = path;
        sr.comparisons = total_comparisons;
        sr.rotations = total_rotations;
        trace.push_back(sr);
    }

    vector<int> inorder_traversal() {
        vector<int> result;
        function<void(int)> dfs = [&](int n) {
            if (n == -1) return;
            dfs(nodes[n].left);
            result.push_back(nodes[n].key);
            dfs(nodes[n].right);
        };
        dfs(root);
        return result;
    }

    vector<int> preorder_traversal() {
        vector<int> result;
        function<void(int)> dfs = [&](int n) {
            if (n == -1) return;
            result.push_back(nodes[n].key);
            dfs(nodes[n].left);
            dfs(nodes[n].right);
        };
        dfs(root);
        return result;
    }

    vector<int> postorder_traversal() {
        vector<int> result;
        function<void(int)> dfs = [&](int n) {
            if (n == -1) return;
            dfs(nodes[n].left);
            dfs(nodes[n].right);
            result.push_back(nodes[n].key);
        };
        dfs(root);
        return result;
    }

    vector<vector<int>> level_order_traversal() {
        vector<vector<int>> result;
        if (root == -1) return result;
        queue<int> q;
        q.push(root);
        while (!q.empty()) {
            int sz = q.size();
            vector<int> level;
            while (sz--) {
                int cur = q.front(); q.pop();
                level.push_back(nodes[cur].key);
                if (nodes[cur].left  != -1) q.push(nodes[cur].left);
                if (nodes[cur].right != -1) q.push(nodes[cur].right);
            }
            result.push_back(level);
        }
        return result;
    }

    // kth smallest (order-statistic)
    int kth_smallest(int k) {
        function<int(int, int)> find = [&](int n, int k) -> int {
            if (n == -1) return -1;
            int ls = size(nodes[n].left);
            if (k == ls + 1) return nodes[n].key;
            if (k <= ls)     return find(nodes[n].left,  k);
            return              find(nodes[n].right, k - ls - 1);
        };
        return find(root, k);
    }

    // LCA
    int lca(int u_key, int v_key) {
        int cur = root;
        while (cur != -1) {
            if (u_key < nodes[cur].key && v_key < nodes[cur].key)
                cur = nodes[cur].left;
            else if (u_key > nodes[cur].key && v_key > nodes[cur].key)
                cur = nodes[cur].right;
            else
                return nodes[cur].key;
        }
        return -1;
    }

    // Tree diameter
    pair<int,int> diameter_helper(int n) { // {height, diameter}
        if (n == -1) return {0, 0};
        auto [lh, ld] = diameter_helper(nodes[n].left);
        auto [rh, rd] = diameter_helper(nodes[n].right);
        int d = max({ld, rd, lh + rh});
        return {1 + max(lh, rh), d};
    }

    int diameter() {
        return diameter_helper(root).second;
    }

    // Floor and Ceil
    int floor_key(int key) {
        int res = INT_MIN, cur = root;
        while (cur != -1) {
            if      (nodes[cur].key == key) return key;
            else if (nodes[cur].key < key)  { res = nodes[cur].key; cur = nodes[cur].right; }
            else                            cur = nodes[cur].left;
        }
        return res;
    }

    int ceil_key(int key) {
        int res = INT_MAX, cur = root;
        while (cur != -1) {
            if      (nodes[cur].key == key) return key;
            else if (nodes[cur].key > key)  { res = nodes[cur].key; cur = nodes[cur].left; }
            else                            cur = nodes[cur].right;
        }
        return res;
    }

    void clear_trace() { trace.clear(); total_comparisons = 0; total_rotations = 0; }
};

// ─────────────────────────────────────────────
//  Red-Black Tree
// ─────────────────────────────────────────────

class RBTree {
public:
    static const int BLACK = 0, RED = 1;
    vector<TreeNode> nodes;
    int root = -1;
    int nil_id = -1; // sentinel
    vector<StepRecord> trace;
    int total_comparisons = 0, total_rotations = 0;

    RBTree() {
        nodes.emplace_back();
        nil_id = 0;
        nodes[nil_id].is_null = true;
        nodes[nil_id].color = BLACK;
        nodes[nil_id].left = nodes[nil_id].right = nodes[nil_id].parent = 0;
        root = nil_id;
    }

    int new_node(int key, int val) {
        nodes.emplace_back();
        int id = nodes.size() - 1;
        nodes[id].key = key; nodes[id].value = val;
        nodes[id].color = RED;
        nodes[id].left = nodes[id].right = nodes[id].parent = nil_id;
        return id;
    }

    void left_rotate(int x) {
        int y = nodes[x].right;
        nodes[x].right = nodes[y].left;
        if (nodes[y].left != nil_id) nodes[nodes[y].left].parent = x;
        nodes[y].parent = nodes[x].parent;
        if (nodes[x].parent == nil_id) root = y;
        else if (x == nodes[nodes[x].parent].left) nodes[nodes[x].parent].left = y;
        else nodes[nodes[x].parent].right = y;
        nodes[y].left = x;
        nodes[x].parent = y;
        total_rotations++;
    }

    void right_rotate(int x) {
        int y = nodes[x].left;
        nodes[x].left = nodes[y].right;
        if (nodes[y].right != nil_id) nodes[nodes[y].right].parent = x;
        nodes[y].parent = nodes[x].parent;
        if (nodes[x].parent == nil_id) root = y;
        else if (x == nodes[nodes[x].parent].right) nodes[nodes[x].parent].right = y;
        else nodes[nodes[x].parent].left = y;
        nodes[y].right = x;
        nodes[x].parent = y;
        total_rotations++;
    }

    void insert_fixup(int z) {
        while (nodes[nodes[z].parent].color == RED) {
            int p = nodes[z].parent, gp = nodes[p].parent;
            if (p == nodes[gp].left) {
                int y = nodes[gp].right;
                if (nodes[y].color == RED) {
                    nodes[p].color = BLACK; nodes[y].color = BLACK; nodes[gp].color = RED; z = gp;
                } else {
                    if (z == nodes[p].right) { z = p; left_rotate(z); p = nodes[z].parent; gp = nodes[p].parent; }
                    nodes[p].color = BLACK; nodes[gp].color = RED; right_rotate(gp);
                }
            } else {
                int y = nodes[gp].left;
                if (nodes[y].color == RED) {
                    nodes[p].color = BLACK; nodes[y].color = BLACK; nodes[gp].color = RED; z = gp;
                } else {
                    if (z == nodes[p].left) { z = p; right_rotate(z); p = nodes[z].parent; gp = nodes[p].parent; }
                    nodes[p].color = BLACK; nodes[gp].color = RED; left_rotate(gp);
                }
            }
        }
        nodes[root].color = BLACK;
    }

    void do_insert(int key, int val) {
        int z = new_node(key, val);
        int y = nil_id, x = root;
        vector<int> path;
        while (x != nil_id) {
            y = x; path.push_back(x); total_comparisons++;
            x = (z < x ? (key < nodes[x].key ? nodes[x].left : nodes[x].right) :
                 (key < nodes[x].key ? nodes[x].left : nodes[x].right));
            if (key < nodes[x == nil_id ? y : x].key && x == nil_id) break;
            if (key >= nodes[x == nil_id ? y : x].key && x == nil_id) break;
            // normal descent
            if (key < nodes[y].key) x = nodes[y].left; else x = nodes[y].right;
            // revert
            x = (key < nodes[y].key) ? nodes[y].left : nodes[y].right;
            break;
        }
        // redo properly
        y = nil_id; x = root;
        path.clear();
        while (x != nil_id) {
            y = x; path.push_back(x); total_comparisons++;
            if (key < nodes[x].key) x = nodes[x].left;
            else x = nodes[x].right;
        }
        nodes[z].parent = y;
        if (y == nil_id) root = z;
        else if (key < nodes[y].key) nodes[y].left = z;
        else nodes[y].right = z;
        insert_fixup(z);
        StepRecord sr; sr.op="insert"; sr.detail="RBT insert key="+to_string(key);
        sr.path=path; sr.highlight={z}; sr.comparisons=total_comparisons; sr.rotations=total_rotations;
        trace.push_back(sr);
    }

    void rb_transplant(int u, int v) {
        if      (nodes[u].parent == nil_id)            root = v;
        else if (u == nodes[nodes[u].parent].left)     nodes[nodes[u].parent].left  = v;
        else                                           nodes[nodes[u].parent].right = v;
        nodes[v].parent = nodes[u].parent;
    }

    int tree_min(int x) {
        while (nodes[x].left != nil_id) x = nodes[x].left;
        return x;
    }

    void delete_fixup(int x) {
        while (x != root && nodes[x].color == BLACK) {
            if (x == nodes[nodes[x].parent].left) {
                int w = nodes[nodes[x].parent].right;
                if (nodes[w].color == RED) { nodes[w].color=BLACK; nodes[nodes[x].parent].color=RED; left_rotate(nodes[x].parent); w=nodes[nodes[x].parent].right; }
                if (nodes[nodes[w].left].color==BLACK && nodes[nodes[w].right].color==BLACK) { nodes[w].color=RED; x=nodes[x].parent; }
                else { if (nodes[nodes[w].right].color==BLACK) { nodes[nodes[w].left].color=BLACK; nodes[w].color=RED; right_rotate(w); w=nodes[nodes[x].parent].right; } nodes[w].color=nodes[nodes[x].parent].color; nodes[nodes[x].parent].color=BLACK; nodes[nodes[w].right].color=BLACK; left_rotate(nodes[x].parent); x=root; }
            } else {
                int w = nodes[nodes[x].parent].left;
                if (nodes[w].color==RED) { nodes[w].color=BLACK; nodes[nodes[x].parent].color=RED; right_rotate(nodes[x].parent); w=nodes[nodes[x].parent].left; }
                if (nodes[nodes[w].right].color==BLACK && nodes[nodes[w].left].color==BLACK) { nodes[w].color=RED; x=nodes[x].parent; }
                else { if (nodes[nodes[w].left].color==BLACK) { nodes[nodes[w].right].color=BLACK; nodes[w].color=RED; left_rotate(w); w=nodes[nodes[x].parent].left; } nodes[w].color=nodes[nodes[x].parent].color; nodes[nodes[x].parent].color=BLACK; nodes[nodes[w].left].color=BLACK; right_rotate(nodes[x].parent); x=root; }
            }
        }
        nodes[x].color = BLACK;
    }

    void do_delete(int key) {
        int z = root;
        vector<int> path;
        while (z != nil_id && nodes[z].key != key) {
            path.push_back(z); total_comparisons++;
            z = (key < nodes[z].key) ? nodes[z].left : nodes[z].right;
        }
        if (z == nil_id) return;
        int y=z, y_orig_color=nodes[y].color, x=nil_id;
        if (nodes[z].left==nil_id) { x=nodes[z].right; rb_transplant(z,nodes[z].right); }
        else if (nodes[z].right==nil_id) { x=nodes[z].left; rb_transplant(z,nodes[z].left); }
        else { y=tree_min(nodes[z].right); y_orig_color=nodes[y].color; x=nodes[y].right;
               if (nodes[y].parent==z) nodes[x].parent=y;
               else { rb_transplant(y,nodes[y].right); nodes[y].right=nodes[z].right; nodes[nodes[y].right].parent=y; }
               rb_transplant(z,y); nodes[y].left=nodes[z].left; nodes[nodes[y].left].parent=y; nodes[y].color=nodes[z].color; }
        if (y_orig_color==BLACK) delete_fixup(x);
        StepRecord sr; sr.op="delete"; sr.detail="RBT delete key="+to_string(key);
        sr.path=path; sr.comparisons=total_comparisons; sr.rotations=total_rotations;
        trace.push_back(sr);
    }

    vector<map<string,int>> serialize() {
        vector<map<string,int>> result;
        for (int i = 0; i < (int)nodes.size(); i++) {
            if (nodes[i].is_null) continue;
            auto& nd = nodes[i];
            map<string,int> m;
            m["id"]=i; m["key"]=nd.key; m["value"]=nd.value;
            m["color"]=nd.color; m["left"]=nd.left; m["right"]=nd.right; m["parent"]=nd.parent;
            result.push_back(m);
        }
        return result;
    }

    int get_root() { return root; }
    int get_nil()  { return nil_id; }
    void clear_trace() { trace.clear(); total_comparisons=0; total_rotations=0; }
};

// ─────────────────────────────────────────────
//  Segment Tree with Lazy Propagation
// ─────────────────────────────────────────────

class SegmentTree {
public:
    int n;
    vector<int> tree;
    vector<int> lazy;
    vector<StepRecord> trace;

    SegmentTree(vector<int>& arr) {
        n = arr.size();
        tree.assign(4*n, 0);
        lazy.assign(4*n, 0);
        build(arr, 1, 0, n-1);
    }

    void build(vector<int>& arr, int node, int lo, int hi) {
        if (lo == hi) { tree[node] = arr[lo]; return; }
        int mid = (lo+hi)/2;
        build(arr, 2*node, lo, mid);
        build(arr, 2*node+1, mid+1, hi);
        tree[node] = tree[2*node] + tree[2*node+1];
    }

    void push_down(int node, int lo, int hi) {
        if (lazy[node]) {
            int mid = (lo+hi)/2;
            tree[2*node]   += lazy[node] * (mid - lo + 1);
            tree[2*node+1] += lazy[node] * (hi - mid);
            lazy[2*node]   += lazy[node];
            lazy[2*node+1] += lazy[node];
            lazy[node] = 0;
        }
    }

    void range_update(int node, int lo, int hi, int l, int r, int val) {
        if (r < lo || hi < l) return;
        if (l <= lo && hi <= r) {
            tree[node] += val * (hi - lo + 1);
            lazy[node] += val;
            return;
        }
        push_down(node, lo, hi);
        int mid = (lo+hi)/2;
        range_update(2*node, lo, mid, l, r, val);
        range_update(2*node+1, mid+1, hi, l, r, val);
        tree[node] = tree[2*node] + tree[2*node+1];
    }

    int range_query(int node, int lo, int hi, int l, int r) {
        if (r < lo || hi < l) return 0;
        if (l <= lo && hi <= r) return tree[node];
        push_down(node, lo, hi);
        int mid = (lo+hi)/2;
        return range_query(2*node, lo, mid, l, r) +
               range_query(2*node+1, mid+1, hi, l, r);
    }

    int query(int l, int r) { return range_query(1, 0, n-1, l, r); }
    void update(int l, int r, int val) { range_update(1, 0, n-1, l, r, val); }

    vector<int> get_array() {
        vector<int> arr(n);
        for (int i=0;i<n;i++) arr[i]=query(i,i);
        return arr;
    }

    vector<int> get_internal() { return tree; }
};

// ─────────────────────────────────────────────
//  Binary Indexed Tree (Fenwick Tree)
// ─────────────────────────────────────────────

class FenwickTree {
public:
    int n;
    vector<int> bit;
    vector<StepRecord> trace;

    FenwickTree(int n) : n(n), bit(n+1, 0) {}

    FenwickTree(vector<int>& arr) : n(arr.size()), bit(arr.size()+1, 0) {
        for (int i=0;i<n;i++) update(i+1, arr[i]);
    }

    void update(int i, int delta) {
        for (; i <= n; i += i & (-i)) bit[i] += delta;
    }

    int prefix_sum(int i) {
        int s = 0;
        for (; i > 0; i -= i & (-i)) s += bit[i];
        return s;
    }

    int range_sum(int l, int r) { return prefix_sum(r) - prefix_sum(l-1); }

    // find kth element (1-indexed)
    int find_kth(int k) {
        int pos = 0;
        for (int pw = 1 << (int)log2(n); pw; pw >>= 1)
            if (pos+pw <= n && bit[pos+pw] < k) { pos += pw; k -= bit[pos]; }
        return pos + 1;
    }

    vector<int> get_bit() { return bit; }
};

// ─────────────────────────────────────────────
//  Min/Max/Generic Heap
// ─────────────────────────────────────────────

class BinaryHeap {
public:
    vector<HeapNode> data;
    bool is_min;
    vector<StepRecord> trace;
    int comparisons = 0;

    BinaryHeap(bool min_heap = true) : is_min(min_heap) {}

    bool cmp(int a, int b) {
        comparisons++;
        return is_min ? (data[a].key < data[b].key) : (data[a].key > data[b].key);
    }

    void heapify_up(int i) {
        vector<int> path = {i};
        while (i > 0) {
            int p = (i-1)/2;
            path.push_back(p);
            if (cmp(i, p)) { swap(data[i], data[p]); i = p; }
            else break;
        }
        StepRecord sr; sr.op="heapify_up"; sr.path=path; sr.detail="Bubble up from idx="+to_string(i);
        trace.push_back(sr);
    }

    void heapify_down(int i) {
        int n = data.size();
        vector<int> path = {i};
        while (true) {
            int best = i, l = 2*i+1, r = 2*i+2;
            if (l < n && cmp(l, best)) best = l;
            if (r < n && cmp(r, best)) best = r;
            if (best == i) break;
            swap(data[i], data[best]);
            path.push_back(best);
            i = best;
        }
        StepRecord sr; sr.op="heapify_down"; sr.path=path; sr.detail="Sink down from idx="+to_string(i);
        trace.push_back(sr);
    }

    void insert(int key, int val = 0) {
        HeapNode hn; hn.key=key; hn.value=val;
        data.push_back(hn);
        heapify_up(data.size()-1);
    }

    HeapNode extract_top() {
        if (data.empty()) throw runtime_error("Heap empty");
        HeapNode top = data[0];
        data[0] = data.back(); data.pop_back();
        if (!data.empty()) heapify_down(0);
        return top;
    }

    int peek_top() { return data.empty() ? -1 : data[0].key; }

    void build_heap(vector<int>& keys) {
        data.clear();
        for (int k : keys) { HeapNode hn; hn.key=k; data.push_back(hn); }
        for (int i = data.size()/2 - 1; i >= 0; i--) heapify_down(i);
    }

    vector<int> heap_sort() {
        BinaryHeap tmp = *this;
        vector<int> sorted;
        while (!tmp.data.empty()) sorted.push_back(tmp.extract_top().key);
        return sorted;
    }

    vector<map<string,int>> serialize() {
        vector<map<string,int>> res;
        for (int i=0;i<(int)data.size();i++) {
            map<string,int> m;
            m["id"]=i; m["key"]=data[i].key; m["value"]=data[i].value;
            m["left"]=2*i+1 < (int)data.size() ? 2*i+1 : -1;
            m["right"]=2*i+2 < (int)data.size() ? 2*i+2 : -1;
            m["parent"]=i>0?(i-1)/2:-1;
            res.push_back(m);
        }
        return res;
    }

    void increase_key(int idx, int new_key) {
        if (is_min && new_key > data[idx].key) { data[idx].key=new_key; heapify_down(idx); }
        else if (!is_min && new_key < data[idx].key) { data[idx].key=new_key; heapify_down(idx); }
        else { data[idx].key=new_key; heapify_up(idx); }
    }

    void clear_trace() { trace.clear(); comparisons=0; }
};

// ─────────────────────────────────────────────
//  Treap
// ─────────────────────────────────────────────

class Treap {
public:
    vector<TreeNode> nodes;
    int root = -1;
    vector<StepRecord> trace;

    int new_node(int key) {
        nodes.emplace_back();
        int id = nodes.size()-1;
        nodes[id].key = key;
        nodes[id].priority = rand_priority();
        nodes[id].size = 1;
        return id;
    }

    void update(int n) {
        if (n==-1) return;
        nodes[n].size = 1 + (nodes[n].left!=-1?nodes[nodes[n].left].size:0)
                          + (nodes[n].right!=-1?nodes[nodes[n].right].size:0);
    }

    pair<int,int> split(int n, int key) { // returns (left_tree, right_tree) where left <= key
        if (n==-1) return {-1,-1};
        if (nodes[n].key <= key) {
            auto [l,r] = split(nodes[n].right, key);
            nodes[n].right = l; update(n);
            return {n, r};
        } else {
            auto [l,r] = split(nodes[n].left, key);
            nodes[n].left = r; update(n);
            return {l, n};
        }
    }

    int merge(int l, int r) {
        if (l==-1) return r;
        if (r==-1) return l;
        if (nodes[l].priority > nodes[r].priority) {
            nodes[l].right = merge(nodes[l].right, r); update(l); return l;
        } else {
            nodes[r].left = merge(l, nodes[r].left); update(r); return r;
        }
    }

    void insert(int key) {
        int z = new_node(key);
        auto [l, r] = split(root, key-1);
        root = merge(merge(l, z), r);
        StepRecord sr; sr.op="insert"; sr.detail="Treap insert "+to_string(key); sr.highlight={z}; trace.push_back(sr);
    }

    void erase(int key) {
        auto [l, mr] = split(root, key-1);
        auto [m, r]  = split(mr, key);
        root = merge(l, r); // discard m (the node with key)
        StepRecord sr; sr.op="delete"; sr.detail="Treap delete "+to_string(key); trace.push_back(sr);
    }

    bool search(int key) {
        int cur = root;
        while (cur != -1) {
            if (nodes[cur].key == key) return true;
            cur = (key < nodes[cur].key) ? nodes[cur].left : nodes[cur].right;
        }
        return false;
    }

    vector<map<string,int>> serialize() {
        vector<map<string,int>> res;
        for (int i=0;i<(int)nodes.size();i++) {
            map<string,int> m;
            m["id"]=i; m["key"]=nodes[i].key; m["priority"]=nodes[i].priority;
            m["size"]=nodes[i].size; m["left"]=nodes[i].left; m["right"]=nodes[i].right;
            res.push_back(m);
        }
        return res;
    }
};

// ─────────────────────────────────────────────
//  Splay Tree
// ─────────────────────────────────────────────

class SplayTree {
public:
    vector<TreeNode> nodes;
    int root = -1;
    vector<StepRecord> trace;

    int new_node(int key) {
        nodes.emplace_back(); int id=nodes.size()-1;
        nodes[id].key=key; nodes[id].splay_size=1; return id;
    }

    void update(int n) {
        if(n==-1)return;
        nodes[n].splay_size = 1;
        if(nodes[n].left!=-1) nodes[n].splay_size += nodes[nodes[n].left].splay_size;
        if(nodes[n].right!=-1) nodes[n].splay_size += nodes[nodes[n].right].splay_size;
    }

    void set_child(int p, int c, bool is_right) {
        if(p!=-1) { if(is_right) nodes[p].right=c; else nodes[p].left=c; }
        if(c!=-1) nodes[c].parent = p;
    }

    void rotate(int x) {
        int p=nodes[x].parent, g=nodes[p].parent;
        bool xr=(nodes[p].right==x), pr=(g!=-1&&nodes[g].right==p);
        set_child(p, xr?nodes[x].left:nodes[x].right, xr);
        set_child(x, p, !xr);
        set_child(g, x, pr);
        if(g==-1) root=x;
        update(p); update(x);
    }

    void splay(int x, int goal=-1) {
        while(nodes[x].parent!=goal) {
            int p=nodes[x].parent, g=nodes[p].parent;
            if(g!=goal) {
                bool same=((nodes[g].right==p)==(nodes[p].right==x));
                rotate(same?p:x);
            }
            rotate(x);
        }
        if(goal==-1) root=x;
    }

    void insert(int key) {
        if(root==-1) { root=new_node(key); return; }
        int cur=root, p=-1;
        while(cur!=-1) { p=cur; cur=(key<nodes[cur].key)?nodes[cur].left:nodes[cur].right; }
        int z=new_node(key); nodes[z].parent=p;
        if(key<nodes[p].key) nodes[p].left=z; else nodes[p].right=z;
        splay(z);
        StepRecord sr; sr.op="insert"; sr.detail="Splay insert "+to_string(key); sr.highlight={z}; trace.push_back(sr);
    }

    bool search(int key) {
        int cur=root;
        while(cur!=-1) {
            if(nodes[cur].key==key) { splay(cur); return true; }
            cur=(key<nodes[cur].key)?nodes[cur].left:nodes[cur].right;
        }
        return false;
    }

    vector<map<string,int>> serialize() {
        vector<map<string,int>> res;
        for(int i=0;i<(int)nodes.size();i++) {
            map<string,int> m;
            m["id"]=i; m["key"]=nodes[i].key; m["size"]=nodes[i].splay_size;
            m["left"]=nodes[i].left; m["right"]=nodes[i].right; m["parent"]=nodes[i].parent;
            res.push_back(m);
        }
        return res;
    }
};

// ─────────────────────────────────────────────
//  B-Tree
// ─────────────────────────────────────────────

class BTree {
public:
    struct BNode {
        vector<int> keys;
        vector<int> children; // indices into nodes[]
        bool is_leaf;
        BNode(bool leaf=true) : is_leaf(leaf) {}
    };

    int t; // minimum degree
    vector<BNode> nodes;
    int root = -1;
    vector<StepRecord> trace;

    BTree(int t=2) : t(t) {}

    int new_node(bool leaf=true) {
        nodes.emplace_back(leaf); return nodes.size()-1;
    }

    void search(int n, int key, vector<int>& path) {
        if(n==-1) return;
        path.push_back(n);
        int i=0;
        while(i<(int)nodes[n].keys.size() && key>nodes[n].keys[i]) i++;
        if(i<(int)nodes[n].keys.size() && key==nodes[n].keys[i]) return;
        if(nodes[n].is_leaf) return;
        search(nodes[n].children[i], key, path);
    }

    void split_child(int x, int i) {
        int y = nodes[x].children[i];
        int z = new_node(nodes[y].is_leaf);
        // z gets last t-1 keys of y
        for(int j=0;j<t-1;j++) nodes[z].keys.push_back(nodes[y].keys[j+t]);
        if(!nodes[y].is_leaf)
            for(int j=0;j<t;j++) nodes[z].children.push_back(nodes[y].children[j+t]);
        int mid = nodes[y].keys[t-1];
        nodes[y].keys.resize(t-1);
        if(!nodes[y].is_leaf) nodes[y].children.resize(t);
        nodes[x].children.insert(nodes[x].children.begin()+i+1, z);
        nodes[x].keys.insert(nodes[x].keys.begin()+i, mid);
    }

    void insert_non_full(int n, int key) {
        int i = nodes[n].keys.size()-1;
        if(nodes[n].is_leaf) {
            nodes[n].keys.push_back(0);
            while(i>=0 && key<nodes[n].keys[i]) { nodes[n].keys[i+1]=nodes[n].keys[i]; i--; }
            nodes[n].keys[i+1]=key;
        } else {
            while(i>=0 && key<nodes[n].keys[i]) i--;
            i++;
            if((int)nodes[nodes[n].children[i]].keys.size()==2*t-1) {
                split_child(n,i);
                if(key>nodes[n].keys[i]) i++;
            }
            insert_non_full(nodes[n].children[i], key);
        }
    }

    void insert(int key) {
        if(root==-1) { root=new_node(true); nodes[root].keys.push_back(key); return; }
        if((int)nodes[root].keys.size()==2*t-1) {
            int s=new_node(false);
            nodes[s].children.push_back(root);
            split_child(s,0);
            root=s;
        }
        insert_non_full(root,key);
        StepRecord sr; sr.op="insert"; sr.detail="BTree insert "+to_string(key); trace.push_back(sr);
    }

    vector<map<string,vector<int>>> serialize() {
        vector<map<string,vector<int>>> res;
        for(int i=0;i<(int)nodes.size();i++) {
            map<string,vector<int>> m;
            m["keys"]=nodes[i].keys;
            m["children"]=nodes[i].children;
            m["is_leaf"]={nodes[i].is_leaf?1:0};
            m["id"]={i};
            res.push_back(m);
        }
        return res;
    }
};

// ─────────────────────────────────────────────
//  Trie
// ─────────────────────────────────────────────

class Trie {
public:
    struct TrieNode {
        map<char,int> children;
        bool is_end = false;
        int count = 0;
        int word_count = 0;
    };

    vector<TrieNode> nodes;
    vector<StepRecord> trace;

    Trie() { nodes.emplace_back(); } // root = 0

    void insert(const string& word) {
        int cur=0; vector<int> path={0};
        for(char c : word) {
            if(!nodes[cur].children.count(c)) {
                nodes.emplace_back();
                nodes[cur].children[c] = nodes.size()-1;
            }
            cur = nodes[cur].children[c];
            nodes[cur].count++;
            path.push_back(cur);
        }
        nodes[cur].is_end = true;
        nodes[cur].word_count++;
        StepRecord sr; sr.op="insert"; sr.detail="Trie insert '"+word+"'"; sr.path=path; trace.push_back(sr);
    }

    bool search(const string& word) {
        int cur=0;
        for(char c : word) {
            if(!nodes[cur].children.count(c)) return false;
            cur = nodes[cur].children[c];
        }
        return nodes[cur].is_end;
    }

    bool starts_with(const string& prefix) {
        int cur=0;
        for(char c : prefix) {
            if(!nodes[cur].children.count(c)) return false;
            cur = nodes[cur].children[c];
        }
        return true;
    }

    vector<string> autocomplete(const string& prefix) {
        int cur=0;
        for(char c : prefix) {
            if(!nodes[cur].children.count(c)) return {};
            cur = nodes[cur].children[c];
        }
        vector<string> results;
        function<void(int,string)> dfs = [&](int n, string s) {
            if(nodes[n].is_end) results.push_back(prefix + s);
            for(auto& [c,ch] : nodes[n].children) dfs(ch, s+c);
        };
        dfs(cur, "");
        return results;
    }

    // Longest common prefix
    string lcp() {
        string res;
        int cur=0;
        while(nodes[cur].children.size()==1 && !nodes[cur].is_end) {
            auto [c,child] = *nodes[cur].children.begin();
            res += c; cur = child;
        }
        return res;
    }

    vector<tuple<int,int,char,bool,int>> serialize() { // id, parent, char, is_end, count
        vector<tuple<int,int,char,bool,int>> res;
        function<void(int,int,char)> dfs = [&](int n, int p, char c) {
            res.emplace_back(n, p, c, nodes[n].is_end, nodes[n].count);
            for(auto& [ch, cn] : nodes[n].children) dfs(cn, n, ch);
        };
        dfs(0, -1, ' ');
        return res;
    }
};

// ─────────────────────────────────────────────
//  Union-Find (Disjoint Set Union)
// ─────────────────────────────────────────────

class DSU {
public:
    vector<int> parent, rank_, size_;
    int components;
    vector<StepRecord> trace;

    DSU(int n) : parent(n), rank_(n,0), size_(n,1), components(n) {
        iota(parent.begin(), parent.end(), 0);
    }

    int find(int x) {
        if(parent[x]!=x) parent[x]=find(parent[x]); // path compression
        return parent[x];
    }

    bool unite(int x, int y) {
        x=find(x); y=find(y);
        if(x==y) return false;
        if(rank_[x]<rank_[y]) swap(x,y);
        parent[y]=x; size_[x]+=size_[y];
        if(rank_[x]==rank_[y]) rank_[x]++;
        components--;
        StepRecord sr; sr.op="union"; sr.detail="Unite "+to_string(x)+" and "+to_string(y); trace.push_back(sr);
        return true;
    }

    bool connected(int x, int y) { return find(x)==find(y); }
    int get_size(int x) { return size_[find(x)]; }
    vector<int> get_parent() { return parent; }
};

// ─────────────────────────────────────────────
//  Graph Algorithms on Trees
// ─────────────────────────────────────────────

struct GraphAlgo {
    // Topological sort (Kahn's algorithm)
    static vector<int> topo_sort(int n, vector<vector<int>>& adj) {
        vector<int> indegree(n,0);
        for(int u=0;u<n;u++) for(int v:adj[u]) indegree[v]++;
        queue<int> q;
        for(int i=0;i<n;i++) if(indegree[i]==0) q.push(i);
        vector<int> order;
        while(!q.empty()) {
            int u=q.front(); q.pop(); order.push_back(u);
            for(int v:adj[u]) if(--indegree[v]==0) q.push(v);
        }
        return order;
    }

    // Dijkstra's shortest path
    static vector<int> dijkstra(int n, int src, vector<vector<pair<int,int>>>& adj) {
        vector<int> dist(n, INT_MAX);
        priority_queue<pair<int,int>, vector<pair<int,int>>, greater<>> pq;
        dist[src]=0; pq.push({0,src});
        while(!pq.empty()) {
            auto [d,u]=pq.top(); pq.pop();
            if(d>dist[u]) continue;
            for(auto [v,w]:adj[u]) if(dist[u]+w<dist[v]) { dist[v]=dist[u]+w; pq.push({dist[v],v}); }
        }
        return dist;
    }

    // Prim's MST
    static vector<pair<int,int>> prim_mst(int n, vector<vector<pair<int,int>>>& adj) {
        vector<bool> inMST(n,false);
        vector<int> key(n,INT_MAX), parent(n,-1);
        priority_queue<pair<int,int>, vector<pair<int,int>>, greater<>> pq;
        key[0]=0; pq.push({0,0});
        while(!pq.empty()) {
            int u=pq.top().second; pq.pop();
            if(inMST[u]) continue;
            inMST[u]=true;
            for(auto [v,w]:adj[u]) if(!inMST[v] && w<key[v]) { key[v]=w; parent[v]=u; pq.push({key[v],v}); }
        }
        vector<pair<int,int>> mst;
        for(int i=1;i<n;i++) if(parent[i]!=-1) mst.push_back({parent[i],i});
        return mst;
    }

    // Heavy-Light Decomposition path sum
    static vector<int> hld_decompose(int n, int root, vector<vector<int>>& adj) {
        vector<int> sz(n,1), heavy(n,-1), depth(n,0), parent(n,-1), head(n), pos(n);
        function<void(int,int)> dfs = [&](int u, int p) {
            for(int v:adj[u]) if(v!=p) { parent[v]=u; depth[v]=depth[u]+1; dfs(v,u); sz[u]+=sz[v]; if(heavy[u]==-1||sz[v]>sz[heavy[u]]) heavy[u]=v; }
        };
        dfs(root,-1);
        int cur=0;
        function<void(int,int,int)> decompose=[&](int u, int h, int p) {
            head[u]=h; pos[u]=cur++;
            if(heavy[u]!=-1) decompose(heavy[u],h,u);
            for(int v:adj[u]) if(v!=p&&v!=heavy[u]) decompose(v,v,u);
        };
        decompose(root,root,-1);
        return pos;
    }
};

// ─────────────────────────────────────────────
//  Huffman Coding Tree
// ─────────────────────────────────────────────

class HuffmanTree {
public:
    struct HNode {
        char ch; int freq;
        int left=-1, right=-1;
        bool is_leaf=false;
    };

    vector<HNode> nodes;
    int root=-1;
    map<char,string> codes;

    void build(map<char,int>& freq_map) {
        using P = pair<int,int>;
        priority_queue<P, vector<P>, greater<P>> pq;
        for(auto& [c,f] : freq_map) {
            nodes.push_back({c,f,-1,-1,true});
            pq.push({f,(int)nodes.size()-1});
        }
        while(pq.size()>1) {
            auto [f1,l] = pq.top(); pq.pop();
            auto [f2,r] = pq.top(); pq.pop();
            nodes.push_back({'\0', f1+f2, l, r, false});
            pq.push({f1+f2,(int)nodes.size()-1});
        }
        root = pq.top().second;
        generate_codes(root,"");
    }

    void generate_codes(int n, string code) {
        if(n==-1) return;
        if(nodes[n].is_leaf) { codes[nodes[n].ch]=code; return; }
        generate_codes(nodes[n].left, code+"0");
        generate_codes(nodes[n].right, code+"1");
    }

    map<char,string> get_codes() { return codes; }

    vector<map<string,int>> serialize() {
        vector<map<string,int>> res;
        for(int i=0;i<(int)nodes.size();i++) {
            map<string,int> m;
            m["id"]=i; m["freq"]=nodes[i].freq;
            m["ch"]=(int)nodes[i].ch; m["is_leaf"]=nodes[i].is_leaf?1:0;
            m["left"]=nodes[i].left; m["right"]=nodes[i].right;
            res.push_back(m);
        }
        return res;
    }
};

// ─────────────────────────────────────────────
//  Persistent Segment Tree (version control)
// ─────────────────────────────────────────────

class PersistentSegTree {
public:
    struct PSNode { int left,right,val; };
    vector<PSNode> nodes;
    vector<int> roots;
    int n;

    PersistentSegTree(int n) : n(n) {
        nodes.push_back({-1,-1,0});
        roots.push_back(0);
    }

    int update(int prev, int lo, int hi, int pos, int val) {
        PSNode nd = nodes[prev];
        nodes.push_back(nd);
        int cur = nodes.size()-1;
        if(lo==hi) { nodes[cur].val+=val; return cur; }
        int mid=(lo+hi)/2;
        if(pos<=mid) nodes[cur].left  = update(nodes[prev].left, lo, mid, pos, val);
        else         nodes[cur].right = update(nodes[prev].right, mid+1, hi, pos, val);
        nodes[cur].val = (nodes[cur].left!=-1?nodes[nodes[cur].left].val:0)
                       + (nodes[cur].right!=-1?nodes[nodes[cur].right].val:0);
        return cur;
    }

    void add_version(int pos, int val) {
        int new_root = update(roots.back(), 0, n-1, pos, val);
        roots.push_back(new_root);
    }

    int query(int node, int lo, int hi, int l, int r) {
        if(node==-1||r<lo||hi<l) return 0;
        if(l<=lo&&hi<=r) return nodes[node].val;
        int mid=(lo+hi)/2;
        return query(nodes[node].left,lo,mid,l,r) + query(nodes[node].right,mid+1,hi,l,r);
    }

    int query_version(int version, int l, int r) {
        if(version>=(int)roots.size()) return 0;
        return query(roots[version],0,n-1,l,r);
    }

    int version_count() { return roots.size(); }
};

// ─────────────────────────────────────────────
//  Statistics / Analytics Engine
// ─────────────────────────────────────────────

struct TreeStats {
    int node_count;
    int height;
    int diameter;
    double avg_depth;
    int leaf_count;
    int internal_count;
    bool is_balanced;
    bool is_complete;
    bool is_perfect;
    int total_comparisons;
    int total_rotations;
    double balance_factor_variance;
};

TreeStats compute_avl_stats(AVLTree& t) {
    TreeStats s{};
    s.total_comparisons = t.total_comparisons;
    s.total_rotations   = t.total_rotations;
    s.node_count = t.node_count;
    s.height = t.height(t.root);
    s.diameter = t.diameter();
    // compute other stats via traversal
    int depth_sum=0, leaves=0, internals=0;
    vector<int> bfs_vec;
    vector<int> depths;
    function<void(int,int)> dfs=[&](int n, int d) {
        if(n==-1) return;
        depths.push_back(d);
        depth_sum+=d;
        if(t.nodes[n].left==-1&&t.nodes[n].right==-1) leaves++;
        else internals++;
        dfs(t.nodes[n].left,d+1);
        dfs(t.nodes[n].right,d+1);
    };
    dfs(t.root,0);
    s.leaf_count=leaves; s.internal_count=internals;
    s.avg_depth = s.node_count ? (double)depth_sum/s.node_count : 0;
    s.is_balanced = (s.height <= (int)(1.44*log2(s.node_count+1)+2));
    return s;
}

// ─────────────────────────────────────────────
//  Pybind11 Module
// ─────────────────────────────────────────────

PYBIND11_MODULE(core_engine, m) {
    m.doc() = "Tree & Heap Visualizer - C++ Core Engine";

    // ── StepRecord ──
    py::class_<StepRecord>(m, "StepRecord")
        .def(py::init<>())
        .def_readwrite("op", &StepRecord::op)
        .def_readwrite("detail", &StepRecord::detail)
        .def_readwrite("highlight", &StepRecord::highlight)
        .def_readwrite("path", &StepRecord::path)
        .def_readwrite("comparisons", &StepRecord::comparisons)
        .def_readwrite("rotations", &StepRecord::rotations)
        .def_readwrite("auxiliary", &StepRecord::auxiliary);

    // ── AVLTree ──
    py::class_<AVLTree>(m, "AVLTree")
        .def(py::init<>())
        .def("do_insert",          &AVLTree::do_insert)
        .def("do_delete",          &AVLTree::do_delete)
        .def("search",             &AVLTree::search)
        .def("serialize",          &AVLTree::serialize)
        .def("inorder",            &AVLTree::inorder_traversal)
        .def("preorder",           &AVLTree::preorder_traversal)
        .def("postorder",          &AVLTree::postorder_traversal)
        .def("level_order",        &AVLTree::level_order_traversal)
        .def("kth_smallest",       &AVLTree::kth_smallest)
        .def("lca",                &AVLTree::lca)
        .def("diameter",           &AVLTree::diameter)
        .def("floor_key",          &AVLTree::floor_key)
        .def("ceil_key",           &AVLTree::ceil_key)
        .def("clear_trace",        &AVLTree::clear_trace)
        .def_readwrite("trace",    &AVLTree::trace)
        .def_readwrite("root",     &AVLTree::root)
        .def_readwrite("total_comparisons", &AVLTree::total_comparisons)
        .def_readwrite("total_rotations",   &AVLTree::total_rotations);

    // ── RBTree ──
    py::class_<RBTree>(m, "RBTree")
        .def(py::init<>())
        .def("do_insert",   &RBTree::do_insert)
        .def("do_delete",   &RBTree::do_delete)
        .def("serialize",   &RBTree::serialize)
        .def("get_root",    &RBTree::get_root)
        .def("get_nil",     &RBTree::get_nil)
        .def("clear_trace", &RBTree::clear_trace)
        .def_readwrite("trace", &RBTree::trace)
        .def_readwrite("total_comparisons", &RBTree::total_comparisons)
        .def_readwrite("total_rotations",   &RBTree::total_rotations);

    // ── SegmentTree ──
    py::class_<SegmentTree>(m, "SegmentTree")
        .def(py::init<vector<int>&>())
        .def("query",        &SegmentTree::query)
        .def("update",       &SegmentTree::update)
        .def("get_array",    &SegmentTree::get_array)
        .def("get_internal", &SegmentTree::get_internal)
        .def_readwrite("n",    &SegmentTree::n)
        .def_readwrite("tree", &SegmentTree::tree);

    // ── FenwickTree ──
    py::class_<FenwickTree>(m, "FenwickTree")
        .def(py::init<int>())
        .def(py::init<vector<int>&>())
        .def("update",      &FenwickTree::update)
        .def("prefix_sum",  &FenwickTree::prefix_sum)
        .def("range_sum",   &FenwickTree::range_sum)
        .def("find_kth",    &FenwickTree::find_kth)
        .def("get_bit",     &FenwickTree::get_bit);

    // ── BinaryHeap ──
    py::class_<BinaryHeap>(m, "BinaryHeap")
        .def(py::init<bool>(), py::arg("min_heap")=true)
        .def("insert",       &BinaryHeap::insert, py::arg("key"), py::arg("val")=0)
        .def("extract_top",  &BinaryHeap::extract_top)
        .def("peek_top",     &BinaryHeap::peek_top)
        .def("build_heap",   &BinaryHeap::build_heap)
        .def("heap_sort",    &BinaryHeap::heap_sort)
        .def("serialize",    &BinaryHeap::serialize)
        .def("increase_key", &BinaryHeap::increase_key)
        .def("clear_trace",  &BinaryHeap::clear_trace)
        .def_readwrite("trace", &BinaryHeap::trace)
        .def_readwrite("comparisons", &BinaryHeap::comparisons);

    // ── Treap ──
    py::class_<Treap>(m, "Treap")
        .def(py::init<>())
        .def("insert",    &Treap::insert)
        .def("erase",     &Treap::erase)
        .def("search",    &Treap::search)
        .def("serialize", &Treap::serialize)
        .def_readwrite("root", &Treap::root);

    // ── SplayTree ──
    py::class_<SplayTree>(m, "SplayTree")
        .def(py::init<>())
        .def("insert",    &SplayTree::insert)
        .def("search",    &SplayTree::search)
        .def("serialize", &SplayTree::serialize)
        .def_readwrite("root", &SplayTree::root);

    // ── BTree ──
    py::class_<BTree>(m, "BTree")
        .def(py::init<int>(), py::arg("t")=2)
        .def("insert",    &BTree::insert)
        .def("serialize", &BTree::serialize)
        .def_readwrite("root", &BTree::root);

    // ── Trie ──
    py::class_<Trie>(m, "Trie")
        .def(py::init<>())
        .def("insert",       &Trie::insert)
        .def("search",       &Trie::search)
        .def("starts_with",  &Trie::starts_with)
        .def("autocomplete", &Trie::autocomplete)
        .def("lcp",          &Trie::lcp)
        .def("serialize",    &Trie::serialize);

    // ── DSU ──
    py::class_<DSU>(m, "DSU")
        .def(py::init<int>())
        .def("find",      &DSU::find)
        .def("unite",     &DSU::unite)
        .def("connected", &DSU::connected)
        .def("get_size",  &DSU::get_size)
        .def("get_parent",&DSU::get_parent)
        .def_readwrite("components", &DSU::components);

    // ── HuffmanTree ──
    py::class_<HuffmanTree>(m, "HuffmanTree")
        .def(py::init<>())
        .def("build",       &HuffmanTree::build)
        .def("get_codes",   &HuffmanTree::get_codes)
        .def("serialize",   &HuffmanTree::serialize)
        .def_readwrite("root", &HuffmanTree::root);

    // ── PersistentSegTree ──
    py::class_<PersistentSegTree>(m, "PersistentSegTree")
        .def(py::init<int>())
        .def("add_version",    &PersistentSegTree::add_version)
        .def("query_version",  &PersistentSegTree::query_version)
        .def("version_count",  &PersistentSegTree::version_count);

    // ── GraphAlgo (static methods) ──
    py::class_<GraphAlgo>(m, "GraphAlgo")
        .def_static("topo_sort",     &GraphAlgo::topo_sort)
        .def_static("dijkstra",      &GraphAlgo::dijkstra)
        .def_static("prim_mst",      &GraphAlgo::prim_mst)
        .def_static("hld_decompose", &GraphAlgo::hld_decompose);

    // ── TreeStats ──
    py::class_<TreeStats>(m, "TreeStats")
        .def(py::init<>())
        .def_readwrite("node_count",   &TreeStats::node_count)
        .def_readwrite("height",       &TreeStats::height)
        .def_readwrite("diameter",     &TreeStats::diameter)
        .def_readwrite("avg_depth",    &TreeStats::avg_depth)
        .def_readwrite("leaf_count",   &TreeStats::leaf_count)
        .def_readwrite("is_balanced",  &TreeStats::is_balanced)
        .def_readwrite("total_comparisons", &TreeStats::total_comparisons)
        .def_readwrite("total_rotations",   &TreeStats::total_rotations);

    m.def("compute_avl_stats", &compute_avl_stats, "Compute AVL tree statistics");
}
