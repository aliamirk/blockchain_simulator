# Requirements Document

## Introduction

This feature migrates the Blockchain Simulator's persistence layer from flat-file (.bc) storage to an Oracle Database using the Oracle C++ Call Interface (OCCI). All existing in-memory data structures (LinkedListBlock, PriorityQueueTransaction, BSTTransactionIndex, HashTableBalance) remain unchanged. The database replaces only the file-based save/load operations, providing a normalized relational schema across nine tables covering six core entities: Accounts, Chains, Blocks, Transactions, Miners, and Transaction Types, plus audit logging and chain statistics.

## Glossary

- **Blockchain_Simulator**: The existing C++ application that manages blocks, transactions, account balances, and a pending transaction pool via a console menu
- **DatabaseManager**: A new C++ class responsible for all Oracle database interactions using OCCI
- **OCCI**: Oracle C++ Call Interface — the Oracle-provided C++ library for database connectivity
- **Prepared_Statement**: A parameterized SQL statement pre-compiled by Oracle to prevent SQL injection and improve performance
- **Schema**: The set of Oracle DDL objects (tables, sequences, triggers, constraints) that store blockchain data
- **Block**: A data record containing an index, timestamp, transactions, nonce, hash, and previous hash
- **Transaction**: A transfer record with sender, receiver, amount, metadata, and signature fields
- **Pending_Transaction**: A transaction that has been submitted but not yet mined into a block
- **Account**: A named entity with a currency balance tracked in the ACCOUNTS table
- **Chain_Metadata**: Per-chain configuration data including difficulty and creation timestamp stored in BLOCKCHAIN_META
- **Miner**: An entity representing the participant who mined a block, stored in the MINERS table
- **Transaction_Type**: A categorization of transactions (transfer, reward, fee) stored in the TRANSACTION_TYPES lookup table
- **Audit_Log**: A record of database operations (save, load, mine events) stored in the AUDIT_LOG table
- **Chain_Statistics**: Aggregated per-chain metrics (total blocks, total transactions, last save timestamp) stored in the CHAIN_STATISTICS table
- **Persistence_Layer**: The subsystem responsible for saving and loading application state to/from durable storage
- **Genesis_Block**: The first block in a blockchain (index 0) with no transactions and a zero previous hash
- **BST_Index**: The BSTTransactionIndex in-memory structure rebuilt from database-loaded blocks

## Requirements

### Requirement 1: Oracle Schema Design

**User Story:** As a database administrator, I want a normalized Oracle schema with a minimum of nine tables covering six entities, so that data integrity is enforced at the database level and redundancy is eliminated.

#### Acceptance Criteria

1. THE Schema SHALL define an ACCOUNTS table with a numeric primary key, a unique VARCHAR2 account name, a NUMBER(15,2) balance column, and a TIMESTAMP column recording when the account was created
2. THE Schema SHALL define a BLOCKCHAIN_META table with a numeric primary key, a unique VARCHAR2 chain name, a NUMBER difficulty column, a NUMBER column for the next transaction ID counter, and a TIMESTAMP creation column
3. THE Schema SHALL define a MINERS table with a numeric primary key, a unique VARCHAR2 miner name, and a TIMESTAMP column recording when the miner was registered
4. THE Schema SHALL define a TRANSACTION_TYPES table with a numeric primary key, a unique VARCHAR2 type name, and a VARCHAR2 description column
5. THE Schema SHALL define a BLOCKS table with a numeric primary key, a foreign key referencing BLOCKCHAIN_META, a foreign key referencing MINERS, and columns for block index, timestamp, nonce, hash, and previous hash
6. THE Schema SHALL define a TRANSACTIONS table with a numeric primary key, a foreign key referencing BLOCKS, a foreign key referencing TRANSACTION_TYPES, and columns for transaction ID, sender, receiver, amount as NUMBER(15,2), metadata, and signature
7. THE Schema SHALL define a PENDING_TRANSACTIONS table with a numeric primary key, a foreign key referencing TRANSACTION_TYPES, and columns for sender, receiver, amount as NUMBER(15,2), metadata, and signature
8. THE Schema SHALL define an AUDIT_LOG table with a numeric primary key, a VARCHAR2 operation type column, a VARCHAR2 chain name column, a VARCHAR2 details column, and a TIMESTAMP column defaulting to the current system timestamp
9. THE Schema SHALL define a CHAIN_STATISTICS table with a numeric primary key, a foreign key referencing BLOCKCHAIN_META, and NUMBER columns for total blocks, total transactions, total pending transactions, and a TIMESTAMP column for the last updated time
10. THE Schema SHALL enforce ON DELETE CASCADE on all foreign key relationships so that deleting a chain removes its blocks, transactions, and statistics
11. THE Schema SHALL include CHECK constraints enforcing that transaction amounts are greater than zero, block nonce values are greater than or equal to zero, and account balances are greater than or equal to zero
12. THE Schema SHALL generate all primary keys using GENERATED ALWAYS AS IDENTITY
13. THE Schema SHALL satisfy first normal form by storing only atomic values with no repeating groups, second normal form by ensuring every non-key column depends on the entire primary key, and third normal form by eliminating transitive dependencies
14. THE Schema SHALL use UPPERCASE identifiers for all table names, column names, and constraint names
15. THE Schema SHALL seed the TRANSACTION_TYPES table with default rows for transfer, reward, and fee transaction types
16. THE Schema SHALL seed the MINERS table with a default SYSTEM miner used for genesis blocks and automated operations

