#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>
#include <cstring>
#include <cstdint>
#include <climits>
#include <functional>

using namespace std;

struct Key {
    string str;
    int val;
    bool operator<(const Key& o) const {
        if (str != o.str) return str < o.str;
        return val < o.val;
    }
    bool operator==(const Key& o) const {
        return str == o.str && val == o.val;
    }
    bool operator>=(const Key& o) const {
        return !(*this < o);
    }
};

struct Node {
    bool leaf;
    vector<Key> keys;
    vector<Node*> children;
    Node* next;
    Node(bool l) : leaf(l), next(nullptr) {}
};

class BPTree {
    Node* root;
    const int maxLeaf = 200;
    const int maxInternal = 200;

    Node* findLeaf(Node* node, const Key& key) {
        while (!node->leaf) {
            int idx = 0;
            while (idx < node->keys.size() && key >= node->keys[idx]) idx++;
            node = node->children[idx];
        }
        return node;
    }

    void insertKey(Node* leaf, const Key& key) {
        auto it = lower_bound(leaf->keys.begin(), leaf->keys.end(), key);
        if (it != leaf->keys.end() && *it == key) return;
        leaf->keys.insert(it, key);
    }

    pair<Node*, Key> splitLeaf(Node* node) {
        int split = node->keys.size() / 2;
        while (split < node->keys.size() && node->keys[split].str == node->keys[split-1].str) split++;
        if (split >= node->keys.size()) split = node->keys.size() / 2;
        Node* newN = new Node(true);
        newN->keys.assign(node->keys.begin() + split, node->keys.end());
        node->keys.resize(split);
        newN->next = node->next;
        node->next = newN;
        return {newN, newN->keys[0]};
    }

    pair<Node*, Key> splitInternal(Node* node) {
        int split = node->keys.size() / 2;
        Node* newN = new Node(false);
        Key upKey = node->keys[split];
        newN->keys.assign(node->keys.begin() + split + 1, node->keys.end());
        node->keys.resize(split);
        newN->children.assign(node->children.begin() + split + 1, node->children.end());
        node->children.resize(split + 1);
        return {newN, upKey};
    }

    pair<Node*, Key> insertRec(Node* node, const Key& key) {
        if (node->leaf) {
            insertKey(node, key);
            if ((int)node->keys.size() > maxLeaf) {
                return splitLeaf(node);
            }
            return {nullptr, Key{}};
        }
        int idx = 0;
        while (idx < node->keys.size() && key >= node->keys[idx]) idx++;
        auto res = insertRec(node->children[idx], key);
        if (res.first) {
            node->children.insert(node->children.begin() + idx + 1, res.first);
            node->keys.insert(node->keys.begin() + idx, res.second);
            if ((int)node->keys.size() > maxInternal) {
                return splitInternal(node);
            }
        }
        return {nullptr, Key{}};
    }

    bool removeRec(Node* node, const Key& key) {
        if (node->leaf) {
            auto it = lower_bound(node->keys.begin(), node->keys.end(), key);
            if (it != node->keys.end() && *it == key) {
                node->keys.erase(it);
            }
            return false;
        }
        int idx = 0;
        while (idx < node->keys.size() && key >= node->keys[idx]) idx++;
        removeRec(node->children[idx], key);
        return false;
    }

public:
    BPTree() { root = new Node(true); }

    void insert(const Key& key) {
        auto res = insertRec(root, key);
        if (res.first) {
            Node* newRoot = new Node(false);
            newRoot->keys.push_back(res.second);
            newRoot->children.push_back(root);
            newRoot->children.push_back(res.first);
            root = newRoot;
        }
    }

    void remove(const Key& key) {
        removeRec(root, key);
    }

    void find(const string& str, ostream& out) {
        Key searchKey{str, INT_MIN};
        Node* node = root;
        while (!node->leaf) {
            int idx = 0;
            while (idx < node->keys.size() && searchKey >= node->keys[idx]) idx++;
            node = node->children[idx];
        }
        vector<int> vals;
        while (node) {
            bool stop = false;
            for (const auto& k : node->keys) {
                if (k.str == str) vals.push_back(k.val);
                else if (k.str > str) { stop = true; break; }
            }
            if (stop) break;
            node = node->next;
        }
        sort(vals.begin(), vals.end());
        if (vals.empty()) {
            out << "null";
        } else {
            for (size_t i = 0; i < vals.size(); ++i) {
                if (i) out << ' ';
                out << vals[i];
            }
        }
        out << '\n';
    }

