#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <ctime>
#include <iomanip>
#include "db_manager.h"

using namespace std;

// ==================== ANSI COLOR CODES ====================
#define RESET   "\033[0m"
#define BOLD    "\033[1m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define BLUE    "\033[34m"
#define MAGENTA "\033[35m"
#define CYAN    "\033[36m"

// ==================== UTILITY FUNCTIONS ====================
void clearScreen() {
    #ifdef _WIN32
        system("cls");
    #else
        system("clear");
    #endif
}

void printHeader(const string& title) {
    cout << "\n" << CYAN << BOLD;
    cout << "╔════════════════════════════════════════════════════════════╗\n";
    cout << "║" << setw(40) << title << setw(22) << "║\n";
    cout << "╚════════════════════════════════════════════════════════════╝";
    cout << RESET << "\n\n";
}

void printSeparator() {
    cout << BLUE << "────────────────────────────────────────────────────────────\n" << RESET;
}

void printSuccess(const string& msg) {
    cout << GREEN << "✓ " << msg << RESET << endl;
}

void printError(const string& msg) {
    cout << RED << "✗ " << msg << RESET << endl;
}

void printWarning(const string& msg) {
    cout << YELLOW << "⚠ " << msg << RESET << endl;
}

void printInfo(const string& msg) {
    cout << CYAN << "ℹ " << msg << RESET << endl;
}

void pressEnter() {
    cout << "\n" << YELLOW << "Press ENTER to continue..." << RESET;
    cin.ignore();
    cin.get();
}

// ==================== SIMPLIFIED HASH FUNCTION ====================
int hashString(const string& s) {
    int h = 5381;
    for (size_t i = 0; i < s.length(); i++) {
        h = ((h << 5) + h) + (unsigned char)s[i];
    }
    return abs(h);
}

// ==================== TRANSACTION CLASS ====================
class Transaction {
public:
    static int nextId;
    int id;
    string sender;
    string receiver;
    double amount;
    string metadata;
    string signature;

    Transaction() : id(0), amount(0.0) {}
    
    Transaction(string s, string r, double a, string m = "") 
        : sender(s), receiver(r), amount(a), metadata(m) {
        id = nextId++;
        signature = "SIG" + to_string(hashString(sender + receiver + to_string(amount)));
    }

    string toString() const {
        stringstream ss;
        ss << MAGENTA << "  [TX#" << id << "] " << RESET
           << sender << " → " << receiver 
           << " | " << YELLOW << "$" << fixed << setprecision(2) << amount << RESET;
        if (!metadata.empty()) {
            ss << " | " << metadata;
        }
        return ss.str();
    }

    string toDetailedString() const {
        stringstream ss;
        ss << "  Transaction ID    : " << MAGENTA << id << RESET << "\n"
           << "  Sender           : " << sender << "\n"
           << "  Receiver         : " << receiver << "\n"
           << "  Amount           : " << YELLOW << "$" << fixed << setprecision(2) << amount << RESET << "\n"
           << "  Metadata         : " << (metadata.empty() ? "-" : metadata) << "\n"
           << "  Signature        : " << signature;
        return ss.str();
    }

    void saveToFile(ofstream& file) const {
        file << id << "\n" << sender << "\n" << receiver << "\n" 
             << amount << "\n" << metadata << "\n" << signature << "\n";
    }

    void loadFromFile(ifstream& file) {
        file >> id;
        file.ignore();
        getline(file, sender);
        getline(file, receiver);
        file >> amount;
        file.ignore();
        getline(file, metadata);
        getline(file, signature);
    }
};

int Transaction::nextId = 1;

// ==================== BINARY SEARCH TREE FOR TRANSACTION INDEXING ====================
struct BSTNode {
    int txId;
    int blockIndex;
    int txIndex;
    BSTNode* left;
    BSTNode* right;
    
    BSTNode(int id, int bIdx, int tIdx) 
        : txId(id), blockIndex(bIdx), txIndex(tIdx), left(nullptr), right(nullptr) {}
};

class BSTTransactionIndex {
private:
    BSTNode* root;
    
