// Feature: oracle-db-migration, Property 3: Account Balance Upsert Idempotence
// **Validates: Requirements 7.1, 7.2**
//
// This property test verifies that for any account name and non-negative balance,
// calling updateBalance(name, balance) and then querying the ACCOUNTS table
// returns exactly the provided balance. Furthermore, calling updateBalance twice
// with the same name but different balances results in only the latest balance
// being stored.
//
// Requires: live Oracle database connection, RapidCheck library, OCCI headers
//
// Compile example:
//   g++ -std=c++17 -o test_property_balance_upsert \
//       test_property_balance_upsert.cpp db_manager.cpp \
//       -I$ORACLE_HOME/sdk/include -L$ORACLE_HOME -locci -lclntsh \
//       -lrapidcheck
//
// Run with minimum 100 iterations (default):
//   ./test_property_balance_upsert
// Override iteration count via environment:
//   RC_PARAMS="max_success=200" ./test_property_balance_upsert

// Rename main() in main.cpp to avoid linker conflict with our test main()
#define main main_blockchain_app
#include "main.cpp"
#undef main

#include <rapidcheck.h>
#include <vector>
#include <cmath>

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

// ==================== Verification Helper ====================
// Uses a separate OCCI connection to query ACCOUNTS directly,
// independent of the DatabaseManager under test.

class VerificationConnection {
private:
    oracle::occi::Environment* env;
    oracle::occi::Connection* conn;

public:
    VerificationConnection() : env(nullptr), conn(nullptr) {}

    ~VerificationConnection() {
        disconnect();
    }

    bool connect() {
        try {
            env = oracle::occi::Environment::createEnvironment();
            conn = env->createConnection(DB_USERNAME, DB_PASSWORD, DB_CONNECTION_STRING);
            return true;
        } catch (oracle::occi::SQLException& e) {
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

    // Count rows in ACCOUNTS matching a given name (should be 0 or 1).
    int countRows(const std::string& accountName) {
        try {
            oracle::occi::Statement* stmt = conn->createStatement(
                "SELECT COUNT(*) FROM ACCOUNTS WHERE NAME = :1");
            stmt->setString(1, accountName);
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

    // Delete a specific account by name (cleanup between iterations).
    void deleteAccount(const std::string& accountName) {
        try {
            oracle::occi::Statement* stmt = conn->createStatement(
                "DELETE FROM ACCOUNTS WHERE NAME = :1");
            stmt->setString(1, accountName);
            stmt->executeUpdate();
            conn->terminateStatement(stmt);
            conn->commit();
        } catch (oracle::occi::SQLException&) {
            // Ignore cleanup errors
        }
    }
};

// ==================== RapidCheck Generators ====================

// Alphanumeric string generator (1–50 chars for account names).
static rc::Gen<std::string> genAlphaNumString(int minLen, int maxLen) {
    return rc::gen::mapcat(
        rc::gen::inRange(minLen, maxLen + 1),
        [](int len) {
            return rc::gen::container<std::string>(
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

// Non-negative balance: 0 to 999999.99, generated as integer cents then divided.
// CHECK constraint CHK_ACCOUNT_BALANCE requires BALANCE >= 0.
static rc::Gen<double> genNonNegativeBalance() {
    return rc::gen::map(
        rc::gen::inRange(0, 100000000),  // 0 to 999999.99 in cents
        [](int cents) { return static_cast<double>(cents) / 100.0; }
    );
}

// ==================== MAIN: Property test with 100+ iterations ====================

int main() {
    // Establish DatabaseManager connection (system under test)
    DatabaseManager dbManager;
    if (!dbManager.connect()) {
        std::cerr << "ERROR: Cannot connect to Oracle database. "
                  << "Property test requires a live database connection." << std::endl;
        return 1;
    }

    // Establish separate verification connection
    VerificationConnection verifier;
    if (!verifier.connect()) {
        std::cerr << "ERROR: Cannot create verification connection to Oracle database." << std::endl;
        dbManager.disconnect();
        return 1;
    }

    std::cout << "Running Property 3: Account Balance Upsert Idempotence "
              << "(min " << MIN_ITERATIONS << " iterations)..." << std::endl;

    bool passed = rc::check(
        "Account Balance Upsert Idempotence",
        [&dbManager, &verifier]() {
            // --- GENERATE random account name (1–50 alphanumeric chars) ---
            std::string accountName = *genAlphaNumString(1, 50);

            // --- GENERATE two distinct non-negative balances ---
            double balance1 = *genNonNegativeBalance();
            double balance2 = *genNonNegativeBalance();

            // Ensure balance2 differs from balance1 for the upsert test
            if (amountsEqual(balance1, balance2)) {
                // Shift balance2 to guarantee a different value
                double shifted = balance2 + 1.01;
                if (shifted > 999999.99) {
                    shifted = (balance2 > 0.0) ? balance2 - 1.01 : 1.01;
                }
                balance2 = shifted;
            }

            // --- CLEANUP: remove any pre-existing row for this account name ---
            verifier.deleteAccount(accountName);

            // === STEP 1: Insert — call updateBalance with first balance ===
            bool ok1 = dbManager.updateBalance(accountName, balance1);
            RC_ASSERT(ok1);

            // Verify: query ACCOUNTS, balance should match balance1
            double queriedBalance = -1.0;
            bool found1 = verifier.queryBalance(accountName, queriedBalance);
            RC_ASSERT(found1);
            RC_ASSERT(amountsEqual(queriedBalance, balance1));

            // Verify: exactly one row for this account name
            int rowCount1 = verifier.countRows(accountName);
            RC_ASSERT(rowCount1 == 1);

            // === STEP 2: Update — call updateBalance with different balance ===
            bool ok2 = dbManager.updateBalance(accountName, balance2);
            RC_ASSERT(ok2);

            // Verify: query ACCOUNTS, balance should now match balance2 (latest)
            double queriedBalance2 = -1.0;
            bool found2 = verifier.queryBalance(accountName, queriedBalance2);
            RC_ASSERT(found2);
            RC_ASSERT(amountsEqual(queriedBalance2, balance2));

            // Verify: still exactly one row (upsert, not duplicate insert)
            int rowCount2 = verifier.countRows(accountName);
            RC_ASSERT(rowCount2 == 1);

            // --- CLEANUP: remove the test account ---
            verifier.deleteAccount(accountName);
        }
    );

    verifier.disconnect();
    dbManager.disconnect();

    if (passed) {
        std::cout << "PASSED: Property 3 — Account Balance Upsert Idempotence "
                  << "(" << MIN_ITERATIONS << "+ iterations)" << std::endl;
    } else {
        std::cout << "FAILED: Property 3 — Account Balance Upsert Idempotence" << std::endl;
    }

    return passed ? 0 : 1;
}
