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
// Globals (autoscramble vote gating)
// ============================================================

bool  g_bAutoscrambleInProgress;        // SetWinningTeam → PreRound transition lock
bool  g_bIsArena;                       // Determined via tf_logic_arena

float g_flNextScrambleVoteAllowedTime;  // Earliest time vote plugins may allow scramble again

// Config-derived policy values
float g_flAutoscrambleVoteDelay_Default; // Non-arena
float g_flAutoscrambleVoteDelay_Arena;   // Arena



// ============================================================
// Config loading (addons/sourcemod/configs/improved_autoscramble.cfg)
// ============================================================

void LoadImprovedAutoscrambleConfig()
{
    char path[PLATFORM_MAX_PATH];
    BuildPath(Path_SM, path, sizeof(path), "configs/improved_autoscramble.cfg");

    // ----------------------------------------------------------------
    // Create config with defaults if it does not exist
    // ----------------------------------------------------------------
    if (!FileExists(path))
    {
        KeyValues kvCreate = new KeyValues("ImprovedAutoscramble");

        kvCreate.JumpToKey("scramble_vote_delay", true);
        kvCreate.SetFloat("default", 30.0); // non-arena default
        kvCreate.SetFloat("arena",   10.0); // arena default
        kvCreate.Rewind();

        if (!kvCreate.ExportToFile(path))
        {
            delete kvCreate;
            SetFailState("Failed to create default config file: %s", path);
            return;
        }

        delete kvCreate;
    }

    // ----------------------------------------------------------------
    // Load config
    // ----------------------------------------------------------------
    KeyValues kv = new KeyValues("ImprovedAutoscramble");

    if (!kv.ImportFromFile(path))
    {
        delete kv;
        SetFailState("Failed to load config file: %s", path);
        return;
    }

    if (!kv.JumpToKey("scramble_vote_delay", false))
    {
        delete kv;
        SetFailState("Missing 'scramble_vote_delay' section in %s", path);
        return;
    }

    g_flAutoscrambleVoteDelay_Default = kv.GetFloat("default", 30.0);
    g_flAutoscrambleVoteDelay_Arena   = kv.GetFloat("arena",   10.0);

    delete kv;
}



// ============================================================
// Helpers to K I L L double scramble.
// ============================================================

float Autoscramble_GetVoteDelay()
{
    return g_bIsArena
        ? g_flAutoscrambleVoteDelay_Arena
        : g_flAutoscrambleVoteDelay_Default;
}

void Autoscramble_ApplyVoteDelay()
{
    g_flNextScrambleVoteAllowedTime =
        GetGameTime() + Autoscramble_GetVoteDelay();
}

bool Autoscramble_IsBlockingVote()
{
    return GetGameTime() < g_flNextScrambleVoteAllowedTime;
}

float Autoscramble_GetNextTimeCanVote()
{
    return g_flNextScrambleVoteAllowedTime;
}


//
// Vars for Mode 1 of Autoscramble
//

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
ConVar cvar_improved_autoscramble_votescrambleblocktime;

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

    Autoscramble_ApplyVoteDelay(); // Needs to be run after the g_bIsArena has been determined.

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
    
    PrintToChatAll("g_GameTeamWins[0] is: %d",g_GameTeamWins[0]);
    PrintToChatAll("g_GameTeamWins[1] is: %d",g_GameTeamWins[1]);

    if (bSwitchTeams)
    {
        int tmp = g_GameTeamWins[0];
        g_GameTeamWins[0] = g_GameTeamWins[1];
        g_GameTeamWins[1] = tmp;
        PrintToChatAll("g_GameTeamWins[0] is: %d AFTER TeamSwitch due to bSwitchTeams being true.",g_GameTeamWins[0]);
        PrintToChatAll("g_GameTeamWins[1] is: %d AFTER TeamSwitch due to bSwitchTeams being true.",g_GameTeamWins[1]);
    }

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