    BSTNode* insert(BSTNode* node, int txId, int blockIdx, int txIdx) {
        if (node == nullptr) {
            return new BSTNode(txId, blockIdx, txIdx);
        }
        
        if (txId < node->txId) {
            node->left = insert(node->left, txId, blockIdx, txIdx);
        } else if (txId > node->txId) {
            node->right = insert(node->right, txId, blockIdx, txIdx);
        }
        
        return node;
    }
    
    BSTNode* search(BSTNode* node, int txId) {
        if (node == nullptr || node->txId == txId) {
            return node;
        }
        
        if (txId < node->txId) {
            return search(node->left, txId);
        } else {
            return search(node->right, txId);
        }
    }
    
    void deleteTree(BSTNode* node) {
        if (node == nullptr) return;
        deleteTree(node->left);
        deleteTree(node->right);
        delete node;
    }
    
public:
    BSTTransactionIndex() : root(nullptr) {}
    
    ~BSTTransactionIndex() {
        deleteTree(root);
    }
    
    void insert(int txId, int blockIdx, int txIdx) {
        root = insert(root, txId, blockIdx, txIdx);
    }
    
    bool search(int txId, int& blockIdx, int& txIdx) {
        BSTNode* result = search(root, txId);
        if (result != nullptr) {
            blockIdx = result->blockIndex;
            txIdx = result->txIndex;
            return true;
        }
        return false;
    }
    
    void clear() {
        deleteTree(root);
        root = nullptr;
    }
};

// ==================== PRIORITY QUEUE FOR TRANSACTIONS (MIN HEAP) ====================
class PriorityQueueTransaction {
private:
    Transaction** heap;
    int capacity;
    int size;
    
    void heapifyUp(int index) {
        while (index > 0) {
            int parent = (index - 1) / 2;
            if (heap[index]->amount > heap[parent]->amount) {
                Transaction* temp = heap[index];
                heap[index] = heap[parent];
                heap[parent] = temp;
                index = parent;
            } else {
                break;
            }
        }
    }
    
    void heapifyDown(int index) {
        while (true) {
            int largest = index;
            int left = 2 * index + 1;
            int right = 2 * index + 2;
            
            if (left < size && heap[left]->amount > heap[largest]->amount) {
                largest = left;
            }
            if (right < size && heap[right]->amount > heap[largest]->amount) {
                largest = right;
            }
            
            if (largest != index) {
                Transaction* temp = heap[index];
                heap[index] = heap[largest];
                heap[largest] = temp;
                index = largest;
            } else {
                break;
            }
        }
    }
    
public:
    PriorityQueueTransaction(int cap = 100) : capacity(cap), size(0) {
        heap = new Transaction*[capacity];
    }
    
    ~PriorityQueueTransaction() {
        for (int i = 0; i < size; i++) {
            delete heap[i];
        }
        delete[] heap;
    }
    
    void enqueue(Transaction* tx) {
        if (size >= capacity) {
            printWarning("Priority queue is full!");
            return;
        }
        heap[size] = tx;
        heapifyUp(size);
        size++;
    }
    
    Transaction* dequeue() {
        if (isEmpty()) return nullptr;
        Transaction* tx = heap[0];
        heap[0] = heap[size - 1];
        size--;
        heapifyDown(0);
        return tx;
    }
    
    bool isEmpty() const { return size == 0; }
    int getSize() const { return size; }
    
    void getAll(Transaction**& txs, int& count) {
        count = size;
        txs = new Transaction*[count];
        
        // Create temporary array to extract in priority order
        PriorityQueueTransaction tempQueue(capacity);
        for (int i = 0; i < size; i++) {
            tempQueue.enqueue(heap[i]);
        }
        
        for (int i = 0; i < count; i++) {
            txs[i] = tempQueue.dequeue();
        }
    }

    void displayPending() const {
        if (isEmpty()) {
            cout << "  No pending transactions\n";
            return;
        }
        
        // Display in priority order without modifying heap
        Transaction** sorted = new Transaction*[size];
        for (int i = 0; i < size; i++) {
            sorted[i] = heap[i];
        }
        
        // Simple selection sort for display
        for (int i = 0; i < size - 1; i++) {
            for (int j = i + 1; j < size; j++) {
                if (sorted[j]->amount > sorted[i]->amount) {
                    Transaction* temp = sorted[i];
                    sorted[i] = sorted[j];
                    sorted[j] = temp;
                }
            }
        }
        
        for (int i = 0; i < size; i++) {
            cout << sorted[i]->toString() << " " << GREEN << "[Priority: $" 
                 << fixed << setprecision(2) << sorted[i]->amount << "]" << RESET << "\n";
        }
        
        delete[] sorted;
    }
};

