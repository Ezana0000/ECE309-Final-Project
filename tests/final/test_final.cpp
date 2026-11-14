// tests/final/test_final.cpp — YOUR TEST SUITE (spec §8).
//
// You need at least 20 tests. The 8 examples below are given and count:
//   Memory  (4): heap, lru, bst, ctx
//   Policy  (2): pol x2
//   Skills  (2): graph, index
// Write at least 12 more of your own, covering the list in spec §8.
// Name tests <area>_<what> with the areas heap, lru, bst, ctx, pol, graph,
// index, so the summary groups them.
//
//   ./build/test_final              run everything
//   ./build/test_final lru_         only tests whose names start with lru_
//   ./build/test_final =lru_evicts_least_recently_used    one test, exactly
//   ./build/test_final --list       list test names
//
// If a sanitizer error (ASan/UBSan) aborts the run, the tests after it do
// not run: rerun the failing test alone with =its_name and read the FIRST
// stack frame that is in your own file.
//
// CHECK_EQ(a, b) prints both values when it fails (vectors too), so prefer
// it over CHECK(a == b).
//
// Helpers you can use (tests/test_helpers.h): TempDir, write_text,
// write_skill, FixedPolicy, ScriptedConfirmer, LinesInput, StringOutput,
// parse_ok, args1. ManualClock is in policy/policy.h.

#include "../mini_test.h"
#include "../test_helpers.h"

#include "memory/context_builder.h"
#include "memory/memory_index.h"
#include "memory/memory_store.h"
#include "policy/policies.h"
#include "policy/policy_chain.h"
#include "skills/dep_graph.h"
#include "skills/skill_catalog.h"
#include "skills/skill_index.h"
#include "util/lru_cache.h"
#include "util/min_heap.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <random>
#include <stdexcept>
#include <memory>
#include <string>
#include <utility>
#include <vector>

// =========================================================================
// Memory — 4 examples
// =========================================================================

// Example 1 (heap): items come back smallest first, duplicates included.
TEST(heap_pops_in_order) {
    MinHeap<int> h;
    for (int x : {5, 1, 4, 1, 3}) h.push(x);
    CHECK_EQ(h.size(), std::size_t{5});
    CHECK_EQ(h.top(), 1);
    std::vector<int> got;
    while (!h.empty()) got.push_back(h.pop());
    CHECK_EQ(got, (std::vector<int>{1, 1, 3, 4, 5}));
}

// Example 2 (lru): the least recently USED entry is evicted, and get()
// counts as a use.
TEST(lru_evicts_least_recently_used) {
    LruCache<std::string, int> c(2);
    c.put("a", 1);
    c.put("b", 2);
    REQUIRE(c.get("a") != nullptr);   // "a" is now the most recent
    c.put("c", 3);                    // full: "b" must go
    CHECK(c.contains("a"));
    CHECK(!c.contains("b"));
    CHECK_EQ(c.keys_mru_to_lru(), (std::vector<std::string>{"c", "a"}));
    CHECK_EQ(c.stats().evictions, std::size_t{1});
}

// Example 3 (bst): removing a node with two children keeps the tree sorted,
// keeps every other record, and lowers size() by exactly one.
TEST(bst_remove_two_children) {
    MemoryIndex t;
    for (std::uint64_t id : {50, 30, 70, 60, 80}) REQUIRE(t.insert(MemoryRecord{id, "m" + std::to_string(id)}));
    REQUIRE(t.remove(70));
    std::vector<std::uint64_t> ids;
    t.for_each_in_order([&](const MemoryRecord& r) { ids.push_back(r.id); });
    CHECK_EQ(ids, (std::vector<std::uint64_t>{30, 50, 60, 80}));
    CHECK(t.find(70) == nullptr);
    REQUIRE(t.find(80) != nullptr);
    CHECK_EQ(t.find(80)->text, std::string("m80"));
    CHECK_EQ(t.size(), std::size_t{4});
}

