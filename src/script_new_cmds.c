#include "../include/config.h"
#include "../include/constants/file.h"
#include "../include/repel.h"
#include "../include/roamer.h"
#include "../include/script.h"
#include "../include/types.h"

#define SCRIPT_NEW_CMD_REPEL_USE 0
#define SCRIPT_NEW_CMD_STARTER_QUIZ_ROLL_CONCEPTS 1
#define SCRIPT_NEW_CMD_STARTER_QUIZ_DRAW 2

#define SCRIPT_NEW_CMD_MAX 256

// defined in src/starters.c
void StarterQuiz_RollConcepts(u16 base_var);
void StarterQuiz_DrawSlot(u16 mon_var);

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
