#include "model/Rules.hpp"

#include <algorithm>

namespace yatze::model {

namespace {
std::array<int, 7> counts(const Dice& d) {
    std::array<int, 7> c{};
    for (int f : d) if (f >= 1 && f <= 6) ++c[f];
    return c;
}
int max_count(const Dice& d) {
    const auto c = counts(d);
    return *std::max_element(c.begin() + 1, c.end());
}
// The longest run of consecutive faces present.
int longest_run(const Dice& d) {
    const auto c = counts(d);
    int best = 0, run = 0;
    for (int f = 1; f <= 6; ++f) {
        run = c[f] ? run + 1 : 0;
        best = std::max(best, run);
    }
    return best;
}
}  // namespace

std::string_view name(Category c) {
    switch (c) {
        case Category::Ones:          return "Ones";
        case Category::Twos:          return "Twos";
        case Category::Threes:        return "Threes";
        case Category::Fours:         return "Fours";
        case Category::Fives:         return "Fives";
        case Category::Sixes:         return "Sixes";
        case Category::ThreeKind:     return "3 of a kind";
        case Category::FourKind:      return "4 of a kind";
        case Category::FullHouse:     return "Full house";
        case Category::SmallStraight: return "Small straight";
        case Category::LargeStraight: return "Large straight";
        case Category::Yatze:         return "Yatzé!";
        case Category::Chance:        return "Chance";
    }
    return "?";
}

std::string_view rule_text(Category c) {
    switch (c) {
        case Category::Ones:          return "Add up the ones";
        case Category::Twos:          return "Add up the twos";
        case Category::Threes:        return "Add up the threes";
        case Category::Fours:         return "Add up the fours";
        case Category::Fives:         return "Add up the fives";
        case Category::Sixes:         return "Add up the sixes";
        case Category::ThreeKind:     return "Three alike: add up all five dice";
        case Category::FourKind:      return "Four alike: add up all five dice";
        case Category::FullHouse:     return "Three of one and two of another: 25";
        case Category::SmallStraight: return "Four in a row: 30";
        case Category::LargeStraight: return "Five in a row: 40";
        case Category::Yatze:         return "Five alike: 50";
        case Category::Chance:        return "Anything: add up all five dice";
    }
    return "";
}

bool is_yatze(const Dice& d) { return rolled(d) && max_count(d) == 5; }
bool rolled(const Dice& d)   { return std::none_of(d.begin(), d.end(), [](int f) { return f < 1 || f > 6; }); }
int  sum(const Dice& d)      { int s = 0; for (int f : d) s += f; return s; }

int base_score(Category c, const Dice& d) {
    if (!rolled(d)) return 0;
    if (is_upper(c)) {
        const int face = index(c) + 1;
        return counts(d)[face] * face;
    }
    switch (c) {
        case Category::ThreeKind:     return max_count(d) >= 3 ? sum(d) : 0;
        case Category::FourKind:      return max_count(d) >= 4 ? sum(d) : 0;
        case Category::FullHouse: {
            const auto k = counts(d);
            bool three = false, two = false;
            for (int f = 1; f <= 6; ++f) { if (k[f] == 3) three = true; if (k[f] == 2) two = true; }
            return three && two ? 25 : 0;
        }
        case Category::SmallStraight: return longest_run(d) >= 4 ? 30 : 0;
        case Category::LargeStraight: return longest_run(d) == 5 ? 40 : 0;
        case Category::Yatze:         return is_yatze(d) ? 50 : 0;
        case Category::Chance:        return sum(d);
        default:                      return 0;
    }
}

Scoring score_options(const Card& card, const Dice& d) {
    Scoring s;
    if (!rolled(d)) return s;   // nothing to place before the first roll
    for (int i = 0; i < kCategories; ++i) s.points[i] = base_score(category(i), d);

    s.joker = is_yatze(d) && !card.open(Category::Yatze);
    if (!s.joker) {
        for (int i = 0; i < kCategories; ++i) s.allowed[i] = card.open(category(i));
        return s;
    }

    // A further Yatzé. The bonus needs a 50 in the Yatzé box (a 0 there earns none).
    s.bonus = card.box[index(Category::Yatze)] == 50;
    s.points[index(Category::FullHouse)]     = 25;
    s.points[index(Category::SmallStraight)] = 30;
    s.points[index(Category::LargeStraight)] = 40;

    const Category face_box = category(d[0] - 1);
    if (card.open(face_box)) {                       // 1. its own upper box, forced
        s.allowed[index(face_box)] = true;
        return s;
    }
    bool any_lower = false;                          // 2. any open lower box, in full
    for (int i = kUpper; i < kCategories; ++i)
        if (card.open(category(i))) { s.allowed[i] = true; any_lower = true; }
    if (any_lower) return s;
    for (int i = 0; i < kUpper; ++i)                 // 3. any open upper box, for 0
        if (card.open(category(i))) { s.allowed[i] = true; s.points[i] = 0; }
    return s;
}

bool Card::full() const {
    return std::none_of(box.begin(), box.end(), [](int v) { return v == kOpen; });
}
int Card::upper_sum() const {
    int s = 0;
    for (int i = 0; i < kUpper; ++i) if (box[i] != kOpen) s += box[i];
    return s;
}
int Card::upper_bonus() const { return upper_sum() >= kBonusAt ? kUpperBonus : 0; }
int Card::lower_sum() const {
    int s = 0;
    for (int i = kUpper; i < kCategories; ++i) if (box[i] != kOpen) s += box[i];
    return s;
}
int Card::total() const { return upper_sum() + upper_bonus() + lower_sum() + bonuses * kYatzeBonus; }

}  // namespace yatze::model
