#include "../include/constants/species.h"
#include "../include/pokemon.h"
#include "../include/save.h"
#include "../include/types.h"

extern u32 space_for_setmondata;
extern u32 sStarterChoiceCries[];

void LONG_CALL GetMonSpriteCharAndPlttNarcIdsEx(MON_PIC *picdata, u16 mons_no, u8 dir, u8 col, u8 form_no, u8 a5, u32 personality);

// ---------------------------------------------------------------------------
// Opening quiz (multi-round, runs in the lab intro scene; see
// data/scr_seq/scr_seq_0843_T20R0101.s):
//
//   Q1     - one fixed question: which difficulty to play (easy/normal/hard)
//   Q2-Q4  - three identical questions ("which of these three concepts
//            interests you the most?"), each showing three of the nine era
//            concept words; a word is only ever shown once, so the three
//            answers always map to three distinct eras
//
// Each answered era then gives one starter: the game draws it randomly from
// that era's pool of the chosen difficulty and stores the result in
// VAR_STARTER_MON_0..2.  Those three species fill the three Poké Balls of the
// starter-selection scene.
//
// The difficulty tiers are themed by strength:
//   1 easy   - the strongest picks (cover legends & co), for an easier run
//   2 normal - secondary legends / trios / mythicals
//   3 hard   - the regional starter trios, for a challenge run
//
// Add, remove or reorder candidates freely - pools may have different lengths
// (see STARTER_POOL_MAX below); saves keep the species they already drew, so
// edits only affect new games.
//
// Keep in mind that these choices impact the Rival team determination in the
// appropriate trigger-scripts.
//
// The starter-selection screen text (data/text/190.txt) is still the static
// Zacian/Zamazenta/Eternatus flavor text; it does not match the drawn species
// yet.
// ---------------------------------------------------------------------------

#define STARTER_ERA_COUNT 9
#define STARTER_DIFF_COUNT 3

// Maximum candidates per pool.  Pools may be shorter than this - the unused
// entries stay 0 (SPECIES_NONE) and terminate the list - so every
// era/difficulty combination can have its own number of candidates, and an
// entry can always be added or removed without touching the others.
#define STARTER_POOL_MAX 8

// menu option text lives in data/text/191.txt (the touchscreen list menu reads
// its options from archive 191):
//   STARTER_QUIZ_CONCEPT_LABEL_BASE + era index   = the era's concept word
//   STARTER_QUIZ_DIFF_LABEL_BASE + 0..2           = EASY / NORMAL / HARD
#define STARTER_QUIZ_CONCEPT_LABEL_BASE 477
#define STARTER_QUIZ_DIFF_LABEL_BASE 486

// script vars holding the quiz state; keep in sync with VAR_STARTER_* in
// asm/include/vars.inc (0x4034/0x4036/0x4038-0x403A are free in vanilla)
#define STARTER_DIFF_VAR 0x4034         // 0 = quiz not finished, 1..3 = easy..hard
#define STARTER_CONCEPT_MASK_VAR 0x4036 // bit n = concept word n not shown yet
#define STARTER_MON_VAR_BASE 0x4038     // +0..2 = drawn species of the ball slots

// the concept menu's result is read from this special var (the lab script
// passes it as the menu's resultVar to menu_init_std_gmm)
#define STARTER_MENU_RESULT_VAR 0x800C // VAR_SPECIAL_RESULT

// quiz results kept for the encounter randomizer (mirror VAR_STARTER_ERA_* and
// VAR_ENCOUNTER_SEED_* in asm/include/vars.inc; see src/encounter_remap.c)
#define STARTER_ERA_VAR_0 0x403B
#define STARTER_ERA_VAR_1 0x4040
#define STARTER_ERA_VAR_2 0x4041
#define STARTER_SEED_LO_VAR 0x4042
#define STARTER_SEED_HI_VAR 0x4043