public any Native_Autoscramble_GetNextScrambleVoteAllowedTime(Handle plugin, int numParams)
{
    return g_flNextScrambleVoteAllowedTime;
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

float Autoscramble_GetNextTimeCanVote()
{
    return g_fVoteScrambleBlockedUntil;
}

void Autoscramble_SetNextTimeCanVote(float time)
{
    g_fVoteScrambleBlockedUntil = time;
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
        CreateConVar("sm_improved_autoscramble_amount", "2",
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
    CreateNative("Autoscramble_GetNextScrambleVoteAllowedTime", Native_Autoscramble_GetNextScrambleVoteAllowedTime);


    // Testing commands, remove later.

    RegConsoleCmd("sm_roundtimer", Command_RoundTimer);
    RegConsoleCmd("sm_scoutblucap", Command_ScoutBluCap);
    RegConsoleCmd("sm_addtestbots", Command_AddTestBots);


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

    // If you cannot vote for a scramble, you cannot autoscramble either.
    if (Autoscramble_GetNextTimeCanVote() - GetGameTime() > float(0))
        return MRES_Ignored;
    

    PrintToChatAll("ShouldAutoscramble returned true!");
    // Set g_bAutoscrambleInProgress to true so plugins that implement vote scramble can implement blocking to prevent double scramble.
    Autoscramble_SetInProgress(true);
    Autoscramble_SetBlockVoting(true);
    CreateTimer(8.0,  Timer_FireScrambleEvent, _, TIMER_FLAG_NO_MAPCHANGE);
    CreateTimer(10, Timer_FireScrambleEvent, _, TIMER_FLAG_NO_MAPCHANGE);

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
    ScrambleTeams();
    CreateTimer(Autoscramble_GetBlockVoteTime(), Timer_SetAutoscrambleInProgressFalse, _, TIMER_FLAG_NO_MAPCHANGE);
    Autoscramble_SetNextTimeCanVote(GetGameTime() + float(30));
    PrintToChatAll("This is where a scramble would have happened in OnPreRoundStart()");

     if (cvar_improved_autoscramble_enable_administrator_vox.BoolValue) {
        //PlayAdministratorVoxPost();
        PrintToChatAll("This is the part where we would have played the Post part of the Autoscramble Vox");
     }

    Autoscramble_SetInProgress(false);
    }

}


// Timers

public Action Timer_FireScrambleEvent(Handle timer)
{
    PrintToServer("[Timer] Delayed action executed");
    CreateAndFireScrambleEvent();

    return Plugin_Stop;
}

// Testing code. Remove later.

// ====================
// CONFIGURATION
// ====================

// setpos -198.440384 2798.721924 -175.817810;setang 29.557467 -62.604286 0.000000
// setpos 2260.668945 2363.089600 -71.817810;setang 22.510197 -129.314087 0.000000
// setpos 2293.320312 -1557.695679 72.182190;setang 19.089193 -179.138428 0.000000
// setpos -1530.393188 -1978.593018 26.182194;setang 13.205048 44.089554 0.000000
// setpos -1859.326904 648.124451 72.182190;setang -6.226285 -86.915520 0.000000
// setpos 524.182312 710.672241 72.182190;setang -3.489487 -93.415474 0.000000

int g_TestBotNameIndex = 0;

// ====================
// !roundtimer COMMAND
// ====================

public Action Command_RoundTimer(int client, int args)
{
    if (args != 1)
    {
        ReplyToCommand(client, "Usage: !roundtimer <seconds>");
        return Plugin_Handled;
    }

    int delta = GetCmdArgInt(1);

    int ent = -1;
    while ((ent = FindEntityByClassname(ent, "team_round_timer")) != -1)
    {
        SetVariantInt(delta);
        AcceptEntityInput(ent, "AddTime");

        ReplyToCommand(client, "Round timer modified by %d seconds.", delta);
        return Plugin_Handled;
    }

    ReplyToCommand(client, "No team_round_timer entity found.");
    return Plugin_Handled;
}




// ====================
// CAP POSITIONS (getpos + 10u Z safety)
// ====================

float g_CapPositions[6][3] =
{
    {  -198.440384,     2798.721924,    -165.817810 }, // cap1
    {  2260.668945,     2363.089600,    -61.817810 },  // cap2
    {  2293.320312,     -1557.695679,   82.182190 },  // cap3
    {  -1530.393188,    -1978.593018,   36.182194 },  // cap4
    {  -1859.326904,    648.124451,     82.182190 },  // cap5
    {  524.182312,      710.672241,     82.182190 }   // cap6
};

// ====================
// !addtestbots COMMAND
// ====================

public Action Command_AddTestBots(int client, int args)
{
    // Reset name pool index for repeatable testing
    g_TestBotNameIndex = 0;

    // ----- RED TEAM -----
    // Ensure one Scout on RED
    AddBotToTeam(2, true);

    // Remaining RED bots (total RED = 6)
    for (int i = 1; i < 6; i++)
    {
        AddBotToTeam(2, false);
    }

    // ----- BLU TEAM -----
    // Total BLU = 5
    for (int i = 0; i < 5; i++)
    {
        AddBotToTeam(3, false);
    }

    ReplyToCommand(client, "Added test bots: 6 RED (1 Scout), 5 BLU.");

    return Plugin_Handled;
}

// ====================
// !scoutblucap COMMAND
// ====================

public Action Command_ScoutBluCap(int client, int args)
{
    if (args != 1)
    {
        ReplyToCommand(client, "Usage: !scoutblucap <1|2|3>");
        return Plugin_Handled;
    }

    int mode = GetCmdArgInt(1);
    if (mode < 1 || mode > 3)
    {
        ReplyToCommand(client, "Invalid mode. Use 1, 2, or 3.");
        return Plugin_Handled;
    }

    int scout = FindAliveBluScout();
    if (scout == 0)
    {
        ReplyToCommand(client, "No alive BLU Scout found.");
        return Plugin_Handled;
    }

    int firstCap  = (mode - 1) * 2;
    int secondCap = firstCap + 1;

    TeleportToCap(scout, firstCap);

    DataPack pack = new DataPack();
    pack.WriteCell(EntIndexToEntRef(scout));
    pack.WriteCell(secondCap);

    CreateTimer(3.0, Timer_SecondCap, pack);


    ReplyToCommand(client, "Moved BLU Scout through cap sequence %d.", mode);
    return Plugin_Handled;
}

// ====================
// BLU SCOUT SEARCH
// ====================

int FindAliveBluScout()
{
    for (int i = 1; i <= MaxClients; i++)
    {
        if (!IsClientInGame(i))
            continue;

        if (!IsPlayerAlive(i))
            continue;

        if (GetClientTeam(i) != 3) // BLU
            continue;

        if (TF2_GetPlayerClass(i) != TFClass_Scout)
            continue;

        return i;
    }

    return 0;
}

// ====================
// TELEPORT HELPERS
// ====================

void TeleportToCap(int client, int capIndex)
{
    float ang[3] = { 0.0, 0.0, 0.0 };
    TeleportEntity(client, g_CapPositions[capIndex], ang, NULL_VECTOR);
}

// ====================
// TIMER CALLBACK
// ====================

public Action Timer_SecondCap(Handle timer, any data)
{
    DataPack pack = view_as<DataPack>(data);
    pack.Reset();

    int entRef   = pack.ReadCell();
    int capIndex = pack.ReadCell();

    delete pack;

    int client = EntRefToEntIndex(entRef);
    if (client <= 0)
        return Plugin_Stop;

    if (!IsClientInGame(client))
        return Plugin_Stop;

    if (!IsPlayerAlive(client))
        return Plugin_Stop;

    TeleportToCap(client, capIndex);
    return Plugin_Stop;
}


// ====================
// BOT NAME POOL
// ====================

static const char g_TestBotNames[][] =
{
    "IronClash",
    "Redline",
    "Payload",
    "Frontline",
    "Hardpoint",
    "Overtime",
    "Crossfire",
    "Breakthrough",
    "LastStand",
    "Steamroll",
    "Stalemate",
    "Backcap",
    "PointHold",
    "SuddenPush",
    "FinalBell",
    "NoMercy",
    "PushFail",
    "LineBreaker",
    "AllHands",
    "Gridlock"
};



// ====================
// BOT CREATION
// ====================

void AddBotToTeam(int team, bool forceScout)
{
    if (g_TestBotNameIndex >= sizeof(g_TestBotNames))
    {
        LogError("AddBotToTeam: bot name pool exhausted.");
        return;
    }

    int bot = CreateFakeClient(g_TestBotNames[g_TestBotNameIndex++]);
    if (bot == 0)
        return;

    TF2_ChangeClientTeam(bot, view_as<TFTeam>(team));

    TFClassType class = forceScout ? TFClass_Scout : GetRandomNonHeavyClass();
    TF2_SetPlayerClass(bot, class, false);

    TF2_RespawnPlayer(bot);
}

// ====================
// CLASS SELECTION
// ====================

TFClassType GetRandomNonHeavyClass()
{
    TFClassType class;
    do
    {
        class = view_as<TFClassType>(GetRandomInt(1, 9));
    }
    while (class == TFClass_Heavy);

    return class;
}

// ====================
