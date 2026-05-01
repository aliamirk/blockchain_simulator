## Activity Diagram — Create Blockchain

```mermaid
flowchart TD
    A([Start]) --> B[Display CREATE NEW BLOCKCHAIN header]
    B --> C[Prompt: Enter difficulty level 1-5]
    C --> D{Difficulty valid\n1 ≤ d ≤ 5?}
    D -- No --> E[Set difficulty = 3\nprint warning]
    D -- Yes --> F[Use entered difficulty]
    E --> G
    F --> G{Existing blockchain\nin memory?}
    G -- Yes --> H[delete bc, pool, balances, bst]
    G -- No --> I
    H --> I[new Blockchain with difficulty\ncreates genesis Block index=0 hash=0]
    I --> J[new PriorityQueueTransaction\ncapacity=100]
    J --> K[new HashTableBalance]
    K --> L[new BSTTransactionIndex]
    L --> M[initializeBalances\nAlice=1000 Bob=1000 Charlie=1000\nDavid=500 Eve=750]
    M --> N[Print success: blockchain created\nwith genesis block]
    N --> O[Print info: default accounts initialized]
    O --> P[pressEnter]
    P --> Q([Return to main menu])
```

## Activity Diagram — Mine Pending Transactions

```mermaid
flowchart TD
    A([Start]) --> B{Blockchain\nloaded?}
    B -- No --> C[printError: no blockchain loaded]
    C --> Z([Return to menu])
    B -- Yes --> D{Pending pool\nempty?}
    D -- Yes --> E[printWarning: no pending transactions]
    E --> Z
    D -- No --> F[Display MINE PENDING TRANSACTIONS header]
    F --> G[Dequeue all transactions from\nPriorityQueueTransaction max-heap\nin priority order by amount]
    G --> H[Create Transaction array from dequeued items]
    H --> I[bc.addBlock: new Block with\nindex=chain.size prevHash=latestBlock.hash]
    I --> J[Block.mineBlock difficulty:\nincrement nonce until abs hash % target == 0]
    J --> K[Append mined block to LinkedListBlock]
    K --> L[Rebuild BSTTransactionIndex\nfor all transactions in chain]
    L --> M[Update HashTableBalance:\ndeduct sender balances\ncredit receiver balances]
    M --> N[dbManager.clearPendingTransactions chainName]
    N --> O{DB call\nsucceeded?}
    O -- No --> P[printError: DB clear failed]
    O -- Yes --> Q[dbManager.updateBalance\nfor each modified account]
    P --> R
    Q --> R[Print success: block mined\nshow nonce and attempt count]
    R --> S[pressEnter]
    S --> Z
```

## Activity Diagram — Save Blockchain to Oracle

```mermaid
flowchart TD
    A([Start]) --> B{Blockchain\nloaded?}
    B -- No --> C[printError: no blockchain to save]
    C --> Z([Return to menu])
    B -- Yes --> D[Display SAVE BLOCKCHAIN header]
    D --> E[Prompt: Enter blockchain name]
    E --> F[dbManager.saveBlockchain\nname bc balances pool]
    F --> G[MERGE INTO BLOCKCHAIN_META\nname difficulty nextTxId]
    G --> H[SELECT CHAIN_ID FROM\nBLOCKCHAIN_META WHERE NAME=:1]
    H --> I[DELETE FROM BLOCKS\nWHERE CHAIN_ID=:1\nON DELETE CASCADE removes\nTRANSACTIONS and CHAIN_STATISTICS]
    I --> J[Traverse LinkedListBlock\nfrom head to tail]
    J --> K{More blocks?}
    K -- Yes --> L[INSERT INTO BLOCKS\nCHAIN_ID MINER_ID BLOCK_INDEX\nTIMESTAMP NONCE HASH PREV_HASH]
    L --> M[SELECT BLOCKS_SEQ.CURRVAL\nto get generated BLOCK_ID]
    M --> N[Iterate block transactions]
    N --> O{More\ntransactions?}
    O -- Yes --> P[INSERT INTO TRANSACTIONS\nBLOCK_ID TYPE_ID TX_ID\nSENDER RECEIVER AMOUNT\nMETADATA SIGNATURE]
    P --> O
    O -- No --> K
    K -- No --> Q[DELETE FROM ACCOUNTS]
    Q --> R[Iterate HashTableBalance\nall 100 buckets]
    R --> S{More\naccounts?}
    S -- Yes --> T[INSERT INTO ACCOUNTS\nNAME BALANCE]
    T --> S
    S -- No --> U[INSERT INTO AUDIT_LOG\nOPERATION=SAVE CHAIN_NAME]
    U --> V[MERGE INTO CHAIN_STATISTICS\ntotal_blocks total_transactions\ntotal_pending]
    V --> W[conn.commit]
    W --> X{Commit\nsucceeded?}
    X -- No --> Y[Rollback / printError\nreturn false]
    Y --> Z
    X -- Yes --> AA[chainName = filename\nprintSuccess: saved]
    AA --> AB[pressEnter]
    AB --> Z
```

