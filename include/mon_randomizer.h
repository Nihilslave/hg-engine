#ifndef MON_RANDOMIZER_H
#define MON_RANDOMIZER_H

#include "types.h"

// ---------------------------------------------------------------------------
// Ability pool & level-up move randomizer (see src/mon_randomizer.c).
//
// Activated by the opening quiz exactly like the wild-encounter randomizer:
// once difficulty + seed are stored, every species' ability pool (slot 1,
// slot 2, hidden) and its level-up learnset are drawn deterministically from
// the save seed.  Before the quiz (or on old saves) both functions fall back
// to vanilla behaviour.
// ---------------------------------------------------------------------------

/**
 *  @brief returns the randomized ability for one slot (0 = ability 1,
 *         1 = ability 2, 2 = hidden) of a species; deterministic per save
 *  @param species base species (0-1075) the ability is generated for
 *  @param slot ability slot (0-2)
 *  @param vanilla value to return while the randomizer is inactive
 */
u16 LONG_CALL MonRandom_Ability(u32 species, u32 slot, u16 vanilla);

/**
 *  @brief fills a learnset buffer (MAX_LEVELUP_MOVES u32 entries) with the
 *         species' randomized level-up moves: one move at level 1, then one
 *         random move every 5 levels; deterministic per save + species
 *  @param index learnset table index (PokeOtherFormMonsNoGet of species+form)
 *  @param species base species (used for the per-species move pool rules)
 *  @param out buffer of MAX_LEVELUP_MOVES u32 entries
 *  @return FALSE while the randomizer is inactive (caller uses vanilla data)
 */
BOOL LONG_CALL MonRandom_LearnsetTry(u32 index, u32 species, u32 *out);

#endif // MON_RANDOMIZER_H
