#include <array>
#include <cstdint>
#include <iostream>
#include <memory>
#include <random>
#include <string>
#include <stdexcept>
#include <utility>
#include <vector>

using namespace std;

// IPv4 longest-prefix-match table implemented as a path-compressed binary
// prefix trie (radix trie / Patricia-style trie). Each edge stores a bit string.
// Prefix routes may terminate at any node, including the root for /0.
class PatriciaLPM {
public:
    struct Match {
        bool found = false;
        uint32_t network = 0;
        uint32_t mask = 0;
        int prefixLength = -1;
        string route;
    };

    enum class InsertResult {
        Inserted,
        DuplicatePrefix,
        InvalidMask,
        InvalidNetwork
    };

private:
    struct Node {
        string label; // compressed bits on edge from parent; root label is empty
        array<unique_ptr<Node>, 2> child{};
        bool hasRoute = false;
        uint32_t network = 0;
        uint32_t mask = 0;
        int prefixLength = -1;
        string route;

        explicit Node(string edgeBits = "") : label(std::move(edgeBits)) {}
    };

    unique_ptr<Node> root = make_unique<Node>();

    static bool validMask(uint32_t mask) {
        bool zeroSeen = false;
        for (int bit = 31; bit >= 0; --bit) {
            const bool one = ((mask >> bit) & 1U) != 0;
            if (!one) zeroSeen = true;
            else if (zeroSeen) return false;
        }
        return true;
    }

    static int getPrefixLength(uint32_t mask) {
        int length = 0;
        for (int bit = 31; bit >= 0; --bit) {
            if ((mask >> bit) & 1U) ++length;
            else break;
        }
        return length;
    }

    static uint32_t maskForPrefix(int prefix) {
        if (prefix == 0) return 0U;
        return 0xFFFFFFFFU << (32 - prefix);
    }

    static string bitsOf(uint32_t ip, int count) {
        string bits;
        bits.reserve(static_cast<size_t>(count));
        for (int bit = 31; bit >= 32 - count; --bit)
            bits.push_back(((ip >> bit) & 1U) ? '1' : '0');
        return bits;
    }

    static int commonPrefixLength(const string& a, const string& b) {
        size_t i = 0;
        while (i < a.size() && i < b.size() && a[i] == b[i]) ++i;
        return static_cast<int>(i);
    }

    static string ipToString(uint32_t ip) {
        return to_string((ip >> 24) & 255U) + "." +
               to_string((ip >> 16) & 255U) + "." +
               to_string((ip >> 8) & 255U) + "." +
               to_string(ip & 255U);
    }

    static void printNode(const Node* node, int depth) {
        if (!node) return;
        cout << string(static_cast<size_t>(depth * 2), ' ')
             << "edge=\"" << node->label << "\"";
        if (node->hasRoute) {
            cout << " route=\"" << node->route << "\""
                 << " network=" << ipToString(node->network)
                 << " /" << node->prefixLength;
        }
        cout << '\n';
        for (int bit = 0; bit <= 1; ++bit)
            if (node->child[bit]) printNode(node->child[bit].get(), depth + 1);
    }

public:
    // Duplicate policy: first route for a given network/prefix wins.
    InsertResult insert(uint32_t network, uint32_t mask, const string& route) {
        if (!validMask(mask)) return InsertResult::InvalidMask;
        if ((network & mask) != network) return InsertResult::InvalidNetwork;

        const int prefix = getPrefixLength(mask);
        const string key = bitsOf(network, prefix);
        Node* current = root.get();
        size_t pos = 0;

        // /0 has an empty key and is stored directly at the root.
        while (pos < key.size()) {
            const int branch = key[pos] - '0';

            if (!current->child[branch]) {
                auto added = make_unique<Node>(key.substr(pos));
                added->hasRoute = true;
                added->network = network;
                added->mask = mask;
                added->prefixLength = prefix;
                added->route = route;
                current->child[branch] = std::move(added);
                return InsertResult::Inserted;
            }

            Node* next = current->child[branch].get();
            const string remaining = key.substr(pos);
            const int common = commonPrefixLength(next->label, remaining);

            // Entire existing edge matches: descend, or arrive at its endpoint.
            if (common == static_cast<int>(next->label.size())) {
                pos += static_cast<size_t>(common);
                current = next;
                continue;
            }

            // Split the existing edge at the first mismatch or at the new
            // prefix endpoint when the new key ends inside the old edge.
            auto split = make_unique<Node>(next->label.substr(0, static_cast<size_t>(common)));
            string oldSuffix = next->label.substr(static_cast<size_t>(common));
            next->label = oldSuffix;
            const int oldBranch = oldSuffix[0] - '0';
            split->child[oldBranch] = std::move(current->child[branch]);
            current->child[branch] = std::move(split);

            Node* splitNode = current->child[branch].get();
            pos += static_cast<size_t>(common);

            if (pos == key.size()) {
                // New prefix terminates at the split node.
                splitNode->hasRoute = true;
                splitNode->network = network;
                splitNode->mask = mask;
                splitNode->prefixLength = prefix;
                splitNode->route = route;
            } else {
                // New and old keys diverge after the common portion.
                string newSuffix = key.substr(pos);
                const int newBranch = newSuffix[0] - '0';
                auto added = make_unique<Node>(std::move(newSuffix));
                added->hasRoute = true;
                added->network = network;
                added->mask = mask;
                added->prefixLength = prefix;
                added->route = route;
                splitNode->child[newBranch] = std::move(added);
            }
            return InsertResult::Inserted;
        }

        // The route prefix ends at the current node (including /0 at root).
        if (current->hasRoute) return InsertResult::DuplicatePrefix;
        current->hasRoute = true;
        current->network = network;
        current->mask = mask;
        current->prefixLength = prefix;
        current->route = route;
        return InsertResult::Inserted;
    }

