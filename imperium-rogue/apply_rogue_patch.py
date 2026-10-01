#!/usr/bin/env python3
from pathlib import Path
import shutil, subprocess, sys

TARGET_COMMIT = "f8c9fbb73cc4f3853b056aebfc427d6e4822ea9b"

if len(sys.argv) != 2:
    raise SystemExit("usage: apply_rogue_patch.py /path/to/iriv24-pokeemerald-expansion")
repo = Path(sys.argv[1]).resolve()
pack = Path(__file__).resolve().parent
required = [repo/'src/main_menu.c', repo/'src/save.c', repo/'src/battle_main.c', repo/'src/item_use.c', repo/'data/battle_scripts_2.s', repo/'include/save.h', repo/'include/constants/battle.h', repo/'Makefile']
for p in required:
    if not p.exists():
        raise SystemExit(f"missing expected Imperium source file: {p}")

try:
    head = subprocess.check_output(["git", "-C", str(repo), "rev-parse", "HEAD"], text=True).strip()
    if head != TARGET_COMMIT:
        raise SystemExit(
            f"wrong Imperium source revision: {head}\n"
            f"expected: {TARGET_COMMIT}\n"
            "Run: git checkout f8c9fbb73cc4f3853b056aebfc427d6e4822ea9b"
        )
except (subprocess.CalledProcessError, FileNotFoundError):
    pass

(repo/'include').mkdir(exist_ok=True)
(repo/'src').mkdir(exist_ok=True)
shutil.copy2(pack/'files/include/rogue_mode.h', repo/'include/rogue_mode.h')
shutil.copy2(pack/'files/src/rogue_mode.c', repo/'src/rogue_mode.c')

def replace_once(path: Path, old: str, new: str, name: str):
    text = path.read_text()
    if new in text:
        return
    if old not in text:
        raise SystemExit(f"patch anchor not found ({name}) in {path}")
    path.write_text(text.replace(old, new, 1))

p = repo/'src/main_menu.c'
replace_once(p,
'#include "mystery_gift_menu.h"\n',
'#include "mystery_gift_menu.h"\n#include "rogue_mode.h"\n', 'main menu include')
replace_once(p,
'static const u8 gText_MainMenuNewGame[] = _("New Game");\n',
'static const u8 gText_MainMenuNewGame[] = _("New Game");\nstatic const u8 gText_MainMenuRogueMode[] = _("Rogue Mode");\n', 'rogue menu text')
replace_once(p,
'    ACTION_EREADER,\n    ACTION_INVALID\n',
'    ACTION_EREADER,\n    ACTION_ROGUE_MODE,\n    ACTION_INVALID\n', 'rogue action enum')
replace_once(p,
'                    sCurrItemAndOptionMenuCheck = tMenuType + 1;\n',
'                    sCurrItemAndOptionMenuCheck = tMenuType + 2;\n', 'returning from options cursor')
replace_once(p,
'        tItemCount = tMenuType + 2;\n',
'        tItemCount = tMenuType + 2;\n        if (tMenuType == HAS_NO_SAVED_GAME || tMenuType == HAS_SAVED_GAME)\n            tItemCount++;\n', 'menu item count')

old = '''                FillWindowPixelBuffer(0, PIXEL_FILL(0xA));
                FillWindowPixelBuffer(1, PIXEL_FILL(0xA));
                AddTextPrinterParameterized3(0, FONT_NORMAL, 0, 1, sTextColor_Headers, TEXT_SKIP_DRAW, gText_MainMenuNewGame);
                AddTextPrinterParameterized3(1, FONT_NORMAL, 0, 1, sTextColor_Headers, TEXT_SKIP_DRAW, gText_MainMenuOption);
                PutWindowTilemap(0);
                PutWindowTilemap(1);
                CopyWindowToVram(0, COPYWIN_GFX);
                CopyWindowToVram(1, COPYWIN_GFX);
                DrawMainMenuWindowBorder(&sWindowTemplates_MainMenu[0], MAIN_MENU_BORDER_TILE);
                DrawMainMenuWindowBorder(&sWindowTemplates_MainMenu[1], MAIN_MENU_BORDER_TILE);
'''
new = '''                FillWindowPixelBuffer(0, PIXEL_FILL(0xA));
                FillWindowPixelBuffer(1, PIXEL_FILL(0xA));
                FillWindowPixelBuffer(3, PIXEL_FILL(0xA));
                AddTextPrinterParameterized3(0, FONT_NORMAL, 0, 1, sTextColor_Headers, TEXT_SKIP_DRAW, gText_MainMenuNewGame);
                AddTextPrinterParameterized3(1, FONT_NORMAL, 0, 1, sTextColor_Headers, TEXT_SKIP_DRAW, gText_MainMenuRogueMode);
                AddTextPrinterParameterized3(3, FONT_NORMAL, 0, 1, sTextColor_Headers, TEXT_SKIP_DRAW, gText_MainMenuOption);
                PutWindowTilemap(0);
                PutWindowTilemap(1);
                PutWindowTilemap(3);
                CopyWindowToVram(0, COPYWIN_GFX);
                CopyWindowToVram(1, COPYWIN_GFX);
                CopyWindowToVram(3, COPYWIN_GFX);
                DrawMainMenuWindowBorder(&sWindowTemplates_MainMenu[0], MAIN_MENU_BORDER_TILE);
                DrawMainMenuWindowBorder(&sWindowTemplates_MainMenu[1], MAIN_MENU_BORDER_TILE);
                DrawMainMenuWindowBorder(&sWindowTemplates_MainMenu[3], MAIN_MENU_BORDER_TILE);
'''
replace_once(p, old, new, 'no-save menu layout')