// Example 4 (ctx): the worked example in spec §4.4.
TEST(ctx_spec_example) {
    std::vector<ContextItem> items{
        {Role::System, 10, false},     // 0  protected
        {Role::User, 10, false},       // 1
        {Role::Assistant, 10, false},  // 2  dropped 3rd (oldest assistant)
        {Role::Tool, 10, false},       // 3  dropped 1st
        {Role::Assistant, 10, false},  // 4
        {Role::Tool, 10, false},       // 5  dropped 2nd
        {Role::User, 10, false},       // 6  recent: protected
        {Role::Assistant, 10, false}}; // 7  recent: protected
    ContextPlan p = select_context(items, 50, 2);
    CHECK_EQ(p.keep, (std::vector<std::size_t>{0, 1, 4, 6, 7}));
    CHECK_EQ(p.total_tokens, std::size_t{50});
    CHECK(!p.over_budget);
}

TEST(heap_empty_throws) {
    MinHeap<int> heap;

    CHECK_THROWS(heap.top(), std::out_of_range);
    CHECK_THROWS(heap.pop(), std::out_of_range);

    heap.push(42);

    CHECK_EQ(heap.top(), 42);
    CHECK_EQ(heap.pop(), 42);
    CHECK(heap.empty());

    CHECK_THROWS(heap.pop(), std::out_of_range);
}

TEST(heap_top_k_edges) {
    std::vector<int> values{5, 1, 9};

    CHECK(top_k(values, 0).empty());

    std::vector<int> expected{9, 5, 1};
    CHECK_EQ(top_k(values, 10), expected);

    std::vector<int> empty_values;
    CHECK(top_k(empty_values, 3).empty());

    // Compare top_k against a full sort with random data.
    std::mt19937 rng(12345);
    std::uniform_int_distribution<int> dist(-100000, 100000);

    std::vector<int> random_values;

    for (int i = 0; i < 1000; ++i) {
        random_values.push_back(dist(rng));
    }

    std::vector<int> sorted = random_values;

    std::sort(sorted.rbegin(), sorted.rend());
    sorted.resize(10);

    std::vector<int> result = top_k(random_values, 10);

    CHECK_EQ(result, sorted);

    // Performance comparison on 100,000 numbers.
    std::vector<int> data;
    data.reserve(100000);

    for (int i = 0; i < 100000; ++i) {
        data.push_back(dist(rng));
    }

    std::vector<int> sort_data = data;

    auto start_heap = std::chrono::steady_clock::now();

    std::vector<int> heap_result = top_k(data, 10);

    auto end_heap = std::chrono::steady_clock::now();

    auto start_sort = std::chrono::steady_clock::now();

    std::sort(sort_data.rbegin(), sort_data.rend());

    auto end_sort = std::chrono::steady_clock::now();

    double heap_ms =
        std::chrono::duration<double, std::milli>(
            end_heap - start_heap
        ).count();

    double sort_ms =
        std::chrono::duration<double, std::milli>(
            end_sort - start_sort
        ).count();

    std::printf(
        "top_k (100000 items, k=10): %.3f ms\n",
        heap_ms
    );

    std::printf(
        "std::sort (100000 items): %.3f ms\n",
        sort_ms
    );

    REQUIRE(!heap_result.empty());
    CHECK_EQ(heap_result.front(), sort_data.front());

    std::vector<int> expected_top(
        sort_data.begin(),
        sort_data.begin() + 10
    );

    CHECK_EQ(heap_result, expected_top);
}

TEST(lru_get_refreshes_and_update) {
    LruCache<std::string, int> cache(3);

    cache.put("a", 1);
    cache.put("b", 2);
    cache.put("c", 3);

    std::vector<std::string> initial{"c", "b", "a"};

    CHECK_EQ(cache.keys_mru_to_lru(), initial);

    // Accessing a moves it to the front.
    int* value = cache.get("a");

    REQUIRE(value != nullptr);
    CHECK_EQ(*value, 1);

    std::vector<std::string> after_get{"a", "c", "b"};

    CHECK_EQ(cache.keys_mru_to_lru(), after_get);

    // Updating an existing key should not evict anything.
    cache.put("b", 20);

    std::vector<std::string> after_update{"b", "a", "c"};

    CHECK_EQ(cache.keys_mru_to_lru(), after_update);

    int* updated = cache.get("b");

    REQUIRE(updated != nullptr);
    CHECK_EQ(*updated, 20);

    CHECK_EQ(cache.size(), 3U);
    CHECK_EQ(cache.stats().evictions, 0U);
}