// ==================== BLOCK CLASS ====================
class Block {
public:
    int index;
    long timestamp;
    Transaction* transactions;
    int txCount;
    int nonce;
    int hash;
    int prevHash;

    Block() : index(0), timestamp(0), transactions(nullptr), txCount(0), 
              nonce(0), hash(0), prevHash(0) {}
    
    Block(int idx, int prevH, Transaction* txs, int count) 
        : index(idx), prevHash(prevH), txCount(count), transactions(nullptr) {
        timestamp = time(nullptr);
        if (count > 0 && txs != nullptr) {
            transactions = new Transaction[count];
            for (int i = 0; i < count; i++) {
                transactions[i].id = txs[i].id;
                transactions[i].sender = txs[i].sender;
                transactions[i].receiver = txs[i].receiver;
                transactions[i].amount = txs[i].amount;
                transactions[i].metadata = txs[i].metadata;
                transactions[i].signature = txs[i].signature;
            }
        }
        nonce = 0;
        hash = calculateHash();
    }

    ~Block() {
        if (transactions != nullptr) {
            delete[] transactions;
        }
    }

    void mineBlock(int difficulty) {
        if (difficulty > 5) difficulty = 5;  // Safety limit
        
        printInfo("Mining block with difficulty " + to_string(difficulty) + "...");

        int target = 1;
        for (int i = 0; i < difficulty; i++) target *= 10;

        int attempts = 0;
        while (abs(hash) % target != 0) {
            nonce++;
            hash = calculateHash();
            attempts++;

            if (attempts % 5000 == 0) {
                cout << YELLOW << "  Mining... Attempts: " << attempts
                     << " | Nonce: " << nonce << RESET << "\r" << flush;
            }
        }

        cout << string(60, ' ') << "\r";
        printSuccess("Block mined! Nonce: " + to_string(nonce) + " | Attempts: " + to_string(attempts));
    }

    string toString() const {
        stringstream ss;
        ss << "\n" << BLUE << BOLD << "┌─ Block #" << index << " " << string(45, '-') << "┐\n" << RESET;
        ss << BLUE << "│" << RESET << " Hash        : " << GREEN << hash << RESET << string(42, ' ') << BLUE << "│\n" << RESET;
        ss << BLUE << "│" << RESET << " Prev Hash   : " << prevHash << string(42, ' ') << BLUE << "│\n" << RESET;
        ss << BLUE << "│" << RESET << " Timestamp   : " << getTimeString() << string(19, ' ') << BLUE << "│\n" << RESET;
        ss << BLUE << "│" << RESET << " Nonce       : " << nonce << string(42, ' ') << BLUE << "│\n" << RESET;
        ss << BLUE << "│" << RESET << " Transactions: " << txCount << string(42, ' ') << BLUE << "│\n" << RESET;
        ss << BLUE << "├" << string(59, '-') << "┤\n" << RESET;
        
        for (int i = 0; i < txCount; i++) {
            ss << BLUE << "│" << RESET << transactions[i].toString();
            ss << string(max(0, 58 - (int)transactions[i].toString().length() + 20), ' ');
            ss << BLUE << "│\n" << RESET;
        }
        
        ss << BLUE << "└" << string(59, '-') << "┘" << RESET;
        return ss.str();
    }

    void saveToFile(ofstream& file) const {
        file << index << "\n" << timestamp << "\n" << txCount << "\n" 
             << nonce << "\n" << hash << "\n" << prevHash << "\n";
        for (int i = 0; i < txCount; i++) {
            transactions[i].saveToFile(file);
        }
    }

    void loadFromFile(ifstream& file) {
        file >> index >> timestamp >> txCount >> nonce >> hash >> prevHash;
        file.ignore();
        
        if (txCount > 0) {
            transactions = new Transaction[txCount];
            for (int i = 0; i < txCount; i++) {
                transactions[i].loadFromFile(file);
            }
        }
    }

private:
    int calculateHash() {
        stringstream ss;
        ss << index << timestamp << prevHash << nonce;
        for (int i = 0; i < txCount; i++) {
            ss << transactions[i].toString();
        }
        return hashString(ss.str());
    }

