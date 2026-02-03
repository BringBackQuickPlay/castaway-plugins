/*
    ============================================================
    Improved Autoscramble
    ============================================================

    Replaces TF2 builtin autoscramble with predictable logic.

    Arena vs Non-Arena handling is explicit to support future
    Administrator VOX logic (e.g. Flawless Victory / Defeat).
*/

#include <sourcemod>
#include <sdktools>
#include <tf2>
#include <tf2_stocks>
#include <dhooks>
#include <scramble>

#define PLUGIN_NAME    "Improved Autoscramble"
#define PLUGIN_VERSION "0.0.1"

public Plugin myinfo =
{
    name        = PLUGIN_NAME,
    author      = "AUTHOR",
    description = "Improved and predictable autoscramble replacement",
    version     = PLUGIN_VERSION,
    url         = "https://castaway.tf"
};

// ============================================================
// Globals (state shared with other plugins)
// ============================================================

bool  g_bAutoscrambleInProgress;
float g_fBlockVoteScrambleTime;

// Arena-specific VOX gating (future use)
bool g_bArenaAllowPreVox;
bool g_bIsArena;

// Valve-style win tracking (mode 1)
int g_GameTeamWins[2]; // [0] = RED, [1] = BLU

// ============================================================
// ConVars (custom)
// ============================================================

ConVar cvar_improved_autoscramble;
ConVar cvar_improved_autoscramble_mode;
ConVar cvar_improved_autoscramble_amount;
ConVar cvar_improved_autoscramble_enable_administrator_vox;

// ============================================================
// ConVars (engine / TF2 references)
// ============================================================

ConVar cvar_ref_mp_scrambleteams_auto;
ConVar cvar_ref_mp_tournament;
ConVar cvar_ref_tf_gamemode_mvm;
ConVar cvar_ref_nextlevel;
ConVar cvar_ref_mp_winlimit;
ConVar cvar_ref_mp_maxrounds;

// ============================================================
// Utility helpers
// ============================================================

int TeamToIndex(TFTeam team)
{
    if (team == TFTeam_Red)  return 0;
    if (team == TFTeam_Blue) return 1;
    return -1;
}

bool IsNearMapEnd()
{
    int timeLeft;
    GetMapTimeLeft(timeLeft);
    return (timeLeft >= 0 && timeLeft <= 300);
}

bool IsNextLevelEmpty()
{
    char buf[PLATFORM_MAX_PATH];
    cvar_ref_nextlevel.GetString(buf, sizeof(buf));
    return (buf[0] == '\0');
}

/*
    Positive predicate:
    "Is autoscramble allowed under current conditions?"
*/
bool AutoscrambleIsAllowed(bool arena)
{
    if (!cvar_improved_autoscramble.BoolValue)
        return false;

    if (cvar_ref_mp_tournament.BoolValue)
        return false;

    if (cvar_ref_tf_gamemode_mvm.BoolValue)
        return false;

    // Valve avoids scrambling near map end
    if (IsNearMapEnd())
        return false;

    // Valve behavior: non-arena maps respect queued nextlevel
    if (!arena)
    {
        if (!IsNextLevelEmpty())
            return false;
    }

    return true;
}

// ============================================================
// Arena detection
// ============================================================

public void OnMapStart()
{
    g_bIsArena = false;

    int ent = -1;
    while ((ent = FindEntityByClassname(ent, "tf_logic_arena")) != -1)
    {
        g_bIsArena = true;
        break;
    }
}

// ============================================================
// Autoscramble logic (mode 1 only for now)
// ============================================================

bool ShouldAutoscramble_Delta(TFTeam team)
{
    if (team != TFTeam_Red && team != TFTeam_Blue)
        return false;

    int winlimit  = cvar_ref_mp_winlimit.IntValue;
    int maxrounds = cvar_ref_mp_maxrounds.IntValue;
    int rounds    = GameRules_GetProp("m_nRoundsPlayed");

    if (maxrounds > 0 && (maxrounds - rounds) == 1)
        return false;

    if (winlimit > 0)
    {
        int red = GetTeamScore(TFTeam_Red);
        int blu = GetTeamScore(TFTeam_Blue);

        if ((winlimit - red) == 1 || (winlimit - blu) == 1)
            return false;
    }

    int idx = TeamToIndex(team);
    g_GameTeamWins[idx]++;

    int delta = abs(g_GameTeamWins[0] - g_GameTeamWins[1]);
    bool trigger = (delta >= cvar_improved_autoscramble_amount.IntValue);

    if (GameRules_GetProp("m_bSwitchTeams"))
    {
        int tmp = g_GameTeamWins[0];
        g_GameTeamWins[0] = g_GameTeamWins[1];
        g_GameTeamWins[1] = tmp;
    }

    return trigger;
}

bool ShouldAutoscramble(TFTeam team, bool arena)
{
    if (!AutoscrambleIsAllowed(arena))
        return false;

    switch (cvar_improved_autoscramble_mode.IntValue)
    {
        case 1: return ShouldAutoscramble_Delta(team);
        case 2: return false; // exact streak mode later
    }

    return false;
}

