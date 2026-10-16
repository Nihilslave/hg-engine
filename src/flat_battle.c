#include "../include/flat_battle.h"

#include "../include/constants/battle_constants.h"
#include "../include/constants/trainerclass.h"

#include "../include/battle.h"
#include "../include/pokemon.h"
#include "../include/save.h"
#include "../include/types.h"

// ---------------------------------------------------------------------------
// Gym leader "flat battle" system; see include/flat_battle.h for the design.
//
// The state below lives in the main payload (always resident), so both the
// field unit (FlatBattle_Begin when a leader battle is set up) and the battle
// unit (FlatBattle_Enforce / FlatBattle_Finish) can reach it.
// ---------------------------------------------------------------------------

#define FLAT_PARTY_MAX 6

// Per party slot: the stat line of that mon at the flat level, used to
// patch the active battlemons (see FlatBattle_Enforce).
struct FlatBattleSlot {
    u32 exp;
    u16 atk;
    u16 def;
    u16 speed;
    u16 spatk;
    u16 spdef;
    u16 maxHp;
    u8 level;
    u8 valid;
};

static u32 sFlatBattleActive = 0;
static struct FlatBattleSlot sFlatBattleSlots[FLAT_PARTY_MAX];

u32 LONG_CALL FlatBattle_IsActive(void)
{
    return sFlatBattleActive;
}

BOOL LONG_CALL FlatBattle_IsGymLeaderClass(u32 trainerClass)
{
    switch (trainerClass) {
    // Johto
    case TRAINERCLASS_LEADER_FALKNER:
    case TRAINERCLASS_LEADER_BUGSY:
    case TRAINERCLASS_LEADER_WHITNEY:
    case TRAINERCLASS_LEADER_MORTY:
    case TRAINERCLASS_LEADER_PRYCE:
    case TRAINERCLASS_LEADER_JASMINE:
    case TRAINERCLASS_LEADER_CHUCK:
    case TRAINERCLASS_LEADER_CLAIR:
    // Kanto
    case TRAINERCLASS_LEADER_BROCK:
    case TRAINERCLASS_LEADER_MISTY:
    case TRAINERCLASS_LEADER_LT_SURGE:
    case TRAINERCLASS_LEADER_ERIKA:
    case TRAINERCLASS_LEADER_JANINE:
    case TRAINERCLASS_LEADER_SABRINA:
    case TRAINERCLASS_LEADER_BLAINE:
    case TRAINERCLASS_LEADER_BLUE:
        return TRUE;
    default:
        return FALSE;
    }
}

// Fully heals a single mon like a Pokemon Center does (hp, status, pp).
static void FlatBattle_HealMon(struct PartyPokemon *mon)
{
    u32 i;
    u32 value;

    // hp
    value = GetMonData(mon, MON_DATA_MAXHP, NULL);
    SetMonData(mon, MON_DATA_HP, &value);

    // status
    value = 0;
    SetMonData(mon, MON_DATA_STATUS, &value);

    // pp
    for (i = 0; i < 4; i++) {
        u32 maxPP;

        if ((u16)GetMonData(mon, MON_DATA_MOVE1 + i, NULL) == 0)
            continue;
        maxPP = GetMonData(mon, MON_DATA_MOVE1MAXPP + i, NULL);
        SetMonData(mon, MON_DATA_MOVE1PP + i, &maxPP);
    }
}

// Patches one active battlemon to the flat stat line.  hp is tracked in
// flat-level terms and is only clamped to the flat max hp.
static void FlatBattle_ApplyMon(struct BattlePokemon *mon, u32 slot)
{
    const struct FlatBattleSlot *info = &sFlatBattleSlots[slot];

    if (!info->valid || mon->species == 0)
        return;
    if (mon->level == info->level && (u16)mon->maxhp == info->maxHp)
        return; // already on the flat stat line

    mon->level = info->level;
    mon->exp = info->exp;
    mon->attack = info->atk;
    mon->defense = info->def;
    mon->speed = info->speed;
    mon->spatk = info->spatk;
    mon->spdef = info->spdef;
    mon->maxhp = info->maxHp;
    if (mon->hp > (s32)info->maxHp)
        mon->hp = (s32)info->maxHp;
}

void LONG_CALL FlatBattle_Enforce(struct BattleSystem *bsys, struct BattleStruct *ctx)
{
    int battler;

    if (!sFlatBattleActive || bsys == NULL || ctx == NULL)
        return;

    for (battler = 0; battler < bsys->maxBattlers; battler++) {
        u32 slot;

        // Player side only (battlers 0 and 2 in double battles).
        if (battler & 1)
            continue;

        slot = ctx->sel_mons_no[battler];
        if (slot >= FLAT_PARTY_MAX)
            continue;

        FlatBattle_ApplyMon(&ctx->battlemon[battler], slot);
    }
}

void LONG_CALL FlatBattle_Finish(struct BattleSystem *bsys)
{
    struct Party *battleParty;
    struct Party *saveParty;

    if (!sFlatBattleActive)
        return;

    sFlatBattleActive = 0;

    // Like a link battle, the battle must leave no lasting effect on the
    // party.  Instead of restoring every value the battle touched, simply
    // reset the whole battle party copy to the save party: the results
    // write-back that follows every normal battle then copies the untouched
    // save data back onto itself, i.e. it becomes a no-op for the party
    // (damage, pp, status and item consumption are all discarded with the
    // copy).  Defeats still black the player out, because the gym scripts
    // call WhiteOut themselves on a loss.
    battleParty = BattleWorkPokePartyGet(bsys, BATTLER_PLAYER);
    saveParty = (struct Party *)SaveData_GetPlayerPartyPtr(SaveBlock2_get());

    if (battleParty != NULL && saveParty != NULL)
        *battleParty = *saveParty;
}