    string getTimeString() const {
        char buffer[80];
        struct tm* timeinfo = localtime(&timestamp);
        strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);
        return string(buffer);
    }
};

// ==================== LINKED LIST FOR BLOCKS ====================
struct NodeBlock {
    Block* block;
    NodeBlock* next;
    NodeBlock(Block* b) : block(b), next(nullptr) {}
};

class LinkedListBlock {
private:
    NodeBlock* head;
    int size;
public:
    LinkedListBlock() : head(nullptr), size(0) {}
    
    ~LinkedListBlock() {
        NodeBlock* current = head;
        while (current != nullptr) {
            NodeBlock* next = current->next;
            delete current->block;
            delete current;
            current = next;
        }
    }
    
    void append(Block* b) {
        NodeBlock* newNode = new NodeBlock(b);
        if (head == nullptr) {
            head = newNode;
        } else {
            NodeBlock* temp = head;
            while (temp->next != nullptr) {
                temp = temp->next;
            }
            temp->next = newNode;
        }
        size++;
    }
    
    Block* get(int index) {
        if (index < 0 || index >= size) return nullptr;
        NodeBlock* temp = head;
        for (int i = 0; i < index; i++) {
            temp = temp->next;
        }
        return temp->block;
    }
    
    int getSize() const { return size; }
    NodeBlock* getHead() { return head; }
};

// ==================== HASH TABLE FOR BALANCES ====================
struct NodeHash {
    string key;
    double value;
    NodeHash* next;
    NodeHash(string k, double v) : key(k), value(v), next(nullptr) {}
};

class HashTableBalance {
    friend class DatabaseManager;
private:
    static const int TABLE_SIZE = 100;
    NodeHash* table[TABLE_SIZE];
    
    int hashFunction(const string& key) {
        int h = 5381;
        for (size_t i = 0; i < key.length(); i++) {
            h = ((h << 5) + h) + (unsigned char)key[i];
        }
        return abs(h) % TABLE_SIZE;
    }
    
public:
    HashTableBalance() {
        for (int i = 0; i < TABLE_SIZE; i++) table[i] = nullptr;
    }
    
    ~HashTableBalance() {
        for (int i = 0; i < TABLE_SIZE; i++) {
            NodeHash* current = table[i];
            while (current != nullptr) {
                NodeHash* next = current->next;
                delete current;
                current = next;
            }
        }
    }
    
    double get(const string& key) {
        int index = hashFunction(key);
        NodeHash* current = table[index];
        while (current != nullptr) {
            if (current->key == key) return current->value;
            current = current->next;
        }
        return 0.0;
    }
    
    bool contains(const string& key) {
        int index = hashFunction(key);
        NodeHash* current = table[index];
        while (current != nullptr) {
            if (current->key == key) return true;
            current = current->next;
        }
        return false;
    }
    
    void update(const string& key, double value) {
        int index = hashFunction(key);
        NodeHash* current = table[index];
        while (current != nullptr) {
            if (current->key == key) {
                current->value = value;
                return;
            }
            current = current->next;
        }
        NodeHash* newNode = new NodeHash(key, value);
        newNode->next = table[index];
        table[index] = newNode;
    }
    
    void printAll() {
        cout << "\n" << CYAN << "  Account Balances:\n" << RESET;
        printSeparator();
        for (int i = 0; i < TABLE_SIZE; i++) {
            NodeHash* current = table[i];
            while (current != nullptr) {
                cout << "  " << setw(15) << left << current->key << ": " 
                     << YELLOW << "$" << fixed << setprecision(2) << right 
                     << setw(10) << current->value << RESET << "\n";
                current = current->next;
            }
        }
        printSeparator();
    }

    bool hasSufficientBalance(const string& key, double amount) {
        return get(key) >= amount;
    }

    void saveToFile(ofstream& file) const {
        for (int i = 0; i < TABLE_SIZE; i++) {
            NodeHash* current = table[i];
            while (current != nullptr) {
                file << current->key << "\n" << current->value << "\n";
                current = current->next;
            }
        }
        file << "END_BALANCES\n";
    }

