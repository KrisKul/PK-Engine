#include "global.h"
#include "rogue_mode.h"
#include "battle.h"
#include "bg.h"
#include "gpu_regs.h"
#include "item.h"
#include "main.h"
#include "malloc.h"
#include "menu.h"
#include "palette.h"
#include "pokemon.h"
#include "random.h"
#include "save.h"
#include "sound.h"
#include "sprite.h"
#include "string_util.h"
#include "task.h"
#include "text.h"
#include "text_window.h"
#include "util.h"
#include "window.h"
#include "constants/battle.h"
#include "constants/items.h"
#include "constants/pokemon.h"
#include "constants/rgb.h"
#include "constants/songs.h"
#include "constants/species.h"

#define ROGUE_SAVE_MAGIC 0x31475249 // "IRG1"
#define ROGUE_SAVE_VERSION 3
#define ROGUE_SAVE_SECTOR SECTOR_ID_TRAINER_HILL
#define ROGUE_CLASSIC_WAVES 20
#define ROGUE_STARTER_CHOICES 3
#define ROGUE_REWARD_CHOICES 3
#define ROGUE_STARTER_POOL_COUNT 27

#define ROGUE_REWARD_POKE_BALLS 0
#define ROGUE_REWARD_POTIONS 1
#define ROGUE_REWARD_REVIVE 2
#define ROGUE_REWARD_SUPER_POTION 3
#define ROGUE_REWARD_GREAT_BALLS 4
#define ROGUE_REWARD_FULL_HEAL 5
#define ROGUE_REWARD_ULTRA_BALLS 6
#define ROGUE_REWARD_MAX_POTION 7
#define ROGUE_REWARD_COUNT 8

enum
{
    ROGUE_RUN_STATE_NONE,
    ROGUE_RUN_STATE_STARTER,
    ROGUE_RUN_STATE_BATTLE,
    ROGUE_RUN_STATE_REWARD,
};

struct RogueInventory
{
    u16 pokeBalls;
    u16 greatBalls;
    u16 ultraBalls;
    u16 potions;
    u16 superPotions;
    u16 maxPotions;
    u16 fullHeals;
    u16 revives;
};

struct RogueRunSave
{
    bool8 active;
    bool8 victory;
    bool8 countedRunStart;
    u8 state;
    u8 wave;
    u8 biome;
    u16 encounterSpecies;
    u8 encounterLevel;
    bool8 encounterPrepared;
    u16 padding;
    u32 encounterPersonality;
    u32 seed;
    u32 rng;
    u32 money;
    u8 partyCount;
    u8 starterChoiceCount;
    u16 starterChoices[ROGUE_STARTER_CHOICES];
    u8 rewardChoices[ROGUE_REWARD_CHOICES];
    u8 padding2;
    struct RogueInventory inventory;
    struct Pokemon party[PARTY_SIZE];
};

struct RogueProfileSave
{
    u32 runsStarted;
    u32 runsWon;
    u32 lifetimeWaves;
    u16 bestWave;
    u8 starterUnlocked[ROGUE_STARTER_POOL_COUNT];
    u8 starterCandies[ROGUE_STARTER_POOL_COUNT];
};

struct RogueSaveData
{
    u32 magic;
    u16 version;
    u16 size;
    struct RogueProfileSave profile;
    struct RogueRunSave run;
    u32 checksum;
};

STATIC_ASSERT(sizeof(struct RogueSaveData) < SECTOR_COUNTER_OFFSET, RogueSaveFitsSpecialSector);

static EWRAM_DATA struct RogueSaveData sRogueSave = {0};
static EWRAM_DATA u8 sRogueMenuCursor = 0;
static EWRAM_DATA u8 sRogueStarterCursor = 0;
static EWRAM_DATA u8 sRogueRewardCursor = 0;
static EWRAM_DATA bool8 sRogueLoaded = FALSE;

static const u16 sStarterPool[ROGUE_STARTER_POOL_COUNT] =
{
    SPECIES_BULBASAUR, SPECIES_CHARMANDER, SPECIES_SQUIRTLE,
    SPECIES_CHIKORITA, SPECIES_CYNDAQUIL, SPECIES_TOTODILE,
    SPECIES_TREECKO, SPECIES_TORCHIC, SPECIES_MUDKIP,
    SPECIES_TURTWIG, SPECIES_CHIMCHAR, SPECIES_PIPLUP,
    SPECIES_SNIVY, SPECIES_TEPIG, SPECIES_OSHAWOTT,
    SPECIES_CHESPIN, SPECIES_FENNEKIN, SPECIES_FROAKIE,
    SPECIES_ROWLET, SPECIES_LITTEN, SPECIES_POPPLIO,
    SPECIES_GROOKEY, SPECIES_SCORBUNNY, SPECIES_SOBBLE,
    SPECIES_SPRIGATITO, SPECIES_FUECOCO, SPECIES_QUAXLY,
};

