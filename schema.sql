-- ============================================================================
-- Oracle DDL Schema for Blockchain Simulator
-- Feature: oracle-db-migration
--
-- Normalization Compliance:
--   1NF: All columns store atomic values. Transactions are stored in a
--        separate table, not embedded within blocks. No repeating groups.
--   2NF: Every non-key column depends on the full primary key. All tables
--        use single-column IDENTITY primary keys, so 2NF is satisfied
--        automatically.
--   3NF: No transitive dependencies. Miner names live in MINERS (not
--        duplicated in BLOCKS). Transaction type names live in
--        TRANSACTION_TYPES (not duplicated in TRANSACTIONS). Chain names
--        live in BLOCKCHAIN_META (not duplicated in BLOCKS).
-- ============================================================================

-- ============================================================================
-- Independent / Lookup Tables (no foreign keys)
-- ============================================================================

-- ACCOUNTS: Stores named accounts with currency balances.
CREATE TABLE ACCOUNTS (
    ACCOUNT_ID      NUMBER GENERATED ALWAYS AS IDENTITY,
    NAME            VARCHAR2(100)   NOT NULL,
    BALANCE         NUMBER(15,2)    NOT NULL,
    CREATED_AT      TIMESTAMP       DEFAULT SYSTIMESTAMP NOT NULL,
    CONSTRAINT PK_ACCOUNTS        PRIMARY KEY (ACCOUNT_ID),
    CONSTRAINT UQ_ACCOUNTS_NAME   UNIQUE (NAME),
    CONSTRAINT CHK_ACCOUNT_BALANCE CHECK (BALANCE >= 0)
);

-- BLOCKCHAIN_META: Per-chain configuration and metadata.
CREATE TABLE BLOCKCHAIN_META (
    CHAIN_ID        NUMBER GENERATED ALWAYS AS IDENTITY,
    NAME            VARCHAR2(100)   NOT NULL,
    DIFFICULTY      NUMBER          NOT NULL,
    NEXT_TX_ID      NUMBER          NOT NULL,
    CREATED_AT      TIMESTAMP       DEFAULT SYSTIMESTAMP NOT NULL,
    CONSTRAINT PK_BLOCKCHAIN_META       PRIMARY KEY (CHAIN_ID),
    CONSTRAINT UQ_BLOCKCHAIN_META_NAME  UNIQUE (NAME)
);

-- MINERS: Registered miners who can be attributed to mined blocks.
CREATE TABLE MINERS (
    MINER_ID        NUMBER GENERATED ALWAYS AS IDENTITY,
    NAME            VARCHAR2(100)   NOT NULL,
    REGISTERED_AT   TIMESTAMP       DEFAULT SYSTIMESTAMP NOT NULL,
    CONSTRAINT PK_MINERS        PRIMARY KEY (MINER_ID),
    CONSTRAINT UQ_MINERS_NAME   UNIQUE (NAME)
);

-- TRANSACTION_TYPES: Lookup table categorising transactions.
CREATE TABLE TRANSACTION_TYPES (
    TYPE_ID         NUMBER GENERATED ALWAYS AS IDENTITY,
    TYPE_NAME       VARCHAR2(100)   NOT NULL,
    DESCRIPTION     VARCHAR2(500),
    CONSTRAINT PK_TRANSACTION_TYPES         PRIMARY KEY (TYPE_ID),
    CONSTRAINT UQ_TRANSACTION_TYPES_NAME    UNIQUE (TYPE_NAME)
);

-- AUDIT_LOG: Records significant database operations (SAVE, LOAD, MINE).
CREATE TABLE AUDIT_LOG (
    LOG_ID          NUMBER GENERATED ALWAYS AS IDENTITY,
    OPERATION       VARCHAR2(100)   NOT NULL,
    CHAIN_NAME      VARCHAR2(100),
    DETAILS         VARCHAR2(500),
    LOG_TIMESTAMP   TIMESTAMP       DEFAULT SYSTIMESTAMP NOT NULL,
    CONSTRAINT PK_AUDIT_LOG PRIMARY KEY (LOG_ID)
);

-- ============================================================================
-- Dependent Tables (have foreign keys)
-- ============================================================================

-- BLOCKS: Individual blocks belonging to a blockchain, mined by a miner.
CREATE TABLE BLOCKS (
    BLOCK_ID        NUMBER GENERATED ALWAYS AS IDENTITY,
    CHAIN_ID        NUMBER          NOT NULL,
    MINER_ID        NUMBER          NOT NULL,
    BLOCK_INDEX     NUMBER          NOT NULL,
    BLOCK_TIMESTAMP NUMBER          NOT NULL,
    NONCE           NUMBER          NOT NULL,
    HASH            NUMBER          NOT NULL,
    PREV_HASH       NUMBER          NOT NULL,
    CONSTRAINT PK_BLOCKS            PRIMARY KEY (BLOCK_ID),
    CONSTRAINT FK_BLOCKS_CHAIN      FOREIGN KEY (CHAIN_ID)
        REFERENCES BLOCKCHAIN_META (CHAIN_ID) ON DELETE CASCADE,
    CONSTRAINT FK_BLOCKS_MINER      FOREIGN KEY (MINER_ID)
        REFERENCES MINERS (MINER_ID) ON DELETE CASCADE,
    CONSTRAINT CHK_BLOCK_NONCE      CHECK (NONCE >= 0)
);