    void loadFromFile(ifstream& file) {
        string key;
        double value;
        while (getline(file, key)) {
            if (key == "END_BALANCES") break;
            file >> value;
            file.ignore();
            update(key, value);
        }
    }
};

// ==================== BLOCKCHAIN CLASS ====================
class Blockchain {
public:
    LinkedListBlock chain;
    int difficulty;

    Blockchain(int diff) : difficulty(diff) {
        if (difficulty > 5) difficulty = 5;  // Safety limit
        
        Transaction* genesisTxs = nullptr;
        Block* genesis = new Block(0, 0, genesisTxs, 0);
        genesis->hash = 0;
        chain.append(genesis);
    }

    Block* getLatestBlock() {
        return chain.get(chain.getSize() - 1);
    }

    void addBlock(Transaction* transactions, int count) {
        Block* newBlock = new Block(chain.getSize(), getLatestBlock()->hash, transactions, count);
        newBlock->mineBlock(difficulty);
        chain.append(newBlock);
    }

    bool isChainValid() {
        NodeBlock* current = chain.getHead()->next;
        NodeBlock* previous = chain.getHead();
        
        while (current != nullptr) {
            if (current->block->prevHash != previous->block->hash) {
                printError("Invalid previous hash link!");
                return false;
            }
            previous = current;
            current = current->next;
        }
        return true;
    }

    void displayBlockchain() {
        for (int i = 0; i < chain.getSize(); i++) {
            cout << chain.get(i)->toString() << endl;
        }
    }

    void displaySummary() {
        cout << "\n" << CYAN << BOLD << "Blockchain Summary:" << RESET << "\n";
        printSeparator();
        cout << "  Total Blocks: " << GREEN << chain.getSize() << RESET << "\n";
        cout << "  Difficulty  : " << YELLOW << difficulty << RESET << "\n";
        cout << "  Latest Hash : " << GREEN << getLatestBlock()->hash << RESET << "\n";
        printSeparator();
    }

    void rebuildBSTIndex(BSTTransactionIndex* bst) {
        bst->clear();
        for (int i = 0; i < chain.getSize(); i++) {
            Block* block = chain.get(i);
            for (int j = 0; j < block->txCount; j++) {
                bst->insert(block->transactions[j].id, i, j);
            }
        }
    }

    bool saveToFile(const string& filename, HashTableBalance* balances) {
        ofstream file(filename + ".bc");
        if (!file.is_open()) {
            return false;
        }

        file << difficulty << "\n";
        file << chain.getSize() << "\n";
        file << Transaction::nextId << "\n";

        NodeBlock* current = chain.getHead();
        while (current != nullptr) {
            current->block->saveToFile(file);
            current = current->next;
        }

        balances->saveToFile(file);
        
        file.close();
        return true;
    }

    bool loadFromFile(const string& filename, HashTableBalance* balances, BSTTransactionIndex* bst) {
        ifstream file(filename + ".bc");
        if (!file.is_open()) {
            return false;
        }

        // Clear existing chain and BST
        NodeBlock* current = chain.getHead();
        while (current != nullptr) {
            NodeBlock* next = current->next;
            delete current->block;
            delete current;
            current = next;
        }
        chain = LinkedListBlock();
        bst->clear();

        int chainSize;
        file >> difficulty >> chainSize >> Transaction::nextId;
        file.ignore();

        for (int i = 0; i < chainSize; i++) {
            Block* block = new Block();
            block->loadFromFile(file);
            chain.append(block);
            
            // Index all transactions in BST
            for (int j = 0; j < block->txCount; j++) {
                bst->insert(block->transactions[j].id, i, j);
            }
        }

        balances->loadFromFile(file);
        
        file.close();
        return true;
    }
};

// ==================== UTILITY FUNCTIONS ====================
void initializeBalances(HashTableBalance& balances) {
    balances.update("Alice", 1000.0);
    balances.update("Bob", 1000.0);
    balances.update("Charlie", 1000.0);
    balances.update("David", 500.0);
    balances.update("Eve", 750.0);
}