TEST(lru_capacity_0_and_1) {
    // A cache with capacity zero stores nothing.
    LruCache<std::string, int> zero(0);

    zero.put("a", 1);

    CHECK_EQ(zero.size(), 0U);
    CHECK(zero.get("a") == nullptr);
    CHECK(zero.keys_mru_to_lru().empty());

    // A cache with capacity one replaces its only entry.
    LruCache<std::string, int> one(1);

    one.put("a", 1);
    CHECK(one.contains("a"));

    one.put("b", 2);

    CHECK(!one.contains("a"));
    CHECK(one.contains("b"));
    CHECK_EQ(one.size(), 1U);
    CHECK_EQ(one.stats().evictions, 1U);

    one.put("c", 3);

    CHECK(!one.contains("b"));
    CHECK(one.contains("c"));
    CHECK_EQ(one.size(), 1U);
    CHECK_EQ(one.stats().evictions, 2U);
}

TEST(lru_erase_head_middle_tail_stats) {
    LruCache<std::string, int> cache(5);

    cache.put("a", 1);
    cache.put("b", 2);
    cache.put("c", 3);
    cache.put("d", 4);
    cache.put("e", 5);

    std::vector<std::string> initial{
        "e", "d", "c", "b", "a"
    };

    CHECK_EQ(cache.keys_mru_to_lru(), initial);

    // Remove the head.
    CHECK(cache.erase("e"));

    std::vector<std::string> after_head{
        "d", "c", "b", "a"
    };

    CHECK_EQ(cache.keys_mru_to_lru(), after_head);

    // Remove a middle node.
    CHECK(cache.erase("c"));

    std::vector<std::string> after_middle{
        "d", "b", "a"
    };

    CHECK_EQ(cache.keys_mru_to_lru(), after_middle);

    // Remove the tail.
    CHECK(cache.erase("a"));

    std::vector<std::string> after_tail{
        "d", "b"
    };

    CHECK_EQ(cache.keys_mru_to_lru(), after_tail);

    // Removing an absent key returns false.
    CHECK(!cache.erase("zzz"));

    CHECK_EQ(cache.size(), 2U);

    // One hit and one miss.
    int* hit = cache.get("d");

    REQUIRE(hit != nullptr);
    CHECK_EQ(*hit, 4);

    CHECK(cache.get("x") == nullptr);

    CHECK_EQ(cache.stats().hits, 1U);
    CHECK_EQ(cache.stats().misses, 1U);

    // Erasing keys does not count as eviction.
    CHECK_EQ(cache.stats().evictions, 0U);

    // Contains should not affect statistics.
    CHECK(cache.contains("d"));
    CHECK(!cache.contains("x"));

    CHECK_EQ(cache.stats().hits, 1U);
    CHECK_EQ(cache.stats().misses, 1U);
    CHECK_EQ(cache.stats().evictions, 0U);
}

