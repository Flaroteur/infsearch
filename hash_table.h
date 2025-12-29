#ifndef HASH_TABLE_H
#define HASH_TABLE_H

#include <cstdlib>
#include <cstring>
#include <new>


inline unsigned int hash_string(const char* str) {
    unsigned int hash = 2166136261u;
    while (*str) {
        hash ^= static_cast<unsigned char>(*str++);
        hash *= 16777619u;
    }
    return hash;
}


template<typename K, typename V>
struct HashNode {
    K key;
    V value;
    HashNode* next;
    
    HashNode(const K& k, const V& v) : key(k), value(v), next(nullptr) {}
};


template<typename K, typename V>
class HashTable {
private:
    HashNode<K, V>** buckets;
    size_t bucket_count;
    size_t size_;
    float load_factor_threshold;
    
    
    unsigned int hash(const char* key) const {
        return hash_string(key) % bucket_count;
    }
    
    
    unsigned int hash(int key) const {
        return static_cast<unsigned int>(key) % bucket_count;
    }
    
    void rehash() {
        size_t new_bucket_count = bucket_count * 2;
        HashNode<K, V>** new_buckets = static_cast<HashNode<K, V>**>(
            calloc(new_bucket_count, sizeof(HashNode<K, V>*))
        );
        
        if (!new_buckets) {
            throw std::bad_alloc();
        }
        
        
        for (size_t i = 0; i < bucket_count; i++) {
            HashNode<K, V>* node = buckets[i];
            while (node) {
                HashNode<K, V>* next = node->next;
                
                
                unsigned int new_idx = hash_string(
                    reinterpret_cast<const char*>(&node->key)
                ) % new_bucket_count;
                
                
                node->next = new_buckets[new_idx];
                new_buckets[new_idx] = node;
                
                node = next;
            }
        }
        
        free(buckets);
        buckets = new_buckets;
        bucket_count = new_bucket_count;
    }
    
public:
    HashTable() : bucket_count(16), size_(0), load_factor_threshold(0.75f) {
        buckets = static_cast<HashNode<K, V>**>(
            calloc(bucket_count, sizeof(HashNode<K, V>*))
        );
        if (!buckets) {
            throw std::bad_alloc();
        }
    }
    
    explicit HashTable(size_t initial_bucket_count) 
        : bucket_count(initial_bucket_count), size_(0), load_factor_threshold(0.75f) {
        buckets = static_cast<HashNode<K, V>**>(
            calloc(bucket_count, sizeof(HashNode<K, V>*))
        );
        if (!buckets) {
            throw std::bad_alloc();
        }
    }
    
    ~HashTable() {
        clear();
        free(buckets);
    }
    
    void insert(const K& key, const V& value) {
        
        if (static_cast<float>(size_ + 1) / bucket_count > load_factor_threshold) {
            rehash();
        }
        
        unsigned int idx = hash(key);
        
        
        HashNode<K, V>* node = buckets[idx];
        while (node) {
            if (strcmp(reinterpret_cast<const char*>(&node->key), 
                      reinterpret_cast<const char*>(&key)) == 0) {
                node->value = value;
                return;
            }
            node = node->next;
        }
        
        
        HashNode<K, V>* new_node = new HashNode<K, V>(key, value);
        new_node->next = buckets[idx];
        buckets[idx] = new_node;
        size_++;
    }
    
    bool find(const K& key, V& value) const {
        unsigned int idx = hash(key);
        HashNode<K, V>* node = buckets[idx];
        
        while (node) {
            if (strcmp(reinterpret_cast<const char*>(&node->key), 
                      reinterpret_cast<const char*>(&key)) == 0) {
                value = node->value;
                return true;
            }
            node = node->next;
        }
        
        return false;
    }
    
    bool contains(const K& key) const {
        V dummy;
        return find(key, dummy);
    }
    
    bool remove(const K& key) {
        unsigned int idx = hash(key);
        HashNode<K, V>* node = buckets[idx];
        HashNode<K, V>* prev = nullptr;
        
        while (node) {
            if (strcmp(reinterpret_cast<const char*>(&node->key), 
                      reinterpret_cast<const char*>(&key)) == 0) {
                if (prev) {
                    prev->next = node->next;
                } else {
                    buckets[idx] = node->next;
                }
                delete node;
                size_--;
                return true;
            }
            prev = node;
            node = node->next;
        }
        
        return false;
    }
    
    void clear() {
        for (size_t i = 0; i < bucket_count; i++) {
            HashNode<K, V>* node = buckets[i];
            while (node) {
                HashNode<K, V>* next = node->next;
                delete node;
                node = next;
            }
            buckets[i] = nullptr;
        }
        size_ = 0;
    }
    