### Requirement 2: Database Connection Configuration

**User Story:** As a developer, I want database connection parameters defined as named constants in a header file, so that I can change Oracle connection details without modifying application logic.

#### Acceptance Criteria

1. THE DatabaseManager SHALL read connection parameters (username, password, connection string) from named constants defined in db_config.h
2. THE db_config.h file SHALL define placeholder values for DB_USERNAME, DB_PASSWORD, and DB_CONNECTION_STRING constants
3. THE db_config.h file SHALL use include guards to prevent multiple inclusion

### Requirement 3: Database Connection Management

**User Story:** As a developer, I want the DatabaseManager to manage Oracle connections reliably, so that the application can connect to and disconnect from the database without resource leaks.

#### Acceptance Criteria

1. WHEN connect() is called, THE DatabaseManager SHALL create an OCCI Environment, open a Connection using the configured parameters, and store both for reuse
2. WHEN disconnect() is called, THE DatabaseManager SHALL terminate the Connection and the Environment in the correct order
3. IF an OCCI SQLException occurs during connect(), THEN THE DatabaseManager SHALL print the error code and message using a printError() utility and return false
4. IF disconnect() is called when no connection exists, THEN THE DatabaseManager SHALL complete without error

### Requirement 4: Save Blockchain to Database

**User Story:** As a user, I want to save the current blockchain state to Oracle, so that my blockchain data persists across application sessions.

#### Acceptance Criteria

1. WHEN saveBlockchain() is called, THE DatabaseManager SHALL insert or update a row in BLOCKCHAIN_META with the chain name and difficulty
2. WHEN saveBlockchain() is called, THE DatabaseManager SHALL delete existing blocks and transactions for the chain before inserting current data, ensuring a clean save
3. WHEN saveBlockchain() is called, THE DatabaseManager SHALL iterate over all blocks in the LinkedListBlock and insert each block into the BLOCKS table using a prepared statement, linking each block to the default SYSTEM miner
4. WHEN saveBlockchain() is called, THE DatabaseManager SHALL iterate over all transactions within each block and insert each transaction into the TRANSACTIONS table using a prepared statement, assigning the default transfer transaction type
5. WHEN saveBlockchain() is called, THE DatabaseManager SHALL save the current Transaction::nextId value into the BLOCKCHAIN_META row so that it can be restored on load
6. WHEN saveBlockchain() is called, THE DatabaseManager SHALL save all account balances from the HashTableBalance into the ACCOUNTS table using prepared statements
7. WHEN saveBlockchain() is called, THE DatabaseManager SHALL insert a row into the AUDIT_LOG table recording the save operation with the chain name and a timestamp
8. WHEN saveBlockchain() is called, THE DatabaseManager SHALL update the CHAIN_STATISTICS table with the current total blocks, total transactions, and total pending transactions for the chain
9. IF an OCCI SQLException occurs during saveBlockchain(), THEN THE DatabaseManager SHALL print the error details and return false
10. WHEN saveBlockchain() completes successfully, THE DatabaseManager SHALL commit the transaction and return true

