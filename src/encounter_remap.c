#include "../include/encounter.h"
#include "../include/save.h"
#include "../include/types.h"

#include "encounter_pool_data.h"

// ---------------------------------------------------------------------------
// Wild encounter randomization.
//
// The mapping is derived from the player's opening-quiz answers (difficulty +
// the three chosen era concept words) plus a per-save random seed:
//
//   - every distinct species that appears in ANY encounter table (routes,
//     safari zone, headbutt trees, bug contest) is mapped one-to-one onto a
//     species from the candidate pool, so each area keeps the exact same
//     number of slots and distinct species
//   - candidates are final-stage / non-evolving species only; mythicals and
//     the player's three starter candidates never participate
//   - the difficulty caps the candidate pool by base stat total:
//     easy = no cap, normal <= 600, hard <= 500
//   - species of the three chosen eras are picked much more often (weight)
//   - the selection and assignment are deterministic per save (own PRNG
//     seeded from the stored seed, consuming no global LCG state)
//
// The table hooks call into the *Apply functions below; see the stubs in
// asm/other_hook.s and the entries in the `hooks` file.
// ---------------------------------------------------------------------------

#define REMAP_MAX_SPECIES 1100

// script vars (mirrors asm/include/vars.inc)
#define VAR_STARTER_DIFF 0x4034 // 1..3 = easy..hard
#define VAR_STARTER_ERA_0 0x403B
#define VAR_STARTER_ERA_1 0x4040
#define VAR_STARTER_ERA_2 0x4041
#define VAR_ENCOUNTER_SEED_LO 0x4042
#define VAR_ENCOUNTER_SEED_HI 0x4043
#define VAR_STARTER_MON_0 0x4038 // drawn starter species (excluded from the pool)
#define VAR_STARTER_MON_1 0x4039
#define VAR_STARTER_MON_2 0x403A

// how much likelier species of the chosen eras are (6:1 vs other eras)
#define ERA_WEIGHT_CHOSEN 6

// base-species start of each generation in hg-engine numbering (see
// tools/gen_encounter_pool.py)
static const u16 sGenStart[9] = { 1, 152, 252, 387, 544, 700, 772, 860, 956 };
static const u16 sGenEnd[9] = { 151, 251, 386, 493, 699, 771, 859, 955, 1075 };

static u16 sRemap[REMAP_MAX_SPECIES]; // identity by default, built on demand
static u16 sTargets[ENCOUNTER_ORIGIN_COUNT]; // rolled replacement species
static BOOL sActive; // TRUE once a valid mapping has been built

// snapshot of the save state the current mapping was built from
static u16 sSnapDiff, sSnapSeedLo, sSnapSeedHi, sSnapEra0, sSnapEra1, sSnapEra2;

static u32 sPrng;

static u32 PrngNext(void)
{
    // xorshift32
    sPrng ^= sPrng << 13;
    sPrng ^= sPrng >> 17;
    sPrng ^= sPrng << 5;
    return sPrng;
}

static int SpeciesGen(u16 species)
{
    for (int i = 0; i < 9; i++) {
        if (species >= sGenStart[i] && species <= sGenEnd[i]) {
            return i;
        }
    }
    return -1;
}

static u16 MapSpecies(u16 species)
{
    if (species < REMAP_MAX_SPECIES) {
        return sRemap[species];
    }
    return species;
}

/**
 *  @brief (re)builds the species mapping from the save's quiz answers
 */
