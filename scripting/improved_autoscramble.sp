/*
    ============================================================
    Improved Autoscramble
    ============================================================

    Replaces TF2 builtin autoscramble with predictable logic.
    that handles both Arena and non-arena maps.
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
    description = "Improved Autoscramble with ability to play administrator voicelines",
    version     = PLUGIN_VERSION,
    url         = "https://castaway.tf"
};

// ============================================================
// Globals (state shared with other plugins)
// ============================================================

bool  g_bAutoscrambleInProgress;
bool  g_bAutoscrambleBlockVoting;
// If map is arena, this will get it's initial value from TimeToBlockVoteScramble_Arena config value instead of
// TimeToBlockVoteScramble. Non-Arena max is 1 minute and Arena max is 10 seconds.
float g_fBlockVoteScrambleTime; 

// Arena-specific VOX gating (future use)
//bool g_bArenaAllowPreVox;
bool g_bIsArena;

// Valve-style win tracking (mode 1)
int g_GameTeamWins[2]; // [0] = RED, [1] = BLU


// Hook Handles etc.

DynamicHook g_hSetWinningTeam;

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

bool IsFlawlessVictory(int team)
{
    int totalPlayers = 0;
    int alivePlayers = 0;

    for (int client = 1; client <= MaxClients; client++)
    {
        if (!IsClientInGame(client))
            continue;

        if (GetClientTeam(client) != team)
            continue;

        totalPlayers++;

        if (IsPlayerAlive(client))
            alivePlayers++;
    }

    // Client explicitly rejects 1-player teams
    if (totalPlayers <= 1)
        return false;

    return (alivePlayers == totalPlayers);
}

void CreateAndFireScrambleEvent()
{
    Event event = CreateEvent("teamplay_alert", true);
    if (!event)
        return;

    event.SetInt("alerttype", 0);
    event.Fire();
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

    // Clean up from previous map:
    Autoscramble_SetInProgress(false);
    g_bAutoscrambleBlockVoting = false;
    g_GameTeamWins[0] = 0; // RED
    g_GameTeamWins[1] = 0; // BLU

    // Need to hook Gamerules on Map Start otherwise we get a pointer hook error.
    GameData gd = new GameData("improved_autoscramble");
    if (gd == null)
        SetFailState("Missing gamedata");

    g_hSetWinningTeam = DynamicHook.FromConf(gd, "CTFGameRules::SetWinningTeam");
    if (g_hSetWinningTeam == null)
        SetFailState("Failed to hook SetWinningTeam");

    g_hSetWinningTeam.HookGamerules(Hook_Post, OnSetWinningTeam_Post);
    delete gd;
}

// ============================================================
// Autoscramble logic (mode 1 only for now)
// ============================================================

bool ShouldAutoscramble_Delta(TFTeam team, bool bSwitchTeams)
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
    PrintToChatAll("trigger is")
    if (bSwitchTeams)
    {
        int tmp = g_GameTeamWins[0];
        g_GameTeamWins[0] = g_GameTeamWins[1];
        g_GameTeamWins[1] = tmp;
    }

    PrintToChatAll("g_GameTeamWins[0] is: %d",g_GameTeamWins[0])

    return trigger;
}

bool ShouldAutoscramble(TFTeam team, bool arena, bool bSwitchTeams)
{
    if (!AutoscrambleIsAllowed(arena))
        return false;

    switch (cvar_improved_autoscramble_mode.IntValue)
    {
        case 1: return ShouldAutoscramble_Delta(team, bSwitchTeams);
        case 2: return false; // exact streak mode later
    }

    return false;
}

// ============================================================
// Natives for other plugins (they may only ever read!)
// ============================================================

public any Native_Autoscramble_IsInProgress(Handle plugin, int numParams)
{
    return g_bAutoscrambleInProgress;
}

public any Native_Autoscramble_IsBlockingVote(Handle plugin, int numParams)
{
    return g_bAutoscrambleBlockVoting;
}

//

bool Autoscramble_InProgress()
{
    return g_bAutoscrambleInProgress;
}

void Autoscramble_SetInProgress(bool inProgress)
{
    g_bAutoscrambleInProgress = inProgress;
}

bool Autoscramble_IsBlockingVote()
{
    return g_bAutoscrambleBlockVoting;
}

void Autoscramble_SetBlockVoting(bool block)
{
    g_bAutoscrambleBlockVoting = block;
}

float Autoscramble_GetBlockVoteTime()
{
    return g_fBlockVoteScrambleTime;
}

void Autoscramble_SetBlockVoteTime(float time)
{
    g_fBlockVoteScrambleTime = time;
}


// ============================================================
// Plugin init
// ============================================================

public void OnPluginStart()
{
    // Custom cvars
    cvar_improved_autoscramble =
        CreateConVar("sm_improved_autoscramble", "1",
        "Enable improved autoscramble", FCVAR_NOTIFY, true, 0.0, true, 1.0);

    cvar_improved_autoscramble_mode =
        CreateConVar("sm_improved_autoscramble_mode", "1",
        "1 = Valve delta, 2 = exact streak", FCVAR_NOTIFY, true, 1.0, true, 2.0);

    cvar_improved_autoscramble_amount =
        CreateConVar("sm_improved_autoscramble_amount", "1",
        "Win delta / streak requirement", FCVAR_NOTIFY, true, 1.0, true, 100.0);

    cvar_improved_autoscramble_enable_administrator_vox =
        CreateConVar("sm_improved_autoscramble_enable_administrator_vox", "1",
        "Enable Administrator VOX", FCVAR_NOTIFY, true, 0.0, true, 1.0);

    AutoExecConfig(true, "improved_autoscramble", "sourcemod");

    // Engine cvar refs
    cvar_ref_mp_scrambleteams_auto = FindConVar("mp_scrambleteams_auto");
    cvar_ref_mp_tournament         = FindConVar("mp_tournament");
    cvar_ref_tf_gamemode_mvm        = FindConVar("tf_gamemode_mvm");
    cvar_ref_nextlevel              = FindConVar("nextlevel");
    cvar_ref_mp_winlimit            = FindConVar("mp_winlimit");
    cvar_ref_mp_maxrounds           = FindConVar("mp_maxrounds");



    cvar_improved_autoscramble.AddChangeHook(OnImprovedAutoscrambleChanged);

    RegPluginLibrary("improved_autoscramble");
    CreateNative("Autoscramble_IsInProgress", Native_Autoscramble_IsInProgress);
    CreateNative("Autoscramble_IsBlockingVote", Native_Autoscramble_IsBlockingVote);

}

public void OnImprovedAutoscrambleChanged(ConVar cvar, const char[] oldVal, const char[] newVal)
{
    if (StringToInt(newVal) == 1 && cvar_ref_mp_scrambleteams_auto.BoolValue)
        cvar_ref_mp_scrambleteams_auto.SetBool(false);
}

// ============================================================
// GameRules hook (decision + VOX gating)
// ============================================================

MRESReturn OnSetWinningTeam_Post(DHookParam params)
{
    TFTeam team = view_as<TFTeam>(params.Get(1));
    bool bForceMapReset = params.Get(3);
    bool bSwitchTeams = params.Get(4);

    if (!bForceMapReset)
        return MRES_Ignored;

    if (!ShouldAutoscramble(team, g_bIsArena, bSwitchTeams))
        return MRES_Ignored;

    PrintToChatAll("ShouldAutoscramble returned true!");
    // Set g_bAutoscrambleInProgress to true so plugins that implement vote scramble can implement blocking to prevent double scramble.
    Autoscramble_SetInProgress(true);
    Autoscramble_SetBlockVoting(true);
    CreateTimer(8.0,  Timer_FireScrambleEvent, _, TIMER_FLAG_NO_MAPCHANGE);
    CreateTimer(12.5, Timer_FireScrambleEvent, _, TIMER_FLAG_NO_MAPCHANGE);

    // Handle Vox.
    if (cvar_improved_autoscramble_enable_administrator_vox.BoolValue)
    {
        if (!g_bIsArena)
        {
            // PlayAdministratorVoxPre();
            PrintToChatAll("This is the time we would have played the Pre part of the Autoscramble Vox (non-arena");
        }
        else
        {
            if (!IsFlawlessVictory(team)) {
                // PlayAdministratorVoxPre();
                PrintToChatAll("This is the time we would have played the Pre part of the Autoscramble Vox (Arena)");
                PrintToChatAll("If you see this text during a flawless victory state, something is wrong!");
            }
        }
    }

    return MRES_Ignored;
}

// ============================================================
// Round start: perform scramble
// ============================================================

public void OnPreRoundStart(Event event, const char[] name, bool dontBroadcast)
{
    if (Autoscramble_InProgress()) {
        
    //ScrambleTeams();
    CreateTimer(Autoscramble_GetBlockVoteTime(), Timer_SetAutoscrambleInProgressFalse, _, TIMER_FLAG_NO_MAPCHANGE);
    PrintToChatAll("This is where a scramble would have happened in OnPreRoundStart()");

     if (cvar_improved_autoscramble_enable_administrator_vox.BoolValue) {
        //PlayAdministratorVoxPost();
        PrintToChatAll("This is the part where we would have played the Post part of the Autoscramble Vox");
     }

    Autoscramble_SetInProgress(false);
    CreateTimer(Autoscramble_GetBlockVoteTime(), Timer_SetAutoscrambleInProgressFalse, _, TIMER_FLAG_NO_MAPCHANGE);
    }

}


// Timers

public Action Timer_SetAutoscrambleInProgressFalse(Handle timer)
{
    PrintToServer("[Timer] Delayed action executed");
    Autoscramble_SetBlockVoting(false);

    return Plugin_Stop;
}

public Action Timer_FireScrambleEvent(Handle timer)
{
    PrintToServer("[Timer] Delayed action executed");
    CreateAndFireScrambleEvent();

    return Plugin_Stop;
}
