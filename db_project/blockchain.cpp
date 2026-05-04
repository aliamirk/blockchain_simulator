/*
 * Blockchain Simulator — MySQL Edition
 *
 * Dependencies (install before compiling):
 *   sudo apt-get install libmysqlclient-dev      # Debian/Ubuntu
 *   sudo dnf install mysql-devel                 # Fedora/RHEL
 *   brew install mysql-client                    # macOS
 *
 * Compile:
 *   g++ -std=c++17 blockchain.cpp -o blockchain \
 *       $(mysql_config --cflags --libs)
 *
 * First-time DB setup (run once):
 *   mysql -u root -p < init.sql
 *
 * Configuration — edit the DB_* constants below OR export env vars:
 *   export DB_HOST=127.0.0.1
 *   export DB_PORT=3306
 *   export DB_NAME=blockchain_db
 *   export DB_USER=root
 *   export DB_PASS=p4pacific
 */

#include <iostream>
#include <sstream>
#include <string>
#include <ctime>
#include <iomanip>
#include <cstdlib>   // getenv
#include <mysql.h>

using namespace std;

// ==================== DB CONFIG (override with env vars) ====================
static const char* DB_HOST_DEFAULT = "127.0.0.1";
static const int   DB_PORT_DEFAULT = 3306;
static const char* DB_NAME_DEFAULT = "blockchain_db";
static const char* DB_USER_DEFAULT = "root";
static const char* DB_PASS_DEFAULT = "p4pacific";

// ==================== ANSI COLOR CODES ====================
#define RESET   "\033[0m"
#define BOLD    "\033[1m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define BLUE    "\033[34m"
#define MAGENTA "\033[35m"
#define CYAN    "\033[36m"

// ==================== UTILITY FUNCTIONS ====================
void clearScreen() {
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

void printHeader(const string& title) {
    cout << "\n" << CYAN << BOLD;
    cout << "╔════════════════════════════════════════════════════════════╗\n";
    cout << "║" << setw(40) << title << setw(22) << "║\n";
    cout << "╚════════════════════════════════════════════════════════════╝";
    cout << RESET << "\n\n";
}

void printSeparator() {
    cout << BLUE << "────────────────────────────────────────────────────────────\n" << RESET;
}

void printSuccess(const string& msg) { cout << GREEN  << "✓ " << msg << RESET << endl; }
void printError  (const string& msg) { cout << RED    << "✗ " << msg << RESET << endl; }
void printWarning(const string& msg) { cout << YELLOW << "⚠ " << msg << RESET << endl; }
void printInfo   (const string& msg) { cout << CYAN   << "ℹ " << msg << RESET << endl; }

void pressEnter() {
    cout << "\n" << YELLOW << "Press ENTER to continue..." << RESET;
    cin.ignore();
    cin.get();
}

// ==================== SIMPLIFIED HASH FUNCTION ====================
int hashString(const string& s) {
    int h = 5381;
    for (size_t i = 0; i < s.length(); i++)
        h = ((h << 5) + h) + (unsigned char)s[i];
    return abs(h);
}

// ==================== TRANSACTION CLASS ====================
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

    string toString() const {
        stringstream ss;
        ss << MAGENTA << "  [TX#" << id << "] " << RESET
           << sender << " → " << receiver
           << " | " << YELLOW << "$" << fixed << setprecision(2) << amount << RESET;
        if (!metadata.empty()) ss << " | " << metadata;
        return ss.str();
    }

    string toDetailedString() const {
        stringstream ss;
        ss << "  Transaction ID    : " << MAGENTA << id    << RESET << "\n"
           << "  Sender           : " << sender   << "\n"
           << "  Receiver         : " << receiver << "\n"
           << "  Amount           : " << YELLOW << "$" << fixed << setprecision(2) << amount << RESET << "\n"
           << "  Metadata         : " << (metadata.empty() ? "-" : metadata) << "\n"
           << "  Signature        : " << signature;
        return ss.str();
    }
};
int Transaction::nextId = 1;

// ==================== BINARY SEARCH TREE ====================
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

// ==================== PRIORITY QUEUE (MAX HEAP BY AMOUNT) ====================
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
        if (size >= capacity) { printWarning("Priority queue full!"); return; }
        heap[size] = tx; up(size++);
    }
    Transaction* dequeue() {
        if (!size) return nullptr;
        Transaction* tx = heap[0]; heap[0] = heap[--size]; down(0); return tx;
    }
    bool isEmpty() const { return size == 0; }
    int  getSize() const { return size; }
    void getAll(Transaction**& txs, int& count) {
        count = size;
        txs = new Transaction*[count];
        PriorityQueueTransaction tmp(capacity);
        for (int i = 0; i < size; i++) tmp.enqueue(heap[i]);
        for (int i = 0; i < count; i++) txs[i] = tmp.dequeue();
    }
    void displayPending() const {
        if (!size) { cout << "  No pending transactions\n"; return; }
        Transaction** sorted = new Transaction*[size];
        for (int i = 0; i < size; i++) sorted[i] = heap[i];
        for (int i = 0; i < size-1; i++)
            for (int j = i+1; j < size; j++)
                if (sorted[j]->amount > sorted[i]->amount) swap(sorted[i], sorted[j]);
        for (int i = 0; i < size; i++)
            cout << sorted[i]->toString() << " "
                 << GREEN << "[Priority: $" << fixed << setprecision(2) << sorted[i]->amount << "]" << RESET << "\n";
        delete[] sorted;
    }
};