static const u16 sEncounterPool[] =
{
    SPECIES_PIDGEY, SPECIES_RATTATA, SPECIES_CATERPIE, SPECIES_WEEDLE,
    SPECIES_ZUBAT, SPECIES_GEODUDE, SPECIES_MAREEP, SPECIES_HOUNDOUR,
    SPECIES_RALTS, SPECIES_SHROOMISH, SPECIES_ARON, SPECIES_TRAPINCH,
    SPECIES_STARLY, SPECIES_SHINX, SPECIES_RIOLU, SPECIES_ROGGENROLA,
    SPECIES_DRILBUR, SPECIES_LITWICK, SPECIES_FLETCHLING, SPECIES_HONEDGE,
    SPECIES_ROCKRUFF, SPECIES_MIMIKYU, SPECIES_ROOKIDEE, SPECIES_TINKATINK,
};

static const u16 sBossPool[] =
{
    SPECIES_GYARADOS,
    SPECIES_SNORLAX,
    SPECIES_GARCHOMP,
    SPECIES_VOLCARONA,
    SPECIES_KOMMO_O,
    SPECIES_DRAGAPULT,
    SPECIES_KINGAMBIT,
    SPECIES_BAXCALIBUR,
};

static const struct BgTemplate sRogueBgTemplates[] =
{
    {
        .bg = 0,
        .charBaseIndex = 0,
        .mapBaseIndex = 31,
        .screenSize = 0,
        .paletteMode = 0,
        .priority = 0,
        .baseTile = 0,
    },
};

static const struct WindowTemplate sRogueWindows[] =
{
    {
        .bg = 0,
        .tilemapLeft = 1,
        .tilemapTop = 1,
        .width = 28,
        .height = 5,
        .paletteNum = 15,
        .baseBlock = 11,
    },
    {
        .bg = 0,
        .tilemapLeft = 3,
        .tilemapTop = 7,
        .width = 24,
        .height = 12,
        .paletteNum = 15,
        .baseBlock = 151,
    },
    DUMMY_WIN_TEMPLATE,
};

static const u8 sTextRogueTitle[] = _("IMPERIUM ROGUE");
static const u8 sTextRoguePlayerName[] = _("ROGUE");
static const u8 sTextNewRun[] = _("NEW RUN");
static const u8 sTextRunActive[] = _("RUN ACTIVE");
static const u8 sTextContinueRun[] = _("CONTINUE RUN");
static const u8 sTextRecords[] = _("RECORDS");
static const u8 sTextBack[] = _("BACK");
static const u8 sTextChooseStarter[] = _("CHOOSE A STARTER");
static const u8 sTextChooseReward[] = _("CHOOSE A REWARD");
static const u8 sTextRunLost[] = _("RUN LOST");
static const u8 sTextRunWon[] = _("CLASSIC CLEARED!");
static const u8 sTextWave[] = _("WAVE ");
static const u8 sTextBest[] = _("BEST ");
static const u8 sTextRuns[] = _("RUNS ");
static const u8 sTextWins[] = _("WINS ");
static const u8 sTextPressB[] = _("PRESS B TO RETURN");
static const u8 sTextRewardPokeBalls[] = _("POKE BALL x3");
static const u8 sTextRewardPotions[] = _("POTION x2");
static const u8 sTextRewardRevive[] = _("REVIVE");
static const u8 sTextRewardSuperPotion[] = _("SUPER POTION");
static const u8 sTextRewardGreatBalls[] = _("GREAT BALL x2");
static const u8 sTextRewardFullHeal[] = _("FULL HEAL");
static const u8 sTextRewardUltraBalls[] = _("ULTRA BALL x2");
static const u8 sTextRewardMaxPotion[] = _("MAX POTION");

static void Rogue_InitScreen(void);
static void Rogue_EnsurePlayerIdentity(void);
static void Rogue_MainCB(void);
static void Rogue_BattleCallback1(void);
static void Rogue_VBlankCB(void);
static void Task_RogueMainMenu(u8 taskId);
static void Task_RogueStarterMenu(u8 taskId);
static void Task_RogueRewardMenu(u8 taskId);
static void Task_RogueRecords(u8 taskId);
static void Rogue_DrawMainMenu(void);
static void Rogue_DrawStarterMenu(void);
static void Rogue_DrawRewardMenu(void);
static void Rogue_DrawRecords(void);
static void Rogue_StartNewRun(void);
static void Rogue_ResumeRun(void);
static void Rogue_StartNextBattle(void);
static void Rogue_EndRun(bool8 victory);
static void Rogue_ReturnToTitle(void);
static void Rogue_LoadSave(void);
static bool32 Rogue_WriteSave(void);
static void Rogue_ResetSave(void);
static void Rogue_SnapshotPartyAndInventory(void);
static void Rogue_RestorePartyAndInventory(void);
static void Rogue_ClearWindows(void);
static u32 Rogue_NextRandom(void);
static u16 Rogue_SelectEncounterSpecies(void);
static void Rogue_PrepareEncounter(void);
static u8 Rogue_GetEnemyLevel(void);
static void Rogue_RollStarterChoices(void);
static void Rogue_RollRewards(void);
static void Rogue_ApplyReward(u8 reward);
static const u8 *Rogue_GetRewardName(u8 reward);
static void Rogue_PrintNumber(u8 windowId, const u8 *prefix, u32 value, u8 x, u8 y);

