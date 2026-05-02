#include "db_manager.h"
#include "db_config.h"
#include <iostream>
#include <string>

using namespace std;
using namespace oracle::occi;

// Forward declaration of printError from main.cpp
extern void printError(const string& msg);

// ==================== Constructor / Destructor ====================

DatabaseManager::DatabaseManager() : env(nullptr), conn(nullptr) {
}

DatabaseManager::~DatabaseManager() {
    disconnect();
}

// ==================== Connection Management ====================

bool DatabaseManager::connect() {
    try {
        env = Environment::createEnvironment();
        conn = env->createConnection(DB_USERNAME, DB_PASSWORD, DB_CONNECTION_STRING);
        return true;
    } catch (SQLException& e) {
        printError("Database connection failed [" + to_string(e.getErrorCode()) + "]: " + e.getMessage());
        // Clean up partially created resources
        if (conn != nullptr) {
            env->terminateConnection(conn);
            conn = nullptr;
        }
        if (env != nullptr) {
            Environment::terminateEnvironment(env);
            env = nullptr;
        }
        return false;
    }
}

void DatabaseManager::disconnect() {
    if (conn != nullptr) {
        env->terminateConnection(conn);
        conn = nullptr;
    }
    if (env != nullptr) {
        Environment::terminateEnvironment(env);
        env = nullptr;
    }
}

// ==================== Private Helper Methods ====================

int DatabaseManager::getChainId(const string& chainName) {
    Statement* stmt = conn->createStatement(
        "SELECT CHAIN_ID FROM BLOCKCHAIN_META WHERE NAME = :1");
    stmt->setString(1, chainName);
    ResultSet* rs = stmt->executeQuery();

    int chainId = -1;
    if (rs->next()) {
        chainId = rs->getInt(1);
    }

    stmt->closeResultSet(rs);
    conn->terminateStatement(stmt);
    return chainId;
}

int DatabaseManager::getSystemMinerId() {
    Statement* stmt = conn->createStatement(
        "SELECT MINER_ID FROM MINERS WHERE NAME = 'SYSTEM'");
    ResultSet* rs = stmt->executeQuery();

    int minerId = -1;
    if (rs->next()) {
        minerId = rs->getInt(1);
    }

    stmt->closeResultSet(rs);
    conn->terminateStatement(stmt);
    return minerId;
}

int DatabaseManager::getTransferTypeId() {
    Statement* stmt = conn->createStatement(
        "SELECT TYPE_ID FROM TRANSACTION_TYPES WHERE TYPE_NAME = 'transfer'");
    ResultSet* rs = stmt->executeQuery();

    int typeId = -1;
    if (rs->next()) {
        typeId = rs->getInt(1);
    }

    stmt->closeResultSet(rs);
    conn->terminateStatement(stmt);
    return typeId;
}

// ==================== Blockchain Persistence ====================

