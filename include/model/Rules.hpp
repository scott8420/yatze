#pragma once
// model::Rules -- the scoring rules, as pure functions over five dice and a
// scorecard. GTK-free; the selftest checks every category and every Joker case.
//
// The rules are the standard ones (the box-lid edition):
//   Upper:  Ones .. Sixes      the sum of that face
//           Upper bonus        +35 when the upper six total 63 or more
//   Lower:  3 of a kind        the sum of all five (needs 3 alike)
//           4 of a kind        the sum of all five (needs 4 alike)
//           Full house         25 (three of one, two of another)
//           Small straight     30 (four in a row)
//           Large straight     40 (five in a row)
//           Yatzé              50 (five alike)
//           Chance             the sum of all five
//   Yatzé bonus: +100 for each further Yatzé, if the Yatzé box holds 50.
//   Joker (forced): a further Yatzé MUST go in its upper box if that is open;
//           else in any open lower box, where Full house / straights score in
//           full; else in any open upper box for 0.
#include <array>
#include <string>
#include <string_view>

namespace yatze::model {

using Dice = std::array<int, 5>;   // faces 1..6 (0 = not rolled yet)

enum class Category : int {
    Ones, Twos, Threes, Fours, Fives, Sixes,
    ThreeKind, FourKind, FullHouse, SmallStraight, LargeStraight, Yatze, Chance,
};
inline constexpr int kCategories = 13;
inline constexpr int kUpper      = 6;    // Ones..Sixes
inline constexpr int kBonusAt    = 63;
inline constexpr int kUpperBonus = 35;
inline constexpr int kYatzeBonus = 100;
inline constexpr int kOpen       = -1;   // a box not yet filled

inline constexpr Category category(int i) { return static_cast<Category>(i); }
inline constexpr int      index(Category c) { return static_cast<int>(c); }
inline constexpr bool     is_upper(Category c) { return index(c) < kUpper; }

std::string_view name(Category c);       // "Full house"
std::string_view rule_text(Category c);  // "25 -- three of one face and two of another"

// One player's card: 13 boxes (kOpen until filled) + how many Yatzé bonuses.
struct Card {
    std::array<int, kCategories> box{kOpen, kOpen, kOpen, kOpen, kOpen, kOpen, kOpen,
                                     kOpen, kOpen, kOpen, kOpen, kOpen, kOpen};
    int bonuses = 0;                      // count of +100s
    bool operator==(const Card&) const = default;

    bool open(Category c) const { return box[index(c)] == kOpen; }
    bool full() const;
    int  upper_sum() const;               // filled upper boxes
    int  upper_bonus() const;             // 0 or 35
    int  lower_sum() const;               // filled lower boxes
    int  total() const;                   // everything, bonuses included
};

bool is_yatze(const Dice& d);
// s018: the dice in words, for a screen reader:
// "Dice: 3, 5, 5, 6, 2. Holding dice 2, 3." / "Dice: not rolled yet"
std::string dice_words(const Dice& d, const std::array<bool, 5>& held);
bool rolled(const Dice& d);               // no zero faces
int  sum(const Dice& d);

// What `c` would score for these dice by the plain table -- no Joker.
int  base_score(Category c, const Dice& d);

// The whole decision for a roll, against a card: what each box would score and
// whether the rules let the player put it there now (the Joker narrows it).
struct Scoring {
    std::array<int, kCategories>  points{};   // what it would score
    std::array<bool, kCategories> allowed{};  // a legal place for this roll
    bool joker = false;                        // a further Yatzé
    bool bonus = false;                        // ...and it earns +100
};
Scoring score_options(const Card& card, const Dice& d);

}  // namespace yatze::model
