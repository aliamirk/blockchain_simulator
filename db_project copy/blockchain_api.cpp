/*
 * Blockchain REST API Server
 *
 * Replaces the ncurses console UI with a JSON HTTP API that your React
 * frontend can call directly.
 *
 * Dependencies (header-only, already in this directory):
 *   httplib.h   — cpp-httplib  (https://github.com/yhirose/cpp-httplib)
 *   json.hpp    — nlohmann/json (https://github.com/nlohmann/json)
 *
 * Also needs MySQL client library (same as original):
 *   brew install mysql-client   # macOS
 *
 * Compile (macOS):
 *   g++ -std=c++17 blockchain_api.cpp -o blockchain_api \
 *       $(mysql_config --cflags --libs) \
 *       -lpthread
 *
 * Run:
 *   ./blockchain_api          # listens on http://localhost:8080
 *   PORT=9000 ./blockchain_api  # custom port
 *
 * DB config (same env vars as before):
 *   DB_HOST, DB_PORT, DB_NAME, DB_USER, DB_PASS
 */

// Disable OpenSSL in cpp-httplib (plain HTTP only)
#undef CPPHTTPLIB_OPENSSL_SUPPORT
#include "httplib.h"
#include "json.hpp"
#include <iostream>
#include <sstream>
#include <string>
#include <ctime>
#include <iomanip>
#include <cstdlib>
#include <mutex>
#include <mysql.h>

using namespace std;
using json = nlohmann::json;

// ==================== DB CONFIG ====================
static const char* DB_HOST_DEFAULT = "127.0.0.1";
static const int   DB_PORT_DEFAULT = 3306;
static const char* DB_NAME_DEFAULT = "blockchain_db";
static const char* DB_USER_DEFAULT = "root";
static const char* DB_PASS_DEFAULT = "p4pacific";

// ==================== UTILITY ====================
int hashString(const string& s) {
    int h = 5381;
    for (size_t i = 0; i < s.length(); i++)
        h = ((h << 5) + h) + (unsigned char)s[i];
    return abs(h);
}

// ==================== TRANSACTION ====================
class Transaction {
public:
    static int nextId;
    int    id;
    string sender;
    string receiver;
    double amount;
    string metadata;
    string signature;

    Transaction() : id(0), amount(0.0) {}

    Transaction(string s, string r, double a, string m = "")
        : sender(s), receiver(r), amount(a), metadata(m) {
        id = nextId++;
        signature = "SIG" + to_string(hashString(sender + receiver + to_string(amount)));
    }

    json toJson() const {
        return {
            {"id",        id},
            {"sender",    sender},
            {"receiver",  receiver},
            {"amount",    amount},
            {"metadata",  metadata},
            {"signature", signature}
        };
    }
};
int Transaction::nextId = 1;

// ==================== BST ====================
struct BSTNode {
    int txId, blockIndex, txIndex;
    BSTNode *left, *right;
    BSTNode(int id, int bIdx, int tIdx)
        : txId(id), blockIndex(bIdx), txIndex(tIdx), left(nullptr), right(nullptr) {}
};

class BSTTransactionIndex {
    BSTNode* root;
    BSTNode* insert(BSTNode* n, int txId, int bi, int ti) {
        if (!n) return new BSTNode(txId, bi, ti);
        if (txId < n->txId) n->left  = insert(n->left,  txId, bi, ti);
        else if (txId > n->txId) n->right = insert(n->right, txId, bi, ti);
        return n;
    }
    BSTNode* search(BSTNode* n, int txId) {
        if (!n || n->txId == txId) return n;
        return txId < n->txId ? search(n->left, txId) : search(n->right, txId);
    }
    void del(BSTNode* n) { if (!n) return; del(n->left); del(n->right); delete n; }
public:
    BSTTransactionIndex() : root(nullptr) {}
    ~BSTTransactionIndex() { del(root); }
    void insert(int txId, int bi, int ti) { root = insert(root, txId, bi, ti); }
    bool search(int txId, int& bi, int& ti) {
        BSTNode* r = search(root, txId);
        if (r) { bi = r->blockIndex; ti = r->txIndex; return true; }
        return false;
    }
    void clear() { del(root); root = nullptr; }
};

