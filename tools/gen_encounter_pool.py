#!/usr/bin/env python3
"""Generates src/encounter_pool_data.h for the wild-encounter randomizer.

Sources:
  include/constants/species.h          - species constants
  data/Evolutions.c                    - which species can still evolve
  data/Species.c                       - base stats (BST)
  data/Encounters.c                    - route encounters (origins)
  data/SafariEncounters.c              - safari zone (origins)
  data/Headbutt.c                      - headbutt trees (origins)
  base_dsrom/files/data/mushi/mushi_encount.bin - bug contest (origins)

Run from the repo root:  .venv/bin/python tools/gen_encounter_pool.py
Regenerate whenever any of the above sources change.
"""
import re
import struct
import sys

# The nine base-species ranges in hg-engine numbering (the 50 slots in
# between are inserted form entries and are deliberately excluded).
GEN_RANGES = [
    (1, 151),     # 1 Kanto
    (152, 251),   # 2 Johto
    (252, 386),   # 3 Hoenn
    (387, 493),   # 4 Sinnoh
    (544, 699),   # 5 Unova
    (700, 771),   # 6 Kalos
    (772, 859),   # 7 Alola
    (860, 955),   # 8 Galar
    (956, 1075),  # 9 Paldea
]

# Mythical Pokemon: never participate in the randomization (neither as
# origins nor as candidates).
MYTHICALS = [
    "MEW", "CELEBI", "JIRACHI", "DEOXYS", "PHIONE", "MANAPHY", "DARKRAI",
    "SHAYMIN", "ARCEUS", "VICTINI", "KELDEO", "MELOETTA", "GENESECT",
    "DIANCIE", "HOOPA", "VOLCANION", "MAGEARNA", "MARSHADOW", "ZERAORA",
    "MELTAN", "MELMETAL", "ZARUDE", "PECHARUNT",
]

# Species that must keep their vanilla encounters (e.g. the Unown chambers).
KEEP_VANILLA = ["UNOWN"]


def parse_species_constants():
    names = {}
    for line in open("include/constants/species.h", encoding="utf-8"):
        m = re.match(r"#define\s+(SPECIES_[A-Z0-9_]+)\s+([0-9]+)\s*(?://.*)?$", line.strip())
        if m:
            names[m.group(1)] = int(m.group(2))
    return names


def parse_evolutions():
    """Returns {species_name: can_still_evolve}"""
    src = open("data/Evolutions.c", encoding="utf-8").read()
    parts = re.split(r"\[(SPECIES_[A-Z0-9_]+)\]\s*=\s*\{", src)
    evo = {}
    for i in range(1, len(parts), 2):
        name, body = parts[i], parts[i + 1]
        kinds = re.findall(r"EVO_[A-Z_]+", body)
        evo[name] = any(k != "EVO_NONE" for k in kinds)
    return evo


def parse_bst():
    """Returns {species_name: base stat total}"""
    src = open("data/Species.c", encoding="utf-8").read()
    parts = re.split(r"\[(SPECIES_[A-Z0-9_]+)\]\s*=\s*\{", src)
    bst = {}
    for i in range(1, len(parts), 2):
        name, body = parts[i], parts[i + 1]
        m = re.search(
            r"\.baseStats\s*=\s*\{\s*\.hp\s*=\s*(\d+),\s*\.attack\s*=\s*(\d+),"
            r"\s*\.defense\s*=\s*(\d+),\s*\.spAttack\s*=\s*(\d+),"
            r"\s*\.spDefense\s*=\s*(\d+),\s*\.speed\s*=\s*(\d+),",
            body,
        )
        if m:
            bst[name] = sum(int(x) for x in m.groups())
    return bst


def species_names_in(path):
    s = set(re.findall(r"SPECIES_[A-Z0-9_]+", open(path, encoding="utf-8").read()))
    s.discard("SPECIES_NONE")
    return s