### Requirement 5: Load Blockchain from Database

**User Story:** As a user, I want to load a previously saved blockchain from Oracle, so that I can resume working with my blockchain data.

#### Acceptance Criteria

1. WHEN loadBlockchain() is called with a chain name, THE DatabaseManager SHALL query BLOCKCHAIN_META to retrieve the difficulty and verify the chain exists
2. WHEN loadBlockchain() is called, THE DatabaseManager SHALL query all blocks for the chain ordered by block index ascending and reconstruct each Block object
3. WHEN loadBlockchain() is called, THE DatabaseManager SHALL query all transactions for each block and populate the Block's transaction array
4. WHEN loadBlockchain() is called, THE DatabaseManager SHALL load all account balances from the ACCOUNTS table into the HashTableBalance
5. WHEN loadBlockchain() is called, THE DatabaseManager SHALL restore the Transaction::nextId static counter from the saved value in BLOCKCHAIN_META
6. WHEN loadBlockchain() is called, THE DatabaseManager SHALL rebuild the BSTTransactionIndex from the loaded blocks
7. WHEN loadBlockchain() is called, THE DatabaseManager SHALL insert a row into the AUDIT_LOG table recording the load operation with the chain name and a timestamp
8. IF the specified chain name does not exist in BLOCKCHAIN_META, THEN THE DatabaseManager SHALL print an error message and return false
9. IF an OCCI SQLException occurs during loadBlockchain(), THEN THE DatabaseManager SHALL print the error details and return false

### Requirement 6: Pending Transaction Persistence

**User Story:** As a user, I want pending transactions saved to the database, so that unmined transactions are not lost between sessions.

#### Acceptance Criteria

1. WHEN savePendingTransaction() is called with a Transaction object, THE DatabaseManager SHALL insert the transaction into the PENDING_TRANSACTIONS table using a prepared statement
2. WHEN loadPendingTransactions() is called, THE DatabaseManager SHALL query all rows from PENDING_TRANSACTIONS and enqueue each into the PriorityQueueTransaction
3. WHEN clearPendingTransactions() is called, THE DatabaseManager SHALL delete all rows from the PENDING_TRANSACTIONS table and commit the change
4. IF an OCCI SQLException occurs during any pending transaction operation, THEN THE DatabaseManager SHALL print the error details

### Requirement 7: Account Balance Updates

**User Story:** As a user, I want account balances updated in the database after mining, so that the persisted balances reflect the latest state.

#### Acceptance Criteria

1. WHEN updateBalance() is called with an account name and new balance, THE DatabaseManager SHALL update the corresponding row in the ACCOUNTS table using a prepared statement
2. IF the account does not exist in the ACCOUNTS table, THEN THE DatabaseManager SHALL insert a new row with the account name and balance
3. WHEN initializeDefaultAccounts() is called, THE DatabaseManager SHALL insert the five default accounts (Alice=1000, Bob=1000, Charlie=1000, David=500, Eve=750) into the ACCOUNTS table
4. IF an OCCI SQLException occurs during any balance operation, THEN THE DatabaseManager SHALL print the error details

### Requirement 8: SQL Injection Prevention

**User Story:** As a developer, I want all database queries to use parameterized statements, so that the application is protected against SQL injection attacks.

#### Acceptance Criteria

1. THE DatabaseManager SHALL use OCCI prepared statements with bind variables for every SQL operation that includes user-supplied or application-supplied data
2. THE DatabaseManager SHALL construct zero SQL statements through string concatenation of data values

### Requirement 9: Miner Registration

**User Story:** As a developer, I want miners tracked as a separate entity in the database, so that each mined block can be attributed to a specific miner.

#### Acceptance Criteria

1. WHEN initializeDefaultAccounts() is called, THE DatabaseManager SHALL insert a SYSTEM miner into the MINERS table if one does not already exist
2. WHEN saveBlockchain() inserts a block, THE DatabaseManager SHALL associate the block with the SYSTEM miner by referencing the MINERS foreign key
3. THE MINERS table SHALL enforce a unique constraint on the miner name to prevent duplicate registrations

### Requirement 10: Transaction Type Classification

**User Story:** As a developer, I want transactions categorized by type, so that different kinds of transactions (transfers, rewards, fees) are distinguishable in the database.