TEST(bst_remove_leaf_one_child_root_dup) {
    MemoryIndex tree;

    auto make_record = [](std::uint64_t id) {
        MemoryRecord rec{};
        rec.id = id;
        return rec;
    };

    // tree im building:
    //   50 has children 30 and 70
    //   30 has children 20 and 40
    //   70 only has a right child 80

    CHECK(tree.insert(make_record(50)));
    CHECK(tree.insert(make_record(30)));
    CHECK(tree.insert(make_record(70)));
    CHECK(tree.insert(make_record(20)));
    CHECK(tree.insert(make_record(40)));
    CHECK(tree.insert(make_record(80)));

    CHECK_EQ(tree.size(), 6U);
    CHECK_EQ(tree.height(), 3U);

    // inserting the same id twice should fail
    CHECK(!tree.insert(make_record(50)));
    CHECK_EQ(tree.size(), 6U);

    // remove a leaf
    CHECK(tree.remove(20));

    CHECK_EQ(tree.size(), 5U);
    CHECK(tree.find(20) == nullptr);

    // remove a node with one child
    CHECK(tree.remove(70));

    CHECK_EQ(tree.size(), 4U);
    CHECK(tree.find(70) == nullptr);
    CHECK(tree.find(80) != nullptr);

    // remove the root, it has two children
    CHECK(tree.remove(50));

    CHECK_EQ(tree.size(), 3U);
    CHECK(tree.find(50) == nullptr);

    std::vector<std::uint64_t> ids;

    tree.for_each_in_order(
        [&](const MemoryRecord& rec) {
            ids.push_back(rec.id);
        }
    );

    std::vector<std::uint64_t> expected{
        30, 40, 80
    };

    CHECK_EQ(ids, expected);

    // removing a id thats not there shouldnt change anything
    CHECK(!tree.remove(999));
    CHECK_EQ(tree.size(), 3U);

    // inserting in order makes the tree basically a linked list
    MemoryIndex sequential;

    for (std::uint64_t i = 1; i <= 1000; ++i) {
        REQUIRE(sequential.insert(make_record(i)));
    }

    CHECK_EQ(sequential.size(), 1000U);
    CHECK_EQ(sequential.height(), 1000U);

    std::printf(
        "BST sequential height (1000 nodes): %zu\n",
        sequential.height()
    );

    // shuffled inserts should give a way shorter tree
    std::vector<std::uint64_t> shuffled_ids;

    for (std::uint64_t i = 1; i <= 1000; ++i) {
        shuffled_ids.push_back(i);
    }

    std::mt19937 rng(12345);

    std::shuffle(
        shuffled_ids.begin(),
        shuffled_ids.end(),
        rng
    );

    MemoryIndex shuffled;

    for (std::uint64_t id : shuffled_ids) {
        REQUIRE(shuffled.insert(make_record(id)));
    }

    CHECK_EQ(shuffled.size(), 1000U);

    std::printf(
        "BST shuffled height (1000 nodes): %zu\n",
        shuffled.height()
    );

    CHECK(shuffled.height() < sequential.height());
}

TEST(ctx_protected_and_over_budget) {
    // case A: tool results get dropped first
    std::vector<ContextItem> items_a{
        {Role::System, 10, false},
        {Role::Tool, 10, false},
        {Role::Assistant, 10, false},
        {Role::User, 10, false},
        {Role::User, 10, false}
    };

    ContextPlan plan_a = select_context(items_a, 40, 1);

    std::vector<std::size_t> expected_a{
        0, 2, 3, 4
    };

    CHECK_EQ(plan_a.keep, expected_a);
    CHECK_EQ(plan_a.total_tokens, 40U);
    CHECK(!plan_a.over_budget);

    // case B: pinned messages stay
    // and the oldest assistant message gets dropped first
    std::vector<ContextItem> items_b{
        {Role::User, 10, true},
        {Role::Assistant, 10, false},
        {Role::Assistant, 10, false},
        {Role::User, 10, false}
    };

    ContextPlan plan_b = select_context(items_b, 30, 1);

    std::vector<std::size_t> expected_b{
        0, 2, 3
    };

    CHECK_EQ(plan_b.keep, expected_b);
    CHECK_EQ(plan_b.total_tokens, 30U);
    CHECK(!plan_b.over_budget);

    // case C: protected stuff alone is over the budget
    std::vector<ContextItem> items_c{
        {Role::System, 50, false},
        {Role::User, 50, false}
    };

    ContextPlan plan_c = select_context(items_c, 60, 1);

    std::vector<std::size_t> expected_c{
        0, 1
    };

    CHECK_EQ(plan_c.keep, expected_c);
    CHECK_EQ(plan_c.total_tokens, 100U);
    CHECK(plan_c.over_budget);

    // case D: everything already fits
    std::vector<ContextItem> items_d{
        {Role::System, 10, false},
        {Role::Tool, 10, false},
        {Role::Assistant, 10, false},
        {Role::User, 10, false}
    };

    ContextPlan plan_d = select_context(items_d, 100, 1);

    std::vector<std::size_t> expected_d{
        0, 1, 2, 3
    };

    CHECK_EQ(plan_d.keep, expected_d);
    CHECK_EQ(plan_d.total_tokens, 40U);
    CHECK(!plan_d.over_budget);
}

// =========================================================================
// Policy — 2 examples
// =========================================================================