// ==================== BLOCK CLASS ====================
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
        printInfo("Mining block (difficulty " + to_string(difficulty) + ")...");
        int target = 1;
        for (int i = 0; i < difficulty; i++) target *= 10;
        int attempts = 0;
        while (abs(hash) % target != 0) {
            nonce++; hash = calculateHash(); attempts++;
            if (attempts % 5000 == 0)
                cout << YELLOW << "  Attempts: " << attempts << " | Nonce: " << nonce << RESET << "\r" << flush;
        }
        cout << string(60, ' ') << "\r";
        printSuccess("Mined! Nonce: " + to_string(nonce) + " | Attempts: " + to_string(attempts));
    }

    string toString() const {
        stringstream ss;
        char tbuf[80]; struct tm* ti = localtime(&timestamp);
        strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S", ti);
        ss << "\n" << BLUE << BOLD << "┌─ Block #" << index << " " << string(45,'-') << "┐\n" << RESET;
        ss << BLUE << "│" << RESET << " Hash        : " << GREEN << hash << RESET << string(42,' ') << BLUE << "│\n" << RESET;
        ss << BLUE << "│" << RESET << " Prev Hash   : " << prevHash << string(42,' ') << BLUE << "│\n" << RESET;
        ss << BLUE << "│" << RESET << " Timestamp   : " << tbuf << string(19,' ') << BLUE << "│\n" << RESET;
        ss << BLUE << "│" << RESET << " Nonce       : " << nonce << string(42,' ') << BLUE << "│\n" << RESET;
        ss << BLUE << "│" << RESET << " Transactions: " << txCount << string(42,' ') << BLUE << "│\n" << RESET;
        ss << BLUE << "├" << string(59,'-') << "┤\n" << RESET;
        for (int i = 0; i < txCount; i++) {
            string line = transactions[i].toString();
            ss << BLUE << "│" << RESET << line;
            int pad = max(0, 78 - (int)line.length());
            ss << string(pad,' ') << BLUE << "│\n" << RESET;
        }
        ss << BLUE << "└" << string(59,'-') << "┘" << RESET;
        return ss.str();
    }

private:
    int calculateHash() {
        stringstream ss;
        ss << index << timestamp << prevHash << nonce;
        for (int i = 0; i < txCount; i++) ss << transactions[i].toString();
        return hashString(ss.str());
    }
};

// ==================== LINKED LIST FOR BLOCKS ====================
struct NodeBlock { Block* block; NodeBlock* next; NodeBlock(Block* b): block(b), next(nullptr){} };

class LinkedListBlock {
    NodeBlock* head; int sz;
public:
    LinkedListBlock() : head(nullptr), sz(0) {}
    ~LinkedListBlock() {
        NodeBlock* c = head;
        while (c) { NodeBlock* n = c->next; delete c->block; delete c; c = n; }
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
    void printAll() {
        cout << "\n" << CYAN << "  Account Balances:\n" << RESET; printSeparator();
        for (int i = 0; i < SZ; i++) {
            NodeHash* c = table[i];
            while (c) {
                cout << "  " << setw(15) << left << c->key << ": "
                     << YELLOW << "$" << fixed << setprecision(2) << right << setw(10) << c->value << RESET << "\n";
                c = c->next;
            }
        }
        printSeparator();
    }
    // Iterator helper for DB sync
    void forEach(void(*fn)(const string&, double, void*), void* ctx) {
        for (int i = 0; i < SZ; i++) {
            NodeHash* c = table[i]; while (c) { fn(c->key, c->value, ctx); c=c->next; }
        }
    }
};

// ============================================================================
// MySQL DATABASE LAYER
// ============================================================================

/*
 * DbConfig holds the connection parameters.
 * Values are read from environment variables if set, otherwise fall back to
 * the compile-time defaults defined at the top of this file.
 *
 *   DB_HOST   — hostname or IP of MySQL server  (default: 127.0.0.1)
 *   DB_PORT   — TCP port                         (default: 3306)
 *   DB_NAME   — database / schema name           (default: blockchain_db)
 *   DB_USER   — MySQL username                   (default: root)
 *   DB_PASS   — MySQL password                   (default: "")
 */
struct DbConfig {
    string host;
    int    port;
    string dbname;
    string user;
    string pass;

    DbConfig() {
        auto env = [](const char* var, const char* def) -> string {
            const char* v = getenv(var);
            return v ? string(v) : string(def);
        };
        host   = env("DB_HOST", DB_HOST_DEFAULT);
        port   = [&]{ const char* v = getenv("DB_PORT");
                       return v ? atoi(v) : DB_PORT_DEFAULT; }();
        dbname = env("DB_NAME", DB_NAME_DEFAULT);
        user   = env("DB_USER", DB_USER_DEFAULT);
        pass   = env("DB_PASS", DB_PASS_DEFAULT);
    }