bool DatabaseManager::saveBlockchain(const string& chainName,
                                     Blockchain* blockchain,
                                     HashTableBalance* balances,
                                     PriorityQueueTransaction* pool) {
    if (conn == nullptr) {
        printError("Not connected to database");
        return false;
    }

    try {
        // 1. MERGE INTO BLOCKCHAIN_META with chain name, difficulty, and Transaction::nextId
        Statement* stmt = conn->createStatement(
            "MERGE INTO BLOCKCHAIN_META dst "
            "USING (SELECT :1 AS NAME FROM DUAL) src "
            "ON (dst.NAME = src.NAME) "
            "WHEN MATCHED THEN UPDATE SET DIFFICULTY = :2, NEXT_TX_ID = :3 "
            "WHEN NOT MATCHED THEN INSERT (NAME, DIFFICULTY, NEXT_TX_ID) VALUES (:4, :5, :6)");
        stmt->setString(1, chainName);
        stmt->setInt(2, blockchain->difficulty);
        stmt->setInt(3, Transaction::nextId);
        stmt->setString(4, chainName);
        stmt->setInt(5, blockchain->difficulty);
        stmt->setInt(6, Transaction::nextId);
        stmt->executeUpdate();
        conn->terminateStatement(stmt);

        // 2. Retrieve CHAIN_ID
        int chainId = getChainId(chainName);
        if (chainId < 0) {
            printError("Failed to retrieve chain ID after merge");
            return false;
        }

        // 3. DELETE FROM BLOCKS WHERE CHAIN_ID = :1 (CASCADE removes child TRANSACTIONS)
        stmt = conn->createStatement("DELETE FROM BLOCKS WHERE CHAIN_ID = :1");
        stmt->setInt(1, chainId);
        stmt->executeUpdate();
        conn->terminateStatement(stmt);

        // 4. Look up SYSTEM miner ID and transfer type ID
        int minerId = getSystemMinerId();
        int typeId = getTransferTypeId();

        // 5. Count total transactions for statistics
        int totalTransactions = 0;

        // 6. Loop over LinkedListBlock: INSERT INTO BLOCKS
        NodeBlock* current = blockchain->chain.getHead();
        while (current != nullptr) {
            Block* block = current->block;

            // INSERT INTO BLOCKS with RETURNING BLOCK_ID INTO :8
            stmt = conn->createStatement(
                "INSERT INTO BLOCKS (CHAIN_ID, MINER_ID, BLOCK_INDEX, BLOCK_TIMESTAMP, NONCE, HASH, PREV_HASH) "
                "VALUES (:1, :2, :3, :4, :5, :6, :7) "
                "RETURNING BLOCK_ID INTO :8");
            stmt->setInt(1, chainId);
            stmt->setInt(2, minerId);
            stmt->setInt(3, block->index);
            stmt->setInt(4, (int)block->timestamp);
            stmt->setInt(5, block->nonce);
            stmt->setInt(6, block->hash);
            stmt->setInt(7, block->prevHash);
            stmt->registerOutParam(8, OCCIINT);
            stmt->executeUpdate();
            int blockId = stmt->getInt(8);
            conn->terminateStatement(stmt);

            // 7. Loop over each block's transactions: INSERT INTO TRANSACTIONS
            for (int i = 0; i < block->txCount; i++) {
                Transaction& tx = block->transactions[i];
                stmt = conn->createStatement(
                    "INSERT INTO TRANSACTIONS (BLOCK_ID, TYPE_ID, TX_ID, SENDER, RECEIVER, AMOUNT, METADATA, SIGNATURE) "
                    "VALUES (:1, :2, :3, :4, :5, :6, :7, :8)");
                stmt->setInt(1, blockId);
                stmt->setInt(2, typeId);
                stmt->setInt(3, tx.id);
                stmt->setString(4, tx.sender);
                stmt->setString(5, tx.receiver);
                stmt->setDouble(6, tx.amount);
                stmt->setString(7, tx.metadata);
                stmt->setString(8, tx.signature);
                stmt->executeUpdate();
                conn->terminateStatement(stmt);
                totalTransactions++;
            }

            current = current->next;
        }

        // 8. DELETE FROM ACCOUNTS, then loop over HashTableBalance: INSERT INTO ACCOUNTS
        stmt = conn->createStatement("DELETE FROM ACCOUNTS");
        stmt->executeUpdate();
        conn->terminateStatement(stmt);

        // Traverse all buckets of the hash table to save all accounts
        for (int i = 0; i < balances->TABLE_SIZE; i++) {
            NodeHash* node = balances->table[i];
            while (node != nullptr) {
                stmt = conn->createStatement(
                    "INSERT INTO ACCOUNTS (NAME, BALANCE) VALUES (:1, :2)");
                stmt->setString(1, node->key);
                stmt->setDouble(2, node->value);
                stmt->executeUpdate();
                conn->terminateStatement(stmt);
                node = node->next;
            }
        }

        // 9. INSERT INTO AUDIT_LOG with OPERATION='SAVE', CHAIN_NAME
        stmt = conn->createStatement(
            "INSERT INTO AUDIT_LOG (OPERATION, CHAIN_NAME) VALUES (:1, :2)");
        stmt->setString(1, "SAVE");
        stmt->setString(2, chainName);
        stmt->executeUpdate();
        conn->terminateStatement(stmt);

        // 10. MERGE INTO CHAIN_STATISTICS with total blocks, total transactions, total pending
        int totalBlocks = blockchain->chain.getSize();
        int totalPending = pool->getSize();

        stmt = conn->createStatement(
            "MERGE INTO CHAIN_STATISTICS dst "
            "USING (SELECT :1 AS CHAIN_ID FROM DUAL) src "
            "ON (dst.CHAIN_ID = src.CHAIN_ID) "
            "WHEN MATCHED THEN UPDATE SET TOTAL_BLOCKS = :2, TOTAL_TRANSACTIONS = :3, "
            "TOTAL_PENDING = :4, LAST_UPDATED = SYSTIMESTAMP "
            "WHEN NOT MATCHED THEN INSERT (CHAIN_ID, TOTAL_BLOCKS, TOTAL_TRANSACTIONS, TOTAL_PENDING) "
            "VALUES (:5, :6, :7, :8)");
        stmt->setInt(1, chainId);
        stmt->setInt(2, totalBlocks);
        stmt->setInt(3, totalTransactions);
        stmt->setInt(4, totalPending);
        stmt->setInt(5, chainId);
        stmt->setInt(6, totalBlocks);
        stmt->setInt(7, totalTransactions);
        stmt->setInt(8, totalPending);
        stmt->executeUpdate();
        conn->terminateStatement(stmt);

        // 11. Commit on success
        conn->commit();
        return true;

    } catch (SQLException& e) {
        printError("saveBlockchain failed [" + to_string(e.getErrorCode()) + "]: " + e.getMessage());
        return false;
    }
}