static u32 Rogue_CalcChecksum(void)
{
    return CalcByteArraySum((const u8 *)&sRogueSave, offsetof(struct RogueSaveData, checksum));
}

static void Rogue_EnsurePlayerIdentity(void)
{
    static const u8 sRogueTrainerId[4] = {'R', 'O', 'G', 'U'};

    if (gSaveFileStatus == SAVE_STATUS_OK)
        return;

    // A no-save Rogue run still needs valid ownership text/ID for starters and
    // catches. These values live only in RAM; Rogue Mode never writes Story save.
    StringCopy(gSaveBlock2Ptr->playerName, sTextRoguePlayerName);
    gSaveBlock2Ptr->playerGender = MALE;
    memcpy(gSaveBlock2Ptr->playerTrainerId, sRogueTrainerId, sizeof(sRogueTrainerId));
}

static void Rogue_ResetSave(void)
{
    memset(&sRogueSave, 0, sizeof(sRogueSave));
    sRogueSave.magic = ROGUE_SAVE_MAGIC;
    sRogueSave.version = ROGUE_SAVE_VERSION;
    sRogueSave.size = sizeof(sRogueSave);
    memset(sRogueSave.profile.starterUnlocked, TRUE, sizeof(sRogueSave.profile.starterUnlocked));
}

static void Rogue_LoadSave(void)
{
    u8 *sectorBuffer;

    if (sRogueLoaded)
        return;

    sectorBuffer = AllocZeroed(SECTOR_COUNTER_OFFSET);
    if (sectorBuffer == NULL)
    {
        Rogue_ResetSave();
        sRogueLoaded = TRUE;
        return;
    }

    if (TryReadSpecialSaveSector(ROGUE_SAVE_SECTOR, sectorBuffer) != SAVE_STATUS_OK)
    {
        Free(sectorBuffer);
        Rogue_ResetSave();
        sRogueLoaded = TRUE;
        return;
    }

    memcpy(&sRogueSave, sectorBuffer, sizeof(sRogueSave));
    Free(sectorBuffer);

    if (sRogueSave.magic != ROGUE_SAVE_MAGIC
     || sRogueSave.version != ROGUE_SAVE_VERSION
     || sRogueSave.size != sizeof(sRogueSave)
     || sRogueSave.checksum != Rogue_CalcChecksum())
    {
        Rogue_ResetSave();
    }
    sRogueLoaded = TRUE;
}

static bool32 Rogue_WriteSave(void)
{
    u32 result;
    u8 *sectorBuffer = AllocZeroed(SECTOR_COUNTER_OFFSET);

    if (sectorBuffer == NULL)
        return FALSE;

    sRogueSave.magic = ROGUE_SAVE_MAGIC;
    sRogueSave.version = ROGUE_SAVE_VERSION;
    sRogueSave.size = sizeof(sRogueSave);
    sRogueSave.checksum = Rogue_CalcChecksum();
    memcpy(sectorBuffer, &sRogueSave, sizeof(sRogueSave));
    result = TryWriteSpecialSaveSector(ROGUE_SAVE_SECTOR, sectorBuffer);
    Free(sectorBuffer);
    return result == SAVE_STATUS_OK;
}

bool32 RogueMode_HasActiveRun(void)
{
    Rogue_LoadSave();
    return sRogueSave.run.active;
}

static u32 Rogue_NextRandom(void)
{
    u32 x = sRogueSave.run.rng;
    if (x == 0)
        x = 0x6D2B79F5;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    sRogueSave.run.rng = x;
    return x;
}

static void Rogue_RollStarterChoices(void)
{
    u32 i;
    for (i = 0; i < ROGUE_STARTER_CHOICES; i++)
    {
        u16 species;
        bool8 duplicate;
        do
        {
            u32 j;
            duplicate = FALSE;
            species = sStarterPool[Rogue_NextRandom() % ARRAY_COUNT(sStarterPool)];
            for (j = 0; j < i; j++)
            {
                if (sRogueSave.run.starterChoices[j] == species)
                    duplicate = TRUE;
            }
        } while (duplicate);
        sRogueSave.run.starterChoices[i] = species;
    }
    sRogueSave.run.starterChoiceCount = ROGUE_STARTER_CHOICES;
}

static void Rogue_StartNewRun(void)
{
    memset(&sRogueSave.run, 0, sizeof(sRogueSave.run));
    sRogueSave.run.active = TRUE;
    sRogueSave.run.state = ROGUE_RUN_STATE_STARTER;
    sRogueSave.run.wave = 1;
    sRogueSave.run.seed = Random32();
    if (sRogueSave.run.seed == 0)
        sRogueSave.run.seed = 1;
    sRogueSave.run.rng = sRogueSave.run.seed;
    sRogueSave.run.money = 1000;
    sRogueSave.run.inventory.pokeBalls = 5;
    sRogueSave.run.inventory.potions = 2;
    Rogue_RollStarterChoices();
    Rogue_WriteSave();
    sRogueStarterCursor = 0;
    Rogue_DrawStarterMenu();
}