    Match lookup(uint32_t destination) const {
        const Node* current = root.get();
        Match best;
        if (current->hasRoute) {
            best = {true, current->network, current->mask,
                    current->prefixLength, current->route};
        }

        const string bits = bitsOf(destination, 32);
        size_t pos = 0;
        while (pos < bits.size()) {
            const int branch = bits[pos] - '0';
            const Node* next = current->child[branch].get();
            if (!next) break;

            const string& label = next->label;
            if (pos + label.size() > bits.size() ||
                bits.compare(pos, label.size(), label) != 0) {
                break;
            }

            pos += label.size();
            current = next;
            if (current->hasRoute) {
                best = {true, current->network, current->mask,
                        current->prefixLength, current->route};
            }
        }
        return best;
    }

    void printTree() const {
        cout << "\n===== Compressed IPv4 Prefix Trie =====\n";
        printNode(root.get(), 0);
    }

    static string formatIP(uint32_t ip) { return ipToString(ip); }
};

static uint32_t makeIP(int a, int b, int c, int d) {
    if (a < 0 || a > 255 || b < 0 || b > 255 ||
        c < 0 || c > 255 || d < 0 || d > 255) {
        throw invalid_argument("IPv4 octets must be in range 0..255");
    }
    return (static_cast<uint32_t>(a) << 24) |
           (static_cast<uint32_t>(b) << 16) |
           (static_cast<uint32_t>(c) << 8) |
            static_cast<uint32_t>(d);
}

// Reference implementation used by randomized self-tests.
struct RefRoute {
    uint32_t network;
    uint32_t mask;
    int prefix;
    string route;
};

static bool referenceLookup(const vector<RefRoute>& routes, uint32_t ip,
                           RefRoute& best) {
    bool found = false;
    int bestPrefix = -1;
    for (const auto& entry : routes) {
        if ((ip & entry.mask) == entry.network && entry.prefix > bestPrefix) {
            best = entry;
            bestPrefix = entry.prefix;
            found = true;
        }
    }
    return found;
}