bool DatabaseManager::loadBlockchain(const string& chainName,
                                     Blockchain* blockchain,
                                     HashTableBalance* balances,
                                     BSTTransactionIndex* bst) {
    if (conn == nullptr) {
        printError("Not connected to database");
        return false;
    }

    try {
        // 1. Query BLOCKCHAIN_META for the chain
        Statement* stmt = conn->createStatement(
            "SELECT CHAIN_ID, DIFFICULTY, NEXT_TX_ID FROM BLOCKCHAIN_META WHERE NAME = :1");
        stmt->setString(1, chainName);
        ResultSet* rs = stmt->executeQuery();

        if (!rs->next()) {
            stmt->closeResultSet(rs);
            conn->terminateStatement(stmt);
            printError("Blockchain '" + chainName + "' not found in database");
            return false;
        }

        int chainId = rs->getInt(1);
        int difficulty = rs->getInt(2);
        int nextTxId = rs->getInt(3);
        stmt->closeResultSet(rs);
        conn->terminateStatement(stmt);

        // 2. Restore difficulty and Transaction::nextId
        blockchain->difficulty = difficulty;
        Transaction::nextId = nextTxId;

        // 3. Clear existing chain — manually traverse and delete nodes
        NodeBlock* current = blockchain->chain.getHead();
        while (current != nullptr) {
            NodeBlock* next = current->next;
            delete current->block;
            delete current;
            current = next;
        }
        blockchain->chain = LinkedListBlock();

        // 4. Clear BST
        bst->clear();

        // 5. Query blocks ordered by BLOCK_INDEX
        stmt = conn->createStatement(
            "SELECT BLOCK_ID, BLOCK_INDEX, BLOCK_TIMESTAMP, NONCE, HASH, PREV_HASH "
            "FROM BLOCKS WHERE CHAIN_ID = :1 ORDER BY BLOCK_INDEX ASC");
        stmt->setInt(1, chainId);
        rs = stmt->executeQuery();

        while (rs->next()) {
            int blockId = rs->getInt(1);
            int blockIndex = rs->getInt(2);
            int blockTimestamp = rs->getInt(3);
            int nonce = rs->getInt(4);
            int hash = rs->getInt(5);
            int prevHash = rs->getInt(6);

            // 6. For each block, query its transactions ordered by TX_ID
            Statement* txStmt = conn->createStatement(
                "SELECT TX_ID, SENDER, RECEIVER, AMOUNT, METADATA, SIGNATURE "
                "FROM TRANSACTIONS WHERE BLOCK_ID = :1 ORDER BY TX_ID ASC");
            txStmt->setInt(1, blockId);
            ResultSet* txRs = txStmt->executeQuery();

            // First pass: count transactions
            int txCount = 0;
            while (txRs->next()) {
                txCount++;
            }
            txStmt->closeResultSet(txRs);

            // Second pass: read transaction data
            Transaction* txArray = nullptr;
            if (txCount > 0) {
                txArray = new Transaction[txCount];
                txRs = txStmt->executeQuery();
                int txIdx = 0;
                while (txRs->next()) {
                    txArray[txIdx].id = txRs->getInt(1);
                    txArray[txIdx].sender = txRs->getString(2);
                    txArray[txIdx].receiver = txRs->getString(3);
                    txArray[txIdx].amount = txRs->getDouble(4);
                    txArray[txIdx].metadata = txRs->getString(5);
                    txArray[txIdx].signature = txRs->getString(6);
                    txIdx++;
                }
                txStmt->closeResultSet(txRs);
            }
            conn->terminateStatement(txStmt);

            // 7. Reconstruct Block object using default constructor and set fields directly
            Block* block = new Block();
            block->index = blockIndex;
            block->timestamp = (long)blockTimestamp;
            block->nonce = nonce;
            block->hash = hash;
            block->prevHash = prevHash;
            block->txCount = txCount;
            block->transactions = txArray;

            // 8. Append block to chain
            blockchain->chain.append(block);

            // 9. Insert transactions into BST
            for (int i = 0; i < txCount; i++) {
                bst->insert(txArray[i].id, blockIndex, i);
            }
        }

        stmt->closeResultSet(rs);
        conn->terminateStatement(stmt);

        // 10. Clear existing balances and load from ACCOUNTS
        for (int i = 0; i < balances->TABLE_SIZE; i++) {
            NodeHash* node = balances->table[i];
            while (node != nullptr) {
                NodeHash* next = node->next;
                delete node;
                node = next;
            }
            balances->table[i] = nullptr;
        }

        stmt = conn->createStatement("SELECT NAME, BALANCE FROM ACCOUNTS");
        rs = stmt->executeQuery();
        while (rs->next()) {
            string name = rs->getString(1);
            double balance = rs->getDouble(2);
            balances->update(name, balance);
        }
        stmt->closeResultSet(rs);
        conn->terminateStatement(stmt);

        // 11. INSERT INTO AUDIT_LOG with OPERATION='LOAD', CHAIN_NAME
        stmt = conn->createStatement(
            "INSERT INTO AUDIT_LOG (OPERATION, CHAIN_NAME) VALUES (:1, :2)");
        stmt->setString(1, "LOAD");
        stmt->setString(2, chainName);
        stmt->executeUpdate();
        conn->terminateStatement(stmt);

        // 12. Commit on success
        conn->commit();
        return true;

    } catch (SQLException& e) {
        printError("loadBlockchain failed [" + to_string(e.getErrorCode()) + "]: " + e.getMessage());
        return false;
    }
}

