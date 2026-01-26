// Things that must NOT be 



// =========================
// Sound system enums (Valve-canonical values)
// =========================

// Channels
enum SoundChannel
{
    CHAN_REPLACE    = -1,
    CHAN_AUTO       = 0,
    CHAN_WEAPON     = 1,
    CHAN_VOICE      = 2,
    CHAN_ITEM       = 3,
    CHAN_BODY       = 4,
    CHAN_STREAM     = 5,
    CHAN_STATIC     = 6,
    CHAN_VOICE2     = 7
};

// Volume
#define VOL_NORM 1.0

// Pitch
enum SoundPitch
{
    PITCH_NORM = 100,
    PITCH_LOW  = 95,
    PITCH_HIGH = 120
};

// Sound levels (dB → attenuation)
enum SoundLevel
{
    SNDLVL_NONE        = 0,

    SNDLVL_20dB        = 20,
    SNDLVL_25dB        = 25,
    SNDLVL_30dB        = 30,
    SNDLVL_35dB        = 35,
    SNDLVL_40dB        = 40,
    SNDLVL_45dB        = 45,

    SNDLVL_50dB        = 50,
    SNDLVL_55dB        = 55,

    SNDLVL_IDLE        = 60,
    SNDLVL_60dB        = 60,

    SNDLVL_65dB        = 65,
    SNDLVL_STATIC      = 66,

    SNDLVL_70dB        = 70,

    SNDLVL_NORM        = 75,
    SNDLVL_75dB        = 75,

    SNDLVL_80dB        = 80,
    SNDLVL_TALKING     = 80,

    SNDLVL_85dB        = 85,
    SNDLVL_90dB        = 90,
    SNDLVL_95dB        = 95,

    SNDLVL_100dB       = 100,
    SNDLVL_105dB       = 105,
    SNDLVL_110dB       = 110,
    SNDLVL_120dB       = 120,
    SNDLVL_130dB       = 130,

    SNDLVL_GUNFIRE     = 140,
    SNDLVL_140dB       = 140,

    SNDLVL_150dB       = 150,
    SNDLVL_180dB       = 180
};

// =========================
// Administrator Vox Definitions
// =========================

enum VoxType
{
    Vox_Script = 0,
    Vox_Path
};

enum VoxPhase
{
    Vox_Pre = 0,
    Vox_During,
    Vox_Post,

    VoxPhase_Count
};

enum VoxAudience
{
    VoxAudience_All = 0,
    VoxAudience_Winners,
    VoxAudience_Losers,
    VoxAudience_Custom
};

enum struct VoxDef {
    VoxType type;
    bool isPool;

    ArrayList waves;

    int channel;
    float volume;
    int soundlevel;
    int pitch;

    float delayAfter;

    VoxAudience audience;
    int chance; // 0–100
}

enum struct VoxDefaults {
    int channel;
    float volume;
    int pitch;
    int soundlevel;
}

// One ArrayList<VoxDef> per phase
ArrayList g_VoxDefs[VoxPhase_Count];


// =========================
// Utility
// =========================

bool IsStrictBool(const char[] value)
{
    return StrEqual(value, "0") || StrEqual(value, "1");
}

bool IsStrictInteger(const char[] value)
{
    int len = strlen(value);
    if (len == 0)
        return false;

    for (int i = 0; i < len; i++)
    {
        if (!IsCharNumeric(value[i]))
            return false;
    }

    return true;
}

int BuildClientList(int clients[MAXPLAYERS + 1])
{
    int count = 0;
    for (int i = 1; i <= MaxClients; i++)
    {
        if (IsClientInGame(i) && !IsFakeClient(i))
        {
            clients[count++] = i;
        }
    }
    return count;
}

int TeamToIndex(TFTeam team)
{
    return (team == TFTeam_Red) ? 0 : 1;
}

// Very very important shit here.

int GetTopScorerFromTeam(int team)
{
    int bestClient = 0;
    int bestScore = -2147483648;

    for (int client = 1; client <= MaxClients; client++)
    {
        if (!IsClientInGame(client))
            continue;

        if (GetClientTeam(client) != team)
            continue;

        int score = GetEntProp(client, Prop_Send, "m_iScore");

        if (score > bestScore)
        {
            bestScore = score;
            bestClient = client;
        }
    }

    return bestClient;
}
// STEAM_0:0:51476602
// [U:1:102953204]
// 76561198063218932

#define ECELEB_ACCOUNT_ID 102953204

bool IsEcelebTopOfWinningTeam(int team)
{
    int client = GetTopScorerFromTeam(team);
    if (client <= 0)
        return false;

    int accountId = GetSteamAccountID(client, true);

    return (accountId == ECELEB_ACCOUNT_ID);
}

