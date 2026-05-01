# Implementation Plan: Oracle Database Migration

## Overview

Migrate the Blockchain Simulator's persistence layer from flat-file (.bc) storage to Oracle Database using OCCI. The implementation creates the Oracle DDL schema, a `db_config.h` header, a `DatabaseManager` class (header + implementation), and integrates database calls into the existing `main.cpp` menu loop. All existing in-memory data structures remain unchanged.

## Tasks

- [x] 1. Create Oracle DDL schema file
  - [x] 1.1 Create `schema.sql` with all nine UPPERCASE tables (ACCOUNTS, BLOCKCHAIN_META, MINERS, TRANSACTION_TYPES, BLOCKS, TRANSACTIONS, PENDING_TRANSACTIONS, AUDIT_LOG, CHAIN_STATISTICS)
    - Define each table with GENERATED ALWAYS AS IDENTITY primary keys
    - Add foreign keys with ON DELETE CASCADE on BLOCKS→BLOCKCHAIN_META, BLOCKS→MINERS, TRANSACTIONS→BLOCKS, TRANSACTIONS→TRANSACTION_TYPES, PENDING_TRANSACTIONS→TRANSACTION_TYPES, CHAIN_STATISTICS→BLOCKCHAIN_META
    - Add CHECK constraints: CHK_TX_AMOUNT (AMOUNT > 0), CHK_PENDING_AMOUNT (AMOUNT > 0), CHK_BLOCK_NONCE (NONCE >= 0), CHK_ACCOUNT_BALANCE (BALANCE >= 0)
    - Add UNIQUE constraints on ACCOUNTS.NAME, BLOCKCHAIN_META.NAME, MINERS.NAME, TRANSACTION_TYPES.TYPE_NAME
    - Use NUMBER(15,2) for all currency columns (BALANCE, AMOUNT)
    - Use UPPERCASE identifiers for all table names, column names, and constraint names
    - Include normalization comments (1NF, 2NF, 3NF compliance)
    - _Requirements: 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9, 1.10, 1.11, 1.12, 1.13, 1.14_

  - [x] 1.2 Add seed data INSERT statements for default TRANSACTION_TYPES and SYSTEM miner
    - Insert ('transfer', 'Standard transfer'), ('reward', 'Mining reward'), ('fee', 'Transaction fee') into TRANSACTION_TYPES
    - Insert ('SYSTEM', SYSTIMESTAMP) into MINERS
    - _Requirements: 1.15, 1.16_