static void Rogue_SnapshotPartyAndInventory(void)
{
    u32 i;
    sRogueSave.run.partyCount = gPlayerPartyCount;
    for (i = 0; i < PARTY_SIZE; i++)
        sRogueSave.run.party[i] = gPlayerParty[i];

    sRogueSave.run.inventory.pokeBalls = CountTotalItemQuantityInBag(ITEM_POKE_BALL);
    sRogueSave.run.inventory.greatBalls = CountTotalItemQuantityInBag(ITEM_GREAT_BALL);
    sRogueSave.run.inventory.ultraBalls = CountTotalItemQuantityInBag(ITEM_ULTRA_BALL);
    sRogueSave.run.inventory.potions = CountTotalItemQuantityInBag(ITEM_POTION);
    sRogueSave.run.inventory.superPotions = CountTotalItemQuantityInBag(ITEM_SUPER_POTION);
    sRogueSave.run.inventory.maxPotions = CountTotalItemQuantityInBag(ITEM_MAX_POTION);
    sRogueSave.run.inventory.fullHeals = CountTotalItemQuantityInBag(ITEM_FULL_HEAL);
    sRogueSave.run.inventory.revives = CountTotalItemQuantityInBag(ITEM_REVIVE);
}

static void Rogue_RestorePartyAndInventory(void)
{
    u32 i;
    ZeroPlayerPartyMons();
    for (i = 0; i < PARTY_SIZE; i++)
        gPlayerParty[i] = sRogueSave.run.party[i];
    gPlayerPartyCount = min(sRogueSave.run.partyCount, PARTY_SIZE);

    ClearBag();
    if (sRogueSave.run.inventory.pokeBalls)
        AddBagItem(ITEM_POKE_BALL, sRogueSave.run.inventory.pokeBalls);
    if (sRogueSave.run.inventory.greatBalls)
        AddBagItem(ITEM_GREAT_BALL, sRogueSave.run.inventory.greatBalls);
    if (sRogueSave.run.inventory.ultraBalls)
        AddBagItem(ITEM_ULTRA_BALL, sRogueSave.run.inventory.ultraBalls);
    if (sRogueSave.run.inventory.potions)
        AddBagItem(ITEM_POTION, sRogueSave.run.inventory.potions);
    if (sRogueSave.run.inventory.superPotions)
        AddBagItem(ITEM_SUPER_POTION, sRogueSave.run.inventory.superPotions);
    if (sRogueSave.run.inventory.maxPotions)
        AddBagItem(ITEM_MAX_POTION, sRogueSave.run.inventory.maxPotions);
    if (sRogueSave.run.inventory.fullHeals)
        AddBagItem(ITEM_FULL_HEAL, sRogueSave.run.inventory.fullHeals);
    if (sRogueSave.run.inventory.revives)
        AddBagItem(ITEM_REVIVE, sRogueSave.run.inventory.revives);
}

static void Rogue_ResumeRun(void)
{
    switch (sRogueSave.run.state)
    {
    case ROGUE_RUN_STATE_STARTER:
        sRogueStarterCursor = 0;
        Rogue_DrawStarterMenu();
        break;
    case ROGUE_RUN_STATE_REWARD:
        Rogue_RestorePartyAndInventory();
        sRogueRewardCursor = 0;
        Rogue_DrawRewardMenu();
        break;
    case ROGUE_RUN_STATE_BATTLE:
    default:
        Rogue_RestorePartyAndInventory();
        Rogue_StartNextBattle();
        break;
    }
}

static u8 Rogue_GetEnemyLevel(void)
{
    u32 level = 5 + (sRogueSave.run.wave * 3);
    if (level > MAX_LEVEL)
        level = MAX_LEVEL;
    return level;
}

static u16 Rogue_SelectEncounterSpecies(void)
{
    if ((sRogueSave.run.wave % 10) == 0)
        return sBossPool[Rogue_NextRandom() % ARRAY_COUNT(sBossPool)];
    return sEncounterPool[Rogue_NextRandom() % ARRAY_COUNT(sEncounterPool)];
}

static void Rogue_PrepareEncounter(void)
{
    if (sRogueSave.run.encounterPrepared)
        return;

    sRogueSave.run.encounterSpecies = Rogue_SelectEncounterSpecies();
    sRogueSave.run.encounterLevel = Rogue_GetEnemyLevel();
    sRogueSave.run.encounterPersonality = Rogue_NextRandom();
    sRogueSave.run.encounterPrepared = TRUE;
}

