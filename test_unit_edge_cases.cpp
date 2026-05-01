// Unit Tests: Error Conditions and Edge Cases
// **Validates: Requirements 5.8, 11.1, 11.2, 11.3, 12.1, 12.2**
//
// Tests:
//   1. loadBlockchain() with non-existent chain name returns false
//   2. saveBlockchain() followed by AUDIT_LOG containing a SAVE row
//   3. loadBlockchain() followed by AUDIT_LOG containing a LOAD row
//   4. clearPendingTransactions() followed by AUDIT_LOG containing a MINE row
//   5. CHAIN_STATISTICS reflects correct counts after save
//
// Compile example:
//   g++ -std=c++17 -o test_unit_edge_cases \
//       test_unit_edge_cases.cpp db_manager.cpp \
//       -I$ORACLE_HOME/sdk/include -L$ORACLE_HOME -locci -lclntsh
//
// Run:
//   ./test_unit_edge_cases

// Rename main() in main.cpp to avoid linker conflict with our test main()
#define main main_blockchain_app
#include "main.cpp"
#undef main

#include "db_manager.h"
#include "db_config.h"

#include <iostream>
#include <string>
#include <cmath>

// ==================== Simple Test Harness ====================

static int testsRun = 0;
static int testsPassed = 0;
static int testsFailed = 0;

static void reportResult(const std::string& testName, bool passed) {
    testsRun++;
    if (passed) {
        testsPassed++;
        std::cout << "  PASS: " << testName << std::endl;
    } else {
        testsFailed++;
        std::cout << "  FAIL: " << testName << std::endl;
    }
}

// ==================== Verification Connection ====================
// Separate OCCI connection for querying the database directly,
// independent of the DatabaseManager under test.

class VerificationConnection {
private:
    oracle::occi::Environment* env;
    oracle::occi::Connection* conn;

public:
    VerificationConnection() : env(nullptr), conn(nullptr) {}

    ~VerificationConnection() { disconnect(); }

    bool connect() {
        try {
            env = oracle::occi::Environment::createEnvironment();
            conn = env->createConnection(DB_USERNAME, DB_PASSWORD, DB_CONNECTION_STRING);
            return true;
        } catch (oracle::occi::SQLException&) {
            if (conn) { env->terminateConnection(conn); conn = nullptr; }
            if (env) { oracle::occi::Environment::terminateEnvironment(env); env = nullptr; }
            return false;
        }
    }

    void disconnect() {
        if (conn) { env->terminateConnection(conn); conn = nullptr; }
        if (env) { oracle::occi::Environment::terminateEnvironment(env); env = nullptr; }
    }

    // Get the current maximum LOG_ID from AUDIT_LOG (0 if table is empty).
    int getMaxLogId() {
        try {
            oracle::occi::Statement* stmt = conn->createStatement(
                "SELECT NVL(MAX(LOG_ID), 0) FROM AUDIT_LOG");
            oracle::occi::ResultSet* rs = stmt->executeQuery();

            int maxId = 0;
            if (rs->next()) {
                maxId = rs->getInt(1);
            }

            stmt->closeResultSet(rs);
            conn->terminateStatement(stmt);
            return maxId;
        } catch (oracle::occi::SQLException&) {
            return -1;
        }
    }

    // Check if an AUDIT_LOG row exists with LOG_ID > minLogId and matching operation and chain name.
    bool auditLogExists(int minLogId, const std::string& operation, const std::string& chainName) {
        try {
            oracle::occi::Statement* stmt = conn->createStatement(
                "SELECT COUNT(*) FROM AUDIT_LOG "
                "WHERE LOG_ID > :1 AND OPERATION = :2 AND CHAIN_NAME = :3");
            stmt->setInt(1, minLogId);
            stmt->setString(2, operation);
            stmt->setString(3, chainName);
            oracle::occi::ResultSet* rs = stmt->executeQuery();

            int count = 0;
            if (rs->next()) {
                count = rs->getInt(1);
            }

            stmt->closeResultSet(rs);
            conn->terminateStatement(stmt);
            return count > 0;
        } catch (oracle::occi::SQLException&) {
            return false;
        }
    }

