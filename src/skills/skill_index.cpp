#include "skills/skill_index.h"
#include "util/min_heap.h"
#include "util/text.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

// Add a document to the inverted index.
void SkillIndex::add(
    std::size_t doc_id,
    std::string_view text) {

    // unique_tokens removes repeated words
    // and normalizes them to lowercase.
    for (const auto& word : unique_tokens(text)) {
        postings_[word].push_back(doc_id);
    }
}

// Return the documents containing a word.
const std::vector<std::size_t>* SkillIndex::postings(
    std::string_view word) const {

    auto it = postings_.find(std::string(word));

    if (it == postings_.end()) {
        return nullptr;
    }

    return &it->second;
}

// Number of different indexed words.
std::size_t SkillIndex::word_count() const noexcept {
    return postings_.size();
}

// Search for the k highest-ranking matching documents.
std::vector<std::size_t> SkillIndex::search(
    std::string_view query,
    std::size_t k) const {

    if (k == 0) {
        return {};
    }

    // Maps document ID to its number of matched words.
    std::unordered_map<std::size_t, std::size_t> scores;

    for (const auto& word : unique_tokens(query)) {

        const auto* docs = postings(word);

        if (docs == nullptr) {
            continue;
        }

        // Each posting list contains the word once per doc.
        for (std::size_t doc_id : *docs) {
            ++scores[doc_id];
        }
    }

    if (scores.empty()) {
        return {};
    }

    // Store (score, document ID) pairs.
    std::vector<std::pair<std::size_t, std::size_t>> ranked;
    ranked.reserve(scores.size());

    for (const auto& entry : scores) {
        ranked.push_back({
            entry.second,
            entry.first
        });
    }

    // Lower score is worse.
    // If scores tie, larger document ID is worse.
    auto less = [](
        const std::pair<std::size_t, std::size_t>& a,
        const std::pair<std::size_t, std::size_t>& b) {

        if (a.first != b.first) {
            return a.first < b.first;
        }

        return a.second > b.second;
    };

    // Use the custom heap instead of sorting.
    auto best = top_k(ranked, k, less);

    std::vector<std::size_t> result;
    result.reserve(best.size());

    for (const auto& entry : best) {
        result.push_back(entry.second);
    }

    return result;
}