static void Rogue_StartNextBattle(void)
{
    Rogue_PrepareEncounter();

    ZeroEnemyPartyMons();
    CreateMon(&gEnemyParty[0],
              sRogueSave.run.encounterSpecies,
              sRogueSave.run.encounterLevel,
              20,
              TRUE,
              sRogueSave.run.encounterPersonality,
              OT_ID_RANDOM_NO_SHINY,
              0);
    gEnemyPartyCount = 1;

    sRogueSave.run.state = ROGUE_RUN_STATE_BATTLE;
    Rogue_SnapshotPartyAndInventory();
    Rogue_WriteSave();

    Rogue_ClearWindows();
    FreeAllWindowBuffers();
    SetVBlankCallback(NULL);
    // Rogue battles do not originate from the overworld. Use a valid no-op
    // callback so battle init/teardown can save and restore callback1 safely
    // without ever executing Story/field code.
    gMain.callback1 = Rogue_BattleCallback1;
    gBattleTypeFlags = BATTLE_TYPE_ROGUE;
    gMain.savedCallback = CB2_RogueBattleEnd;
    SetMainCallback2(CB2_InitBattle);
}

void CB2_RogueBattleEnd(void)
{
    // Any non-successful battle outcome ends the run. Most importantly, do not
    // touch the normal Story save or snapshot the fainted party here.
    if (gBattleOutcome != B_OUTCOME_WON && gBattleOutcome != B_OUTCOME_CAUGHT)
    {
        Rogue_EndRun(FALSE);
        return;
    }

    Rogue_SnapshotPartyAndInventory();
    sRogueSave.profile.lifetimeWaves++;
    if (sRogueSave.run.wave > sRogueSave.profile.bestWave)
        sRogueSave.profile.bestWave = sRogueSave.run.wave;

    if (sRogueSave.run.wave >= ROGUE_CLASSIC_WAVES)
    {
        Rogue_EndRun(TRUE);
        return;
    }

    sRogueSave.run.wave++;
    sRogueSave.run.state = ROGUE_RUN_STATE_REWARD;
    sRogueSave.run.encounterPrepared = FALSE;
    sRogueSave.run.encounterSpecies = SPECIES_NONE;
    sRogueSave.run.encounterLevel = 0;
    sRogueSave.run.encounterPersonality = 0;
    Rogue_RollRewards();
    Rogue_WriteSave();
    sRogueRewardCursor = 0;
    SetMainCallback2(CB2_InitRogueMode);
}

static void Rogue_EndRun(bool8 victory)
{
    // This function is deliberately minimal. The v0.1.1 crash occurred here
    // because it attempted to LoadGameSave(SAVE_NORMAL) while battle cleanup
    // was still active. Story Mode was never written by Rogue Mode, so a clean
    // soft reset is enough to restore it safely on the next normal boot.
    sRogueSave.run.active = FALSE;
    sRogueSave.run.state = ROGUE_RUN_STATE_NONE;
    sRogueSave.run.victory = victory;
    if (victory)
        sRogueSave.profile.runsWon++;
    Rogue_WriteSave();

    SetVBlankCallback(NULL);
    DoSoftReset();
}

static void Rogue_RollRewards(void)
{
    u32 i;
    for (i = 0; i < ROGUE_REWARD_CHOICES; i++)
        sRogueSave.run.rewardChoices[i] = Rogue_NextRandom() % ROGUE_REWARD_COUNT;
}

static const u8 *Rogue_GetRewardName(u8 reward)
{
    switch (reward)
    {
    case ROGUE_REWARD_POKE_BALLS: return sTextRewardPokeBalls;
    case ROGUE_REWARD_POTIONS: return sTextRewardPotions;
    case ROGUE_REWARD_REVIVE: return sTextRewardRevive;
    case ROGUE_REWARD_SUPER_POTION: return sTextRewardSuperPotion;
    case ROGUE_REWARD_GREAT_BALLS: return sTextRewardGreatBalls;
    case ROGUE_REWARD_FULL_HEAL: return sTextRewardFullHeal;
    case ROGUE_REWARD_ULTRA_BALLS: return sTextRewardUltraBalls;
    case ROGUE_REWARD_MAX_POTION: return sTextRewardMaxPotion;
    default: return sTextRewardPokeBalls;
    }
}

static void Rogue_ApplyReward(u8 reward)
{
    switch (reward)
    {
    case ROGUE_REWARD_POKE_BALLS: AddBagItem(ITEM_POKE_BALL, 3); break;
    case ROGUE_REWARD_POTIONS: AddBagItem(ITEM_POTION, 2); break;
    case ROGUE_REWARD_REVIVE: AddBagItem(ITEM_REVIVE, 1); break;
    case ROGUE_REWARD_SUPER_POTION: AddBagItem(ITEM_SUPER_POTION, 1); break;
    case ROGUE_REWARD_GREAT_BALLS: AddBagItem(ITEM_GREAT_BALL, 2); break;
    case ROGUE_REWARD_FULL_HEAL: AddBagItem(ITEM_FULL_HEAL, 1); break;
    case ROGUE_REWARD_ULTRA_BALLS: AddBagItem(ITEM_ULTRA_BALL, 2); break;
    case ROGUE_REWARD_MAX_POTION: AddBagItem(ITEM_MAX_POTION, 1); break;
    }
    Rogue_SnapshotPartyAndInventory();
    Rogue_WriteSave();
}

