#include "../include/mon_randomizer.h"
#include "../include/save.h"
#include "../include/types.h"

#include "../include/battle.h"
#include "../include/constants/ability.h"
#include "../include/constants/file.h"
#include "../include/constants/generated/learnsets.h"
#include "../include/constants/moves.h"
#include "../include/move_data.h"

// ---------------------------------------------------------------------------
// Ability pool & level-up move randomizer.
//
// Active as soon as the player's opening quiz stored a difficulty and a save
// seed (same gate as src/encounter_remap.c); inactive before that and on old
// saves, in which case every caller keeps its vanilla value.
//
//   - every species' ability pool gets three random, pairwise distinct
//     abilities: slot 1, slot 2 and the hidden ability slot
//   - every species' learnset becomes one move at level 1 plus one random
//     move every 5 levels (levels 1, 5, 10, ..., 100); only moves the battle
//     engine implements are drawn
//   - the three drawn starter species are special-cased: their ability pool
//     never contains a negative ability; their level-1 move is never a
//     status move, and their level-5 move (the starter is received at level
//     5) always is one
//
// Everything is a pure function of (seed, species) - no sequential PRNG
// state - so the same species always generates the same pool/learnset
// regardless of when or how often it is queried.  Whether a move may be
// drawn is decided at runtime from the move data the battle engine actually
// ships (ARC_MOVE_DATA), so the draw pool can never drift from it.
// ---------------------------------------------------------------------------

// script vars (mirrors asm/include/vars.inc; shared with src/encounter_remap.c)
#define VAR_STARTER_DIFF 0x4034
#define VAR_STARTER_MON_0 0x4038
#define VAR_STARTER_MON_1 0x4039
#define VAR_STARTER_MON_2 0x403A
#define VAR_ENCOUNTER_SEED_LO 0x4042
#define VAR_ENCOUNTER_SEED_HI 0x4043

// learnset entry format: (level << 16) | move, 0xFFFF terminates the list
#define LEARNSET_LEVEL_SHIFT 16
#define LEARNSET_END 0xFFFFu
#define LEARNSET_FIRST_LEVEL 1
#define LEARNSET_LAST_LEVEL 100
#define LEARNSET_LEVEL_STEP 5
// one entry at level 1 plus one every 5 levels (5, 10, ..., 100)
#define LEARNSET_ENTRY_COUNT ((LEARNSET_LAST_LEVEL / LEARNSET_LEVEL_STEP) + 1)
// draws are retried when the result collides with an existing entry (or
// violates a starter rule)
#define LEARNSET_DRAW_ATTEMPTS 32
#define ABILITY_DRAW_ATTEMPTS 32

// draw ranges: moves 1..NUM_OF_MOVES-1, abilities 1..ABILITY_AURA_GUARD
// (id 0 is always "none" and never drawn)
#define MOVE_ID_MAX (NUM_OF_MOVES - 1)
#define ABILITY_ID_MAX ABILITY_AURA_GUARD
#define MOVE_SEEN_BYTES ((MOVE_ID_MAX / 8) + 1)

// move draw roles (starter rules)
#define MOVE_ROLE_ANY 0
#define MOVE_ROLE_NON_STATUS 1
#define MOVE_ROLE_STATUS 2

static BOOL sActive; // TRUE once the quiz stored a difficulty + seed
static u32 sSeed;
static u16 sStarters[3]; // base species of the three starter candidates

/**
 *  @brief refreshes the randomizer state from the save (cheap; call before
 *         every query while the result is not cached)
 */
static void Prepare(void)
{
    u16 diff = GetScriptVar(VAR_STARTER_DIFF);
    u16 seedLo = GetScriptVar(VAR_ENCOUNTER_SEED_LO);
    u16 seedHi = GetScriptVar(VAR_ENCOUNTER_SEED_HI);

    if (diff < 1 || diff > 3 || (seedLo == 0 && seedHi == 0)) {
        sActive = FALSE; // quiz not finished / old save: keep vanilla data
        return;
    }

    sSeed = ((u32)seedHi << 16) | seedLo;
    if (sSeed == 0) {
        sSeed = 0x9E3779B9u;
    }

    sStarters[0] = GetScriptVar(VAR_STARTER_MON_0) & 0x7FF;
    sStarters[1] = GetScriptVar(VAR_STARTER_MON_1) & 0x7FF;
    sStarters[2] = GetScriptVar(VAR_STARTER_MON_2) & 0x7FF;

    sActive = TRUE;
}

static BOOL IsStarterSpecies(u32 species)
{
    return species != 0
        && (species == sStarters[0] || species == sStarters[1] || species == sStarters[2]);
}

static BOOL IsNegativeAbility(u16 ability)
{
    return ability == ABILITY_TRUANT
        || ability == ABILITY_STALL
        || ability == ABILITY_KLUTZ
        || ability == ABILITY_SLOW_START
        || ability == ABILITY_DEFEATIST;
}

/**
 *  @brief 32-bit finalizer; turns (seed, key, salt) into a well mixed PRNG
 *         state so independent queries do not correlate
 */