// ==================== PRIORITY QUEUE ====================
class PriorityQueueTransaction {
    Transaction** heap;
    int capacity, size;
    void up(int i) {
        while (i > 0) {
            int p = (i-1)/2;
            if (heap[i]->amount > heap[p]->amount) { swap(heap[i], heap[p]); i = p; }
            else break;
        }
    }
    void down(int i) {
        while (true) {
            int largest = i, l = 2*i+1, r = 2*i+2;
            if (l < size && heap[l]->amount > heap[largest]->amount) largest = l;
            if (r < size && heap[r]->amount > heap[largest]->amount) largest = r;
            if (largest != i) { swap(heap[i], heap[largest]); i = largest; }
            else break;
        }
    }
public:
    PriorityQueueTransaction(int cap = 100) : capacity(cap), size(0) {
        heap = new Transaction*[capacity];
    }
    ~PriorityQueueTransaction() {
        for (int i = 0; i < size; i++) delete heap[i];
        delete[] heap;
    }
    void enqueue(Transaction* tx) {
        if (size >= capacity) return;
        heap[size] = tx; up(size++);
    }
    Transaction* dequeue() {
        if (!size) return nullptr;
        Transaction* tx = heap[0]; heap[0] = heap[--size]; down(0); return tx;
    }
    bool isEmpty() const { return size == 0; }
    int  getSize() const { return size; }
    // Returns a snapshot array (caller must delete[])
    void getAll(Transaction**& txs, int& count) {
        count = size;
        txs = new Transaction*[count];
        // Sort by amount descending for display
        Transaction** sorted = new Transaction*[size];
        for (int i = 0; i < size; i++) sorted[i] = heap[i];
        for (int i = 0; i < size-1; i++)
            for (int j = i+1; j < size; j++)
                if (sorted[j]->amount > sorted[i]->amount) swap(sorted[i], sorted[j]);
        for (int i = 0; i < count; i++) txs[i] = sorted[i];
        delete[] sorted;
    }
    json toJson() const {
        json arr = json::array();
        Transaction** sorted = new Transaction*[size];
        for (int i = 0; i < size; i++) sorted[i] = heap[i];
        for (int i = 0; i < size-1; i++)
            for (int j = i+1; j < size; j++)
                if (sorted[j]->amount > sorted[i]->amount) swap(sorted[i], sorted[j]);
        for (int i = 0; i < size; i++) arr.push_back(sorted[i]->toJson());
        delete[] sorted;
        return arr;
    }
};

// ==================== BLOCK ====================
class Block {
public:
    int    index, nonce, prevHash, hash;
    long   timestamp;
    Transaction* transactions;
    int txCount;

    Block() : index(0), nonce(0), prevHash(0), hash(0), timestamp(0),
              transactions(nullptr), txCount(0) {}

    Block(int idx, int prevH, Transaction* txs, int count)
        : index(idx), prevHash(prevH), txCount(count), transactions(nullptr) {
        timestamp = time(nullptr);
        if (count > 0 && txs) {
            transactions = new Transaction[count];
            for (int i = 0; i < count; i++) transactions[i] = txs[i];
        }
        nonce = 0; hash = calculateHash();
    }

    ~Block() { delete[] transactions; }

    void mineBlock(int difficulty) {
        if (difficulty > 5) difficulty = 5;
        int target = 1;
        for (int i = 0; i < difficulty; i++) target *= 10;
        while (abs(hash) % target != 0) { nonce++; hash = calculateHash(); }
    }

    json toJson() const {
        char tbuf[80]; struct tm* ti = localtime(&timestamp);
        strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S", ti);
        json txArr = json::array();
        for (int i = 0; i < txCount; i++) txArr.push_back(transactions[i].toJson());
        return {
            {"index",        index},
            {"hash",         hash},
            {"prevHash",     prevHash},
            {"timestamp",    tbuf},
            {"nonce",        nonce},
            {"transactions", txArr}
        };
    }

private:
    int calculateHash() {
        stringstream ss;
        ss << index << timestamp << prevHash << nonce;
        for (int i = 0; i < txCount; i++)
            ss << transactions[i].id << transactions[i].sender
               << transactions[i].receiver << transactions[i].amount;
        return hashString(ss.str());
    }
};

// ==================== LINKED LIST ====================
struct NodeBlock { Block* block; NodeBlock* next; NodeBlock(Block* b): block(b), next(nullptr){} };

class LinkedListBlock {
    NodeBlock* head; int sz;
public:
    LinkedListBlock() : head(nullptr), sz(0) {}
    ~LinkedListBlock() {
        NodeBlock* c = head;
        while (c) { NodeBlock* n = c->next; delete c->block; delete c; c = n; }
    }
    // Move constructor
    LinkedListBlock(LinkedListBlock&& o) : head(o.head), sz(o.sz) { o.head = nullptr; o.sz = 0; }
    LinkedListBlock& operator=(LinkedListBlock&& o) {
        if (this != &o) {
            NodeBlock* c = head;
            while (c) { NodeBlock* n = c->next; delete c->block; delete c; c = n; }
            head = o.head; sz = o.sz; o.head = nullptr; o.sz = 0;
        }
        return *this;
    }
    void clear() {
        NodeBlock* c = head;
        while (c) { NodeBlock* n = c->next; delete c->block; delete c; c = n; }
        head = nullptr; sz = 0;
    }
    void append(Block* b) {
        NodeBlock* nn = new NodeBlock(b);
        if (!head) { head = nn; } else {
            NodeBlock* t = head; while (t->next) t = t->next; t->next = nn;
        }
        sz++;
    }
    Block* get(int i) {
        if (i < 0 || i >= sz) return nullptr;
        NodeBlock* t = head; for (int j = 0; j < i; j++) t = t->next; return t->block;
    }
    int getSize() const { return sz; }
    NodeBlock* getHead() { return head; }
};

// ==================== HASH TABLE FOR BALANCES ====================
struct NodeHash { string key; double value; NodeHash* next;
    NodeHash(string k, double v): key(k), value(v), next(nullptr){} };