void displayMenu() {
    cout << CYAN << BOLD;
    cout << "\n  1. " << RESET << "Create New Blockchain\n";
    cout << CYAN << BOLD << "  2. " << RESET << "Load Blockchain\n";
    cout << CYAN << BOLD << "  3. " << RESET << "Save Blockchain\n";
    cout << CYAN << BOLD << "  4. " << RESET << "Add Transaction to Pool\n";
    cout << CYAN << BOLD << "  5. " << RESET << "Mine Pending Transactions\n";
    cout << CYAN << BOLD << "  6. " << RESET << "Display Full Blockchain\n";
    cout << CYAN << BOLD << "  7. " << RESET << "View Blockchain Summary\n";
    cout << CYAN << BOLD << "  8. " << RESET << "Verify Blockchain Integrity\n";
    cout << CYAN << BOLD << "  9. " << RESET << "View Account Balances\n";
    cout << CYAN << BOLD << "  10. " << RESET << "View Pending Transactions\n";
    cout << CYAN << BOLD << "  11. " << RESET << "Search Transactions\n";
    cout << CYAN << BOLD << "  0. " << RESET << RED << "Exit\n" << RESET;
    printSeparator();
    cout << YELLOW << "  Enter your choice: " << RESET;
}

void searchMenu(Blockchain& bc, BSTTransactionIndex& bst) {
    clearScreen();
    printHeader("SEARCH TRANSACTIONS");
    
    cout << "  1. Search by Sender\n";
    cout << "  2. Search by Receiver\n";
    cout << "  3. Search by Transaction ID (BST)\n";
    cout << "  4. View Specific Block\n";
    printSeparator();
    cout << YELLOW << "  Enter choice: " << RESET;
    
    int choice;
    cin >> choice;
    
    if (choice == 1) {
        string sender;
        cout << "  Enter sender name: ";
        cin >> sender;
        cout << "\n" << CYAN << "Transactions from " << sender << ":\n" << RESET;
        printSeparator();
        bool found = false;
        for (int i = 0; i < bc.chain.getSize(); i++) {
            Block* blk = bc.chain.get(i);
            for (int j = 0; j < blk->txCount; j++) {
                if (blk->transactions[j].sender == sender) {
                    cout << "  Block " << i << ": " << blk->transactions[j].toString() << "\n";
                    found = true;
                }
            }
        }
        if (!found) printWarning("No transactions found for sender: " + sender);
    } else if (choice == 2) {
        string receiver;
        cout << "  Enter receiver name: ";
        cin >> receiver;
        cout << "\n" << CYAN << "Transactions to " << receiver << ":\n" << RESET;
        printSeparator();
        bool found = false;
        for (int i = 0; i < bc.chain.getSize(); i++) {
            Block* blk = bc.chain.get(i);
            for (int j = 0; j < blk->txCount; j++) {
                if (blk->transactions[j].receiver == receiver) {
                    cout << "  Block " << i << ": " << blk->transactions[j].toString() << "\n";
                    found = true;
                }
            }
        }
        if (!found) printWarning("No transactions found for receiver: " + receiver);
    } else if (choice == 3) {
        int id;
        cout << "  Enter transaction ID: ";
        cin >> id;
        
        int blockIdx = -1, txIdx = -1;
        bool found = bst.search(id, blockIdx, txIdx);
        
        if (found && blockIdx >= 0 && txIdx >= 0) {
            Block* blk = bc.chain.get(blockIdx);
            if (blk != nullptr && txIdx < blk->txCount) {
                cout << "\n";
                printSuccess("Transaction found using BST in O(log n) time!");
                cout << GREEN << "  Location: Block #" << blockIdx << ", Position #" << txIdx << RESET << "\n\n";
                cout << blk->transactions[txIdx].toDetailedString() << "\n";
            } else {
                printError("BST index corrupted. Transaction exists but data unavailable.");
            }
        } else {
            printError("Transaction ID " + to_string(id) + " not found in BST index");
            printInfo("Note: Only mined transactions are indexed in BST");
        }
    } else if (choice == 4) {
        int idx;
        cout << "  Enter block index: ";
        cin >> idx;
        if (idx >= 0 && idx < bc.chain.getSize()) {
            cout << bc.chain.get(idx)->toString() << endl;
        } else {
            printError("Block index out of range");
        }
    }
    
    pressEnter();
}