    void print() const {
        cout << CYAN << "  DB Config:\n" << RESET;
        cout << "    Host   : " << host   << "\n";
        cout << "    Port   : " << port   << "\n";
        cout << "    Schema : " << dbname << "\n";
        cout << "    User   : " << user   << "\n";
        cout << "    Pass   : " << string(pass.size(), '*') << "\n";
    }
};

// ==================== MySQLDB wrapper ====================
class MySQLDB {
    MYSQL* conn;
    DbConfig cfg;

    // Helper: escape a string for safe interpolation
    string esc(const string& s) {
        string out(s.size()*2+1, '\0');
        unsigned long len = mysql_real_escape_string(conn, &out[0], s.c_str(), s.size());
        out.resize(len);
        return out;
    }

    // Helper: run a statement, throw on error
    void exec(const string& sql) {
        if (mysql_query(conn, sql.c_str())) {
            throw runtime_error(string("MySQL error: ") + mysql_error(conn) + "\nSQL: " + sql);
        }
    }

    // Helper: run query and return result (caller must mysql_free_result)
    MYSQL_RES* query(const string& sql) {
        if (mysql_query(conn, sql.c_str()))
            throw runtime_error(string("MySQL error: ") + mysql_error(conn) + "\nSQL: " + sql);
        return mysql_store_result(conn);
    }

    // Helper: get last AUTO_INCREMENT id
    long long lastInsertId() { return (long long)mysql_insert_id(conn); }

public:
    MySQLDB() : conn(nullptr) {}
    ~MySQLDB() { disconnect(); }

    // ── Connect ─────────────────────────────────────────────────────────────
    bool connect(const DbConfig& config) {
        cfg = config;
        conn = mysql_init(nullptr);
        if (!conn) { printError("mysql_init() failed"); return false; }

        unsigned int timeout = 10;
        mysql_options(conn, MYSQL_OPT_CONNECT_TIMEOUT, &timeout);

        if (!mysql_real_connect(conn,
                cfg.host.c_str(),
                cfg.user.c_str(),
                cfg.pass.c_str(),
                cfg.dbname.c_str(),
                cfg.port,
                nullptr, 0)) {
            printError("Cannot connect to MySQL: " + string(mysql_error(conn)));
            mysql_close(conn); conn = nullptr;
            return false;
        }
        // Enforce UTF-8
        mysql_set_character_set(conn, "utf8mb4");
        return true;
    }

    void disconnect() {
        if (conn) { mysql_close(conn); conn = nullptr; }
    }

    bool isConnected() const { return conn != nullptr; }

    // ── Audit ────────────────────────────────────────────────────────────────
    void auditLog(const string& op, const string& chainName, const string& details) {
        exec("INSERT INTO AUDIT_LOG (OPERATION, CHAIN_NAME, DETAILS) VALUES ('"
             + esc(op) + "','" + esc(chainName) + "','" + esc(details) + "')");
    }

    // ── ACCOUNTS ─────────────────────────────────────────────────────────────
    void upsertAccount(const string& name, double balance) {
        exec("INSERT INTO ACCOUNTS (NAME, BALANCE) VALUES ('" + esc(name) + "'," +
             to_string(balance) + ") ON DUPLICATE KEY UPDATE BALANCE=" + to_string(balance));
    }

    // Load all accounts into the in-memory hash table
    void loadAccounts(HashTableBalance& ht) {
        MYSQL_RES* res = query("SELECT NAME, BALANCE FROM ACCOUNTS");
        MYSQL_ROW row;
        while ((row = mysql_fetch_row(res)))
            ht.update(string(row[0]), atof(row[1]));
        mysql_free_result(res);
    }

    // Persist entire in-memory balance table to DB
    void saveAccounts(HashTableBalance& ht) {
        struct Ctx { MySQLDB* db; };
        Ctx ctx{this};
        ht.forEach([](const string& k, double v, void* c){
            static_cast<Ctx*>(c)->db->upsertAccount(k, v);
        }, &ctx);
    }

    // ── BLOCKCHAIN META ───────────────────────────────────────────────────────
    // Returns CHAIN_ID, or -1 if not found
    int getChainId(const string& name) {
        MYSQL_RES* res = query("SELECT CHAIN_ID FROM BLOCKCHAIN_META WHERE NAME='" + esc(name) + "'");
        MYSQL_ROW row = mysql_fetch_row(res);
        int id = row ? atoi(row[0]) : -1;
        mysql_free_result(res);
        return id;
    }

