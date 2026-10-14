.text
.align 2
.thumb

.global set_starter_hidden_ability_hook
set_starter_hidden_ability_hook:
mov r5, r2
add r4, #8
mul r5, r1
add r1, r4, r5
push {r0-r7}
bl set_starter_hidden_ability
pop {r0-r7}
ldr r2, =0x02096160 | 1
bx r2

.pool


.global ImplementLevelCap_hook
ImplementLevelCap_hook:
ldr r3, =0x206E540 | 1
bl bx_r3
// r0 is level
bl IsLevelAtLevelCap
cmp r0, #1
beq return_to_2245B3E // above level cap
ldr r3, =0x02245A32 | 1
bx r3

return_to_2245B3E:
ldr r3, =0x2245B3E|1
// fall through

bx_r3:
bx r3

.pool


// r0 is level
.global CapRareCandies
CapRareCandies:
bl IsLevelAtLevelCap
cmp r0, #0
bne return_to_208FEFA
mov r0, r4
ldr r3, =0x0201AB0C | 1 // free memory
bl bx_r3
ldr r3, =0x0208FEF4 | 1
bx r3

return_to_208FEFA:
ldr r3, =0x0208FEFA | 1
bx r3


// still needed because function itself is not 0x1C bytes long
.global Bag_HasSpaceForItem_hook
Bag_HasSpaceForItem_hook:
ldr r5, =Bag_HasSpaceForItem_return_address
mov r6, lr
str r6, [r5]
pop {r5-r6}
bl Bag_HasSpaceForItem
ldr r1, =Bag_HasSpaceForItem_return_address
ldr r1, [r1]
mov pc, r1

.pool

Bag_HasSpaceForItem_return_address:
.word 0


// still needed because function itself is not 0x1C bytes long
.global Bag_HasItem_hook
Bag_HasItem_hook:
ldr r5, =Bag_HasItem_return_address
mov r6, lr
str r6, [r5]
pop {r5-r6}
bl Bag_HasItem
ldr r1, =Bag_HasItem_return_address
ldr r1, [r1]
mov pc, r1

.pool

Bag_HasItem_return_address:
.word 0


// still needed because function itself is not 0x1C bytes long
.global CanUseItemOnMonInParty_hook
CanUseItemOnMonInParty_hook:
ldr r5, =CanUseItemOnMonInParty_return_address
mov r6, lr
str r6, [r5]
pop {r5-r6}
bl CanUseItemOnMonInParty
ldr r1, =CanUseItemOnMonInParty_return_address
ldr r1, [r1]
mov pc, r1

.pool

CanUseItemOnMonInParty_return_address:
.word 0

.global PartyMenu_ItemUseFunc_LevelUpLearnMovesLoop_Case6_hook
PartyMenu_ItemUseFunc_LevelUpLearnMovesLoop_Case6_hook:
mov r0, r4
bl PartyMenu_ItemUseFunc_LevelUpLearnMovesLoop_Case6
ldr r1, =0x02081ea6 | 1
bx r1

.global PartyMenu_ItemUseFunc_ReuseItem_hook
PartyMenu_ItemUseFunc_ReuseItem_hook:
mov r0, r4
bl PartyMenu_ItemUseFunc_ReuseItem
ldr r3, =0x02081398 | 1
bx r3

.global Bag_RenderMachineMoveSlot_hook
Bag_RenderMachineMoveSlot_hook:
ldr r3, [sp, #0x28]
sub sp, #8
str r3, [sp, #0]
mov r0, r5
mov r1, r6
mov r3, r4
bl  Bag_RenderMachineMoveSlot
ldr r3, [r4, #0]
cmp r0, #0
add sp, #8
ldr r2, =0x021FF662 | 1
bx  r2

.pool

// Ability machines have no move, so the rom's TM pocket info strip (ov15_021FE620) would
// show TM01's move stats for them. Draw the granted ability's effect text instead.
.global Bag_MachineMoveInfo_hook
Bag_MachineMoveInfo_hook:
ldr r3, =0xA7D // ABILITY_MACHINE_ITEM_FIRST (2685)
sub r3, r1, r3
cmp r3, #0x1C // number of ability machines
bhs Bag_MachineMoveInfo_hook_normal
push {r3, lr}
bl Bag_DrawAbilityMachineCard
pop {r3, pc}

Bag_MachineMoveInfo_hook_normal:
// replay the prologue we overwrote, then carry on in the original function
push {r4, r5, r6, r7, lr}
sub sp, #0x14
add r5, r0, #0
add r4, r5, #0
ldr r3, =0x021FE628 | 1
bx r3

// Same for the type/category icons (ov15_021FF97C): ability machines have no move type,
// so force the suppress flag instead of drawing TM01's icons.
.global Bag_MachineMoveInfoIcons_hook
Bag_MachineMoveInfoIcons_hook:
ldr r3, =0xA7D // ABILITY_MACHINE_ITEM_FIRST (2685)
sub r3, r1, r3
cmp r3, #0x1C
bhs Bag_MachineMoveInfoIcons_hook_normal
mov r2, #0

Bag_MachineMoveInfoIcons_hook_normal:
// replay the prologue we overwrote, then carry on in the original function
push {r3, r4, r5, r6, r7, lr}
sub sp, #8
add r5, r0, #0
mov r0, #0x9b
ldr r3, =0x021FF984 | 1
bx r3

.pool