static bool runSelfTests() {
    // Deterministic edge cases.
    PatriciaLPM trie;
    vector<RefRoute> reference;

    auto add = [&](uint32_t network, int prefix, const string& name) {
        uint32_t mask = prefix == 0 ? 0U : (0xFFFFFFFFU << (32 - prefix));
        auto result = trie.insert(network, mask, name);
        if (result == PatriciaLPM::InsertResult::Inserted)
            reference.push_back({network, mask, prefix, name});
        return result;
    };

    if (add(makeIP(0,0,0,0), 0, "default") != PatriciaLPM::InsertResult::Inserted) return false;
    if (add(makeIP(10,0,0,0), 8, "net10") != PatriciaLPM::InsertResult::Inserted) return false;
    if (add(makeIP(10,1,0,0), 16, "net10-1") != PatriciaLPM::InsertResult::Inserted) return false;
    if (add(makeIP(10,1,2,0), 24, "net10-1-2") != PatriciaLPM::InsertResult::Inserted) return false;
    if (add(makeIP(192,168,1,0), 24, "lan") != PatriciaLPM::InsertResult::Inserted) return false;
    if (add(makeIP(192,168,1,10), 32, "host") != PatriciaLPM::InsertResult::Inserted) return false;

    // Duplicate exact prefix is rejected and does not replace the original.
    if (add(makeIP(10,1,0,0), 16, "duplicate") != PatriciaLPM::InsertResult::DuplicatePrefix) return false;

    // Invalid non-contiguous mask and non-canonical network are rejected.
    if (trie.insert(makeIP(10,0,0,0), makeIP(255,0,255,0), "bad-mask") != PatriciaLPM::InsertResult::InvalidMask) return false;
    if (trie.insert(makeIP(10,1,2,3), makeIP(255,255,255,0), "bad-network") != PatriciaLPM::InsertResult::InvalidNetwork) return false;

    const vector<pair<uint32_t, string>> cases = {
        {makeIP(10,1,2,50), "net10-1-2"},
        {makeIP(10,1,3,50), "net10-1"},
        {makeIP(10,2,3,50), "net10"},
        {makeIP(192,168,1,10), "host"},
        {makeIP(192,168,1,11), "lan"},
        {makeIP(8,8,8,8), "default"}
    };
    for (const auto& test : cases) {
        auto result = trie.lookup(test.first);
        if (!result.found || result.route != test.second) return false;
    }

    // Randomized differential test against exhaustive LPM lookup.
    mt19937_64 rng(0xC0FFEE1234ULL);
    PatriciaLPM randomTrie;
    vector<RefRoute> randomReference;
    for (int i = 0; i < 3000; ++i) {
        uint32_t raw = static_cast<uint32_t>(rng());
        int prefix = static_cast<int>(rng() % 33);
        uint32_t mask = prefix == 0 ? 0U : (0xFFFFFFFFU << (32 - prefix));
        uint32_t network = raw & mask;
        string name = "r" + to_string(i);
        auto status = randomTrie.insert(network, mask, name);
        if (status == PatriciaLPM::InsertResult::Inserted)
            randomReference.push_back({network, mask, prefix, name});
    }

    for (int i = 0; i < 20000; ++i) {
        uint32_t ip = static_cast<uint32_t>(rng());
        RefRoute expected{};
        bool expectedFound = referenceLookup(randomReference, ip, expected);
        auto actual = randomTrie.lookup(ip);
        if (actual.found != expectedFound) return false;
        if (expectedFound && (actual.route != expected.route ||
                              actual.prefixLength != expected.prefix ||
                              actual.network != expected.network)) return false;
    }

    return true;
}

int main() {
    if (!runSelfTests()) {
        cerr << "SELF-TEST FAILED\n";
        return 1;
    }
    cout << "Self-tests passed: boundary cases + 20,000 randomized lookups.\n";

    PatriciaLPM tree;
    tree.insert(makeIP(0,0,0,0), makeIP(0,0,0,0), "Default route");
    tree.insert(makeIP(10,0,0,0), makeIP(255,0,0,0), "Network A /8");
    tree.insert(makeIP(10,1,0,0), makeIP(255,255,0,0), "Network B /16");
    tree.insert(makeIP(10,1,2,0), makeIP(255,255,255,0), "Network C /24");
    tree.insert(makeIP(10,0,0,0), makeIP(255,255,0,0), "Network D /16");
    tree.insert(makeIP(192,168,1,0), makeIP(255,255,255,0), "Network E /24");
    tree.insert(makeIP(192,168,1,10), makeIP(255,255,255,255), "Host F /32");

    tree.printTree();
    cout << "\n===== Lookup Results =====\n";
    for (uint32_t ip : {makeIP(10,1,2,50), makeIP(10,1,3,50),
                        makeIP(10,0,5,10), makeIP(192,168,1,10),
                        makeIP(192,168,1,11), makeIP(8,8,8,8)}) {
        auto match = tree.lookup(ip);
        cout << "Destination: " << PatriciaLPM::formatIP(ip) << '\n';
        if (match.found)
            cout << "Best route: " << match.route << " (/" << match.prefixLength << ")\n";
        else
            cout << "No matching route\n";
    }
    return 0;
}
