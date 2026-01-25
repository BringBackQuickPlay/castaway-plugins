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


// Cvars references

ConVar cvar_ref_mp_scrambleteams_auto;
ConVar cvar_ref_mp_tournament;
ConVar cvar_ref_tf_gamemode_mvm;
ConVar cvar_ref_nextlevel;

// Plugin created convars

// Following comments are for cvar_improved_autoscramble_mode
// 1 = Default TF2, I.e if you set it to 2, in a worst case scenario, a team would need to win 3 times in a row to autoscramble.
// 2 = Streak mode, the team only needs to win this amount of times in a row to trigger autoscramble.
// so instead of TF2's delta stuff, it just checks "Has this team won x in a row? Then Autoscramble".
// If mode is set to 2, then cvar_improved_autoscramble_amount will clamp it's min to 2, if you use mode 1, then it can be a minimum of 1.

ConVar cvar_improved_autoscramble;
ConVar cvar_improved_autoscramble_mode; 
ConVar cvar_improved_autoscramble_amount;
ConVar cvar_improved_autoscramble_enable_administrator_vox;

// Mode stuff for cvar_improved_autoscramble_mode

// Used for mode 1 (Replicates valves own math)
int g_GameTeamWins[2]; // [0]=RED, [1]=BLU

// Used for mode 2 (Our own custom math)
int RealPointsRED; // How many real points (aka not multistage wins/caps) the team has won during this match.
int RealPointsBLU; // Ditto

// Hooks

DynamicHook dhook_CTFGameRules_SetWinningTeam;

// Vox stuff




#include <improved_autoscramble>


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
	GameData conf;

	// Initialize point tracking.
	RealPointsRED = 0;
	RealPointsBLU = 0;

	// Create ConVars

	cvar_improved_autoscramble = CreateConVar("sm_improved_autoscramble", "0", PLUGIN_NAME ... " - Enable improved autoscramble and disable TF2's builtin autoscramble", FCVAR_NOTIFY, true, 0.0, true, 1.0);
	cvar_improved_autoscramble_mode = CreateConVar("sm_improved_autoscramble_mode", "1", PLUGIN_NAME ... " - Mode: 1 = TF2 delta (windifference), 2 = exact win-streak (stages ignored)", FCVAR_NOTIFY, true, 1.0, true, 2.0);
	cvar_improved_autoscramble_amount = CreateConVar("sm_improved_autoscramble_amount", "2", PLUGIN_NAME ... " - Windifference (mode 1) or required wins in a row (mode 2)", FCVAR_NOTIFY, true, 1.0, true, 100.0);
	cvar_improved_autoscramble_enable_administrator_vox = CreateConVar("sm_improved_autoscramble_enable_administrator_vox", "0", PLUGIN_NAME ... " - Should we play administrator voicelines when autoscrambling?", FCVAR_NOTIFY, true, 0.0, true, 1.0);
	
	// Find ConVars
	cvar_ref_mp_scrambleteams_auto = FindConVar("mp_scrambleteams_auto");
	cvar_ref_mp_tournament = FindConVar("mp_tournament");
	cvar_ref_tf_gamemode_mvm = FindConVar("tf_gamemode_mvm");
	cvar_ref_nextlevel = FindConVar("nextlevel");

	// Do Convar Hooks.

	cvar_improved_autoscramble.AddChangeHook(OnImprovedAutoscrambleChanged);
	cvar_improved_autoscramble_mode.AddChangeHook(OnImprovedAutoscrambleModeChanged);
	cvar_improved_autoscramble_amount.AddChangeHook(OnImprovedAutoscrambleAmountChanged);
	cvar_improved_autoscramble_enable_administrator_vox.AddChangeHook(OnImprovedAutoscrambleVoxChanged);
	cvar_ref_mp_scrambleteams_auto.AddChangeHook(OnValveAutoscrambleChanged);


	// GameData Config
	conf = new GameData("improved_autoscramble");
	if (conf == null) SetFailState("Failed to load reverts conf");

	// Do hooks
	dhook_CTFGameRules_SetWinningTeam = DynamicHook.FromConf(conf, "CTFGameRules::SetWinningTeam");

	// Check hooks
	if (dhook_CTFGameRules_SetWinningTeam == null) SetFailState("Failed to create dhook_CTFGameRules_SetWinningTeam");

	// Setup Callbacks for hooks
	dhook_CTFGameRules_SetWinningTeam.HookGamerules(Hook_Post, DHookCallback_CTFGameRules_SetWinningTeam);


}

public void OnConfigsExecuted()
{
    if (!ValidateAutoscrambleCvars())
    {
        SetFailState("Improved Autoscramble disabled due to invalid ConVar values");
    }
    if (cvar_improved_autoscramble.BoolValue) {
    	// Disable the built in cvar.
    	cvar_ref_mp_scrambleteams_auto.SetBool(false);
    }
}