## Component Diagram

```mermaid
graph LR
    subgraph SourceFiles["Source Files"]
        MAIN["main.cpp\n─────────────\nMenu loop\nTransaction\nBlock\nLinkedListBlock\nPriorityQueueTransaction\nBSTTransactionIndex\nHashTableBalance\nBlockchain"]
        DMH["db_manager.h\n─────────────\nDatabaseManager\ndeclaration"]
        DM["db_manager.cpp\n─────────────\nDatabaseManager\nimplementation"]
        CFG["db_config.h\n─────────────\nDB_USERNAME\nDB_PASSWORD\nDB_CONNECTION_STRING"]
        SQL["schema.sql\n─────────────\nOracle DDL\nSeed data"]
    end

    subgraph InMemory["In-Memory Data Structures"]
        TX["Transaction\nid sender receiver\namount metadata signature"]
        BLK["Block\nindex timestamp txCount\nnonce hash prevHash"]
        LL["LinkedListBlock\nNodeBlock linked list"]
        PQ["PriorityQueueTransaction\nMax-heap by amount"]
        BST["BSTTransactionIndex\nBSTNode binary search tree"]
        HT["HashTableBalance\nOpen-chain hash table\nsize=100"]
        BC["Blockchain\nLinkedListBlock + difficulty"]
    end

    subgraph OracleDB["Oracle XE Database — XEPDB1"]
        ACCT[("ACCOUNTS\nACCOUNT_ID NAME BALANCE")]
        META[("BLOCKCHAIN_META\nCHAIN_ID NAME DIFFICULTY\nNEXT_TX_ID")]
        MINERS[("MINERS\nMINER_ID NAME")]
        TTYPES[("TRANSACTION_TYPES\nTYPE_ID TYPE_NAME")]
        BLOCKS[("BLOCKS\nBLOCK_ID CHAIN_ID MINER_ID\nBLOCK_INDEX NONCE HASH")]
        TXS[("TRANSACTIONS\nTRANSACTION_ID BLOCK_ID\nTX_ID SENDER RECEIVER AMOUNT")]
        PTX[("PENDING_TRANSACTIONS\nPENDING_TX_ID TYPE_ID\nSENDER RECEIVER AMOUNT")]
        AUDIT[("AUDIT_LOG\nLOG_ID OPERATION\nCHAIN_NAME LOG_TIMESTAMP")]
        STATS[("CHAIN_STATISTICS\nSTAT_ID CHAIN_ID\nTOTAL_BLOCKS TOTAL_TRANSACTIONS")]
    end

    MAIN -->|includes| DMH
    MAIN -->|uses| TX
    MAIN -->|uses| BLK
    MAIN -->|uses| LL
    MAIN -->|uses| PQ
    MAIN -->|uses| BST
    MAIN -->|uses| HT
    MAIN -->|uses| BC
    BLK -->|contains array of| TX
    LL -->|nodes hold| BLK
    BC -->|owns| LL
    DMH -->|implemented by| DM
    DM -->|reads| CFG
    DM -->|traverses| BC
    DM -->|reads writes| HT
    DM -->|reads| PQ
    DM -->|rebuilds| BST
    SQL -->|creates| ACCT
    SQL -->|creates| META
    SQL -->|creates| MINERS
    SQL -->|creates| TTYPES
    SQL -->|creates| BLOCKS
    SQL -->|creates| TXS
    SQL -->|creates| PTX
    SQL -->|creates| AUDIT
    SQL -->|creates| STATS
    DM -->|OCCI INSERT SELECT| ACCT
    DM -->|OCCI MERGE SELECT| META
    DM -->|OCCI SELECT| MINERS
    DM -->|OCCI SELECT| TTYPES
    DM -->|OCCI INSERT DELETE| BLOCKS
    DM -->|OCCI INSERT| TXS
    DM -->|OCCI INSERT DELETE SELECT| PTX
    DM -->|OCCI INSERT| AUDIT
    DM -->|OCCI MERGE| STATS
```

