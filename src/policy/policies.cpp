#include "policy/policies.h"

#include <string>
#include <utility>
#include <vector>

// Build the allowlist from the provided programs.
AllowlistPolicy::AllowlistPolicy(
    const std::vector<std::string>& programs) {

    for (const auto& program : programs) {
        programs_.insert(program);
    }
}

Decision AllowlistPolicy::evaluate(
    const CommandRequest& req) {

    if (programs_.find(req.program) != programs_.end()) {
        return Decision::allow(
            "program is on the allowlist",
            name()
        );
    }

    return Decision::deny(
        "'" + req.program + "' is not on the allowlist",
        name()
    );
}

// Store the argument patterns.
ArgumentPatternPolicy::ArgumentPatternPolicy(
    std::vector<std::string> patterns)
    : patterns_(std::move(patterns)) {}

// Reject commands containing forbidden patterns.
Decision ArgumentPatternPolicy::evaluate(
    const CommandRequest& req) {

    const std::string command = req.command_line();

    for (const auto& pattern : patterns_) {

        if (pattern.empty()) {
            continue;
        }

        if (command.find(pattern) != std::string::npos) {
            return Decision::deny(
                "command contains forbidden pattern \"" +
                pattern + "\"",
                name()
            );
        }
    }

    return Decision::allow(
        "no forbidden patterns found",
        name()
    );
}

// Build the confirmation list.
ConfirmPolicy::ConfirmPolicy(
    const std::vector<std::string>& programs) {

    for (const auto& program : programs) {
        programs_.insert(program);
    }
}

// Ask for confirmation only for listed programs.
Decision ConfirmPolicy::evaluate(
    const CommandRequest& req) {

    if (programs_.find(req.program) != programs_.end()) {
        return Decision::ask(
            "program needs confirmation",
            name()
        );
    }

    return Decision::allow(
        "program does not require confirmation",
        name()
    );
}

// Initialize the sliding-window rate limiter.
RateLimitPolicy::RateLimitPolicy(
    std::size_t max,
    double window,
    const Clock& clock)
    : max_(max),
      window_(window),
      clock_(clock) {}

// Allow at most max_ requests during the time window.
Decision RateLimitPolicy::evaluate(
    const CommandRequest& req) {

    (void)req;

    const double t = clock_.now_seconds();

    // Remove timestamps that are outside the window.
    while (!times_.empty() &&
           t - times_.front() >= window_) {
        times_.pop();
    }

    // Denied requests are never recorded.
    if (times_.size() >= max_) {
        return Decision::deny(
            "rate limit exceeded",
            name()
        );
    }

    times_.push(t);

    return Decision::allow(
        "request is within the rate limit",
        name()
    );
}

// Number of requests currently tracked.
std::size_t RateLimitPolicy::tracked() const noexcept {
    return times_.size();
}