    // Insert or update BLOCKCHAIN_META; returns CHAIN_ID
    int upsertChainMeta(const string& name, int difficulty, int nextTxId) {
        int id = getChainId(name);
        if (id < 0) {
            exec("INSERT INTO BLOCKCHAIN_META (NAME, DIFFICULTY, NEXT_TX_ID) VALUES ('"
                 + esc(name) + "'," + to_string(difficulty) + "," + to_string(nextTxId) + ")");
            id = (int)lastInsertId();
            // Create statistics row
            exec("INSERT INTO CHAIN_STATISTICS (CHAIN_ID) VALUES (" + to_string(id) + ")");
        } else {
            exec("UPDATE BLOCKCHAIN_META SET DIFFICULTY=" + to_string(difficulty) +
                 ", NEXT_TX_ID=" + to_string(nextTxId) +
                 " WHERE CHAIN_ID=" + to_string(id));
        }
        return id;
    }

    // ── SAVE BLOCKCHAIN ───────────────────────────────────────────────────────
    /*
     * Saves the full blockchain + balances to MySQL.
     * Steps:
     *   1. Upsert BLOCKCHAIN_META to get CHAIN_ID
     *   2. Delete existing BLOCKS for this chain (cascades to TRANSACTIONS)
     *   3. Re-insert all blocks and their transactions
     *   4. Delete + re-insert PENDING_TRANSACTIONS
     *   5. Upsert all account balances
     *   6. Update CHAIN_STATISTICS
     *   7. Write AUDIT_LOG entry
     */
    bool saveBlockchain(const string& chainName,
                        LinkedListBlock& chain,
                        PriorityQueueTransaction& pool,
                        HashTableBalance& balances,
                        int difficulty)
    {
        if (!conn) return false;
        try {
            exec("START TRANSACTION");

            int chainId = upsertChainMeta(chainName, difficulty, Transaction::nextId);

            // Resolve SYSTEM miner id
            MYSQL_RES* mr = query("SELECT MINER_ID FROM MINERS WHERE NAME='SYSTEM'");
            MYSQL_ROW  mrow = mysql_fetch_row(mr);
            int minerId = mrow ? atoi(mrow[0]) : 1;
            mysql_free_result(mr);

            // Resolve default type id ('transfer')
            MYSQL_RES* tr = query("SELECT TYPE_ID FROM TRANSACTION_TYPES WHERE TYPE_NAME='transfer'");
            MYSQL_ROW  trow = mysql_fetch_row(tr);
            int typeId = trow ? atoi(trow[0]) : 1;
            mysql_free_result(tr);

            // Drop old blocks (cascade deletes transactions)
            exec("DELETE FROM BLOCKS WHERE CHAIN_ID=" + to_string(chainId));

            // Insert blocks + transactions
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

            // Pending transactions
            exec("DELETE FROM PENDING_TRANSACTIONS WHERE TYPE_ID=" + to_string(typeId));
            // Re-insert from pool (non-destructively)
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

            // Balances
            saveAccounts(balances);

            // Stats
            exec("UPDATE CHAIN_STATISTICS SET TOTAL_BLOCKS=" + to_string(chain.getSize()) +
                 ", TOTAL_TRANSACTIONS=" + to_string(totalTx) +
                 ", TOTAL_PENDING=" + to_string(pool.getSize()) +
                 " WHERE CHAIN_ID=" + to_string(chainId));

            exec("COMMIT");
            auditLog("SAVE", chainName,
                     "blocks=" + to_string(chain.getSize()) + " tx=" + to_string(totalTx));
            return true;
        } catch (const exception& e) {
            exec("ROLLBACK");
            printError(e.what());
            return false;
        }
    }

