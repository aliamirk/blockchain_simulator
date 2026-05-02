#ifndef DB_MANAGER_H
#define DB_MANAGER_H

#include <string>
#include <occi.h>

// Forward declarations (existing classes, unchanged)
class Blockchain;
class HashTableBalance;
class PriorityQueueTransaction;
class BSTTransactionIndex;
class Transaction;

class DatabaseManager {
private:
    oracle::occi::Environment* env;
    oracle::occi::Connection* conn;

    int getChainId(const std::string& chainName);
    int getSystemMinerId();
    int getTransferTypeId();

public:
    DatabaseManager();
    ~DatabaseManager();

    // Connection management
    bool connect();
    void disconnect();

    // Blockchain persistence
    bool saveBlockchain(const std::string& chainName,
                        Blockchain* blockchain,
                        HashTableBalance* balances,
                        PriorityQueueTransaction* pool);
    bool loadBlockchain(const std::string& chainName,
                        Blockchain* blockchain,
                        HashTableBalance* balances,
                        BSTTransactionIndex* bst);

    // Pending transactions
    bool savePendingTransaction(const Transaction& tx);
    bool loadPendingTransactions(PriorityQueueTransaction* pool);
    bool clearPendingTransactions(const std::string& chainName);

    // Account balances
    bool updateBalance(const std::string& accountName, double balance);
    bool initializeDefaultAccounts();
};

#endif // DB_MANAGER_H