class HashTableBalance {
    static const int SZ = 100;
    NodeHash* table[SZ];
    int hf(const string& k) {
        int h = 5381;
        for (size_t i = 0; i < k.size(); i++) h = ((h<<5)+h)+(unsigned char)k[i];
        return abs(h) % SZ;
    }
public:
    HashTableBalance() { for (int i = 0; i < SZ; i++) table[i] = nullptr; }
    ~HashTableBalance() {
        for (int i = 0; i < SZ; i++) {
            NodeHash* c = table[i]; while (c) { NodeHash* n = c->next; delete c; c = n; }
        }
    }
    double get(const string& k) {
        NodeHash* c = table[hf(k)]; while (c) { if (c->key==k) return c->value; c=c->next; } return 0.0;
    }
    bool contains(const string& k) {
        NodeHash* c = table[hf(k)]; while (c) { if (c->key==k) return true; c=c->next; } return false;
    }
    void update(const string& k, double v) {
        int i = hf(k); NodeHash* c = table[i];
        while (c) { if (c->key==k) { c->value=v; return; } c=c->next; }
        NodeHash* nn = new NodeHash(k,v); nn->next = table[i]; table[i] = nn;
    }
    bool hasSufficientBalance(const string& k, double a) { return get(k) >= a; }
    json toJson() const {
        json obj = json::object();
        for (int i = 0; i < SZ; i++) {
            NodeHash* c = table[i];
            while (c) { obj[c->key] = c->value; c = c->next; }
        }
        return obj;
    }
    void forEach(void(*fn)(const string&, double, void*), void* ctx) {
        for (int i = 0; i < SZ; i++) {
            NodeHash* c = table[i]; while (c) { fn(c->key, c->value, ctx); c=c->next; }
        }
    }
};

// ============================================================================
// MySQL DATABASE LAYER
// ============================================================================
struct DbConfig {
    string host; int port; string dbname; string user; string pass;
    DbConfig() {
        auto env = [](const char* var, const char* def) -> string {
            const char* v = getenv(var); return v ? string(v) : string(def);
        };
        host   = env("DB_HOST", DB_HOST_DEFAULT);
        port   = [&]{ const char* v = getenv("DB_PORT"); return v ? atoi(v) : DB_PORT_DEFAULT; }();
        dbname = env("DB_NAME", DB_NAME_DEFAULT);
        user   = env("DB_USER", DB_USER_DEFAULT);
        pass   = env("DB_PASS", DB_PASS_DEFAULT);
    }
};

class MySQLDB {
    MYSQL* conn;
    DbConfig cfg;

    string esc(const string& s) {
        string out(s.size()*2+1, '\0');
        unsigned long len = mysql_real_escape_string(conn, &out[0], s.c_str(), s.size());
        out.resize(len); return out;
    }
    void exec(const string& sql) {
        if (mysql_query(conn, sql.c_str()))
            throw runtime_error(string("MySQL error: ") + mysql_error(conn) + "\nSQL: " + sql);
    }
    MYSQL_RES* query(const string& sql) {
        if (mysql_query(conn, sql.c_str()))
            throw runtime_error(string("MySQL error: ") + mysql_error(conn) + "\nSQL: " + sql);
        return mysql_store_result(conn);
    }
    long long lastInsertId() { return (long long)mysql_insert_id(conn); }

public:
    MySQLDB() : conn(nullptr) {}
    ~MySQLDB() { disconnect(); }

    bool connect(const DbConfig& config) {
        cfg = config;
        conn = mysql_init(nullptr);
        if (!conn) return false;
        unsigned int timeout = 10;
        mysql_options(conn, MYSQL_OPT_CONNECT_TIMEOUT, &timeout);
        if (!mysql_real_connect(conn, cfg.host.c_str(), cfg.user.c_str(), cfg.pass.c_str(),
                cfg.dbname.c_str(), cfg.port, nullptr, 0)) {
            mysql_close(conn); conn = nullptr; return false;
        }
        mysql_set_character_set(conn, "utf8mb4");
        return true;
    }
    void disconnect() { if (conn) { mysql_close(conn); conn = nullptr; } }
    bool isConnected() const { return conn != nullptr; }

    void auditLog(const string& op, const string& chainName, const string& details) {
        exec("INSERT INTO AUDIT_LOG (OPERATION, CHAIN_NAME, DETAILS) VALUES ('"
             + esc(op) + "','" + esc(chainName) + "','" + esc(details) + "')");
    }

    void upsertAccount(const string& name, double balance) {
        exec("INSERT INTO ACCOUNTS (NAME, BALANCE) VALUES ('" + esc(name) + "'," +
             to_string(balance) + ") ON DUPLICATE KEY UPDATE BALANCE=" + to_string(balance));
    }

    void loadAccounts(HashTableBalance& ht) {
        MYSQL_RES* res = query("SELECT NAME, BALANCE FROM ACCOUNTS");
        MYSQL_ROW row;
        while ((row = mysql_fetch_row(res))) ht.update(string(row[0]), atof(row[1]));
        mysql_free_result(res);
    }

    void saveAccounts(HashTableBalance& ht) {
        struct Ctx { MySQLDB* db; };
        Ctx ctx{this};
        ht.forEach([](const string& k, double v, void* c){
            static_cast<Ctx*>(c)->db->upsertAccount(k, v);
        }, &ctx);
    }

    int getChainId(const string& name) {
        MYSQL_RES* res = query("SELECT CHAIN_ID FROM BLOCKCHAIN_META WHERE NAME='" + esc(name) + "'");
        MYSQL_ROW row = mysql_fetch_row(res);
        int id = row ? atoi(row[0]) : -1;
        mysql_free_result(res); return id;
    }

