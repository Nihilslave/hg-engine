#ifndef FLAT_BATTLE_H
#define FLAT_BATTLE_H

#include "types.h"

struct Party;
struct BattleSystem;
struct BattleStruct;

// ---------------------------------------------------------------------------
// Gym leader "flat battle" system (link-battle-style gym battles)
//
// When a gym leader battle starts, the player's battle party is forced to
// the leader's ace level (the highest level in the leader's party), no
// experience is gained during the battle, and items cannot be used - just
// like a link battle with flat rules:
//
//   - src/field/enemy_party.c detects the leader when it builds the
//     trainer's party and calls FlatBattle_Begin on the player's battle
//     party (a copy of the save party).
//   - FlatBattle_Begin fully heals the team (like a Pokemon Center), then
//     computes each party member's stat line at the flat level by raising
//     it for a moment and immediately putting the original level,
//     experience and stats back.  The party itself is never left modified,
//     so the post-battle write-back can not leak a flat level into the
//     save.
//   - FlatBattle_Enforce is called every battle frame: whenever a battlemon
//     is (re)built from the party, its level and stats are patched to the
//     flat stat line.  hp is tracked in flat-level terms and clamped on the
//     new max hp.
//   - src/battle/battle_script_commands.c skips the whole experience
//     (and EV) distribution while FlatBattle_IsActive() is true.
//   - src/battle/battle_controller_player.c bans item usage while
//     FlatBattle_IsActive() is true, and calls FlatBattle_Finish when the
//     battle ends (win or loss).  Like a link battle, the battle must leave
//     no lasting effect on the party, so FlatBattle_Finish simply resets the
//     battle party copy to the save party: the results write-back that
//     follows every normal battle then has nothing left to copy.  Defeats
//     still black the player out, because the gym scripts call WhiteOut
//     themselves when a gym battle is lost.
//
// The leader's own battle items are cleared when the flat battle begins, so
// the AI cannot use them either.
// ---------------------------------------------------------------------------

/**
 *  @brief returns a non-zero value while a flat (gym leader) battle is active
 */
u32 LONG_CALL FlatBattle_IsActive(void);

/**
 *  @brief returns TRUE if the given trainer class is a gym leader class
 *         (all Johto and Kanto leaders, including Blue)
 */
BOOL LONG_CALL FlatBattle_IsGymLeaderClass(u32 trainerClass);

/**
 *  @brief starts a flat battle: fully heals the team and computes the
 *         flat-level stat line of every party member.  The party itself is
 *         not left modified; the active battlemons are patched instead (see
 *         FlatBattle_Enforce).
 *  @param level flat level (the leader's ace level)
 *  @param party the player's battle party (the battle setup copy)
 */
void LONG_CALL FlatBattle_Begin(u32 level, struct Party *party);

/**
 *  @brief gives the battle party copy the flat-level display values (level,
 *         stats and a fully healed flat-level hp/max hp pair) so that the
 *         battle and the in-battle party lists agree with each other.
 *         Experience is never touched, and the whole copy is discarded when
 *         the battle ends (FlatBattle_Finish), so none of this can leak
 *         into the save.  Call once after the battle setup finished
 *         modifying the party copy (battle form changes recalculate stats
 *         and clamp hp), right before the battle starts.
 *  @param party the player's battle party (the battle setup copy)
 */
void LONG_CALL FlatBattle_ApplyBattleEntryValues(struct Party *party);

/**
 *  @brief keeps the player's active battlemons on the flat stat line.  Call
 *         every battle frame after the controller command dispatch: it
 *         re-patches any battlemon whose stat data was rebuilt from the
 *         party copy (initial send-out, switch-in, reload).
 *  @param bsys battle system (for the battler count)
 *  @param ctx  battle structure (battlemons, selected party slots)
 */
void LONG_CALL FlatBattle_Enforce(struct BattleSystem *bsys, struct BattleStruct *ctx);

/**
 *  @brief ends a flat battle (win or loss): like a link battle, discards
 *         everything the battle did to the party copy by resetting it to
 *         the save party, so the results write-back that follows every
 *         normal battle cannot change the party.  Called from the battle
 *         controller (COMMAND_45) before the battle app exits.
 *  @param bsys battle system (the battle party copy is read from it)
 */
void LONG_CALL FlatBattle_Finish(struct BattleSystem *bsys);

#endif // FLAT_BATTLE_H
