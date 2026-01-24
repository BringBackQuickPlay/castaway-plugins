/*
	╔════════════════════════════════════════════════════╗
	║                    !!README!!!                     ║
	╚════════════════════════════════════════════════════╝

	This plugin is intended to replace the builtin
	autoscramble in TF2. If this plugin is loaded, it will
	automatically disable the autoscramble cvars in TF2.

	The plugin additionally enables you to play up to 5 gamesounds/voicelines
	during the autoscramble process.
*/

#include <sourcemod>
#include <sdktools>
#include <sdkhooks>
#include <clientprefs>
#include <tf2>
#include <tf2_stocks>
#include <dhooks>
#include <scramble>

#define PLUGIN_NAME "Improved Autoscramble"
#define PLUGIN_DESC "Allows more control over the autoscramble process, also adds the ability to play game sounds and voicelines during the autoscramble."
#define PLUGIN_AUTHOR "AUTHOR"

#define PLUGIN_VERSION "0.0.1"

#if defined GIT_COMMIT
#define PLUGIN_VERSION_GIT PLUGIN_VERSION ... "%GIT_COMMIT%"
#endif

#define PLUGIN_URL "https://castaway.tf"

public Plugin myinfo = {
	name = PLUGIN_NAME,
	description = PLUGIN_DESC,
	author = PLUGIN_AUTHOR,
#if defined GIT_COMMIT
	version = PLUGIN_VERSION_GIT,
#else
	version = PLUGIN_VERSION,
#endif
	url = PLUGIN_URL
};

int RealPointsRED // How many real points (aka not multistage wins/caps) the team has won during this match.
int RealPointsBLU // Ditto

// Cvars

ConVar cvar_ref_mp_tournament;
ConVar cvar_ref_tf_gamemode_mvm;
ConVar cvar_ref_nextlevel;

// 1 = Default TF2, I.e if you set it to 2, in a worst case scenario, a team would need to win 3 times in a row to autoscramble.
// 2 = Streak mode, the team only needs to win this amount of times in a row to trigger autoscramble.
// so instead of TF2's delta stuff, it just checks "Has this team won x in a row? Then Autoscramble".
// If mode is set to 2, then streak will clamp it's min to 2, if you use mode 1, then it can be a minimum of 1.

ConVar cvar_improved_autoscramble_mode; 
ConVar cvar_improved_autoscramble_streak;
ConVar cvar_improved_autoscramble;

// Hooks

DynamicHook dhook_CTFGameRules_SetWinningTeam;

enum
{
	TF_GAMETYPE_UNDEFINED = 0,
	TF_GAMETYPE_CTF,
	TF_GAMETYPE_CP,
	TF_GAMETYPE_ESCORT,
	TF_GAMETYPE_ARENA,
	TF_GAMETYPE_MVM,
	TF_GAMETYPE_RD,
	TF_GAMETYPE_PASSTIME,
	TF_GAMETYPE_PD,

	TF_GAMETYPE_COUNT
};


public void OnPluginStart() {
	// Initialize point tracking.
	RealPointsRED = 0;
	RealPointsBLU = 0;

	// Create ConVars

	cvar_improved_autoscramble = CreateConVar("sm_improved_autoscramble", "0", (PLUGIN_NAME ... " - Use the improved autoscramble (forces TF2's own Autoscramble to OFF"), _, true, 0.0, true, 1.0);
	cvar_improved_autoscramble_mode = CreateConVar("sm_improved_autoscramble", "0", (PLUGIN_NAME ... " - Use the improved autoscramble (forces TF2's own Autoscramble to OFF"), _, true, 0.0, true, 1.0);
	cvar_improved_autoscramble_streak = CreateConVar("sm_improved_autoscramble", "0", (PLUGIN_NAME ... " - Use the improved autoscramble (forces TF2's own Autoscramble to OFF"), _, true, 0.0, true, 1.0);

	// Find ConVars
	cvar_ref_mp_tournament = FindConVar("mp_tournament");
	cvar_ref_tf_gamemode_mvm = FindConVar("tf_gamemode_mvm");
	cvar_ref_nextlevel = FindConVar("nextlevel");

	// Do hooks
	dhook_CTFGameRules_SetWinningTeam = DynamicHook.FromConf(conf, "CTFGameRules::SetWinningTeam");

	// Check hooks
	if (dhook_CTFGameRules_SetWinningTeam == null) SetFailState("Failed to create dhook_CTFGameRules_SetWinningTeam");

	// Setup Callbacks for hooks
	dhook_CTFGameRules_SetWinningTeam.HookGameRules(Hook_Post, DHookCallback_CTFGameRules_SetWinningTeam);


}

public int GetGameType() {
	return GameRules_GetProp("m_nGameType");
}

// First three methods to recreate IF check at
// https://github.com/ValveSoftware/source-sdk-2013/blob/11a677c349b149b2f77184dc903e6bb17f8df69b/src/game/shared/teamplayroundbased_gamerules.cpp#L2407
public bool IsInArenaMode() {
	if (GetGameType() == TF_GAMETYPE_ARENA) {
		return true;
	}
	return false;
}
public bool IsInTournamentMode()  {
	if (cvar_ref_mp_tournament.BoolValue) {
		return true;
	}
	return false;
}
public bool ShouldSkipAutoScramble()  {
	// In TF2 it also has checks for the cancelled raid mode, but we don't need to bother with that.
	// We only need to check if we are playing MvM or not.
	if (cvar_ref_tf_gamemode_mvm.BoolValue) {
		return true;
	}
	return false;
}
bool IsNextLevelEmpty()
{
    char nextlevel[PLATFORM_MAX_PATH];
    cvar_ref_nextlevel.GetString(nextlevel, sizeof(nextlevel));

    return StrEqual(nextlevel, "", false);
}

// Callback

MRESReturn DHookCallback_CTFGameRules_SetWinningTeam(DHookReturn returnValue, DHookParam parameters) {
	int team = parameters.Get(1);
	int iWinReason = parameters.Get(2);
	bool bForceMapReset = parameters.Get(3);

	

	return MRES_Ignored;
}

// Primary main function. Always keep this at the bottom of the plugin.
public bool HandleAutoScramble (int team, int iWinReason, bool bForceMapReset) {

	if (bForceMapReset && cvar_improved_autoscramble.BoolValue) {
		if (IsInArenaMode() || IsInTournamentMode() || ShouldSkipAutoScramble()) {
			return false;
		}	
		// cvar_ref_nextlevel
		if (!IsNextLevelEmpty())
		{
		    return false;
		}


	} else {return false;}


	return true;
}