static void BuildMapping(u16 diff, u16 seedLo, u16 seedHi, u16 era0, u16 era1, u16 era2)
{
    u16 candidates[ENCOUNTER_POOL_COUNT];
    int numCandidates = 0;
    int numRemaining;
    u16 eras[3];
    u16 starters[3];
    u32 totalWeight;
    int i, j, k;

    u32 cap4;
    switch (diff) {
    case 1: cap4 = 0xFF; break; // easy: no extra cap
    case 3: cap4 = 125; break;  // hard: bst <= 500
    default: cap4 = 150; break; // normal: bst <= 600
    }

    starters[0] = GetScriptVar(VAR_STARTER_MON_0);
    starters[1] = GetScriptVar(VAR_STARTER_MON_1);
    starters[2] = GetScriptVar(VAR_STARTER_MON_2);

    for (i = 0; i < ENCOUNTER_POOL_COUNT; i++) {
        if (sEncounterPoolBst4[i] > cap4) {
            continue;
        }
        if (sEncounterPoolSpecies[i] == starters[0] || sEncounterPoolSpecies[i] == starters[1] || sEncounterPoolSpecies[i] == starters[2]) {
            continue;
        }
        candidates[numCandidates++] = i;
    }

    eras[0] = era0;
    eras[1] = era1;
    eras[2] = era2;

    // deterministic PRNG stream for this save
    sPrng = ((u32)seedHi << 16) | seedLo;
    if (sPrng == 0) {
        sPrng = 0x9E3779B9;
    }

    for (i = 0; i < REMAP_MAX_SPECIES; i++) {
        sRemap[i] = i;
    }

    // weighted sampling without replacement of ENCOUNTER_ORIGIN_COUNT targets
    numRemaining = numCandidates;
    for (k = 0; k < ENCOUNTER_ORIGIN_COUNT; k++) {
        int pick = -1;
        u32 roll;

        if (numRemaining == 0) {
            // candidate pool smaller than the origin set (only possible on
            // hard after future pool edits): refill and allow duplicates
            numRemaining = numCandidates;
            for (i = 0; i < numCandidates; i++) {
                candidates[i] = i;
            }
        }

        totalWeight = 0;
        for (i = 0; i < numRemaining; i++) {
            int gen = SpeciesGen(sEncounterPoolSpecies[candidates[i]]);
            totalWeight += (gen == eras[0] || gen == eras[1] || gen == eras[2]) ? ERA_WEIGHT_CHOSEN : 1;
        }

        roll = PrngNext() % totalWeight;
        for (i = 0; i < numRemaining; i++) {
            int gen = SpeciesGen(sEncounterPoolSpecies[candidates[i]]);
            u32 w = (gen == eras[0] || gen == eras[1] || gen == eras[2]) ? ERA_WEIGHT_CHOSEN : 1;

            if (roll < w) {
                pick = i;
                break;
            }
            roll -= w;
        }
        if (pick < 0) {
            pick = numRemaining - 1;
        }

        sTargets[k] = sEncounterPoolSpecies[candidates[pick]];
        candidates[pick] = candidates[--numRemaining]; // swap-remove
    }

    // shuffle the targets so the assignment order carries no pattern
    for (i = ENCOUNTER_ORIGIN_COUNT - 1; i > 0; i--) {
        u16 tmp;
        j = PrngNext() % (i + 1);
        tmp = sTargets[i];
        sTargets[i] = sTargets[j];
        sTargets[j] = tmp;
    }

    for (i = 0; i < ENCOUNTER_ORIGIN_COUNT; i++) {
        u16 origin = sEncounterOriginSpecies[i];
        if (origin < REMAP_MAX_SPECIES) {
            sRemap[origin] = sTargets[i];
        }
    }
}

/**
 *  @brief refreshes the mapping when needed; inactive until the quiz was
 *         answered (no seed stored yet)
 */
static void PrepareRemap(void)
{
    u16 diff = GetScriptVar(VAR_STARTER_DIFF);
    u16 seedLo = GetScriptVar(VAR_ENCOUNTER_SEED_LO);
    u16 seedHi = GetScriptVar(VAR_ENCOUNTER_SEED_HI);
    u16 era0 = GetScriptVar(VAR_STARTER_ERA_0);
    u16 era1 = GetScriptVar(VAR_STARTER_ERA_1);
    u16 era2 = GetScriptVar(VAR_STARTER_ERA_2);

    if (diff < 1 || diff > 3 || (seedLo == 0 && seedHi == 0)) {
        sActive = FALSE; // quiz not finished / old save: keep vanilla encounters
        return;
    }

    if (sActive && diff == sSnapDiff && seedLo == sSnapSeedLo && seedHi == sSnapSeedHi
        && era0 == sSnapEra0 && era1 == sSnapEra1 && era2 == sSnapEra2) {
        return; // already built for this save state
    }

    BuildMapping(diff, seedLo, seedHi, era0, era1, era2);

    sSnapDiff = diff;
    sSnapSeedLo = seedLo;
    sSnapSeedHi = seedHi;
    sSnapEra0 = era0;
    sSnapEra1 = era1;
    sSnapEra2 = era2;
    sActive = TRUE;
}