void LONG_CALL FlatBattle_Begin(u32 level, struct Party *party)
{
    u32 i;
    struct Party *saveParty;

    if (level == 0 || level > 100)
        return;

    sFlatBattleActive = 0;
    for (i = 0; i < FLAT_PARTY_MAX; i++)
        sFlatBattleSlots[i].valid = 0;

    // The gym battle starts with a completely healed team: heal the real
    // save party (visible in the field menu immediately) and the battle
    // party copy that the battle actually uses.
    saveParty = (struct Party *)SaveData_GetPlayerPartyPtr(SaveBlock2_get());
    if (saveParty != NULL) {
        for (i = 0; i < FLAT_PARTY_MAX; i++) {
            struct PartyPokemon *mon = Party_GetMonByIndex(saveParty, (s32)i);

            if (mon == NULL)
                continue;
            if (GetMonData(mon, MON_DATA_IS_EGG, NULL))
                continue;
            if ((u16)GetMonData(mon, MON_DATA_SPECIES, NULL) == 0)
                continue;
            FlatBattle_HealMon(mon);
        }
    }

    if (party != NULL) {
        for (i = 0; i < FLAT_PARTY_MAX; i++) {
            struct PartyPokemon *mon = Party_GetMonByIndex(party, (s32)i);
            u16 species;
            u32 savedExp;
            u32 flatExp;
            u8 savedLevel;
            u8 newLevel;

            if (mon == NULL)
                continue;
            if (GetMonData(mon, MON_DATA_IS_EGG, NULL))
                continue;
            species = (u16)GetMonData(mon, MON_DATA_SPECIES, NULL);
            if (species == 0)
                continue;

            FlatBattle_HealMon(mon);

            // Compute the mon's stat line at the flat level by temporarily
            // raising it, then put the original level/experience/stats back
            // right away so nothing flat can ever leak into the save.
            savedExp = GetMonData(mon, MON_DATA_EXPERIENCE, NULL);
            savedLevel = (u8)GetMonData(mon, MON_DATA_LEVEL, NULL);
            flatExp = GetExpByGrowthRateAndLevel((int)PokePersonalParaGet(species, PERSONAL_EXP_GROUP), (int)level);
            newLevel = (u8)level;

            SetMonData(mon, MON_DATA_EXPERIENCE, &flatExp);
            SetMonData(mon, MON_DATA_LEVEL, &newLevel);
            RecalcPartyPokemonStats(mon);

            sFlatBattleSlots[i].exp = flatExp;
            sFlatBattleSlots[i].atk = (u16)GetMonData(mon, MON_DATA_ATTACK, NULL);
            sFlatBattleSlots[i].def = (u16)GetMonData(mon, MON_DATA_DEFENSE, NULL);
            sFlatBattleSlots[i].speed = (u16)GetMonData(mon, MON_DATA_SPEED, NULL);
            sFlatBattleSlots[i].spatk = (u16)GetMonData(mon, MON_DATA_SPECIAL_ATTACK, NULL);
            sFlatBattleSlots[i].spdef = (u16)GetMonData(mon, MON_DATA_SPECIAL_DEFENSE, NULL);
            sFlatBattleSlots[i].maxHp = (u16)GetMonData(mon, MON_DATA_MAXHP, NULL);
            sFlatBattleSlots[i].level = newLevel;
            sFlatBattleSlots[i].valid = 1;

            SetMonData(mon, MON_DATA_EXPERIENCE, &savedExp);
            SetMonData(mon, MON_DATA_LEVEL, &savedLevel);
            RecalcPartyPokemonStats(mon);
        }
    }

    sFlatBattleActive = 1;
}

void LONG_CALL FlatBattle_ApplyBattleEntryValues(struct Party *party)
{
    u32 i;

    if (!sFlatBattleActive || party == NULL)
        return;

    // Called once after the battle setup has finished touching the party
    // copy (battle form changes recalculate stats and clamp hp): the copy
    // is what the battle and the in-battle party lists read, so make its
    // displayed values (level, stats, fully healed hp/max hp pair) match
    // the flat stat line.  Experience is not touched, and the whole copy is
    // discarded when the battle ends (FlatBattle_Finish), so none of this
    // can leak into the save.
    for (i = 0; i < FLAT_PARTY_MAX; i++) {
        const struct FlatBattleSlot *info = &sFlatBattleSlots[i];
        struct PartyPokemon *mon;
        u32 value;

        if (!info->valid)
            continue;
        mon = Party_GetMonByIndex(party, (s32)i);
        if (mon == NULL)
            continue;

        value = info->level;
        SetMonData(mon, MON_DATA_LEVEL, &value);
        value = info->atk;
        SetMonData(mon, MON_DATA_ATTACK, &value);
        value = info->def;
        SetMonData(mon, MON_DATA_DEFENSE, &value);
        value = info->speed;
        SetMonData(mon, MON_DATA_SPEED, &value);
        value = info->spatk;
        SetMonData(mon, MON_DATA_SPECIAL_ATTACK, &value);
        value = info->spdef;
        SetMonData(mon, MON_DATA_SPECIAL_DEFENSE, &value);
        value = info->maxHp;
        SetMonData(mon, MON_DATA_HP, &value);
        SetMonData(mon, MON_DATA_MAXHP, &value);
    }
}