// ============================================================
// Natives
// ============================================================

public any Native_Autoscramble_IsBusy(Handle plugin, int numParams)
{
    return g_bAutoscrambleInProgress;
}

public any Native_Autoscramble_SetBusy(Handle plugin, int numParams)
{
    g_bAutoscrambleInProgress = GetNativeCell(1) != 0;
    return 0;
}

public any Native_Autoscramble_GetBlockVoteTime(Handle plugin, int numParams)
{
    return view_as<any>(g_fBlockVoteScrambleTime);
}

public any Native_Autoscramble_SetBlockVoteTime(Handle plugin, int numParams)
{
    g_fBlockVoteScrambleTime = GetNativeCell(1);
    return 0;
}

// ============================================================
// Plugin init
// ============================================================

DynamicHook g_hSetWinningTeam;

public void OnPluginStart()
{
    // Custom cvars
    cvar_improved_autoscramble =
        CreateConVar("sm_improved_autoscramble", "0",
        "Enable improved autoscramble", FCVAR_NOTIFY, true, 0.0, true, 1.0);

    cvar_improved_autoscramble_mode =
        CreateConVar("sm_improved_autoscramble_mode", "1",
        "1 = Valve delta, 2 = exact streak", FCVAR_NOTIFY, true, 1.0, true, 2.0);

    cvar_improved_autoscramble_amount =
        CreateConVar("sm_improved_autoscramble_amount", "2",
        "Win delta / streak requirement", FCVAR_NOTIFY, true, 1.0, true, 100.0);

    cvar_improved_autoscramble_enable_administrator_vox =
        CreateConVar("sm_improved_autoscramble_enable_administrator_vox", "0",
        "Enable Administrator VOX", FCVAR_NOTIFY, true, 0.0, true, 1.0);

    // Engine cvar refs
    cvar_ref_mp_scrambleteams_auto = FindConVar("mp_scrambleteams_auto");
    cvar_ref_mp_tournament         = FindConVar("mp_tournament");
    cvar_ref_tf_gamemode_mvm        = FindConVar("tf_gamemode_mvm");
    cvar_ref_nextlevel              = FindConVar("nextlevel");
    cvar_ref_mp_winlimit            = FindConVar("mp_winlimit");
    cvar_ref_mp_maxrounds           = FindConVar("mp_maxrounds");

    cvar_improved_autoscramble.AddChangeHook(OnImprovedAutoscrambleChanged);

    RegPluginLibrary("improved_autoscramble");
    CreateNative("Autoscramble_IsBusy", Native_Autoscramble_IsBusy);
    CreateNative("Autoscramble_SetBusy", Native_Autoscramble_SetBusy);
    CreateNative("Autoscramble_GetBlockVoteTime", Native_Autoscramble_GetBlockVoteTime);
    CreateNative("Autoscramble_SetBlockVoteTime", Native_Autoscramble_SetBlockVoteTime);

    GameData gd = new GameData("improved_autoscramble");
    if (gd == null)
        SetFailState("Missing gamedata");

    g_hSetWinningTeam = DynamicHook.FromConf(gd, "CTFGameRules::SetWinningTeam");
    if (g_hSetWinningTeam == null)
        SetFailState("Failed to hook SetWinningTeam");

    g_hSetWinningTeam.HookGamerules(Hook_Post, OnSetWinningTeam_Post);
    delete gd;
}

public void OnImprovedAutoscrambleChanged(ConVar cvar, const char[] oldVal, const char[] newVal)
{
    if (StringToInt(newVal) == 1 && cvar_ref_mp_scrambleteams_auto.BoolValue)
        cvar_ref_mp_scrambleteams_auto.SetBool(false);
}

// ============================================================
// GameRules hook (decision + VOX gating)
// ============================================================

MRESReturn OnSetWinningTeam_Post(DHookReturn ret, DHookParam params)
{
    TFTeam team = view_as<TFTeam>(params.Get(1));
    bool reset = params.Get(3);

    if (!reset)
        return MRES_Ignored;

    if (!ShouldAutoscramble(team, g_bIsArena))
        return MRES_Ignored;

    g_bAutoscrambleInProgress = true;

    if (cvar_improved_autoscramble_enable_administrator_vox.BoolValue)
    {
        if (!g_bIsArena)
        {
            // Non-Arena:
            // PlayAdministratorVoxPre();
        }
        else
        {
            g_bArenaAllowPreVox = true;

            // Later:
            // if (WasArenaFlawlessVictory_Server(team))
            //     g_bArenaAllowPreVox = false;
        }
    }

    return MRES_Ignored;
}

// ============================================================
// Round start: perform scramble
// ============================================================

public void OnPreRoundStart(Event event, const char[] name, bool dontBroadcast)
{
    if (!g_bAutoscrambleInProgress)
        return;

    ScrambleTeams();

    // Future:
    // if (cvar_improved_autoscramble_enable_administrator_vox.BoolValue)
    //     PlayAdministratorVoxPost();

    g_bAutoscrambleInProgress = false;
    g_bArenaAllowPreVox = false;
}