    int upsertChainMeta(const string& name, int difficulty, int nextTxId) {
        int id = getChainId(name);
        if (id < 0) {
            exec("INSERT INTO BLOCKCHAIN_META (NAME, DIFFICULTY, NEXT_TX_ID) VALUES ('"
                 + esc(name) + "'," + to_string(difficulty) + "," + to_string(nextTxId) + ")");
            id = (int)lastInsertId();
            exec("INSERT INTO CHAIN_STATISTICS (CHAIN_ID) VALUES (" + to_string(id) + ")");
        } else {
            exec("UPDATE BLOCKCHAIN_META SET DIFFICULTY=" + to_string(difficulty) +
                 ", NEXT_TX_ID=" + to_string(nextTxId) + " WHERE CHAIN_ID=" + to_string(id));
        }
        return id;
    }

    bool saveBlockchain(const string& chainName, LinkedListBlock& chain,
                        PriorityQueueTransaction& pool, HashTableBalance& balances, int difficulty) {
        if (!conn) return false;
        try {
            exec("START TRANSACTION");
            int chainId = upsertChainMeta(chainName, difficulty, Transaction::nextId);

            MYSQL_RES* mr = query("SELECT MINER_ID FROM MINERS WHERE NAME='SYSTEM'");
            MYSQL_ROW mrow = mysql_fetch_row(mr);
            int minerId = mrow ? atoi(mrow[0]) : 1;
            mysql_free_result(mr);

            MYSQL_RES* tr = query("SELECT TYPE_ID FROM TRANSACTION_TYPES WHERE TYPE_NAME='transfer'");
            MYSQL_ROW trow = mysql_fetch_row(tr);
            int typeId = trow ? atoi(trow[0]) : 1;
            mysql_free_result(tr);

            exec("DELETE FROM BLOCKS WHERE CHAIN_ID=" + to_string(chainId));

            int totalTx = 0;
            for (int i = 0; i < chain.getSize(); i++) {
                Block* blk = chain.get(i);
                exec("INSERT INTO BLOCKS (CHAIN_ID,MINER_ID,BLOCK_INDEX,BLOCK_TIMESTAMP,NONCE,HASH,PREV_HASH) VALUES ("
                     + to_string(chainId) + "," + to_string(minerId) + ","
                     + to_string(blk->index) + "," + to_string(blk->timestamp) + ","
                     + to_string(blk->nonce) + "," + to_string(blk->hash) + ","
                     + to_string(blk->prevHash) + ")");
                long long blockDbId = lastInsertId();
                for (int j = 0; j < blk->txCount; j++) {
                    Transaction& tx = blk->transactions[j];
                    exec("INSERT INTO TRANSACTIONS (BLOCK_ID,TYPE_ID,TX_ID,SENDER,RECEIVER,AMOUNT,METADATA,SIGNATURE) VALUES ("
                         + to_string(blockDbId) + "," + to_string(typeId) + ","
                         + to_string(tx.id) + ",'" + esc(tx.sender) + "','"
                         + esc(tx.receiver) + "'," + to_string(tx.amount) + ",'"
                         + esc(tx.metadata) + "','" + esc(tx.signature) + "')");
                    totalTx++;
                }
            }

            exec("DELETE FROM PENDING_TRANSACTIONS WHERE TYPE_ID=" + to_string(typeId));
            Transaction** txs; int cnt;
            pool.getAll(txs, cnt);
            for (int i = 0; i < cnt; i++) {
                Transaction* tx = txs[i];
                exec("INSERT INTO PENDING_TRANSACTIONS (TYPE_ID,SENDER,RECEIVER,AMOUNT,METADATA,SIGNATURE) VALUES ("
                     + to_string(typeId) + ",'" + esc(tx->sender) + "','"
                     + esc(tx->receiver) + "'," + to_string(tx->amount) + ",'"
                     + esc(tx->metadata) + "','" + esc(tx->signature) + "')");
            }
            delete[] txs;

            saveAccounts(balances);
            exec("UPDATE CHAIN_STATISTICS SET TOTAL_BLOCKS=" + to_string(chain.getSize()) +
                 ", TOTAL_TRANSACTIONS=" + to_string(totalTx) +
                 ", TOTAL_PENDING=" + to_string(pool.getSize()) +
                 " WHERE CHAIN_ID=" + to_string(chainId));
            exec("COMMIT");
            auditLog("SAVE", chainName, "blocks=" + to_string(chain.getSize()) + " tx=" + to_string(totalTx));
            return true;
        } catch (const exception& e) {
            exec("ROLLBACK");
            cerr << e.what() << endl;
            return false;
        }
    }

