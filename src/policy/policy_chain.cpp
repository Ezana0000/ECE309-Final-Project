#include "policy/policy_chain.h"

#include <cstddef>
#include <memory>
#include <utility>

// Add another policy to the chain.
bool PolicyChain::add(std::unique_ptr<Policy> child) {

    if (!child) {
        return false;
    }

    children_.push_back(std::move(child));
    return true;
}

// Return the number of direct child policies.
std::size_t PolicyChain::size() const noexcept {
    return children_.size();
}

// Evaluate policies in insertion order.
Decision PolicyChain::evaluate(
    const CommandRequest& req) {

    // An empty chain denies by default.
    if (children_.empty()) {
        return Decision::deny(
            "policy chain is empty",
            "chain"
        );
    }

    bool found_ask = false;

    Decision first_ask = Decision::ask(
        "confirmation required",
        "chain"
    );

    for (const auto& child : children_) {

        Decision result = child->evaluate(req);

        // Deny takes priority over every other decision.
        // Stop immediately and preserve the source policy.
        if (result.kind == Decision::deny(
                "denied", "chain").kind) {
            return result;
        }

        // Keep only the first Ask decision.
        if (result.kind == Decision::ask(
                "confirmation required", "chain").kind) {

            if (!found_ask) {
                first_ask = std::move(result);
                found_ask = true;
            }
        }
    }

    // Ask takes priority over Allow.
    if (found_ask) {
        return first_ask;
    }

    return Decision::allow(
        "all policies allowed the command",
        "chain"
    );
}