def main():
    const = parse_species_constants()
    evo = parse_evolutions()
    bst = parse_bst()

    def in_base_range(v):
        return any(lo <= v <= hi for lo, hi in GEN_RANGES)

    def gen_of(v):
        for i, (lo, hi) in enumerate(GEN_RANGES):
            if lo <= v <= hi:
                return i
        return -1

    # the const dict is keyed by "SPECIES_XXX" while the lists above use bare
    # names, so resolve them to species *values* for the exclusion checks
    excluded_vals = set()
    for n in MYTHICALS + KEEP_VANILLA:
        key = "SPECIES_" + n
        if key in const:
            excluded_vals.add(const[key])
        else:
            print(f"WARNING: {key} not found in species.h")

    # ---- candidate pool: final-stage, non-mythical, base species ----
    pool = []
    for name, val in sorted(const.items(), key=lambda kv: kv[1]):
        if not in_base_range(val):
            continue
        if val in excluded_vals:
            continue
        if name not in bst or bst[name] == 0:
            continue
        if evo.get(name, False):
            continue  # can still evolve
        pool.append((val, bst[name], gen_of(val)))

    pool.sort()

    # ---- origin set: every species appearing in any encounter source ----
    origins = set()
    for path in ("data/Encounters.c", "data/SafariEncounters.c", "data/Headbutt.c"):
        for name in species_names_in(path):
            if name in const and const[name] != 0 and const[name] not in excluded_vals:
                origins.add(const[name])

    mushi = open("base_dsrom/files/data/mushi/mushi_encount.bin", "rb").read()
    for i in range(0, len(mushi) // 8):
        v = struct.unpack_from("<H", mushi, i * 8)[0]
        if 0 < v < 1100 and v not in excluded_vals:
            origins.add(v)

    origins = sorted(origins)

    # ---- consistency checks ----
    n_pool = len(pool)
    n600 = sum(1 for _, b, _ in pool if b <= 600)
    n500 = sum(1 for _, b, _ in pool if b <= 500)
    print(f"candidate pool: {n_pool}  (bst<=600: {n600}, bst<=500: {n500})")
    print(f"origin species: {len(origins)}")
    if len(origins) > n500:
        print("WARNING: hard-mode pool smaller than origin set; duplicates will occur")
    if len(origins) > n600:
        print("WARNING: normal-mode pool smaller than origin set; duplicates will occur")

    # ---- emit ----
    out = []
    out.append("// AUTO-GENERATED by tools/gen_encounter_pool.py - do not edit by hand.")
    out.append("// Regenerate after editing Encounters.c / SafariEncounters.c / Headbutt.c /")
    out.append("// Evolutions.c / Species.c / mushi_encount.bin.")
    out.append("")
    out.append("#define ENCOUNTER_POOL_COUNT %d" % n_pool)
    out.append("")
    out.append("// candidate species and their base stat total encoded as")
    out.append("// ceil(bst / 4) to save payload space (500 and 600 divide by 4 exactly,")
    out.append("// so the difficulty caps stay exact)")
    out.append("static const u16 sEncounterPoolSpecies[ENCOUNTER_POOL_COUNT] = {")
    for i, (val, b, g) in enumerate(pool):
        name = next(k for k, v in const.items() if v == val)
        out.append("    %s, // %s gen %d" % (name, b, g + 1))
    out.append("};")
    out.append("")
    out.append("static const u8 sEncounterPoolBst4[ENCOUNTER_POOL_COUNT] = {")
    for i in range(0, n_pool, 16):
        chunk = pool[i:i + 16]
        out.append("    " + " ".join("%d," % ((b + 3) // 4) for _, b, _ in chunk))
    out.append("};")
    out.append("")
    out.append("#define ENCOUNTER_ORIGIN_COUNT %d" % len(origins))
    out.append("")
    out.append("static const u16 sEncounterOriginSpecies[ENCOUNTER_ORIGIN_COUNT] = {")
    for i in range(0, len(origins), 8):
        chunk = origins[i:i + 8]
        out.append("    " + " ".join("0x%04X," % v for v in chunk))
    out.append("};")
    out.append("")

    with open("src/encounter_pool_data.h", "w", encoding="utf-8") as f:
        f.write("\n".join(out))
    print("wrote src/encounter_pool_data.h")


if __name__ == "__main__":
    sys.exit(main())
