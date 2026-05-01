// Feature: oracle-db-migration, Property 1: Blockchain Save/Load Round-Trip
// **Validates: Requirements 4.1, 4.2, 4.3, 4.4, 4.5, 4.6, 5.1, 5.2, 5.3, 5.4, 5.5, 5.6**
//
// This property test verifies that for any valid blockchain state, saving via
// saveBlockchain() and loading via loadBlockchain() into fresh structures
// produces identical data across all fields.
//
// Requires: live Oracle database connection, RapidCheck library, OCCI headers
//
// Compile example:
//   g++ -std=c++17 -o test_property_blockchain_roundtrip \
//       test_property_blockchain_roundtrip.cpp db_manager.cpp \
//       -I$ORACLE_HOME/sdk/include -L$ORACLE_HOME -locci -lclntsh \
//       -lrapidcheck
//
// Run with minimum 100 iterations (default):
//   ./test_property_blockchain_roundtrip
// Override iteration count via environment:
//   RC_PARAMS="max_success=200" ./test_property_blockchain_roundtrip

// Rename main() in main.cpp to avoid linker conflict with our test main()
#define main main_blockchain_app
#include "main.cpp"
#undef main

#include <rapidcheck.h>
#include <vector>
#include <cmath>
#include <set>

#include "db_manager.h"
#include "db_config.h"

// ==================== Constants ====================
static const int MIN_ITERATIONS = 100;

// ==================== HELPER: Compare doubles with NUMBER(15,2) precision ====================
static bool amountsEqual(double a, double b) {
    double ra = round(a * 100.0) / 100.0;
    double rb = round(b * 100.0) / 100.0;
    return fabs(ra - rb) < 0.001;
}

// ==================== RapidCheck Generators ====================

static rc::Gen<string> genAlphaNumString(int minLen, int maxLen) {
    return rc::gen::mapcat(
        rc::gen::inRange(minLen, maxLen + 1),
        [](int len) {
            return rc::gen::container<string>(
                len,
                rc::gen::oneOf(
                    rc::gen::inRange('a', (char)('z' + 1)),
                    rc::gen::inRange('A', (char)('Z' + 1)),
                    rc::gen::inRange('0', (char)('9' + 1))
                )
            );
        }
    );
}

// Amounts must be > 0 (CHECK constraint CHK_TX_AMOUNT) and fit in NUMBER(15,2).
static rc::Gen<double> genPositiveAmount() {
    return rc::gen::map(
        rc::gen::inRange(1, 100000000),
        [](int cents) { return static_cast<double>(cents) / 100.0; }
    );
}

// Balances must be >= 0 (CHECK constraint CHK_ACCOUNT_BALANCE).
static rc::Gen<double> genNonNegativeBalance() {
    return rc::gen::map(
        rc::gen::inRange(0, 100000000),
        [](int cents) { return static_cast<double>(cents) / 100.0; }
    );
}

// ==================== Structured types for generated blockchain state ====================

struct GenTransaction {
    int id;
    string sender;
    string receiver;
    double amount;
    string metadata;
    string signature;
};

struct GenBlock {
    int index;
    long timestamp;
    int nonce;
    int hash;
    int prevHash;
    vector<GenTransaction> transactions;
};

struct GenAccountBalance {
    string name;
    double balance;
};

struct GenBlockchainState {
    string chainName;
    int difficulty;
    int nextTxId;
    vector<GenBlock> blocks;
    vector<GenAccountBalance> balances;
};

// ==================== Generator for full blockchain state ====================