    // Query CHAIN_STATISTICS for a given chain name. Returns true if found, sets output params.
    bool queryChainStatistics(const std::string& chainName,
                              int& outTotalBlocks,
                              int& outTotalTransactions,
                              int& outTotalPending) {
        try {
            oracle::occi::Statement* stmt = conn->createStatement(
                "SELECT CS.TOTAL_BLOCKS, CS.TOTAL_TRANSACTIONS, CS.TOTAL_PENDING "
                "FROM CHAIN_STATISTICS CS "
                "JOIN BLOCKCHAIN_META BM ON CS.CHAIN_ID = BM.CHAIN_ID "
                "WHERE BM.NAME = :1");
            stmt->setString(1, chainName);
            oracle::occi::ResultSet* rs = stmt->executeQuery();

            bool found = false;
            if (rs->next()) {
                outTotalBlocks = rs->getInt(1);
                outTotalTransactions = rs->getInt(2);
                outTotalPending = rs->getInt(3);
                found = true;
            }

            stmt->closeResultSet(rs);
            conn->terminateStatement(stmt);
            return found;
        } catch (oracle::occi::SQLException&) {
            return false;
        }
    }

    // Clean up a chain by name (delete from BLOCKCHAIN_META; CASCADE handles the rest).
    void cleanupChain(const std::string& chainName) {
        try {
            oracle::occi::Statement* stmt = conn->createStatement(
                "DELETE FROM BLOCKCHAIN_META WHERE NAME = :1");
            stmt->setString(1, chainName);
            stmt->executeUpdate();
            conn->terminateStatement(stmt);
            conn->commit();
        } catch (oracle::occi::SQLException&) {
            // Ignore cleanup errors
        }
    }
};

// ==================== Test 1: loadBlockchain() with non-existent chain returns false ====================
// Requirement 5.8: If the specified chain name does not exist, return false

static void testLoadNonExistentChain(DatabaseManager& dbm) {
    Blockchain bc(1);
    HashTableBalance balances;
    BSTTransactionIndex bst;

    bool result = dbm.loadBlockchain("nonexistent_chain_xyz_12345", &bc, &balances, &bst);
    reportResult("loadBlockchain() with non-existent chain returns false", result == false);
}

// ==================== Test 2: saveBlockchain() creates AUDIT_LOG SAVE row ====================
// Requirement 11.1: saveBlockchain() inserts AUDIT_LOG row with OPERATION='SAVE'

static void testSaveAuditLog(DatabaseManager& dbm, VerificationConnection& verifier) {
    std::string chainName = "test_audit_save_chain";

    // Clean up any previous test data
    verifier.cleanupChain(chainName);

    // Create a minimal blockchain with one transaction in a block
    Blockchain bc(1);
    HashTableBalance balances;
    balances.update("Alice", 1000.0);
    balances.update("Bob", 1000.0);
    PriorityQueueTransaction pool;

    // Record current max LOG_ID before the operation
    int prevMaxLogId = verifier.getMaxLogId();

    // Perform save
    bool saved = dbm.saveBlockchain(chainName, &bc, &balances, &pool);
    reportResult("saveBlockchain() returns true", saved == true);

    // Verify AUDIT_LOG contains a SAVE row after the operation
    bool auditFound = verifier.auditLogExists(prevMaxLogId, "SAVE", chainName);
    reportResult("AUDIT_LOG contains SAVE row after saveBlockchain()", auditFound);

    // Clean up
    verifier.cleanupChain(chainName);
}

// ==================== Test 3: loadBlockchain() creates AUDIT_LOG LOAD row ====================
// Requirement 11.2: loadBlockchain() inserts AUDIT_LOG row with OPERATION='LOAD'

static void testLoadAuditLog(DatabaseManager& dbm, VerificationConnection& verifier) {
    std::string chainName = "test_audit_load_chain";

    // Clean up any previous test data
    verifier.cleanupChain(chainName);

    // First, save a blockchain so we have something to load
    Blockchain bc(1);
    HashTableBalance balances;
    balances.update("Alice", 1000.0);
    PriorityQueueTransaction pool;

    bool saved = dbm.saveBlockchain(chainName, &bc, &balances, &pool);
    reportResult("saveBlockchain() for load test returns true", saved == true);

    // Record current max LOG_ID before the load operation
    int prevMaxLogId = verifier.getMaxLogId();

    // Perform load into fresh structures
    Blockchain bc2(1);
    HashTableBalance balances2;
    BSTTransactionIndex bst2;

    bool loaded = dbm.loadBlockchain(chainName, &bc2, &balances2, &bst2);
    reportResult("loadBlockchain() returns true", loaded == true);

    // Verify AUDIT_LOG contains a LOAD row after the operation
    bool auditFound = verifier.auditLogExists(prevMaxLogId, "LOAD", chainName);
    reportResult("AUDIT_LOG contains LOAD row after loadBlockchain()", auditFound);

    // Clean up
    verifier.cleanupChain(chainName);
}

