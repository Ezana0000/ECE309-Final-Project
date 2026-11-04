#ifndef MINI_HARNESS_UTIL_LRU_CACHE_H
#define MINI_HARNESS_UTIL_LRU_CACHE_H

#include <cstddef>
#include <functional>
#include <unordered_map>
#include <utility>
#include <vector>

// Hash lookup + manually linked list: front is MRU, back is LRU.
template <typename Key, typename Value, typename Hash = std::hash<Key>>
class LruCache {
private:
    struct Node {
        Key key;
        Value value;
        Node* prev;
        Node* next;
    };

    std::size_t capacity_;
    std::unordered_map<Key, Node*, Hash> map_;
    Node* head_ = nullptr;
    Node* tail_ = nullptr;

public:
    struct Stats {
        std::size_t hits = 0;
        std::size_t misses = 0;
        std::size_t evictions = 0;
    };

private:
    Stats stats_{};

    void unlink(Node* n) {
        if (n->prev) {
            n->prev->next = n->next;
        } else {
            head_ = n->next;
        }

        if (n->next) {
            n->next->prev = n->prev;
        } else {
            tail_ = n->prev;
        }

        n->prev = n->next = nullptr;
    }

    void push_front(Node* n) {
        n->next = head_;
        n->prev = nullptr;

        if (head_) {
            head_->prev = n;
        }
        head_ = n;
        if (!tail_) {
            tail_ = n;
        }
    }

public:
    explicit LruCache(std::size_t capacity) : capacity_(capacity) {}

    // The list nodes have single ownership; copying would double-delete them.
    LruCache(const LruCache&) = delete;
    LruCache& operator=(const LruCache&) = delete;
    LruCache(LruCache&&) = delete;
    LruCache& operator=(LruCache&&) = delete;

    ~LruCache() { clear(); }

    std::size_t size() const { return map_.size(); }
    std::size_t capacity() const { return capacity_; }
    bool empty() const { return map_.empty(); }
    const Stats& stats() const { return stats_; }

    Value* get(const Key& key) {
        auto it = map_.find(key);
        if (it == map_.end()) {
            ++stats_.misses;
            return nullptr;
        }

        Node* n = it->second;
        unlink(n);
        push_front(n);
        ++stats_.hits;
        return &n->value;
    }

    void put(const Key& key, Value value) {
        if (capacity_ == 0) {
            return;
        }

        auto it = map_.find(key);
        if (it != map_.end()) {
            Node* n = it->second;
            n->value = std::move(value);
            unlink(n);
            push_front(n);
            return;
        }

        if (size() == capacity_) {
            Node* v = tail_;
            unlink(v);
            map_.erase(v->key);
            delete v;
            ++stats_.evictions;
        }

        Node* n = new Node{key, std::move(value), nullptr, nullptr};
        push_front(n);
        map_[key] = n;
    }

    bool contains(const Key& key) const {
        return map_.find(key) != map_.end();
    }

    bool erase(const Key& key) {
        auto it = map_.find(key);
        if (it == map_.end()) {
            return false;
        }

        Node* n = it->second;
        unlink(n);
        map_.erase(it);
        delete n;
        return true;
    }

    void clear() {
        Node* n = head_;
        while (n) {
            Node* nx = n->next;
            delete n;
            n = nx;
        }
        map_.clear();
        head_ = tail_ = nullptr;
    }

    std::vector<Key> keys_mru_to_lru() const {
        std::vector<Key> keys;
        keys.reserve(size());
        for (Node* n = head_; n != nullptr; n = n->next) {
            keys.push_back(n->key);
        }
        return keys;
    }
};

#endif  // MINI_HARNESS_UTIL_LRU_CACHE_H