## Deployment Diagram

```mermaid
graph TB
    subgraph DevMachine["Developer Machine"]
        subgraph CppApp["C++ Application Process"]
            MAIN2["main.cpp\nMenu-driven loop"]
            DM2["DatabaseManager\nOCCI client"]
            DS["In-memory structures\nBlockchain PriorityQueue\nHashTable BST"]
            CFG2["db_config.h\nlocalhost:1521/XEPDB1\nblockchain_user"]
        end
        OCCI["Oracle OCCI Library\noracle/occi.h\nlibocci.so"]
        BCFILE["test.bc\nLegacy flat-file\nbackup"]
    end

    subgraph OracleNode["Oracle XE Node — localhost:1521"]
        subgraph OracleXE["Oracle Database XE"]
            PDB["XEPDB1\nPluggable Database"]
            subgraph Schema["blockchain_user schema"]
                T1[("BLOCKCHAIN_META")]
                T2[("BLOCKS")]
                T3[("TRANSACTIONS")]
                T4[("PENDING_TRANSACTIONS")]
                T5[("ACCOUNTS")]
                T6[("MINERS")]
                T7[("TRANSACTION_TYPES")]
                T8[("AUDIT_LOG")]
                T9[("CHAIN_STATISTICS")]
            end
        end
    end

    MAIN2 -->|calls| DM2
    MAIN2 -->|manages| DS
    DM2 -->|reads| CFG2
    DM2 -->|uses| OCCI
    OCCI -->|TCP/IP port 1521\nSQL*Net protocol| PDB
    PDB --> Schema
    CppApp -->|reads writes legacy| BCFILE
```

## Sequence Diagram — Save Blockchain

```mermaid
sequenceDiagram
    participant Main as main.cpp
    participant DM as DatabaseManager
    participant DB as Oracle DB

    Main->>DM: saveBlockchain(name, bc, balances, pool)
    DM->>DB: MERGE INTO BLOCKCHAIN_META (NAME, DIFFICULTY, NEXT_TX_ID)
    DM->>DB: SELECT CHAIN_ID FROM BLOCKCHAIN_META WHERE NAME = :1
    DB-->>DM: chain_id
    DM->>DB: DELETE FROM BLOCKS WHERE CHAIN_ID = :1
    Note over DB: ON DELETE CASCADE removes TRANSACTIONS,<br/>CHAIN_STATISTICS rows automatically
    loop Each Block in LinkedListBlock (head → tail)
        DM->>DB: INSERT INTO BLOCKS (CHAIN_ID, MINER_ID, BLOCK_INDEX, BLOCK_TIMESTAMP, NONCE, HASH, PREV_HASH)
        DM->>DB: SELECT BLOCKS_SEQ.CURRVAL
        DB-->>DM: block_id
        loop Each Transaction in Block
            DM->>DB: INSERT INTO TRANSACTIONS (BLOCK_ID, TYPE_ID, TX_ID, SENDER, RECEIVER, AMOUNT, METADATA, SIGNATURE)
        end
    end
    DM->>DB: DELETE FROM ACCOUNTS
    loop Each entry in HashTableBalance (100 buckets)
        DM->>DB: INSERT INTO ACCOUNTS (NAME, BALANCE)
    end
    DM->>DB: INSERT INTO AUDIT_LOG (OPERATION='SAVE', CHAIN_NAME, DETAILS)
    DM->>DB: MERGE INTO CHAIN_STATISTICS (CHAIN_ID, TOTAL_BLOCKS, TOTAL_TRANSACTIONS, TOTAL_PENDING)
    DM->>DB: COMMIT
    DB-->>DM: commit OK
    DM-->>Main: return true
    Main->>Main: chainName = name
    Main->>Main: printSuccess("Blockchain saved")
```