static u16 Remap(u16 species)
{
    if (!sActive || species == 0) {
        return species;
    }
    return MapSpecies(species);
}

/**
 *  @brief remaps every species field of a loaded map encounter table; called
 *         right after WildEncounters_ReadFromNarc loaded it (asm hook)
 */
void EncounterRemap_ApplyToTable(EncounterData *table)
{
    if (table == NULL) {
        return;
    }
    PrepareRemap();
    if (!sActive) {
        return;
    }

    for (int i = 0; i < NUM_ENCOUNTERS_LAND; i++) {
        table->landSlots.speciesMorning[i] = Remap(table->landSlots.speciesMorning[i]);
        table->landSlots.speciesDay[i] = Remap(table->landSlots.speciesDay[i]);
        table->landSlots.speciesNight[i] = Remap(table->landSlots.speciesNight[i]);
    }
    for (int i = 0; i < NUM_ENCOUNTERS_SOUNDS; i++) {
        table->hoennSoundSpecies[i] = Remap(table->hoennSoundSpecies[i]);
        table->sinnohSoundSpecies[i] = Remap(table->sinnohSoundSpecies[i]);
    }
    for (int i = 0; i < NUM_ENCOUNTERS_SURF; i++) {
        table->surfSlots[i].species = Remap(table->surfSlots[i].species);
    }
    for (int i = 0; i < NUM_ENCOUNTERS_ROCKSMASH; i++) {
        table->rockSmashSlots[i].species = Remap(table->rockSmashSlots[i].species);
    }
    for (int i = 0; i < NUM_ENCOUNTERS_FISH; i++) {
        table->oldRodSlots[i].species = Remap(table->oldRodSlots[i].species);
        table->goodRodSlots[i].species = Remap(table->goodRodSlots[i].species);
        table->superRodSlots[i].species = Remap(table->superRodSlots[i].species);
    }
    table->landSwarm = Remap(table->landSwarm);
    table->surfSwarm = Remap(table->surfSwarm);
    table->nightFish = Remap(table->nightFish);
    table->fishSwarm = Remap(table->fishSwarm);
}

/**
 *  @brief remaps the 3x6 headbutt slots of a loaded headbutt table (asm hook)
 */
void EncounterRemap_HeadbuttTable(u8 *table)
{
    // HeadbuttEncounterData: u16 numRegularTrees, u16 numSecretTrees,
    // then common[6], rare[6], secret[6] of { u16 species; u8 min; u8 max; }
    if (table == NULL) {
        return;
    }
    PrepareRemap();
    if (!sActive) {
        return;
    }

    u8 *slots = table + 4;
    for (int i = 0; i < 18; i++) {
        u16 *species = (u16 *)(slots + i * 4);
        *species = Remap(*species);
    }
}

/**
 *  @brief remaps the 12 safari zone encounter slots built by
 *         SafariZoneAreaSet_LoadAreaEncounters (asm hook)
 */
void EncounterRemap_SafariSlots(u8 *slots)
{
    // EncounterSlot: { u16 species; u8 minLevel; u8 maxLevel; }
    if (slots == NULL) {
        return;
    }
    PrepareRemap();
    if (!sActive) {
        return;
    }

    for (int i = 0; i < 12; i++) {
        u16 *species = (u16 *)(slots + i * 4);
        *species = Remap(*species);
    }
}

/**
 *  @brief remaps the 10 bug contest entries filled by BugContest_InitEncounters
 *         (asm hook)
 */
void EncounterRemap_BugMonTable(u8 *encounters)
{
    // BUGMON: { u16 species; u8 lvlmin; u8 lvlmax; u8 rate; u8 score; u8 dummy[2]; }
    if (encounters == NULL) {
        return;
    }
    PrepareRemap();
    if (!sActive) {
        return;
    }

    for (int i = 0; i < 10; i++) {
        u16 *species = (u16 *)(encounters + i * 8);
        *species = Remap(*species);
    }
}