-- TRANSACTIONS: Mined transactions stored within blocks.
CREATE TABLE TRANSACTIONS (
    TRANSACTION_ID  NUMBER GENERATED ALWAYS AS IDENTITY,
    BLOCK_ID        NUMBER          NOT NULL,
    TYPE_ID         NUMBER          NOT NULL,
    TX_ID           NUMBER          NOT NULL,
    SENDER          VARCHAR2(100)   NOT NULL,
    RECEIVER        VARCHAR2(100)   NOT NULL,
    AMOUNT          NUMBER(15,2)    NOT NULL,
    METADATA        VARCHAR2(500),
    SIGNATURE       VARCHAR2(200),
    CONSTRAINT PK_TRANSACTIONS          PRIMARY KEY (TRANSACTION_ID),
    CONSTRAINT FK_TRANSACTIONS_BLOCK    FOREIGN KEY (BLOCK_ID)
        REFERENCES BLOCKS (BLOCK_ID) ON DELETE CASCADE,
    CONSTRAINT FK_TRANSACTIONS_TYPE     FOREIGN KEY (TYPE_ID)
        REFERENCES TRANSACTION_TYPES (TYPE_ID) ON DELETE CASCADE,
    CONSTRAINT CHK_TX_AMOUNT            CHECK (AMOUNT > 0)
);

-- PENDING_TRANSACTIONS: Submitted but not yet mined transactions.
CREATE TABLE PENDING_TRANSACTIONS (
    PENDING_TX_ID   NUMBER GENERATED ALWAYS AS IDENTITY,
    TYPE_ID         NUMBER          NOT NULL,
    SENDER          VARCHAR2(100)   NOT NULL,
    RECEIVER        VARCHAR2(100)   NOT NULL,
    AMOUNT          NUMBER(15,2)    NOT NULL,
    METADATA        VARCHAR2(500),
    SIGNATURE       VARCHAR2(200),
    CONSTRAINT PK_PENDING_TRANSACTIONS      PRIMARY KEY (PENDING_TX_ID),
    CONSTRAINT FK_PENDING_TRANSACTIONS_TYPE FOREIGN KEY (TYPE_ID)
        REFERENCES TRANSACTION_TYPES (TYPE_ID) ON DELETE CASCADE,
    CONSTRAINT CHK_PENDING_AMOUNT           CHECK (AMOUNT > 0)
);

-- CHAIN_STATISTICS: Aggregated per-chain metrics.
CREATE TABLE CHAIN_STATISTICS (
    STAT_ID             NUMBER GENERATED ALWAYS AS IDENTITY,
    CHAIN_ID            NUMBER          NOT NULL,
    TOTAL_BLOCKS        NUMBER          DEFAULT 0 NOT NULL,
    TOTAL_TRANSACTIONS  NUMBER          DEFAULT 0 NOT NULL,
    TOTAL_PENDING       NUMBER          DEFAULT 0 NOT NULL,
    LAST_UPDATED        TIMESTAMP       DEFAULT SYSTIMESTAMP NOT NULL,
    CONSTRAINT PK_CHAIN_STATISTICS      PRIMARY KEY (STAT_ID),
    CONSTRAINT FK_CHAIN_STATISTICS_CHAIN FOREIGN KEY (CHAIN_ID)
        REFERENCES BLOCKCHAIN_META (CHAIN_ID) ON DELETE CASCADE
);

-- ============================================================================
-- Seed Data
-- ============================================================================

-- Default transaction types (Requirement 1.15, 10.1)
INSERT INTO TRANSACTION_TYPES (TYPE_NAME, DESCRIPTION) VALUES ('transfer', 'Standard transfer');
INSERT INTO TRANSACTION_TYPES (TYPE_NAME, DESCRIPTION) VALUES ('reward', 'Mining reward');
INSERT INTO TRANSACTION_TYPES (TYPE_NAME, DESCRIPTION) VALUES ('fee', 'Transaction fee');

-- Default SYSTEM miner for genesis blocks and automated operations (Requirement 1.16, 9.1)
INSERT INTO MINERS (NAME, REGISTERED_AT) VALUES ('SYSTEM', SYSTIMESTAMP);

COMMIT;
