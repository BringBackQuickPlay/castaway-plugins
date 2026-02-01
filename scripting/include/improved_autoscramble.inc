// administrator_vox.inc
// Timeline-based Administrator VOX system
// Supports sequential SEQ execution, grammar-safe pools, delays, stop times, and team-based audiences.

enum VoxPhase
{
    Vox_Pre = 0,
    Vox_Post,

    VoxPhase_Count
};

enum VoxEntryKind
{
    VoxEntry_Single = 0,
    VoxEntry_Pool
};

enum VoxAudience
{
    VoxAudience_All = 0,
    VoxAudience_Winners,
    VoxAudience_Losers,
    VoxAudience_Custom
};

enum struct VoxPoolEntry
{
    char sound[PLATFORM_MAX_PATH];
    int chance;
    float delay;
    float stopAt;
};

enum struct VoxSequence
{
    bool enabled;
    int chanceToPlay;
    VoxAudience audience;

    int channel;
    float volume;
    int pitch;
    int soundlevel;

    VoxEntryKind kind;

    char singleSound[PLATFORM_MAX_PATH];
    float singleDelay;
    float singleStopAt;

    bool poolRandom;
    ArrayList poolEntries;
};

enum struct VoxDefaults
{
    int channel;
    float volume;
    int pitch;
    int soundlevel;
};

ArrayList g_VoxSequences[VoxPhase_Count];

static VoxPhase ParseVoxPhase(const char[] name)
{
    if (StrEqual(name, "Pre", false))  return Vox_Pre;
    if (StrEqual(name, "Post", false)) return Vox_Post;
    return VoxPhase_Count;
}

static VoxAudience ParseAudience(const char[] value)
{
    if (StrEqual(value, "Winner", false) || StrEqual(value, "Winners", false))
        return VoxAudience_Winners;

    if (StrEqual(value, "Loser", false) || StrEqual(value, "Losers", false) ||
        StrEqual(value, "Looser", false) || StrEqual(value, "Loosers", false))
        return VoxAudience_Losers;

    return VoxAudience_All;
}

static int GetOpposingTeam(int winningTeam)
{
    if (winningTeam == 2) return 3;
    if (winningTeam == 3) return 2;
    return 0;
}

static int BuildAudienceClientList(VoxAudience audience, int winningTeam, int clients[MAXPLAYERS + 1])
{
    int count = 0;
    int losingTeam = GetOpposingTeam(winningTeam);

    for (int i = 1; i <= MaxClients; i++)
    {
        if (!IsClientInGame(i) || IsFakeClient(i))
            continue;

        int team = GetClientTeam(i);
        if (team <= 1)
            continue;

        switch (audience)
        {
            case VoxAudience_All:
                clients[count++] = i;
            case VoxAudience_Winners:
                if (team == winningTeam)
                    clients[count++] = i;
            case VoxAudience_Losers:
                if (team == losingTeam)
                    clients[count++] = i;
        }
    }

    return count;
}

void ClearAdministratorVox()
{
    for (int i = 0; i < VoxPhase_Count; i++)
    {
        if (g_VoxSequences[i] == null)
            continue;

        for (int j = 0; j < g_VoxSequences[i].Length; j++)
        {
            VoxSequence seq;
            g_VoxSequences[i].GetArray(j, seq, sizeof(seq));
            if (seq.poolEntries != null)
                delete seq.poolEntries;
        }

        delete g_VoxSequences[i];
        g_VoxSequences[i] = null;
    }
}