old = '''                FillWindowPixelBuffer(2, PIXEL_FILL(0xA));
                FillWindowPixelBuffer(3, PIXEL_FILL(0xA));
                FillWindowPixelBuffer(4, PIXEL_FILL(0xA));
                AddTextPrinterParameterized3(2, FONT_NORMAL, 0, 1, sTextColor_Headers, TEXT_SKIP_DRAW, gText_MainMenuContinue);
                AddTextPrinterParameterized3(3, FONT_NORMAL, 0, 1, sTextColor_Headers, TEXT_SKIP_DRAW, gText_MainMenuNewGame);
                AddTextPrinterParameterized3(4, FONT_NORMAL, 0, 1, sTextColor_Headers, TEXT_SKIP_DRAW, gText_MainMenuOption);
                MainMenu_FormatSavegameText();
                PutWindowTilemap(2);
                PutWindowTilemap(3);
                PutWindowTilemap(4);
                CopyWindowToVram(2, COPYWIN_GFX);
                CopyWindowToVram(3, COPYWIN_GFX);
                CopyWindowToVram(4, COPYWIN_GFX);
                DrawMainMenuWindowBorder(&sWindowTemplates_MainMenu[2], MAIN_MENU_BORDER_TILE);
                DrawMainMenuWindowBorder(&sWindowTemplates_MainMenu[3], MAIN_MENU_BORDER_TILE);
                DrawMainMenuWindowBorder(&sWindowTemplates_MainMenu[4], MAIN_MENU_BORDER_TILE);
'''
new = '''                FillWindowPixelBuffer(2, PIXEL_FILL(0xA));
                FillWindowPixelBuffer(3, PIXEL_FILL(0xA));
                FillWindowPixelBuffer(4, PIXEL_FILL(0xA));
                FillWindowPixelBuffer(5, PIXEL_FILL(0xA));
                AddTextPrinterParameterized3(2, FONT_NORMAL, 0, 1, sTextColor_Headers, TEXT_SKIP_DRAW, gText_MainMenuContinue);
                AddTextPrinterParameterized3(3, FONT_NORMAL, 0, 1, sTextColor_Headers, TEXT_SKIP_DRAW, gText_MainMenuNewGame);
                AddTextPrinterParameterized3(4, FONT_NORMAL, 0, 1, sTextColor_Headers, TEXT_SKIP_DRAW, gText_MainMenuRogueMode);
                AddTextPrinterParameterized3(5, FONT_NORMAL, 0, 1, sTextColor_Headers, TEXT_SKIP_DRAW, gText_MainMenuOption);
                MainMenu_FormatSavegameText();
                PutWindowTilemap(2);
                PutWindowTilemap(3);
                PutWindowTilemap(4);
                PutWindowTilemap(5);
                CopyWindowToVram(2, COPYWIN_GFX);
                CopyWindowToVram(3, COPYWIN_GFX);
                CopyWindowToVram(4, COPYWIN_GFX);
                CopyWindowToVram(5, COPYWIN_GFX);
                DrawMainMenuWindowBorder(&sWindowTemplates_MainMenu[2], MAIN_MENU_BORDER_TILE);
                DrawMainMenuWindowBorder(&sWindowTemplates_MainMenu[3], MAIN_MENU_BORDER_TILE);
                DrawMainMenuWindowBorder(&sWindowTemplates_MainMenu[4], MAIN_MENU_BORDER_TILE);
                DrawMainMenuWindowBorder(&sWindowTemplates_MainMenu[5], MAIN_MENU_BORDER_TILE);
'''
replace_once(p, old, new, 'saved menu layout')

