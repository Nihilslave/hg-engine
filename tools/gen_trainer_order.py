#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""生成《训练师调整顺序清单》（按主线剧情推荐顺序）。

数据来源（只读）：
  - asm/include/trainers.inc                   训练师常量 <-> 编号
  - data/Trainers.c                            队伍/职业/道具等（编辑目标文件）
  - <pokeheartgold>/files/fielddata/eventdata/zone_event/*.json
                                               地图事件对象里的 std_trainer(TRAINER_X) 绑定
  - <pokeheartgold>/files/fielddata/script/scr_seq/*.s
                                               特殊战斗（馆主/劲敌/四天王等）TrainerBattle 命令
  - <pokeheartgold>/files/tel/pmtel_book.json  电话联系人（重战标记）
  - <pokeheartgold>/include/constants/maps.h   内部地图名 <-> 正式地图名

输出：
  - documentation/trainer_order.md
  - documentation/trainer_order.csv

用法：
  python3 tools/gen_trainer_order.py [--pokeheartgold PATH] [--out DIR]

想调整剧情顺序：改下面的 STORY 列表（区域标题, [内部地图名, ...]）。
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
DEFAULT_PHG = REPO.parent / "pokeheartgold"

# 个别训练师的特殊备注（常量 -> 备注文本）
NOTES = {
    "TRAINER_PKMN_TRAINER_RED_RED": "白银山山顶（16 枚徽章后）",
    "TRAINER_ROCKET_BOSS_GIOVANNI": "奇拉比配信事件限定",
    "TRAINER_MYSTERY_MAN_EUSINE": "水君剧情（初次到湛蓝市时）",
    "TRAINER_ACE_TRAINER_F_IRENE": "Route 34 秘密基地事件（三姐妹，胜后得 Power Herb）",
    "TRAINER_ACE_TRAINER_F_JENN": "同三姐妹事件",
    "TRAINER_ACE_TRAINER_F_KATE": "同三姐妹事件",
    "TRAINER_TEAM_ROCKET_GRUNT_26": "舞蹈剧院（早期剧情）",
}
# 按常量名匹配的备注（正则 -> 备注文本）
NOTES_PATTERNS = [
    (r"^TRAINER_ELITE_FOUR_.*_2$", "通关后重打联盟时的强化阵容"),
    (r"^TRAINER_CHAMPION_LANCE_2$", "通关后重打联盟时的强化阵容"),
    (r"^TRAINER_RIVAL_SILVER_(19|20|21)$", "通关后重打联盟时再战"),
    (r"^TRAINER_RIVAL_SILVER_(14|15|16)$", "通关后月见山后期战"),
    (r"^TRAINER_KIMONO_GIRL_", "五姐妹对战为剧情后期"),
    (r"^TRAINER_LEADER_.*_2$", "16 枚徽章后在格斗道场重战"),
]


def notes_for(const: str) -> str:
    if const in NOTES:
        return NOTES[const]
    for pat, txt in NOTES_PATTERNS:
        if re.match(pat, const):
            return txt
    return ""

# ---------------------------------------------------------------------------
# 剧情推荐顺序（内部地图名；顺序即章节顺序）
# ---------------------------------------------------------------------------
STORY: list[tuple[str, list[str]]] = [
    # ---- 城都 ----
    ("若叶镇（初始区域）", ["T20", "T20R0101", "T20R0201", "T20R0202", "T20R0301", "T20R0401", "T20R0402"]),
    ("29号道路", ["R29", "R29R0101"]),
    ("樱花镇（含劲敌·首战）", ["T21", "T21FS0101", "T21PC0101", "T21PC0102", "T21R0301", "T21R0401", "T21R0501"]),
    ("30号道路（含大木博士小屋）", ["R30", "R30R0101", "R30R0201"]),
    ("31号道路", ["R31", "R31R0101"]),
    ("桔梗市·喇叭芽之塔", ["D15R0101", "D15R0102", "D15R0103"]),
    ("桔梗市·道馆（阿速）", ["T22GYM0101"]),
    ("桔梗市", ["T22", "T22FS0101", "T22PC0101", "T22PC0102", "T22R0301", "T22R0401", "T22R0501", "T22R0601", "T22R0701"]),
    ("32号道路", ["R32", "R32R0101", "R32PC0101", "R32PC0102"]),
    ("阿露福遗迹（可选）", ["D24R0101", "D24R0102", "D24R0201", "D24R0202", "D24R0203", "D24R0204",
                    "D24R0205", "D24R0206", "D24R0207", "D24R0208", "D24R0209", "D24R0210",
                    "D24R0211", "D24R0212", "D24R0213", "D24R0214", "D24R0215", "D24R0216",
                    "D24R0217", "D24R0218"] ),
    ("连结洞窟", ["D25R0101", "D25R0102", "D25R0103"]),
    ("33号道路", ["R33"]),
    ("桧皮镇·呆呆兽之井", ["D26R0101", "D26R0102", "D26R0103"]),
    ("桧皮镇·道馆（阿笔）", ["T23GYM0101", "T23GYM0102"]),
    ("桧皮镇（含劲敌·二战）", ["T23", "T23FS0101", "T23PC0101", "T23PC0102", "T23R0101", "T23R0201", "T23R0501"]),
    ("桐树林", ["D36R0101"]),
    ("34号道路（含饲育屋）", ["R34", "R34R0101", "R34R0201"]),
    ("满金市·道馆（小茜）", ["T25GYM0101"]),
    ("满金市", ["T25", "T25PC0101", "T25PC0102", "T25R0201", "T25R0301", "T25R0401", "T25R0501",
              "T25R0502", "T25R0601", "T25R0801", "T25R0901", "T25R1001", "T25R1002", "T25R1003",
              "T25R1004", "T25R1005", "T25R1006", "T25R1101", "T25R1201", "T25R1202", "T25R1203", "T25SP0101"]),
    ("满金市·地下通道 B1F", ["D37R0101", "D37R0102"]),
    ("35号道路", ["R35", "R35R0101", "R35R0201"]),
    ("自然公园", ["D22R0101", "D22R0102"]),
    ("36号道路", ["R36", "R36R0101", "R36R0201"]),
    ("37号道路", ["R37"]),
    ("圆朱市·烧焦塔（含劲敌·三战）", ["D18R0101", "D18R0102"]),
    ("圆朱市·道馆（松叶）", ["T27GYM0101"]),
    ("圆朱市·舞蹈剧院（早期打退火箭队员；五姐妹对战在剧情后期）", ["T27R0501"]),
    ("圆朱市", ["T27", "T27FS0101", "T27PC0101", "T27PC0102", "T27PC0103", "T27PC0104", "T27PC0105",
              "T27R0101", "T27R0201", "T27R0202", "T27R0301", "T27R0401", "T27R0601", "T27R0801"]),
    ("38号道路", ["R38", "R38R0101"]),
    ("39号道路（含牧场）", ["R39", "R39R0101", "R39R0201"]),
    ("浅葱市·灯塔", ["D27R0101", "D27R0102", "D27R0103", "D27R0104", "D27R0105", "D27R0106", "D27R0107", "D27R0108"]),
    ("浅葱市·道馆（阿蜜）", ["T26GYM0101"]),
    ("浅葱市", ["T26", "T26PC0101", "T26PC0102", "T26FS0101", "T26R0301", "T26R0501", "T26R0601", "T26R0701"]),
    ("40号水路", ["W40", "W40R0101"]),
    ("41号水路", ["W41"]),
    ("湛蓝市（含神秘人尤塞恩战）", ["T24", "T24PC0101", "T24PC0102", "T24R0201", "T24R0501", "T24R0601", "T24R0701", "T24R0801"]),
    ("湛蓝市·道馆（阿四）", ["T24GYM0101"]),
    ("42号道路", ["R42", "R42R0101"]),
    ("钢铁山（深层需冲浪/攀瀑）", ["D38R0101", "D38R0102", "D38R0103", "D38R0104"]),
    ("卡吉镇", ["T28", "T28PC0101", "T28PC0102", "T28R0201"]),
    ("43号道路", ["R43", "R43R0101", "R43R0201"]),
    ("愤怒湖", ["T29", "T29R0101", "T29R0201"]),
    ("卡吉镇·火箭队总部", ["D35R0101", "D35R0102", "D35R0103", "D35R0104"]),
    ("卡吉镇·道馆（柳伯）", ["T28GYM0101", "T28GYM0102", "T28GYM0103"]),
    ("满金市·电波塔（火箭队事件·第7枚徽章后触发）", ["D23R0101", "D23R0102", "D23R0103", "D23R0104", "D23R0105", "D23R0106", "D23R0107"]),
    ("满金市·地下通道 B2F / 仓库", ["D37R0103", "D37R0104", "D37R0105"]),
    ("44号道路", ["R44"]),
    ("冰之通道", ["D39R0101", "D39R0102", "D39R0103", "D39R0104"]),
    ("烟墨市·道馆（小桩）", ["T30GYM0101"]),
    ("烟墨市", ["T30", "T30FS0101", "T30PC0101", "T30PC0102", "T30R0201", "T30R0301", "T30R0601"]),
    ("龙穴", ["D44R0101", "D44R0102", "D44R0103"]),
    ("45号道路", ["R45"]),
    ("46号道路（可选）", ["R46"]),
    ("黑暗洞窟（可选）", ["D42R0101", "D42R0102"]),
    ("47/48号道路·崖边通道·狩猎区（可选）", ["R47", "R48", "D48R0101", "D47", "D47R0101", "D47R0102", "D47PC0101",
                                     "SAF01", "SAF02", "SAF03", "SAF04", "SAF05", "SAF06", "SAF07",
                                     "SAF08", "SAF09", "SAF10", "SAF11", "SAF12", "SAF13", "SAF14"]),
    ("27号道路", ["R27", "R27R0101"]),
    ("26号道路（含正辉家）", ["R26", "R26R0101", "R26R0201"]),
    ("东城瀑布", ["D45R0101", "D45R0102"]),
    ("冠军之路", ["D43R0101", "D43R0102", "D43R0103"]),
    ("石英高原·联盟（四天王 / 冠军）", ["T10", "T10R0101", "T10R0201", "T10R0301", "T10R0401",
                                 "T10R0501", "T10R0601", "T10R0701", "T10R0801",
                                 "R22", "R22R0101"]),
    # ---- 关都（通关后，顺序为推荐路线） ----
    ("水手号 S.S. Aqua（浅葱→枯叶）", ["P01R0101", "P01R0102", "P01R0103", "P01R0104",
                                       "P01R0301", "P01R0302", "P01R0303", "P01R0304",
                                       "P01R0305", "P01R0306", "P01R0307"]),
    ("枯叶市·道馆（马志士）", ["T06GYM0101"]),
    ("枯叶市", ["T06", "T06PC0101", "T06PC0102", "T06FS0101", "T06R0101", "T06R0301", "T06R0401", "T06R0601"]),
    ("6号道路", ["R06", "R06R0201"]),
    ("11号道路", ["R11", "R11R0101"]),
    ("金黄市·道馆（娜姿）", ["T11GYM0101"]),
    ("金黄市", ["T11", "T11PC0101", "T11PC0102", "T11FS0101", "T11R0501", "T11R0601", "T11R0602",
              "T11R0701", "T11R0702", "T11R0703", "T11R0801", "T11R0802", "T11R0901"]),
    ("8号道路", ["R08", "R08R0201"]),
    ("玉虹市·道馆（莉佳）", ["T07GYM0101"]),
    ("玉虹市", ["T07", "T07PC0101", "T07PC0102", "T07R0101", "T07R0102", "T07R0103", "T07R0104",
              "T07R0105", "T07R0106", "T07R0107", "T07R0201", "T07R0202", "T07R0203", "T07R0204",
              "T07R0205", "T07R0206", "T07R0207", "T07R0401", "T07R0501", "T07R0701", "T07SP0101"]),
    ("16/17/18号道路（自行车道）", ["R16", "R16R0101", "R16R0201", "R16R0301", "R17", "R18", "R18R0101"]),
    ("浅红市·道馆（阿杏）", ["T08GYM0101"]),
    ("浅红市", ["T08", "T08FS0101", "T08PC0101", "T08PC0102", "T08R0201", "T08R0401", "T08R0601"]),
    ("12/13/14/15号道路", ["R12", "R12R0101", "R13", "R14", "R15", "R15R0101"]),
    ("紫苑镇", ["T05", "T05FS0101", "T05PC0101", "T05PC0102", "T05R0201", "T05R0301", "T05R0401", "T05R0601", "T05R0701"]),
    ("9/10号道路·岩山隧道", ["R09", "R09R0101", "R10", "R10PC0101", "R10PC0102", "R10R0101",
                        "R10R0201", "R10R0202", "D05R0101", "D05R0102"]),
    ("华蓝市·道馆（小霞）", ["T04GYM0101"]),
    ("华蓝市", ["T04", "T04FS0101", "T04PC0101", "T04PC0102", "T04R0101", "T04R0201", "T04R0301", "T04R0401"]),
    ("24/25号道路（海角）", ["R24", "R25", "R25R0101"]),
    ("3/4号道路", ["R03", "R03PC0101", "R04"]),
    ("尼比市·道馆（小刚）", ["T03GYM0101"]),
    ("尼比市", ["T03", "T03FS0101", "T03PC0101", "T03PC0102", "T03R0101", "T03R0201", "T03R0601"]),
    ("常磐森林", ["D46R0101"]),
    ("2号道路", ["R02", "R02R0101", "R02R0201", "R02R0301", "R02R0401", "R02R0501"]),
    ("常磐市·道馆（青绿）", ["T02GYM0101"]),
    ("常磐市", ["T02", "T02FS0101", "T02PC0101", "T02PC0102", "T02R0201", "T02R0301", "T02R0302", "T02R0401"]),
    ("1号道路", ["R01"]),
    ("真新镇", ["T01", "T01R0101", "T01R0102", "T01R0201", "T01R0202", "T01R0301"]),
    ("21号水路", ["W21"]),
    ("20号水路·双子岛（红莲道馆·夏伯）", ["W20", "W20R0101", "D11R0101", "D11R0102", "D11R0103",
                                "D11R0104", "D11R0105", "D11R0106", "D11R0107",
                                "T09", "T09PC0101", "T09PC0102"]),
    ("19号水路", ["W19", "W19R0101"]),
    # ---- 通关后 ----
    ("【通关后】金黄市·格斗道场（16 馆主重战）", ["T11R0101"]),
    ("【通关后】月见山（劲敌·后期战）", ["D02R0101", "D02R0102", "D02R0103", "D02R0104"]),
    # ---- 通关后·城都 ----
    ("漩涡列岛（洛奇亚）", ["D40R0101", "D40R0102", "D40R0104", "D40R0106", "D40R0107"]),
    ("圆朱市·铃铛塔（凤王）", ["D17R0101", "D17R0102", "D17R0103", "D17R0104", "D17R0105",
                        "D17R0106", "D17R0107", "D17R0108", "D17R0109", "D17R0110", "D17R0112"]),
    ("白银山（红）", ["T31", "T31PC0101", "T31PC0102", "R28", "R28R0101",
                 "D41R0101", "D41R0102", "D41R0103", "D41R0104", "D41R0105",
                 "D41R0106", "D41R0107", "D41R0108"]),
]


# ---------------------------------------------------------------------------
# 解析
# ---------------------------------------------------------------------------
def parse_trainers_inc(path: Path):
    const2id: dict[str, int] = {}
    id2const: dict[int, str] = {}
    for m in re.finditer(r"\.equ\s+(TRAINER_[A-Z0-9_]+)\s*,\s*(\d+)", path.read_text(encoding="utf-8")):
        const, tid = m.group(1), int(m.group(2))
        const2id[const] = tid
        id2const[tid] = const
    return const2id, id2const


def _extract_brace(s: str, start: int) -> str:
    """s[start] == '{'，返回花括号内容（不含外层）。"""
    depth = 0
    for i in range(start, len(s)):
        if s[i] == "{":
            depth += 1
        elif s[i] == "}":
            depth -= 1
            if depth == 0:
                return s[start + 1:i]
    return s[start + 1:]


def parse_trainers_c(path: Path):
    text = path.read_text(encoding="utf-8")
    marks = [(m.start(), int(m.group(1))) for m in re.finditer(r"^\s*\[(\d+)\]\s*=\s*\{", text, re.M)]
    marks.append((len(text), None))
    entries: dict[int, dict] = {}

    for i in range(len(marks) - 1):
        start, tid = marks[i]
        chunk = text[start:marks[i + 1][0]]

        name_m = re.search(r"\.name\s*=\s*\"((?:[^\"\\]|\\.)*)\"", chunk)
        name = name_m.group(1).replace('\\"', '"') if name_m else "?"

        cls_m = re.search(r"\.trainerClass\s*=\s*(TRAINERCLASS_\w+)", chunk)
        cls = cls_m.group(1) if cls_m else "TRAINERCLASS_UNKNOWN"

        bt_m = re.search(r"\.battleType\s*=\s*(\w+)", chunk)
        battle_type = bt_m.group(1) if bt_m else "SINGLE_BATTLE"

        items_m = re.search(r"\.items\s*=\s*\{([^}]*)\}", chunk)
        items = []
        if items_m:
            items = [s.strip() for s in items_m.group(1).split(",")
                     if s.strip() and s.strip() != "ITEM_NONE"]

        party = []
        pm = re.search(r"\.party\s*=\s*\{", chunk)
        if pm:
            body = _extract_brace(chunk, chunk.index("{", pm.start()))
            depth = 0
            block_start = None
            for pos, c in enumerate(body):
                if c == "{":
                    if depth == 0:
                        block_start = pos
                    depth += 1
                elif c == "}":
                    depth -= 1
                    if depth == 0 and block_start is not None:
                        mon = body[block_start + 1:pos]
                        lv = re.search(r"\.level\s*=\s*(\d+)", mon)
                        sp = re.search(r"\.species\s*=\s*(SPECIES_\w+)", mon)
                        it = re.search(r"\.item\s*=\s*(ITEM_\w+)", mon)
                        if sp:
                            party.append({
                                "species": sp.group(1),
                                "level": int(lv.group(1)) if lv else 0,
                                "item": it.group(1) if it else None,
                            })
                        block_start = None

        entries[tid] = {"name": name, "class": cls, "battleType": battle_type,
                        "items": items, "party": party}
    return entries


def parse_maps(path: Path):
    internal2human: dict[str, str] = {}
    for m in re.finditer(r"^\s*#define\s+(MAP_\S+)\s+\d+\s*//\s*(MAP_\S+)", path.read_text(encoding="utf-8"), re.M):
        internal2human[m.group(2)[4:]] = m.group(1)[4:]
    return internal2human


