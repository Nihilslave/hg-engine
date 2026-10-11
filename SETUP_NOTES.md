# hg-engine 本机工作笔记（WSL 环境）

> 2026-10-09 整理（由 Copilot 根据本次配置会话生成）。
> 本地私用笔记：未跟踪时不提交也可；若要提交到自己的 fork 也没问题。
> 主项目路径：`~/personalCode/pokemon-fangame/hg-engine`

---

## 0. 路线决策记录（2026-10-09）

目标：基于宝可梦心金/魂银做自己的玩法向改版。

| 方案 | 结论 | 原因 |
|---|---|---|
| pokeheartgold（反编译） | ⏸️ 搁置（保留作参考） | 完成度不错但仍需专有 MWCC 工具链，且大量模块还是汇编；不适合玩法向快速迭代 |
| 黑/白、黑2/白2 改版 | ❌ 放弃 | 没有可用的反编译项目，只能工具+汇编注入，生态小 |
| 从零做游戏（基于 Showdown） | ❌ 放弃 | Showdown 只提供战斗引擎，世界/剧情/UI 全要重写，工作量不成比例 |
| **hg-engine（基于 pokeheartgold 的引擎分支）** | ✅ **采用** | 补丁式构建（armips 打在原版 ROM 上）、大量现代机制已实现、社区活跃、构建仅需免费工具链 |

hg-engine 已提供的现成内容：Gen6 全图鉴（部分后续世代）、512 特性、招式/道具扩展、Mega+原始回归、妖精系、隐藏特性、新天气、30 个 PC 箱子等。
**我们的工作重心 = 把这些机制布置进游戏（遭遇/训练师/进化/平衡）+ 玩法设计 + 剧情。**

---

## 1. 仓库与 Git

- 仓库位置：`~/personalCode/pokemon-fangame/hg-engine`
- 自己的 fork：`origin` → `https://github.com/Nihilslave/hg-engine.git`
- 上游：`upstream` → `https://github.com/BluRosie/hg-engine.git`
- 克隆时的基线提交：`5f1c91fb9`（2026-10-05，main 分支）
- 同步上游更新：`git fetch upstream && git merge upstream/main`

## 2. 本机环境（WSL2 / Ubuntu 22.04.5）——已完成的配置

### 2.1 系统依赖（apt）
- 已安装：`libpng-dev`、`gcc-arm-none-eabi`（10.3.1）
- 系统自带：`build-essential`(gcc 12.3)、`pkg-config`、`python3-venv`、`make`
- 注意：本机 sudo 需密码；apt 走 Clash 代理（`127.0.0.1:10809`，环境变量已导出）。
  遇到 404 一般是索引过期：先 `sudo apt-get update`，必要时 `sudo -E apt-get ...` 保留代理变量。

### 2.2 Rust
- 本机 stable 是 1.83（太旧，ds-rom 需要 edition2024 ≥1.85）；1.98.1 缺 cargo 组件。
- 已设置**目录级 override**：在 hg-engine 目录内使用 **1.88.0**：
  ```bash
  rustup override set 1.88.0    # 已执行；撤销：rustup override unset
  ```
- crates.io 被墙：已写镜像到 `~/.cargo/config.toml`（rsproxy-sparse）。撤销：删除该文件。

### 2.3 Python
- `.venv`（miniconda Python 3.13）已创建并安装 `requirements.txt`：
  `ndspy==4.1.0, pandas, Pillow, py-desmume, PyYAML`
- 注意：`py-desmume` 是给仓库自带自动化战斗测试（`AUTO_TEST=Y`）用的，平常用不到。

### 2.4 已预构建的工具（tools/）
`armips`、`dsrom`、`adpcm-xq`、`msgenc`、`nitrogfx`、`btx`、`ENCODE_IMG`、`o2narc`、`jsonproc`、`movedatagen`、`pokedexdatagen`、`speciesdatagen`、`trainerdatagen`、`ntrWavTool.py`
> `make` 会检测这些文件，存在即跳过重建。不要手贱 `make clean_tools`（会连 .venv 一起删）。

### 2.5 模拟器 melonDS 1.1（跑在 WSL 里）
- 安装：官方 AppImage（`melonDS-1.1-appimage-x86_64.zip`）解包到 `~/opt/melonds/`（无 FUSE 依赖）。
- 启动：`melonds <ROM路径>`（包装脚本在 `~/.local/bin/melonds`，已在 PATH）。
  ```bash
  cd ~/personalCode/pokemon-fangame/hg-engine
  melonds test.nds
  ```
- 通过 WSLg 显示到 Windows 桌面（音频走 Windows）。启动日志里的
  `wayland plugin not found` 和 `bad ROM size 268435456` 均为正常警告。
- 配置/存档：
  - 模拟器配置：`~/.config/melonDS/melonDS.toml`
  - 游戏存档：ROM 旁边同名 `.sav`（如 `test.sav`），**重编译 ROM 后存档继续有效**；想从头测试就删掉它。
