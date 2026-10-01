#pragma once

#include <string>
#include <vector>

namespace smart_lost_found {

struct Item {
    std::string type;        // "lost" or "found"
    std::string title;
    std::string description;
    std::string category;
    std::string location;
    std::string date;        // ISO date: YYYY-MM-DD
};

struct MatchResult {
    int percentage;
    std::vector<std::string> reasons;
};

// Explainable score compatible with the factors used by the website.
// This is a ranking aid, not proof that two reports concern the same item.
MatchResult score(const Item& a, const Item& b);

} // namespace smart_lost_found