public void OnImprovedAutoscrambleChanged(
    ConVar convar,
    const char[] oldValue,
    const char[] newValue
)
{
    // Reject invalid values live
    if (!IsStrictBool(newValue))
    {
        LogError(
            "[Improved Autoscramble] Invalid value \"%s\" for sm_improved_autoscramble. Reverting to \"%s\".",
            newValue,
            oldValue
        );

        // Revert silently
        convar.SetString(oldValue);
        return;
    }

    bool enabled = StringToInt(newValue) == 1;

    if (enabled)
    {
        // Force-disable Valve autoscramble
        if (cvar_ref_mp_scrambleteams_auto != null &&
            cvar_ref_mp_scrambleteams_auto.BoolValue)
        {
            cvar_ref_mp_scrambleteams_auto.SetBool(false);
            LogMessage("[Improved Autoscramble] Disabled mp_scrambleteams_auto");
        }
    }
}

public void OnImprovedAutoscrambleModeChanged(
    ConVar convar,
    const char[] oldValue,
    const char[] newValue
)
{
    if (!IsStrictInteger(newValue))
    {
        LogError(
            "[Improved Autoscramble] sm_improved_autoscramble_mode must be an integer (got \"%s\"). Reverting.",
            newValue
        );
        convar.SetString(oldValue);
        return;
    }

    int mode = StringToInt(newValue);
    if (mode != 1 && mode != 2)
    {
        LogError(
            "[Improved Autoscramble] sm_improved_autoscramble_mode must be 1 or 2 (got %d). Reverting.",
            mode
        );
        convar.SetString(oldValue);
    }
}

public void OnImprovedAutoscrambleAmountChanged(
    ConVar convar,
    const char[] oldValue,
    const char[] newValue
)
{
    if (!IsStrictInteger(newValue))
    {
        LogError(
            "[Improved Autoscramble] sm_improved_autoscramble_amount must be an integer (got \"%s\"). Reverting.",
            newValue
        );
        convar.SetString(oldValue);
        return;
    }

    int amount = StringToInt(newValue);
    if (amount < 1 || amount > 100)
    {
        LogError(
            "[Improved Autoscramble] sm_improved_autoscramble_amount must be between 1 and 100 (got %d). Reverting.",
            amount
        );
        convar.SetString(oldValue);
    }
}

public void OnImprovedAutoscrambleVoxChanged(
    ConVar convar,
    const char[] oldValue,
    const char[] newValue
)
{
    if (!IsStrictBool(newValue))
    {
        LogError(
            "[Improved Autoscramble] sm_improved_autoscramble_enable_administrator_vox must be 0 or 1 (got \"%s\"). Reverting.",
            newValue
        );
        convar.SetString(oldValue);
    }
}

