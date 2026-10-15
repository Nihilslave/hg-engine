#ifndef HG_ENGINE_SILVALLY_FORMS_H
#define HG_ENGINE_SILVALLY_FORMS_H

#include "types.h"

#include "constants/item.h"
#include "constants/pokemon.h"

// Form number (== type id) for each of Silvally's memories, in item order.
// The memories run fighting..steel, fire..dark, fairy, while the form order
// (and the type ids) put fairy ninth (between steel and fire), so this cannot
// be a straight subtraction.
// Shared by the battle form change (src/individual/BattleFormChangeCheck.c)
// and the box mon form update (src/pokemon.c), the same way the plate lookup
// GetArceusType is used for arceus.
static const u8 sSilvallyMemoryFormNos[] = {
    TYPE_FIGHTING,
    TYPE_FLYING,
    TYPE_POISON,
    TYPE_GROUND,
    TYPE_ROCK,
    TYPE_BUG,
    TYPE_GHOST,
    TYPE_STEEL,
    TYPE_FIRE,
    TYPE_WATER,
    TYPE_GRASS,
    TYPE_ELECTRIC,
    TYPE_PSYCHIC,
    TYPE_ICE,
    TYPE_DRAGON,
    TYPE_DARK,
    TYPE_FAIRY,
};

// form number for a held memory; 0 (normal form) for anything else
#define GET_SILVALLY_MEMORY_FORM_NO(item)                              \
    (((item) >= ITEM_FIGHTING_MEMORY && (item) <= ITEM_FAIRY_MEMORY)   \
        ? sSilvallyMemoryFormNos[(item) - ITEM_FIGHTING_MEMORY]        \
        : 0)

#endif