- [x] 2. Create database configuration header
  - [x] 2.1 Create `db_config.h` with connection constants
    - Define DB_USERNAME, DB_PASSWORD, DB_CONNECTION_STRING as `const std::string`
    - Use include guards (#ifndef DB_CONFIG_H / #define DB_CONFIG_H / #endif)
    - Set placeholder values: "blockchain_user", "blockchain_pass", "//localhost:1521/XEPDB1"
    - _Requirements: 2.1, 2.2, 2.3_

- [x] 3. Create DatabaseManager class declaration
  - [x] 3.1 Create `db_manager.h` with the DatabaseManager class declaration
    - Include `<string>` and `<oracle/occi.h>`
    - Add forward declarations for Blockchain, HashTableBalance, PriorityQueueTransaction, BSTTransactionIndex, Transaction
    - Declare private members: `oracle::occi::Environment* env`, `oracle::occi::Connection* conn`
    - Declare private helpers: `getChainId(const std::string&)`, `getSystemMinerId()`, `getTransferTypeId()`
    - Declare public methods: constructor, destructor, connect(), disconnect(), saveBlockchain(), loadBlockchain(), savePendingTransaction(), loadPendingTransactions(), clearPendingTransactions(), updateBalance(), initializeDefaultAccounts()
    - Use include guards
    - _Requirements: 3.1, 3.2, 3.3, 3.4, 4.1–4.10, 5.1–5.9, 6.1–6.4, 7.1–7.4, 8.1, 8.2_

- [x] 4. Implement DatabaseManager — connection management and helpers
  - [x] 4.1 Create `db_manager.cpp` with includes, constructor, destructor, connect(), and disconnect()
    - Constructor initializes env and conn to nullptr
    - connect() creates OCCI Environment and opens Connection using db_config.h constants; returns bool
    - disconnect() terminates Connection then Environment with null checks; safe to call without prior connect
    - Wrap connect() in try/catch for oracle::occi::SQLException; call printError() on failure
    - _Requirements: 3.1, 3.2, 3.3, 3.4, 14.1, 14.2, 14.3_

  - [x] 4.2 Implement private helper methods: getChainId(), getSystemMinerId(), getTransferTypeId()
    - getChainId(): SELECT CHAIN_ID FROM BLOCKCHAIN_META WHERE NAME = :1
    - getSystemMinerId(): SELECT MINER_ID FROM MINERS WHERE NAME = 'SYSTEM'
    - getTransferTypeId(): SELECT TYPE_ID FROM TRANSACTION_TYPES WHERE TYPE_NAME = 'transfer'
    - All use prepared statements with positional bind variables
    - _Requirements: 8.1, 8.2, 9.2, 10.2_

- [x] 5. Checkpoint — Verify schema and connection layer compile
  - Ensure all files created so far compile (schema.sql is syntactically valid, db_config.h and db_manager.h/cpp compile with OCCI headers). Ask the user if questions arise.

- [x] 6. Implement DatabaseManager — saveBlockchain()
  - [x] 6.1 Implement saveBlockchain() method
    - MERGE INTO BLOCKCHAIN_META with chain name, difficulty, and Transaction::nextId
    - Retrieve CHAIN_ID via getChainId()
    - DELETE FROM BLOCKS WHERE CHAIN_ID = :1 (CASCADE removes child TRANSACTIONS)
    - Loop over LinkedListBlock: INSERT INTO BLOCKS with CHAIN_ID, MINER_ID (from getSystemMinerId()), BLOCK_INDEX, BLOCK_TIMESTAMP, NONCE, HASH, PREV_HASH using prepared statements
    - Retrieve generated BLOCK_ID for each block
    - Loop over each block's transactions: INSERT INTO TRANSACTIONS with BLOCK_ID, TYPE_ID (from getTransferTypeId()), TX_ID, SENDER, RECEIVER, AMOUNT, METADATA, SIGNATURE using prepared statements
    - DELETE FROM ACCOUNTS, then loop over HashTableBalance: INSERT INTO ACCOUNTS with NAME, BALANCE
    - INSERT INTO AUDIT_LOG with OPERATION='SAVE', CHAIN_NAME
    - MERGE INTO CHAIN_STATISTICS with total blocks, total transactions, total pending
    - conn->commit() on success, return true; catch SQLException, printError(), return false
    - All SQL uses positional bind variables — zero string concatenation of data values
    - _Requirements: 4.1, 4.2, 4.3, 4.4, 4.5, 4.6, 4.7, 4.8, 4.9, 4.10, 8.1, 8.2, 9.2, 10.2, 11.1, 12.1, 14.1, 14.2, 14.3_

  - [x] 6.2 Write property test for blockchain round-trip (Property 1)
    - **Property 1: Blockchain Save/Load Round-Trip**
    - Use RapidCheck to generate random chain name, difficulty (1–5), 1–10 blocks each with 0–5 transactions (random sender/receiver, positive amounts, random metadata/signatures), random account balances, random nextId
    - Save via saveBlockchain(), load into fresh structures via loadBlockchain(), compare all fields: difficulty, block count, block order, index, timestamp, nonce, hash, prevHash, transaction id/sender/receiver/amount/metadata/signature, account balances, Transaction::nextId, BST index correctness
    - Minimum 100 iterations
    - Tag: `// Feature: oracle-db-migration, Property 1: Blockchain Save/Load Round-Trip`
    - **Validates: Requirements 4.1, 4.2, 4.3, 4.4, 4.5, 4.6, 5.1, 5.2, 5.3, 5.4, 5.5, 5.6**

- [x] 7. Implement DatabaseManager — loadBlockchain()
  - [x] 7.1 Implement loadBlockchain() method
    - SELECT * FROM BLOCKCHAIN_META WHERE NAME = :1; if no row, printError and return false
    - Restore difficulty and Transaction::nextId from the result
    - Clear existing chain (LinkedListBlock) and BST
    - SELECT * FROM BLOCKS WHERE CHAIN_ID = :1 ORDER BY BLOCK_INDEX ASC
    - For each block row: SELECT * FROM TRANSACTIONS WHERE BLOCK_ID = :1 ORDER BY TX_ID ASC
    - Reconstruct Block objects with Transaction arrays, append to chain, insert into BST
    - SELECT * FROM ACCOUNTS; populate HashTableBalance via update()
    - INSERT INTO AUDIT_LOG with OPERATION='LOAD', CHAIN_NAME
    - conn->commit() on success, return true; catch SQLException, printError(), return false
    - All SQL uses positional bind variables
    - _Requirements: 5.1, 5.2, 5.3, 5.4, 5.5, 5.6, 5.7, 5.8, 5.9, 8.1, 8.2, 11.2, 14.1, 14.2, 14.3_

- [x] 8. Implement DatabaseManager — pending transaction operations
  - [x] 8.1 Implement savePendingTransaction(), loadPendingTransactions(), and clearPendingTransactions()
    - savePendingTransaction(): INSERT INTO PENDING_TRANSACTIONS with TYPE_ID (from getTransferTypeId()), SENDER, RECEIVER, AMOUNT, METADATA, SIGNATURE; commit
    - loadPendingTransactions(): SELECT * FROM PENDING_TRANSACTIONS; create Transaction objects and enqueue into PriorityQueueTransaction; commit
    - clearPendingTransactions(): DELETE FROM PENDING_TRANSACTIONS; INSERT INTO AUDIT_LOG with OPERATION='MINE', CHAIN_NAME; commit
    - All use prepared statements with positional bind variables
    - Catch SQLException in each method, printError()
    - _Requirements: 6.1, 6.2, 6.3, 6.4, 8.1, 8.2, 10.3, 11.3, 14.1, 14.2, 14.3_

  - [x] 8.2 Write property test for pending transaction round-trip (Property 2)
    - **Property 2: Pending Transaction Save/Load Round-Trip**
    - Use RapidCheck to generate 1–20 random transactions with random sender/receiver, positive amounts, random metadata/signatures
    - Save each via savePendingTransaction(), load all via loadPendingTransactions() into fresh PriorityQueueTransaction
    - Verify same set of transactions exists (order may differ due to priority queue); match by sender, receiver, amount, metadata, signature
    - Minimum 100 iterations
    - Tag: `// Feature: oracle-db-migration, Property 2: Pending Transaction Save/Load Round-Trip`
    - **Validates: Requirements 6.1, 6.2**

- [x] 9. Implement DatabaseManager — account balance operations
  - [x] 9.1 Implement updateBalance() and initializeDefaultAccounts()
    - updateBalance(): MERGE INTO ACCOUNTS using NAME as match key, setting BALANCE; use prepared statement with positional bind variables; commit
    - initializeDefaultAccounts(): call updateBalance() for Alice=1000, Bob=1000, Charlie=1000, David=500, Eve=750; also INSERT SYSTEM miner into MINERS if not exists; commit
    - Catch SQLException, printError()
    - _Requirements: 7.1, 7.2, 7.3, 7.4, 9.1, 14.1, 14.2, 14.3_

  - [x] 9.2 Write property test for balance upsert idempotence (Property 3)
    - **Property 3: Account Balance Upsert Idempotence**
    - Use RapidCheck to generate random account name (alphanumeric, 1–50 chars) and non-negative balance (0 to 999999.99)
    - Call updateBalance(name, balance), query ACCOUNTS for that name, verify balance matches
    - Call updateBalance(name, differentBalance), query again, verify only latest balance stored
    - Minimum 100 iterations
    - Tag: `// Feature: oracle-db-migration, Property 3: Account Balance Upsert Idempotence`
    - **Validates: Requirements 7.1, 7.2**

- [x] 10. Checkpoint — Verify all DatabaseManager methods compile and unit test basics
  - Ensure db_manager.cpp compiles with all methods implemented. Ensure all tests pass, ask the user if questions arise.

- [x] 11. Integrate DatabaseManager into main.cpp
  - [x] 11.1 Add DatabaseManager instance and connection setup in main()
    - Add `#include "db_manager.h"` at top of main.cpp
    - Create `DatabaseManager dbManager;` alongside existing pointers in main()
    - Call `dbManager.connect()` at startup; if it returns false, print error and continue (allow offline usage)
    - Call `dbManager.disconnect()` before exit in the cleanup section
    - _Requirements: 3.1, 3.2, 13.5, 15.1, 15.2, 15.3, 15.4, 15.5_

  - [x] 11.2 Update menu option 2 (Load Blockchain) to use DatabaseManager
    - Replace `bc->loadFromFile(filename, balances, bst)` with `dbManager.loadBlockchain(filename, bc, balances, bst)`
    - Preserve existing error handling flow (delete and null-out on failure)
    - _Requirements: 13.1_

  - [x] 11.3 Update menu option 3 (Save Blockchain) to use DatabaseManager
    - Replace `bc->saveToFile(filename, balances)` with `dbManager.saveBlockchain(filename, bc, balances, pool)`
    - Preserve existing success/error messages
    - _Requirements: 13.2_

  - [x] 11.4 Update menu option 4 (Add Transaction) to persist pending transaction
    - After `pool->enqueue(tx)`, add `dbManager.savePendingTransaction(*tx)`
    - _Requirements: 13.4_

  - [x] 11.5 Update menu option 5 (Mine Block) to clear pending and update balances
    - After mining and balance updates, call `dbManager.clearPendingTransactions(chainName)` where chainName is the current chain name
    - After each balance update in the mining loop, call `dbManager.updateBalance(accountName, newBalance)` for sender and receiver
    - _Requirements: 13.3_

- [x] 12. Write unit tests for DatabaseManager
  - [x] 12.1 Write unit tests for connection management
    - Test connect with valid credentials succeeds
    - Test connect with invalid credentials returns false
    - Test disconnect without connect is safe
    - Test double-disconnect is safe
    - _Requirements: 3.1, 3.2, 3.3, 3.4_

  - [x] 12.2 Write unit tests for default initialization and seed data
    - Test initializeDefaultAccounts() creates Alice=1000, Bob=1000, Charlie=1000, David=500, Eve=750
    - Test SYSTEM miner exists after initialization
    - Test TRANSACTION_TYPES has 3 rows (transfer, reward, fee)
    - _Requirements: 7.3, 9.1, 10.1_

  - [x] 12.3 Write unit tests for error conditions and edge cases
    - Test loadBlockchain() with non-existent chain name returns false
    - Test saveBlockchain() followed by AUDIT_LOG containing a SAVE row
    - Test loadBlockchain() followed by AUDIT_LOG containing a LOAD row
    - Test clearPendingTransactions() followed by AUDIT_LOG containing a MINE row
    - Test CHAIN_STATISTICS reflects correct counts after save
    - _Requirements: 5.8, 11.1, 11.2, 11.3, 12.1, 12.2_

- [x] 13. Final checkpoint — Ensure all tests pass
  - Ensure all tests pass, ask the user if questions arise.

## Notes

- Tasks marked with `*` are optional and can be skipped for faster MVP
- Each task references specific requirements for traceability
- Checkpoints ensure incremental validation
- Property tests validate universal correctness properties using RapidCheck
- Unit tests validate specific examples and edge cases
- All existing data structures (LinkedListBlock, PriorityQueueTransaction, BSTTransactionIndex, HashTableBalance, Block, Transaction) remain unmodified
