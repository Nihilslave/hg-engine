#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../../include/trainer_data.h"

enum {
    RAWTEXT_PATH_LENGTH = 128,
    TRAINER_DATA_MEMBER_PATH_LENGTH = 32,
    TRAINER_TEXT_OFFSET_MEMBER_PATH_LENGTH = 48,
    TRAINER_TEXT_ENTRY_SIZE = 4,
};

static uint16_t EncodeNicknameChar(char c) {
    if (c >= '0' && c <= '9') {
        return 0x0121 + (uint16_t)(c - '0');
    }

    if (c >= 'A' && c <= 'Z') {
        return 0x012B + (uint16_t)(c - 'A');
    }

    if (c >= 'a' && c <= 'z') {
        return 0x0145 + (uint16_t)(c - 'a');
    }

    return 0;
}

static void BuildRawTextPath(char *path, size_t pathSize, const char *dir, int index) {
    size_t dirLen;

    if (index < 0 || index > 9999) {
        fprintf(stderr, "Rawtext file index out of range: %d\n", index);
        exit(EXIT_FAILURE);
    }

    dirLen = strlen(dir);
    if (dirLen + sizeof("/0000.txt") > pathSize) {
        fprintf(stderr, "Rawtext output path too long: %s\n", dir);
        exit(EXIT_FAILURE);
    }

    memcpy(path, dir, dirLen);
    path[dirLen] = '/';
    path[dirLen + 1] = (char)('0' + ((index / 1000) % 10));
    path[dirLen + 2] = (char)('0' + ((index / 100) % 10));
    path[dirLen + 3] = (char)('0' + ((index / 10) % 10));
    path[dirLen + 4] = (char)('0' + (index % 10));
    memcpy(path + dirLen + 5, ".txt", sizeof(".txt"));
}

static int IsPartyMonDefined(const TrainerPokemonData *mon) {
    return mon->species != SPECIES_NONE;
}

static int GetTrainerPartyMonCount(const TrainerData *entry) {
    int count = 0;
    int i;
    int sawGap = 0;

    for (i = 0; i < TRAINER_SOURCE_MAX_PARTY_SIZE; i++) {
        if (IsPartyMonDefined(&entry->party[i])) {
            if (sawGap) {
                fprintf(stderr, "Trainer \"%s\" has a gap in its party definition\n", entry->name);
                exit(EXIT_FAILURE);
            }
            count++;
        } else if (count > 0) {
            sawGap = 1;
        }
    }

    return count;
}

static int IsTextEntryDefined(const TrainerMessageEntry *entry) {
    return entry->text != NULL;
}

static int GetTrainerTextEntryCount(const TrainerData *entry) {
    int count = 0;
    int i;
    int sawGap = 0;

    for (i = 0; i < TRAINER_SOURCE_MAX_TEXT_ENTRY_COUNT; i++) {
        if (IsTextEntryDefined(&entry->text[i])) {
            if (sawGap) {
                fprintf(stderr, "Trainer \"%s\" has a gap in its text definition\n", entry->name);
                exit(EXIT_FAILURE);
            }
            count++;
        } else if (count > 0) {
            sawGap = 1;
        }
    }

    return count;
}

static int GetTrainerTextOffsetCount(void) {
    int i;

    for (i = (int)sTrainerDataCount - 1; i >= 0; i--) {
        if (GetTrainerTextEntryCount(&sTrainerData[i]) > 0) {
            return i + 1;
        }
    }

    return 0;
}

static void WriteLe16(uint8_t *dst, uint16_t value) {
    dst[0] = (uint8_t)(value & 0xFF);
    dst[1] = (uint8_t)(value >> 8);
}

static void WriteLe32(uint8_t *dst, uint32_t value) {
    dst[0] = (uint8_t)(value & 0xFF);
    dst[1] = (uint8_t)((value >> 8) & 0xFF);
    dst[2] = (uint8_t)((value >> 16) & 0xFF);
    dst[3] = (uint8_t)((value >> 24) & 0xFF);
}

static FILE *OpenBinaryForWrite(const char *path) {
    FILE *file = fopen(path, "wb");
    if (file == NULL) {
        perror(path);
        exit(EXIT_FAILURE);
    }
    return file;
}

static FILE *OpenTextForWrite(const char *path) {
    FILE *file = fopen(path, "w");
    if (file == NULL) {
        perror(path);
        exit(EXIT_FAILURE);
    }
    return file;
}