static rc::Gen<GenBlockchainState> genBlockchainState() {
    return rc::gen::exec([]() {
        GenBlockchainState state;

        // Chain name: unique alphanumeric (3-20 chars)
        state.chainName = *genAlphaNumString(3, 20);

        // Difficulty: 1-5
        state.difficulty = *rc::gen::inRange(1, 6);

        // Starting nextTxId (1-1000)
        int startTxId = *rc::gen::inRange(1, 1001);
        int currentTxId = startTxId;

        // Number of blocks: 1-10 (first is always genesis)
        int numBlocks = *rc::gen::inRange(1, 11);
        int prevHash = 0;

        for (int i = 0; i < numBlocks; i++) {
            GenBlock block;
            block.index = i;
            block.timestamp = static_cast<long>(
                *rc::gen::inRange(1000000000, 2000000000));
            block.nonce = *rc::gen::inRange(0, 100000);
            block.prevHash = prevHash;

            if (i == 0) {
                // Genesis block: no transactions, hash = 0
                block.hash = 0;
            } else {
                // Non-genesis: 0-5 transactions
                int numTx = *rc::gen::inRange(0, 6);
                for (int j = 0; j < numTx; j++) {
                    GenTransaction tx;
                    tx.id = currentTxId++;
                    tx.sender = *genAlphaNumString(2, 15);
                    tx.receiver = *genAlphaNumString(2, 15);
                    tx.amount = *genPositiveAmount();
                    tx.metadata = *genAlphaNumString(0, 30);
                    tx.signature = "SIG" + to_string(
                        *rc::gen::inRange(1000, 999999));
                    block.transactions.push_back(tx);
                }
                block.hash = *rc::gen::inRange(1, 2000000000);
            }

            prevHash = block.hash;
            state.blocks.push_back(block);
        }

        state.nextTxId = currentTxId;

        // Generate 1-8 unique account balances
        int numAccounts = *rc::gen::inRange(1, 9);
        set<string> usedNames;
        for (int i = 0; i < numAccounts; i++) {
            GenAccountBalance acct;
            do {
                acct.name = *genAlphaNumString(2, 20);
            } while (usedNames.count(acct.name) > 0);
            usedNames.insert(acct.name);
            acct.balance = *genNonNegativeBalance();
            state.balances.push_back(acct);
        }

        return state;
    });
}

// ==================== Build in-memory blockchain from generated state ====================
// Writes a temporary .bc file and uses the existing loadFromFile() to populate
// the Blockchain, ensuring the in-memory structures are consistent.

static void buildBlockchainFromState(
    const GenBlockchainState& state,
    Blockchain*& outBlockchain,
    HashTableBalance*& outBalances,
    PriorityQueueTransaction*& outPool)
{
    string tmpFile = "__pbt_roundtrip_" + state.chainName;

    // Write temporary .bc file matching the generated state
    {
        ofstream file(tmpFile + ".bc");
        file << state.difficulty << "\n";
        file << (int)state.blocks.size() << "\n";
        file << state.nextTxId << "\n";

        for (const auto& block : state.blocks) {
            file << block.index << "\n"
                 << block.timestamp << "\n"
                 << (int)block.transactions.size() << "\n"
                 << block.nonce << "\n"
                 << block.hash << "\n"
                 << block.prevHash << "\n";
            for (const auto& tx : block.transactions) {
                file << tx.id << "\n"
                     << tx.sender << "\n"
                     << tx.receiver << "\n"
                     << tx.amount << "\n"
                     << tx.metadata << "\n"
                     << tx.signature << "\n";
            }
        }

        for (const auto& acct : state.balances) {
            file << acct.name << "\n" << acct.balance << "\n";
        }
        file << "END_BALANCES\n";
        file.close();
    }

    // Create blockchain and load from temp file
    outBlockchain = new Blockchain(state.difficulty);
    outBalances = new HashTableBalance();
    BSTTransactionIndex* tmpBst = new BSTTransactionIndex();

    outBlockchain->loadFromFile(tmpFile, outBalances, tmpBst);
    delete tmpBst;

    // Remove temp file
    remove((tmpFile + ".bc").c_str());

    // Create empty pool (saveBlockchain needs it for statistics)
    outPool = new PriorityQueueTransaction(100);

    // Ensure Transaction::nextId is set correctly after load
    Transaction::nextId = state.nextTxId;
}

// ==================== MAIN: Property test with 100+ iterations ====================