## Sequence Diagram — Load Blockchain

```mermaid
sequenceDiagram
    participant Main as main.cpp
    participant DM as DatabaseManager
    participant DB as Oracle DB

    Main->>DM: loadBlockchain(name, bc, balances, bst)
    DM->>DB: SELECT CHAIN_ID, DIFFICULTY, NEXT_TX_ID FROM BLOCKCHAIN_META WHERE NAME = :1
    DB-->>DM: chain_id, difficulty, next_tx_id
    Note over DM: Set bc.difficulty = difficulty<br/>Transaction::nextId = next_tx_id
    DM->>DB: SELECT * FROM BLOCKS WHERE CHAIN_ID = :1 ORDER BY BLOCK_INDEX
    DB-->>DM: block rows
    loop Each Block row
        DM->>DB: SELECT * FROM TRANSACTIONS WHERE BLOCK_ID = :1 ORDER BY TX_ID
        DB-->>DM: transaction rows
        Note over DM: Reconstruct Block with Transaction array<br/>copy id, sender, receiver, amount,<br/>metadata, signature
        DM->>DM: bc.chain.append(block)
        loop Each Transaction in block
            DM->>DM: bst.insert(tx.id, blockIndex, txIndex)
        end
    end
    DM->>DB: SELECT NAME, BALANCE FROM ACCOUNTS
    DB-->>DM: account rows
    loop Each Account row
        DM->>DM: balances.update(name, balance)
    end
    DM->>DB: INSERT INTO AUDIT_LOG (OPERATION='LOAD', CHAIN_NAME)
    DM->>DB: COMMIT
    DB-->>DM: commit OK
    DM-->>Main: return true
    Main->>Main: chainName = name
    Main->>Main: printSuccess("Blockchain loaded")
```

## Use Case Diagram 1 — Blockchain Management

```mermaid
graph LR
    User(["👤 User"])

    UC1["Create Blockchain\n─────────────\nSet difficulty 1-5\nGenerate genesis block\nInit default accounts"]
    UC2["Load Blockchain\n─────────────\nQuery BLOCKCHAIN_META\nRestore blocks and txs\nRebuild BST index"]
    UC3["Save Blockchain\n─────────────\nMERGE chain metadata\nInsert blocks and txs\nInsert accounts"]
    UC4["View Blockchain Summary\n─────────────\nShow total blocks\nShow difficulty\nShow latest hash"]
    UC5["Verify Chain Integrity\n─────────────\nTraverse LinkedListBlock\nCheck prevHash links\nReport valid or invalid"]

    User --> UC1
    User --> UC2
    User --> UC3
    User --> UC4
    User --> UC5
```

## Use Case Diagram 2 — Transaction Management

```mermaid
graph LR
    User(["👤 User"])

    UC1["Add Transaction to Pool\n─────────────\nEnter sender receiver amount\nValidate sufficient balance\nEnqueue in PriorityQueue\nSave to PENDING_TRANSACTIONS"]
    UC2["View Pending Transactions\n─────────────\nDisplay PriorityQueue\nSorted by amount desc\nShow priority score"]
    UC3["Mine Pending Transactions\n─────────────\nDequeue all from max-heap\nProof-of-work nonce search\nAppend block to chain\nUpdate balances\nClear PENDING_TRANSACTIONS"]

    User --> UC1
    User --> UC2
    User --> UC3
```

## Use Case Diagram 3 — Account Management

```mermaid
graph LR
    User(["👤 User"])

    UC1["View Account Balances\n─────────────\nIterate HashTableBalance\nDisplay name and balance\nFormatted currency output"]
    UC2["Initialize Default Accounts\n─────────────\nAlice=1000 Bob=1000\nCharlie=1000 David=500\nEve=750\nInsert into ACCOUNTS table"]
    UC3["Update Balance After Mining\n─────────────\nDeduct sender balance\nCredit receiver balance\nCall dbManager.updateBalance\nper modified account"]

    User --> UC1
    User --> UC2
    User --> UC3
```

## Use Case Diagram 4 — Search & Query