static void Rogue_PrintNumber(u8 windowId, const u8 *prefix, u32 value, u8 x, u8 y)
{
    StringCopy(gStringVar4, prefix);
    ConvertIntToDecimalStringN(gStringVar1, value, STR_CONV_MODE_LEFT_ALIGN, 6);
    StringAppend(gStringVar4, gStringVar1);
    AddTextPrinterParameterized(windowId, FONT_NORMAL, gStringVar4, x, y, TEXT_SKIP_DRAW, NULL);
}

static void Rogue_ClearWindows(void)
{
    FillWindowPixelBuffer(0, PIXEL_FILL(0));
    FillWindowPixelBuffer(1, PIXEL_FILL(0));
}

static void Rogue_DrawMainMenu(void)
{
    static const u8 *const options[] = {sTextNewRun, sTextContinueRun, sTextRecords, sTextBack};
    u32 i;

    Rogue_ClearWindows();
    DrawStdFrameWithCustomTileAndPalette(0, FALSE, 2, 14);
    DrawStdFrameWithCustomTileAndPalette(1, FALSE, 2, 14);
    AddTextPrinterParameterized(0, FONT_NORMAL, sTextRogueTitle, 4, 1, TEXT_SKIP_DRAW, NULL);
    Rogue_PrintNumber(0, sTextWave, sRogueSave.run.active ? sRogueSave.run.wave : 0, 4, 18);
    Rogue_PrintNumber(0, sTextBest, sRogueSave.profile.bestWave, 116, 18);

    for (i = 0; i < ARRAY_COUNT(options); i++)
    {
        u8 y = 8 + i * 16;
        if (i == 1 && !sRogueSave.run.active)
            continue;
        AddTextPrinterParameterized(1, FONT_NORMAL,
                                    (i == 0 && sRogueSave.run.active) ? sTextRunActive : options[i],
                                    20, y, TEXT_SKIP_DRAW, NULL);
        if (i == sRogueMenuCursor)
            AddTextPrinterParameterized(1, FONT_NORMAL, COMPOUND_STRING(">"), 4, y, TEXT_SKIP_DRAW, NULL);
    }

    if (!sRogueSave.run.active && sRogueSave.run.victory)
        AddTextPrinterParameterized(1, FONT_SMALL, sTextRunWon, 20, 80, TEXT_SKIP_DRAW, NULL);
    else if (!sRogueSave.run.active && sRogueSave.profile.runsStarted != 0)
        AddTextPrinterParameterized(1, FONT_SMALL, sTextRunLost, 20, 80, TEXT_SKIP_DRAW, NULL);

    PutWindowTilemap(0);
    PutWindowTilemap(1);
    CopyWindowToVram(0, COPYWIN_FULL);
    CopyWindowToVram(1, COPYWIN_FULL);
}

static void Rogue_DrawStarterMenu(void)
{
    u32 i;
    Rogue_ClearWindows();
    DrawStdFrameWithCustomTileAndPalette(0, FALSE, 2, 14);
    DrawStdFrameWithCustomTileAndPalette(1, FALSE, 2, 14);
    AddTextPrinterParameterized(0, FONT_NORMAL, sTextChooseStarter, 4, 1, TEXT_SKIP_DRAW, NULL);

    for (i = 0; i < ROGUE_STARTER_CHOICES; i++)
    {
        u8 y = 12 + i * 20;
        AddTextPrinterParameterized(1, FONT_NORMAL, GetSpeciesName(sRogueSave.run.starterChoices[i]), 20, y, TEXT_SKIP_DRAW, NULL);
        if (i == sRogueStarterCursor)
            AddTextPrinterParameterized(1, FONT_NORMAL, COMPOUND_STRING(">"), 4, y, TEXT_SKIP_DRAW, NULL);
    }
    PutWindowTilemap(0);
    PutWindowTilemap(1);
    CopyWindowToVram(0, COPYWIN_FULL);
    CopyWindowToVram(1, COPYWIN_FULL);
}

static void Rogue_DrawRewardMenu(void)
{
    u32 i;
    Rogue_ClearWindows();
    DrawStdFrameWithCustomTileAndPalette(0, FALSE, 2, 14);
    DrawStdFrameWithCustomTileAndPalette(1, FALSE, 2, 14);
    AddTextPrinterParameterized(0, FONT_NORMAL, sTextChooseReward, 4, 1, TEXT_SKIP_DRAW, NULL);
    Rogue_PrintNumber(0, sTextWave, sRogueSave.run.wave - 1, 150, 1);

    for (i = 0; i < ROGUE_REWARD_CHOICES; i++)
    {
        u8 y = 12 + i * 20;
        AddTextPrinterParameterized(1, FONT_NORMAL, Rogue_GetRewardName(sRogueSave.run.rewardChoices[i]), 20, y, TEXT_SKIP_DRAW, NULL);
        if (i == sRogueRewardCursor)
            AddTextPrinterParameterized(1, FONT_NORMAL, COMPOUND_STRING(">"), 4, y, TEXT_SKIP_DRAW, NULL);
    }
    PutWindowTilemap(0);
    PutWindowTilemap(1);
    CopyWindowToVram(0, COPYWIN_FULL);
    CopyWindowToVram(1, COPYWIN_FULL);
}