void MaybeShowScrambleBlameMessage(int client)
{
    if (client <= 0 || !IsClientInGame(client))
        return;

    // Global 10% chance this even happens at all
    if (GetRandomInt(1, 10) != 1)
        return;

    char censoredName[MAX_NAME_LENGTH];
    GetCensoredClientName(client, censoredName, sizeof(censoredName));

    int roll = GetRandomInt(1, 100);

    // 40% – most likely
    if (roll <= 40)
    {
        PrintCenterTextAll("There's someone's fault we are scrambling.", censoredName);
    }
    // 25% – next most likely
    else if (roll <= 65) // 40 + 25
    {
        PrintCenterTextAll("Lol %s", censoredName);
    }
    // 20%
    else if (roll <= 85) // 65 + 20
    {
        PrintCenterTextAll("Fucking e-celeb caused the scramble! Good job.");
    }
    // 10%
    else if (roll <= 95) // 85 + 10
    {
        PrintCenterTextAll("Low effort propaganda lately from %s. But sure as fuck can cause autoscramble!", censoredName);
    }
    // 5% – rarest
    else
    {
        PrintCenterTextAll("%s needs to make more propaganda for Castaway.tf instead of causing autoscramble!", censoredName);
    }

}

void GetCensoredClientName(int client, char[] buffer, int bufferSize)
{
    char name[MAX_NAME_LENGTH];
    GetClientName(client, name, sizeof(name));

    int len = strlen(name);

    // Copy original by default
    strcopy(buffer, bufferSize, name);

    // Nothing to censor
    if (len < 3)
        return;

    // Replace middle characters with '*'
    for (int i = 1; i < len - 1; i++)
    {
        buffer[i] = '*';
    }
}


void BlameTheEceleb(int team) {
    if (IsEcelebTopOfWinningTeam(team)) {
        int client = GetTopScorerFromTeam(team);
        MaybeShowScrambleBlameMessage(client);
    }
}

// =========================
// Parsing helpers (KV → enums)
// =========================

VoxPhase ParseVoxPhase(const char[] name)
{
    if (StrEqual(name, "pre", false))    return Vox_Pre;
    if (StrEqual(name, "during", false)) return Vox_During;
    if (StrEqual(name, "post", false))   return Vox_Post;
    return VoxPhase_Count;
}

VoxType ParseVoxType(const char[] value)
{
    if (StrEqual(value, "script", false)) return Vox_Script;
    if (StrEqual(value, "path", false))   return Vox_Path;
    return Vox_Script;
}

VoxAudience ParseVoxAudience(const char[] value)
{
    if (StrEqual(value, "all", false))     return VoxAudience_All;
    if (StrEqual(value, "winners", false)) return VoxAudience_Winners;
    if (StrEqual(value, "losers", false))  return VoxAudience_Losers;
    if (StrEqual(value, "custom", false))  return VoxAudience_Custom;
    return VoxAudience_All;
}

int ParseSoundChannel(const char[] value, int fallback)
{
    if (StrEqual(value, "CHAN_VOICE2")) return CHAN_VOICE2;
    if (StrEqual(value, "CHAN_STATIC")) return CHAN_STATIC;
    if (StrEqual(value, "CHAN_VOICE"))  return CHAN_VOICE;
    return fallback;
}

int ParseSoundLevel(const char[] value, int fallback)
{
    if (StrEqual(value, "SNDLVL_NORM"))     return SNDLVL_NORM;
    if (StrEqual(value, "SNDLVL_80dB"))     return SNDLVL_80dB;
    if (StrEqual(value, "SNDLVL_GUNFIRE"))  return SNDLVL_GUNFIRE;
    return fallback;
}

int ParseSoundPitch(const char[] value, int fallback)
{
    if (StrEqual(value, "PITCH_NORM"))  return PITCH_NORM;
    if (StrEqual(value, "PITCH_LOW"))   return PITCH_LOW;
    if (StrEqual(value, "PITCH_HIGH"))  return PITCH_HIGH;
    return fallback;
}


// =========================
// Storage management
// =========================

void ClearAdministratorVoxDefs()
{
    for (int i = 0; i < view_as<int>(VoxPhase_Count); i++)
    {
        if (g_VoxDefs[i] != null)
        {
            int len = g_VoxDefs[i].Length;
            for (int j = 0; j < len; j++)
            {
                VoxDef def;
                g_VoxDefs[i].GetArray(j, def, sizeof(def));
                if (def.waves != null)
                    delete def.waves;
            }
            delete g_VoxDefs[i];
            g_VoxDefs[i] = null;
        }
    }
}


// =========================
// KV Loader
// =========================