bool DatabaseManager::savePendingTransaction(const Transaction& tx) {
    if (conn == nullptr) {
        printError("Not connected to database");
        return false;
    }

    try {
        int typeId = getTransferTypeId();

        Statement* stmt = conn->createStatement(
            "INSERT INTO PENDING_TRANSACTIONS (TYPE_ID, SENDER, RECEIVER, AMOUNT, METADATA, SIGNATURE) "
            "VALUES (:1, :2, :3, :4, :5, :6)");
        stmt->setInt(1, typeId);
        stmt->setString(2, tx.sender);
        stmt->setString(3, tx.receiver);
        stmt->setDouble(4, tx.amount);
        stmt->setString(5, tx.metadata);
        stmt->setString(6, tx.signature);
        stmt->executeUpdate();
        conn->terminateStatement(stmt);

        conn->commit();
        return true;

    } catch (SQLException& e) {
        printError("savePendingTransaction failed [" + to_string(e.getErrorCode()) + "]: " + e.getMessage());
        return false;
    }
}

bool DatabaseManager::loadPendingTransactions(PriorityQueueTransaction* pool) {
    if (conn == nullptr) {
        printError("Not connected to database");
        return false;
    }

    try {
        Statement* stmt = conn->createStatement(
            "SELECT SENDER, RECEIVER, AMOUNT, METADATA, SIGNATURE FROM PENDING_TRANSACTIONS");
        ResultSet* rs = stmt->executeQuery();

        while (rs->next()) {
            string sender = rs->getString(1);
            string receiver = rs->getString(2);
            double amount = rs->getDouble(3);
            string metadata = rs->getString(4);
            string signature = rs->getString(5);

            Transaction* tx = new Transaction(sender, receiver, amount, metadata);
            tx->signature = signature;
            pool->enqueue(tx);
        }

        stmt->closeResultSet(rs);
        conn->terminateStatement(stmt);

        conn->commit();
        return true;

    } catch (SQLException& e) {
        printError("loadPendingTransactions failed [" + to_string(e.getErrorCode()) + "]: " + e.getMessage());
        return false;
    }
}