    size_t size() const {
        return size_;
    }
    
    bool empty() const {
        return size_ == 0;
    }
    
    
    class Iterator {
    private:
        HashTable* table;
        size_t bucket_idx;
        HashNode<K, V>* current;
        
        void advance() {
            if (current && current->next) {
                current = current->next;
                return;
            }
            
            current = nullptr;
            bucket_idx++;
            
            while (bucket_idx < table->bucket_count) {
                if (table->buckets[bucket_idx]) {
                    current = table->buckets[bucket_idx];
                    return;
                }
                bucket_idx++;
            }
        }
        
    public:
        Iterator(HashTable* t, size_t idx, HashNode<K, V>* node)
            : table(t), bucket_idx(idx), current(node) {}
        
        Iterator& operator++() {
            advance();
            return *this;
        }
        
        bool operator!=(const Iterator& other) const {
            return current != other.current;
        }
        
        HashNode<K, V>& operator*() {
            return *current;
        }
        
        HashNode<K, V>* operator->() {
            return current;
        }
    };
    
    Iterator begin() {
        for (size_t i = 0; i < bucket_count; i++) {
            if (buckets[i]) {
                return Iterator(this, i, buckets[i]);
            }
        }
        return end();
    }
    
    Iterator end() {
        return Iterator(this, bucket_count, nullptr);
    }
};


class StringHashTable {
private:
    struct Node {
        char* key;
        int value;
        Node* next;
        
        Node(const char* k, int v) : value(v), next(nullptr) {
            key = static_cast<char*>(malloc(strlen(k) + 1));
            strcpy(key, k);
        }
        
        ~Node() {
            free(key);
        }
    };
    
    Node** buckets;
    size_t bucket_count;
    size_t size_;
    
    unsigned int hash(const char* str) const {
        return hash_string(str) % bucket_count;
    }
    
public:
    StringHashTable() : bucket_count(1024), size_(0) {
        buckets = static_cast<Node**>(calloc(bucket_count, sizeof(Node*)));
    }
    
    explicit StringHashTable(size_t initial_size) 
        : bucket_count(initial_size), size_(0) {
        buckets = static_cast<Node**>(calloc(bucket_count, sizeof(Node*)));
    }
    
    ~StringHashTable() {
        clear();
        free(buckets);
    }
    
    void insert(const char* key, int value) {
        unsigned int idx = hash(key);
        
        
        Node* node = buckets[idx];
        while (node) {
            if (strcmp(node->key, key) == 0) {
                node->value = value;
                return;
            }
            node = node->next;
        }
        
        
        Node* new_node = new Node(key, value);
        new_node->next = buckets[idx];
        buckets[idx] = new_node;
        size_++;
    }
    
    bool find(const char* key, int& value) const {
        unsigned int idx = hash(key);
        Node* node = buckets[idx];
        
        while (node) {
            if (strcmp(node->key, key) == 0) {
                value = node->value;
                return true;
            }
            node = node->next;
        }
        
        return false;
    }
    
    bool contains(const char* key) const {
        int dummy;
        return find(key, dummy);
    }
    
    void clear() {
        for (size_t i = 0; i < bucket_count; i++) {
            Node* node = buckets[i];
            while (node) {
                Node* next = node->next;
                delete node;
                node = next;
            }
            buckets[i] = nullptr;
        }
        size_ = 0;
    }
    
    size_t size() const {
        return size_;
    }
    
    
    class Iterator {
    private:
        StringHashTable* table;
        size_t bucket_idx;
        Node* current;
        
        void advance() {
            if (current && current->next) {
                current = current->next;
                return;
            }
            
            current = nullptr;
            bucket_idx++;
            
            while (bucket_idx < table->bucket_count) {
                if (table->buckets[bucket_idx]) {
                    current = table->buckets[bucket_idx];
                    return;
                }
                bucket_idx++;
            }
        }
        
    public:
        Iterator(StringHashTable* t, size_t idx, Node* node)
            : table(t), bucket_idx(idx), current(node) {}
        
        Iterator& operator++() {
            advance();
            return *this;
        }
        
        bool operator!=(const Iterator& other) const {
            return current != other.current;
        }
        
        Node& operator*() {
            return *current;
        }
        
        Node* operator->() {
            return current;
        }
    };
    
    Iterator begin() {
        for (size_t i = 0; i < bucket_count; i++) {
            if (buckets[i]) {
                return Iterator(this, i, buckets[i]);
            }
        }
        return end();
    }
    
    Iterator end() {
        return Iterator(this, bucket_count, nullptr);
    }
};

#endif 