    void save(const string& filename) {
        ofstream f(filename, ios::binary);
        if (!f) return;
        f.write("BPT1", 4);
        vector<Node*> nodes;
        function<void(Node*)> collect = [&](Node* n) {
            nodes.push_back(n);
            if (!n->leaf) for (auto c : n->children) collect(c);
        };
        collect(root);
        int nNodes = nodes.size();
        f.write(reinterpret_cast<const char*>(&nNodes), sizeof(int));
        int rootId = 0;
        for (int i = 0; i < nNodes; ++i) {
            if (nodes[i] == root) { rootId = i; break; }
        }
        f.write(reinterpret_cast<const char*>(&rootId), sizeof(int));
        for (int i = 0; i < nNodes; ++i) {
            Node* n = nodes[i];
            bool leaf = n->leaf;
            f.write(reinterpret_cast<const char*>(&leaf), sizeof(bool));
            int nkeys = n->keys.size();
            f.write(reinterpret_cast<const char*>(&nkeys), sizeof(int));
            for (auto& k : n->keys) {
                uint8_t len = (uint8_t)k.str.size();
                f.write(reinterpret_cast<const char*>(&len), 1);
                f.write(k.str.data(), len);
                f.write(reinterpret_cast<const char*>(&k.val), sizeof(int));
            }
            if (!leaf) {
                int nchild = n->children.size();
                f.write(reinterpret_cast<const char*>(&nchild), sizeof(int));
                for (auto c : n->children) {
                    int cid = -1;
                    for (int j = 0; j < nNodes; ++j) if (nodes[j] == c) { cid = j; break; }
                    f.write(reinterpret_cast<const char*>(&cid), sizeof(int));
                }
            }
        }
    }

    void load(const string& filename) {
        ifstream f(filename, ios::binary);
        if (!f) { root = new Node(true); return; }
        char magic[4];
        f.read(magic, 4);
        if (strncmp(magic, "BPT1", 4) != 0) { root = new Node(true); return; }
        int nNodes, rootId;
        f.read(reinterpret_cast<char*>(&nNodes), sizeof(int));
        f.read(reinterpret_cast<char*>(&rootId), sizeof(int));
        if (nNodes <= 0) { root = new Node(true); return; }
        vector<Node*> nodes(nNodes);
        for (int i = 0; i < nNodes; ++i) nodes[i] = new Node(false);
        for (int i = 0; i < nNodes; ++i) {
            bool leaf;
            f.read(reinterpret_cast<char*>(&leaf), sizeof(bool));
            nodes[i]->leaf = leaf;
            int nkeys;
            f.read(reinterpret_cast<char*>(&nkeys), sizeof(int));
            nodes[i]->keys.resize(nkeys);
            for (int j = 0; j < nkeys; ++j) {
                uint8_t len;
                f.read(reinterpret_cast<char*>(&len), 1);
                string s(len, ' ');
                if (len > 0) f.read(&s[0], len);
                int val;
                f.read(reinterpret_cast<char*>(&val), sizeof(int));
                nodes[i]->keys[j] = {s, val};
            }
            if (!leaf) {
                int nchild;
                f.read(reinterpret_cast<char*>(&nchild), sizeof(int));
                nodes[i]->children.resize(nchild);
                for (int j = 0; j < nchild; ++j) {
                    int cid;
                    f.read(reinterpret_cast<char*>(&cid), sizeof(int));
                    nodes[i]->children[j] = nodes[cid];
                }
            }
        }
        vector<Key> allKeys;
        function<void(Node*)> collectKeys = [&](Node* n) {
            if (n->leaf) {
                for (auto& k : n->keys) allKeys.push_back(k);
            } else {
                for (auto c : n->children) collectKeys(c);
            }
        };
        collectKeys(nodes[rootId]);
        // rebuild fresh tree to guarantee consistent structure and links
        root = new Node(true);
        for (auto& k : allKeys) insert(k);
    }
};

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    BPTree tree;
    tree.load("bpt.db");
    int n;
    if (!(cin >> n)) return 0;
    string cmd;
    while (n--) {
        cin >> cmd;
        if (cmd == "insert") {
            string idx; int val;
            cin >> idx >> val;
            tree.insert({idx, val});
        } else if (cmd == "delete") {
            string idx; int val;
            cin >> idx >> val;
            tree.remove({idx, val});
        } else if (cmd == "find") {
            string idx;
            cin >> idx;
            tree.find(idx, cout);
        }
    }
    tree.save("bpt.db");
    return 0;
}