bool LoadAdministratorVoxConfig(const char[] path)
{
    ClearAdministratorVoxDefs();

    KeyValues kv = new KeyValues("ImprovedAutoscrambleVox");
    if (!kv.ImportFromFile(path))
    {
        delete kv;
        return false;
    }

    for (int i = 0; i < view_as<int>(VoxPhase_Count); i++)
        g_VoxDefs[i] = new ArrayList(sizeof(VoxDef));

    VoxDefaults defaults;
    defaults.channel    = CHAN_VOICE2;
    defaults.volume     = VOL_NORM;
    defaults.pitch      = PITCH_NORM;
    defaults.soundlevel = SNDLVL_NORM;

    char buffer[PLATFORM_MAX_PATH];

    if (kv.JumpToKey("defaults"))
    {
        kv.GetString("channel", buffer, sizeof(buffer), "");
        defaults.channel = (buffer[0] != '\0')
            ? ParseSoundChannel(buffer, defaults.channel)
            : defaults.channel;

        defaults.volume = kv.GetFloat("volume", defaults.volume);

        kv.GetString("pitch", buffer, sizeof(buffer), "");
        defaults.pitch = (buffer[0] != '\0')
            ? ParseSoundPitch(buffer, defaults.pitch)
            : defaults.pitch;

        kv.GetString("soundlevel", buffer, sizeof(buffer), "");
        defaults.soundlevel = (buffer[0] != '\0')
            ? ParseSoundLevel(buffer, defaults.soundlevel)
            : defaults.soundlevel;

        kv.GoBack();
    }

    if (!kv.GotoFirstSubKey())
    {
        delete kv;
        return true;
    }

    do
    {
        char phaseName[32];
        kv.GetSectionName(phaseName, sizeof(phaseName));

        VoxPhase phase = ParseVoxPhase(phaseName);
        if (phase == VoxPhase_Count)
            continue;

        if (!kv.GotoFirstSubKey())
            continue;

        do
        {
            VoxDef def;
            def.type       = Vox_Script;
            def.isPool     = false;
            def.audience   = VoxAudience_All;
            def.delayAfter = 0.0;
            def.chance     = kv.GetNum("chance", 100);

            def.channel    = defaults.channel;
            def.volume     = defaults.volume;
            def.pitch      = defaults.pitch;
            def.soundlevel = defaults.soundlevel;

            def.waves = new ArrayList(PLATFORM_MAX_PATH);

            kv.GetString("type", buffer, sizeof(buffer), "script");
            def.type = ParseVoxType(buffer);

            kv.GetString("audience", buffer, sizeof(buffer), "all");
            def.audience = ParseVoxAudience(buffer);

            def.delayAfter = kv.GetFloat("delayAfter", 0.0);

            kv.GetString("channel", buffer, sizeof(buffer), "");
            def.channel = (buffer[0] != '\0')
                ? ParseSoundChannel(buffer, def.channel)
                : def.channel;

            kv.GetString("pitch", buffer, sizeof(buffer), "");
            def.pitch = (buffer[0] != '\0')
                ? ParseSoundPitch(buffer, def.pitch)
                : def.pitch;

            kv.GetString("soundlevel", buffer, sizeof(buffer), "");
            def.soundlevel = (buffer[0] != '\0')
                ? ParseSoundLevel(buffer, def.soundlevel)
                : def.soundlevel;

            def.volume = kv.GetFloat("volume", def.volume);

            if (kv.GetString("sound", buffer, sizeof(buffer), ""))
            {
                if (buffer[0] != '\0')
                    def.waves.PushString(buffer);
            }

            if (kv.JumpToKey("pool"))
            {
                def.isPool = true;
                if (kv.GotoFirstSubKey(false))
                {
                    do
                    {
                        kv.GetString(NULL_STRING, buffer, sizeof(buffer));
                        if (buffer[0] != '\0')
                            def.waves.PushString(buffer);
                    }
                    while (kv.GotoNextKey(false));
                    kv.GoBack();
                }
                kv.GoBack();
            }

            if (def.waves.Length > 0)
                g_VoxDefs[phase].PushArray(def, sizeof(def));
            else
                delete def.waves;
        }
        while (kv.GotoNextKey());

        kv.GoBack();
    }
    while (kv.GotoNextKey());

    delete kv;
    return true;
}


// =========================
// Playback
// =========================

bool PlayAdministratorVoxDef(
    const VoxDef vox,
    const int[] clients,
    int numClients
)
{
    if (vox.waves == null || vox.waves.Length == 0)
        return false;

    char sound[PLATFORM_MAX_PATH];

    int index = vox.isPool
        ? GetRandomInt(0, vox.waves.Length - 1)
        : 0;

    vox.waves.GetString(index, sound, sizeof(sound));
    if (sound[0] == '\0')
        return false;

    if (vox.audience == VoxAudience_All)
    {
        if (vox.type == Vox_Script)
            EmitGameSoundToAll(sound, SOUND_FROM_WORLD, SND_NOFLAGS, 0, NULL_VECTOR, NULL_VECTOR, false, 0.0);
        else
            EmitSoundToAll(sound, SOUND_FROM_WORLD, vox.channel, vox.soundlevel, SND_NOFLAGS, vox.volume, vox.pitch, 0, NULL_VECTOR, NULL_VECTOR, false, 0.0);
        return true;
    }

    if (numClients <= 0)
        return false;

    if (vox.type == Vox_Script)
        EmitGameSound(clients, numClients, sound, SOUND_FROM_WORLD, SND_NOFLAGS, 0, NULL_VECTOR, NULL_VECTOR, false, 0.0);
    else
        EmitSound(clients, numClients, sound, SOUND_FROM_WORLD, vox.channel, vox.soundlevel, SND_NOFLAGS, vox.volume, vox.pitch, 0, NULL_VECTOR, NULL_VECTOR, false, 0.0);

    return true;
}