public void OnValveAutoscrambleChanged(
    ConVar convar,
    const char[] oldValue,
    const char[] newValue
)
{
    // Only enforce if our plugin is enabled
    if (!cvar_improved_autoscramble.BoolValue)
        return;

    // Valve autoscramble is boolean, but be strict anyway
    if (!IsStrictBool(newValue))
    {
        LogError(
            "[Improved Autoscramble] Invalid value \"%s\" for mp_scrambleteams_auto. Reverting.",
            newValue
        );
        convar.SetString(oldValue);
        return;
    }

    // If someone tries to enable it, shut it down
    if (StringToInt(newValue) == 1)
    {
        LogMessage(
            "[Improved Autoscramble] mp_scrambleteams_auto was enabled while Improved Autoscramble is active. Forcing it OFF."
        );

        convar.SetBool(false);
    }
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

bool ValidateAutoscrambleCvars()
{
    char buf[32];

    cvar_improved_autoscramble.GetString(buf, sizeof(buf));
    if (!IsStrictBool(buf))
    {
        LogError("[Improved Autoscramble] sm_improved_autoscramble must be 0 or 1 (got \"%s\")", buf);
        return false;
    }

    cvar_improved_autoscramble_mode.GetString(buf, sizeof(buf));
    if (!IsStrictInteger(buf))
    {
        LogError("[Improved Autoscramble] sm_improved_autoscramble_mode must be an integer (got \"%s\")", buf);
        return false;
    }

    cvar_improved_autoscramble_amount.GetString(buf, sizeof(buf));
    if (!IsStrictInteger(buf))
    {
        LogError("[Improved Autoscramble] sm_improved_autoscramble_amount must be an integer (got \"%s\")", buf);
        return false;
    }

    cvar_improved_autoscramble_enable_administrator_vox.GetString(buf, sizeof(buf));
	if (!IsStrictBool(buf))
	{
	    LogError(
	        "[Improved Autoscramble] sm_improved_autoscramble_enable_administrator_vox must be 0 or 1 (got \"%s\")",
	        buf
	    );
	    return false;
	}


    int mode = cvar_improved_autoscramble_mode.IntValue;
	if (mode != 1 && mode != 2)
	{
	    LogError("[Improved Autoscramble] sm_improved_autoscramble_mode must be 1 or 2 (got %d)", mode);
	    return false;
	}

	int amount = cvar_improved_autoscramble_amount.IntValue;
	if (amount < 1 || amount > 100)
	{
	    LogError("[Improved Autoscramble] sm_improved_autoscramble_amount out of range (1–100): %d", amount);
	    return false;
	}

    return true;
}


// Callbacks

MRESReturn DHookCallback_CTFGameRules_SetWinningTeam(DHookReturn returnValue, DHookParam parameters) {
	int team = parameters.Get(1);
	int iWinReason = parameters.Get(2);
	bool bForceMapReset = parameters.Get(3);

	bool ShouldWeScramble = HandleAutoScramble(team, iWinReason, bForceMapReset);

	if (ShouldWeScramble) {
		if (cvar_improved_autoscramble_enable_administrator_vox.BoolValue) {
				//PlayAdministratorVoxPreScramble();
				//ScrambleTeams(); // Method from include/scramble.inc
				//Add a delay here, or defer to a team-switch event or something, anything so it's not directly after.
				//PlayAdministratorVoxPostScramble();
			} else {
				//ScrambleTeams(); // Method from include/scramble.inc
			}
	}

	return MRES_Ignored;
}

// Primary main function. Always keep this at the bottom of the plugin.
// Do not add anything below this method!
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

		// Mode selection. If mode 1, use TF2's Math.
		// If mode 2, use our own math.
		// 1 = Team might need to win up to 3 times if streak is set to 2 because Valve math be weird.
		// 2 = Team only need to win exactly the streak to win. For example if BLU wins dustbowl (Not the stages!) 2 times, a scramble will happen.

		// In Essence, When Valve made Autoscramble, they seemed to care more about the team "team", as in the team color not mattering, but instead
		// looking at the team in terms of players, this
		// could be why they switch the scores over to the other team.
		// Also the team score you see ingame is NOT the score Valve tracks for their Autoscramble, instead they
		// gate it on SetWinningTeam where IF bForceMapReset and autoscramble is true = store a "win" for that team.

		// This is a hypothesis btw and not complete or neccesarily correct.

		if (cvar_improved_autoscramble_mode.IntValue == 1) {
			// Safety: only RED or BLU should ever reach here
			if (team != TFTeam_Red && team != TFTeam_Blue)
			return false;

			int winIndex = TeamToIndex(team);

			// -------------------------------
			// Look for impending map end
			// -------------------------------
			// Valve avoids scrambling near map end to prevent pointless reshuffles

			// NOTE: In SM we don’t have CanChangelevelBecauseOfTimeLimit()
			// but we *can* approximate intent.

			if (GetMapTimeLeft() > 0 && GetMapTimeLeft() <= 300)
			{
				// Near map end → skip autoscramble
				return false;
			}

			// -------------------------------
			// Winlimit / maxrounds guards
			// -------------------------------
			int winlimit   = FindConVar("mp_winlimit").IntValue;
			int maxrounds  = FindConVar("mp_maxrounds").IntValue;
			int roundsPlayed = GameRules_GetProp("m_nRoundsPlayed");

			if (maxrounds > 0 && (maxrounds - roundsPlayed) == 1)
			{
				return false;
			}

			if (winlimit > 0)
			{
				int redScore  = GetTeamScore(TFTeam_Red);
				int bluScore  = GetTeamScore(TFTeam_Blue);

				if ((winlimit - redScore) == 1 || (winlimit - bluScore) == 1)
				{
					return false;
				}
			}

			// -------------------------------
			// Increment win counter
			// -------------------------------
			g_GameTeamWins[winIndex]++;

			// -------------------------------
			// Compute win delta
			// -------------------------------
			int winDelta = abs(g_GameTeamWins[0] - g_GameTeamWins[1]);

			int requiredDelta = cvar_improved_autoscramble_amount.IntValue;

			if (winDelta >= requiredDelta)
			{
				// ===============================
				// AUTOSCRAMBLE SHOULD TRIGGER HERE
				// ===============================
				// - announce scramble
				// - schedule scramble on round restart
				// - reset scores / rounds as desired
				// - play administrator vox if enabled

				// DO NOT ACTUALLY SCRAMBLE HERE YET
				// just signal intent
			}

			// -------------------------------
			// Handle post-win team switching
			// -------------------------------
			if (GameRules_GetProp("m_bSwitchTeams"))
			{
				int temp = g_GameTeamWins[0];
				g_GameTeamWins[0] = g_GameTeamWins[1];
				g_GameTeamWins[1] = temp;
			}
		} else if (cvar_improved_autoscramble_mode.IntValue == 2) {

		}


	} else {return false;}

	return true;
}