    bool loadBlockchain(const string& chainName, LinkedListBlock& chain,
                        PriorityQueueTransaction& pool, HashTableBalance& balances,
                        BSTTransactionIndex& bst, int& difficulty) {
        if (!conn) return false;
        try {
            MYSQL_RES* mr = query("SELECT CHAIN_ID, DIFFICULTY, NEXT_TX_ID FROM BLOCKCHAIN_META WHERE NAME='" + esc(chainName) + "'");
            MYSQL_ROW mrow = mysql_fetch_row(mr);
            if (!mrow) { mysql_free_result(mr); return false; }
            int chainId = atoi(mrow[0]);
            difficulty  = atoi(mrow[1]);
            Transaction::nextId = atoi(mrow[2]);
            mysql_free_result(mr);

            MYSQL_RES* br = query("SELECT BLOCK_ID, BLOCK_INDEX, BLOCK_TIMESTAMP, NONCE, HASH, PREV_HASH "
                                  "FROM BLOCKS WHERE CHAIN_ID=" + to_string(chainId) + " ORDER BY BLOCK_INDEX");
            MYSQL_ROW brow;
            int blockPos = 0;
            while ((brow = mysql_fetch_row(br))) {
                int blockDbId = atoi(brow[0]);
                MYSQL_RES* txr = query("SELECT TX_ID, SENDER, RECEIVER, AMOUNT, METADATA, SIGNATURE "
                                       "FROM TRANSACTIONS WHERE BLOCK_ID=" + to_string(blockDbId) + " ORDER BY TRANSACTION_ID");
                int txCnt = (int)mysql_num_rows(txr);
                Transaction* txArr = txCnt > 0 ? new Transaction[txCnt] : nullptr;
                MYSQL_ROW txrow; int ti = 0;
                while ((txrow = mysql_fetch_row(txr))) {
                    txArr[ti].id        = atoi(txrow[0]);
                    txArr[ti].sender    = txrow[1];
                    txArr[ti].receiver  = txrow[2];
                    txArr[ti].amount    = atof(txrow[3]);
                    txArr[ti].metadata  = txrow[4] ? txrow[4] : "";
                    txArr[ti].signature = txrow[5] ? txrow[5] : "";
                    bst.insert(txArr[ti].id, blockPos, ti);
                    ti++;
                }
                mysql_free_result(txr);

                Block* blk = new Block();
                blk->index     = atoi(brow[1]);
                blk->timestamp = atol(brow[2]);
                blk->nonce     = atoi(brow[3]);
                blk->hash      = atoi(brow[4]);
                blk->prevHash  = atoi(brow[5]);
                blk->txCount   = txCnt;
                blk->transactions = txArr;
                chain.append(blk);
                blockPos++;
            }
            mysql_free_result(br);

            MYSQL_RES* pr = query("SELECT SENDER, RECEIVER, AMOUNT, METADATA, SIGNATURE FROM PENDING_TRANSACTIONS ORDER BY PENDING_TX_ID");
            MYSQL_ROW prow;
            while ((prow = mysql_fetch_row(pr))) {
                Transaction* tx = new Transaction(prow[0], prow[1], atof(prow[2]), prow[3] ? prow[3] : "");
                if (prow[4]) tx->signature = prow[4];
                pool.enqueue(tx);
            }
            mysql_free_result(pr);

            loadAccounts(balances);
            auditLog("LOAD", chainName, "blocks=" + to_string(blockPos));
            return true;
        } catch (const exception& e) {
            cerr << e.what() << endl;
            return false;
        }
    }

    json listChains() {
        MYSQL_RES* res = query(
            "SELECT m.NAME, m.DIFFICULTY, COALESCE(s.TOTAL_BLOCKS,0), "
            "COALESCE(s.TOTAL_TRANSACTIONS,0), m.CREATED_AT "
            "FROM BLOCKCHAIN_META m "
            "LEFT JOIN CHAIN_STATISTICS s ON s.CHAIN_ID = m.CHAIN_ID "
            "ORDER BY m.CHAIN_ID");
        json arr = json::array();
        MYSQL_ROW row;
        while ((row = mysql_fetch_row(res))) {
            arr.push_back({
                {"name",         row[0]},
                {"difficulty",   atoi(row[1])},
                {"totalBlocks",  atoi(row[2])},
                {"totalTx",      atoi(row[3])},
                {"createdAt",    row[4]}
            });
        }
        mysql_free_result(res);
        return arr;
    }
};

// ============================================================================
// BLOCKCHAIN CLASS
// ============================================================================
class Blockchain {
public:
    LinkedListBlock chain;
    int difficulty;

    Blockchain(int diff) : difficulty(min(diff, 5)) {
        Block* genesis = new Block(0, 0, nullptr, 0);
        genesis->hash = 0;
        chain.append(genesis);
    }

    Block* getLatestBlock() { return chain.get(chain.getSize()-1); }

    void addBlock(Transaction* txs, int count) {
        Block* blk = new Block(chain.getSize(), getLatestBlock()->hash, txs, count);
        blk->mineBlock(difficulty);
        chain.append(blk);
    }

    bool isChainValid() {
        NodeBlock* cur  = chain.getHead() ? chain.getHead()->next : nullptr;
        NodeBlock* prev = chain.getHead();
        while (cur) {
            if (cur->block->prevHash != prev->block->hash) return false;
            prev = cur; cur = cur->next;
        }
        return true;
    }

    void rebuildBSTIndex(BSTTransactionIndex* bst) {
        bst->clear();
        for (int i = 0; i < chain.getSize(); i++) {
            Block* blk = chain.get(i);
            for (int j = 0; j < blk->txCount; j++)
                bst->insert(blk->transactions[j].id, i, j);
        }
    }

