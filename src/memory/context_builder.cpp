#include "memory/context_builder.h"
#include "util/min_heap.h"

#include <cstddef>
#include <utility>
#include <vector>

// picks which messages to keep so the total fits in the budget
ContextPlan select_context(
    const std::vector<ContextItem>& items,
    std::size_t budget,
    std::size_t keep_recent) {

    const std::size_t n = items.size();

    // index where the recent messages start, these are protected
    // check keep_recent >= n first so it doesnt underflow
    const std::size_t recent_start =
        (keep_recent >= n) ? 0 : n - keep_recent;

    std::size_t total = 0;

    // start by keeping everything
    std::vector<bool> keep(n, true);

    for (const ContextItem& item : items) {
        total += item.tokens;
    }

    // only need to drop stuff if were over the budget
    if (total > budget) {

        // min heap of (drop rank, index) so the lowest rank comes out first
        // and if the ranks tie the older message comes out first
        MinHeap<std::pair<int, std::size_t>> heap;

        for (std::size_t i = 0; i < n; ++i) {

            // system, pinned and recent messages never get dropped
            const bool protected_item =
                items[i].role == Role::System ||
                items[i].pinned ||
                i >= recent_start;

            if (!protected_item) {
                heap.push({
                    drop_rank(items[i].role),
                    i
                });
            }
        }

        // keep dropping the lowest priority message till it fits
        while (total > budget && !heap.empty()) {

            // .second is the index of the message
            const std::size_t i = heap.pop().second;

            keep[i] = false;
            total -= items[i].tokens;
        }
    }

    ContextPlan plan;

    // put the kept indices in the plan in there original order
    for (std::size_t i = 0; i < n; ++i) {
        if (keep[i]) {
            plan.keep.push_back(i);
        }
    }

    plan.total_tokens = total;
    plan.over_budget = total > budget;

    return plan;
}
