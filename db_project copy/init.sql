-- ============================================================================
-- MySQL DDL Schema for Blockchain Simulator
-- Converted from Oracle DDL
--
-- Normalization Compliance:
--   1NF: All columns store atomic values. Transactions are stored in a
--        separate table, not embedded within blocks. No repeating groups.
--   2NF: Every non-key column depends on the full primary key. All tables
--        use single-column AUTO_INCREMENT primary keys, so 2NF is satisfied
--        automatically.
--   3NF: No transitive dependencies. Miner names live in MINERS (not
--        duplicated in BLOCKS). Transaction type names live in
--        TRANSACTION_TYPES (not duplicated in TRANSACTIONS). Chain names
--        live in BLOCKCHAIN_META (not duplicated in BLOCKS).
--
-- Run once before first app start:
--   mysql -u root -p < init.sql
-- ============================================================================

CREATE DATABASE IF NOT EXISTS blockchain_db
    CHARACTER SET utf8mb4
    COLLATE utf8mb4_unicode_ci;

USE blockchain_db;

-- ============================================================================
-- Independent / Lookup Tables (no foreign keys)
-- ============================================================================

-- ACCOUNTS: Stores named accounts with currency balances.
CREATE TABLE IF NOT EXISTS ACCOUNTS (
    ACCOUNT_ID      INT             NOT NULL AUTO_INCREMENT,
    NAME            VARCHAR(100)    NOT NULL,
    BALANCE         DECIMAL(15,2)   NOT NULL DEFAULT 0.00,
    CREATED_AT      DATETIME        NOT NULL DEFAULT CURRENT_TIMESTAMP,
    CONSTRAINT PK_ACCOUNTS        PRIMARY KEY (ACCOUNT_ID),
    CONSTRAINT UQ_ACCOUNTS_NAME   UNIQUE (NAME),
    CONSTRAINT CHK_ACCOUNT_BALANCE CHECK (BALANCE >= 0)
);

-- BLOCKCHAIN_META: Per-chain configuration and metadata.
CREATE TABLE IF NOT EXISTS BLOCKCHAIN_META (
    CHAIN_ID        INT             NOT NULL AUTO_INCREMENT,
    NAME            VARCHAR(100)    NOT NULL,
    DIFFICULTY      INT             NOT NULL,
    NEXT_TX_ID      INT             NOT NULL DEFAULT 1,
    CREATED_AT      DATETIME        NOT NULL DEFAULT CURRENT_TIMESTAMP,
    CONSTRAINT PK_BLOCKCHAIN_META       PRIMARY KEY (CHAIN_ID),
    CONSTRAINT UQ_BLOCKCHAIN_META_NAME  UNIQUE (NAME)
);

-- MINERS: Registered miners who can be attributed to mined blocks.
CREATE TABLE IF NOT EXISTS MINERS (
    MINER_ID        INT             NOT NULL AUTO_INCREMENT,
    NAME            VARCHAR(100)    NOT NULL,
    REGISTERED_AT   DATETIME        NOT NULL DEFAULT CURRENT_TIMESTAMP,
    CONSTRAINT PK_MINERS        PRIMARY KEY (MINER_ID),
    CONSTRAINT UQ_MINERS_NAME   UNIQUE (NAME)
);

-- TRANSACTION_TYPES: Lookup table categorising transactions.
CREATE TABLE IF NOT EXISTS TRANSACTION_TYPES (
    TYPE_ID         INT             NOT NULL AUTO_INCREMENT,
    TYPE_NAME       VARCHAR(100)    NOT NULL,
    DESCRIPTION     VARCHAR(500),
    CONSTRAINT PK_TRANSACTION_TYPES         PRIMARY KEY (TYPE_ID),
    CONSTRAINT UQ_TRANSACTION_TYPES_NAME    UNIQUE (TYPE_NAME)
);

-- AUDIT_LOG: Records significant database operations (SAVE, LOAD, MINE).
CREATE TABLE IF NOT EXISTS AUDIT_LOG (
    LOG_ID          INT             NOT NULL AUTO_INCREMENT,
    OPERATION       VARCHAR(100)    NOT NULL,
    CHAIN_NAME      VARCHAR(100),
    DETAILS         VARCHAR(500),
    LOG_TIMESTAMP   DATETIME        NOT NULL DEFAULT CURRENT_TIMESTAMP,
    CONSTRAINT PK_AUDIT_LOG PRIMARY KEY (LOG_ID)
);

-- ============================================================================
-- Dependent Tables (have foreign keys)
-- ============================================================================