    // ── LOAD BLOCKCHAIN ───────────────────────────────────────────────────────
    /*
     * Loads blockchain state from MySQL into the in-memory structures.
     * Steps:
     *   1. Look up CHAIN_ID from BLOCKCHAIN_META
     *   2. Restore difficulty + Transaction::nextId
     *   3. Rebuild LinkedListBlock from BLOCKS + TRANSACTIONS
     *   4. Rebuild BSTTransactionIndex
     *   5. Restore PENDING_TRANSACTIONS into priority queue
     *   6. Load account balances
     */
    bool loadBlockchain(const string& chainName,
                        LinkedListBlock& chain,
                        PriorityQueueTransaction& pool,
                        HashTableBalance& balances,
                        BSTTransactionIndex& bst,
                        int& difficulty)
    {
        if (!conn) return false;
        try {
            // Chain meta
            MYSQL_RES* mr = query("SELECT CHAIN_ID, DIFFICULTY, NEXT_TX_ID "
                                  "FROM BLOCKCHAIN_META WHERE NAME='" + esc(chainName) + "'");
            MYSQL_ROW mrow = mysql_fetch_row(mr);
            if (!mrow) { mysql_free_result(mr); printError("Chain '" + chainName + "' not found."); return false; }
            int chainId = atoi(mrow[0]);
            difficulty  = atoi(mrow[1]);
            Transaction::nextId = atoi(mrow[2]);
            mysql_free_result(mr);

            // Blocks ordered by BLOCK_INDEX
            MYSQL_RES* br = query("SELECT BLOCK_ID, BLOCK_INDEX, BLOCK_TIMESTAMP, NONCE, HASH, PREV_HASH "
                                  "FROM BLOCKS WHERE CHAIN_ID=" + to_string(chainId) +
                                  " ORDER BY BLOCK_INDEX");
            MYSQL_ROW brow;
            int blockPos = 0;
            while ((brow = mysql_fetch_row(br))) {
                int blockDbId    = atoi(brow[0]);
                int blkIndex     = atoi(brow[1]);
                long blkTs       = atol(brow[2]);
                int  blkNonce    = atoi(brow[3]);
                int  blkHash     = atoi(brow[4]);
                int  blkPrevHash = atoi(brow[5]);

                // Transactions for this block
                MYSQL_RES* tr = query("SELECT TX_ID, SENDER, RECEIVER, AMOUNT, METADATA, SIGNATURE "
                                      "FROM TRANSACTIONS WHERE BLOCK_ID=" + to_string(blockDbId) +
                                      " ORDER BY TRANSACTION_ID");
                int txCnt = (int)mysql_num_rows(tr);
                Transaction* txArr = txCnt > 0 ? new Transaction[txCnt] : nullptr;
                MYSQL_ROW txrow; int ti = 0;
                while ((txrow = mysql_fetch_row(tr))) {
                    txArr[ti].id       = atoi(txrow[0]);
                    txArr[ti].sender   = txrow[1];
                    txArr[ti].receiver = txrow[2];
                    txArr[ti].amount   = atof(txrow[3]);
                    txArr[ti].metadata = txrow[4] ? txrow[4] : "";
                    txArr[ti].signature= txrow[5] ? txrow[5] : "";
                    bst.insert(txArr[ti].id, blockPos, ti);
                    ti++;
                }
                mysql_free_result(tr);

                Block* blk = new Block();
                blk->index     = blkIndex;
                blk->timestamp = blkTs;
                blk->nonce     = blkNonce;
                blk->hash      = blkHash;
                blk->prevHash  = blkPrevHash;
                blk->txCount   = txCnt;
                blk->transactions = txArr;
                chain.append(blk);
                blockPos++;
            }
            mysql_free_result(br);

            // Pending transactions
            MYSQL_RES* pr = query("SELECT SENDER, RECEIVER, AMOUNT, METADATA, SIGNATURE "
                                  "FROM PENDING_TRANSACTIONS ORDER BY PENDING_TX_ID");
            MYSQL_ROW prow;
            while ((prow = mysql_fetch_row(pr))) {
                Transaction* tx = new Transaction(prow[0], prow[1], atof(prow[2]),
                                                  prow[3] ? prow[3] : "");
                if (prow[4]) tx->signature = prow[4];
                pool.enqueue(tx);
            }
            mysql_free_result(pr);

            // Balances
            loadAccounts(balances);

            auditLog("LOAD", chainName, "blocks=" + to_string(blockPos));
            return true;
        } catch (const exception& e) {
            printError(e.what());
            return false;
        }
    }

    // ── LIST CHAINS ───────────────────────────────────────────────────────────
    void listChains() {
        MYSQL_RES* res = query(
            "SELECT m.NAME, m.DIFFICULTY, s.TOTAL_BLOCKS, s.TOTAL_TRANSACTIONS, m.CREATED_AT "
            "FROM BLOCKCHAIN_META m "
            "LEFT JOIN CHAIN_STATISTICS s ON s.CHAIN_ID = m.CHAIN_ID "
            "ORDER BY m.CHAIN_ID");
        MYSQL_ROW row;
        cout << "\n" << CYAN << "  Saved Blockchains in DB:\n" << RESET;
        printSeparator();
        cout << left << setw(20) << "Name" << setw(12) << "Difficulty"
             << setw(10) << "Blocks" << setw(10) << "TXs" << "Created\n";
        printSeparator();
        while ((row = mysql_fetch_row(res))) {
            cout << setw(20) << row[0] << setw(12) << row[1]
                 << setw(10) << (row[2]?row[2]:"0") << setw(10) << (row[3]?row[3]:"0")
                 << row[4] << "\n";
        }
        mysql_free_result(res);
        printSeparator();
    }
};

// ============================================================================
// BLOCKCHAIN CLASS
// ============================================================================
class Blockchain {
public:
    LinkedListBlock chain;
    int difficulty;

    Blockchain(int diff) : difficulty(min(diff,5)) {
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
        NodeBlock* cur  = chain.getHead()->next;
        NodeBlock* prev = chain.getHead();
        while (cur) {
            if (cur->block->prevHash != prev->block->hash) {
                printError("Invalid previous hash link!"); return false;
            }
            prev = cur; cur = cur->next;
        }
        return true;
    }

    void displayBlockchain() { for (int i = 0; i < chain.getSize(); i++) cout << chain.get(i)->toString() << "\n"; }

    void displaySummary() {
        cout << "\n" << CYAN << BOLD << "Blockchain Summary:" << RESET << "\n"; printSeparator();
        cout << "  Total Blocks: " << GREEN  << chain.getSize()         << RESET << "\n";
        cout << "  Difficulty  : " << YELLOW << difficulty               << RESET << "\n";
        cout << "  Latest Hash : " << GREEN  << getLatestBlock()->hash   << RESET << "\n";
        printSeparator();
    }

