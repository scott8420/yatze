# Changelog

## 1.1.1 — 2026-10-21

- **Dice never overlap** (Scott: "check your collision control"). The
  physics now tests dice against each other exactly -- edge on edge and
  face on face, not only corners -- and knocks them apart with a real bounce
  and click. Dice at rest, dice laying themselves flat, and dice gliding
  into the row are all kept apart by the same test.
- The dice lie **inside** the cup and pour out of its mouth; the cup slides
  back off them before it lifts away (it used to pass through them).
- A soft ceiling: a knock no longer throws a die higher than the tray, and
  the tray's walls are a little taller -- no more bouncing off thin air.

## 1.1.0 — 2026-10-21

- **Computer pace** (menu): *Watch every roll* (as before) or *Quicker* --
  the computer's rolls, and its pauses between steps, about twice as fast,
  the clatter sped up to match.
- The window comes back at the size you left it (or maximized).
- **Screen readers:** the dice are described ("Dice: 3, 5, 5, 6, 2. Holding
  dice 2, 3."); each roll, each box taken and each turn is announced; the
  icon-only buttons and the view orb have names.

## 1.0.0 — 2026-10-21

The first release.

- **The game:** five dice, thirteen boxes, three rolls a turn; the upper
  bonus, Yatzé bonuses and the forced Joker. Undo for holds and for the box
  you took; a roll is final.
- **Players:** 1–6 seats, people and computer players mixed freely. The
  computer plays at Easy, Medium or Hard; Hard plays optimally. A card over
  the table says whose turn it is.
- **Hints** (F9): an exact strategy engine says what to keep and which box
  to take, with the average final score each choice leads to. The advised
  dice glow on the table. The rules are in the same drawer.
- **The table:** every roll is real physics, poured from a leather cup into
  a wooden tray and filmed from your seat, with soft cast shadows. Then the
  dice line up to read. Drag the orb to turn the table, or click an axis to
  look along it; Home goes back to the standard view. Red or ivory dice.
- **Sound:** every knock of the roll, the cup's rattle, and a chime for a
  Yatzé and at game over, all synthesised. The speaker button, M or the menu
  turns it on or off.
- **Standings** (Shift+F9): this game, the high scores, every player's
  record, and each person's games over time. Scores fly into their boxes.
- Install / uninstall scripts for the GNOME overview; full keyboard play
  (Ctrl+? lists every key).