    json summaryJson(const string& chainName, int pendingCount) {
        int total = 0;
        for (int i = 0; i < chain.getSize(); i++) total += chain.get(i)->txCount;
        return {
            {"chainName",        chainName},
            {"totalBlocks",      chain.getSize()},
            {"difficulty",       difficulty},
            {"latestHash",       getLatestBlock()->hash},
            {"totalTransactions",total},
            {"pendingCount",     pendingCount}
        };
    }
};

// ============================================================================
// GLOBAL STATE  (protected by a mutex — one blockchain active at a time)
// ============================================================================
struct AppState {
    mutex              mtx;
    Blockchain*              bc       = nullptr;
    PriorityQueueTransaction* pool    = nullptr;
    HashTableBalance*         balances = nullptr;
    BSTTransactionIndex*      bst     = nullptr;
    string                    chainName;
    MySQLDB                   db;

    void reset(int diff = 3) {
        delete bc; delete pool; delete balances; delete bst;
        bc       = new Blockchain(diff);
        pool     = new PriorityQueueTransaction();
        balances = new HashTableBalance();
        bst      = new BSTTransactionIndex();
    }

    void initDefaultBalances() {
        balances->update("Alice",   1000.0);
        balances->update("Bob",     1000.0);
        balances->update("Charlie", 1000.0);
        balances->update("David",    500.0);
        balances->update("Eve",      750.0);
    }

    bool loaded() const { return bc != nullptr; }

    ~AppState() { delete bc; delete pool; delete balances; delete bst; }
};

// ============================================================================
// HELPERS
// ============================================================================
static void setCORS(httplib::Response& res) {
    res.set_header("Access-Control-Allow-Origin",  "*");
    res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    res.set_header("Access-Control-Allow-Headers", "Content-Type");
}

static void sendJson(httplib::Response& res, int status, const json& body) {
    setCORS(res);
    res.status = status;
    res.set_content(body.dump(), "application/json");
}

static void ok(httplib::Response& res, const json& body) { sendJson(res, 200, body); }
static void err(httplib::Response& res, int status, const string& msg) {
    sendJson(res, status, {{"error", msg}});
}

