// Unit Tests: Default Initialization and Seed Data
// **Validates: Requirements 7.3, 9.1, 10.1**
//
// Tests:
//   1. initializeDefaultAccounts() creates Alice=1000, Bob=1000, Charlie=1000, David=500, Eve=750
//   2. SYSTEM miner exists in MINERS after initialization
//   3. TRANSACTION_TYPES has 3 rows (transfer, reward, fee)
//
// Compile example:
//   g++ -std=c++17 -o test_unit_initialization \
//       test_unit_initialization.cpp db_manager.cpp \
//       -I$ORACLE_HOME/sdk/include -L$ORACLE_HOME -locci -lclntsh
//
// Run:
//   ./test_unit_initialization

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

    // Query ACCOUNTS for a given name. Returns true if found, sets outBalance.
    bool queryBalance(const std::string& accountName, double& outBalance) {
        try {
            oracle::occi::Statement* stmt = conn->createStatement(
                "SELECT BALANCE FROM ACCOUNTS WHERE NAME = :1");
            stmt->setString(1, accountName);
            oracle::occi::ResultSet* rs = stmt->executeQuery();

            bool found = false;
            if (rs->next()) {
                outBalance = rs->getDouble(1);
                found = true;
            }

            stmt->closeResultSet(rs);
            conn->terminateStatement(stmt);
            return found;
        } catch (oracle::occi::SQLException&) {
            return false;
        }
    }

    // Query MINERS for a given name. Returns true if found.
    bool queryMinerExists(const std::string& minerName) {
        try {
            oracle::occi::Statement* stmt = conn->createStatement(
                "SELECT COUNT(*) FROM MINERS WHERE NAME = :1");
            stmt->setString(1, minerName);
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

    // Query TRANSACTION_TYPES row count.
    int queryTransactionTypeCount() {
        try {
            oracle::occi::Statement* stmt = conn->createStatement(
                "SELECT COUNT(*) FROM TRANSACTION_TYPES");
            oracle::occi::ResultSet* rs = stmt->executeQuery();

            int count = 0;
            if (rs->next()) {
                count = rs->getInt(1);
            }

            stmt->closeResultSet(rs);
            conn->terminateStatement(stmt);
            return count;
        } catch (oracle::occi::SQLException&) {
            return -1;
        }
    }

    // Query whether a specific TRANSACTION_TYPES row exists by TYPE_NAME.
    bool queryTransactionTypeExists(const std::string& typeName) {
        try {
            oracle::occi::Statement* stmt = conn->createStatement(
                "SELECT COUNT(*) FROM TRANSACTION_TYPES WHERE TYPE_NAME = :1");
            stmt->setString(1, typeName);
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

    // Delete all rows from ACCOUNTS (cleanup before test).
    void clearAccounts() {
        try {
            oracle::occi::Statement* stmt = conn->createStatement(
                "DELETE FROM ACCOUNTS");
            stmt->executeUpdate();
            conn->terminateStatement(stmt);
            conn->commit();
        } catch (oracle::occi::SQLException&) {
            // Ignore cleanup errors
        }
    }
};

// ==================== Helper: Compare doubles with NUMBER(15,2) precision ====================
static bool balancesEqual(double a, double b) {
    double ra = round(a * 100.0) / 100.0;
    double rb = round(b * 100.0) / 100.0;
    return fabs(ra - rb) < 0.001;
}

// ==================== Test 1: initializeDefaultAccounts() creates correct balances ====================
// Requirement 7.3: initializeDefaultAccounts() inserts Alice=1000, Bob=1000, Charlie=1000, David=500, Eve=750

static void testInitializeDefaultAccounts(DatabaseManager& dbm, VerificationConnection& verifier) {
    // Clean up ACCOUNTS before test
    verifier.clearAccounts();

    // Call initializeDefaultAccounts()
    bool result = dbm.initializeDefaultAccounts();
    reportResult("initializeDefaultAccounts() returns true", result == true);

    // Verify each default account balance
    struct ExpectedAccount {
        std::string name;
        double balance;
    };

    ExpectedAccount expected[] = {
        {"Alice",   1000.0},
        {"Bob",     1000.0},
        {"Charlie", 1000.0},
        {"David",    500.0},
        {"Eve",      750.0}
    };

    for (int i = 0; i < 5; i++) {
        double queriedBalance = -1.0;
        bool found = verifier.queryBalance(expected[i].name, queriedBalance);
        bool correct = found && balancesEqual(queriedBalance, expected[i].balance);
        reportResult(expected[i].name + " has balance " +
                     std::to_string((int)expected[i].balance), correct);
    }
}

// ==================== Test 2: SYSTEM miner exists after initialization ====================
// Requirement 9.1: initializeDefaultAccounts() inserts SYSTEM miner if not already present

static void testSystemMinerExists(DatabaseManager& dbm, VerificationConnection& verifier) {
    // initializeDefaultAccounts() was already called in test 1, which also
    // does a MERGE for the SYSTEM miner. The SYSTEM miner is also seeded
    // by schema.sql. Either way, it should exist.
    bool exists = verifier.queryMinerExists("SYSTEM");
    reportResult("SYSTEM miner exists in MINERS table", exists);
}

// ==================== Test 3: TRANSACTION_TYPES has 3 rows ====================
// Requirement 10.1: Schema seeds transfer, reward, fee transaction types

static void testTransactionTypes(VerificationConnection& verifier) {
    int count = verifier.queryTransactionTypeCount();
    reportResult("TRANSACTION_TYPES has 3 rows", count == 3);

    bool hasTransfer = verifier.queryTransactionTypeExists("transfer");
    reportResult("TRANSACTION_TYPES contains 'transfer'", hasTransfer);

    bool hasReward = verifier.queryTransactionTypeExists("reward");
    reportResult("TRANSACTION_TYPES contains 'reward'", hasReward);

    bool hasFee = verifier.queryTransactionTypeExists("fee");
    reportResult("TRANSACTION_TYPES contains 'fee'", hasFee);
}

// ==================== Main ====================

int main() {
    std::cout << "Running Unit Tests: Default Initialization and Seed Data" << std::endl;
    std::cout << "========================================================" << std::endl;

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

    testInitializeDefaultAccounts(dbm, verifier);
    testSystemMinerExists(dbm, verifier);
    testTransactionTypes(verifier);

    verifier.disconnect();
    dbm.disconnect();

    std::cout << std::endl;
    std::cout << "Results: " << testsPassed << " passed, "
              << testsFailed << " failed, "
              << testsRun << " total" << std::endl;

    return (testsFailed == 0) ? 0 : 1;
}