    void rebuildBSTIndex(BSTTransactionIndex* bst) {
        bst->clear();
        for (int i = 0; i < chain.getSize(); i++) {
            Block* blk = chain.get(i);
            for (int j = 0; j < blk->txCount; j++)
                bst->insert(blk->transactions[j].id, i, j);
        }
    }
};

// ==================== MENU HELPERS ====================
void initializeBalances(HashTableBalance& b) {
    b.update("Alice",   1000.0);
    b.update("Bob",     1000.0);
    b.update("Charlie", 1000.0);
    b.update("David",    500.0);
    b.update("Eve",      750.0);
}

void displayMenu() {
    cout << CYAN << BOLD;
    cout << "\n  1. " << RESET << "Create New Blockchain\n";
    cout << CYAN << BOLD << "  2. " << RESET << "Load Blockchain from DB\n";
    cout << CYAN << BOLD << "  3. " << RESET << "Save Blockchain to DB\n";
    cout << CYAN << BOLD << "  4. " << RESET << "Add Transaction to Pool\n";
    cout << CYAN << BOLD << "  5. " << RESET << "Mine Pending Transactions\n";
    cout << CYAN << BOLD << "  6. " << RESET << "Display Full Blockchain\n";
    cout << CYAN << BOLD << "  7. " << RESET << "View Blockchain Summary\n";
    cout << CYAN << BOLD << "  8. " << RESET << "Verify Blockchain Integrity\n";
    cout << CYAN << BOLD << "  9. " << RESET << "View Account Balances\n";
    cout << CYAN << BOLD << " 10. " << RESET << "View Pending Transactions\n";
    cout << CYAN << BOLD << " 11. " << RESET << "Search Transactions\n";
    cout << CYAN << BOLD << " 12. " << RESET << "List Chains in DB\n";
    cout << CYAN << BOLD << "  0. " << RESET << RED << "Exit\n" << RESET;
    printSeparator();
    cout << YELLOW << "  Enter your choice: " << RESET;
}

void searchMenu(Blockchain& bc, BSTTransactionIndex& bst) {
    clearScreen(); printHeader("SEARCH TRANSACTIONS");
    cout << "  1. Search by Sender\n  2. Search by Receiver\n"
            "  3. Search by Transaction ID (BST)\n  4. View Specific Block\n";
    printSeparator(); cout << YELLOW << "  Enter choice: " << RESET;
    int choice; cin >> choice;
    if (choice == 1) {
        string s; cout << "  Enter sender name: "; cin >> s;
        bool found = false;
        for (int i = 0; i < bc.chain.getSize(); i++) {
            Block* blk = bc.chain.get(i);
            for (int j = 0; j < blk->txCount; j++)
                if (blk->transactions[j].sender == s) { cout << "  Block " << i << ": " << blk->transactions[j].toString() << "\n"; found=true; }
        }
        if (!found) printWarning("No transactions found for sender: " + s);
    } else if (choice == 2) {
        string r; cout << "  Enter receiver name: "; cin >> r;
        bool found = false;
        for (int i = 0; i < bc.chain.getSize(); i++) {
            Block* blk = bc.chain.get(i);
            for (int j = 0; j < blk->txCount; j++)
                if (blk->transactions[j].receiver == r) { cout << "  Block " << i << ": " << blk->transactions[j].toString() << "\n"; found=true; }
        }
        if (!found) printWarning("No transactions found for receiver: " + r);
    } else if (choice == 3) {
        int id; cout << "  Enter transaction ID: "; cin >> id;
        int bi=-1, ti=-1;
        if (bst.search(id, bi, ti)) {
            Block* blk = bc.chain.get(bi);
            if (blk && ti < blk->txCount) {
                printSuccess("Found via BST (O log n)!");
                cout << GREEN << "  Location: Block #" << bi << ", Pos #" << ti << RESET << "\n\n";
                cout << blk->transactions[ti].toDetailedString() << "\n";
            } else printError("BST index corrupted.");
        } else printError("TX ID " + to_string(id) + " not found.");
    } else if (choice == 4) {
        int idx; cout << "  Enter block index: "; cin >> idx;
        if (idx >= 0 && idx < bc.chain.getSize()) cout << bc.chain.get(idx)->toString() << "\n";
        else printError("Block index out of range");
    }
    pressEnter();
}

// ==================== MAIN ====================
int main() {
    // ── DB connection setup ──────────────────────────────────────────────────
    DbConfig cfg;
    clearScreen(); printHeader("BLOCKCHAIN SIMULATOR — MySQL Edition");
    cfg.print();
    cout << "\n  Connecting to MySQL...\n";

    MySQLDB db;
    if (!db.connect(cfg)) {
        printError("Failed to connect to MySQL. Check your config / env vars.");
        printInfo("You can override defaults with env vars: DB_HOST, DB_PORT, DB_NAME, DB_USER, DB_PASS");
        return 1;
    }
    printSuccess("Connected to MySQL successfully!");
    pressEnter();

    // ── Runtime state ────────────────────────────────────────────────────────
    Blockchain*              bc      = nullptr;
    PriorityQueueTransaction* pool   = nullptr;
    HashTableBalance*         balances = nullptr;
    BSTTransactionIndex*      bst    = nullptr;
    string                    currentChainName;

    int choice;
    while (true) {
        clearScreen(); printHeader("BLOCKCHAIN SIMULATOR");
        if (bc) bc->displaySummary();
        else    printWarning("No blockchain loaded. Please create or load one first.");
        displayMenu();
        cin >> choice;

        // ── 1. CREATE ──────────────────────────────────────────────────────
        if (choice == 1) {
            clearScreen(); printHeader("CREATE NEW BLOCKCHAIN");
            int diff; cout << "  Enter difficulty (1-5): "; cin >> diff;
            if (diff < 1 || diff > 5) { printWarning("Invalid difficulty, setting to 3"); diff = 3; }
            cout << "  Enter a name for this blockchain: "; cin >> currentChainName;

            delete bc; delete pool; delete balances; delete bst;
            bc = new Blockchain(diff);
            pool = new PriorityQueueTransaction();
            balances = new HashTableBalance();
            bst = new BSTTransactionIndex();
            initializeBalances(*balances);

            printSuccess("New blockchain '" + currentChainName + "' created!");
            printInfo("Default accounts: Alice($1000), Bob($1000), Charlie($1000), David($500), Eve($750)");
            pressEnter();

        // ── 2. LOAD ────────────────────────────────────────────────────────
        } else if (choice == 2) {
            clearScreen(); printHeader("LOAD BLOCKCHAIN FROM DB");
            db.listChains();
            string name; cout << "  Enter blockchain name to load: "; cin >> name;

            delete bc; delete pool; delete balances; delete bst;
            bc       = new Blockchain(3);    // temp difficulty, overwritten by load
            pool     = new PriorityQueueTransaction();
            balances = new HashTableBalance();
            bst      = new BSTTransactionIndex();

            // Remove genesis block added by constructor — load will rebuild chain
            // We recreate bc after knowing the difficulty from the DB
            int loadedDiff = 3;
            LinkedListBlock tmpChain;
            PriorityQueueTransaction tmpPool;
            HashTableBalance tmpBal;
            BSTTransactionIndex tmpBst;

            if (db.loadBlockchain(name, tmpChain, tmpPool, tmpBal, tmpBst, loadedDiff)) {
                delete bc; delete pool; delete balances; delete bst;

                bc = new Blockchain(loadedDiff);
                // Replace genesis block with loaded chain
                // (re-create object; chain is populated inside loadBlockchain via tmpChain)
                // Simplest approach: reconstruct fresh bc with loaded data
                delete bc;
                bc = new Blockchain(loadedDiff);
                // Destroy the genesis node and steal tmpChain's list
                NodeBlock* gc = bc->chain.getHead();
                delete gc->block; delete gc;
                bc->chain = move(tmpChain);  // LinkedListBlock needs move — add below

                pool     = new PriorityQueueTransaction();
                balances = new HashTableBalance();
                bst      = new BSTTransactionIndex();

                // Re-load since tmp objects moved
                // Actually reload fresh into the real objects
                delete bc; delete pool; delete balances; delete bst;
                bc       = new Blockchain(loadedDiff);
                pool     = new PriorityQueueTransaction();
                balances = new HashTableBalance();
                bst      = new BSTTransactionIndex();

                // Clear genesis; loadBlockchain will fill chain
                NodeBlock* h = bc->chain.getHead();
                delete h->block; delete h;
                bc->chain = LinkedListBlock();  // reset

                if (db.loadBlockchain(name, bc->chain, *pool, *balances, *bst, bc->difficulty)) {
                    currentChainName = name;
                    printSuccess("Blockchain '" + name + "' loaded!");
                } else {
                    printError("Load failed (second pass).");
                    delete bc; delete pool; delete balances; delete bst;
                    bc=nullptr; pool=nullptr; balances=nullptr; bst=nullptr;
                }
            } else {
                printError("Chain '" + name + "' not found or load failed.");
                delete bc; delete pool; delete balances; delete bst;
                bc=nullptr; pool=nullptr; balances=nullptr; bst=nullptr;
            }
            pressEnter();

        // ── 3. SAVE ────────────────────────────────────────────────────────
        } else if (choice == 3) {
            if (!bc) { printError("No blockchain to save!"); pressEnter(); continue; }
            clearScreen(); printHeader("SAVE BLOCKCHAIN TO DB");
            string name = currentChainName;
            if (name.empty()) { cout << "  Enter blockchain name: "; cin >> name; currentChainName = name; }
            cout << "  Saving as '" << name << "'...\n";
            if (db.saveBlockchain(name, bc->chain, *pool, *balances, bc->difficulty))
                printSuccess("Saved to MySQL as '" + name + "'!");
            else
                printError("Save failed.");
            pressEnter();

        // ── 4. ADD TRANSACTION ─────────────────────────────────────────────
        } else if (choice == 4) {
            if (!pool) { printError("Create or load blockchain first!"); pressEnter(); continue; }
            clearScreen(); printHeader("ADD NEW TRANSACTION");
            string sender, receiver, meta; double amount;
            cout << "  Sender: "; cin >> sender;
            cout << "  Receiver: "; cin >> receiver;
            cout << "  Amount: $"; cin >> amount;
            cout << "  Message (optional): "; cin.ignore(); getline(cin, meta);

            if (!balances->contains(sender)) { printError("Sender account does not exist!"); pressEnter(); continue; }
            if (amount <= 0) { printError("Amount must be positive!"); pressEnter(); continue; }
            if (!balances->hasSufficientBalance(sender, amount)) {
                printError("Insufficient balance! Balance: $" + to_string(balances->get(sender)));
                pressEnter(); continue;
            }
            Transaction* tx = new Transaction(sender, receiver, amount, meta);
            pool->enqueue(tx);
            printSuccess("Transaction added to pool!"); cout << tx->toString() << "\n";
            pressEnter();

        // ── 5. MINE ────────────────────────────────────────────────────────
        } else if (choice == 5) {
            if (!pool || pool->isEmpty()) { printWarning("No pending transactions!"); pressEnter(); continue; }
            clearScreen(); printHeader("MINE BLOCK");
            int count; Transaction** txs; pool->getAll(txs, count);
            cout << "\n  Mining block with " << count << " transaction(s)...\n\n";
            Transaction* txArr = new Transaction[count];
            for (int i = 0; i < count; i++) txArr[i] = *txs[i];
            bc->addBlock(txArr, count); delete[] txArr;
            for (int i = 0; i < count; i++) {
                balances->update(txs[i]->sender,   balances->get(txs[i]->sender)   - txs[i]->amount);
                balances->update(txs[i]->receiver, balances->get(txs[i]->receiver) + txs[i]->amount);
                pool->dequeue();
            }
            delete[] txs;
            bc->rebuildBSTIndex(bst);
            printSuccess("Block mined and added!");
            pressEnter();

        // ── 6. DISPLAY ─────────────────────────────────────────────────────
        } else if (choice == 6) {
            if (!bc) { printError("No blockchain!"); pressEnter(); continue; }
            clearScreen(); printHeader("FULL BLOCKCHAIN"); bc->displayBlockchain(); pressEnter();

        // ── 7. SUMMARY ─────────────────────────────────────────────────────
        } else if (choice == 7) {
            if (!bc) { printError("No blockchain!"); pressEnter(); continue; }
            clearScreen(); printHeader("BLOCKCHAIN SUMMARY"); bc->displaySummary();
            int total=0; for (int i=0;i<bc->chain.getSize();i++) total+=bc->chain.get(i)->txCount;
            cout << "  Total Transactions  : " << GREEN  << total             << RESET << "\n";
            cout << "  Pending Transactions: " << YELLOW << pool->getSize()   << RESET << "\n";
            cout << "  Chain Name          : " << CYAN   << currentChainName  << RESET << "\n";
            printSeparator(); pressEnter();

        // ── 8. VERIFY ──────────────────────────────────────────────────────
        } else if (choice == 8) {
            if (!bc) { printError("No blockchain!"); pressEnter(); continue; }
            clearScreen(); printHeader("VERIFY BLOCKCHAIN");
            printInfo("Verifying integrity...");
            if (bc->isChainValid()) printSuccess("Blockchain valid and secure!");
            else printError("Blockchain tampered!");
            pressEnter();

        // ── 9. BALANCES ────────────────────────────────────────────────────
        } else if (choice == 9) {
            if (!balances) { printError("No balances!"); pressEnter(); continue; }
            clearScreen(); printHeader("ACCOUNT BALANCES"); balances->printAll(); pressEnter();

        // ── 10. PENDING ────────────────────────────────────────────────────
        } else if (choice == 10) {
            if (!pool) { printError("No pool!"); pressEnter(); continue; }
            clearScreen(); printHeader("PENDING TRANSACTIONS");
            cout << "\n"; pool->displayPending();
            cout << "\n  Total pending: " << YELLOW << pool->getSize() << RESET << "\n";
            printSeparator(); pressEnter();

        // ── 11. SEARCH ─────────────────────────────────────────────────────
        } else if (choice == 11) {
            if (!bc) { printError("No blockchain!"); pressEnter(); continue; }
            searchMenu(*bc, *bst);

        // ── 12. LIST CHAINS ────────────────────────────────────────────────
        } else if (choice == 12) {
            clearScreen(); printHeader("CHAINS IN DATABASE");
            db.listChains(); pressEnter();

        // ── 0. EXIT ────────────────────────────────────────────────────────
        } else if (choice == 0) {
            clearScreen(); printHeader("EXITING");
            printSuccess("Thank you for using Blockchain Simulator!");
            cout << "\n"; break;
        } else {
            printError("Invalid choice!"); pressEnter();
        }
    }

    delete bc; delete pool; delete balances; delete bst;
    return 0;
}
