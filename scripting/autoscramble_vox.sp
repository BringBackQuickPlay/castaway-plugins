#include <sourcemod>
#include <sdktools>
#include <tf2>
#include <tf2_stocks>

#pragma semicolon 1
#pragma newdecls required

#define PLUGIN_NAME        "Autoscramble Voicelines"
#define PLUGIN_DESC        "Administrator VO responses for autoscramble events"
#define PLUGIN_AUTHOR      "VerdiusArcana"
#define PLUGIN_VERSION_NUM "0.0.1"

#define CONFIG_FILE "autoscramble_voicelines.cfg"

enum PreMode
{
	Pre_Off = 0,
	Pre_Insult,
	Pre_Snide
};

ArrayList g_PreInsults;
ArrayList g_PreSnide;
ArrayList g_Core;
ArrayList g_Mid;
ArrayList g_Post;

int g_PreChance;
int g_MidChance;
int g_PostChance;
PreMode g_PreMode;

public Plugin myinfo =
{
	name        = PLUGIN_NAME,
	description = PLUGIN_DESC,
	author      = PLUGIN_AUTHOR,
	version     = PLUGIN_VERSION_NUM
};

public void OnPluginStart()
{
	g_PreInsults = new ArrayList(ByteCountToCells(128));
	g_PreSnide   = new ArrayList(ByteCountToCells(128));
	g_Core       = new ArrayList(ByteCountToCells(128));
	g_Mid        = new ArrayList(ByteCountToCells(128));
	g_Post       = new ArrayList(ByteCountToCells(128));

	LoadConfig();
}

void LoadConfig()
{
	char path[PLATFORM_MAX_PATH];
	BuildPath(Path_SM, path, sizeof(path), "configs/%s", CONFIG_FILE);

	KeyValues kv = new KeyValues("AutoscrambleVoicelines");
	if (!kv.ImportFromFile(path))
	{
		SetFailState("Failed to load %s", path);
	}

	// ---- PRE ----
	kv.JumpToKey("pre");

	g_PreChance = kv.GetNum("chance", 0);

	char mode[16];
	kv.GetString("mode", mode, sizeof(mode), "off");

	if (StrEqual(mode, "insult", false))
		g_PreMode = Pre_Insult;
	else if (StrEqual(mode, "snide", false))
		g_PreMode = Pre_Snide;
	else
		g_PreMode = Pre_Off;

	ReadStringList(kv, "insult", g_PreInsults);
	ReadStringList(kv, "snide", g_PreSnide);

	kv.GoBack();

	// ---- CORE ----
	ReadStringList(kv, "core", g_Core);

	// ---- MID ----
	kv.JumpToKey("mid");
	g_MidChance = kv.GetNum("chance", 0);
	ReadStringList(kv, "lines", g_Mid);
	kv.GoBack();

	// ---- POST ----
	kv.JumpToKey("post");
	g_PostChance = kv.GetNum("chance", 0);
	ReadStringList(kv, "lines", g_Post);
	kv.GoBack();

	delete kv;
}

void ReadStringList(KeyValues kv, const char[] key, ArrayList list)
{
	list.Clear();

	if (!kv.JumpToKey(key))
		return;

	if (!kv.GotoFirstSubKey(false))
	{
		kv.GoBack();
		return;
	}

	do
	{
		char buffer[128];
		kv.GetSectionName(buffer, sizeof(buffer));
		list.PushString(buffer);
	}
	while (kv.GotoNextKey(false));

	kv.GoBack();
}

void EmitToTeam(const char[] sound, int team)
{
	for (int i = 1; i <= MaxClients; i++)
	{
		if (!IsClientInGame(i))
			continue;

		if (GetClientTeam(i) != team)
			continue;

		EmitGameSoundToClient(i, sound);
	}
}

void EmitGlobal(const char[] sound)
{
	EmitGameSoundToAll(sound);
}

void PickRandom(ArrayList list, char[] out, int size)
{
	list.GetString(GetRandomInt(0, list.Length - 1), out, size);
}

/*
	Call this function when autoscramble is triggered.

	losingTeam = TFTeam_Red or TFTeam_Blue
	callPost   = whether to allow POST lines (after scramble completes)
*/
public void PlayAutoscrambleVO(int losingTeam, bool callPost = true)
{
	char sound[128];

	// ---- PRE ----
	if (g_PreMode != Pre_Off && GetRandomInt(1, 100) <= g_PreChance)
	{
		if (g_PreMode == Pre_Insult && g_PreInsults.Length > 0)
		{
			PickRandom(g_PreInsults, sound, sizeof(sound));
			EmitToTeam(sound, losingTeam);
		}
		else if (g_PreMode == Pre_Snide && g_PreSnide.Length > 0)
		{
			PickRandom(g_PreSnide, sound, sizeof(sound));
			EmitGlobal(sound);
		}
	}

	// ---- CORE ----
	if (g_Core.Length > 0)
	{
		PickRandom(g_Core, sound, sizeof(sound));
		EmitGlobal(sound);
	}

	// ---- MID ----
	if (g_Mid.Length > 0 && GetRandomInt(1, 100) <= g_MidChance)
	{
		PickRandom(g_Mid, sound, sizeof(sound));
		EmitGlobal(sound);
	}

	// ---- POST ----
	if (callPost && g_Post.Length > 0 && GetRandomInt(1, 100) <= g_PostChance)
	{
		PickRandom(g_Post, sound, sizeof(sound));
		EmitGlobal(sound);
	}
}
