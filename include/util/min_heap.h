#ifndef MINI_HARNESS_UTIL_MIN_HEAP_H
#define MINI_HARNESS_UTIL_MIN_HEAP_H

#include <algorithm>
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <stdexcept>
#include <utility>
#include <vector>

// A binary heap: the item at index 0 is smallest according to Less.
template <typename T, typename Less = std::less<T>>
class MinHeap {
private:
    std::vector<T> data_;
    Less less_;

    void sift_up(std::size_t i) {
        while (i > 0) {
            const std::size_t p = (i - 1) / 2;
            if (!less_(data_[i], data_[p])) {
                break;
            }
            std::swap(data_[i], data_[p]);
            i = p;
        }
    }

    void sift_down(std::size_t i) {
        while (true) {
            const std::size_t l = 2 * i + 1;
            const std::size_t r = 2 * i + 2;
            std::size_t smallest = i;

            if (l < size() && less_(data_[l], data_[smallest])) {
                smallest = l;
            }
            if (r < size() && less_(data_[r], data_[smallest])) {
                smallest = r;
            }
            if (smallest == i) {
                break;
            }
            std::swap(data_[i], data_[smallest]);
            i = smallest;
        }
    }

public:
    explicit MinHeap(Less less = Less{}) : less_(std::move(less)) {}

    std::size_t size() const { return data_.size(); }
    bool empty() const { return data_.empty(); }
    void clear() { data_.clear(); }

    void push(T value) {
        data_.push_back(std::move(value));
        sift_up(size() - 1);
    }

    const T& top() const {
        if (empty()) {
            throw std::out_of_range("empty");
        }
        return data_[0];
    }

    T pop() {
        if (empty()) {
            throw std::out_of_range("empty");
        }

        T result = std::move(data_[0]);
        // Avoid self-move-assignment when the root is also the last item.
        if (size() == 1) {
            data_.pop_back();
            return result;
        }

        data_[0] = std::move(data_.back());
        data_.pop_back();
        sift_down(0);
        return result;
    }
};

// Return the k largest items, ordered largest to smallest by Less.
// Maintains at most k elements instead of sorting the entire input.
template <typename T, typename Less = std::less<T>>
std::vector<T> top_k(const std::vector<T>& items,
                     std::size_t k,
                     Less less = Less{}) {
    if (k == 0) {
        return {};
    }

    MinHeap<T, Less> heap(less);
    for (const T& item : items) {
        if (heap.size() < k) {
            heap.push(item);
        } else if (less(heap.top(), item)) {
            heap.pop();
            heap.push(item);
        }
    }

    std::vector<T> result;
    result.reserve(heap.size());
    while (!heap.empty()) {
        result.push_back(heap.pop());
    }
    std::reverse(result.begin(), result.end());
    return result;
}

// Supports calls such as top_k({5, 1, 9, 3, 7}, 2).
template <typename T, typename Less = std::less<T>>
std::vector<T> top_k(std::initializer_list<T> items,
                     std::size_t k,
                     Less less = Less{}) {
    return top_k(std::vector<T>(items), k, std::move(less));
}

#endif  // MINI_HARNESS_UTIL_MIN_HEAP_H