bool LoadAdministratorVoxConfig(const char[] path)
{
    ClearAdministratorVox();

    KeyValues kv = new KeyValues("AdministratorVox");
    if (!kv.ImportFromFile(path))
    {
        delete kv;
        return false;
    }

    for (int i = 0; i < VoxPhase_Count; i++)
        g_VoxSequences[i] = new ArrayList(sizeof(VoxSequence));

    VoxDefaults defaults;
    defaults.channel    = CHAN_VOICE2;
    defaults.volume     = VOL_NORM;
    defaults.pitch      = PITCH_NORM;
    defaults.soundlevel = SNDLVL_NORM;

    char buffer[PLATFORM_MAX_PATH];

    if (kv.JumpToKey("default_sound_settings"))
    {
        kv.GetString("channel", buffer, sizeof(buffer), "");
        if (buffer[0]) defaults.channel = ParseSoundChannel(buffer, defaults.channel);
        defaults.volume = kv.GetFloat("volume", defaults.volume);
        kv.GetString("pitch", buffer, sizeof(buffer), "");
        if (buffer[0]) defaults.pitch = ParseSoundPitch(buffer, defaults.pitch);
        kv.GetString("soundlevel", buffer, sizeof(buffer), "");
        if (buffer[0]) defaults.soundlevel = ParseSoundLevel(buffer, defaults.soundlevel);
        kv.GoBack();
    }

    kv.GotoFirstSubKey();
    do
    {
        char phaseName[32];
        kv.GetSectionName(phaseName, sizeof(phaseName));
        VoxPhase phase = ParseVoxPhase(phaseName);
        if (phase == VoxPhase_Count)
            continue;

        kv.GotoFirstSubKey();
        do
        {
            VoxSequence seq;
            seq.enabled      = kv.GetNum("Enabled", 0) != 0;
            seq.chanceToPlay = kv.GetNum("ChanceToPlay", -1);
            kv.GetString("TeamWinState", buffer, sizeof(buffer), "Global");
            seq.audience = ParseAudience(buffer);

            seq.channel    = defaults.channel;
            seq.volume     = defaults.volume;
            seq.pitch      = defaults.pitch;
            seq.soundlevel = defaults.soundlevel;

            seq.poolEntries = null;
            seq.singleSound[0] = '\0';
            seq.singleDelay = 0.0;
            seq.singleStopAt = 0.0;
            seq.poolRandom = false;

            if (kv.JumpToKey("Single"))
            {
                seq.kind = VoxEntry_Single;
                kv.GetString("sound", seq.singleSound, sizeof(seq.singleSound), "");
                seq.singleDelay = kv.GetFloat("Delay", 0.0);
                seq.singleStopAt = kv.GetFloat("StopAt", 0.0);
                kv.GoBack();
            }
            else if (kv.JumpToKey("Pool"))
            {
                seq.kind = VoxEntry_Pool;
                seq.poolRandom = kv.GetNum("IsPoolRnd", 0) != 0;
                seq.poolEntries = new ArrayList(sizeof(VoxPoolEntry));

                kv.GotoFirstSubKey();
                do
                {
                    VoxPoolEntry entry;
                    entry.chance = kv.GetNum("ChanceToPlayEntry", -1);
                    entry.delay = kv.GetFloat("Delay", 0.0);
                    entry.stopAt = kv.GetFloat("StopAt", 0.0);
                    kv.GetString("sound", entry.sound, sizeof(entry.sound), "");
                    if (entry.sound[0])
                        seq.poolEntries.PushArray(entry, sizeof(entry));
                }
                while (kv.GotoNextKey());
                kv.GoBack();
                kv.GoBack();
            }
            else
            {
                continue;
            }

            g_VoxSequences[phase].PushArray(seq, sizeof(seq));
        }
        while (kv.GotoNextKey());
        kv.GoBack();
    }
    while (kv.GotoNextKey());

    delete kv;
    return true;
}

static void EmitScheduledSound(any data)
{
    DataPack dp = view_as<DataPack>(data);
    dp.Reset();

    char sound[PLATFORM_MAX_PATH];
    dp.ReadString(sound, sizeof(sound));

    int channel = dp.ReadCell();
    int level   = dp.ReadCell();
    float volume = dp.ReadFloat();
    int pitch   = dp.ReadCell();
    float stopAt = dp.ReadFloat();

    int numClients = dp.ReadCell();
    int clients[MAXPLAYERS + 1];
    for (int i = 0; i < numClients; i++)
        clients[i] = dp.ReadCell();

    int ch, lvl, pt;
    float vol;
    char sample[PLATFORM_MAX_PATH];

    if (GetGameSoundParams(sound, ch, lvl, vol, pt, sample, sizeof(sample), SOUND_FROM_WORLD))
    {
        EmitGameSound(clients, numClients, sound,
            SOUND_FROM_WORLD, SND_NOFLAGS, 0, NULL_VECTOR, NULL_VECTOR, false, stopAt);
    }
    else
    {
        EmitSound(clients, numClients, sound,
            SOUND_FROM_WORLD, channel, level,
            SND_NOFLAGS, volume, pitch, 0,
            NULL_VECTOR, NULL_VECTOR, false, stopAt);
    }

    delete dp;
}

static const char[] VoxAudienceToString(VoxAudience a)
{
    switch (a)
    {
        case VoxAudience_Winners: return "Winner";
        case VoxAudience_Losers:  return "Loser";
        case VoxAudience_Custom:  return "Custom";
    }
    return "Global";
}


static const char[] VoxPhaseToString(VoxPhase p)
{
    switch (p)
    {
        case Vox_Pre:  return "Pre";
        case Vox_Post: return "Post";
    }
    return "Unknown";
}