// ==================== Test 4: clearPendingTransactions() creates AUDIT_LOG MINE row ====================
// Requirement 11.3: clearPendingTransactions() inserts AUDIT_LOG row with OPERATION='MINE'

static void testClearPendingAuditLog(DatabaseManager& dbm, VerificationConnection& verifier) {
    std::string chainName = "test_audit_mine_chain";

    // Record current max LOG_ID before the operation
    int prevMaxLogId = verifier.getMaxLogId();

    // Perform clearPendingTransactions
    bool cleared = dbm.clearPendingTransactions(chainName);
    reportResult("clearPendingTransactions() returns true", cleared == true);

    // Verify AUDIT_LOG contains a MINE row after the operation
    bool auditFound = verifier.auditLogExists(prevMaxLogId, "MINE", chainName);
    reportResult("AUDIT_LOG contains MINE row after clearPendingTransactions()", auditFound);
}

// ==================== Test 5: CHAIN_STATISTICS reflects correct counts after save ====================
// Requirements 12.1, 12.2: CHAIN_STATISTICS has correct block/transaction/pending counts

static void testChainStatistics(DatabaseManager& dbm, VerificationConnection& verifier) {
    std::string chainName = "test_stats_chain";

    // Clean up any previous test data
    verifier.cleanupChain(chainName);

    // Create a blockchain with known block/transaction counts:
    //   - Genesis block (0 transactions)
    //   - Block 1 with 2 transactions
    //   - Block 2 with 1 transaction
    // Total blocks = 3, total transactions = 3
    Blockchain bc(1);
    HashTableBalance balances;
    balances.update("Alice", 1000.0);
    balances.update("Bob", 1000.0);
    balances.update("Charlie", 1000.0);

    // Add block 1 with 2 transactions
    Transaction txs1[2];
    txs1[0] = Transaction("Alice", "Bob", 50.0, "tx1");
    txs1[1] = Transaction("Bob", "Charlie", 30.0, "tx2");
    bc.addBlock(txs1, 2);

    // Add block 2 with 1 transaction
    Transaction txs2[1];
    txs2[0] = Transaction("Charlie", "Alice", 10.0, "tx3");
    bc.addBlock(txs2, 1);

    // Create a pool with 1 pending transaction
    PriorityQueueTransaction pool;
    Transaction* pendingTx = new Transaction("Alice", "Bob", 5.0, "pending");
    pool.enqueue(pendingTx);

    // Save the blockchain
    bool saved = dbm.saveBlockchain(chainName, &bc, &balances, &pool);
    reportResult("saveBlockchain() for statistics test returns true", saved == true);

    // Query CHAIN_STATISTICS and verify counts
    int totalBlocks = 0, totalTransactions = 0, totalPending = 0;
    bool statsFound = verifier.queryChainStatistics(chainName, totalBlocks, totalTransactions, totalPending);

    reportResult("CHAIN_STATISTICS row exists for saved chain", statsFound);
    reportResult("CHAIN_STATISTICS TOTAL_BLOCKS = 3 (genesis + 2 mined)",
                 statsFound && totalBlocks == 3);
    reportResult("CHAIN_STATISTICS TOTAL_TRANSACTIONS = 3 (2 + 1)",
                 statsFound && totalTransactions == 3);
    reportResult("CHAIN_STATISTICS TOTAL_PENDING = 1",
                 statsFound && totalPending == 1);

    // Clean up
    verifier.cleanupChain(chainName);
}

// ==================== Main ====================

int main() {
    std::cout << "Running Unit Tests: Error Conditions and Edge Cases" << std::endl;
    std::cout << "====================================================" << std::endl;

    // Connect DatabaseManager (system under test)
    DatabaseManager dbm;
    if (!dbm.connect()) {
        std::cerr << "ERROR: Cannot connect to Oracle database." << std::endl;
        return 1;
    }

    // Connect verification connection
    VerificationConnection verifier;
    if (!verifier.connect()) {
        std::cerr << "ERROR: Cannot create verification connection." << std::endl;
        dbm.disconnect();
        return 1;
    }

    testLoadNonExistentChain(dbm);
    testSaveAuditLog(dbm, verifier);
    testLoadAuditLog(dbm, verifier);
    testClearPendingAuditLog(dbm, verifier);
    testChainStatistics(dbm, verifier);

    verifier.disconnect();
    dbm.disconnect();

    std::cout << std::endl;
    std::cout << "Results: " << testsPassed << " passed, "
              << testsFailed << " failed, "
              << testsRun << " total" << std::endl;

    return (testsFailed == 0) ? 0 : 1;
}