static void WriteTrainerNameTextFile(const char *dir, int index, const char *name) {
    char path[RAWTEXT_PATH_LENGTH];
    FILE *file;

    BuildRawTextPath(path, sizeof(path), dir, index);
    file = OpenTextForWrite(path);
    fputs("{TRNAME}", file);
    fputs(name, file);
    fclose(file);
}

static void WriteTextFile(const char *dir, int index, const char *text) {
    char path[RAWTEXT_PATH_LENGTH];
    FILE *file;

    BuildRawTextPath(path, sizeof(path), dir, index);
    file = OpenTextForWrite(path);
    fputs(text, file);
    fclose(file);
}

/*
 * Presence detection for .setIvs / .setEvs.
 *
 * C turns every stat that is not written in a designated initializer into a plain 0,
 * which is indistinguishable from an explicit "= 0". To know which stats the author
 * actually wrote (everything else keeps the maxed-out default at runtime), scan
 * data/Trainers.c as text and record one presence bit per stat. Missing stats are
 * written as TRAINER_IV_EV_UNSET; the game substitutes the default for them.
 */
enum {
    PRESENCE_HP         = 1u << 0,
    PRESENCE_ATTACK     = 1u << 1,
    PRESENCE_DEFENSE    = 1u << 2,
    PRESENCE_SPEED      = 1u << 3,
    PRESENCE_SP_ATTACK  = 1u << 4,
    PRESENCE_SP_DEFENSE = 1u << 5,
    PRESENCE_ALL        = (1u << 6) - 1,
};

typedef struct TrainerPresenceData {
    uint8_t monCount;
    uint8_t ivMasks[TRAINER_SOURCE_MAX_PARTY_SIZE];
    uint8_t evMasks[TRAINER_SOURCE_MAX_PARTY_SIZE];
} TrainerPresenceData;

static TrainerPresenceData *sPresence;

static const char *const sPresenceFieldNames[6] = {
    "hp", "attack", "defense", "speed", "spAttack", "spDefense",
};