def scan_zones(dir_: Path):
    refs: dict[str, list[tuple[str, int]]] = {}
    for f in sorted(dir_.glob("*.json")):
        internal = f.name.split("_", 1)[1][:-5] if "_" in f.name else f.stem
        text = f.read_text(encoding="utf-8", errors="ignore")
        for idx, m in enumerate(re.finditer(r"std_trainer(?:_2)?\((TRAINER_[A-Z0-9_]+)\)", text)):
            refs.setdefault(m.group(1), []).append((internal, idx))
    return refs


def scan_scripts(dir_: Path, valid_internals: set[str]):
    refs: dict[str, list[tuple[str | None, str, int]]] = {}
    for f in sorted(dir_.glob("scr_seq_*.s")):
        stem = f.name[:-2]
        mm = re.match(r"scr_seq_\d+(?:_(.+))?$", stem)
        internal = None
        if mm and mm.group(1):
            cand = mm.group(1)
            if cand.endswith("_hdr"):
                cand = cand[:-4]
            if cand in valid_internals:
                internal = cand
        for ln, line in enumerate(f.read_text(encoding="utf-8", errors="ignore").splitlines(), 1):
            for m in re.finditer(r"\bTrainerBattle\s+(TRAINER_[A-Z0-9_]+)", line):
                refs.setdefault(m.group(1), []).append((internal, f.name, ln))
    return refs


