#include "matcher.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <set>
#include <sstream>

namespace smart_lost_found {
namespace {

std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

std::set<std::string> words(const std::string& text) {
    std::set<std::string> result;
    std::string token;
    for (unsigned char c : lower(text)) {
        if (std::isalnum(c)) {
            token.push_back(static_cast<char>(c));
        } else {
            if (token.size() > 2) result.insert(token);
            token.clear();
        }
    }
    if (token.size() > 2) result.insert(token);
    return result;
}

long long civilDays(int y, unsigned m, unsigned d) {
    y -= m <= 2;
    const int era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(y - era * 400);
    const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return static_cast<long long>(era) * 146097 + static_cast<long long>(doe) - 719468;
}

bool parseDate(const std::string& s, long long& result) {
    if (s.size() != 10 || s[4] != '-' || s[7] != '-') return false;
    try {
        int y = std::stoi(s.substr(0, 4));
        unsigned m = static_cast<unsigned>(std::stoi(s.substr(5, 2)));
        unsigned d = static_cast<unsigned>(std::stoi(s.substr(8, 2)));
        if (m < 1 || m > 12 || d < 1 || d > 31) return false;
        result = civilDays(y, m, d);
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace

MatchResult score(const Item& a, const Item& b) {
    int points = 0;
    std::vector<std::string> reasons;

    if (!a.category.empty() && lower(a.category) == lower(b.category)) {
        points += 30;
        reasons.emplace_back("same category");
    }

    const auto aw = words(a.title + " " + a.description);
    const auto bw = words(b.title + " " + b.description);
    int overlap = 0;
    for (const auto& word : aw) if (bw.count(word)) ++overlap;
    if (overlap > 0) {
        points += std::min(35, overlap * 10);
        reasons.emplace_back("matching keywords");
    }

    const std::string al = lower(a.location), bl = lower(b.location);
    if (!al.empty() && !bl.empty() &&
        (al.find(bl) != std::string::npos || bl.find(al) != std::string::npos)) {
        points += 20;
        reasons.emplace_back("similar location");
    }

    long long ad = 0, bd = 0;
    if (parseDate(a.date, ad) && parseDate(b.date, bd)) {
        const long long difference = std::llabs(ad - bd);
        if (difference <= 1) {
            points += 15;
            reasons.emplace_back("nearby date");
        } else if (difference <= 7) {
            points += 8;
            reasons.emplace_back("close date");
        }
    }

    return {std::min(100, points), reasons};
}

} // namespace smart_lost_found
