#include "../include/config.h"
#include "../include/constants/file.h"
#include "../include/repel.h"
#include "../include/pokemon.h"
#include "../include/roamer.h"
#include "../include/save.h"
#include "../include/script.h"
#include "../include/types.h"

#define SCRIPT_NEW_CMD_REPEL_USE 0
#define SCRIPT_NEW_CMD_STARTER_QUIZ_ROLL_CONCEPTS 1
#define SCRIPT_NEW_CMD_STARTER_QUIZ_DRAW 2
#define SCRIPT_NEW_CMD_MON_SERVICE_SELECT 3
#define SCRIPT_NEW_CMD_MON_SERVICE_SET_IV 4
#define SCRIPT_NEW_CMD_MON_SERVICE_SET_EV 5
#define SCRIPT_NEW_CMD_MON_SERVICE_SET_NATURE 6

#define SCRIPT_NEW_CMD_MAX 256

// defined in src/starters.c
void StarterQuiz_RollConcepts(u16 base_var);
void StarterQuiz_DrawSlot(u16 mon_var);

// ---- Pokemon stats adjustment service (standard script 2074) --------------
// the script stores the chosen party slot via MonServiceSelect, then applies
// the changes described by the payload vars
static u16 sMonServiceSlot = 0xFFFF;

static struct PartyPokemon *MonService_GetTarget(SCRIPTCONTEXT *ctx)
{
    struct Party *party;

    if (sMonServiceSlot > 5)
        return NULL;

    party = SaveData_GetPlayerPartyPtr(ctx->fsys->savedata);
    if (party == NULL || (s32)sMonServiceSlot >= party->count)
        return NULL;

    return Party_GetMonByIndex(party, sMonServiceSlot);
}

static void MonService_Recalc(struct PartyPokemon *mon)
{
    RecalcPartyPokemonStats(mon);
    if (mon->party.hp > mon->party.maxHp)
        mon->party.hp = mon->party.maxHp;
}

// stat: 0-5 = single stat, 6 = all six (mon data enum order)
static void MonService_SetIVs(SCRIPTCONTEXT *ctx, u16 stat, u16 value)
{
    struct PartyPokemon *mon = MonService_GetTarget(ctx);
    u8 iv;
    int i;

    if (mon == NULL || stat > 6)
        return;

    if (value > 31)
        value = 31;
    iv = value;

    if (stat == 6) {
        for (i = 0; i < 6; i++)
            SetMonData(mon, MON_DATA_HP_IV + i, &iv);
    } else {
        SetMonData(mon, MON_DATA_HP_IV + stat, &iv);
    }

    MonService_Recalc(mon);
}

static void MonService_SetEVs(SCRIPTCONTEXT *ctx, u16 stat, u16 value)
{
    struct PartyPokemon *mon = MonService_GetTarget(ctx);
    u8 ev;
    int i;

    if (mon == NULL || stat > 6)
        return;

    if (value > 252)
        value = 252;
    ev = value;

    if (stat == 6) {
        for (i = 0; i < 6; i++)
            SetMonData(mon, MON_DATA_HP_EV + i, &ev);
    } else {
        SetMonData(mon, MON_DATA_HP_EV + stat, &ev);
    }

    MonService_Recalc(mon);
}

// handles the same way as nature mints (ItemMenuUseFunc_Mint)
static void MonService_SetNature(SCRIPTCONTEXT *ctx, u16 nature)
{
    struct PartyPokemon *mon = MonService_GetTarget(ctx);

    if (mon == NULL || nature >= 25)
        return;

    SET_MON_NATURE_OVERRIDE(mon, nature)
    MonService_Recalc(mon);
}

BOOL Script_RunNewCmd(SCRIPTCONTEXT *ctx)
{
    u8 sw = ScriptReadByte(ctx);
    u16 arg0 = ScriptReadHalfword(ctx);

    switch (sw) {
    case SCRIPT_NEW_CMD_REPEL_USE:;
#ifdef IMPLEMENT_REUSABLE_REPELS
        u16 most_recent_repel = Repel_GetMostRecent();
        SetScriptVar(arg0, most_recent_repel);
        Repel_Use(most_recent_repel, HEAPID_MAIN_HEAP);
#endif
        break;

    case SCRIPT_NEW_CMD_STARTER_QUIZ_ROLL_CONCEPTS:
        StarterQuiz_RollConcepts(arg0);
        break;

    case SCRIPT_NEW_CMD_STARTER_QUIZ_DRAW:
        StarterQuiz_DrawSlot(arg0);
        break;

    // pokemon stats adjustment service (standard script 2074): arg0 is the
    // script var holding the payload; SET_IV/SET_EV read a second var
    case SCRIPT_NEW_CMD_MON_SERVICE_SELECT:
        sMonServiceSlot = GetScriptVar(arg0);
        break;

    case SCRIPT_NEW_CMD_MON_SERVICE_SET_IV:;
        u16 ivValueVar = ScriptReadHalfword(ctx);
        MonService_SetIVs(ctx, GetScriptVar(arg0), GetScriptVar(ivValueVar));
        break;

    case SCRIPT_NEW_CMD_MON_SERVICE_SET_EV:;
        u16 evValueVar = ScriptReadHalfword(ctx);
        MonService_SetEVs(ctx, GetScriptVar(arg0), GetScriptVar(evValueVar));
        break;

    case SCRIPT_NEW_CMD_MON_SERVICE_SET_NATURE:
        MonService_SetNature(ctx, GetScriptVar(arg0));
        break;

    default:
        break;
    }

    return FALSE;
}

#ifdef EXPAND_ROAMERS
BOOL LONG_CALL ScrCmd_CreateRoamer(SCRIPTCONTEXT *ctx)
{
    u8 roamerNo = ScriptReadByte(ctx);
    Save_CreateRoamerByID(ctx->fsys->savedata, roamerNo);
    return FALSE;
}
#endif // EXPAND_ROAMERS