static const u16 sStarterPools[STARTER_ERA_COUNT][STARTER_DIFF_COUNT][STARTER_POOL_MAX] = {
    {   // 0: Kanto (gen 1)
        { SPECIES_MEWTWO },             // easy
        { SPECIES_ARTICUNO, SPECIES_ZAPDOS, SPECIES_MOLTRES },          // normal
        { SPECIES_VENUSAUR, SPECIES_CHARIZARD, SPECIES_BLASTOISE },     // hard
    },
    {   // 1: Johto (gen 2)
        { SPECIES_LUGIA, SPECIES_HO_OH },               // easy
        { SPECIES_RAIKOU, SPECIES_ENTEI, SPECIES_SUICUNE },             // normal
        { SPECIES_MEGANIUM, SPECIES_TYPHLOSION, SPECIES_FERALIGATR },   // hard
    },
    {   // 2: Hoenn (gen 3)
        { SPECIES_KYOGRE, SPECIES_GROUDON, SPECIES_RAYQUAZA },          // easy
        { SPECIES_REGIROCK, SPECIES_REGICE, SPECIES_REGISTEEL },        // normal
        { SPECIES_SCEPTILE, SPECIES_BLAZIKEN, SPECIES_SWAMPERT },       // hard
    },
    {   // 3: Sinnoh (gen 4)
        { SPECIES_DIALGA, SPECIES_PALKIA, SPECIES_GIRATINA },           // easy
        { SPECIES_UXIE, SPECIES_MESPRIT, SPECIES_AZELF },               // normal
        { SPECIES_TORTERRA, SPECIES_INFERNAPE, SPECIES_EMPOLEON },      // hard
    },
    {   // 4: Unova (gen 5)
        { SPECIES_RESHIRAM, SPECIES_ZEKROM, SPECIES_KYUREM },           // easy
        { SPECIES_COBALION, SPECIES_TERRAKION, SPECIES_VIRIZION, SPECIES_TORNADUS, SPECIES_THUNDURUS, SPECIES_LANDORUS },      // normal
        { SPECIES_SERPERIOR, SPECIES_EMBOAR, SPECIES_SAMUROTT },        // hard
    },
    {   // 5: Kalos (gen 6)
        { SPECIES_XERNEAS, SPECIES_YVELTAL },          // easy
        { SPECIES_ZYGARDE },          // normal
        { SPECIES_CHESNAUGHT, SPECIES_DELPHOX, SPECIES_GRENINJA },      // hard
    },
    {   // 6: Alola (gen 7)
        { SPECIES_SOLGALEO, SPECIES_LUNALA },                         // easy
        { SPECIES_TAPU_KOKO, SPECIES_TAPU_LELE, SPECIES_TAPU_BULU, SPECIES_TAPU_FINI, SPECIES_NECROZMA }, // normal (all four tapus)
        { SPECIES_DECIDUEYE, SPECIES_INCINEROAR, SPECIES_PRIMARINA },                   // hard
    },
    {   // 7: Galar (gen 8)
        { SPECIES_ZACIAN, SPECIES_ZAMAZENTA, SPECIES_ETERNATUS },       // easy
        { SPECIES_URSHIFU, SPECIES_REGIELEKI, SPECIES_REGIDRAGO, SPECIES_GLASTRIER, SPECIES_SPECTRIER, SPECIES_ENAMORUS },      // normal
        { SPECIES_RILLABOOM, SPECIES_CINDERACE, SPECIES_INTELEON, SPECIES_CALYREX },     // hard
    },
    {   // 8: Paldea (gen 9)
        { SPECIES_KORAIDON, SPECIES_MIRAIDON },      // easy
        { SPECIES_WO_CHIEN, SPECIES_CHIEN_PAO, SPECIES_TING_LU, SPECIES_CHI_YU },       // normal
        { SPECIES_MEOWSCARADA, SPECIES_SKELEDIRGE, SPECIES_QUAQUAVAL, SPECIES_TERAPAGOS }, // hard
    },
};

/**
 *  @brief rolls the next three concept words for one quiz question, writing
 *         the results into six consecutive special vars (called via the
 *         starter_quiz_roll_concepts script command)
 *
 *         A word is removed from the pool as soon as it is shown, so across
 *         the three questions every era's concept appears exactly once.
 *
 *  @param base_var first special var, receives:
 *                  base_var + 0..2 = menu option message ids (data/text/191.txt)
 *                  base_var + 3..5 = era indices (menu option return values)
 */
void StarterQuiz_RollConcepts(u16 base_var)
{
    u16 mask = GetScriptVar(STARTER_CONCEPT_MASK_VAR);
    int picked[3];

    if ((mask & 0x1FF) == 0) { // fresh quiz (the script resets it before Q2)
        mask = 0x1FF;
    }

    for (int i = 0; i < 3; i++) {
        if ((mask & 0x1FF) == 0) { // defensive: never run a question dry
            mask = 0x1FF;
        }
        do {
            picked[i] = gf_rand() % STARTER_ERA_COUNT;
        } while (!(mask & (1 << picked[i])));
        mask &= ~(1 << picked[i]);
    }

    SetScriptVar(STARTER_CONCEPT_MASK_VAR, mask);

    for (int i = 0; i < 3; i++) {
        SetScriptVar(base_var + i, STARTER_QUIZ_CONCEPT_LABEL_BASE + picked[i]); // option text
        SetScriptVar(base_var + 3 + i, picked[i]);                               // option value
    }
}

/**
 *  @brief draws one random candidate from an era/difficulty pool; the pools
 *         are terminated by SPECIES_NONE (0), so they may have different
 *         lengths
 *
 *  @param era era index (0-8)
 *  @param diff difficulty index (0-2, easy to hard)
 *  @return the drawn species entry
 */
static u16 DrawFromPool(int era, int diff)
{
    const u16 *pool = sStarterPools[era][diff];
    int count = 0;

    while (count < STARTER_POOL_MAX && pool[count] != SPECIES_NONE) {
        count++;
    }

    if (count == 0) { // safety net: an empty pool should never be reachable
        return sStarterPools[0][1][0];
    }

    return pool[gf_rand() % count];
}

/**
 *  @brief draws one starter for the era the player just picked and stores it
 *         in the given script var (called via the starter_quiz_draw script
 *         command)
 *
 *         The era is read from VAR_SPECIAL_RESULT (the concept menu returns
 *         the chosen option's value), the difficulty from
 *         VAR_STARTER_DIFFICULTY.  The answered era is kept for the encounter
 *         randomizer; the final call additionally rolls its save seed.
 *
 *  @param mon_var script var to store the drawn species in
 */