static u32 Mix32(u32 x)
{
    x ^= x >> 16;
    x *= 0x7FEB352Du;
    x ^= x >> 15;
    x *= 0x846CA68Bu;
    x ^= x >> 16;
    return x;
}

static u32 PrngNext(u32 *state)
{
    u32 x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

/**
 *  @brief one ability draw; skips the placeholder slots the data never uses,
 *         the starter-forbidden negative abilities and the abilities of the
 *         previous slots so the pool is pairwise distinct
 */
static u16 DrawAbility(u32 *state, const u16 *prev, int numPrev, BOOL starter)
{
    u16 cand = 0;

    for (u32 attempt = 0; attempt < ABILITY_DRAW_ATTEMPTS; attempt++) {
        cand = 1 + (u16)(PrngNext(state) % ABILITY_ID_MAX);

        if (cand == ABILITY_TEMP2 || cand == ABILITY_TEMP4) {
            continue; // internal placeholder abilities
        }
        if (starter && IsNegativeAbility(cand)) {
            continue;
        }
        if (numPrev >= 1 && cand == prev[0]) {
            continue;
        }
        if (numPrev >= 2 && cand == prev[1]) {
            continue;
        }
        break;
    }

    return cand;
}

/**
 *  @brief value of one ability slot; slots build on each other so the three
 *         abilities of a species are pairwise distinct
 */
static u16 AbilitySlotValue(u32 species, u32 slot)
{
    u16 prev[2];
    int numPrev = 0;
    BOOL starter = IsStarterSpecies(species);

    for (u32 s = 0; s <= slot; s++) {
        u32 state = Mix32(sSeed ^ (species * 0x9E3779B9u) ^ (0x51u + s));
        u16 value;

        if (state == 0) {
            state = 0xA341316Cu;
        }

        value = DrawAbility(&state, prev, numPrev, starter);

        if (s == slot) {
            return value;
        }
        prev[numPrev++] = value;
    }

    return 0;
}

u16 LONG_CALL MonRandom_Ability(u32 species, u32 slot, u16 vanilla)
{
    Prepare();

    if (!sActive || slot > 2) {
        return vanilla;
    }

    return AbilitySlotValue(species, slot);
}

/**
 *  @brief one learnset move draw; checks the engine's move data directly:
 *         moves flagged as unused/unimplemented are never taught, and the
 *         starter rules restrict the draw to status / non-status moves
 */
static u16 DrawMove(u32 *state, u32 role, const u8 *seen)
{
    u16 cand = 0;

    for (u32 attempt = 0; attempt < LEARNSET_DRAW_ATTEMPTS; attempt++) {
        struct BattleMove bm;

        cand = 1 + (u16)(PrngNext(state) % MOVE_ID_MAX);
        ReadWholeNarcMemberByIdPair(&bm, ARC_MOVE_DATA, cand);

        if (bm.flag & FLAG_UNUSED_MOVE) {
            continue; // same rule the game applies when learning moves
        }
        if (role == MOVE_ROLE_NON_STATUS && bm.split == SPLIT_STATUS) {
            continue;
        }
        if (role == MOVE_ROLE_STATUS && bm.split != SPLIT_STATUS) {
            continue;
        }
        if (seen[cand >> 3] & (1u << (cand & 7))) {
            continue; // already in this learnset
        }
        break;
    }

    return cand;
}

BOOL LONG_CALL MonRandom_LearnsetTry(u32 index, u32 species, u32 *out)
{
    // one bit per move id: keeps the drawn moves of this learnset distinct
    u8 seen[MOVE_SEEN_BYTES];
    u32 state;
    int w = 0;
    BOOL starter;

    Prepare();

    if (!sActive) {
        return FALSE;
    }

    starter = IsStarterSpecies(species);

    for (int i = 0; i < MOVE_SEEN_BYTES; i++) {
        seen[i] = 0;
    }

    state = Mix32(sSeed ^ (index * 0x85EBCA6Bu) ^ 0x1D2u);
    if (state == 0) {
        state = 0x9E3779B9u;
    }

    for (u32 step = 0; step < LEARNSET_ENTRY_COUNT; step++) {
        // step 0 = level 1; steps 1..20 = levels 5, 10, ..., 100
        u32 level = step == 0 ? LEARNSET_FIRST_LEVEL : (step * LEARNSET_LEVEL_STEP);
        u32 role = MOVE_ROLE_ANY;
        u16 move;

        if (starter) {
            if (level == 1) {
                role = MOVE_ROLE_NON_STATUS; // starter's first move: never a status move
            } else if (level == 5) {
                role = MOVE_ROLE_STATUS;     // starter's second move (received at level 5): a status move
            }
        }

        move = DrawMove(&state, role, seen);
        seen[move >> 3] |= (1u << (move & 7));
        out[w++] = ((u32)level << LEARNSET_LEVEL_SHIFT) | move;
    }

    // terminator + clean fill of the unused tail of the buffer
    while (w < MAX_LEVELUP_MOVES) {
        out[w++] = LEARNSET_END;
    }

    return TRUE;
}