// Example 5 (pol): the sliding window. A request exactly `window` seconds
// after an allowed one no longer counts it.
TEST(pol_rate_limit_window_boundary) {
    ManualClock clock(100);
    RateLimitPolicy p(2, 10.0, clock);   // at most 2 per 10 s
    CommandRequest req;
    req.program = "ls";
    CHECK(p.evaluate(req).kind == Decision::Kind::Allow);   // t = 100
    clock.advance(1);
    CHECK(p.evaluate(req).kind == Decision::Kind::Allow);   // t = 101
    clock.advance(1);
    CHECK(p.evaluate(req).kind == Decision::Kind::Deny);    // t = 102: two in the window
    clock.advance(8);
    CHECK(p.evaluate(req).kind == Decision::Kind::Allow);   // t = 110: 100 has left (110 - 100 = 10)
    CHECK(p.tracked() <= 2u);
}

// Example 6 (pol): first Deny wins and later policies are not asked; with no
// Deny, the FIRST Ask is returned unchanged.
TEST(pol_chain_first_deny_stops_first_ask_wins) {
    auto ask1 = std::make_unique<FixedPolicy>(Decision::Kind::Ask, "ask1");
    auto ask2 = std::make_unique<FixedPolicy>(Decision::Kind::Ask, "ask2");
    PolicyChain chain;
    REQUIRE(chain.add(std::make_unique<FixedPolicy>(Decision::Kind::Allow, "ok")));
    REQUIRE(chain.add(std::move(ask1)));
    REQUIRE(chain.add(std::move(ask2)));
    CommandRequest req;
    req.program = "git";
    Decision d = chain.evaluate(req);
    CHECK(d.kind == Decision::Kind::Ask);
    CHECK_EQ(d.policy, std::string("ask1"));

    auto deny = std::make_unique<FixedPolicy>(Decision::Kind::Deny, "no");
    auto after = std::make_unique<FixedPolicy>(Decision::Kind::Allow, "after");
    FixedPolicy* after_ptr = after.get();   // still valid: the chain owns it now
    REQUIRE(chain.add(std::move(deny)));
    REQUIRE(chain.add(std::move(after)));
    d = chain.evaluate(req);
    CHECK(d.kind == Decision::Kind::Deny);
    CHECK_EQ(d.policy, std::string("no"));
    CHECK_EQ(after_ptr->calls, 0);     // never evaluated: the chain stopped at the Deny
}

// =========================================================================
// Skills — 2 examples
// =========================================================================

// Example 7 (graph): post-order load order, and a cycle message that lists
// only the nodes still on the DFS path (b is already finished).
TEST(graph_order_and_cycle_message) {
    DepGraph g;
    for (const char* n : {"a", "b", "c", "d"}) g.add_node(n);
    g.add_edge("a", "b");   // a requires b
    g.add_edge("a", "c");
    g.add_edge("b", "d");
    g.add_edge("c", "d");   // diamond: d is reached twice, but it is not a cycle
    std::vector<std::string> order;
    std::string error;
    REQUIRE(g.order_from("a", &order, &error));
    CHECK_EQ(order, (std::vector<std::string>{"d", "b", "c", "a"}));

    g.add_edge("c", "a");   // now a -> c -> a is a cycle
    CHECK(!g.order_from("a", &order, &error));
    CHECK_EQ(error, std::string("cycle: a -> c -> a"));
}

// Example 8 (index): rank by number of distinct query words; ties go to the
// smaller doc id.
TEST(index_ranking_and_ties) {
    SkillIndex ix;
    ix.add(0, "format numbers");
    ix.add(1, "csv report with table and numbers");
    ix.add(2, "table format");
    ix.add(3, "unrelated");
    CHECK_EQ(ix.search("csv table numbers", 10), (std::vector<std::size_t>{1, 0, 2}));
    CHECK_EQ(ix.search("format table", 1), (std::vector<std::size_t>{2}));
    REQUIRE(ix.postings("numbers") != nullptr);
    CHECK_EQ(*ix.postings("numbers"), (std::vector<std::size_t>{0, 1}));
}

// =========================================================================
// Your tests (at least 12 more). Spec §8 lists what they must cover.
// =========================================================================

int main(int argc, char** argv) { return mt::run_all(argc, argv); }