static void Rogue_DrawRecords(void)
{
    Rogue_ClearWindows();
    DrawStdFrameWithCustomTileAndPalette(0, FALSE, 2, 14);
    DrawStdFrameWithCustomTileAndPalette(1, FALSE, 2, 14);
    AddTextPrinterParameterized(0, FONT_NORMAL, sTextRecords, 4, 1, TEXT_SKIP_DRAW, NULL);
    Rogue_PrintNumber(1, sTextRuns, sRogueSave.profile.runsStarted, 12, 12);
    Rogue_PrintNumber(1, sTextWins, sRogueSave.profile.runsWon, 12, 32);
    Rogue_PrintNumber(1, sTextBest, sRogueSave.profile.bestWave, 12, 52);
    AddTextPrinterParameterized(1, FONT_SMALL, sTextPressB, 12, 84, TEXT_SKIP_DRAW, NULL);
    PutWindowTilemap(0);
    PutWindowTilemap(1);
    CopyWindowToVram(0, COPYWIN_FULL);
    CopyWindowToVram(1, COPYWIN_FULL);
}

static void Task_RogueRecords(u8 taskId)
{
    if (JOY_NEW(A_BUTTON | B_BUTTON))
    {
        PlaySE(SE_SELECT);
        gTasks[taskId].func = Task_RogueMainMenu;
        Rogue_DrawMainMenu();
    }
}

static void Task_RogueMainMenu(u8 taskId)
{
    if (JOY_NEW(DPAD_UP))
    {
        do
        {
            if (sRogueMenuCursor == 0)
                sRogueMenuCursor = 3;
            else
                sRogueMenuCursor--;
        } while (sRogueMenuCursor == 1 && !sRogueSave.run.active);
        PlaySE(SE_SELECT);
        Rogue_DrawMainMenu();
    }
    else if (JOY_NEW(DPAD_DOWN))
    {
        do
        {
            sRogueMenuCursor = (sRogueMenuCursor + 1) % 4;
        } while (sRogueMenuCursor == 1 && !sRogueSave.run.active);
        PlaySE(SE_SELECT);
        Rogue_DrawMainMenu();
    }
    else if (JOY_NEW(A_BUTTON))
    {
        PlaySE(SE_SELECT);
        switch (sRogueMenuCursor)
        {
        case 0:
            if (!sRogueSave.run.active)
            {
                gTasks[taskId].func = Task_RogueStarterMenu;
                Rogue_StartNewRun();
            }
            break;
        case 1:
            if (sRogueSave.run.active)
            {
                if (sRogueSave.run.state == ROGUE_RUN_STATE_STARTER)
                    gTasks[taskId].func = Task_RogueStarterMenu;
                else if (sRogueSave.run.state == ROGUE_RUN_STATE_REWARD)
                    gTasks[taskId].func = Task_RogueRewardMenu;
                Rogue_ResumeRun();
                if (sRogueSave.run.state == ROGUE_RUN_STATE_BATTLE)
                    DestroyTask(taskId);
            }
            break;
        case 2:
            gTasks[taskId].func = Task_RogueRecords;
            Rogue_DrawRecords();
            break;
        case 3:
            DestroyTask(taskId);
            Rogue_ReturnToTitle();
            break;
        }
    }
    else if (JOY_NEW(B_BUTTON))
    {
        DestroyTask(taskId);
        Rogue_ReturnToTitle();
    }
}

static void Task_RogueStarterMenu(u8 taskId)
{
    if (JOY_NEW(DPAD_UP))
    {
        sRogueStarterCursor = (sRogueStarterCursor + ROGUE_STARTER_CHOICES - 1) % ROGUE_STARTER_CHOICES;
        PlaySE(SE_SELECT);
        Rogue_DrawStarterMenu();
    }
    else if (JOY_NEW(DPAD_DOWN))
    {
        sRogueStarterCursor = (sRogueStarterCursor + 1) % ROGUE_STARTER_CHOICES;
        PlaySE(SE_SELECT);
        Rogue_DrawStarterMenu();
    }
    else if (JOY_NEW(A_BUTTON))
    {
        u16 species = sRogueSave.run.starterChoices[sRogueStarterCursor];
        u32 poolIndex;
        PlaySE(SE_SELECT);

        ZeroPlayerPartyMons();
        CreateMon(&gPlayerParty[0], species, 5, 31, FALSE, 0, OT_ID_PLAYER_ID, 0);
        gPlayerPartyCount = 1;
        ClearBag();
        AddBagItem(ITEM_POKE_BALL, sRogueSave.run.inventory.pokeBalls);
        AddBagItem(ITEM_POTION, sRogueSave.run.inventory.potions);

        for (poolIndex = 0; poolIndex < ARRAY_COUNT(sStarterPool); poolIndex++)
        {
            if (sStarterPool[poolIndex] == species)
            {
                sRogueSave.profile.starterUnlocked[poolIndex] = TRUE;
                break;
            }
        }

        if (!sRogueSave.run.countedRunStart)
        {
            sRogueSave.profile.runsStarted++;
            sRogueSave.run.countedRunStart = TRUE;
        }

        sRogueSave.run.state = ROGUE_RUN_STATE_BATTLE;
        Rogue_SnapshotPartyAndInventory();
        Rogue_WriteSave();
        DestroyTask(taskId);
        Rogue_StartNextBattle();
    }
    else if (JOY_NEW(B_BUTTON))
    {
        sRogueSave.run.active = FALSE;
        sRogueSave.run.state = ROGUE_RUN_STATE_NONE;
        sRogueSave.run.victory = FALSE;
        Rogue_WriteSave();
        gTasks[taskId].func = Task_RogueMainMenu;
        Rogue_DrawMainMenu();
    }
}