def parse_pmtel(path: Path):
    data = json.loads(path.read_text(encoding="utf-8"))["pmtel_book"]
    out: dict[str, dict] = {}
    for c in data:
        tid = c.get("trainerId", "TRAINER_NONE")
        if tid and tid != "TRAINER_NONE":
            out[tid] = {"id": c.get("id", ""), "rematchWeekday": c.get("rematchWeekday", "")}
    return out


# ---------------------------------------------------------------------------
# 输出
# ---------------------------------------------------------------------------
def strip_const(s: str, *prefixes: str) -> str:
    for p in prefixes:
        if s.startswith(p):
            return s[len(p):]
    return s


def party_str(entry: dict) -> str:
    mons = []
    for mon in entry["party"]:
        s = f"{strip_const(mon['species'], 'SPECIES_')} L{mon['level']}"
        if mon["item"] and mon["item"] != "ITEM_NONE":
            s += f"@{strip_const(mon['item'], 'ITEM_')}"
        mons.append(s)
    return " / ".join(mons) if mons else "—"


def double_mark(bt: str) -> str:
    return {"DOUBLE_BATTLE": "【双打】", "NO_PARTNER_DOUBLE_BATTLE": "【多人】"}.get(bt, "")


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--pokeheartgold", type=Path, default=DEFAULT_PHG)
    ap.add_argument("--out", type=Path, default=REPO / "documentation")
    args = ap.parse_args()

    phg: Path = args.pokeheartgold
    if not phg.is_dir():
        print(f"错误：找不到 pokeheartgold 参考仓库：{phg}", file=sys.stderr)
        return 1

    const2id, id2const = parse_trainers_inc(REPO / "asm/include/trainers.inc")
    trainers = parse_trainers_c(REPO / "data/Trainers.c")
    internal2human = parse_maps(phg / "include/constants/maps.h")
    zones = scan_zones(phg / "files/fielddata/eventdata/zone_event")
    scripts = scan_scripts(phg / "files/fielddata/script/scr_seq", set(internal2human))
    pmtel = parse_pmtel(phg / "files/tel/pmtel_book.json")

    # 变体分组（根名存在时归组）
    roots: dict[str, list[str]] = {}
    for c in const2id:
        root = re.sub(r"(_\d+)+$", "", c)
        if root != c and root in const2id:
            roots.setdefault(root, []).append(c)
    for v in roots.values():
        v.sort(key=lambda c: const2id[c])

    def human_area(internal: str | None) -> str:
        if internal is None:
            return "全局脚本"
        return internal2human.get(internal, internal)

    zone_order: dict[str, int] = {}
    src_rows: list[dict] = []
    placed: set[str] = set()
    number = 0

    def row_for(tid: int, const: str, area: str, area_map: str, sources: str,
                remark: str = "", variant: bool = False) -> dict:
        nonlocal number
        if not variant:
            number += 1
        e = trainers.get(tid, {})
        return {
            "order": number if not variant else "",
            "area_zh": area, "area_map": area_map,
            "id": tid, "const": const,
            "class": strip_const(e.get("class", "?"), "TRAINERCLASS_"),
            "name": e.get("name", "?"),
            "party": f"{double_mark(e.get('battleType', ''))}{party_str(e)}" if e else "?",
            "items": ", ".join(strip_const(i, "ITEM_") for i in e.get("items", [])) or "",
            "source": sources,
            "note": remark,
        }

    rows: list[dict] = []
    area_headers: list[tuple[str, int]] = []   # (标题, 该区第一行在 rows 中的下标)

    def sources_for(const: str) -> tuple[str, str]:
        """返回（来源显示文本, 内部名提示）"""
        parts = []
        maps_seen = []
        for internal, _ in zones.get(const, []):
            if internal not in maps_seen:
                maps_seen.append(internal)
        for internal in maps_seen:
            parts.append(f"对象({human_area(internal)})")
        by_file: dict[str, list[int]] = {}
        for _, fname, ln in scripts.get(const, []):
            by_file.setdefault(fname, []).append(ln)
        for fname, lns in by_file.items():
            desc = f"脚本 {fname}:{lns[0]}"
            if len(lns) > 1:
                desc += f" 等{len(lns)}处"
            parts.append(desc)
        script_first = scripts.get(const, [(None, "", 0)])[0][0]
        return "; ".join(parts), (maps_seen[0] if maps_seen else (script_first or ""))

    # 按剧情顺序收集
    for title, internals in STORY:
        area_rows: list[dict] = []
        emitted_here: list[str] = []

        def emit(const: str):
            if const in placed:
                return
            tid = const2id[const]
            src, _ = sources_for(const)
            ref_maps = [i for i, _ in zones.get(const, [])] + [i for i, _, _ in scripts.get(const, []) if i]
            area_map = next((i for i in internals if i in ref_maps), ref_maps[0] if ref_maps else "")
            area_rows.append(row_for(tid, const, title, area_map, src, remark=notes_for(const)))
            placed.add(const)
            emitted_here.append(const)
            # 未被引用的变体附到主条目下
            for v in roots.get(const, []):
                if v in placed or v in zones or v in scripts:
                    continue
                if const in pmtel:
                    contact = pmtel[const]["id"].replace("PHONE_CONTACT_", "")
                    src_v = f"电话重战变体（联系人 {contact}）"
                else:
                    src_v = "未引用变体（预留/残留，用途待确认）"
                area_rows.append(row_for(const2id[v], v, title, area_map, src_v, variant=True))
                placed.add(v)

        # 该区域内的触发顺序：先事件对象（按对象序），再脚本（按文件/行号）
        seq: list[str] = []
        for internal in internals:
            for const, items in zones.items():
                for i, idx in items:
                    if i == internal and const not in seq:
                        seq.append(const)
        script_hits = []
        for const, items in scripts.items():
            for internal, fname, ln in items:
                if internal in internals:
                    script_hits.append((fname, ln, const))
        script_hits.sort()
        for _, _, const in script_hits:
            if const not in seq:
                seq.append(const)

        for const in seq:
            emit(const)

        if area_rows:
            area_headers.append((title, len(rows)))
            rows.extend(area_rows)

    # 其余未被编排到的（理论上应尽量为空）
    leftover_rows: list[dict] = []
    for const in sorted(zones, key=lambda c: const2id.get(c, 10**9)):
        if const in placed or const not in const2id:
            continue
        tid = const2id[const]
        src, area = sources_for(const)
        leftover_rows.append(row_for(tid, const, "其它（未编排）", area, src, remark=notes_for(const)))
        placed.add(const)
    for const in sorted(scripts, key=lambda c: const2id.get(c, 10**9)):
        if const in placed or const not in const2id:
            continue
        tid = const2id[const]
        src, area = sources_for(const)
        leftover_rows.append(row_for(tid, const, "其它（未编排）", area, src, remark=notes_for(const)))
        placed.add(const)

    # 完全未被引用（含设施/未使用条目/独立变体）
    unreferenced = [id2const[t] for t in sorted(trainers)
                    if t != 0 and t in id2const and id2const[t] not in placed
                    and id2const[t] not in zones and id2const[t] not in scripts]

    # ---------------- 写 Markdown ----------------
    outdir: Path = args.out
    outdir.mkdir(parents=True, exist_ok=True)
    md: list[str] = []
    total_ref = len([c for c in zones if c in const2id]) + len([c for c in scripts if c in const2id and c not in zones])
    md.append("# 训练师调整顺序清单\n")
    md.append("> 按主线剧情推荐顺序（城都 → 联盟 → 关都 → 通关后）。**编号 # 可直接用于沟通**（如“改一下 12 号”）。\n")
    md.append("> 同一区域内的先后顺序仅供参考（以实际游戏内遭遇为准）；标【可选】/关卡为非必经。\n")
    md.append("> 电话重战 / 未引用变体以空编号行附在主条目下方。\n>\n")
    md.append(f"> 生成：`python3 tools/gen_trainer_order.py`；数据：trainers.inc({len(const2id)}) / "
              f"Trainers.c({len(trainers)}) / 事件对象+脚本引用({total_ref}) / 未引用({len(unreferenced)})\n")
    md.append(">\n> 队伍列：`物种 L等级@道具`；【双打】=双打战斗。调整目标文件：`data/Trainers.c`（按 ID 搜索 `[ID] = {`）。\n")
    md.append(">\n> 注意：初始宝可梦与劲敌还会受 `src/starters.c` / `src/field/enemy_party.c` 的运行时逻辑影响。\n")

    def fmt_row(r: dict) -> str:
        no = str(r["order"]) if r["order"] != "" else ""
        return (f"| {no} | {r['id']} | `{r['const']}` | {r['class']} | {r['name']} | {r['party']} | "
                f"{r['source']} | {r['note']} |")

    header = ("| # | ID | 常量 | 职业 | 姓名 | 队伍 | 触发来源 | 备注 |\n"
              "|---|----|------|------|------|------|----------|------|")

    for title, idx in area_headers:
        md.append(f"\n## {title}\n")
        md.append(header)
        for r in rows[idx:]:
            if r["area_zh"] != title:
                break
            md.append(fmt_row(r))

    if leftover_rows:
        md.append("\n## 其它（未编排到剧情顺序）\n")
        md.append(header)
        for r in leftover_rows:
            md.append(fmt_row(r))

    # 附录
    md.append("\n## 附录 A · 道馆馆主 / 四天王 / 冠军 / 劲敌 / 干部\n")
    md.append(header)
    key_pat = re.compile(r"LEADER|ELITE|CHAMPION|RIVAL|PASSERBY|EXECUTIVE|ARCHER|ARIANA|PROTON|PETREL")
    for tid in sorted(trainers):
        const = id2const.get(tid)
        if not const:
            continue
        if key_pat.search(const):
            src, area = sources_for(const)
            r = row_for(tid, const, "附录A", area, src, remark=notes_for(const))
            r["order"] = ""
            md.append(fmt_row(r))

    md.append(f"\n## 附录 B · 完全未引用的条目（{len(unreferenced)} 个）\n")
    md.append("没有任何事件对象/脚本引用的条目。包含：对战塔/边疆首领与搭档训练师（游戏内由设施代码使用，"
              "如 #667-675、#707-711）、大量 Mickey 等占位残留、未直接引用的重战/变体（#735-737 为多人战搭档）。"
              "调整前请先确认用途。\n")
    for i in range(0, len(unreferenced), 4):
        chunk = unreferenced[i:i + 4]
        md.append("- " + "；".join(f"#{const2id[c]} `{c}`（{trainers.get(const2id[c], {}).get('name', '?')}）"
                                for c in chunk))

    (outdir / "trainer_order.md").write_text("\n".join(md) + "\n", encoding="utf-8")

    # ---------------- 写 CSV ----------------
    import csv
    with open(outdir / "trainer_order.csv", "w", newline="", encoding="utf-8-sig") as fp:
        w = csv.writer(fp)
        w.writerow(["order", "area", "map", "id", "const", "class", "name", "party",
                    "trainer_items", "source", "note"])
        for r in rows + leftover_rows:
            w.writerow([r["order"], r["area_zh"], r["area_map"], r["id"], r["const"],
                        r["class"], r["name"], r["party"], r["items"], r["source"], r["note"]])

    print(f"OK: {outdir / 'trainer_order.md'}")
    print(f"OK: {outdir / 'trainer_order.csv'}")
    print(f"训练师条目 {len(trainers)}；有引用 {total_ref}；已编排 {len([r for r in rows])} 行；"
          f"未编排引用 {len(leftover_rows)}；未引用 {len(unreferenced)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