-- BLOCKS: Individual blocks belonging to a blockchain, mined by a miner.
CREATE TABLE IF NOT EXISTS BLOCKS (
    BLOCK_ID        INT             NOT NULL AUTO_INCREMENT,
    CHAIN_ID        INT             NOT NULL,
    MINER_ID        INT             NOT NULL,
    BLOCK_INDEX     INT             NOT NULL,
    BLOCK_TIMESTAMP BIGINT          NOT NULL,
    NONCE           INT             NOT NULL DEFAULT 0,
    HASH            BIGINT          NOT NULL,
    PREV_HASH       BIGINT          NOT NULL,
    CONSTRAINT PK_BLOCKS            PRIMARY KEY (BLOCK_ID),
    CONSTRAINT FK_BLOCKS_CHAIN      FOREIGN KEY (CHAIN_ID)
        REFERENCES BLOCKCHAIN_META (CHAIN_ID) ON DELETE CASCADE,
    CONSTRAINT FK_BLOCKS_MINER      FOREIGN KEY (MINER_ID)
        REFERENCES MINERS (MINER_ID) ON DELETE CASCADE,
    CONSTRAINT CHK_BLOCK_NONCE      CHECK (NONCE >= 0)
);

-- TRANSACTIONS: Mined transactions stored within blocks.
CREATE TABLE IF NOT EXISTS TRANSACTIONS (
    TRANSACTION_ID  INT             NOT NULL AUTO_INCREMENT,
    BLOCK_ID        INT             NOT NULL,
    TYPE_ID         INT             NOT NULL,
    TX_ID           INT             NOT NULL,
    SENDER          VARCHAR(100)    NOT NULL,
    RECEIVER        VARCHAR(100)    NOT NULL,
    AMOUNT          DECIMAL(15,2)   NOT NULL,
    METADATA        VARCHAR(500),
    SIGNATURE       VARCHAR(200),
    CONSTRAINT PK_TRANSACTIONS          PRIMARY KEY (TRANSACTION_ID),
    CONSTRAINT FK_TRANSACTIONS_BLOCK    FOREIGN KEY (BLOCK_ID)
        REFERENCES BLOCKS (BLOCK_ID) ON DELETE CASCADE,
    CONSTRAINT FK_TRANSACTIONS_TYPE     FOREIGN KEY (TYPE_ID)
        REFERENCES TRANSACTION_TYPES (TYPE_ID) ON DELETE CASCADE,
    CONSTRAINT CHK_TX_AMOUNT            CHECK (AMOUNT > 0)
);

-- PENDING_TRANSACTIONS: Submitted but not yet mined transactions.
CREATE TABLE IF NOT EXISTS PENDING_TRANSACTIONS (
    PENDING_TX_ID   INT             NOT NULL AUTO_INCREMENT,
    TYPE_ID         INT             NOT NULL,
    SENDER          VARCHAR(100)    NOT NULL,
    RECEIVER        VARCHAR(100)    NOT NULL,
    AMOUNT          DECIMAL(15,2)   NOT NULL,
    METADATA        VARCHAR(500),
    SIGNATURE       VARCHAR(200),
    CONSTRAINT PK_PENDING_TRANSACTIONS      PRIMARY KEY (PENDING_TX_ID),
    CONSTRAINT FK_PENDING_TX_TYPE           FOREIGN KEY (TYPE_ID)
        REFERENCES TRANSACTION_TYPES (TYPE_ID) ON DELETE CASCADE,
    CONSTRAINT CHK_PENDING_AMOUNT           CHECK (AMOUNT > 0)
);

-- CHAIN_STATISTICS: Aggregated per-chain metrics (updated on mine/save).
CREATE TABLE IF NOT EXISTS CHAIN_STATISTICS (
    STAT_ID             INT             NOT NULL AUTO_INCREMENT,
    CHAIN_ID            INT             NOT NULL,
    TOTAL_BLOCKS        INT             NOT NULL DEFAULT 0,
    TOTAL_TRANSACTIONS  INT             NOT NULL DEFAULT 0,
    TOTAL_PENDING       INT             NOT NULL DEFAULT 0,
    LAST_UPDATED        DATETIME        NOT NULL DEFAULT CURRENT_TIMESTAMP
                                        ON UPDATE CURRENT_TIMESTAMP,
    CONSTRAINT PK_CHAIN_STATISTICS          PRIMARY KEY (STAT_ID),
    CONSTRAINT FK_CHAIN_STATISTICS_CHAIN    FOREIGN KEY (CHAIN_ID)
        REFERENCES BLOCKCHAIN_META (CHAIN_ID) ON DELETE CASCADE
);

-- ============================================================================
-- Seed Data
-- ============================================================================

-- Default transaction types
INSERT IGNORE INTO TRANSACTION_TYPES (TYPE_NAME, DESCRIPTION) VALUES
    ('transfer', 'Standard transfer'),
    ('reward',   'Mining reward'),
    ('fee',      'Transaction fee');

-- Default SYSTEM miner for genesis blocks and automated operations
INSERT IGNORE INTO MINERS (NAME) VALUES ('SYSTEM');
