#include "../include/constants/species.h"
#include "../include/pokemon.h"
#include "../include/save.h"
#include "../include/types.h"

extern u32 space_for_setmondata;
extern u32 sStarterChoiceCries[];

void LONG_CALL GetMonSpriteCharAndPlttNarcIdsEx(MON_PIC *picdata, u16 mons_no, u8 dir, u8 col, u8 form_no, u8 a5, u32 personality);

// Every save gets one group of three starter choices.  The opening quiz in
// T20R0201 (the player's house) asks which era interests the player and stores
// the answer in VAR_STARTER_GROUP; saves that never answered the quiz fall
// back to a random roll the first time the starter-selection scene is set up.
// Add, remove, or reorder groups freely (existing saves keep the group index
// they stored, so reordering changes their contents).
//
// ORDER MATTERS: the quiz menu labels appended to data/text/191.txt (entries
// 477 onward) and the quiz logic in GetStarterGroupFromAnswers both assume
// this order.
//
// Use MON_WITH_FORM(SPECIES_NAME, form) to specify a starter
// with a form.
//
// Keep in mind that these choices will impact Rival team
// determination in the appropriate trigger-scripts.
//
// This will NOT update the text during the starter-selection
// sequence. To update that text, modify text archive 190 in
// DSPRE, or override it via data/text/190.txt like this repo
// does now.
static const u16 sStarterGroups[][3] = {
    { SPECIES_KYOGRE, SPECIES_GROUDON, SPECIES_RAYQUAZA },    // 0: Hoenn (gen 3)
    { SPECIES_DIALGA, SPECIES_PALKIA, SPECIES_GIRATINA },     // 1: Sinnoh (gen 4)
    { SPECIES_RESHIRAM, SPECIES_ZEKROM, SPECIES_KYUREM },     // 2: Unova (gen 5)
    { SPECIES_XERNEAS, SPECIES_YVELTAL, SPECIES_ZYGARDE },    // 3: Kalos (gen 6)
    { SPECIES_SOLGALEO, SPECIES_LUNALA, SPECIES_NECROZMA },   // 4: Alola (gen 7)
    { SPECIES_ZACIAN, SPECIES_ZAMAZENTA, SPECIES_ETERNATUS }, // 5: Galar (gen 8)
};

#define STARTER_GROUP_COUNT ((int)(sizeof(sStarterGroups) / sizeof(sStarterGroups[0])))

// index of the first quiz menu label appended to data/text/191.txt (the
// vanilla archive has exactly this many entries, 0 to STARTER_QUIZ_LABEL_BASE - 1)
#define STARTER_QUIZ_LABEL_BASE 477

// script var that remembers which group the player will get, as index + 1
// (0 = not chosen yet).  Mirrors VAR_STARTER_GROUP in asm/include/vars.inc;
// 0x4031 is unused by both the vanilla scripts and hg-engine itself.
#define STARTER_GROUP_VAR 0x4031

/**
 *  @brief rolls three distinct starter groups for the era quiz, writing the
 *         results into six consecutive special vars (called via the
 *         starter_quiz_roll script command)
 *
 *  @param base_var first special var, receives:
 *                  base_var + 0..2 = menu label message ids (data/text/191.txt)
 *                  base_var + 3..5 = group indices (menu option return values)
 */
void StarterQuiz_Roll(u16 base_var)
{
    int rolled[3];

    rolled[0] = gf_rand() % STARTER_GROUP_COUNT;

    do {
        rolled[1] = gf_rand() % STARTER_GROUP_COUNT;
    } while (rolled[1] == rolled[0] && STARTER_GROUP_COUNT > 1);

    do {
        rolled[2] = gf_rand() % STARTER_GROUP_COUNT;
    } while ((rolled[2] == rolled[0] || rolled[2] == rolled[1]) && STARTER_GROUP_COUNT > 2);

    for (int i = 0; i < 3; i++) {
        SetScriptVar(base_var + i, STARTER_QUIZ_LABEL_BASE + rolled[i]);
        SetScriptVar(base_var + 3 + i, rolled[i]);
    }
}

/**
 *  @brief maps the player's quiz answers to the starter group they should get.
 *         This is the single place to extend when the quiz logic changes
 *         (e.g. multiple rounds of questions).
 *
 *  @return group index, or -1 if the quiz hasn't been answered yet
 */
static int GetStarterGroupFromAnswers(void)
{
    u16 stored = GetScriptVar(STARTER_GROUP_VAR); // quiz stores its answer as index + 1

    if (stored == 0 || stored > STARTER_GROUP_COUNT) {
        return -1;
    }

    return stored - 1;
}

/**
 *  @brief gets the starter group this save uses; falls back to a random roll
 *         for saves that never answered the quiz (old saves, skipped quiz, ...)
 *
 *  @return group index, 0 to STARTER_GROUP_COUNT - 1
 */
static int GetStarterGroup(void)
{
    int group = GetStarterGroupFromAnswers();

    if (group < 0) {
        group = gf_rand() % STARTER_GROUP_COUNT;
        SetScriptVar(STARTER_GROUP_VAR, group + 1);
    }

    return group;
}

/**
 *  @brief returns the species entry (including form bits) for one slot of this save's group
 *
 *  @param slot starter choice (0-2)
 */
static u16 GetStarterSlot(int slot)
{
    return sStarterGroups[GetStarterGroup()][slot];
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