int main() {
    // Establish database connection once for all iterations
    DatabaseManager dbManager;
    if (!dbManager.connect()) {
        cerr << "ERROR: Cannot connect to Oracle database. "
             << "Property test requires a live database connection." << endl;
        return 1;
    }

    cout << "Running Property 1: Blockchain Save/Load Round-Trip "
         << "(min " << MIN_ITERATIONS << " iterations)..." << endl;

    // rc::check returns true if all tests pass
    bool passed = rc::check(
        "Blockchain Save/Load Round-Trip",
        [&dbManager]() {
            // Generate a random blockchain state
            GenBlockchainState state = *genBlockchainState();

            // --- BUILD source blockchain from generated state ---
            Blockchain* srcBlockchain = nullptr;
            HashTableBalance* srcBalances = nullptr;
            PriorityQueueTransaction* srcPool = nullptr;
            buildBlockchainFromState(state, srcBlockchain, srcBalances, srcPool);

            // --- SAVE to Oracle database ---
            bool saveOk = dbManager.saveBlockchain(
                state.chainName, srcBlockchain, srcBalances, srcPool);
            RC_ASSERT(saveOk);

            // --- LOAD into fresh structures ---
            Blockchain* dstBlockchain = new Blockchain(1);
            HashTableBalance* dstBalances = new HashTableBalance();
            BSTTransactionIndex* dstBst = new BSTTransactionIndex();

            bool loadOk = dbManager.loadBlockchain(
                state.chainName, dstBlockchain, dstBalances, dstBst);
            RC_ASSERT(loadOk);

            // ========== VERIFY ALL FIELDS ==========

            // 1. Difficulty
            RC_ASSERT(dstBlockchain->difficulty == state.difficulty);

            // 2. Block count and order
            RC_ASSERT(dstBlockchain->chain.getSize()
                      == (int)state.blocks.size());

            // 3. Block-by-block field comparison
            for (int i = 0; i < (int)state.blocks.size(); i++) {
                Block* srcBlock = srcBlockchain->chain.get(i);
                Block* dstBlock = dstBlockchain->chain.get(i);
                RC_ASSERT(srcBlock != nullptr);
                RC_ASSERT(dstBlock != nullptr);

                RC_ASSERT(dstBlock->index == srcBlock->index);
                RC_ASSERT(dstBlock->timestamp == srcBlock->timestamp);
                RC_ASSERT(dstBlock->nonce == srcBlock->nonce);
                RC_ASSERT(dstBlock->hash == srcBlock->hash);
                RC_ASSERT(dstBlock->prevHash == srcBlock->prevHash);
                RC_ASSERT(dstBlock->txCount == srcBlock->txCount);

                // 4. Transaction-by-transaction comparison
                for (int j = 0; j < srcBlock->txCount; j++) {
                    Transaction& srcTx = srcBlock->transactions[j];
                    Transaction& dstTx = dstBlock->transactions[j];

                    RC_ASSERT(dstTx.id == srcTx.id);
                    RC_ASSERT(dstTx.sender == srcTx.sender);
                    RC_ASSERT(dstTx.receiver == srcTx.receiver);
                    RC_ASSERT(amountsEqual(dstTx.amount, srcTx.amount));
                    RC_ASSERT(dstTx.metadata == srcTx.metadata);
                    RC_ASSERT(dstTx.signature == srcTx.signature);
                }
            }

            // 5. Account balances
            for (const auto& acct : state.balances) {
                RC_ASSERT(dstBalances->contains(acct.name));
                RC_ASSERT(amountsEqual(
                    dstBalances->get(acct.name), acct.balance));
            }

            // 6. Transaction::nextId preserved
            RC_ASSERT(Transaction::nextId == state.nextTxId);

            // 7. BST index correctness: every transaction ID resolves to
            //    the correct (blockIndex, txPosition) pair
            for (int i = 0; i < dstBlockchain->chain.getSize(); i++) {
                Block* block = dstBlockchain->chain.get(i);
                for (int j = 0; j < block->txCount; j++) {
                    int foundBlockIdx = -1, foundTxIdx = -1;
                    bool found = dstBst->search(
                        block->transactions[j].id,
                        foundBlockIdx, foundTxIdx);
                    RC_ASSERT(found);
                    RC_ASSERT(foundBlockIdx == i);
                    RC_ASSERT(foundTxIdx == j);
                }
            }

            // --- CLEANUP ---
            delete srcBlockchain;
            delete srcBalances;
            delete srcPool;
            delete dstBlockchain;
            delete dstBalances;
            delete dstBst;
        }
    );

    dbManager.disconnect();

    if (passed) {
        cout << "PASSED: Property 1 — Blockchain Save/Load Round-Trip "
             << "(" << MIN_ITERATIONS << "+ iterations)" << endl;
    } else {
        cout << "FAILED: Property 1 — Blockchain Save/Load Round-Trip" << endl;
    }

    return passed ? 0 : 1;
}