- 画面异常时：melonDS 设置里将 3D renderer 切换为 `software`。
- 可选（提高精度）：把自 dump 的 `bios7.bin / bios9.bin / firmware.bin` 放到 `~/.config/melonDS/`。

## 3. 构建工作流

```bash
cd ~/personalCode/pokemon-fangame/hg-engine

# 1) 改代码/数据
# 2) 构建（首次 ~30 秒，增量更快）
make -j$(nproc)          # 产出 test.nds（256MB，扩展版，正常）

# 3) 测试
melonds test.nds         # 或直接重开 melonDS 窗口
```

- 全量日志：上一次构建我当时存在 `/tmp/hg_build.log`（可自行重定向）。
- 改代码后行为异常时：`make clean_code && make -j$(nproc)` 再试。
- `make clean` 会删 `build/ base/ base_dsrom/`，**不会**删 `rom.nds`。

## 4. 改内容的入口（已实测）

| 想改什么 | 文件 |
|---|---|
| 种族值/属性/特性 | `data/Species.c` |
| 招式数据 | `data/Moves.c` |
| 训练师队伍/文本 | `data/Trainers.c`、`data/text/` |
| 进化方式 | `data/Evolutions.c` |
| 野生遭遇（含头锤/狩猎区） | `data/Encounters.c` |
| 学习表 | `data/learnsets/learnsets.json` |
| 道具数据 | `data/itemdata/` |
| 精灵图/图标等 | `data/graphics/` |
| 事件/脚本（高级） | `data/eventdata/`、`data/scr_seq/` |

## 5. 功能开关（CONFIG.md 有完整说明）

- `include/config.h` + `armips/include/config.s`：
  - 妖精系 `FAIRY_TYPE_IMPLEMENTED`
  - 隐藏特性 `HIDDEN_ABILITIES`、Mega `MEGA_EVOLUTIONS`、原始回归 `PRIMAL_REVERSION`
  - 存档扩展 `ALLOW_SAVE_CHANGES`（关掉可保 PKHeX 兼容）
  - 战斗设为 Set 模式 `BATTLE_MODE_FORCE_SET`、可重复 TM `REUSABLE_TMS`、
    可遗忘秘传机 `DELETABLE_HMS`、文本加速 `FAST_TEXT_PRINTING` 等
- 文档：本地 `documentation/wiki/`，在线 wiki 见文末链接。

## 6. 不要提交 / 分发的东西

- `rom.nds`（自己的卡带 dump，美版心金 `IPKE`）、`test.nds`、`*.sav` —— 已在 `.gitignore` 中（构建后 `git status` 干净验证过）。
- 不要在网上分发 ROM 或改版 ROM；分享时用 **xdelta 补丁**（对原版 ROM 打补丁）。

## 7. 常用命令速查

```bash
# 同步上游
git fetch upstream && git merge upstream/main

# 提交到自己的 fork
git add -A && git commit -m "..." && git push origin main

# 撤销本机特殊配置（一般不用）
rustup override unset            # 在仓库目录内
rm ~/.cargo/config.toml          # 删 cargo 镜像
rm -rf ~/opt/melonds ~/.local/bin/melonds ~/.config/melonDS   # 删模拟器
```

## 8. 已知小坑

- `make` 会在解析阶段检查 `rom.nds` 的 gamecode（必须是 `IPKE`）；文件不在/版本不对会直接报错。
- crates.io / pypi 在本机网络直连不稳定，cargo 已配镜像；pip 直接装通常能成功。
- melonDS 没有 BIOS 也能跑（用 FreeBIOS），个别场景（麦克风、WFC 等）建议放真实固件。

## 9. 相关链接

- hg-engine wiki：https://github.com/BluRosie/hg-engine/wiki
- 上游仓库：https://github.com/BluRosie/hg-engine
- DS 改版资料（权威）：https://ds-pokemon-hacking.github.io/
- DSPRE（Gen4 图形化 ROM 编辑器，改地图/事件/脚本用，Windows+Wine）：https://github.com/DS-Pokemon-Rom-Editor/DSPRE
- 我方 fork：https://github.com/Nihilslave/hg-engine

## 10. 待办 / 想法

- [ ] 确认 melonDS 窗口显示正常、游戏可正常进入标题画面
- [ ] 选定第一个改动练手（建议从 `data/Species.c` 或 `data/Trainers.c` 开始）
- [ ] 想清楚大方向：平衡性调整 / 难度改版 / 新玩法机制 / 剧情向
- [ ] 可选：本地无线联机（开两个 melonDS 实例）做交换/对战测试
- [ ] 可选：`make AUTO_TEST=Y` 的自动化战斗测试（依赖 py-desmume）
- [ ] 搁置项：pokeheartgold（`~/personalCode/pokemon-fangame/pokeheartgold`）暂不需要