bool DatabaseManager::clearPendingTransactions(const string& chainName) {
    if (conn == nullptr) {
        printError("Not connected to database");
        return false;
    }

    try {
        Statement* stmt = conn->createStatement(
            "DELETE FROM PENDING_TRANSACTIONS");
        stmt->executeUpdate();
        conn->terminateStatement(stmt);

        stmt = conn->createStatement(
            "INSERT INTO AUDIT_LOG (OPERATION, CHAIN_NAME) VALUES (:1, :2)");
        stmt->setString(1, "MINE");
        stmt->setString(2, chainName);
        stmt->executeUpdate();
        conn->terminateStatement(stmt);

        conn->commit();
        return true;

    } catch (SQLException& e) {
        printError("clearPendingTransactions failed [" + to_string(e.getErrorCode()) + "]: " + e.getMessage());
        return false;
    }
}

bool DatabaseManager::updateBalance(const string& accountName, double balance) {
    if (conn == nullptr) {
        printError("Not connected to database");
        return false;
    }

    try {
        Statement* stmt = conn->createStatement(
            "MERGE INTO ACCOUNTS dst "
            "USING (SELECT :1 AS NAME FROM DUAL) src "
            "ON (dst.NAME = src.NAME) "
            "WHEN MATCHED THEN UPDATE SET BALANCE = :2 "
            "WHEN NOT MATCHED THEN INSERT (NAME, BALANCE) VALUES (:3, :4)");
        stmt->setString(1, accountName);
        stmt->setDouble(2, balance);
        stmt->setString(3, accountName);
        stmt->setDouble(4, balance);
        stmt->executeUpdate();
        conn->terminateStatement(stmt);

        conn->commit();
        return true;

    } catch (SQLException& e) {
        printError("updateBalance failed [" + to_string(e.getErrorCode()) + "]: " + e.getMessage());
        return false;
    }
}

bool DatabaseManager::initializeDefaultAccounts() {
    if (conn == nullptr) {
        printError("Not connected to database");
        return false;
    }

    try {
        // Initialize default account balances
        updateBalance("Alice", 1000.0);
        updateBalance("Bob", 1000.0);
        updateBalance("Charlie", 1000.0);
        updateBalance("David", 500.0);
        updateBalance("Eve", 750.0);

        // Insert SYSTEM miner if not already present
        Statement* stmt = conn->createStatement(
            "MERGE INTO MINERS dst "
            "USING (SELECT 'SYSTEM' AS NAME FROM DUAL) src "
            "ON (dst.NAME = src.NAME) "
            "WHEN NOT MATCHED THEN INSERT (NAME, REGISTERED_AT) VALUES ('SYSTEM', SYSTIMESTAMP)");
        stmt->executeUpdate();
        conn->terminateStatement(stmt);

        conn->commit();
        return true;

    } catch (SQLException& e) {
        printError("initializeDefaultAccounts failed [" + to_string(e.getErrorCode()) + "]: " + e.getMessage());
        return false;
    }
}