static void Task_RogueRewardMenu(u8 taskId)
{
    if (JOY_NEW(DPAD_UP))
    {
        sRogueRewardCursor = (sRogueRewardCursor + ROGUE_REWARD_CHOICES - 1) % ROGUE_REWARD_CHOICES;
        PlaySE(SE_SELECT);
        Rogue_DrawRewardMenu();
    }
    else if (JOY_NEW(DPAD_DOWN))
    {
        sRogueRewardCursor = (sRogueRewardCursor + 1) % ROGUE_REWARD_CHOICES;
        PlaySE(SE_SELECT);
        Rogue_DrawRewardMenu();
    }
    else if (JOY_NEW(A_BUTTON))
    {
        PlaySE(SE_SELECT);
        Rogue_RestorePartyAndInventory();
        Rogue_ApplyReward(sRogueSave.run.rewardChoices[sRogueRewardCursor]);
        sRogueSave.run.state = ROGUE_RUN_STATE_BATTLE;
        Rogue_WriteSave();
        DestroyTask(taskId);
        Rogue_StartNextBattle();
    }
}

static void Rogue_ReturnToTitle(void)
{
    // Rogue Mode never writes the normal Story save. Resetting is safer than
    // reloading Story structures while custom Rogue tasks/windows are active.
    SetVBlankCallback(NULL);
    DoSoftReset();
}

static void Rogue_BattleCallback1(void)
{
    // Intentionally empty. BattleMainCB1 temporarily replaces this during the
    // battle and restores it before CB2_RogueBattleEnd runs.
}

static void Rogue_MainCB(void)
{
    RunTasks();
    UpdatePaletteFade();
}

static void Rogue_VBlankCB(void)
{
    TransferPlttBuffer();
}

static void Rogue_InitScreen(void)
{
    SetVBlankCallback(NULL);
    SetGpuReg(REG_OFFSET_DISPCNT, DISPCNT_MODE_0);
    SetGpuReg(REG_OFFSET_BG0HOFS, 0);
    SetGpuReg(REG_OFFSET_BG0VOFS, 0);
    SetGpuReg(REG_OFFSET_WIN0H, 0);
    SetGpuReg(REG_OFFSET_WIN0V, 0);
    SetGpuReg(REG_OFFSET_WININ, 0);
    SetGpuReg(REG_OFFSET_WINOUT, 0);
    SetGpuReg(REG_OFFSET_BLDCNT, 0);
    SetGpuReg(REG_OFFSET_BLDALPHA, 0);
    SetGpuReg(REG_OFFSET_BLDY, 0);
    DmaFill16(3, 0, (void *)VRAM, VRAM_SIZE);
    DmaFill32(3, 0, (void *)OAM, OAM_SIZE);
    DmaFill16(3, 0, (void *)(PLTT + 2), PLTT_SIZE - 2);
    ResetPaletteFade();
    ResetTasks();
    ResetSpriteData();
    ResetBgsAndClearDma3BusyFlags(0);
    InitBgsFromTemplates(0, sRogueBgTemplates, ARRAY_COUNT(sRogueBgTemplates));
    SetGpuReg(REG_OFFSET_DISPCNT, DISPCNT_OBJ_ON | DISPCNT_OBJ_1D_MAP);
    ShowBg(0);
    InitWindows(sRogueWindows);
    DeactivateAllTextPrinters();
    FillWindowPixelBuffer(0, PIXEL_FILL(0));
    FillWindowPixelBuffer(1, PIXEL_FILL(0));
    LoadWindowGfx(0, 0, 2, BG_PLTT_ID(14));
    LoadPalette(gStandardMenuPalette, BG_PLTT_ID(15), PLTT_SIZE_4BPP);
    EnableInterrupts(INTR_FLAG_VBLANK);
    SetVBlankCallback(Rogue_VBlankCB);
    SetMainCallback2(Rogue_MainCB);
}

void CB2_InitRogueMode(void)
{
    Rogue_LoadSave();
    Rogue_EnsurePlayerIdentity();
    Rogue_InitScreen();
    Rogue_DrawMainMenu();
    CreateTask(Task_RogueMainMenu, 0);
}