#### Acceptance Criteria

1. THE Schema SHALL seed the TRANSACTION_TYPES table with three default rows: transfer, reward, and fee
2. WHEN saveBlockchain() inserts a transaction, THE DatabaseManager SHALL assign the transfer type by default
3. WHEN savePendingTransaction() inserts a pending transaction, THE DatabaseManager SHALL assign the transfer type by default
4. THE TRANSACTION_TYPES table SHALL enforce a unique constraint on the type name to prevent duplicate entries

### Requirement 11: Audit Logging

**User Story:** As an administrator, I want all significant database operations logged, so that I can track when blockchains were saved, loaded, or mined.

#### Acceptance Criteria

1. WHEN saveBlockchain() completes successfully, THE DatabaseManager SHALL insert a row into AUDIT_LOG with operation type SAVE and the chain name
2. WHEN loadBlockchain() completes successfully, THE DatabaseManager SHALL insert a row into AUDIT_LOG with operation type LOAD and the chain name
3. WHEN clearPendingTransactions() is called after mining, THE DatabaseManager SHALL insert a row into AUDIT_LOG with operation type MINE and the chain name
4. THE AUDIT_LOG table SHALL automatically set the timestamp to the current system time using a DEFAULT value

### Requirement 12: Chain Statistics Tracking

**User Story:** As a user, I want aggregated statistics for each blockchain stored in the database, so that I can quickly see chain metrics without recalculating from raw data.

#### Acceptance Criteria

1. WHEN saveBlockchain() completes, THE DatabaseManager SHALL insert or update a row in CHAIN_STATISTICS with the total block count, total mined transaction count, and total pending transaction count for the chain
2. THE CHAIN_STATISTICS table SHALL record a LAST_UPDATED timestamp each time statistics are refreshed
3. THE CHAIN_STATISTICS table SHALL reference BLOCKCHAIN_META via a foreign key with ON DELETE CASCADE

### Requirement 13: Main Menu Integration

**User Story:** As a user, I want the menu options to use the database instead of flat files, so that I interact with Oracle storage seamlessly through the existing interface.

#### Acceptance Criteria

1. WHEN menu option 2 (Load Blockchain) is selected, THE Blockchain_Simulator SHALL call DatabaseManager::loadBlockchain() instead of the file-based loadFromFile() method
2. WHEN menu option 3 (Save Blockchain) is selected, THE Blockchain_Simulator SHALL call DatabaseManager::saveBlockchain() instead of the file-based saveToFile() method
3. WHEN menu option 5 (Mine Pending Transactions) completes mining, THE Blockchain_Simulator SHALL call DatabaseManager::clearPendingTransactions() and DatabaseManager::updateBalance() for each affected account
4. WHEN menu option 4 (Add Transaction to Pool) adds a transaction, THE Blockchain_Simulator SHALL call DatabaseManager::savePendingTransaction() to persist the pending transaction
5. THE Blockchain_Simulator SHALL preserve all existing in-memory data structures and menu operations without modification to their class definitions

### Requirement 14: Error Handling

**User Story:** As a developer, I want consistent error handling for all database operations, so that failures are reported clearly and the application remains stable.

#### Acceptance Criteria

1. WHEN an oracle::occi::SQLException is caught, THE DatabaseManager SHALL extract the error code and error message and pass them to a printError() utility function
2. THE DatabaseManager SHALL catch SQLException at the boundary of every public method so that exceptions do not propagate to calling code
3. IF a database operation fails, THEN THE DatabaseManager SHALL return a boolean false or equivalent failure indicator to the caller

### Requirement 15: Existing Data Structure Preservation

**User Story:** As a developer, I want all existing in-memory data structures to remain unchanged, so that the migration only affects the persistence layer.

#### Acceptance Criteria

1. THE Blockchain_Simulator SHALL retain the LinkedListBlock class without modification to its interface or implementation
2. THE Blockchain_Simulator SHALL retain the PriorityQueueTransaction class without modification to its interface or implementation
3. THE Blockchain_Simulator SHALL retain the BSTTransactionIndex class without modification to its interface or implementation
4. THE Blockchain_Simulator SHALL retain the HashTableBalance class without modification to its interface or implementation
5. THE Blockchain_Simulator SHALL retain the Block and Transaction classes without modification to their interface or implementation