replace_once(p,
'''                    case 1:
                        action = ACTION_OPTION;
                        break;
''',
'''                    case 1:
                        action = ACTION_ROGUE_MODE;
                        break;
                    case 2:
                        action = ACTION_OPTION;
                        break;
''', 'no-save input mapping')

replace_once(p,
'''                    case 2:
                        action = ACTION_OPTION;
                        break;
                }
                break;
            case HAS_MYSTERY_GIFT:
''',
'''                    case 2:
                        action = ACTION_ROGUE_MODE;
                        break;
                    case 3:
                        action = ACTION_OPTION;
                        break;
                }
                break;
            case HAS_MYSTERY_GIFT:
''', 'saved input mapping')

replace_once(p,
'''            case ACTION_OPTION:
                gMain.savedCallback = CB2_ReinitMainMenu;
                SetMainCallback2(CB2_InitOptionMenu);
                DestroyTask(taskId);
                break;
''',
'''            case ACTION_OPTION:
                gMain.savedCallback = CB2_ReinitMainMenu;
                SetMainCallback2(CB2_InitOptionMenu);
                DestroyTask(taskId);
                break;
            case ACTION_ROGUE_MODE:
                SetMainCallback2(CB2_InitRogueMode);
                DestroyTask(taskId);
                break;
''', 'rogue action handler')

old = '''        case HAS_NO_SAVED_GAME:
        default:
            switch (selectedMenuItem)
            {
                case 0:
                default:
                    SetGpuReg(REG_OFFSET_WIN0V, MENU_WIN_VCOORDS(0));
                    break;
                case 1:
                    SetGpuReg(REG_OFFSET_WIN0V, MENU_WIN_VCOORDS(1));
                    break;
            }
            break;
        case HAS_SAVED_GAME:
            switch (selectedMenuItem)
            {
                case 0:
                default:
                    SetGpuReg(REG_OFFSET_WIN0V, MENU_WIN_VCOORDS(2));
                    break;
                case 1:
                    SetGpuReg(REG_OFFSET_WIN0V, MENU_WIN_VCOORDS(3));
                    break;
                case 2:
                    SetGpuReg(REG_OFFSET_WIN0V, MENU_WIN_VCOORDS(4));
                    break;
            }
            break;
'''
new = '''        case HAS_NO_SAVED_GAME:
        default:
            switch (selectedMenuItem)
            {
                case 0:
                default:
                    SetGpuReg(REG_OFFSET_WIN0V, MENU_WIN_VCOORDS(0));
                    break;
                case 1:
                    SetGpuReg(REG_OFFSET_WIN0V, MENU_WIN_VCOORDS(1));
                    break;
                case 2:
                    SetGpuReg(REG_OFFSET_WIN0V, MENU_WIN_VCOORDS(3));
                    break;
            }
            break;
        case HAS_SAVED_GAME:
            switch (selectedMenuItem)
            {
                case 0:
                default:
                    SetGpuReg(REG_OFFSET_WIN0V, MENU_WIN_VCOORDS(2));
                    break;
                case 1:
                    SetGpuReg(REG_OFFSET_WIN0V, MENU_WIN_VCOORDS(3));
                    break;
                case 2:
                    SetGpuReg(REG_OFFSET_WIN0V, MENU_WIN_VCOORDS(4));
                    break;
                case 3:
                    SetGpuReg(REG_OFFSET_WIN0V, MENU_WIN_VCOORDS(5));
                    break;
            }
            break;
'''
replace_once(p, old, new, 'menu highlight mapping')


# Explicit Rogue battle bit: isolate Rogue behavior from Story battles.
p = repo/'include/constants/battle.h'
replace_once(p,
'#define BATTLE_TYPE_30                 (1 << 30)\n',
'#define BATTLE_TYPE_ROGUE              (1 << 30)\n', 'rogue battle type bit')
replace_once(p,
'                                             | BATTLE_TYPE_RECORDED | BATTLE_TYPE_TRAINER_HILL | BATTLE_TYPE_SECRET_BASE))\n',
'                                             | BATTLE_TYPE_RECORDED | BATTLE_TYPE_TRAINER_HILL | BATTLE_TYPE_SECRET_BASE | BATTLE_TYPE_ROGUE))\n', 'rogue recorded exclusion')