static int IsSourceSpace(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static uint8_t PresenceBitForFieldName(const char *name, size_t length) {
    size_t k;

    for (k = 0; k < 6; k++) {
        if (strlen(sPresenceFieldNames[k]) == length
            && strncmp(sPresenceFieldNames[k], name, length) == 0) {
            return (uint8_t)(1u << k);
        }
    }

    return 0;
}

static char *ReadTrainerSourceText(void) {
    static const char *const candidates[] = {
        "data/Trainers.c",
        "../data/Trainers.c",
        "../../data/Trainers.c",
        "../../../data/Trainers.c",
    };
    FILE *file = NULL;
    long length;
    char *buffer;
    size_t k;

    for (k = 0; k < sizeof(candidates) / sizeof(candidates[0]) && file == NULL; k++) {
        file = fopen(candidates[k], "rb");
    }

    if (file == NULL) {
        fprintf(stderr, "Cannot open data/Trainers.c for IV/EV presence detection\n");
        exit(EXIT_FAILURE);
    }

    fseek(file, 0, SEEK_END);
    length = ftell(file);
    fseek(file, 0, SEEK_SET);
    if (length < 0) {
        fprintf(stderr, "Cannot determine the size of data/Trainers.c\n");
        exit(EXIT_FAILURE);
    }

    buffer = malloc((size_t)length + 1);
    if (buffer == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    if (fread(buffer, 1, (size_t)length, file) != (size_t)length) {
        fprintf(stderr, "Failed to read data/Trainers.c\n");
        exit(EXIT_FAILURE);
    }

    buffer[length] = '\0';
    fclose(file);
    return buffer;
}

static void ParseTrainerSourcePresence(void) {
    char *source = ReadTrainerSourceText();
    const char *p = source;
    int depth = 0;
    int curTrainer = -1;
    int partyActive = 0;
    int monActive = 0;
    int monIndex = 0;
    int collectTarget = 0; // 1 = inside .setIvs, 2 = inside .setEvs

    while (*p != '\0') {
        unsigned char c = (unsigned char)*p;

        if (IsSourceSpace((char)c)) {
            p++;
            continue;
        }

        if (c == '/' && p[1] == '/') {
            while (*p != '\0' && *p != '\n') {
                p++;
            }
            continue;
        }

        if (c == '/' && p[1] == '*') {
            p += 2;
            while (*p != '\0' && !(p[0] == '*' && p[1] == '/')) {
                p++;
            }
            if (*p != '\0') {
                p += 2;
            }
            continue;
        }

        if (c == '#') {
            while (*p != '\0' && *p != '\n') {
                p++;
            }
            continue;
        }

        if (c == '"' || c == '\'') {
            char quote = (char)c;

            p++;
            while (*p != '\0' && *p != quote) {
                if (*p == '\\' && p[1] != '\0') {
                    p++;
                }
                p++;
            }
            if (*p != '\0') {
                p++;
            }
            continue;
        }

        if (c == '{') {
            if (depth == 3 && partyActive && curTrainer >= 0 && !monActive) {
                if (monIndex >= TRAINER_SOURCE_MAX_PARTY_SIZE) {
                    fprintf(stderr, "Trainer %d has more than %d party entries in data/Trainers.c\n",
                            curTrainer, TRAINER_SOURCE_MAX_PARTY_SIZE);
                    exit(EXIT_FAILURE);
                }
                monIndex++;
                monActive = 1;
            }
            depth++;
            p++;
            continue;
        }

        if (c == '}') {
            depth--;
            if (collectTarget != 0 && depth < 5) {
                collectTarget = 0;
            }
            if (monActive && depth < 4) {
                monActive = 0;
            }
            if (partyActive && depth < 3) {
                partyActive = 0;
            }
            if (curTrainer >= 0 && depth < 2) {
                sPresence[curTrainer].monCount = (uint8_t)monIndex;
                curTrainer = -1;
                monIndex = 0;
            }
            p++;
            continue;
        }

        if (c == '[' && depth == 1) {
            const char *q = p + 1;
            long value = 0;
            int digits = 0;

            while (*q >= '0' && *q <= '9') {
                value = (value * 10) + (*q - '0');
                digits++;
                q++;
            }

            if (digits > 0) {
                const char *r = q;

                while (IsSourceSpace(*r)) {
                    r++;
                }
                if (*r == ']') {
                    r++;
                    while (IsSourceSpace(*r)) {
                        r++;
                    }
                    if (*r == '=') {
                        r++;
                        while (IsSourceSpace(*r)) {
                            r++;
                        }
                        if (*r == '{') {
                            if ((unsigned long)value >= sTrainerDataCount) {
                                fprintf(stderr, "Trainer index [%ld] in data/Trainers.c is out of range\n", value);
                                exit(EXIT_FAILURE);
                            }
                            curTrainer = (int)value;
                            partyActive = 0;
                            monActive = 0;
                            monIndex = 0;
                            collectTarget = 0;
                            memset(&sPresence[curTrainer], 0, sizeof(sPresence[curTrainer]));
                            p = r + 1;
                            depth++;
                            continue;
                        }
                    }
                }
            }

            p++;
            continue;
        }

        if (c == '.') {
            const char *name = p + 1;
            size_t length = 0;

            while ((name[length] >= 'a' && name[length] <= 'z')
                   || (name[length] >= 'A' && name[length] <= 'Z')
                   || (name[length] >= '0' && name[length] <= '9')
                   || name[length] == '_') {
                length++;
            }

            if (length == 0) {
                p++;
                continue;
            }

            if (collectTarget != 0 && depth == 5 && monIndex > 0) {
                uint8_t bit = PresenceBitForFieldName(name, length);

                if (bit != 0) {
                    uint8_t *mask = (collectTarget == 1)
                        ? &sPresence[curTrainer].ivMasks[monIndex - 1]
                        : &sPresence[curTrainer].evMasks[monIndex - 1];

                    *mask |= bit;
                }
            } else if (depth == 4 && monActive && curTrainer >= 0 && monIndex > 0
                       && length == 6
                       && (strncmp(name, "setIvs", 6) == 0 || strncmp(name, "setEvs", 6) == 0)) {
                int target = (name[3] == 'I') ? 1 : 2;
                const char *r = name + length;

                while (IsSourceSpace(*r)) {
                    r++;
                }
                if (*r == '=') {
                    r++;
                    while (IsSourceSpace(*r)) {
                        r++;
                    }
                    if (*r == '{') {
                        collectTarget = target;
                        p = r + 1;
                        depth++;
                        continue;
                    }

                    /* Macro form (e.g. TRAINER_MAX_EVS): every stat is present. */
                    {
                        size_t idLength = 0;

                        while ((r[idLength] >= 'a' && r[idLength] <= 'z')
                               || (r[idLength] >= 'A' && r[idLength] <= 'Z')
                               || (r[idLength] >= '0' && r[idLength] <= '9')
                               || r[idLength] == '_') {
                            idLength++;
                        }

                        if (idLength > 0) {
                            if (target == 1) {
                                sPresence[curTrainer].ivMasks[monIndex - 1] |= PRESENCE_ALL;
                            } else {
                                sPresence[curTrainer].evMasks[monIndex - 1] |= PRESENCE_ALL;
                            }
                            p = r + idLength;
                            continue;
                        }
                    }
                }
            } else if (depth == 2 && curTrainer >= 0 && length == 5 && strncmp(name, "party", 5) == 0) {
                const char *r = name + length;

                while (IsSourceSpace(*r)) {
                    r++;
                }
                if (*r == '=') {
                    r++;
                    while (IsSourceSpace(*r)) {
                        r++;
                    }
                    if (*r == '{') {
                        partyActive = 1;
                        p = r + 1;
                        depth++;
                        continue;
                    }
                }
            }

            p = name + length;
            continue;
        }

        p++;
    }

    if (depth != 0 || curTrainer >= 0) {
        fprintf(stderr, "Failed to parse the structure of data/Trainers.c (unbalanced braces)\n");
        exit(EXIT_FAILURE);
    }

    free(source);
}

static void ValidateTrainerPresence(void) {
    int i;
    int k;
    int j;

    for (i = 0; i < (int)sTrainerDataCount; i++) {
        const TrainerData *entry = &sTrainerData[i];
        const TrainerPresenceData *pres = &sPresence[i];
        const char *name = (entry->name != NULL) ? entry->name : "?";
        int partyCount = GetTrainerPartyMonCount(entry);

        if (pres->monCount < partyCount) {
            fprintf(stderr,
                    "IV/EV presence scan mismatch for trainer %d (\"%s\"): found %u party entries in the source, expected %d\n",
                    i, name, pres->monCount, partyCount);
            exit(EXIT_FAILURE);
        }

        for (k = 0; k < partyCount; k++) {
            const TrainerPokemonData *mon = &entry->party[k];
            const uint8_t ivValues[6] = {
                mon->setIvs.hp, mon->setIvs.attack, mon->setIvs.defense,
                mon->setIvs.speed, mon->setIvs.spAttack, mon->setIvs.spDefense,
            };
            const uint8_t evValues[6] = {
                mon->setEvs.hp, mon->setEvs.attack, mon->setEvs.defense,
                mon->setEvs.speed, mon->setEvs.spAttack, mon->setEvs.spDefense,
            };

            for (j = 0; j < 6; j++) {
                uint8_t bit = (uint8_t)(1u << j);

                if (ivValues[j] != 0 && (pres->ivMasks[k] & bit) == 0) {
                    fprintf(stderr,
                            "IV/EV presence scan mismatch: trainer %d (\"%s\") party slot %d sets .%s = %u IVs, but it was not detected while scanning the source\n",
                            i, name, k, sPresenceFieldNames[j], ivValues[j]);
                    exit(EXIT_FAILURE);
                }
                if (evValues[j] != 0 && (pres->evMasks[k] & bit) == 0) {
                    fprintf(stderr,
                            "IV/EV presence scan mismatch: trainer %d (\"%s\") party slot %d sets .%s = %u EVs, but it was not detected while scanning the source\n",
                            i, name, k, sPresenceFieldNames[j], evValues[j]);
                    exit(EXIT_FAILURE);
                }
            }
        }

        for (k = partyCount; k < pres->monCount; k++) {
            if (pres->ivMasks[k] != 0 || pres->evMasks[k] != 0) {
                fprintf(stderr,
                        "Warning: trainer %d (\"%s\") has .setIvs/.setEvs on an empty party slot (%d); it will be ignored\n",
                        i, name, k);
            }
        }
    }
}

static int TrainerUsesCustomIVEvs(const TrainerData *entry, int index) {
    const TrainerPresenceData *pres = &sPresence[index];
    int k;

    if (entry->data.trainerType & TRAINER_DATA_TYPE_IV_EV_SET) {
        return 1;
    }

    for (k = 0; k < pres->monCount && k < TRAINER_SOURCE_MAX_PARTY_SIZE; k++) {
        if (pres->ivMasks[k] != 0 || pres->evMasks[k] != 0) {
            return 1;
        }
    }

    return 0;
}

static void WriteTrainerHeaderMember(const char *dir, int index, const TrainerData *entry) {
    char path[TRAINER_DATA_MEMBER_PATH_LENGTH];
    uint8_t data[20] = {0};
    FILE *file;
    int partyCount;
    int i;

    snprintf(path, sizeof(path), "%s/5_%03d", dir, index);
    file = OpenBinaryForWrite(path);
    partyCount = GetTrainerPartyMonCount(entry);

    data[0x00] = entry->data.trainerType;
    if (TrainerUsesCustomIVEvs(entry, index)) {
        // also turn the flag on when the entry relies on auto-detected .setIvs/.setEvs
        data[0x00] |= TRAINER_DATA_TYPE_IV_EV_SET;
    }
    WriteLe16(&data[0x01], entry->data.trainerClass);
    data[0x03] = (uint8_t)((entry->data.partySize & TRAINER_DATA_RANDOM_PARTY_ORDER) | partyCount);
    for (i = 0; i < 4; i++) {
        WriteLe16(&data[0x04 + (i * 2)], entry->data.items[i]);
    }
    WriteLe32(&data[0x0C], entry->data.aiFlags);
    WriteLe32(&data[0x10], entry->data.battleType);

    if (fwrite(data, sizeof(data), 1, file) != 1) {
        perror(path);
        fclose(file);
        exit(EXIT_FAILURE);
    }

    fclose(file);
}

static void WriteTrainerPartyMember(const char *dir, int index, const TrainerData *entry) {
    char path[TRAINER_DATA_MEMBER_PATH_LENGTH];
    FILE *file;
    uint8_t data[512] = {0};
    size_t size = 0;
    int partyCount;
    int i;
    int j;

    snprintf(path, sizeof(path), "%s/6_%03d", dir, index);
    file = OpenBinaryForWrite(path);
    partyCount = GetTrainerPartyMonCount(entry);

    if (partyCount == 0) {
        uint8_t placeholder[8] = {0};

        if (fwrite(placeholder, sizeof(placeholder), 1, file) != 1) {
            perror(path);
            fclose(file);
            exit(EXIT_FAILURE);
        }

        fclose(file);
        return;
    }

    for (i = 0; i < partyCount; i++) {
        const TrainerPokemonData *mon = &entry->party[i];

        data[size++] = mon->ivs;
        data[size++] = mon->abilitySlot;
        WriteLe16(&data[size], mon->level);
        size += 2;

        WriteLe16(&data[size], mon->species);
        size += 2;

        if (entry->data.trainerType & TRAINER_DATA_TYPE_ITEMS) {
            WriteLe16(&data[size], mon->item);
            size += 2;
        }

        if (entry->data.trainerType & TRAINER_DATA_TYPE_MOVES) {
            for (j = 0; j < 4; j++) {
                WriteLe16(&data[size], mon->moves[j]);
                size += 2;
            }
        }

        if (entry->data.trainerType & TRAINER_DATA_TYPE_ABILITY) {
            WriteLe16(&data[size], mon->ability);
            size += 2;
        }

        if (entry->data.trainerType & TRAINER_DATA_TYPE_BALL) {
            WriteLe16(&data[size], mon->ball);
            size += 2;
        }

        if (TrainerUsesCustomIVEvs(entry, index)) {
            const uint8_t ivMask = sPresence[index].ivMasks[i];
            const uint8_t evMask = sPresence[index].evMasks[i];

            data[size++] = (ivMask & PRESENCE_HP) ? mon->setIvs.hp : TRAINER_IV_EV_UNSET;
            data[size++] = (ivMask & PRESENCE_ATTACK) ? mon->setIvs.attack : TRAINER_IV_EV_UNSET;
            data[size++] = (ivMask & PRESENCE_DEFENSE) ? mon->setIvs.defense : TRAINER_IV_EV_UNSET;
            data[size++] = (ivMask & PRESENCE_SPEED) ? mon->setIvs.speed : TRAINER_IV_EV_UNSET;
            data[size++] = (ivMask & PRESENCE_SP_ATTACK) ? mon->setIvs.spAttack : TRAINER_IV_EV_UNSET;
            data[size++] = (ivMask & PRESENCE_SP_DEFENSE) ? mon->setIvs.spDefense : TRAINER_IV_EV_UNSET;
            data[size++] = (evMask & PRESENCE_HP) ? mon->setEvs.hp : TRAINER_IV_EV_UNSET;
            data[size++] = (evMask & PRESENCE_ATTACK) ? mon->setEvs.attack : TRAINER_IV_EV_UNSET;
            data[size++] = (evMask & PRESENCE_DEFENSE) ? mon->setEvs.defense : TRAINER_IV_EV_UNSET;
            data[size++] = (evMask & PRESENCE_SPEED) ? mon->setEvs.speed : TRAINER_IV_EV_UNSET;
            data[size++] = (evMask & PRESENCE_SP_ATTACK) ? mon->setEvs.spAttack : TRAINER_IV_EV_UNSET;
            data[size++] = (evMask & PRESENCE_SP_DEFENSE) ? mon->setEvs.spDefense : TRAINER_IV_EV_UNSET;
        }

        if (entry->data.trainerType & TRAINER_DATA_TYPE_NATURE_SET) {
            data[size++] = mon->nature;
        }

        if (entry->data.trainerType & TRAINER_DATA_TYPE_SHINY_LOCK) {
            data[size++] = mon->shinyLock;
        }

        if (entry->data.trainerType & TRAINER_DATA_TYPE_ADDITIONAL_FLAGS) {
            WriteLe32(&data[size], mon->additionalFlags);
            size += 4;

            if (mon->additionalFlags & TRAINER_DATA_EXTRA_TYPE_STATUS) {
                WriteLe32(&data[size], mon->status);
                size += 4;
            }
            if (mon->additionalFlags & TRAINER_DATA_EXTRA_TYPE_HP) {
                WriteLe16(&data[size], mon->hp);
                size += 2;
            }
            if (mon->additionalFlags & TRAINER_DATA_EXTRA_TYPE_ATK) {
                WriteLe16(&data[size], mon->attack);
                size += 2;
            }
            if (mon->additionalFlags & TRAINER_DATA_EXTRA_TYPE_DEF) {
                WriteLe16(&data[size], mon->defense);
                size += 2;
            }
            if (mon->additionalFlags & TRAINER_DATA_EXTRA_TYPE_SPEED) {
                WriteLe16(&data[size], mon->speed);
                size += 2;
            }
            if (mon->additionalFlags & TRAINER_DATA_EXTRA_TYPE_SP_ATK) {
                WriteLe16(&data[size], mon->spAttack);
                size += 2;
            }
            if (mon->additionalFlags & TRAINER_DATA_EXTRA_TYPE_SP_DEF) {
                WriteLe16(&data[size], mon->spDefense);
                size += 2;
            }
            if (mon->additionalFlags & TRAINER_DATA_EXTRA_TYPE_PP_COUNTS) {
                for (j = 0; j < 4; j++) {
                    data[size++] = mon->ppCounts[j];
                }
            }
            if (mon->additionalFlags & TRAINER_DATA_EXTRA_TYPE_NICKNAME) {
                if (mon->nicknameStr != NULL) {
                    int nicknameLen = strlen(mon->nicknameStr);
                    if (nicknameLen > 10) {
                        fprintf(stderr, "Nickname \"%s\" is too long; max is 10 characters\n", mon->nicknameStr);
                        exit(EXIT_FAILURE);
                    }

                    for (j = 0; j < 11; j++) {
                        if (j < nicknameLen) {
                            WriteLe16(&data[size], EncodeNicknameChar(mon->nicknameStr[j]));
                            size += 2;
                        } else if (j == nicknameLen) {
                            WriteLe16(&data[size], 0xFFFF);
                            size += 2;
                        } else {
                            WriteLe16(&data[size], 0);
                            size += 2;
                        }
                    }
                } else {
                    for (j = 0; j < 11; j++) {
                        WriteLe16(&data[size], mon->nickname[j]);
                        size += 2;
                    }
                }
            }
        }

        WriteLe16(&data[size], mon->ballSeal);
        size += 2;
    }

    if (size > 0 && fwrite(data, size, 1, file) != 1) {
        perror(path);
        fclose(file);
        exit(EXIT_FAILURE);
    }

    fclose(file);
}

static void WriteTrainerTextData(const char *mapDir, const char *offsetDir, const char *rawTextDir) {
    char mapPath[TRAINER_TEXT_OFFSET_MEMBER_PATH_LENGTH];
    char offsetPath[TRAINER_TEXT_OFFSET_MEMBER_PATH_LENGTH];
    FILE *mapFile;
    FILE *offsetFile;
    uint16_t *offsets;
    uint16_t offset = 0;
    int textFileIndex = 0;
    int trainerTextOffsetCount;
    int i;
    int j;
    u32 orderIndex;

    offsets = calloc(sTrainerDataCount, sizeof(*offsets));
    if (offsets == NULL) {
        perror("calloc");
        exit(EXIT_FAILURE);
    }
    trainerTextOffsetCount = GetTrainerTextOffsetCount();

    snprintf(mapPath, sizeof(mapPath), "%s/7_0", mapDir);
    snprintf(offsetPath, sizeof(offsetPath), "%s/1_0", offsetDir);
    mapFile = OpenBinaryForWrite(mapPath);
    offsetFile = OpenBinaryForWrite(offsetPath);

    for (orderIndex = 0; orderIndex < sTrainerTextOrderCount; orderIndex++) {
        const TrainerData *entry;
        uint16_t trainerId = sTrainerTextOrder[orderIndex];
        int textCount;

        if (trainerId >= sTrainerDataCount) {
            fprintf(stderr, "Trainer text order contains out-of-range trainer id %u\n", trainerId);
            free(offsets);
            exit(EXIT_FAILURE);
        }

        entry = &sTrainerData[trainerId];
        textCount = GetTrainerTextEntryCount(entry);
        if (textCount == 0) {
            continue;
        }

        offsets[trainerId] = offset;
        for (j = 0; j < textCount; j++) {
            uint8_t data[TRAINER_TEXT_ENTRY_SIZE];

            WriteLe16(&data[0], trainerId);
            WriteLe16(&data[2], entry->text[j].type);
            if (fwrite(data, sizeof(data), 1, mapFile) != 1) {
                perror(mapPath);
                fclose(mapFile);
                fclose(offsetFile);
                free(offsets);
                exit(EXIT_FAILURE);
            }
            WriteTextFile(rawTextDir, textFileIndex++, entry->text[j].text);
            offset = (uint16_t)(offset + TRAINER_TEXT_ENTRY_SIZE);
        }
    }

    for (i = 0; i < trainerTextOffsetCount; i++) {
        uint8_t data[2];

        WriteLe16(data, offsets[i]);
        if (fwrite(data, sizeof(data), 1, offsetFile) != 1) {
            perror(offsetPath);
            fclose(mapFile);
            fclose(offsetFile);
            free(offsets);
            exit(EXIT_FAILURE);
        }
    }

    fclose(mapFile);
    fclose(offsetFile);
    free(offsets);
}

int main(int argc, char **argv) {
    const char *trainerDataDir;
    const char *trainerPartyDir;
    const char *trainerTextMapDir;
    const char *trainerTextOffsetDir;
    const char *rawTextRoot;
    char trainerNamesDir[RAWTEXT_PATH_LENGTH];
    char trainerTextDir[RAWTEXT_PATH_LENGTH];
    int i;

    if (argc != 6) {
        fprintf(stderr, "Usage: %s <a055-dir> <a056-dir> <a057-dir> <a131-dir> <rawtext-root>\n", argv[0]);
        return EXIT_FAILURE;
    }

    trainerDataDir = argv[1];
    trainerPartyDir = argv[2];
    trainerTextMapDir = argv[3];
    trainerTextOffsetDir = argv[4];
    rawTextRoot = argv[5];

    sPresence = calloc(sTrainerDataCount, sizeof(*sPresence));
    if (sPresence == NULL) {
        perror("calloc");
        return EXIT_FAILURE;
    }
    ParseTrainerSourcePresence();
    ValidateTrainerPresence();

    snprintf(trainerNamesDir, sizeof(trainerNamesDir), "%s/729", rawTextRoot);
    snprintf(trainerTextDir, sizeof(trainerTextDir), "%s/728", rawTextRoot);

    for (i = 0; i < (int)sTrainerDataCount; i++) {
        WriteTrainerHeaderMember(trainerDataDir, i, &sTrainerData[i]);
        WriteTrainerPartyMember(trainerPartyDir, i, &sTrainerData[i]);
        WriteTrainerNameTextFile(trainerNamesDir, i, sTrainerData[i].name);
    }

    WriteTrainerTextData(trainerTextMapDir, trainerTextOffsetDir, trainerTextDir);
    return EXIT_SUCCESS;
}