void StarterQuiz_DrawSlot(u16 mon_var)
{
    static const u16 sEraVars[3] = { STARTER_ERA_VAR_0, STARTER_ERA_VAR_1, STARTER_ERA_VAR_2 };
    u16 era = GetScriptVar(STARTER_MENU_RESULT_VAR);
    u16 diff = GetScriptVar(STARTER_DIFF_VAR);
    int slot = (int)mon_var - STARTER_MON_VAR_BASE;

    if (era >= STARTER_ERA_COUNT) {
        era = STARTER_ERA_COUNT - 1; // defensive clamp
    }
    if (diff < 1 || diff > STARTER_DIFF_COUNT) {
        diff = 2; // default to normal
    }

    SetScriptVar(mon_var, DrawFromPool(era, diff - 1));

    if (slot >= 0 && slot < 3) {
        // remember which era the player answered (0-8)
        SetScriptVar(sEraVars[slot], era);

        if (slot == 2) {
            // quiz finished: roll the seed for the encounter randomizer
            u32 seed = ((u32)gf_rand() << 16) | gf_rand();

            if (seed == 0) {
                seed = 0x9E3779B9;
            }
            SetScriptVar(STARTER_SEED_LO_VAR, (u16)(seed & 0xFFFF));
            SetScriptVar(STARTER_SEED_HI_VAR, (u16)(seed >> 16));
        }
    }
}

/**
 *  @brief draws a random trio (three distinct eras at the quiz's difficulty,
 *         normal if unset) for saves that somehow skipped the quiz
 */
static void RollFallbackTrio(void)
{
    u16 diff = GetScriptVar(STARTER_DIFF_VAR);
    int eras[3];

    if (diff < 1 || diff > STARTER_DIFF_COUNT) {
        diff = 2;
    }

    eras[0] = gf_rand() % STARTER_ERA_COUNT;

    do {
        eras[1] = gf_rand() % STARTER_ERA_COUNT;
    } while (eras[1] == eras[0]);

    do {
        eras[2] = gf_rand() % STARTER_ERA_COUNT;
    } while (eras[2] == eras[0] || eras[2] == eras[1]);

    for (int i = 0; i < 3; i++) {
        SetScriptVar(STARTER_MON_VAR_BASE + i, DrawFromPool(eras[i], diff - 1));
    }
}

/**
 *  @brief returns the species entry (including form bits) for one ball slot of
 *         this save; falls back to a random trio when the quiz never ran
 *
 *  @param slot starter choice (0-2)
 */
static u16 GetStarterSlot(int slot)
{
    u16 species = 0;

    if (slot >= 0 && slot < 3) {
        species = GetScriptVar(STARTER_MON_VAR_BASE + slot);
    }

    if (species == 0) {
        // old save, skipped quiz, ...: roll a trio like in the old days
        RollFallbackTrio();
        species = (slot >= 0 && slot < 3) ? GetScriptVar(STARTER_MON_VAR_BASE + slot) : sStarterPools[0][1][0];
    }

    return species;
}

/**
 *  @brief fills the given array with the species ids of the starter choices
 *
 *  @param species pointer to the species array
 */
void LONG_CALL CreateStarter_SetStarterSpecies(int *species)
{
    for (int i = 0; i < 3; i++) {
        // strip off form
        species[i] = GetStarterSlot(i) & 0x7FF;
    }
}

/**
 *  @brief wrap the CreateMon call within CreateStarter to allow for starter forms
 *
 *  @param mon the PartyPokemon pointer
 *  @param species the species id
 *  @param slot starter choice (0-2)
 */
void LONG_CALL CreateStarter_CreateMon(struct PartyPokemon *mon, int species, int slot)
{
    u32 form = 0;

    if (slot >= 0 && slot < 3) {
        form = GetStarterSlot(slot) >> 11;
    }

    space_for_setmondata = form;
    PokeParaSet(mon, species, 5, 32, FALSE, 0, 0, 0);
    space_for_setmondata = 0;

    if (form != 0) {
        SetMonData(mon, MON_DATA_FORM, &form);
    }
}

/**
 *  @brief wrap the GetMonSpriteCharAndPlttNarcIdsEx call within createMonSprites to handle starter forms
 *
 *  @param pic MON_PIC pointer
 *  @param species species id
 */
void LONG_CALL CreateMonSprites_HandleForm(MON_PIC *pic, u16 species, u8 gender, u8 shiny, int slot)
{
    u32 form = 0;

    if (slot >= 0 && slot < 3) {
        form = GetStarterSlot(slot) >> 11;
        sStarterChoiceCries[slot] = (form == 0) ? species : PokeOtherFormMonsNoGet(species, form);
    }

    GetMonSpriteCharAndPlttNarcIdsEx(pic, species, gender, 2, shiny, 0, 0);

    if (form != 0) {
        GetOtherFormPic(pic, species, 2, shiny, form);
    }
}
