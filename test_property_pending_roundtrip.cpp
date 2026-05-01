// Feature: oracle-db-migration, Property 2: Pending Transaction Save/Load Round-Trip
// **Validates: Requirements 6.1, 6.2**
//
// This property test verifies that for any set of pending transactions,
// saving each via savePendingTransaction() and loading all via
// loadPendingTransactions() into a fresh PriorityQueueTransaction produces
// a queue containing exactly the same transactions (matched by sender,
// receiver, amount, metadata, signature), regardless of insertion order.
//
// Requires: live Oracle database connection, RapidCheck library, OCCI headers
//
// Compile example:
//   g++ -std=c++17 -o test_property_pending_roundtrip \
//       test_property_pending_roundtrip.cpp db_manager.cpp \
//       -I$ORACLE_HOME/sdk/include -L$ORACLE_HOME -locci -lclntsh \
//       -lrapidcheck
//
// Run with minimum 100 iterations (default):
//   ./test_property_pending_roundtrip
// Override iteration count via environment:
//   RC_PARAMS="max_success=200" ./test_property_pending_roundtrip

// Rename main() in main.cpp to avoid linker conflict with our test main()
#define main main_blockchain_app
#include "main.cpp"
#undef main

#include <rapidcheck.h>
#include <vector>
#include <cmath>
#include <algorithm>
#include <tuple>

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

// Amounts must be > 0 (CHECK constraint CHK_PENDING_AMOUNT) and fit in NUMBER(15,2).
static rc::Gen<double> genPositiveAmount() {
    return rc::gen::map(
        rc::gen::inRange(1, 100000000),
        [](int cents) { return static_cast<double>(cents) / 100.0; }
    );
}

// ==================== Structured type for a generated pending transaction ====================

struct GenPendingTx {
    string sender;
    string receiver;
    double amount;
    string metadata;
    string signature;
};

// ==================== Generator for a list of pending transactions ====================

static rc::Gen<vector<GenPendingTx>> genPendingTransactions() {
    return rc::gen::mapcat(
        rc::gen::inRange(1, 21),  // 1–20 transactions
        [](int count) {
            return rc::gen::container<vector<GenPendingTx>>(
                count,
                rc::gen::exec([]() {
                    GenPendingTx tx;
                    tx.sender = *genAlphaNumString(2, 15);
                    tx.receiver = *genAlphaNumString(2, 15);
                    tx.amount = *genPositiveAmount();
                    tx.metadata = *genAlphaNumString(0, 30);
                    tx.signature = "SIG" + to_string(
                        *rc::gen::inRange(1000, 999999));
                    return tx;
                })
            );
        }
    );
}

// ==================== Comparison key for set matching ====================
// We use a tuple of (sender, receiver, rounded_amount, metadata, signature)
// to match transactions as a multiset (duplicates are possible).

using TxKey = tuple<string, string, int, string, string>;

static TxKey makeTxKey(const string& sender, const string& receiver,
                       double amount, const string& metadata,
                       const string& signature) {
    // Round amount to cents for stable comparison with NUMBER(15,2)
    int cents = static_cast<int>(round(amount * 100.0));
    return make_tuple(sender, receiver, cents, metadata, signature);
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

    cout << "Running Property 2: Pending Transaction Save/Load Round-Trip "
         << "(min " << MIN_ITERATIONS << " iterations)..." << endl;

    bool passed = rc::check(
        "Pending Transaction Save/Load Round-Trip",
        [&dbManager]() {
            // --- CLEAR PENDING_TRANSACTIONS table for a clean state ---
            dbManager.clearPendingTransactions("__pbt_pending_roundtrip");

            // --- GENERATE random pending transactions ---
            vector<GenPendingTx> genTxs = *genPendingTransactions();

            // --- SAVE each transaction via savePendingTransaction() ---
            for (const auto& gtx : genTxs) {
                Transaction tx(gtx.sender, gtx.receiver, gtx.amount, gtx.metadata);
                tx.signature = gtx.signature;  // Override auto-generated signature
                bool saveOk = dbManager.savePendingTransaction(tx);
                RC_ASSERT(saveOk);
            }

            // --- LOAD all into a fresh PriorityQueueTransaction ---
            PriorityQueueTransaction* loadedPool = new PriorityQueueTransaction(100);
            bool loadOk = dbManager.loadPendingTransactions(loadedPool);
            RC_ASSERT(loadOk);

            // --- VERIFY same count ---
            RC_ASSERT(loadedPool->getSize() == (int)genTxs.size());

            // --- BUILD expected multiset from generated transactions ---
            vector<TxKey> expectedKeys;
            for (const auto& gtx : genTxs) {
                expectedKeys.push_back(
                    makeTxKey(gtx.sender, gtx.receiver, gtx.amount,
                              gtx.metadata, gtx.signature));
            }
            sort(expectedKeys.begin(), expectedKeys.end());

            // --- EXTRACT all transactions from loaded queue via dequeue() ---
            vector<TxKey> loadedKeys;
            while (!loadedPool->isEmpty()) {
                Transaction* tx = loadedPool->dequeue();
                loadedKeys.push_back(
                    makeTxKey(tx->sender, tx->receiver, tx->amount,
                              tx->metadata, tx->signature));
                delete tx;
            }
            sort(loadedKeys.begin(), loadedKeys.end());

            // --- COMPARE as sorted multisets ---
            RC_ASSERT(expectedKeys.size() == loadedKeys.size());
            for (size_t i = 0; i < expectedKeys.size(); i++) {
                RC_ASSERT(get<0>(expectedKeys[i]) == get<0>(loadedKeys[i]));  // sender
                RC_ASSERT(get<1>(expectedKeys[i]) == get<1>(loadedKeys[i]));  // receiver
                RC_ASSERT(get<2>(expectedKeys[i]) == get<2>(loadedKeys[i]));  // amount (cents)
                RC_ASSERT(get<3>(expectedKeys[i]) == get<3>(loadedKeys[i]));  // metadata
                RC_ASSERT(get<4>(expectedKeys[i]) == get<4>(loadedKeys[i]));  // signature
            }

            // --- CLEANUP ---
            delete loadedPool;
        }
    );

    dbManager.disconnect();

    if (passed) {
        cout << "PASSED: Property 2 — Pending Transaction Save/Load Round-Trip "
             << "(" << MIN_ITERATIONS << "+ iterations)" << endl;
    } else {
        cout << "FAILED: Property 2 — Pending Transaction Save/Load Round-Trip" << endl;
    }

    return passed ? 0 : 1;
}