public Action Command_DumpVox(int client, int args)
{
    if (client <= 0 || !IsClientInGame(client))
        return Plugin_Handled;

    PrintToConsole(client, "========== Administrator VOX Config Dump ==========");

    for (int phase = 0; phase < VoxPhase_Count; phase++)
    {
        ArrayList list = g_VoxSequences[phase];

        PrintToConsole(client, "Phase: %s", VoxPhaseToString(view_as<VoxPhase>(phase)));

        if (list == null || list.Length == 0)
        {
            PrintToConsole(client, "  (no sequences)");
            continue;
        }

        for (int i = 0; i < list.Length; i++)
        {
            VoxSequence seq;
            list.GetArray(i, seq, sizeof(seq));

            PrintToConsole(
                client,
                "  SEQ_%d | Enabled=%d | Chance=%d | Audience=%s",
                i + 1,
                seq.enabled,
                seq.chanceToPlay,
                VoxAudienceToString(seq.audience)
            );

            PrintToConsole(
                client,
                "        channel=%d volume=%.2f pitch=%d soundlevel=%d",
                seq.channel,
                seq.volume,
                seq.pitch,
                seq.soundlevel
            );

            if (seq.kind == VoxEntry_Single)
            {
                PrintToConsole(client, "        Single:");
                PrintToConsole(
                    client,
                    "          sound=\"%s\" Delay=%.2f StopAt=%.2f",
                    seq.singleSound,
                    seq.singleDelay,
                    seq.singleStopAt
                );
            }
            else
            {
                PrintToConsole(
                    client,
                    "        Pool: IsPoolRnd=%d Entries=%d",
                    seq.poolRandom,
                    seq.poolEntries != null ? seq.poolEntries.Length : 0
                );

                if (seq.poolEntries != null)
                {
                    for (int p = 0; p < seq.poolEntries.Length; p++)
                    {
                        VoxPoolEntry entry;
                        seq.poolEntries.GetArray(p, entry, sizeof(entry));

                        PrintToConsole(
                            client,
                            "          [%d] sound=\"%s\" Chance=%d Delay=%.2f StopAt=%.2f",
                            p + 1,
                            entry.sound,
                            entry.chance,
                            entry.delay,
                            entry.stopAt
                        );
                    }
                }
            }
        }
    }

    PrintToConsole(client, "===================================================");

    return Plugin_Handled;
}


void PlayAdministratorVox(VoxPhase phase, int winningTeam)
{
    ArrayList list = g_VoxSequences[phase];
    if (list == null || list.Length == 0)
        return;

    float t = 0.0;

    for (int i = 0; i < list.Length; i++)
    {
        VoxSequence seq;
        list.GetArray(i, seq, sizeof(seq));

        if (!seq.enabled)
            continue;

        if (seq.chanceToPlay > 0 && GetRandomInt(1, 100) > seq.chanceToPlay)
            continue;

        int clients[MAXPLAYERS + 1];
        int numClients = BuildAudienceClientList(seq.audience, winningTeam, clients);
        if (numClients <= 0)
            continue;

        if (seq.kind == VoxEntry_Single)
        {
            DataPack dp = new DataPack();
            dp.WriteString(seq.singleSound);
            dp.WriteCell(seq.channel);
            dp.WriteCell(seq.soundlevel);
            dp.WriteFloat(seq.volume);
            dp.WriteCell(seq.pitch);
            dp.WriteFloat(seq.singleStopAt);
            dp.WriteCell(numClients);
            for (int c = 0; c < numClients; c++)
                dp.WriteCell(clients[c]);

            CreateTimer(t, EmitScheduledSound, dp);
            t += seq.singleDelay;
        }
        else
        {
            if (seq.poolRandom)
            {
                VoxPoolEntry entry;
                seq.poolEntries.GetArray(GetRandomInt(0, seq.poolEntries.Length - 1), entry, sizeof(entry));

                DataPack dp = new DataPack();
                dp.WriteString(entry.sound);
                dp.WriteCell(seq.channel);
                dp.WriteCell(seq.soundlevel);
                dp.WriteFloat(seq.volume);
                dp.WriteCell(seq.pitch);
                dp.WriteFloat(entry.stopAt);
                dp.WriteCell(numClients);
                for (int c = 0; c < numClients; c++)
                    dp.WriteCell(clients[c]);

                CreateTimer(t, EmitScheduledSound, dp);
                t += entry.delay;
            }
            else
            {
                for (int p = 0; p < seq.poolEntries.Length; p++)
                {
                    VoxPoolEntry entry;
                    seq.poolEntries.GetArray(p, entry, sizeof(entry));

                    if (entry.chance > 0 && GetRandomInt(1, 100) > entry.chance)
                        continue;

                    DataPack dp = new DataPack();
                    dp.WriteString(entry.sound);
                    dp.WriteCell(seq.channel);
                    dp.WriteCell(seq.soundlevel);
                    dp.WriteFloat(seq.volume);
                    dp.WriteCell(seq.pitch);
                    dp.WriteFloat(entry.stopAt);
                    dp.WriteCell(numClients);
                    for (int c = 0; c < numClients; c++)
                        dp.WriteCell(clients[c]);

                    CreateTimer(t, EmitScheduledSound, dp);
                    t += entry.delay;
                }
            }
        }
    }
}