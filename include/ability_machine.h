#ifndef HG_ENGINE_ABILITY_MACHINE_H
#define HG_ENGINE_ABILITY_MACHINE_H

#include "types.h"

#include "constants/ability.h"
#include "constants/item.h"

// Abilities granted by the ability machine items: the item with id
// (ABILITY_MACHINE_ITEM_FIRST + i) grants sAbilityMachineAbilities[i].
// Shared by the party menu (item use effect) and the bag (TM pocket info strip).
// The order must stay in sync with the ability machine item ids in constants/item.h.
static const u16 sAbilityMachineAbilities[] = {
    ABILITY_SPEED_BOOST,
    ABILITY_DROUGHT,
    ABILITY_DRIZZLE,
    ABILITY_SAND_STREAM,
    ABILITY_SNOW_WARNING,
    ABILITY_INTIMIDATE,
    ABILITY_LEVITATE,
    ABILITY_TECHNICIAN,
    ABILITY_ADAPTABILITY,
    ABILITY_MULTISCALE,
    ABILITY_REGENERATOR,
    ABILITY_MAGIC_GUARD,
    ABILITY_MAGIC_BOUNCE,
    ABILITY_MOLD_BREAKER,
    ABILITY_GALE_WINGS,
    ABILITY_PRANKSTER,
    ABILITY_UNAWARE,
    ABILITY_STURDY,
    ABILITY_HUGE_POWER,
    ABILITY_PURE_POWER,
    ABILITY_PROTEAN,
    ABILITY_SERENE_GRACE,
    ABILITY_RECKLESS,
    ABILITY_IRON_FIST,
    ABILITY_THICK_FAT,
    ABILITY_WATER_ABSORB,
    ABILITY_VOLT_ABSORB,
    ABILITY_FLASH_FIRE,
};

#endif // HG_ENGINE_ABILITY_MACHINE_H