# Battle initialization: Rogue starts from a menu, so do not read overworld terrain
# and do not overwrite the enemy party created by rogue_mode.c.
p = repo/'src/battle_main.c'
replace_once(p,
'#include "randomizer.h"\n',
'#include "randomizer.h"\n#include "rogue_mode.h"\n', 'battle main rogue include')
replace_once(p,
'''    if (!DEBUG_OVERWORLD_MENU || (DEBUG_OVERWORLD_MENU && !gIsDebugBattle))
    {
        gBattleTerrain = BattleSetup_GetTerrainId();
    }
    if (gBattleTypeFlags & BATTLE_TYPE_RECORDED)
        gBattleTerrain = BATTLE_TERRAIN_BUILDING;
''',
'''    if (gBattleTypeFlags & BATTLE_TYPE_ROGUE)
    {
        gBattleTerrain = BATTLE_TERRAIN_PLAIN;
    }
    else if (!DEBUG_OVERWORLD_MENU || (DEBUG_OVERWORLD_MENU && !gIsDebugBattle))
    {
        gBattleTerrain = BattleSetup_GetTerrainId();
    }
    if (gBattleTypeFlags & BATTLE_TYPE_RECORDED)
        gBattleTerrain = BATTLE_TERRAIN_BUILDING;
''', 'rogue fixed battle terrain')
replace_once(p,
'''        if (!(gBattleTypeFlags & (BATTLE_TYPE_LINK | BATTLE_TYPE_RECORDED)))
        {
            CreateNPCTrainerParty(&gEnemyParty[0], gTrainerBattleOpponent_A, TRUE);
            if (gBattleTypeFlags & BATTLE_TYPE_TWO_OPPONENTS && !BATTLE_TWO_VS_ONE_OPPONENT)
                CreateNPCTrainerParty(&gEnemyParty[PARTY_SIZE / 2], gTrainerBattleOpponent_B, FALSE);
            SetWildMonHeldItem();
            CalculateEnemyPartyCount();
        }
''',
'''        if (!(gBattleTypeFlags & (BATTLE_TYPE_LINK | BATTLE_TYPE_RECORDED)))
        {
            if (!(gBattleTypeFlags & BATTLE_TYPE_ROGUE))
            {
                CreateNPCTrainerParty(&gEnemyParty[0], gTrainerBattleOpponent_A, TRUE);
                if (gBattleTypeFlags & BATTLE_TYPE_TWO_OPPONENTS && !BATTLE_TWO_VS_ONE_OPPONENT)
                    CreateNPCTrainerParty(&gEnemyParty[PARTY_SIZE / 2], gTrainerBattleOpponent_B, FALSE);
                SetWildMonHeldItem();
            }
            CalculateEnemyPartyCount();
        }
''', 'preserve rogue enemy party')

# Capture pipeline: do not enter Story Pokedex/nickname screens. Keep Emerald's
# normal GiveMonToPlayer path, but block ball use when the Rogue party is full so
# it can never fall back to Story PC storage.
p = repo/'data/battle_scripts_2.s'
replace_once(p,
'''BattleScript_TryPrintCaughtMonInfo:
	jumpifbattletype BATTLE_TYPE_RECORDED, BattleScript_GiveCaughtMonEnd
	trysetcaughtmondexflags BattleScript_TryNicknameCaughtMon
''',
'''BattleScript_TryPrintCaughtMonInfo:
	jumpifbattletype BATTLE_TYPE_RECORDED, BattleScript_GiveCaughtMonEnd
	jumpifbattletype BATTLE_TYPE_ROGUE, BattleScript_GiveCaughtMonEnd
	trysetcaughtmondexflags BattleScript_TryNicknameCaughtMon
''', 'rogue capture skips story dex and nickname')

# Ball use in Rogue mode uses Rogue party capacity only; never query Story PC and
# never inherit Story's temporary no-catching field flag.
p = repo/'src/item_use.c'
replace_once(p,
'#include "constants/map_types.h"\n',
'#include "constants/map_types.h"\n#include "constants/battle.h"\n', 'item use battle constants include')
replace_once(p,
'''    else if (IsPlayerPartyAndPokemonStorageFull() == TRUE)
        return BALL_THROW_UNABLE_NO_ROOM;
''',
'''    else if ((gBattleTypeFlags & BATTLE_TYPE_ROGUE)
          ? (CalculatePlayerPartyCount() >= PARTY_SIZE)
          : (IsPlayerPartyAndPokemonStorageFull() == TRUE))
        return BALL_THROW_UNABLE_NO_ROOM;
''', 'rogue party-only capture capacity')
replace_once(p,
'''    else if (FlagGet(B_FLAG_NO_CATCHING))
        return BALL_THROW_UNABLE_DISABLED_FLAG;
''',
'''    else if (!(gBattleTypeFlags & BATTLE_TYPE_ROGUE) && FlagGet(B_FLAG_NO_CATCHING))
        return BALL_THROW_UNABLE_DISABLED_FLAG;
''', 'ignore story no-catching flag in rogue battle')

print("Imperium Rogue v0.1.4-clean source patch applied.")