```mermaid
graph LR
    User(["👤 User"])

    UC1["Search by Sender\n─────────────\nLinear scan all blocks\nMatch sender field\nDisplay matching txs\nwith block location"]
    UC2["Search by Receiver\n─────────────\nLinear scan all blocks\nMatch receiver field\nDisplay matching txs\nwith block location"]
    UC3["Search by Transaction ID\nvia BST\n─────────────\nO log n BST lookup\nResolve blockIndex txIndex\nDisplay full tx details"]
    UC4["View Specific Block\n─────────────\nEnter block index\nLinkedListBlock.get index\nDisplay block header\nand all transactions"]

    User --> UC1
    User --> UC2
    User --> UC3
    User --> UC4
```

## Use Case Diagram 5 — Database Operations

```mermaid
graph LR
    User(["👤 User"])
    DBA(["🗄️ DatabaseManager"])

    UC1["Connect to Oracle\n─────────────\nOCCI Environment create\nConnection with credentials\nfrom db_config.h\nReturn true or false"]
    UC2["Disconnect from Oracle\n─────────────\nNull-check conn and env\nterminateConnection\nterminateEnvironment"]
    UC3["Save to DB\n─────────────\nsaveBlockchain\nsavePendingTransaction\nMERGE upsert strategy\nExplicit COMMIT"]
    UC4["Load from DB\n─────────────\nloadBlockchain\nloadPendingTransactions\nRebuild in-memory state\nExplicit COMMIT"]
    UC5["Audit Logging\n─────────────\nINSERT AUDIT_LOG on\nSAVE LOAD MINE ops\nTimestamp via SYSTIMESTAMP"]
    UC6["View Chain Statistics\n─────────────\nMERGE CHAIN_STATISTICS\ntotal_blocks\ntotal_transactions\ntotal_pending"]

    User --> UC1
    User --> UC2
    User --> UC3
    User --> UC4
    User --> UC5
    User --> UC6
    UC1 --> DBA
    UC2 --> DBA
    UC3 --> DBA
    UC4 --> DBA
    UC5 --> DBA
    UC6 --> DBA
```

## State Diagram — Blockchain Application Lifecycle

```mermaid
stateDiagram-v2
    [*] --> Idle : Application starts\ndbManager.connect()

    Idle --> Idle : DB connect failed\n(offline mode warning)

    Idle --> BlockchainActive : Menu 1: Create Blockchain\nnew Blockchain + genesis block\ninitializeBalances()

    Idle --> Loading : Menu 2: Load Blockchain\nenter chain name

    Loading --> BlockchainActive : loadBlockchain() returns true\nchain restored from Oracle\nBST rebuilt

    Loading --> Idle : loadBlockchain() returns false\nchain name not found\npointers set to nullptr

    BlockchainActive --> Saving : Menu 3: Save Blockchain\nenter chain name

    Saving --> BlockchainActive : saveBlockchain() returns true\nAUDIT_LOG SAVE entry\nCOMMIT

    Saving --> BlockchainActive : saveBlockchain() returns false\nprintError DB failure\nchain unchanged in memory

    BlockchainActive --> TransactionPending : Menu 4: Add Transaction to Pool\nvalidate balance\nenqueue PriorityQueueTransaction\nsavePendingTransaction() to DB

    TransactionPending --> TransactionPending : Menu 4: Add another transaction\nmax-heap rebalanced

    TransactionPending --> Mining : Menu 5: Mine Pending Transactions\ndequeue all from max-heap

    Mining --> BlockchainActive : Proof-of-work complete\nnew Block appended\nBST rebuilt\nbalances updated\nclearPendingTransactions()

    BlockchainActive --> Searching : Menu 11: Search Transactions\nenter search sub-menu

    Searching --> Searching : Search by sender linear scan\nSearch by receiver linear scan\nView specific block by index

    Searching --> Searching : Search by TX ID\nBST O log n lookup

    Searching --> BlockchainActive : Return to main menu

    BlockchainActive --> Loading : Menu 2: Load different chain\nexisting pointers deleted

    BlockchainActive --> Exiting : Menu 0: Exit\ndbManager.disconnect()

    Idle --> Exiting : Menu 0: Exit\ndbManager.disconnect()

    Exiting --> [*] : delete bc pool balances bst\nApplication terminates
```
