#include "matcher.hpp"

#include <iostream>
#include <string>

int main() {
    using smart_lost_found::Item;
    using smart_lost_found::score;

    Item lost{
        "lost", "Black water bottle", "Black bottle with a small blue sticker",
        "Other", "Library", "2026-09-30"
    };
    Item found{
        "found", "Black bottle", "Water bottle with blue sticker on it",
        "Other", "Main Library", "2026-09-30"
    };

    const auto result = score(lost, found);
    std::cout << "Potential match: " << result.percentage << "%\n";
    std::cout << "Reasons:";
    for (const auto& reason : result.reasons) std::cout << " " << reason << ";";
    std::cout << "\nNote: verify ownership before returning an item.\n";
    return 0;
}