// ==================== MAIN FUNCTION ====================
int main() {
    Blockchain* bc = nullptr;
    PriorityQueueTransaction* pool = nullptr;
    HashTableBalance* balances = nullptr;
    BSTTransactionIndex* bst = nullptr;
    string chainName = "";
    DatabaseManager dbManager;

    if (!dbManager.connect()) {
        printWarning("Database connection failed. Running in offline mode.");
    }

    int choice;
    while (true) {
        clearScreen();
        printHeader("BLOCKCHAIN SIMULATOR");
        
        if (bc != nullptr) {
            bc->displaySummary();
        } else {
            printWarning("No blockchain loaded. Please create or load one first.");
        }
        
        displayMenu();
        cin >> choice;

        if (choice == 1) {
            clearScreen();
            printHeader("CREATE NEW BLOCKCHAIN");
            
            int difficulty;
            cout << "  Enter difficulty level (1-5): ";
            cin >> difficulty;
            
            if (difficulty < 1 || difficulty > 5) {
                printWarning("Invalid difficulty! Setting to 3 (default)");
                difficulty = 3;
            }
            
            if (bc != nullptr) {
                delete bc;
                delete pool;
                delete balances;
                delete bst;
            }

            bc = new Blockchain(difficulty);
            pool = new PriorityQueueTransaction();
            balances = new HashTableBalance();
            bst = new BSTTransactionIndex();
            initializeBalances(*balances);
            
            printSuccess("New blockchain created with genesis block!");
            printInfo("Default accounts initialized: Alice, Bob, Charlie, David, Eve");
            pressEnter();
            
        } else if (choice == 2) {
            clearScreen();
            printHeader("LOAD BLOCKCHAIN");
            
            string filename;
            cout << "  Enter blockchain name to load: ";
            cin >> filename;
            
            if (bc != nullptr) {
                delete bc;
                delete pool;
                delete balances;
                delete bst;
            }

            bc = new Blockchain(3);
            pool = new PriorityQueueTransaction();
            balances = new HashTableBalance();
            bst = new BSTTransactionIndex();

            if (dbManager.loadBlockchain(filename, bc, balances, bst)) {
                chainName = filename;
                printSuccess("Blockchain '" + filename + "' loaded successfully!");
            } else {
                printError("Failed to load blockchain '" + filename + "'.");
                delete bc;
                delete pool;
                delete balances;
                delete bst;
                bc = nullptr;
                pool = nullptr;
                balances = nullptr;
                bst = nullptr;
            }
            pressEnter();
            
        } else if (choice == 3) {
            if (bc == nullptr) {
                printError("No blockchain to save! Create or load one first.");
                pressEnter();
                continue;
            }
            
            clearScreen();
            printHeader("SAVE BLOCKCHAIN");
            
            string filename;
            cout << "  Enter blockchain name to save: ";
            cin >> filename;
            
            if (dbManager.saveBlockchain(filename, bc, balances, pool)) {
                chainName = filename;
                printSuccess("Blockchain saved as '" + filename + "'!");
            } else {
                printError("Failed to save blockchain!");
            }
            pressEnter();
            
        } else if (choice == 4) {
            if (pool == nullptr) {
                printError("Create or load blockchain first!");
                pressEnter();
                continue;
            }
            
            clearScreen();
            printHeader("ADD NEW TRANSACTION");
            
            string sender, receiver, meta;
            double amount;
            
            cout << "  Sender: ";
            cin >> sender;
            cout << "  Receiver: ";
            cin >> receiver;
            cout << "  Amount: $";
            cin >> amount;
            cout << "  Message (optional): ";
            cin.ignore();
            getline(cin, meta);
            
            if (!balances->contains(sender)) {
                printError("Sender account does not exist!");
                pressEnter();
                continue;
            }
            
            if (amount <= 0) {
                printError("Amount must be positive!");
                pressEnter();
                continue;
            }
            
            if (!balances->hasSufficientBalance(sender, amount)) {
                printError("Insufficient balance! Current balance: $" + 
                          to_string(balances->get(sender)));
                pressEnter();
                continue;
            }
            
            Transaction* tx = new Transaction(sender, receiver, amount, meta);
            pool->enqueue(tx);
            dbManager.savePendingTransaction(*tx);
            printSuccess("Transaction added to pending pool!");
            cout << tx->toString() << "\n";
            pressEnter();
            
        } else if (choice == 5) {
            if (pool == nullptr || pool->isEmpty()) {
                printWarning("No pending transactions to mine!");
                pressEnter();
                continue;
            }
            
            clearScreen();
            printHeader("MINE BLOCK");
            
            int count;
            Transaction** txs;
            pool->getAll(txs, count);
            
            cout << "\n  Mining block with " << count << " transaction(s)...\n\n";
            
            // Create array of Transaction objects (not pointers)
            Transaction* txArray = new Transaction[count];
            for (int i = 0; i < count; i++) {
                txArray[i] = *txs[i];
            }
            
            bc->addBlock(txArray, count);
            delete[] txArray;

            for (int i = 0; i < count; i++) {
                balances->update(txs[i]->sender,
                               balances->get(txs[i]->sender) - txs[i]->amount);
                balances->update(txs[i]->receiver,
                               balances->get(txs[i]->receiver) + txs[i]->amount);
                dbManager.updateBalance(txs[i]->sender, balances->get(txs[i]->sender));
                dbManager.updateBalance(txs[i]->receiver, balances->get(txs[i]->receiver));
                pool->dequeue();
            }

            delete[] txs;
            dbManager.clearPendingTransactions(chainName);
            // Update BST index with new transactions
            bc->rebuildBSTIndex(bst);
            printSuccess("Block successfully mined and added to blockchain!");
            pressEnter();
            
        } else if (choice == 6) {
            if (bc == nullptr) {
                printError("No blockchain to display!");
                pressEnter();
                continue;
            }
            clearScreen();
            printHeader("FULL BLOCKCHAIN");
            bc->displayBlockchain();
            pressEnter();
            
        } else if (choice == 7) {
            if (bc == nullptr) {
                printError("No blockchain exists!");
                pressEnter();
                continue;
            }
            clearScreen();
            printHeader("BLOCKCHAIN SUMMARY");
            bc->displaySummary();
            
            int totalTx = 0;
            for (int i = 0; i < bc->chain.getSize(); i++) {
                totalTx += bc->chain.get(i)->txCount;
            }
            cout << "  Total Transactions: " << GREEN << totalTx << RESET << "\n";
            cout << "  Pending Transactions: " << YELLOW << pool->getSize() << RESET << "\n";
            printSeparator();
            pressEnter();
            
        } else if (choice == 8) {
            if (bc == nullptr) {
                printError("No blockchain to verify!");
                pressEnter();
                continue;
            }
            clearScreen();
            printHeader("VERIFY BLOCKCHAIN");
            
            printInfo("Verifying blockchain integrity...");
            if (bc->isChainValid()) {
                printSuccess("Blockchain is valid and secure!");
            } else {
                printError("Blockchain has been tampered with!");
            }
            pressEnter();
            
        } else if (choice == 9) {
            if (balances == nullptr) {
                printError("No balances to display!");
                pressEnter();
                continue;
            }
            clearScreen();
            printHeader("ACCOUNT BALANCES");
            balances->printAll();
            pressEnter();
            
        } else if (choice == 10) {
            if (pool == nullptr) {
                printError("No transaction pool!");
                pressEnter();
                continue;
            }
            clearScreen();
            printHeader("PENDING TRANSACTIONS");
            cout << "\n";
            pool->displayPending();
            cout << "\n  Total pending: " << YELLOW << pool->getSize() << RESET << "\n";
            printSeparator();
            pressEnter();
            
        } else if (choice == 11) {
            if (bc == nullptr) {
                printError("No blockchain to search!");
                pressEnter();
                continue;
            }
            searchMenu(*bc, *bst);
            
        } else if (choice == 0) {
            clearScreen();
            printHeader("EXITING");
            printSuccess("Thank you for using Blockchain Simulator!");
            cout << "\n";
            break;
            
        } else {
            printError("Invalid choice! Please try again.");
            pressEnter();
        }
    }

    // Cleanup
    dbManager.disconnect();
    if (bc != nullptr) delete bc;
    if (pool != nullptr) delete pool;
    if (balances != nullptr) delete balances;
    if (bst != nullptr) delete bst;

    return 0;
}