// ============================================================================
// MAIN — register routes and start server
// ============================================================================
int main() {
    DbConfig cfg;
    AppState state;

    cout << "Connecting to MySQL at " << cfg.host << ":" << cfg.port << "/" << cfg.dbname << " ...\n";
    if (!state.db.connect(cfg)) {
        cerr << "Failed to connect to MySQL. Check DB_HOST/DB_PORT/DB_NAME/DB_USER/DB_PASS env vars.\n";
        return 1;
    }
    cout << "MySQL connected.\n";

    httplib::Server svr;

    // ── CORS preflight ────────────────────────────────────────────────────────
    svr.Options(".*", [](const httplib::Request&, httplib::Response& res) {
        setCORS(res);
        res.status = 204;
    });

    // ── POST /api/blockchain/create ───────────────────────────────────────────
    // Body: { "name": "mychain", "difficulty": 3 }
    svr.Post("/api/blockchain/create", [&](const httplib::Request& req, httplib::Response& res) {
        lock_guard<mutex> lock(state.mtx);
        try {
            auto body = json::parse(req.body);
            string name = body.value("name", "default");
            int diff    = body.value("difficulty", 3);
            if (diff < 1 || diff > 5) diff = 3;

            state.reset(diff);
            state.chainName = name;
            state.initDefaultBalances();

            ok(res, {
                {"success", true},
                {"message", "Blockchain '" + name + "' created"},
                {"chainName", name},
                {"difficulty", diff},
                {"defaultAccounts", {"Alice","Bob","Charlie","David","Eve"}}
            });
        } catch (const exception& e) {
            err(res, 400, e.what());
        }
    });

    // ── POST /api/blockchain/load ─────────────────────────────────────────────
    // Body: { "name": "mychain" }
    svr.Post("/api/blockchain/load", [&](const httplib::Request& req, httplib::Response& res) {
        lock_guard<mutex> lock(state.mtx);
        try {
            auto body = json::parse(req.body);
            string name = body.value("name", "");
            if (name.empty()) { err(res, 400, "name is required"); return; }

            state.reset(3);
            // Clear genesis block so loadBlockchain can rebuild from DB
            state.bc->chain.clear();

            if (state.db.loadBlockchain(name, state.bc->chain, *state.pool,
                                        *state.balances, *state.bst, state.bc->difficulty)) {
                state.chainName = name;
                ok(res, {
                    {"success",   true},
                    {"message",   "Blockchain '" + name + "' loaded"},
                    {"chainName", name},
                    {"difficulty",state.bc->difficulty},
                    {"blocks",    state.bc->chain.getSize()}
                });
            } else {
                err(res, 404, "Chain '" + name + "' not found in database");
            }
        } catch (const exception& e) {
            err(res, 500, e.what());
        }
    });

    // ── POST /api/blockchain/save ─────────────────────────────────────────────
    // Body: { "name": "mychain" }  (optional — uses current name if omitted)
    svr.Post("/api/blockchain/save", [&](const httplib::Request& req, httplib::Response& res) {
        lock_guard<mutex> lock(state.mtx);
        if (!state.loaded()) { err(res, 400, "No blockchain loaded"); return; }
        try {
            auto body = json::parse(req.body);
            string name = body.value("name", state.chainName);
            if (name.empty()) { err(res, 400, "name is required"); return; }
            state.chainName = name;

            if (state.db.saveBlockchain(name, state.bc->chain, *state.pool,
                                        *state.balances, state.bc->difficulty)) {
                ok(res, {{"success", true}, {"message", "Saved as '" + name + "'"}});
            } else {
                err(res, 500, "Save failed");
            }
        } catch (const exception& e) {
            err(res, 500, e.what());
        }
    });

    // ── GET /api/blockchain/chains ────────────────────────────────────────────
    svr.Get("/api/blockchain/chains", [&](const httplib::Request&, httplib::Response& res) {
        lock_guard<mutex> lock(state.mtx);
        try {
            ok(res, {{"chains", state.db.listChains()}});
        } catch (const exception& e) {
            err(res, 500, e.what());
        }
    });

    // ── GET /api/blockchain/summary ───────────────────────────────────────────
    svr.Get("/api/blockchain/summary", [&](const httplib::Request&, httplib::Response& res) {
        lock_guard<mutex> lock(state.mtx);
        if (!state.loaded()) { err(res, 400, "No blockchain loaded"); return; }
        ok(res, state.bc->summaryJson(state.chainName, state.pool->getSize()));
    });

    // ── GET /api/blockchain/blocks ────────────────────────────────────────────
    svr.Get("/api/blockchain/blocks", [&](const httplib::Request&, httplib::Response& res) {
        lock_guard<mutex> lock(state.mtx);
        if (!state.loaded()) { err(res, 400, "No blockchain loaded"); return; }
        json arr = json::array();
        for (int i = 0; i < state.bc->chain.getSize(); i++)
            arr.push_back(state.bc->chain.get(i)->toJson());
        ok(res, {{"blocks", arr}, {"total", state.bc->chain.getSize()}});
    });

    // ── GET /api/blockchain/blocks/:index ─────────────────────────────────────
    svr.Get(R"(/api/blockchain/blocks/(\d+))", [&](const httplib::Request& req, httplib::Response& res) {
        lock_guard<mutex> lock(state.mtx);
        if (!state.loaded()) { err(res, 400, "No blockchain loaded"); return; }
        int idx = stoi(req.matches[1]);
        Block* blk = state.bc->chain.get(idx);
        if (!blk) { err(res, 404, "Block not found"); return; }
        ok(res, blk->toJson());
    });

    // ── GET /api/blockchain/verify ────────────────────────────────────────────
    svr.Get("/api/blockchain/verify", [&](const httplib::Request&, httplib::Response& res) {
        lock_guard<mutex> lock(state.mtx);
        if (!state.loaded()) { err(res, 400, "No blockchain loaded"); return; }
        bool valid = state.bc->isChainValid();
        ok(res, {{"valid", valid}, {"message", valid ? "Blockchain is valid" : "Blockchain integrity check failed"}});
    });

    // ── GET /api/accounts ─────────────────────────────────────────────────────
    svr.Get("/api/accounts", [&](const httplib::Request&, httplib::Response& res) {
        lock_guard<mutex> lock(state.mtx);
        if (!state.loaded()) { err(res, 400, "No blockchain loaded"); return; }
        ok(res, {{"accounts", state.balances->toJson()}});
    });

    // ── POST /api/transactions ────────────────────────────────────────────────
    // Body: { "sender": "Alice", "receiver": "Bob", "amount": 50.0, "metadata": "optional" }
    svr.Post("/api/transactions", [&](const httplib::Request& req, httplib::Response& res) {
        lock_guard<mutex> lock(state.mtx);
        if (!state.loaded()) { err(res, 400, "No blockchain loaded"); return; }
        try {
            auto body     = json::parse(req.body);
            string sender   = body.value("sender",   "");
            string receiver = body.value("receiver", "");
            double amount   = body.value("amount",   0.0);
            string meta     = body.value("metadata", "");

            if (sender.empty() || receiver.empty()) { err(res, 400, "sender and receiver are required"); return; }
            if (amount <= 0)                         { err(res, 400, "amount must be positive"); return; }
            if (!state.balances->contains(sender))   { err(res, 400, "Sender account does not exist"); return; }
            if (!state.balances->hasSufficientBalance(sender, amount)) {
                err(res, 400, "Insufficient balance. Available: " + to_string(state.balances->get(sender)));
                return;
            }

            Transaction* tx = new Transaction(sender, receiver, amount, meta);
            state.pool->enqueue(tx);
            ok(res, {{"success", true}, {"transaction", tx->toJson()}, {"pendingCount", state.pool->getSize()}});
        } catch (const exception& e) {
            err(res, 400, e.what());
        }
    });

    // ── GET /api/transactions/pending ─────────────────────────────────────────
    svr.Get("/api/transactions/pending", [&](const httplib::Request&, httplib::Response& res) {
        lock_guard<mutex> lock(state.mtx);
        if (!state.loaded()) { err(res, 400, "No blockchain loaded"); return; }
        ok(res, {{"pending", state.pool->toJson()}, {"count", state.pool->getSize()}});
    });

    // ── POST /api/mine ────────────────────────────────────────────────────────
    svr.Post("/api/mine", [&](const httplib::Request&, httplib::Response& res) {
        lock_guard<mutex> lock(state.mtx);
        if (!state.loaded())        { err(res, 400, "No blockchain loaded"); return; }
        if (state.pool->isEmpty())  { err(res, 400, "No pending transactions to mine"); return; }

        Transaction** txs; int count;
        state.pool->getAll(txs, count);

        Transaction* txArr = new Transaction[count];
        for (int i = 0; i < count; i++) txArr[i] = *txs[i];

        state.bc->addBlock(txArr, count);
        delete[] txArr;

        // Update balances and drain pool
        for (int i = 0; i < count; i++) {
            state.balances->update(txs[i]->sender,
                state.balances->get(txs[i]->sender) - txs[i]->amount);
            state.balances->update(txs[i]->receiver,
                state.balances->get(txs[i]->receiver) + txs[i]->amount);
            state.pool->dequeue();
        }
        delete[] txs;

        state.bc->rebuildBSTIndex(state.bst);

        Block* newBlk = state.bc->getLatestBlock();
        ok(res, {
            {"success",     true},
            {"message",     "Block mined successfully"},
            {"block",       newBlk->toJson()},
            {"totalBlocks", state.bc->chain.getSize()}
        });
    });

    // ── GET /api/search/sender/:name ──────────────────────────────────────────
    svr.Get(R"(/api/search/sender/(.+))", [&](const httplib::Request& req, httplib::Response& res) {
        lock_guard<mutex> lock(state.mtx);
        if (!state.loaded()) { err(res, 400, "No blockchain loaded"); return; }
        string name = req.matches[1];
        json results = json::array();
        for (int i = 0; i < state.bc->chain.getSize(); i++) {
            Block* blk = state.bc->chain.get(i);
            for (int j = 0; j < blk->txCount; j++) {
                if (blk->transactions[j].sender == name) {
                    json entry = blk->transactions[j].toJson();
                    entry["blockIndex"] = i;
                    results.push_back(entry);
                }
            }
        }
        ok(res, {{"results", results}, {"count", results.size()}});
    });

    // ── GET /api/search/receiver/:name ────────────────────────────────────────
    svr.Get(R"(/api/search/receiver/(.+))", [&](const httplib::Request& req, httplib::Response& res) {
        lock_guard<mutex> lock(state.mtx);
        if (!state.loaded()) { err(res, 400, "No blockchain loaded"); return; }
        string name = req.matches[1];
        json results = json::array();
        for (int i = 0; i < state.bc->chain.getSize(); i++) {
            Block* blk = state.bc->chain.get(i);
            for (int j = 0; j < blk->txCount; j++) {
                if (blk->transactions[j].receiver == name) {
                    json entry = blk->transactions[j].toJson();
                    entry["blockIndex"] = i;
                    results.push_back(entry);
                }
            }
        }
        ok(res, {{"results", results}, {"count", results.size()}});
    });

    // ── GET /api/search/tx/:id ────────────────────────────────────────────────
    svr.Get(R"(/api/search/tx/(\d+))", [&](const httplib::Request& req, httplib::Response& res) {
        lock_guard<mutex> lock(state.mtx);
        if (!state.loaded()) { err(res, 400, "No blockchain loaded"); return; }
        int txId = stoi(req.matches[1]);
        int bi = -1, ti = -1;
        if (state.bst->search(txId, bi, ti)) {
            Block* blk = state.bc->chain.get(bi);
            if (blk && ti < blk->txCount) {
                json entry = blk->transactions[ti].toJson();
                entry["blockIndex"] = bi;
                entry["positionInBlock"] = ti;
                ok(res, {{"found", true}, {"transaction", entry}});
                return;
            }
        }
        err(res, 404, "Transaction ID " + to_string(txId) + " not found");
    });

    // ── GET /api/health ───────────────────────────────────────────────────────
    svr.Get("/api/health", [&](const httplib::Request&, httplib::Response& res) {
        ok(res, {
            {"status",      "ok"},
            {"dbConnected", state.db.isConnected()},
            {"chainLoaded", state.loaded()},
            {"chainName",   state.chainName}
        });
    });

    // ── Start ─────────────────────────────────────────────────────────────────
    const char* portEnv = getenv("PORT");
    int port = portEnv ? atoi(portEnv) : 8080;

    cout << "\n=== Blockchain API Server ===\n";
    cout << "Listening on http://localhost:" << port << "\n";
    cout << "Press Ctrl+C to stop.\n\n";
    cout << "Endpoints:\n";
    cout << "  GET  /api/health\n";
    cout << "  GET  /api/blockchain/chains\n";
    cout << "  POST /api/blockchain/create   { name, difficulty }\n";
    cout << "  POST /api/blockchain/load     { name }\n";
    cout << "  POST /api/blockchain/save     { name? }\n";
    cout << "  GET  /api/blockchain/summary\n";
    cout << "  GET  /api/blockchain/blocks\n";
    cout << "  GET  /api/blockchain/blocks/:index\n";
    cout << "  GET  /api/blockchain/verify\n";
    cout << "  GET  /api/accounts\n";
    cout << "  POST /api/transactions        { sender, receiver, amount, metadata? }\n";
    cout << "  GET  /api/transactions/pending\n";
    cout << "  POST /api/mine\n";
    cout << "  GET  /api/search/sender/:name\n";
    cout << "  GET  /api/search/receiver/:name\n";
    cout << "  GET  /api/search/tx/:id\n\n";

    svr.listen("0.0.0.0", port);
    return 0;
}
