// ProjectThunderCE.asi -- GTA IV: The Complete Edition (1.2.0.59) + FusionFix
// ---------------------------------------------------------------------------
// Standalone port of ItsClonkAndre's Project Thunder (GPL-3.0) that needs no
// IV-SDK .NET (which only supports 1.0.7/1.0.8). v1.0 adds: blackouts near the
// electrical substations (building windows, street lamps and distant lights go
// out, the night gets darker, power-down sound), ped and player reactions,
// umbrella danger and scripted lightning. Stage 1:
//   * lightning bolts built from coronas (same DRAW_CORONA call the original
//     used through CoronaHelper: id/alpha 20/450/1.5/4.0, radius x10)
//   * the bolt lights the scene (DRAW_LIGHT_WITH_RANGE = AddSceneLight)
//   * the clouds of the LIGHTNING weather light up while a bolt is alive
//     (timecycle table, restored afterwards)
//   * thunder heard from the bolt's direction, delayed by the speed of sound,
//     quieter and duller with distance (bass.dll, loaded dynamically)
//   * optional ground-strike explosion, never during missions/cutscenes and
//     never close to the player
// Test keys (can be disabled): Ctrl+F10 force/release a storm, Ctrl+F11 bolt
// in front of the camera.
// Nothing here is written to the save file.
// ---------------------------------------------------------------------------

#include <windows.h>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <vector>
#include <string>
#include <algorithm>

#include "seh.inc"

#define PTCE_VERSION "1.7"

static HINSTANCE g_selfInst = nullptr;
static char g_logPath[MAX_PATH] = { 0 };
static char g_iniPath[MAX_PATH] = { 0 };
static char g_dataDir[MAX_PATH] = { 0 };      // <asi dir>\ProjectThunderCE
static uintptr_t g_moduleBase = 0;
static size_t    g_moduleSize = 0;

// ---- logging -----------------------------------------------------------------
static void Log(const char* fmt, ...)
{
    if (!g_logPath[0]) return;
    FILE* f = fopen(g_logPath, "a");
    if (!f) return;
    SYSTEMTIME t; GetLocalTime(&t);
    fprintf(f, "[%02d:%02d:%02d] ", t.wHour, t.wMinute, t.wSecond);
    va_list ap; va_start(ap, fmt); vfprintf(f, fmt, ap); va_end(ap);
    fprintf(f, "\n"); fclose(f);
}

// ---- settings ------------------------------------------------------------------
struct Settings
{
    // General
    int   enabled = 1;
    int   testKeys = 1;
    int   inCutscenes = 1;
    // LightningBolt
    float chanceBegin = 6.f, chanceOngoing = 15.f, chanceEnd = 4.f;
    float branchChance = 1.25f;
    int   minSize = 35, maxSize = 500;
    float minFade = 0.06f, maxFade = 0.25f;
    int   holdMin = 0, holdMax = 1000;        // ms a bolt stays on screen before it starts to fade
    float color[3] = { 0.766f, 0.9f, 1.0f };
    float coronaSize = 6000.f;
    float height = 500.f;
    int   maxCoronas = 350;
    float restrikeChance = 50.f;
    // Light
    int   lightEnabled = 1;
    float lightIntensity = 0.003f;
    float lightRange = 700.f;
    // Sky
    int   skyFlash = 1;
    float cloudBrightness = 20.f;
    // Sound
    int   soundEnabled = 1;
    float volume = 0.9f;
    float interiorReduction = 0.25f;
    float speedOfSound = 343.f;
    float delayMultiplier = 1.0f;
    float maxDelay = 12.f;
    float distantVolume = 0.35f;     // volume factor for a very distant bolt (1.0 = no attenuation)
    float distantMuffle = 1.0f;      // 0 = no muffling of distant thunder
    int   invertPan = 0;
    // Danger
    int   dangerHeight = 1;
    float dangerHeightZ = 189.f;
    float dangerChance = 3.f;
    // Explosion
    int   explosions = 1;
    int   explosionType = 2;
    float explosionRadius = 15.f;
    float explosionCamShake = 1.f;
    float explosionMinDist = 35.f;
    int   explosionsInMissions = 0;
    // Scripted lightning / umbrellas
    float scriptedChance = 30.f;
    int   umbrellaDanger = 1;
    float umbrellaChance = 0.1f;
    // Reactions
    int   playerReactions = 1;
    int   pedReactions = 1;
    float reactionChance = 25.f;
    // Blackout
    int   blackout = 1;
    float blackoutChanceDay = 5.f, blackoutChanceEvening = 7.5f;
    float substationRange = 30.f;
    int   blackoutMin = 25, blackoutMax = 80;
    int   blackoutSound = 1, blackoutSoundInterior = 0;
    float blackoutDarkness = 0.3f;
    int   blackoutInMissions = 0;
    int   blackoutFromExplosions = 1;
    int   blackoutLamps = 1;
    // 1.1
    float closeDistance = 800.f, farDistance = 1800.f;
    int   padRumble = 1; float rumbleDistance = 1200.f; float rumbleStrength = 1.0f;
    float tallTargetChance = 25.f; float tallTargetRadius = 900.f;
    int   carAlarms = 1; float carAlarmRadius = 45.f; float carAlarmChance = 60.f;
    float interiorMuffle = 0.9f;
    // 1.2
    int   sparks = 1; float sparksDuration = 6.f; float sparksVolume = 1.0f; int sparksLight = 1;
    int   radioStatic = 1; float staticDistance = 800.f; float staticVolume = 0.6f; int staticNeedsRadio = 1;
    // 1.3
    int   water = 1; float waterVolume = 1.0f;
    int   decor = 1; float decorChance = 12.f; int decorMax = 2; float decorMinDist = 1500.f, decorMaxDist = 4000.f;
    float decorCloudChance = 50.f; int decorThunder = 1;
    int   heat = 1; float heatChance = 6.f;
    int   echo = 1; float echoStrength = 1.0f;
    int   generators = 1; float genRadius = 70.f; int genLights = 1; int genSound = 1; float genVolume = 0.8f;
    int   dogs = 1; float dogChance = 35.f; float dogMaxDist = 900.f; float dogVolume = 0.2f;
    // 1.4
    float rainThunderChance = 0.f;
    int   smoke = 1; int smokeMin = 30, smokeMax = 60;
    int   stElmo = 1; float stElmoMinHeight = 70.f;
};
static Settings S;

static float IniF(const char* sec, const char* key, float def)
{
    char b[64], d[64]; snprintf(d, sizeof d, "%g", def);
    GetPrivateProfileStringA(sec, key, d, b, sizeof b, g_iniPath);
    return (float)atof(b);
}
static int IniI(const char* sec, const char* key, int def)
{
    return (int)GetPrivateProfileIntA(sec, key, def, g_iniPath);
}
static void LoadSettings()
{
    Settings d;
    S.enabled = IniI("General", "Enabled", d.enabled);
    S.testKeys = IniI("General", "TestKeys", d.testKeys);
    S.inCutscenes = IniI("General", "AllowLightningBoltsInCutscene", d.inCutscenes);

    S.chanceBegin = IniF("LightningBolt", "SpawnChancePercentageBeginning", d.chanceBegin);
    S.chanceOngoing = IniF("LightningBolt", "SpawnChancePercentageOngoing", d.chanceOngoing);
    S.chanceEnd = IniF("LightningBolt", "SpawnChancePercentageEnding", d.chanceEnd);
    S.branchChance = IniF("LightningBolt", "BranchSpawnChance", d.branchChance);
    S.minSize = IniI("LightningBolt", "MinSize", d.minSize);
    S.maxSize = IniI("LightningBolt", "MaxSize", d.maxSize);
    S.minFade = IniF("LightningBolt", "MinFadeOutSpeed", d.minFade);
    S.maxFade = IniF("LightningBolt", "MaxFadeOutSpeed", d.maxFade);
    {
        char b[64]; GetPrivateProfileStringA("LightningBolt", "ColorRGB", "0.766,0.9,1.0", b, sizeof b, g_iniPath);
        float c[3] = { d.color[0], d.color[1], d.color[2] };
        sscanf(b, "%f,%f,%f", &c[0], &c[1], &c[2]);
        for (int i = 0; i < 3; ++i) S.color[i] = c[i] < 0 ? 0 : (c[i] > 1 ? 1 : c[i]);
    }
    S.coronaSize = IniF("LightningBolt", "CoronaSize", d.coronaSize);
    S.height = IniF("LightningBolt", "Height", d.height);
    S.maxCoronas = IniI("LightningBolt", "MaxCoronasPerFrame", d.maxCoronas);
    S.holdMin = IniI("LightningBolt", "StayOnScreenMinMs", d.holdMin);
    S.holdMax = IniI("LightningBolt", "StayOnScreenMaxMs", d.holdMax);
    if (S.holdMin < 0) S.holdMin = 0;
    if (S.holdMax < S.holdMin) S.holdMax = S.holdMin;
    S.restrikeChance = IniF("LightningBolt", "RestrikeChance", d.restrikeChance);

    S.lightEnabled = IniI("Light", "Enabled", d.lightEnabled);
    S.lightIntensity = IniF("Light", "Intensity", d.lightIntensity);
    S.lightRange = IniF("Light", "Range", d.lightRange);

    S.skyFlash = IniI("Sky", "LightUpClouds", d.skyFlash);
    S.cloudBrightness = IniF("Sky", "AdditionalCloudBrightness", d.cloudBrightness);

    S.soundEnabled = IniI("Sound", "Enabled", d.soundEnabled);
    S.volume = IniF("Sound", "Volume", d.volume);
    S.interiorReduction = IniF("Sound", "LowerVolumeWhenInInterior", d.interiorReduction);
    S.speedOfSound = IniF("Sound", "SpeedOfSound", d.speedOfSound);
    S.delayMultiplier = IniF("Sound", "DelayMultiplier", d.delayMultiplier);
    S.maxDelay = IniF("Sound", "MaxDelaySeconds", d.maxDelay);
    S.distantVolume = IniF("Sound", "DistantVolume", d.distantVolume);
    S.distantMuffle = IniF("Sound", "DistantMuffle", d.distantMuffle);
    S.invertPan = IniI("Sound", "InvertLeftRight", d.invertPan);

    S.dangerHeight = IniI("Danger", "EnableDangerousHeight", d.dangerHeight);
    S.dangerHeightZ = IniF("Danger", "DangerousHeight", d.dangerHeightZ);
    S.dangerChance = IniF("Danger", "SpawnChanceWhenAboveDangerousHeight", d.dangerChance);

    S.explosions = IniI("Explosion", "CreateExplosions", d.explosions);
    S.explosionType = IniI("Explosion", "Type", d.explosionType);
    S.explosionRadius = IniF("Explosion", "Radius", d.explosionRadius);
    S.explosionCamShake = IniF("Explosion", "CamShake", d.explosionCamShake);
    S.explosionMinDist = IniF("Explosion", "MinDistanceToPlayer", d.explosionMinDist);
    S.explosionsInMissions = IniI("Explosion", "AllowDuringMissions", d.explosionsInMissions);

    S.scriptedChance = IniF("LightningBolt", "ScriptedSpawnChance", d.scriptedChance);
    S.umbrellaDanger = IniI("Danger", "EnableUmbrellaDanger", d.umbrellaDanger);
    S.umbrellaChance = IniF("Danger", "SpawnChanceWhenHoldingUmbrella", d.umbrellaChance);
    S.playerReactions = IniI("Reactions", "AllowPlayerReactions", d.playerReactions);
    S.pedReactions = IniI("Reactions", "AllowPedReactions", d.pedReactions);
    S.reactionChance = IniF("Reactions", "ReactionChance", d.reactionChance);
    S.blackout = IniI("Blackout", "Enabled", d.blackout);
    S.blackoutChanceDay = IniF("Blackout", "ChanceDay", d.blackoutChanceDay);
    S.blackoutChanceEvening = IniF("Blackout", "ChanceEvening", d.blackoutChanceEvening);
    S.substationRange = IniF("Blackout", "RangeAroundElectricalSubstation", d.substationRange);
    S.blackoutMin = IniI("Blackout", "MinActiveFor", d.blackoutMin);
    S.blackoutMax = IniI("Blackout", "MaxActiveFor", d.blackoutMax);
    S.blackoutSound = IniI("Blackout", "PlayBlackoutSound", d.blackoutSound);
    S.blackoutSoundInterior = IniI("Blackout", "CanPlaySoundWhenInInterior", d.blackoutSoundInterior);
    S.blackoutDarkness = IniF("Blackout", "AdditionalDarkness", d.blackoutDarkness);
    S.blackoutInMissions = IniI("Blackout", "AllowDuringMissions", d.blackoutInMissions);
    S.blackoutFromExplosions = IniI("Blackout", "ExplosionsCanTrigger", d.blackoutFromExplosions);
    S.blackoutLamps = IniI("Blackout", "TurnOffStreetLamps", d.blackoutLamps);
    S.closeDistance = IniF("Sound", "CloseThunderDistance", d.closeDistance);
    S.farDistance = IniF("Sound", "FarThunderDistance", d.farDistance);
    S.interiorMuffle = IniF("Sound", "InteriorMuffle", d.interiorMuffle);
    S.padRumble = IniI("Rumble", "Enabled", d.padRumble);
    S.rumbleDistance = IniF("Rumble", "MaxDistance", d.rumbleDistance);
    S.rumbleStrength = IniF("Rumble", "Strength", d.rumbleStrength);
    S.tallTargetChance = IniF("LightningBolt", "TallTargetChance", d.tallTargetChance);
    S.tallTargetRadius = IniF("LightningBolt", "TallTargetRadius", d.tallTargetRadius);
    S.carAlarms = IniI("CarAlarms", "Enabled", d.carAlarms);
    S.carAlarmRadius = IniF("CarAlarms", "Radius", d.carAlarmRadius);
    S.carAlarmChance = IniF("CarAlarms", "Chance", d.carAlarmChance);
    S.sparks = IniI("Sparks", "Enabled", d.sparks);
    S.sparksDuration = IniF("Sparks", "Duration", d.sparksDuration);
    S.sparksVolume = IniF("Sparks", "Volume", d.sparksVolume);
    S.sparksLight = IniI("Sparks", "Light", d.sparksLight);
    S.radioStatic = IniI("RadioStatic", "Enabled", d.radioStatic);
    S.staticDistance = IniF("RadioStatic", "MaxDistance", d.staticDistance);
    S.staticVolume = IniF("RadioStatic", "Volume", d.staticVolume);
    S.staticNeedsRadio = IniI("RadioStatic", "OnlyWhenRadioIsOn", d.staticNeedsRadio);
    S.water = IniI("Water", "Enabled", d.water);
    S.waterVolume = IniF("Water", "Volume", d.waterVolume);
    S.decor = IniI("DistantLightning", "Enabled", d.decor);
    S.decorChance = IniF("DistantLightning", "Chance", d.decorChance);
    S.decorMax = IniI("DistantLightning", "MaxAtOnce", d.decorMax);
    S.decorMinDist = IniF("DistantLightning", "MinDistance", d.decorMinDist);
    S.decorMaxDist = IniF("DistantLightning", "MaxDistance", d.decorMaxDist);
    S.decorCloudChance = IniF("DistantLightning", "CloudToCloudChance", d.decorCloudChance);
    S.decorThunder = IniI("DistantLightning", "Thunder", d.decorThunder);
    S.heat = IniI("DistantLightning", "HeatLightning", d.heat);
    S.heatChance = IniF("DistantLightning", "HeatLightningChance", d.heatChance);
    S.echo = IniI("Echo", "Enabled", d.echo);
    S.echoStrength = IniF("Echo", "Strength", d.echoStrength);
    S.generators = IniI("Generators", "Enabled", d.generators);
    S.genRadius = IniF("Generators", "Radius", d.genRadius);
    S.genLights = IniI("Generators", "EntranceLights", d.genLights);
    S.genSound = IniI("Generators", "Sound", d.genSound);
    S.genVolume = IniF("Generators", "Volume", d.genVolume);
    S.dogs = IniI("Dogs", "Enabled", d.dogs);
    S.dogChance = IniF("Dogs", "Chance", d.dogChance);
    S.dogMaxDist = IniF("Dogs", "MaxThunderDistance", d.dogMaxDist);
    S.dogVolume = IniF("Dogs", "Volume", d.dogVolume);
    S.rainThunderChance = IniF("Storms", "HeavyRainWithThunderChance", d.rainThunderChance);
    S.smoke = IniI("Smoke", "Enabled", d.smoke);
    S.smokeMin = IniI("Smoke", "MinSeconds", d.smokeMin);
    S.smokeMax = IniI("Smoke", "MaxSeconds", d.smokeMax);
    S.stElmo = IniI("StElmosFire", "Enabled", d.stElmo);
    S.stElmoMinHeight = IniF("StElmosFire", "MinHeight", d.stElmoMinHeight);
    if (S.smokeMax < S.smokeMin) S.smokeMax = S.smokeMin;
    if (S.decorMax < 1) S.decorMax = 1;
    if (S.decorMax > 6) S.decorMax = 6;
    if (S.decorMinDist < 300.f) S.decorMinDist = 300.f;
    if (S.decorMaxDist < S.decorMinDist) S.decorMaxDist = S.decorMinDist;
    if (S.sparksDuration < 0.5f) S.sparksDuration = 0.5f;
    if (S.sparksDuration > 60.f) S.sparksDuration = 60.f;
    if (S.staticDistance < 50.f) S.staticDistance = 50.f;
    if (S.blackoutMax < S.blackoutMin) S.blackoutMax = S.blackoutMin;
    if (S.blackoutDarkness < 0.f) S.blackoutDarkness = 0.f;
    if (S.blackoutDarkness > 0.9f) S.blackoutDarkness = 0.9f;

    if (S.maxSize < S.minSize) S.maxSize = S.minSize;
    if (S.maxSize > 2000) S.maxSize = 2000;
    if (S.maxFade < S.minFade) S.maxFade = S.minFade;
    if (S.speedOfSound < 10.f) S.speedOfSound = 10.f;
    if (S.maxCoronas < 10) S.maxCoronas = 10;
    if (S.maxCoronas > 600) S.maxCoronas = 600;   // the game's corona buffer holds 768 per frame
}

// ---- RNG -------------------------------------------------------------------------
static uint32_t g_rng = 0x9E3779B9u;
static uint32_t RndU() { uint32_t x = g_rng; x ^= x << 13; x ^= x >> 17; x ^= x << 5; return g_rng = x; }
static float Rnd01() { return (RndU() >> 8) * (1.0f / 16777216.0f); }
static float RndF(float a, float b) { return a + (b - a) * Rnd01(); }
static int   RndI(int a, int b) { return b <= a ? a : a + (int)(RndU() % (uint32_t)(b - a + 1)); }
static bool  Chance(float pct) { return Rnd01() * 100.f < pct; }

// ---- native invoker (same method as FusionFix; natives only on the sim thread) --------------
typedef void* (__stdcall* getNativeAddress_t)(uint32_t);
static getNativeAddress_t g_getNativeAddress = nullptr;
static uint32_t** g_pNatives = nullptr;
static uint32_t* g_pNativeSize = nullptr;

struct NativeCtx
{
    void* pReturn; uint32_t nArgCount; void* pArgs; uint32_t nDataCount;
    void* pOriginalData[4]; float tempData[16]; uint32_t stack[32];
    NativeCtx() { pReturn = stack; pArgs = stack; nArgCount = 0; nDataCount = 0;
        for (int i = 0; i < 4; i++) pOriginalData[i] = nullptr;
        for (int i = 0; i < 32; i++) stack[i] = 0;
        for (int i = 0; i < 16; i++) tempData[i] = 0.0f; }
    NativeCtx& I(int32_t v) { stack[nArgCount++] = (uint32_t)v; return *this; }
    NativeCtx& F(float v) { memcpy(&stack[nArgCount++], &v, 4); return *this; }
    NativeCtx& P(const void* v) { stack[nArgCount++] = (uint32_t)(uintptr_t)v; return *this; }
    int32_t resI() { return (int32_t)stack[0]; }
};
typedef void (*NativeFn)(NativeCtx*);

static NativeFn NatFnTable(uint32_t hash)
{
    if (!g_pNatives || !g_pNativeSize) return nullptr;
    FP_TRY {
        uint32_t* nat = *g_pNatives; uint32_t sz = *g_pNativeSize;
        if (!nat || !sz || !hash) return nullptr;
        uint32_t idx = hash % sz, tmp = hash, h = nat[2 * idx];
        if (h != hash) {
            while (h) { tmp = (tmp >> 1) + 1; idx = (tmp + idx) % sz; h = nat[2 * idx]; if (h == hash) break; }
        }
        if (h != hash) return nullptr;
        return (NativeFn)nat[2 * idx + 1];
    }
    FP_EXCEPT { return nullptr; }
    return nullptr;
}
static NativeFn NatFn(uint32_t hash)
{
    NativeFn f = NatFnTable(hash);
    if (f) return f;
    if (!g_getNativeAddress) return nullptr;
    FP_TRY { return (NativeFn)g_getNativeAddress(hash); }
    FP_EXCEPT { return nullptr; }
    return nullptr;
}
static bool Call(NativeFn f, NativeCtx& c)
{
    if (!f) return false;
    FP_TRY { f(&c); return true; }
    FP_EXCEPT { return false; }
    return false;
}

enum : uint32_t {
    H_GET_GAME_TIMER = 0x022B2DA9,
    H_GET_PLAYER_CHAR = 0x511454A9,
    H_IS_PLAYER_PLAYING = 0x08274BA4,
    H_GET_CHAR_COORDINATES = 0x2B5C06E6,
    H_GET_ROOT_CAM = 0x75E005F1,
    H_GET_CAM_POS = 0x60C22E93,
    H_GET_CAM_ROT = 0x51A06698,
    H_IS_PAUSE_MENU_ACTIVE = 0x6C4568A7,
    H_IS_INTERIOR_SCENE = 0x61DA102E,
    H_HAS_CUTSCENE_FINISHED = 0x4ECE1AD2,
    H_GET_TIME_OF_DAY = 0x384B3876,
    H_GET_CURRENT_WEATHER = 0x27E421EA,
    H_FORCE_WEATHER_NOW = 0x63737D31,
    H_RELEASE_WEATHER = 0x3A115D9D,
    H_DRAW_CORONA = 0x39ED0C43,
    H_DRAW_LIGHT_WITH_RANGE = 0x30D27EB1,
    H_GET_GROUND_Z_FOR_3D_COORD = 0x6D902EE3,
    H_ADD_EXPLOSION = 0x32DA5E3A,
    H_GET_MISSION_FLAG = 0x2BC64736,
    H_PRINT_NOW = 0x0CA539D6,           // PRINT_STRING_WITH_LITERAL_STRING_NOW
    H_IS_NETWORK_SESSION = 0x6E2B38F3,
    H_GET_GAME_CAM = 0x0B2A2801,
    H_SAY_AMBIENT_SPEECH = 0x5CF149C8,
    H_TASK_LOOK_AT_COORD = 0x26E27605,
    H_TASK_PLAY_ANIM_SEC_UPPER = 0x34574B2A,
    H_HAVE_ANIMS_LOADED = 0x1D3F681D,
    H_IS_PED_A_MISSION_PED = 0x05801768,
    H_IS_CHAR_DEAD = 0x6A6B4F18,
    H_IS_CHAR_IN_ANY_CAR = 0x71184DA3,
    H_GET_OBJECT_PED_IS_HOLDING = 0x45345838,
    H_GET_OBJECT_MODEL = 0x5CC55619,
    H_IS_EXPLOSION_IN_SPHERE = 0x47A77D2E,
    H_CAM_IS_SPHERE_VISIBLE = 0x2D5611D4,
    H_GET_INTERIOR_FROM_CHAR = 0x028227F7,
    H_SHAKE_PAD = 0x66CC16BD,
    H_TRIGGER_VEH_ALARM = 0x5E5047AC,
    H_GET_DRIVER_OF_CAR = 0x22457083,
    H_IS_CAR_A_MISSION_CAR = 0x7A422E14,
    H_IS_CAR_DEAD = 0x2AAB340A,
    H_TRIGGER_PTFX = 0x21C44026,
    H_GET_PLAYER_RADIO_STATION_INDEX = 0x4E493AAF,
    H_GET_WATER_HEIGHT = 0x2BB9620F,
    H_SET_NO_RESPRAYS = 0x418D0889,
    H_IS_PAY_N_SPRAY_ACTIVE = 0x1EE70376,
    H_SET_TRAIN_SPEED = 0x3F4950AC,
    H_GET_CAR_MODEL = 0x5FF84497,
    H_IS_THIS_MODEL_A_TRAIN = 0x7B8537F7,
    H_GET_CAR_SPEED = 0x16DD2D00,
};
static NativeFn nTimer, nPlayerChar, nPlayerPlaying, nCharCoords, nRootCam, nCamPos, nCamRot, nPauseMenu,
    nInterior, nCutsceneDone, nTimeOfDay, nCurWeather, nForceWeatherNow, nReleaseWeather, nCorona, nLight,
    nGroundZ, nExplosion, nMissionFlag, nPrintNow, nNetSession, nGameCam,
    nSay, nLookAt, nPlayAnim, nAnimsLoaded, nMissionPed, nCharDead, nInAnyCar, nHolding, nObjModel, nExplInSphere, nCamSphereVis, nIntFromChar,
    nShakePad, nVehAlarm, nDriverOfCar, nMissionCar, nCarDead, nTriggerPtfx, nRadioIdx, nWaterH,
    nNoResprays, nPnsActive, nTrainSpeed, nCarModel, nIsTrain, nCarSpeed;
static bool g_natsResolved = false;
static void ResolveNatives()
{
    nTimer = NatFn(H_GET_GAME_TIMER); nPlayerChar = NatFn(H_GET_PLAYER_CHAR); nPlayerPlaying = NatFn(H_IS_PLAYER_PLAYING);
    nCharCoords = NatFn(H_GET_CHAR_COORDINATES); nRootCam = NatFn(H_GET_ROOT_CAM); nCamPos = NatFn(H_GET_CAM_POS);
    nCamRot = NatFn(H_GET_CAM_ROT); nPauseMenu = NatFn(H_IS_PAUSE_MENU_ACTIVE); nInterior = NatFn(H_IS_INTERIOR_SCENE);
    nCutsceneDone = NatFn(H_HAS_CUTSCENE_FINISHED); nTimeOfDay = NatFn(H_GET_TIME_OF_DAY); nCurWeather = NatFn(H_GET_CURRENT_WEATHER);
    nForceWeatherNow = NatFn(H_FORCE_WEATHER_NOW); nReleaseWeather = NatFn(H_RELEASE_WEATHER); nCorona = NatFn(H_DRAW_CORONA);
    nLight = NatFn(H_DRAW_LIGHT_WITH_RANGE); nGroundZ = NatFn(H_GET_GROUND_Z_FOR_3D_COORD); nExplosion = NatFn(H_ADD_EXPLOSION);
    nMissionFlag = NatFn(H_GET_MISSION_FLAG); nPrintNow = NatFn(H_PRINT_NOW); nNetSession = NatFn(H_IS_NETWORK_SESSION); nGameCam = NatFn(H_GET_GAME_CAM);
    nSay = NatFn(H_SAY_AMBIENT_SPEECH); nLookAt = NatFn(H_TASK_LOOK_AT_COORD); nPlayAnim = NatFn(H_TASK_PLAY_ANIM_SEC_UPPER);
    nAnimsLoaded = NatFn(H_HAVE_ANIMS_LOADED); nMissionPed = NatFn(H_IS_PED_A_MISSION_PED); nCharDead = NatFn(H_IS_CHAR_DEAD);
    nInAnyCar = NatFn(H_IS_CHAR_IN_ANY_CAR); nHolding = NatFn(H_GET_OBJECT_PED_IS_HOLDING); nObjModel = NatFn(H_GET_OBJECT_MODEL);
    nExplInSphere = NatFn(H_IS_EXPLOSION_IN_SPHERE); nCamSphereVis = NatFn(H_CAM_IS_SPHERE_VISIBLE); nIntFromChar = NatFn(H_GET_INTERIOR_FROM_CHAR);
    nShakePad = NatFn(H_SHAKE_PAD); nVehAlarm = NatFn(H_TRIGGER_VEH_ALARM); nDriverOfCar = NatFn(H_GET_DRIVER_OF_CAR);
    nMissionCar = NatFn(H_IS_CAR_A_MISSION_CAR); nCarDead = NatFn(H_IS_CAR_DEAD);
    nTriggerPtfx = NatFn(H_TRIGGER_PTFX); nRadioIdx = NatFn(H_GET_PLAYER_RADIO_STATION_INDEX); nWaterH = NatFn(H_GET_WATER_HEIGHT);
    nNoResprays = NatFn(H_SET_NO_RESPRAYS); nPnsActive = NatFn(H_IS_PAY_N_SPRAY_ACTIVE); nTrainSpeed = NatFn(H_SET_TRAIN_SPEED);
    nCarModel = NatFn(H_GET_CAR_MODEL); nIsTrain = NatFn(H_IS_THIS_MODEL_A_TRAIN); nCarSpeed = NatFn(H_GET_CAR_SPEED);
    g_natsResolved = true;
    Log("natives: timer=%p char=%p corona=%p light=%p groundZ=%p explosion=%p weather=%p force=%p",
        nTimer, nPlayerChar, nCorona, nLight, nGroundZ, nExplosion, nCurWeather, nForceWeatherNow);
}

// ---- pattern scanning --------------------------------------------------------------------------
static bool GetMainModuleRange()
{
    HMODULE h = GetModuleHandleA(nullptr);
    auto dos = (IMAGE_DOS_HEADER*)h;
    auto nt = (IMAGE_NT_HEADERS*)((uint8_t*)h + dos->e_lfanew);
    g_moduleBase = (uintptr_t)h; g_moduleSize = nt->OptionalHeader.SizeOfImage;
    return true;
}
static bool GetTextRange(uintptr_t& lo, uintptr_t& hi)
{
    auto dos = (IMAGE_DOS_HEADER*)g_moduleBase;
    auto nt = (IMAGE_NT_HEADERS*)(g_moduleBase + dos->e_lfanew);
    auto sec = IMAGE_FIRST_SECTION(nt);
    for (unsigned i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++sec)
        if (sec->Characteristics & IMAGE_SCN_MEM_EXECUTE)
        { lo = g_moduleBase + sec->VirtualAddress; hi = lo + sec->Misc.VirtualSize; return true; }
    return false;
}
// "A1 ? ? ? ? 83 C4" style pattern, searched in .text; returns all hits
static int FindAll(const char* sig, uintptr_t* out, int maxHits)
{
    uint8_t pat[128]; bool wild[128]; int n = 0;
    for (const char* p = sig; *p && n < 128;)
    {
        while (*p == ' ') ++p;
        if (!*p) break;
        if (*p == '?') { wild[n] = true; pat[n++] = 0; ++p; if (*p == '?') ++p; }
        else { wild[n] = false; pat[n++] = (uint8_t)strtoul(p, nullptr, 16); p += 2; }
    }
    uintptr_t lo, hi; if (!GetTextRange(lo, hi)) return 0;
    int hits = 0;
    for (uintptr_t a = lo; a + n <= hi; ++a)
    {
        const uint8_t* m = (const uint8_t*)a; int j = 0;
        for (; j < n; ++j) if (!wild[j] && m[j] != pat[j]) break;
        if (j == n) { if (hits < maxHits) out[hits] = a; ++hits; }
    }
    return hits;
}
static uintptr_t FindFirst(const char* sig) { uintptr_t h[1] = { 0 }; return FindAll(sig, h, 1) ? h[0] : 0; }

// ---- game globals (FusionFix patterns, verified on 1.2.0.59) --------------------------------------
static volatile uint32_t* g_oldWeather = nullptr;
static volatile uint32_t* g_newWeather = nullptr;
static volatile float* g_weatherInterp = nullptr;
static uint8_t* g_timecycle = nullptr;          // TimeCycleParams mParams[11][9], 528 bytes each
enum { TC_HOURS = 11, TC_WEATHERS = 9, TC_STRIDE = 528, TC_CLOUDS_BRIGHTNESS = 432, TC_CLOUD1 = 272, TC_CLOUD2 = 304, TC_CLOUD3 = 384 };
static const int kTcHours[TC_HOURS] = { 0, 5, 6, 7, 9, 12, 18, 19, 20, 21, 22 };
enum { W_LIGHTNING = 7 };

static uint8_t** g_pedPoolPtr = nullptr;
static uint8_t** g_vehPoolPtr = nullptr;      // CPool<CPed>* (storage @0, flags @4, size @8, stride @0xC)
static uint32_t** g_miTable = nullptr;        // -> sorted (hash, index) pairs used by CModelInfo::GetModelInfo
static uint16_t* g_miCount = nullptr;
static uint8_t*** g_miPtrsCode = nullptr;     // address of the modelinfo pointer array (read from the code)
static uintptr_t g_miPtrsBase = 0;
static volatile int g_lampsOff = 0;           // blackout: skip the 2dfx light/corona pass of world objects
static uintptr_t g_orig2dfx = 0;
static uintptr_t g_timeVt = 0;               // CTimeModelInfo vtable
static uint32_t* g_restartCnt[2] = { nullptr, nullptr };   // CRestart: hospitals, police stations
static float* g_restartPts[2] = { nullptr, nullptr };      // CVector_pad[10] each
static uint8_t** g_bldPoolPtr = nullptr;                   // CPool<CBuilding>*
static volatile int32_t* g_coronaCount = nullptr;          // coronas registered so far this frame
static int32_t g_coronaMax = 0x300;                         // size of the game's per-frame corona buffer

__declspec(naked) void Gate2dfxStub()
{
    __asm
    {
        cmp   dword ptr[g_lampsOff], 0
        jne   skip
        jmp   dword ptr[g_orig2dfx]
    skip:
        ret
    }
}

// ---- blackout gates (ported from the original's AddSceneLight / RenderCorona / GetTrafficLightState hooks) ----
// coronas: every glow halo the game registers (street lamps, signs, traffic lights, windows) goes through one
// function. During a blackout it drops them all, except car lights (their id points inside the vehicle) and
// DRAW_CORONA from scripts -- that call site is redirected past the gate, so the bolts always render.
static volatile int g_coronaGate = 0;
static bool g_coronaGateOk = false;
static uintptr_t g_coronaFn = 0, g_coronaResume = 0;
static volatile uintptr_t g_vehLo = 0, g_vehSpan = 0;
__declspec(naked) void CoronaRawStub()
{
    __asm
    {
        mov   edx, dword ptr[g_coronaCount]
        mov   edx, dword ptr[edx]
        jmp   dword ptr[g_coronaResume]
    }
}
__declspec(naked) void CoronaGateStub()
{
    __asm
    {
        cmp   dword ptr[g_coronaGate], 0
        je    cg_pass
        mov   eax, dword ptr[esp + 4]
        sub   eax, dword ptr[g_vehLo]
        cmp   eax, dword ptr[g_vehSpan]
        jb    cg_pass
        ret
    cg_pass:
        mov   edx, dword ptr[g_coronaCount]
        mov   edx, dword ptr[edx]
        jmp   dword ptr[g_coronaResume]
    }
}
// traffic lights: both state functions start with "mov eax,[timer]"; during a blackout they answer 3 (= off)
static volatile int g_tlOff = 0;
static uintptr_t g_tlTimer = 0, g_tl1Resume = 0, g_tl2Resume = 0;
__declspec(naked) void TrafficLight1Stub()
{
    __asm
    {
        cmp   dword ptr[g_tlOff], 0
        je    t1_pass
        mov   eax, 3
        ret
    t1_pass:
        mov   eax, dword ptr[g_tlTimer]
        mov   eax, dword ptr[eax]
        jmp   dword ptr[g_tl1Resume]
    }
}
__declspec(naked) void TrafficLight2Stub()
{
    __asm
    {
        cmp   dword ptr[g_tlOff], 0
        je    t2_pass
        mov   eax, 3
        ret
    t2_pass:
        mov   eax, dword ptr[g_tlTimer]
        mov   eax, dword ptr[eax]
        jmp   dword ptr[g_tl2Resume]
    }
}
static bool PatchJmp(uintptr_t at, void* to, int len)
{
    DWORD op;
    if (!VirtualProtect((void*)at, len, PAGE_EXECUTE_READWRITE, &op)) return false;
    uint8_t* p = (uint8_t*)at;
    p[0] = 0xE9; *(int32_t*)(p + 1) = (int32_t)((uintptr_t)to - (at + 5));
    for (int i = 5; i < len; ++i) p[i] = 0x90;
    VirtualProtect((void*)at, len, op, &op);
    FlushInstructionCache(GetCurrentProcess(), (void*)at, len);
    return true;
}
static uintptr_t FollowJmps(uintptr_t a)
{
    for (int i = 0; i < 4; ++i)
    {
        const uint8_t* p = (const uint8_t*)a;
        if (p[0] == 0xE9) a = a + 5 + *(const int32_t*)(p + 1);
        else if (p[0] == 0xFF && p[1] == 0x25) a = **(uintptr_t**)(p + 2);
        else break;
    }
    return a;
}
// Called once natives are known: point DRAW_CORONA's internal call past the gate, then arm the gate hook.
static void InstallCoronaGate()
{
    if (g_coronaGateOk || !g_coronaFn || !g_coronaCount || !nCorona) return;
    FP_TRY {
        uintptr_t h = FollowJmps((uintptr_t)nCorona), impl = 0;
        for (int o = 0; o < 0x80 && !impl; ++o)
            if (((uint8_t*)h)[o] == 0xE8) impl = FollowJmps(h + o + 5 + *(int32_t*)(h + o + 1));
        uintptr_t site = 0;
        if (impl) for (int o = 0; o < 0x400 && !site; ++o)
        {
            uint8_t* p = (uint8_t*)impl + o;
            if (p[0] == 0xE8 && impl + o + 5 + *(int32_t*)(p + 1) == g_coronaFn) site = impl + o;
        }
        const uint8_t* f = (const uint8_t*)g_coronaFn;
        if (!site || f[0] != 0x8B || f[1] != 0x15) { Log("corona gate: not installed (handler %p impl %p site %p)", (void*)h, (void*)impl, (void*)site); return; }
        DWORD op; VirtualProtect((void*)site, 5, PAGE_EXECUTE_READWRITE, &op);
        *(int32_t*)(site + 1) = (int32_t)((uintptr_t)&CoronaRawStub - (site + 5));
        VirtualProtect((void*)site, 5, op, &op); FlushInstructionCache(GetCurrentProcess(), (void*)site, 5);
        g_coronaResume = g_coronaFn + 6;
        if (!PatchJmp(g_coronaFn, (void*)&CoronaGateStub, 6)) { Log("corona gate: patch failed"); return; }
        g_coronaGateOk = true;
        Log("corona gate at %p (DRAW_CORONA call %p bypasses it)", (void*)g_coronaFn, (void*)site);
    }
    FP_EXCEPT { Log("corona gate: faulted"); }
}

static void ResolveGlobals()
{
    uintptr_t m;
    if ((m = FindFirst("A1 ? ? ? ? 83 C4 08 8B CF")) != 0) g_oldWeather = *(uint32_t**)(m + 1);
    if ((m = FindFirst("A1 ? ? ? ? 89 46 4C A1")) != 0) g_newWeather = *(uint32_t**)(m + 1);
    if ((m = FindFirst("F3 0F 10 05 ? ? ? ? 8B 44 24 0C 8B 4C 24 04")) != 0) g_weatherInterp = *(float**)(m + 4);
    if ((m = FindFirst("05 ? ? ? ? 50 8D 4C 24 60")) != 0) g_timecycle = *(uint8_t**)(m + 1);
    Log("weather old=%p new=%p interp=%p timecycle=%p", g_oldWeather, g_newWeather, g_weatherInterp, g_timecycle);

    // getNativeAddress: 2nd .text match of "56 8B 35 ? ? ? ? 85 F6 75 06"; read its globals
    uintptr_t hits[8];
    int n = FindAll("56 8B 35 ? ? ? ? 85 F6 75 06", hits, 8);
    if (n >= 2) g_getNativeAddress = (getNativeAddress_t)hits[1];
    else if (n == 1) g_getNativeAddress = (getNativeAddress_t)hits[0];
    if (g_getNativeAddress)
    {
        uint8_t* g = (uint8_t*)g_getNativeAddress;
        if (g[0] == 0x56 && g[1] == 0x8B && g[2] == 0x35) g_pNativeSize = *(uint32_t**)(g + 3);
        for (int off = 8; off < 0x40; ++off)
            if (g[off] == 0x8B && g[off + 1] == 0x1D) { g_pNatives = *(uint32_t***)(g + off + 2); break; }
    }
    Log("getNativeAddress=%p (hits %d) natives=%p size=%p", g_getNativeAddress, n, g_pNatives, g_pNativeSize);

    if ((m = FindFirst("8B 3D ? ? ? ? 8B F1 8B 47")) != 0) g_pedPoolPtr = *(uint8_t***)(m + 2);
    if ((m = FindFirst("8B 15 ? ? ? ? 46 3B 72 ? 7C ? 5E")) != 0) g_vehPoolPtr = *(uint8_t***)(m + 2);
    {
        uintptr_t h2[4]; int k = FindAll("8B 44 24 04 89 44 24 04 66 A1 ? ? ? ? 66 85 C0 74 ? 68 ? ? ? ? 0F B7 C0 6A 08 50 FF 35", h2, 4);
        for (int i = 0; i < k && i < 4 && !g_miPtrsBase; ++i)
        {
            uint8_t* f = (uint8_t*)h2[i];
            for (int o = 32; o < 0x60; ++o)
                if (f[o] == 0x8B && f[o + 1] == 0x04 && f[o + 2] == 0x85)
                {
                    g_miCount = *(uint16_t**)(f + 10);
                    g_miTable = *(uint32_t***)(f + 32);
                    g_miPtrsBase = *(uintptr_t*)(f + o + 3);
                    break;
                }
        }
    }
    Log("pedPool=%p vehPool=%p modelTable=%p count=%p modelPtrs=%p", g_pedPoolPtr, g_vehPoolPtr, g_miTable, g_miCount, (void*)g_miPtrsBase);

    // CTimeModelInfo vtable: [3] = "mov al,3; ret" (GetModelType), [5] = "lea eax,[ecx+60h]; ret" (GetTimeInfo).
    // Time models are then recognised by their vtable -- no virtual calls into the game.
    FP_TRY {
        uintptr_t lo, hi; GetTextRange(lo, hi);
        std::vector<uintptr_t> getType;
        for (uintptr_t a = lo; a + 3 <= hi; ++a) { const uint8_t* q = (const uint8_t*)a; if (q[0] == 0xB0 && q[1] == 0x03 && q[2] == 0xC3) getType.push_back(a); }
        auto dos = (IMAGE_DOS_HEADER*)g_moduleBase; auto nt = (IMAGE_NT_HEADERS*)(g_moduleBase + dos->e_lfanew);
        auto sec = IMAGE_FIRST_SECTION(nt);
        for (unsigned si = 0; si < nt->FileHeader.NumberOfSections && !g_timeVt; ++si, ++sec)
        {
            if (sec->Characteristics & IMAGE_SCN_MEM_EXECUTE) continue;
            const uint8_t* sb = (const uint8_t*)(g_moduleBase + sec->VirtualAddress);
            size_t sz = sec->SizeOfRawData < sec->Misc.VirtualSize ? sec->SizeOfRawData : sec->Misc.VirtualSize;
            for (size_t off = 0; off + 24 <= sz && !g_timeVt; off += 4)
            {
                uintptr_t v3 = *(const uintptr_t*)(sb + off + 12);
                bool match = false; for (uintptr_t gt : getType) if (gt == v3) { match = true; break; }
                if (!match) continue;
                uintptr_t v5 = *(const uintptr_t*)(sb + off + 20);
                if (v5 < lo || v5 + 4 > hi) continue;
                const uint8_t* q = (const uint8_t*)v5;
                if (q[0] == 0x8D && q[1] == 0x41 && q[2] == 0x60 && q[3] == 0xC3) g_timeVt = (uintptr_t)(sb + off);
            }
        }
    }
    FP_EXCEPT { g_timeVt = 0; }
    Log("time-model vtable %p", (void*)g_timeVt);

    // the game's corona buffer (768 per frame, shared with street lights, car lights...): when it is full,
    // further coronas are dropped -- that cut the bottom of the bolts off. Read the fill level and fit the bolt.
    if ((m = FindFirst("8B 15 ? ? ? ? 56 8D 72 01 81 FE ? ? 00 00")) != 0)
    {
        g_coronaCount = *(int32_t**)(m + 2);
        int32_t mx = *(int32_t*)(m + 12); if (mx > 64 && mx < 100000) g_coronaMax = mx;
        if (*(const uint8_t*)(m - 1) == 0xCC || *(const uint8_t*)(m - 1) == 0xC3) g_coronaFn = m;   // the pattern is the function's first instruction
    }
    Log("corona buffer: count %p, size %d, register fn %p", (void*)g_coronaCount, g_coronaMax, (void*)g_coronaFn);

    // traffic light state functions: "mov eax,[timer]; add eax,[esp+8]; xor edx,edx; cmp [esp+4],dl"
    {
        uintptr_t tl[4]; int k = FindAll("A1 ? ? ? ? 03 44 24 08 33 D2 38 54 24 04", tl, 4);
        if (k == 2 && *(uint32_t*)(tl[0] + 1) == *(uint32_t*)(tl[1] + 1))
        {
            g_tlTimer = *(uint32_t*)(tl[0] + 1);
            g_tl1Resume = tl[0] + 5; g_tl2Resume = tl[1] + 5;
            bool ok = PatchJmp(tl[0], (void*)&TrafficLight1Stub, 5) && PatchJmp(tl[1], (void*)&TrafficLight2Stub, 5);
            Log("traffic lights: %s (%p, %p)", ok ? "hooked" : "patch failed", (void*)tl[0], (void*)tl[1]);
        }
        else Log("traffic lights: not found (%d match(es)) -- they stay on during blackouts", k);
    }

    // CRestart::AddHospitalRestart / AddPoliceRestart (same code, their counters are consecutive globals)
    {
        uintptr_t h3[6]; int k = FindAll("8B 4C 24 04 56 8B 35 ? ? ? ? F3 0F 10 41 04 8B 01 F3 0F 10 49 08 8B D6 C1 E2 04 81 C2", h3, 6);
        FP_TRY {
            for (int i = 0; i < k && !g_restartCnt[0]; ++i) for (int j = 0; j < k; ++j)
            {
                uint32_t* ci = *(uint32_t**)(h3[i] + 7); uint32_t* cj = *(uint32_t**)(h3[j] + 7);
                if ((uintptr_t)cj == (uintptr_t)ci + 4)
                {
                    g_restartCnt[0] = ci; g_restartPts[0] = *(float**)(h3[i] + 0x1E);
                    g_restartCnt[1] = cj; g_restartPts[1] = *(float**)(h3[j] + 0x1E);
                    break;
                }
            }
        }
        FP_EXCEPT { g_restartCnt[0] = g_restartCnt[1] = nullptr; }
        Log("restart points: %d hit(s), hospitals %p/%p, police %p/%p", k, g_restartCnt[0], g_restartPts[0], g_restartCnt[1], g_restartPts[1]);
    }
    // building pool: CBuilding's deleting destructor (found through its RTTI) frees into it
    FP_TRY {
        uintptr_t lo, hi; GetTextRange(lo, hi);
        auto dos = (IMAGE_DOS_HEADER*)g_moduleBase; auto nt = (IMAGE_NT_HEADERS*)(g_moduleBase + dos->e_lfanew);
        struct Sec { const uint8_t* b; size_t n; }; std::vector<Sec> data;
        auto sec = IMAGE_FIRST_SECTION(nt);
        for (unsigned si = 0; si < nt->FileHeader.NumberOfSections; ++si, ++sec)
        {
            if (sec->Characteristics & IMAGE_SCN_MEM_EXECUTE) continue;
            size_t sz = sec->SizeOfRawData < sec->Misc.VirtualSize ? sec->SizeOfRawData : sec->Misc.VirtualSize;
            data.push_back({ (const uint8_t*)(g_moduleBase + sec->VirtualAddress), sz });
        }
        static const char kName[] = ".?AVCBuilding@@";
        uintptr_t td = 0, col = 0, vt = 0;
        for (auto& d : data) for (size_t o = 8; o + sizeof kName <= d.n && !td; ++o)
            if (d.b[o] == '.' && !memcmp(d.b + o, kName, sizeof kName)) td = (uintptr_t)(d.b + o - 8);
        if (td) for (auto& d : data) for (size_t o = 0; o + 20 <= d.n && !col; o += 4)
            if (*(const uint32_t*)(d.b + o) == 0 && *(const uintptr_t*)(d.b + o + 12) == td) col = (uintptr_t)(d.b + o);
        if (col) for (auto& d : data) for (size_t o = 0; o + 8 <= d.n && !vt; o += 4)
            if (*(const uintptr_t*)(d.b + o) == col)
            {
                uintptr_t f = *(const uintptr_t*)(d.b + o + 4);
                if (f >= lo && f < hi) vt = (uintptr_t)(d.b + o + 4);
            }
        if (vt)
        {
            const uint8_t* dtor = *(const uint8_t**)vt;
            for (int o = 0; o < 0x30; ++o)
                if (dtor[o] == 0x8B && dtor[o + 1] == 0x0D) { g_bldPoolPtr = *(uint8_t***)(dtor + o + 2); break; }
        }
        Log("CBuilding vtable %p, building pool %p", (void*)vt, g_bldPoolPtr);
    }
    FP_EXCEPT { g_bldPoolPtr = nullptr; Log("building pool lookup faulted"); }

    // street lamps / building lights: the per-object 2dfx light pass (lights + coronas) has one call site
    if ((m = FindFirst("50 57 E8 ? ? ? ? 83 C4 34 C6 44 24 17 01")) != 0)
    {
        uintptr_t call = m + 2;
        uintptr_t tgt = call + 5 + *(int32_t*)(call + 1);
        g_orig2dfx = tgt;
        DWORD op; VirtualProtect((void*)call, 5, PAGE_EXECUTE_READWRITE, &op);
        *(int32_t*)(call + 1) = (int32_t)((uintptr_t)&Gate2dfxStub - (call + 5));
        VirtualProtect((void*)call, 5, op, &op);
        FlushInstructionCache(GetCurrentProcess(), (void*)call, 5);
        Log("2dfx light gate at %p (target %p)", (void*)call, (void*)tgt);
    }
}

// ---- bass.dll (dynamic) ---------------------------------------------------------------------------------
typedef int(__stdcall* BASS_Init_t)(int, DWORD, DWORD, HWND, const void*);
typedef int(__stdcall* BASS_ErrorGetCode_t)();
typedef DWORD(__stdcall* BASS_StreamCreateFile_t)(int, const void*, uint64_t, uint64_t, DWORD);
typedef int(__stdcall* BASS_ChannelPlay_t)(DWORD, int);
typedef int(__stdcall* BASS_ChannelPause_t)(DWORD);
typedef DWORD(__stdcall* BASS_ChannelIsActive_t)(DWORD);
typedef int(__stdcall* BASS_ChannelSetAttribute_t)(DWORD, DWORD, float);
typedef int(__stdcall* BASS_StreamFree_t)(DWORD);
typedef DWORD(__stdcall* BASS_ChannelSetFX_t)(DWORD, DWORD, int);
typedef int(__stdcall* BASS_FXSetParameters_t)(DWORD, const void*);
typedef int(__stdcall* BASS_ChannelRemoveFX_t)(DWORD, DWORD);
static struct {
    HMODULE dll; bool ok;
    BASS_Init_t Init; BASS_ErrorGetCode_t Err; BASS_StreamCreateFile_t Create; BASS_ChannelPlay_t Play;
    BASS_ChannelPause_t Pause; BASS_ChannelIsActive_t IsActive; BASS_ChannelSetAttribute_t SetAttr;
    BASS_StreamFree_t Free; BASS_ChannelSetFX_t SetFX; BASS_FXSetParameters_t FXSet; BASS_ChannelRemoveFX_t RemoveFX;
} B = {};
enum { BASS_ATTRIB_VOL = 2, BASS_ATTRIB_PAN = 3, BASS_ACTIVE_STOPPED = 0, BASS_ACTIVE_PLAYING = 1, BASS_ACTIVE_PAUSED = 3,
       BASS_FX_DX8_PARAMEQ = 7, BASS_ERROR_ALREADY = 14 };
struct BASS_DX8_PARAMEQ { float fCenter, fBandwidth, fGain; };
struct BASS_DX8_ECHO { float fWetDryMix, fFeedback, fLeftDelay, fRightDelay; BOOL lPanDelay; };
struct BASS_DX8_I3DL2REVERB { int lRoom, lRoomHF; float flRoomRolloffFactor, flDecayTime, flDecayHFRatio; int lReflections;
    float flReflectionsDelay; int lReverb; float flReverbDelay, flDiffusion, flDensity, flHFReference; };
enum { BASS_FX_DX8_ECHO = 3, BASS_FX_DX8_I3DL2REVERB = 6, BASS_SAMPLE_LOOP = 4 };

static std::vector<std::string> g_thunderFiles, g_thunderClose, g_thunderFar, g_dogFiles;

static void InitAudio()
{
    if (!S.soundEnabled) return;
    char p[MAX_PATH]; snprintf(p, sizeof p, "%s\\bass.dll", g_dataDir);
    B.dll = LoadLibraryExA(p, nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!B.dll) { Log("bass.dll not found at %s (error %lu) -- no thunder sound", p, GetLastError()); return; }
#define GP(field, name) B.field = (decltype(B.field))GetProcAddress(B.dll, name)
    GP(Init, "BASS_Init"); GP(Err, "BASS_ErrorGetCode"); GP(Create, "BASS_StreamCreateFile"); GP(Play, "BASS_ChannelPlay");
    GP(Pause, "BASS_ChannelPause"); GP(IsActive, "BASS_ChannelIsActive"); GP(SetAttr, "BASS_ChannelSetAttribute");
    GP(Free, "BASS_StreamFree"); GP(SetFX, "BASS_ChannelSetFX"); GP(FXSet, "BASS_FXSetParameters"); GP(RemoveFX, "BASS_ChannelRemoveFX");
#undef GP
    if (!B.Init || !B.Create || !B.Play || !B.Pause || !B.IsActive || !B.SetAttr || !B.Free)
    { Log("bass.dll is missing exports -- no thunder sound"); return; }
    HWND hw = GetForegroundWindow(); DWORD pid = 0;
    if (hw) GetWindowThreadProcessId(hw, &pid);
    if (pid != GetCurrentProcessId()) hw = nullptr;
    if (!B.Init(-1, 44100, 0, hw, nullptr))
    {
        int e = B.Err ? B.Err() : -1;
        if (e != BASS_ERROR_ALREADY) { Log("BASS_Init failed (%d) -- no thunder sound", e); return; }
        Log("BASS already initialised by another mod, sharing it");
    }
    B.ok = true;

    // .mp3/.wav/.ogg in Thunder\Close (cracks), Thunder\Far (rumbles) and Thunder (any distance)
    auto scan = [](const char* sub, std::vector<std::string>& out)
    {
        const char* exts[] = { "mp3", "wav", "ogg" };
        for (const char* e : exts)
        {
            char q[MAX_PATH]; snprintf(q, sizeof q, "%s\\%s\\*.%s", g_dataDir, sub, e);
            WIN32_FIND_DATAA fd; HANDLE h = FindFirstFileA(q, &fd);
            if (h == INVALID_HANDLE_VALUE) continue;
            do { char f[MAX_PATH]; snprintf(f, sizeof f, "%s\\%s\\%s", g_dataDir, sub, fd.cFileName); out.push_back(f); }
            while (FindNextFileA(h, &fd));
            FindClose(h);
        }
    };
    scan("Thunder", g_thunderFiles); scan("Thunder\\Close", g_thunderClose); scan("Thunder\\Far", g_thunderFar);
    scan("Audio\\Dogs", g_dogFiles);
    for (auto& f : g_dogFiles) { size_t k = f.find("\\Audio\\"); if (k != std::string::npos) f = f.substr(k + 7); }   // path relative to the Audio folder
    Log("audio ready: %d any-distance, %d close, %d far thunder sound(s)", (int)g_thunderFiles.size(), (int)g_thunderClose.size(), (int)g_thunderFar.size());
}

// ---- world state --------------------------------------------------------------------------------------------
struct V3 { float x, y, z; };
static inline V3 operator-(V3 a, V3 b) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
static inline float Dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static inline float Len(V3 a) { return sqrtf(Dot(a, a)); }

struct Bolt
{
    std::vector<V3> pts;
    std::vector<std::vector<V3>> branches;
    float startSize, size, fade;
    float color[3];
    bool  skyOverride; float sky[3]; float skyBright;
    bool  decor; float skyMul;
    uint32_t holdUntil;        // stays lit (flickering) until this time, then fades
};
struct BoltOpts
{
    bool growUp = false, branches = true, fromScript = false;
    int  size = -1;
    bool hasColor = false; float color[3] = { 0, 0, 0 };
    bool hasSky = false; float sky[3] = { 0, 0, 0 };
    float skyBright = -1.f;
    // 1.3: decorative / distant lightning
    bool decor = false;        // never touches anything: no explosion, blackout, alarms, rumble
    bool horizontal = false;   // cloud-to-cloud
    bool silent = false;       // too far to be heard (heat lightning)
    float stopAbove = 0.f;     // descending bolt that dies this many metres above the ground
    float sizeMul = 1.f, skyMul = 1.f, fadeMul = 1.f;
};
struct PendingSound { uint32_t at; V3 pos; std::string file; float vol, audible; };   // file empty = thunder
struct PendingBolt { uint32_t at; V3 pos; BoltOpts o; };
static std::vector<PendingBolt> g_pendingBolts;
struct Flash { V3 pos; uint32_t until; int r, g, b; float range, inten; };
static std::vector<Flash> g_flashes;
struct ActiveSound { DWORD h; V3 pos; float baseVol; bool paused; bool ui; DWORD fx1, fx2; float distMuffle, lastMuffle; };

static std::vector<Bolt> g_bolts;
static std::vector<PendingSound> g_pending;
static std::vector<ActiveSound> g_sounds;

static uint32_t g_now = 0, g_lastNow = 0, g_nextWeatherTick = 0;
static int g_player = 0;
static V3 g_playerPos = { 0, 0, 0 }, g_camPos = { 0, 0, 0 }, g_camFwd = { 0, 1, 0 }, g_camRight = { 1, 0, 0 };
static bool g_forcedStorm = false;
static char g_toast[160] = { 0 };
static bool g_toastPending = false;

static void Toast(const char* s) { strncpy(g_toast, s, sizeof g_toast - 1); g_toastPending = true; }

static float GroundZ(float x, float y, float z, bool* found)
{
    float gz = 0.f; NativeCtx c; c.F(x).F(y).F(z).P(&gz);
    bool ok = Call(nGroundZ, c) && c.resI();
    if (found) *found = ok;
    return gz;
}

// ---- sky flash: timecycle params of the LIGHTNING weather ----------------------------------------------
struct TcSave { bool saved; float bright, c1[3], c2[3], c3[3]; };
static TcSave g_tcSave[TC_HOURS][TC_WEATHERS] = {};
static float g_avgFade = 0.5f;

static inline uint8_t* TcSlot(int h, int w) { return g_timecycle + (size_t)(h * TC_WEATHERS + w) * TC_STRIDE; }
static void CurrentTcHours(int& h1, int& h2)
{
    uint32_t hr = 12, mn = 0; NativeCtx c; c.P(&hr).P(&mn); Call(nTimeOfDay, c);
    h1 = TC_HOURS - 1; h2 = 0;
    for (int i = 0; i < TC_HOURS - 1; ++i)
        if ((int)hr >= kTcHours[i] && (int)hr < kTcHours[i + 1]) { h1 = i; h2 = i + 1; break; }
}
static void SkyFlashUpdate(bool active, float dt60)
{
    if (!g_timecycle || !S.skyFlash) return;
    FP_TRY {
        if (active)
        {
            // light up the clouds of the LIGHTNING weather and of whatever weather is on now (distant / heat lightning)
            int ws[3] = { W_LIGHTNING, -1, -1 }; int nw = 1;
            uint32_t ow = g_oldWeather ? *g_oldWeather : W_LIGHTNING, nwth = g_newWeather ? *g_newWeather : W_LIGHTNING;
            if (ow < TC_WEATHERS && (int)ow != ws[0]) ws[nw++] = (int)ow;
            if (nwth < TC_WEATHERS && (int)nwth != ws[0] && (nw < 2 || (int)nwth != ws[1])) ws[nw++] = (int)nwth;
            const Bolt* best = nullptr; float add = 0.f;
            for (auto& b : g_bolts)
            {
                float a = (b.skyBright >= 0.f ? b.skyBright : S.cloudBrightness) * b.skyMul;
                if (!best || a > add) { best = &b; add = a; }
            }
            const float* col = (best && best->skyOverride) ? best->sky : S.color;
            int h[2]; CurrentTcHours(h[0], h[1]);
            for (int k = 0; k < 2; ++k) for (int wi = 0; wi < nw; ++wi)
            {
                uint8_t* p = TcSlot(h[k], ws[wi]); TcSave& s = g_tcSave[h[k]][ws[wi]];
                if (!s.saved)
                {
                    s.bright = *(float*)(p + TC_CLOUDS_BRIGHTNESS);
                    memcpy(s.c1, p + TC_CLOUD1, 12); memcpy(s.c2, p + TC_CLOUD2, 12); memcpy(s.c3, p + TC_CLOUD3, 12);
                    s.saved = true;
                }
                *(float*)(p + TC_CLOUDS_BRIGHTNESS) = s.bright + add;
                // weak flashes only tint the clouds a little
                float m = best && best->skyMul < 1.f ? best->skyMul : 1.f;
                float* c[3] = { (float*)(p + TC_CLOUD1), (float*)(p + TC_CLOUD2), (float*)(p + TC_CLOUD3) };
                float* o[3] = { s.c1, s.c2, s.c3 };
                for (int i = 0; i < 3; ++i) for (int j = 0; j < 3; ++j) c[i][j] = o[i][j] + (col[j] - o[i][j]) * m;
            }
        }
        else
        {
            // fade back to the saved values (framerate-independent version of the original's Lerp)
            float t = 1.f - powf(1.f - (g_avgFade > 0.99f ? 0.99f : g_avgFade), dt60);
            for (int h = 0; h < TC_HOURS; ++h) for (int w = 0; w < TC_WEATHERS; ++w)
            {
                TcSave& s = g_tcSave[h][w]; if (!s.saved) continue;
                uint8_t* p = TcSlot(h, w);
                float* b = (float*)(p + TC_CLOUDS_BRIGHTNESS);
                float* c[3] = { (float*)(p + TC_CLOUD1), (float*)(p + TC_CLOUD2), (float*)(p + TC_CLOUD3) };
                float* o[3] = { s.c1, s.c2, s.c3 };
                *b += (s.bright - *b) * t;
                float err = fabsf(*b - s.bright);
                for (int i = 0; i < 3; ++i) for (int j = 0; j < 3; ++j) { c[i][j] += (o[i][j] - c[i][j]) * t; err += fabsf(c[i][j] - o[i][j]); }
                if (err < 0.01f)
                {
                    *b = s.bright; memcpy(c[0], s.c1, 12); memcpy(c[1], s.c2, 12); memcpy(c[2], s.c3, 12);
                    s.saved = false;
                }
            }
        }
    }
    FP_EXCEPT { Log("sky flash faulted -- disabled"); g_timecycle = nullptr; }
}
static void SkyRestoreAll()
{
    if (!g_timecycle) return;
    FP_TRY {
        for (int h = 0; h < TC_HOURS; ++h) for (int w = 0; w < TC_WEATHERS; ++w)
        {
            TcSave& s = g_tcSave[h][w]; if (!s.saved) continue;
            uint8_t* p = TcSlot(h, w);
            *(float*)(p + TC_CLOUDS_BRIGHTNESS) = s.bright;
            memcpy(p + TC_CLOUD1, s.c1, 12); memcpy(p + TC_CLOUD2, s.c2, 12); memcpy(p + TC_CLOUD3, s.c3, 12);
            s.saved = false;
        }
    }
    FP_EXCEPT {}
}

// ---- thunder sound ----------------------------------------------------------------------------------------------
static void ApplySoundParams(ActiveSound& s, bool interior)
{
    V3 d = s.pos - g_camPos; float dist = Len(d);
    float pan = 0.f;
    if (!s.ui && dist > 1.f) { pan = Dot(d, g_camRight) / dist; if (S.invertPan) pan = -pan; }
    pan *= 0.85f;
    float vol = s.baseVol * (interior ? (1.f - S.interiorReduction) : 1.f);
    if (vol < 0) vol = 0;
    B.SetAttr(s.h, BASS_ATTRIB_VOL, vol);
    B.SetAttr(s.h, BASS_ATTRIB_PAN, pan);
    // distant = duller; indoors = through the walls (muffled even when close)
    if (s.fx1 && s.fx2 && B.FXSet)
    {
        float k = s.distMuffle;
        if (interior && S.interiorMuffle > k) k = S.interiorMuffle;
        if (fabsf(k - s.lastMuffle) > 0.01f)
        {
            s.lastMuffle = k;
            BASS_DX8_PARAMEQ e1 = { 3500.f, 24.f, -12.f * k }, e2 = { 9000.f, 30.f, -15.f * k };
            B.FXSet(s.fx1, &e1); B.FXSet(s.fx2, &e2);
        }
    }
}
static void MakePedsReact(bool look, V3 pos);
// how "built up" the place around the player is: roof heights around you (0 = open, 1 = deep street canyon)
static float g_density = 0.f; static uint32_t g_densityAt = 0;
static float UrbanDensity()
{
    if (g_densityAt && (int32_t)(g_now - g_densityAt) < 3000) return g_density;
    g_densityAt = g_now;
    bool f0 = false; float base = GroundZ(g_playerPos.x, g_playerPos.y, g_playerPos.z + 1.5f, &f0);
    if (!f0) base = g_playerPos.z - 1.f;
    float sum = 0.f;
    for (int ring = 0; ring < 2; ++ring)
        for (int k = 0; k < 8; ++k)
        {
            float a = k * 0.785398f + ring * 0.392699f, r = ring ? 70.f : 30.f;
            bool f = false; float gz = GroundZ(g_playerPos.x + cosf(a) * r, g_playerPos.y + sinf(a) * r, base + 300.f, &f);
            if (!f) continue;                                        // water / nothing loaded = open
            float v = (gz - base) / 35.f; sum += v < 0.f ? 0.f : (v > 1.f ? 1.f : v);
        }
    g_density = sum / 16.f;
    return g_density;
}
static void QueueWorldSound(const char* file, V3 pos, float vol, float audible, uint32_t extraMs = 0)
{
    if (!B.ok) return;
    float d = Len(pos - g_playerPos);
    if (d >= audible) return;
    g_pending.push_back({ g_now + extraMs + (uint32_t)(d / S.speedOfSound * 1000.f), pos, std::string(file), vol, audible });
}
static void StartThunder(V3 pos, bool interior)
{
    if (!B.ok) return;
    float dist = Len(pos - g_camPos);
    // close strike -> sharp crack, far -> long low rumble; in between both are possible
    const std::vector<std::string>* pool = &g_thunderFiles;
    float tFar = (dist - S.closeDistance) / (S.farDistance - S.closeDistance > 1.f ? S.farDistance - S.closeDistance : 1.f);
    tFar = tFar < 0 ? 0 : (tFar > 1 ? 1 : tFar);
    if (!g_thunderClose.empty() || !g_thunderFar.empty())
    {
        bool isFar = Rnd01() < tFar;
        if (isFar && !g_thunderFar.empty()) pool = &g_thunderFar;
        else if (!isFar && !g_thunderClose.empty()) pool = &g_thunderClose;
        else pool = !g_thunderFar.empty() ? &g_thunderFar : &g_thunderClose;
    }
    if (pool->empty()) pool = !g_thunderFiles.empty() ? &g_thunderFiles : (!g_thunderClose.empty() ? &g_thunderClose : &g_thunderFar);
    if (pool->empty()) return;
    const std::string& f = (*pool)[RndI(0, (int)pool->size() - 1)];
    DWORD h = B.Create(0, f.c_str(), 0, 0, 0);
    if (!h) { Log("could not open %s (%d)", f.c_str(), B.Err ? B.Err() : -1); return; }
    // close strikes at full volume, far ones fall to DistantVolume (smoothly, ~1.5 km scale)
    float att = S.distantVolume + (1.f - S.distantVolume) * expf(-dist / 1500.f);
    ActiveSound s = { h, pos, S.volume * att, false, false, 0, 0, 0.f, -1.f };
    // distant thunder is a low rumble: two DX8 EQ bands cut the highs (also used indoors)
    if (B.SetFX && B.FXSet)
    {
        float k = (dist - 300.f) / 2500.f; k = k < 0 ? 0 : (k > 1 ? 1 : k); k *= S.distantMuffle;
        s.distMuffle = k;
        s.fx1 = B.SetFX(h, BASS_FX_DX8_PARAMEQ, 0); s.fx2 = B.SetFX(h, BASS_FX_DX8_PARAMEQ, 0);
    }
    // streets between tall buildings throw the thunder back: echo + reverb, nothing in the open
    if (S.echo && !interior && B.SetFX && B.FXSet)
    {
        float k = UrbanDensity() * S.echoStrength; k = k > 1.f ? 1.f : k;
        if (k > 0.08f)
        {
            // slap-back off the facades (an effect that can't be configured is removed, never left on its defaults)
            DWORD e = B.SetFX(h, BASS_FX_DX8_ECHO, 1);
            BASS_DX8_ECHO ep = { 8.f + 22.f * k, 10.f + 28.f * k, RndF(80.f, 140.f), RndF(150.f, 260.f), FALSE };
            if (e && !B.FXSet(e, &ep) && B.RemoveFX) B.RemoveFX(h, e);
            // long street-canyon reverb
            DWORD r = B.SetFX(h, BASS_FX_DX8_I3DL2REVERB, 2);
            BASS_DX8_I3DL2REVERB rp = { (int)(-2600.f + 2100.f * k), -1400, 0.f, 1.3f + 1.9f * k, 0.5f, (int)(-1600.f + 1200.f * k),
                                         0.09f - 0.05f * k, (int)(-1500.f + 1100.f * k), 0.03f, 75.f, 85.f, 5000.f };
            if (r && !B.FXSet(r, &rp) && B.RemoveFX) B.RemoveFX(h, r);
        }
        Log("thunder %.0f m, urban echo %.2f", dist, k);
    }
    ApplySoundParams(s, interior);
    B.Play(h, 0);
    g_sounds.push_back(s);
    if (dist < 2500.f) MakePedsReact(true, pos);
    // a loud clap sets the neighbourhood dogs off
    if (S.dogs && !interior && !g_dogFiles.empty() && dist < S.dogMaxDist)
    {
        static uint32_t s_lastDog = 0;
        float t = 1.f - dist / S.dogMaxDist;
        if ((!s_lastDog || (int32_t)(g_now - s_lastDog) > 8000) && Chance(S.dogChance * (0.4f + 0.6f * t)))
        {
            s_lastDog = g_now;
            int n = Chance(35.f) ? 2 : 1;
            uint32_t at = (uint32_t)RndI(400, 2500);
            for (int k = 0; k < n; ++k)
            {
                float a = RndF(0.f, 6.2831853f), r = RndF(120.f, 350.f);     // somewhere in the neighbourhood, never next to you
                V3 dp = { g_playerPos.x + cosf(a) * r, g_playerPos.y + sinf(a) * r, g_playerPos.z };
                const std::string& f = g_dogFiles[RndI(0, (int)g_dogFiles.size() - 1)];
                QueueWorldSound(f.c_str(), dp, S.dogVolume * S.volume * RndF(0.6f, 1.f), 500.f, at);
                at += (uint32_t)RndI(600, 1800);
            }
            Log("dogs barking (%d)", n);
        }
    }
    // the controller feels the thunder (the game's own pad rumble -> NativeDualSense turns it into haptics)
    if (S.padRumble && nShakePad && dist < S.rumbleDistance)
    {
        float t = 1.f - dist / S.rumbleDistance; t = t * t;
        int inten = (int)(60.f + 195.f * t * S.rumbleStrength); if (inten > 255) inten = 255;
        int dur = (int)(350.f + 900.f * t);
        NativeCtx c; c.I(0).I(dur).I(inten); Call(nShakePad, c);
    }
}
static void QueueThunder(V3 pos)
{
    if (!B.ok) return;
    float dist = Len(pos - g_playerPos);
    float delay = dist / S.speedOfSound * S.delayMultiplier;
    if (delay > S.maxDelay) delay = S.maxDelay;
    if (delay < 0) delay = 0;
    g_pending.push_back({ g_now + (uint32_t)(delay * 1000.f), pos, std::string(), 0.f, 0.f });
}
static void PauseAllSounds(bool pause)
{
    if (!B.ok) return;
    for (auto& s : g_sounds)
    {
        if (pause && !s.paused && B.IsActive(s.h) == BASS_ACTIVE_PLAYING) { B.Pause(s.h); s.paused = true; }
        else if (!pause && s.paused) { B.Play(s.h, 0); s.paused = false; }
    }
}

// ---- ped helpers ------------------------------------------------------------------------------------------------
struct PedRef { int handle; V3 pos; };
static void CollectPeds(std::vector<PedRef>& out, V3 c, float radius)
{
    out.clear();
    if (!g_pedPoolPtr) return;
    FP_TRY {
        uint8_t* pool = *g_pedPoolPtr;
        if (!pool) return;
        uint8_t* storage = *(uint8_t**)(pool + 0); uint8_t* flags = *(uint8_t**)(pool + 4);
        int32_t size = *(int32_t*)(pool + 8), stride = *(int32_t*)(pool + 0xC);
        if (!storage || !flags || size <= 0 || size > 4096 || stride < 0x200 || stride > 0x4000) return;
        for (int i = 0; i < size; ++i)
        {
            if (flags[i] & 0x80) continue;
            float* m = *(float**)(storage + i * stride + 0x20);
            if (!m) continue;
            V3 p = { m[12], m[13], m[14] };
            if (Len(p - c) > radius) continue;
            int h = (i << 8) | flags[i];
            if (h == g_player) continue;
            out.push_back({ h, p });
        }
    }
    FP_EXCEPT { out.clear(); }
}
static bool PedOk(int h)
{
    NativeCtx a; a.I(h); if (Call(nCharDead, a) && a.resI()) return false;
    NativeCtx b; b.I(h); if (Call(nMissionPed, b) && b.resI()) return false;
    return true;
}
static void Say(int h, const char* phrase)
{
    if (!nSay) return;
    NativeCtx c; c.I(h).P(phrase).I(1).I(1).I(0); Call(nSay, c);
}
static void MakePedsReact(bool look, V3 pos)
{
    if (!S.pedReactions) return;
    static std::vector<PedRef> peds; CollectPeds(peds, g_playerPos, 80.f);
    bool animOk = false;
    if (look && nAnimsLoaded) { NativeCtx a; a.P("amb@shock_events"); animOk = Call(nAnimsLoaded, a) && a.resI(); }
    for (auto& pr : peds)
    {
        if (!Chance(S.reactionChance)) continue;
        if (!PedOk(pr.handle)) continue;
        if (RndI(0, 99) < 40) Say(pr.handle, RndI(0, 99) < 50 ? "SURPRISED" : "SHIT");
        if (!look) continue;
        NativeCtx ic; ic.I(pr.handle); if (Call(nInAnyCar, ic) && ic.resI()) continue;
        if (RndI(0, 99) < 50 || !animOk)
        {
            if (nLookAt) { NativeCtx c; c.I(pr.handle).F(pos.x).F(pos.y).F(pos.z).I(RndI(2000, 4000)).I(0); Call(nLookAt, c); }
        }
        else if (nPlayAnim)
        {
            NativeCtx c; c.I(pr.handle).P("look_over_shoulder").P("amb@shock_events").F(3.f).I(0).I(0).I(0).I(0).I(-1); Call(nPlayAnim, c);
        }
    }
}

// ---- data files (tiny JSON readers for the original mod's two files) ----------------------------------------------
static std::string ReadFileStr(const char* path)
{
    std::string r; FILE* f = fopen(path, "rb"); if (!f) return r;
    char buf[4096]; size_t n; while ((n = fread(buf, 1, sizeof buf, f)) > 0) r.append(buf, n); fclose(f); return r;
}
static std::vector<std::string> JsonObjects(const std::string& j)   // top-level objects of a top-level array
{
    std::vector<std::string> out; int depth = 0; size_t start = 0; bool str = false;
    for (size_t i = 0; i < j.size(); ++i)
    {
        char c = j[i];
        if (c == '"' && (i == 0 || j[i - 1] != '\\')) str = !str;
        if (str) continue;
        if (c == '{') { if (depth == 0) start = i; ++depth; }
        else if (c == '}') { --depth; if (depth == 0) out.push_back(j.substr(start, i - start + 1)); }
    }
    return out;
}
static bool JsonNum(const std::string& o, const char* key, float& v, size_t from = 0, size_t* after = nullptr)
{
    std::string k = std::string("\"") + key + "\""; size_t p = o.find(k, from); if (p == std::string::npos) return false;
    p = o.find(':', p); if (p == std::string::npos) return false; ++p;
    while (p < o.size() && (o[p] == ' ' || o[p] == '\t' || o[p] == '\r' || o[p] == '\n')) ++p;
    if (o.compare(p, 4, "true") == 0) { v = 1.f; if (after) *after = p + 4; return true; }
    if (o.compare(p, 5, "false") == 0) { v = 0.f; if (after) *after = p + 5; return true; }
    char* e = nullptr; v = strtof(o.c_str() + p, &e); if (after) *after = e - o.c_str(); return e != o.c_str() + p;
}
static bool JsonVec(const std::string& o, const char* key, float v[3])
{
    std::string k = std::string("\"") + key + "\""; size_t p = o.find(k); if (p == std::string::npos) return false;
    size_t b = o.find('{', p), e = o.find('}', p); if (b == std::string::npos || e == std::string::npos) return false;
    std::string sub = o.substr(b, e - b + 1);
    return JsonNum(sub, "X", v[0]) && JsonNum(sub, "Y", v[1]) && JsonNum(sub, "Z", v[2]);
}
struct Substation { V3 pos; bool hit; };
struct ScriptedBolt { BoltOpts o; V3 spawn; float rMin, rMax; V3 trig; float trigDist; int trigger; float chance; };
static std::vector<Substation> g_substations;
static std::vector<ScriptedBolt> g_scripted;
static void LoadData()
{
    char p[MAX_PATH];
    snprintf(p, sizeof p, "%s\\Data\\ElectricalSubstations.json", g_dataDir);
    for (auto& o : JsonObjects(ReadFileStr(p)))
    {
        float v[3]; if (JsonVec(o, "Position", v)) g_substations.push_back({ { v[0], v[1], v[2] }, false });
    }
    snprintf(p, sizeof p, "%s\\Data\\ScriptedLightning.json", g_dataDir);
    for (auto& o : JsonObjects(ReadFileStr(p)))
    {
        ScriptedBolt sb; float f, v[3];
        if (!JsonVec(o, "SpawnPosition", v)) continue;
        sb.spawn = { v[0], v[1], v[2] };
        sb.o.fromScript = true;
        if (JsonNum(o, "LightningGrowsFromBottomToTop", f)) sb.o.growUp = f != 0.f;
        if (JsonNum(o, "CanHaveBranches", f)) sb.o.branches = f != 0.f;
        if (JsonNum(o, "OverrideLightningSize", f)) sb.o.size = (int)f;
        if (JsonVec(o, "OverrideLightningColor", v) && (v[0] || v[1] || v[2])) { sb.o.hasColor = true; memcpy(sb.o.color, v, 12); }
        if (JsonVec(o, "OverrideSkyColor", v) && (v[0] || v[1] || v[2])) { sb.o.hasSky = true; memcpy(sb.o.sky, v, 12); }
        if (JsonNum(o, "OverrideSkyBrightness", f)) sb.o.skyBright = f;
        sb.rMin = JsonNum(o, "SpawnRadiusMin", f) ? f : 0.f;
        sb.rMax = JsonNum(o, "SpawnRadiusMax", f) ? f : 0.f;
        sb.trig = { 0, 0, 0 }; if (JsonVec(o, "TriggerPos", v)) sb.trig = { v[0], v[1], v[2] };
        sb.trigDist = JsonNum(o, "TriggerDistance", f) ? f : 0.f;
        sb.trigger = JsonNum(o, "TheTrigger", f) ? (int)f : 0;
        sb.chance = JsonNum(o, "SpawnChance", f) ? f : 0.f;
        g_scripted.push_back(sb);
    }
    Log("data: %d substation(s), %d scripted lightning spot(s)", (int)g_substations.size(), (int)g_scripted.size());
}

// ---- blackout ----------------------------------------------------------------------------------------------------------
enum { TC_DIST_CORONA_SIZE = 64, TC_AMB0 = 16, TC_AMB1 = 20, TC_SKYLIGHT = 36 };
struct TimeModel { uint32_t* hours; uint32_t orig; bool exempt; };
static std::vector<TimeModel> g_timeModels;
static bool g_timeModelsScanned = false;
static bool g_blackout = false;            // logically on
static uint32_t g_blackoutEnd = 0;
static size_t g_cascadePos = 0;            // models switched so far in the current direction
static bool g_cascadeOff = false;          // direction: true = turning lights off
static float g_cascadeAcc = 0.f;
static float g_darkFade = 0.f;
static uint32_t g_curseAt = 0;
static uint32_t g_pedsReactAt = 0;
struct TcBlackSave { bool saved; float dcs, a0, a1, sky; };
static TcBlackSave g_tcBlack[TC_HOURS][TC_WEATHERS] = {};

static void ScanTimeModels()
{
    g_timeModelsScanned = true;
    if (!g_miTable || !g_miCount || !g_miPtrsBase) { Log("blackout: model table not found -- buildings stay lit"); return; }
    FP_TRY {
        uint32_t* tab = *g_miTable; int n = *g_miCount;
        uint8_t** ptrs = (uint8_t**)g_miPtrsBase;
        // CTimeModelInfo vtable: [3] = "mov al,3; ret" (GetModelType), [5] = "lea eax,[ecx+60h]; ret" (GetTimeInfo).
        // Find it once, then recognise time models by their vtable -- no virtual calls into the game.
        const uintptr_t timeVt = g_timeVt;
        if (!timeVt) return;
        for (int i = 0; i < n; ++i)
        {
            uint32_t idx = tab[i * 2 + 1];
            if (idx > 200000) continue;
            uint8_t* mi = ptrs[idx];
            if (!mi || *(uintptr_t*)mi != timeVt) continue;
            uint32_t* ti = (uint32_t*)(mi + 0x60);
            g_timeModels.push_back({ ti, *ti, false });
        }
    }
    FP_EXCEPT { Log("blackout: model scan faulted after %d models", (int)g_timeModels.size()); }
    for (size_t i = g_timeModels.size(); i > 1; --i) { size_t j = RndU() % i; std::swap(g_timeModels[i - 1], g_timeModels[j]); }
    Log("blackout: %d timed models (lit windows / signs)", (int)g_timeModels.size());
}
static void TcBlackoutApply(float fade)
{
    if (!g_timecycle) return;
    FP_TRY {
        uint32_t w[2] = { g_oldWeather ? *g_oldWeather : 0, g_newWeather ? *g_newWeather : 0 };
        uint32_t hr = 12, mn = 0; { NativeCtx c; c.P(&hr).P(&mn); Call(nTimeOfDay, c); }
        const bool night = hr >= 20 || hr < 6;
        for (int k = 0; k < 2; ++k)
        {
            if (w[k] >= TC_WEATHERS) continue;
            for (int h = 0; h < TC_HOURS; ++h)
            {
                uint8_t* p = g_timecycle + (size_t)(h * TC_WEATHERS + w[k]) * TC_STRIDE;
                TcBlackSave& sv = g_tcBlack[h][w[k]];
                if (!sv.saved)
                {
                    sv.dcs = *(float*)(p + TC_DIST_CORONA_SIZE); sv.a0 = *(float*)(p + TC_AMB0);
                    sv.a1 = *(float*)(p + TC_AMB1); sv.sky = *(float*)(p + TC_SKYLIGHT); sv.saved = true;
                }
                *(float*)(p + TC_DIST_CORONA_SIZE) = sv.dcs * (1.f - fade);
                const float d = night ? S.blackoutDarkness * fade : 0.f;
                *(float*)(p + TC_AMB0) = sv.a0 * (1.f - d); *(float*)(p + TC_AMB1) = sv.a1 * (1.f - d);
                *(float*)(p + TC_SKYLIGHT) = sv.sky * (1.f - d);
            }
        }
    }
    FP_EXCEPT { Log("blackout timecycle faulted"); }
}
static void TcBlackoutRestore()
{
    if (!g_timecycle) return;
    FP_TRY {
        for (int h = 0; h < TC_HOURS; ++h) for (int w = 0; w < TC_WEATHERS; ++w)
        {
            TcBlackSave& sv = g_tcBlack[h][w]; if (!sv.saved) continue;
            uint8_t* p = g_timecycle + (size_t)(h * TC_WEATHERS + w) * TC_STRIDE;
            *(float*)(p + TC_DIST_CORONA_SIZE) = sv.dcs; *(float*)(p + TC_AMB0) = sv.a0;
            *(float*)(p + TC_AMB1) = sv.a1; *(float*)(p + TC_SKYLIGHT) = sv.sky; sv.saved = false;
        }
    }
    FP_EXCEPT {}
}
static void RestoreAllModels()
{
    FP_TRY { for (auto& m : g_timeModels) *m.hours = m.orig; }
    FP_EXCEPT {}
}
static void PlayUiSound(const char* file, float vol)
{
    if (!B.ok) return;
    char p[MAX_PATH]; snprintf(p, sizeof p, "%s\\Audio\\%s", g_dataDir, file);
    DWORD h = B.Create(0, p, 0, 0, 0); if (!h) return;
    ActiveSound s = { h, g_camPos, vol, false, true, 0, 0, 0.f, -1.f };
    B.SetAttr(h, BASS_ATTRIB_VOL, vol); B.Play(h, 0);
    g_sounds.push_back(s);
}
// positional one-shot from Audio\ (pans with the camera, quieter and duller with distance)
static DWORD PlayWorldSound(const char* file, V3 pos, float vol, float audible, bool loop = false)
{
    if (!B.ok) return 0;
    float d = Len(pos - g_camPos);
    if (d >= audible) return 0;
    char p[MAX_PATH]; snprintf(p, sizeof p, "%s\\Audio\\%s", g_dataDir, file);
    DWORD h = B.Create(0, p, 0, 0, loop ? BASS_SAMPLE_LOOP : 0); if (!h) { Log("could not open %s", p); return 0; }
    float t = 1.f - d / audible;
    ActiveSound s = { h, pos, vol * t * sqrtf(t), false, false, 0, 0, 0.f, -1.f };
    if (B.SetFX && B.FXSet) { s.distMuffle = (d / audible) * 0.7f; s.fx1 = B.SetFX(h, BASS_FX_DX8_PARAMEQ, 0); s.fx2 = B.SetFX(h, BASS_FX_DX8_PARAMEQ, 0); }
    NativeCtx in; bool interior = Call(nInterior, in) && in.resI();
    ApplySoundParams(s, interior);
    B.Play(h, 0);
    g_sounds.push_back(s);
    return h;
}
static void AddFlash(V3 pos, uint32_t ms, int r, int g, int b, float range, float inten)
{
    g_flashes.push_back({ pos, g_now + ms, r, g, b, range, inten });
}
static void UpdateFlashes()
{
    for (size_t i = 0; i < g_flashes.size();)
    {
        Flash& f = g_flashes[i];
        if ((int32_t)(g_now - f.until) >= 0) { g_flashes.erase(g_flashes.begin() + i); continue; }
        if (nLight) { NativeCtx c; c.F(f.pos.x).F(f.pos.y).F(f.pos.z).I(f.r).I(f.g).I(f.b).F(f.range).F(f.inten * RndF(0.7f, 1.f)); Call(nLight, c); }
        ++i;
    }
}

// ---- lightning into the water ------------------------------------------------------------------------------------------
static void WaterStrike(V3 at)
{
    float d = Len(at - g_playerPos);
    Log("lightning hit the water (%.0f m)", d);
    if (d < 700.f && nTriggerPtfx)
    {
        NativeCtx c; c.P("imp_exp_water").F(at.x).F(at.y).F(at.z).F(0.f).F(0.f).F(0.f).F(RndF(1.6f, 2.4f));
        bool ok = Call(nTriggerPtfx, c);
        static bool s_logged = false;
        if (!s_logged) { s_logged = true; Log("TRIGGER_PTFX(imp_exp_water) called=%d ret=%d", ok ? 1 : 0, c.resI()); }
        for (int k = 0; k < 4; ++k)
        {
            float a = RndF(0.f, 6.2831853f), r = RndF(2.f, 7.f);
            NativeCtx w; w.P("water_splash_vehicle").F(at.x + cosf(a) * r).F(at.y + sinf(a) * r).F(at.z).F(0.f).F(0.f).F(RndF(0.f, 360.f)).F(RndF(1.f, 2.f));
            Call(nTriggerPtfx, w);
        }
    }
    AddFlash({ at.x, at.y, at.z + 3.f }, 180, 190, 220, 255, 70.f, 6.f);
    QueueWorldSound("waterstrike.mp3", at, S.waterVolume * S.volume, 450.f);
}

// ---- sparks at a substation that got hit ------------------------------------------------------------------------------
// One-shot "break_sparks" particle bursts (TRIGGER_PTFX does not depend on a script thread, unlike START_PTFX),
// a short blue-white light at every burst and an electrical arcing sound.
struct SparkSession { V3 pos; uint32_t end, next, flashUntil; V3 flashPos; };
static std::vector<SparkSession> g_sparks;
static void StartSparks(V3 at)
{
    if (!S.sparks) return;
    for (auto& s : g_sparks)
        if (Len(s.pos - at) < 15.f) { s.end = g_now + (uint32_t)(S.sparksDuration * 1000.f); return; }   // already sparking: keep going
    SparkSession s; s.pos = at; s.end = g_now + (uint32_t)(S.sparksDuration * 1000.f); s.next = g_now; s.flashUntil = 0; s.flashPos = at;
    g_sparks.push_back(s);
    PlayWorldSound("sparks.mp3", { at.x, at.y, at.z + 2.f }, S.sparksVolume * S.volume, 220.f);
    Log("sparks at substation (%.0f m from the player)", Len(at - g_playerPos));
}
static void UpdateSparks()
{
    for (size_t i = 0; i < g_sparks.size();)
    {
        SparkSession& s = g_sparks[i];
        if ((int32_t)(g_now - s.end) >= 0) { g_sparks.erase(g_sparks.begin() + i); continue; }
        const bool isNear = Len(s.pos - g_playerPos) < 400.f;          // particles further away are not drawn anyway
        if (isNear && (int32_t)(g_now - s.next) >= 0)
        {
            // denser at the start, then dying out
            float left = (float)(int32_t)(s.end - g_now) / (S.sparksDuration * 1000.f);
            s.next = g_now + (uint32_t)RndI(120, 300) + (uint32_t)((1.f - left) * 350.f);
            int bursts = left > 0.6f ? RndI(1, 3) : 1;
            for (int k = 0; k < bursts; ++k)
            {
                V3 p = { s.pos.x + RndF(-4.f, 4.f), s.pos.y + RndF(-4.f, 4.f), s.pos.z + RndF(1.f, 4.5f) };
                NativeCtx c; c.P("break_sparks").F(p.x).F(p.y).F(p.z).F(RndF(-60.f, 60.f)).F(RndF(-60.f, 60.f)).F(RndF(0.f, 360.f)).F(RndF(1.0f, 2.0f) * (0.5f + 0.5f * left));
                bool ok = Call(nTriggerPtfx, c);
                static bool s_logged = false;
                if (!s_logged) { s_logged = true; Log("TRIGGER_PTFX(break_sparks) native=%p called=%d ret=%d", (void*)nTriggerPtfx, ok ? 1 : 0, c.resI()); }
                s.flashPos = p;
            }
            if (S.sparksLight) s.flashUntil = g_now + (uint32_t)RndI(40, 90);
        }
        if (S.sparksLight && nLight && isNear && (int32_t)(s.flashUntil - g_now) > 0)
        {
            NativeCtx c; c.F(s.flashPos.x).F(s.flashPos.y).F(s.flashPos.z).I(170).I(205).I(255).F(14.f).F(RndF(1.0f, 2.5f));
            Call(nLight, c);
        }
        ++i;
    }
}

// ---- radio interference when lightning strikes while driving -----------------------------------------------------------
static int g_lastRadioIdx = -999;
static void RadioStatic(V3 at)
{
    if (!S.radioStatic || !B.ok || !nInAnyCar) return;
    float d = Len(at - g_playerPos);
    if (d > S.staticDistance) return;
    { NativeCtx c; c.I(g_player); if (!Call(nInAnyCar, c) || !c.resI()) return; }
    if (S.staticNeedsRadio && nRadioIdx)
    {
        NativeCtx c; Call(nRadioIdx, c); int idx = c.resI();
        if (idx != g_lastRadioIdx) { g_lastRadioIdx = idx; Log("radio station index = %d", idx); }
        // GET_PLAYER_RADIO_STATION_INDEX returns 16 when the radio is off (any internal station >= 24 maps to 16)
        if (idx < 0 || idx >= 16) return;
    }
    static uint32_t s_last = 0;
    if (s_last && (int32_t)(g_now - s_last) < 700) return;
    s_last = g_now;
    float t = 1.f - d / S.staticDistance;                              // closer strike = louder, longer crackle
    const char* f = t > 0.66f ? "static3.mp3" : (t > 0.33f ? "static2.mp3" : "static1.mp3");
    PlayUiSound(f, S.staticVolume * S.volume * (0.25f + 0.75f * t));
    Log("radio static (%.0f m)", d);
}

static bool MissionRunning() { NativeCtx mf; return Call(nMissionFlag, mf) && mf.resI(); }

// ---- emergency generators: hospitals and police stations keep their lights during a blackout ------------------------
// Their positions come from the game's own hospital / police respawn points; every building placed near one
// keeps its lit windows (its timed model is left alone by the blackout).
static std::vector<V3> g_genPts;
static DWORD g_genSnd = 0;
static void MarkGeneratorBuildings()
{
    g_genPts.clear();
    for (auto& m : g_timeModels) m.exempt = false;
    if (!S.generators) return;
    FP_TRY {
        for (int k = 0; k < 2; ++k)
        {
            if (!g_restartCnt[k] || !g_restartPts[k]) continue;
            uint32_t n = *g_restartCnt[k]; if (n > 10) n = 10;
            for (uint32_t i = 0; i < n; ++i)
            {
                const float* p = g_restartPts[k] + i * 4;
                V3 v = { p[0], p[1], p[2] };
                if (fabsf(v.x) < 1.f && fabsf(v.y) < 1.f) continue;
                bool dup = false; for (auto& q : g_genPts) if (Len(q - v) < 40.f) { dup = true; break; }
                if (!dup) g_genPts.push_back(v);
            }
        }
    }
    FP_EXCEPT {}
    if (g_genPts.empty()) { Log("generators: no hospital/police points known yet"); return; }
    if (!g_bldPoolPtr || !g_miPtrsBase || g_timeModels.empty()) { Log("generators: %d points, no building pool -- only entrance lights", (int)g_genPts.size()); return; }
    std::vector<uint8_t*> keep; int nearCount = 0;
    FP_TRY {
        uint8_t* pool = *g_bldPoolPtr;
        if (pool)
        {
            uint8_t* storage = *(uint8_t**)(pool + 0); uint8_t* flags = *(uint8_t**)(pool + 4);
            int32_t size = *(int32_t*)(pool + 8), stride = *(int32_t*)(pool + 0xC);
            uint8_t** mptrs = (uint8_t**)g_miPtrsBase;
            const float r2 = S.genRadius * S.genRadius;
            if (storage && flags && size > 0 && size < 400000 && stride >= 0x40 && stride <= 0x400)
                for (int i = 0; i < size; ++i)
                {
                    if (flags[i] & 0x80) continue;
                    uint8_t* e = storage + (size_t)i * stride;
                    const float* mat = *(const float**)(e + 0x20);
                    V3 p = mat ? V3{ mat[12], mat[13], mat[14] } : V3{ *(float*)(e + 0x10), *(float*)(e + 0x14), *(float*)(e + 0x18) };
                    for (auto& g : g_genPts)
                    {
                        float dx = p.x - g.x, dy = p.y - g.y;
                        if (dx * dx + dy * dy > r2 || fabsf(p.z - g.z) > 150.f) continue;
                        int16_t mi = *(int16_t*)(e + 0x2E);
                        if (mi >= 0 && mptrs[mi]) { keep.push_back(mptrs[mi]); ++nearCount; }
                        break;
                    }
                }
        }
    }
    FP_EXCEPT { Log("generators: building scan faulted"); }
    std::sort(keep.begin(), keep.end()); keep.erase(std::unique(keep.begin(), keep.end()), keep.end());
    int kept = 0;
    for (auto& m : g_timeModels)
        if (std::binary_search(keep.begin(), keep.end(), (uint8_t*)m.hours - 0x60)) { m.exempt = true; ++kept; }
    Log("generators: %d hospital/police points, %d buildings near them, %d lit model(s) kept on", (int)g_genPts.size(), nearCount, kept);
}
static void StopGeneratorSound()
{
    if (!g_genSnd) return;
    for (size_t i = 0; i < g_sounds.size(); ++i) if (g_sounds[i].h == g_genSnd) { B.Free(g_genSnd); g_sounds.erase(g_sounds.begin() + i); break; }
    g_genSnd = 0;
}
static void UpdateGenerators()
{
    if (!g_blackout || !S.generators || g_genPts.empty()) { StopGeneratorSound(); return; }
    const V3* nearest = nullptr; float nd = 1e9f;
    for (auto& g : g_genPts)
    {
        float d = Len(g - g_playerPos);
        if (d < nd) { nd = d; nearest = &g; }
        if (S.genLights && nLight && d < 350.f)
        {   // floodlight over the entrance
            NativeCtx c; c.F(g.x).F(g.y).F(g.z + 4.f).I(255).I(238).I(205).F(22.f).F(1.4f); Call(nLight, c);
        }
    }
    const float aud = 90.f;
    if (!S.genSound || !nearest || nd >= aud) { StopGeneratorSound(); return; }
    static bool s_genFail = false;
    if (!g_genSnd && !s_genFail) { g_genSnd = PlayWorldSound("generator.wav", { nearest->x, nearest->y, nearest->z + 1.f }, 0.f, aud, true); s_genFail = !g_genSnd; }
    for (auto& s : g_sounds)
        if (s.h == g_genSnd) { float t = 1.f - nd / aud; s.pos = *nearest; s.baseVol = S.genVolume * S.volume * t * sqrtf(t); break; }
}

static void UpdateVehRange()
{
    g_vehLo = 0; g_vehSpan = 0;
    if (!g_vehPoolPtr) return;
    FP_TRY {
        uint8_t* pool = *g_vehPoolPtr; if (!pool) return;
        uintptr_t storage = *(uintptr_t*)(pool + 0);
        int32_t size = *(int32_t*)(pool + 8), stride = *(int32_t*)(pool + 0xC);
        if (storage && size > 0 && size <= 4096 && stride >= 0x200 && stride <= 0x8000) { g_vehLo = storage; g_vehSpan = (uintptr_t)size * stride; }
    }
    FP_EXCEPT { g_vehLo = 0; g_vehSpan = 0; }
}
// trains stop and Pay'n'Spray shops close while the power is out (as in the original)
struct TrainSave { int h; float speed; };
static std::vector<TrainSave> g_trains;
static uint32_t g_trainScanAt = 0;
static bool g_noResprays = false;
static bool MissionRunning();
static void UpdateBlackoutWorld()
{
    // save safety: if a game was saved during a blackout, the "no resprays" flag could come back with it.
    // After the game starts or a save is loaded (the game timer jumps back), clear it once outside missions.
    static bool s_pnsChecked = false; static uint32_t s_prevNow = 0;
    if (s_prevNow && g_now + 5000u < s_prevNow) s_pnsChecked = false;
    s_prevNow = g_now;
    if (!s_pnsChecked && !g_blackout && nNoResprays && !MissionRunning())
    {
        NativeCtx c; c.I(0); Call(nNoResprays, c); s_pnsChecked = true;
    }
    if (!g_blackout)
    {
        if (g_noResprays && nNoResprays) { NativeCtx c; c.I(0); Call(nNoResprays, c); g_noResprays = false; }
        g_trains.clear();
        return;
    }
    if (nNoResprays && nPnsActive && !g_noResprays)
    {
        NativeCtx a; if (Call(nPnsActive, a) && !a.resI()) { NativeCtx c; c.I(1); Call(nNoResprays, c); g_noResprays = true; }
    }
    if (!nTrainSpeed || !nCarModel || !nIsTrain || !g_vehPoolPtr) return;
    if ((int32_t)(g_now - g_trainScanAt) >= 0)
    {
        g_trainScanAt = g_now + 500;
        std::vector<int> hs;
        FP_TRY {
            uint8_t* pool = *g_vehPoolPtr;
            if (pool)
            {
                uint8_t* storage = *(uint8_t**)(pool + 0); uint8_t* flags = *(uint8_t**)(pool + 4);
                int32_t size = *(int32_t*)(pool + 8), stride = *(int32_t*)(pool + 0xC);
                if (storage && flags && size > 0 && size <= 4096 && stride >= 0x200 && stride <= 0x8000)
                    for (int i = 0; i < size; ++i) if (!(flags[i] & 0x80)) hs.push_back((i << 8) | flags[i]);
            }
        }
        FP_EXCEPT { hs.clear(); }
        std::vector<TrainSave> keep;
        for (int h : hs)
        {
            const TrainSave* old = nullptr; for (auto& t : g_trains) if (t.h == h) { old = &t; break; }
            if (old) { keep.push_back(*old); continue; }
            uint32_t model = 0; { NativeCtx c; c.I(h).P(&model); if (!Call(nCarModel, c)) continue; }
            { NativeCtx c; c.I((int)model); if (!Call(nIsTrain, c) || !c.resI()) continue; }
            int drv = 0; { NativeCtx c; c.I(h).P(&drv); Call(nDriverOfCar, c); }
            if (drv == g_player) continue;
            float sp = 0.f; if (nCarSpeed) { NativeCtx c; c.I(h).P(&sp); Call(nCarSpeed, c); }
            keep.push_back({ h, sp });
        }
        g_trains.swap(keep);
    }
    for (auto& t : g_trains)                      // ease the speed down to a standstill over the fade-in
    {
        NativeCtx c; c.I(t.h).F(t.speed * (1.f - g_darkFade)); Call(nTrainSpeed, c);
    }
}

static void StartBlackout(bool force)
{
    if (!S.blackout || g_blackout) return;
    if (!force && !S.blackoutInMissions && MissionRunning()) return;
    if (!force)
    {
        uint32_t hr = 12, mn = 0; { NativeCtx c; c.P(&hr).P(&mn); Call(nTimeOfDay, c); }
        const float ch = (hr >= 18 || hr < 6) ? S.blackoutChanceEvening : S.blackoutChanceDay;
        if (!Chance(ch)) { Log("blackout roll failed (%.1f%%)", ch); return; }
    }
    if (!g_timeModelsScanned) ScanTimeModels();
    MarkGeneratorBuildings();
    g_blackout = true;
    g_cascadeOff = true; g_cascadePos = 0; g_cascadeAcc = 0.f;
    for (size_t i = g_timeModels.size(); i > 1; --i) { size_t j = RndU() % i; std::swap(g_timeModels[i - 1], g_timeModels[j]); }
    g_blackoutEnd = g_now + (uint32_t)RndI(S.blackoutMin, S.blackoutMax) * 1000u;
    if (S.blackoutLamps)
    {
        g_lampsOff = 1;
        UpdateVehRange();
        g_coronaGate = g_coronaGateOk ? 1 : 0;
        g_tlOff = g_tl1Resume ? 1 : 0;
    }
    g_trainScanAt = g_now;
    NativeCtx in; bool interior = Call(nInterior, in) && in.resI();
    if (S.blackoutSound && (S.blackoutSoundInterior || !interior)) PlayUiSound("blackout.mp3", 0.4f * S.volume);
    if (S.playerReactions) g_curseAt = g_now + (uint32_t)RndI(500, 1000);
    g_pedsReactAt = g_now + (uint32_t)RndI(1500, 3000);
    Log("BLACKOUT for %u s -- %d lit models, lamps %d, coronas %d, traffic lights %d, car range %p+%x",
        (g_blackoutEnd - g_now) / 1000u, (int)g_timeModels.size(), (int)g_lampsOff, (int)g_coronaGate, (int)g_tlOff,
        (void*)g_vehLo, (unsigned)g_vehSpan);
}
static void EndBlackout()
{
    if (!g_blackout) return;
    g_blackout = false;
    g_cascadeOff = false; g_cascadePos = 0; g_cascadeAcc = 0.f;
    g_lampsOff = 0; g_coronaGate = 0; g_tlOff = 0;
    Log("blackout over");
}
static void UpdateBlackout(float dtMs)
{
    if (g_blackout && (int32_t)(g_now - g_blackoutEnd) >= 0) EndBlackout();
    // cascade: about one building per millisecond, in random order (like the original)
    if (!g_timeModels.empty() && g_cascadePos < g_timeModels.size() && (g_blackout || g_cascadeOff == false))
    {
        g_cascadeAcc += dtMs / 1.1f;
        size_t n = (size_t)g_cascadeAcc; g_cascadeAcc -= (float)n;
        FP_TRY {
            for (size_t k = 0; k < n && g_cascadePos < g_timeModels.size(); ++k, ++g_cascadePos)
            {
                TimeModel& m = g_timeModels[g_cascadePos];
                // no "on" hours, plus bit 24 ("swap even while on screen"): with plain 0 -- what the original
                // writes -- a lit window / sign only went dark once the camera looked away from it
                *m.hours = (g_cascadeOff && !m.exempt) ? ((m.orig & 0xFE000000u) | 0x01000000u) : m.orig;
            }
        }
        FP_EXCEPT { Log("blackout cascade faulted"); g_cascadePos = g_timeModels.size(); }
    }
    // fade the darkness / distant lights
    const float target = g_blackout ? 1.f : 0.f;
    const float rate = g_blackout ? dtMs / 4000.f : dtMs / 2500.f;
    if (g_darkFade < target) g_darkFade = g_darkFade + rate > target ? target : g_darkFade + rate;
    else if (g_darkFade > target) g_darkFade = g_darkFade - rate < target ? target : g_darkFade - rate;
    if (g_darkFade > 0.f) TcBlackoutApply(g_darkFade);
    else TcBlackoutRestore();
    UpdateBlackoutWorld();
    if (g_curseAt && (int32_t)(g_now - g_curseAt) >= 0) { g_curseAt = 0; Say(g_player, "GENERIC_CURSE"); }
    if (g_pedsReactAt && (int32_t)(g_now - g_pedsReactAt) >= 0) { g_pedsReactAt = 0; MakePedsReact(false, g_playerPos); }
}
static void CheckSubstationStrike(V3 ground)
{
    for (auto& ss : g_substations)
        if (Len(ground - ss.pos) <= S.substationRange) { Log("lightning hit a substation"); StartSparks(ss.pos); StartBlackout(false); return; }
}
static void CheckSubstationExplosions()
{
    const bool canBlack = S.blackout && S.blackoutFromExplosions;
    if ((!canBlack && !S.sparks) || !nExplInSphere) return;
    for (auto& ss : g_substations)
    {
        bool any = false;
        for (int t = 0; t < 24 && !any; ++t)
        {
            if (t == 1 || t == 3 || t == 8 || t == 9 || t == 10 || t == 11) continue;
            NativeCtx c; c.I(t).F(ss.pos.x).F(ss.pos.y).F(ss.pos.z).F(S.substationRange);
            any = Call(nExplInSphere, c) && c.resI();
        }
        if (any && !ss.hit) { ss.hit = true; Log("explosion at a substation"); StartSparks(ss.pos); if (canBlack) StartBlackout(false); }
        else if (!any) ss.hit = false;
    }
}

// ---- car alarms near a strike -----------------------------------------------------------------------------------------
static void TriggerCarAlarms(V3 at)
{
    if (!S.carAlarms || !g_vehPoolPtr || !nVehAlarm) return;
    std::vector<int> cars;
    FP_TRY {
        uint8_t* pool = *g_vehPoolPtr;
        if (!pool) return;
        uint8_t* storage = *(uint8_t**)(pool + 0); uint8_t* flags = *(uint8_t**)(pool + 4);
        int32_t size = *(int32_t*)(pool + 8), stride = *(int32_t*)(pool + 0xC);
        if (!storage || !flags || size <= 0 || size > 4096 || stride < 0x200 || stride > 0x8000) return;
        for (int i = 0; i < size; ++i)
        {
            if (flags[i] & 0x80) continue;
            float* m = *(float**)(storage + i * stride + 0x20);
            if (!m) continue;
            V3 p = { m[12], m[13], m[14] };
            if (Len(p - at) > S.carAlarmRadius) continue;
            cars.push_back((i << 8) | flags[i]);
        }
    }
    FP_EXCEPT { return; }
    int n = 0;
    for (int h : cars)
    {
        if (n >= 6 || !Chance(S.carAlarmChance)) continue;
        { NativeCtx c; c.I(h); if (Call(nCarDead, c) && c.resI()) continue; }
        { NativeCtx c; c.I(h); if (Call(nMissionCar, c) && c.resI()) continue; }
        int drv = 0; { NativeCtx c; c.I(h).P(&drv); Call(nDriverOfCar, c); }
        if (drv) continue;                                   // only parked, empty cars
        NativeCtx c; c.I(h); Call(nVehAlarm, c); ++n;
    }
    if (n) Log("%d car alarm(s)", n);
}

// ---- smoke where the lightning struck ------------------------------------------------------------------------------
// Only one-shot effects (checked in gta_core.wpfl: numLoops = 0), re-triggered while the spot smoulders,
// so nothing can be left running forever.
struct SmokeSpot { V3 pos; uint32_t start, end, next; };
static std::vector<SmokeSpot> g_smoke;
static void StartSmoke(V3 at)
{
    if (!S.smoke || Len(at - g_playerPos) > 600.f) return;
    if (g_smoke.size() >= 6) g_smoke.erase(g_smoke.begin());
    SmokeSpot sp; sp.pos = at; sp.start = g_now; sp.end = g_now + (uint32_t)RndI(S.smokeMin, S.smokeMax) * 1000u; sp.next = g_now + 600;
    g_smoke.push_back(sp);
    if (nTriggerPtfx)
    {   // small flames that die down on their own
        NativeCtx c; c.P("fire_electrical_spawned_sm").F(at.x).F(at.y).F(at.z + 0.2f).F(0.f).F(0.f).F(0.f).F(1.0f);
        bool ok = Call(nTriggerPtfx, c);
        static bool s_logged = false;
        if (!s_logged) { s_logged = true; Log("TRIGGER_PTFX(fire_electrical_spawned_sm) called=%d ret=%d", ok ? 1 : 0, c.resI()); }
    }
}
static void UpdateSmoke()
{
    for (size_t i = 0; i < g_smoke.size();)
    {
        SmokeSpot& sp = g_smoke[i];
        if ((int32_t)(g_now - sp.end) >= 0) { g_smoke.erase(g_smoke.begin() + i); continue; }
        float life = (float)(g_now - sp.start) / (float)(sp.end - sp.start);     // 0 -> 1
        const bool isNear = Len(sp.pos - g_playerPos) < 600.f;
        if (isNear && nTriggerPtfx && (int32_t)(g_now - sp.next) >= 0)
        {
            sp.next = g_now + (uint32_t)RndI(700, 1300) + (uint32_t)(life * 900.f);
            V3 p = { sp.pos.x + RndF(-1.2f, 1.2f), sp.pos.y + RndF(-1.2f, 1.2f), sp.pos.z + 0.3f };
            NativeCtx c; c.P("fire_ped_smoke").F(p.x).F(p.y).F(p.z).F(0.f).F(0.f).F(RndF(0.f, 360.f)).F(2.2f - 1.6f * life);
            Call(nTriggerPtfx, c);
        }
        // embers glowing for the first seconds
        if (isNear && nLight && life < 0.25f)
        {
            NativeCtx c; c.F(sp.pos.x).F(sp.pos.y).F(sp.pos.z + 0.5f).I(255).I(110).I(40).F(6.f).F(RndF(0.6f, 1.2f) * (1.f - life * 4.f));
            Call(nLight, c);
        }
        ++i;
    }
}

// ---- St. Elmo's fire: faint blue glow on the tops of the tallest buildings during a storm --------------------------
static std::vector<V3> g_elmo;
static uint32_t g_elmoNextScan = 0;
static bool g_stormNow = false;          // set by the weather tick (thunderstorm weather or a thundery heavy rain)
static void UpdateStElmo()
{
    if (!S.stElmo || !g_stormNow || !nCorona) { g_elmo.clear(); return; }
    if ((int32_t)(g_now - g_elmoNextScan) >= 0)
    {
        g_elmoNextScan = g_now + 8000;
        g_elmo.clear();
        bool f0 = false; float base = GroundZ(g_playerPos.x, g_playerPos.y, g_playerPos.z + 1.5f, &f0);
        if (!f0) base = g_playerPos.z;
        struct Cand { V3 p; float h; }; std::vector<Cand> c;
        for (int k = 0; k < 28; ++k)
        {
            float a = RndF(0.f, 6.2831853f), r = RndF(60.f, 650.f);
            float x = g_playerPos.x + cosf(a) * r, y = g_playerPos.y + sinf(a) * r;
            bool f = false; float gz = GroundZ(x, y, 1000.f, &f);
            if (f && gz - base > S.stElmoMinHeight) c.push_back({ { x, y, gz }, gz });
        }
        std::sort(c.begin(), c.end(), [](const Cand& a, const Cand& b) { return a.h > b.h; });
        for (auto& k : c) { if (g_elmo.size() >= 5) break; g_elmo.push_back(k.p); }
    }
    uint32_t hr = 12, mn = 0; { NativeCtx t; t.P(&hr).P(&mn); Call(nTimeOfDay, t); }
    const float night = (hr >= 19 || hr < 6) ? 1.f : 0.45f;
    for (auto& p : g_elmo)
    {
        if (Rnd01() < 0.25f) continue;                                    // flickers
        float sz = RndF(3.f, 7.f) * night;
        int b = (int)(RndF(150.f, 230.f) * night);
        NativeCtx c; c.F(p.x).F(p.y).F(p.z + 0.5f).F(sz).I(0).F(0.f).I(b * 2 / 3).I(b * 4 / 5).I(b);
        Call(nCorona, c);
    }
}

// ---- bolts --------------------------------------------------------------------------------------------------
static void SummonBolt(V3 spawn, bool mayRestrike, const BoltOpts* opt = nullptr)
{
    BoltOpts o = opt ? *opt : BoltOpts();
    // a bolt already in the sky gets "new power" instead (the original's re-strike flicker)
    if (!o.decor && mayRestrike && !g_bolts.empty() && Chance(S.restrikeChance))
    {
        for (auto& b : g_bolts)
        {
            if (b.decor) continue;
            b.startSize += RndF(300.f, 500.f);
            b.size = b.startSize;
            b.holdUntil = g_now + (uint32_t)RndI(S.holdMin, S.holdMax) / 2u;
            if (!b.pts.empty()) RadioStatic(b.pts.back());
            return;
        }
    }

    // a ped holding an umbrella may attract it (the bolt then grows up from the ped)
    if (!o.fromScript && !o.decor && S.umbrellaDanger && !MissionRunning())
    {
        static uint32_t u1 = 0, u2 = 0, u3 = 0;
        if (!u1)
        {
            auto joaat = [](const char* s) { uint32_t h = 0; for (; *s; ++s) { h += (uint8_t)tolower(*s); h += h << 10; h ^= h >> 6; } h += h << 3; h ^= h >> 11; h += h << 15; return h; };
            u1 = joaat("ec_char_brollie"); u2 = joaat("ec_char_brollie02"); u3 = joaat("ec_char_brollie03");
        }
        static std::vector<PedRef> peds; CollectPeds(peds, g_playerPos, 300.f);
        for (auto& pr : peds)
        {
            if (!Chance(S.umbrellaChance)) continue;
            if (!PedOk(pr.handle)) continue;
            int obj = 0; { NativeCtx c; c.I(pr.handle); if (Call(nHolding, c)) obj = c.resI(); }
            if (!obj) continue;
            uint32_t model = 0; { NativeCtx c; c.I(obj).P(&model); Call(nObjModel, c); }
            if (model == u1 || model == u2 || model == u3)
            {
                spawn = pr.pos; o.growUp = true; Log("umbrella strike!");
                break;
            }
        }
    }

    Bolt b;
    int n = o.size > 0 ? o.size : RndI(S.minSize, S.maxSize);
    // a normal bolt always reaches the ground (short ones used to die in the air; the distant ones do that now)
    if (o.size <= 0 && !o.decor && !o.horizontal && !o.growUp)
    {
        int need = (int)sqrtf(2.f * (spawn.z > 50.f ? spawn.z : 50.f) / 0.2f) + 12;
        if (n < need) n = need;
    }
    b.fade = RndF(S.minFade, S.maxFade) * o.fadeMul; if (b.fade > 0.95f) b.fade = 0.95f;
    b.startSize = b.size = S.coronaSize * o.sizeMul;
    memcpy(b.color, o.hasColor ? o.color : S.color, 12);
    b.skyOverride = o.hasSky; memcpy(b.sky, o.sky, 12); b.skyBright = o.skyBright;
    b.decor = o.decor; b.skyMul = o.skyMul;
    // only the main bolts may linger; distant ones and heat lightning fade at once, as before
    b.holdUntil = o.decor ? g_now : g_now + (uint32_t)RndI(S.holdMin, S.holdMax);
    b.pts.reserve(n);
    b.pts.push_back(spawn);
    const float seg = 0.2f, zOff = 0.2f;
    bool hitGround = false, hitWater = false; V3 groundPos = { 0, 0, 0 };
    auto addBranch = [&](V3 p)
    {
        std::vector<V3> br; int bn = RndI(20, 40) + 1; br.reserve(bn); br.push_back(p);
        float xv = RndF(-0.5f, 0.5f), yv = RndF(-0.5f, 0.5f);
        for (int k = 1; k < bn; ++k)
        {
            V3 q = br.back();
            q.x += k * xv + RndF(xv < 0 ? xv - 0.3f : xv + 0.3f, xv + 0.5f);
            q.y += k * yv + RndF(yv < 0 ? yv - 0.3f : yv + 0.3f, yv + 0.5f);
            q.z += k * RndF(-0.5f, 0.5f);
            br.push_back(q);
        }
        b.branches.push_back(br);
    };
    if (o.horizontal)
    {
        // cloud to cloud: crawls sideways through the cloud base, never comes down
        float a = RndF(0.f, 6.2831853f); V3 dir = { cosf(a), sinf(a), 0.f }, perp = { -dir.y, dir.x, 0.f };
        for (int i = 1; i < n; ++i)
        {
            V3 p = b.pts.back();
            float st = i * RndF(0.10f, 0.28f), side = i * RndF(-0.22f, 0.22f);
            p.x += dir.x * st + perp.x * side; p.y += dir.y * st + perp.y * side;
            p.z += i * RndF(-0.22f, 0.16f);
            if (p.z < spawn.z - 140.f) p.z = spawn.z - 140.f;
            if (p.z > spawn.z + 60.f) p.z = spawn.z + 60.f;
            if (o.branches && Chance(S.branchChance * 2.f)) addBranch(p);
            b.pts.push_back(p);
        }
    }
    else for (int i = 1; i < n; ++i)
    {
        V3 p = b.pts.back();
        p.x += i * RndF(-seg, seg); p.y += i * RndF(-seg, seg);
        if (o.growUp) { p.z += i * zOff; b.pts.push_back(p); continue; }
        p.z -= i * zOff;
        bool found = false;
        float gz = GroundZ(p.x, p.y, p.z + 2.f, &found);
        if (!found) gz = 0.f;                     // collision not loaded that far away: use sea level
        if (o.stopAbove > 0.f && p.z - gz < o.stopAbove) break;   // decorative: dies in the air
        // water: the river / sea surface stops it before the bottom does
        if (S.water && nWaterH && p.z < 60.f)
        {
            float wh = 0.f; NativeCtx c; c.F(p.x).F(p.y).F(p.z + 50.f).P(&wh);
            if (Call(nWaterH, c) && c.resI() && (wh > gz || !found) && p.z <= wh)
            {
                groundPos = { p.x, p.y, wh };
                b.pts.push_back(groundPos);
                hitGround = hitWater = true;
                break;
            }
        }
        if (p.z <= gz)
        {
            groundPos = { p.x, p.y, gz };
            b.pts.push_back(groundPos);
            hitGround = true;
            break;
        }
        if (o.branches && p.z - gz > 150.f && Chance(S.branchChance)) addBranch(p);
        b.pts.push_back(p);
    }

    if (o.growUp) { hitGround = true; groundPos = spawn; }
    if (o.decor) hitGround = hitWater = false;
    if (hitGround)
    {
        if (hitWater) WaterStrike(groundPos);
        else { CheckSubstationStrike(groundPos); TriggerCarAlarms(groundPos); if (!o.growUp) StartSmoke(groundPos); }
    }
    if (!o.silent && !o.decor) RadioStatic(hitGround ? groundPos : b.pts.back());   // distant/decor bolts never crackle
    // a very close strike is felt at once (the thunder rumble comes with the sound)
    if (hitGround && S.padRumble && nShakePad && Len(groundPos - g_playerPos) < 150.f)
    { NativeCtx c; c.I(0).I(250).I(255); Call(nShakePad, c); }
    // ground strike explosion -- only where it can't hurt a playthrough (never in the water)
    if (hitGround && !hitWater && S.explosions && nExplosion)
    {
        float dPlayer = Len(groundPos - g_playerPos);
        NativeCtx mf; bool mission = Call(nMissionFlag, mf) && mf.resI();
        NativeCtx cs; bool cutsceneDone = !Call(nCutsceneDone, cs) || cs.resI();
        if (dPlayer >= S.explosionMinDist && dPlayer < 1500.f && (S.explosionsInMissions || !mission) && cutsceneDone)
        {
            NativeCtx c; c.F(groundPos.x).F(groundPos.y).F(groundPos.z).I(S.explosionType).F(S.explosionRadius).I(1).I(0).F(S.explosionCamShake);
            Call(nExplosion, c);
        }
    }

    Log("bolt%s%s: %d pts, %d branches, ground=%d water=%d, dist=%.0f m", o.decor ? " (distant)" : "", o.horizontal ? " cloud-to-cloud" : "",
        (int)b.pts.size(), (int)b.branches.size(), hitGround ? 1 : 0, hitWater ? 1 : 0, Len(spawn - g_playerPos));
    if (!o.silent) QueueThunder(b.pts.front());
    g_bolts.push_back(std::move(b));
}

// ---- distant lightning: extra bolts far away that never reach the ground, and silent heat lightning ---------------
static V3 FarPos(float dMin, float dMax)
{
    float a;
    if (Chance(55.f)) a = atan2f(g_camFwd.y, g_camFwd.x) + RndF(-1.3f, 1.3f);   // mostly where you are looking
    else a = RndF(0.f, 6.2831853f);
    float r = RndF(dMin, dMax);
    return { g_playerPos.x + cosf(a) * r, g_playerPos.y + sinf(a) * r, 0.f };
}
static void SpawnDistant()
{
    int n = RndI(1, S.decorMax);
    uint32_t at = g_now;
    for (int k = 0; k < n; ++k)
    {
        PendingBolt pb; pb.at = at; at += (uint32_t)RndI(150, 1400);
        pb.pos = FarPos(S.decorMinDist, S.decorMaxDist);
        pb.o.decor = true; pb.o.silent = !S.decorThunder;
        pb.o.sizeMul = RndF(0.7f, 1.0f); pb.o.skyMul = 0.3f;
        if (Chance(S.decorCloudChance)) { pb.o.horizontal = true; pb.o.size = RndI(50, 110); pb.pos.z = S.height * RndF(0.7f, 1.1f); }
        else { pb.o.stopAbove = RndF(80.f, 260.f); pb.pos.z = S.height * RndF(0.9f, 1.2f); }
        g_pendingBolts.push_back(pb);
    }
}
static void SpawnHeatLightning()
{
    PendingBolt pb; pb.at = g_now;
    pb.pos = FarPos(S.decorMaxDist, S.decorMaxDist + 2500.f);
    pb.o.decor = true; pb.o.silent = true; pb.o.branches = Chance(50.f);
    pb.o.sizeMul = 0.45f; pb.o.skyMul = 0.35f; pb.o.fadeMul = 2.f;
    if (Chance(60.f)) { pb.o.horizontal = true; pb.o.size = RndI(40, 90); pb.pos.z = S.height * RndF(0.8f, 1.1f); }
    else { pb.o.stopAbove = RndF(150.f, 300.f); pb.pos.z = S.height; }
    g_pendingBolts.push_back(pb);
    Log("heat lightning");
}

static V3 RandomWorldPos()
{
    return { RndF(-2687.735f, 2427.867f), RndF(-1381.632f, 3249.545f), S.height };
}
static void TrySpawn(float chancePct)
{
    if (!Chance(chancePct)) return;
    if (!g_scripted.empty() && Chance(S.scriptedChance))
    {
        int cam = 0; { NativeCtx c; c.P(&cam); Call(nRootCam, c); }
        for (auto& sb : g_scripted)
        {
            bool ok = false;
            if (sb.trigger == 0)
            {
                if (cam && nCamSphereVis) { NativeCtx c; c.I(cam).F(sb.spawn.x).F(sb.spawn.y).F(sb.spawn.z).F(20.f); ok = Call(nCamSphereVis, c) && c.resI(); }
            }
            else ok = Len(g_playerPos - sb.trig) < sb.trigDist;
            if (!ok || !Chance(sb.chance)) continue;
            float a = RndF(0.f, 6.2831853f), r = RndF(sb.rMin, sb.rMax);
            SummonBolt({ sb.spawn.x + cosf(a) * r, sb.spawn.y + sinf(a) * r, sb.spawn.z }, false, &sb.o);
            return;
        }
    }
    if (S.dangerHeight && g_playerPos.z > S.dangerHeightZ && Chance(S.dangerChance))
    {
        float a = RndF(0.f, 6.2831853f), r = RndF(0.f, 200.f);
        SummonBolt({ g_playerPos.x + cosf(a) * r, g_playerPos.y + sinf(a) * r, g_playerPos.z + S.height }, true);
        return;
    }
    // lightning prefers tall things: sometimes test a few spots around you and take the highest one
    if (Chance(S.tallTargetChance))
    {
        V3 best = { 0, 0, -1e9f };
        for (int k = 0; k < 10; ++k)
        {
            float a = RndF(0.f, 6.2831853f), r = RndF(150.f, S.tallTargetRadius);
            float x = g_playerPos.x + cosf(a) * r, y = g_playerPos.y + sinf(a) * r;
            bool found = false; float gz = GroundZ(x, y, 1000.f, &found);
            if (found && gz > best.z) best = { x, y, gz };
        }
        if (best.z > 25.f) { Log("tall target at %.0f m height", best.z); SummonBolt({ best.x, best.y, S.height }, true); return; }
    }
    SummonBolt(RandomWorldPos(), true);
}

static void DrawBolts(float dt60)
{
    int total = 0;
    for (auto& b : g_bolts) { total += (int)b.pts.size(); for (auto& br : b.branches) total += (int)br.size(); }
    int budget = S.maxCoronas;
    if (g_coronaCount)
    {
        int used = 0; FP_TRY { used = *g_coronaCount; } FP_EXCEPT { g_coronaCount = nullptr; }
        int freeSlots = g_coronaMax - used - 24;                     // leave a few for the game
        if (freeSlots < budget) budget = freeSlots;
        static uint32_t s_lastLog = 0;
        if (total && budget < total && (!s_lastLog || (int32_t)(g_now - s_lastLog) > 10000)) { s_lastLog = g_now; Log("corona buffer busy: %d used by the game, bolt %d -> %d points", used, total, budget < 0 ? 0 : budget); }
    }
    if (budget < 12) budget = 12;
    int step = total > budget ? (total + budget - 1) / budget : 1;
    int r = (int)(S.color[0] * 255.f), g = (int)(S.color[1] * 255.f), bl = (int)(S.color[2] * 255.f);

    float fadeSum = 0.f;
    for (size_t i = 0; i < g_bolts.size();)
    {
        Bolt& b = g_bolts[i];
        fadeSum += b.fade;
        r = (int)(b.color[0] * 255.f); g = (int)(b.color[1] * 255.f); bl = (int)(b.color[2] * 255.f);
        int drawn = 0;
        // while it holds, the channel keeps flickering a little (real strokes pulse); then it fades
        const bool holding = (int32_t)(b.holdUntil - g_now) > 0;
        const float flick = holding ? (Rnd01() < 0.12f ? 0.6f : RndF(0.85f, 1.f)) : 1.f;
        auto drawList = [&](const std::vector<V3>& v)
        {
            for (size_t k = 0; k < v.size(); k += step)
            {
                const V3& p = v[k];
                V3 d = p - g_camPos;
                if (Dot(d, g_camFwd) < -50.f) continue;             // behind the camera
                NativeCtx c; c.F(p.x).F(p.y).F(p.z).F(b.size * flick).I(0).F(0.f).I(r).I(g).I(bl);
                Call(nCorona, c);
                ++drawn;
            }
        };
        drawList(b.pts);
        for (auto& br : b.branches) drawList(br);

        // a few scene lights along the bolt carry the light of all its points
        if (S.lightEnabled && nLight && !b.pts.empty() && !b.decor)
        {
            const int nL = 4;
            float scale = b.size / (b.startSize > 1.f ? b.startSize : 1.f);
            float inten = S.lightIntensity * (float)b.pts.size() / nL * (scale > 1.f ? 1.f : scale);
            for (int k = 0; k < nL; ++k)
            {
                const V3& p = b.pts[(b.pts.size() - 1) * (k + 1) / nL];
                NativeCtx c; c.F(p.x).F(p.y).F(p.z).I(r).I(g).I(bl).F(S.lightRange).F(inten);
                Call(nLight, c);
            }
        }

        if (holding) ++i;
        else if (b.size > 25.f)
        {
            float f = b.fade > 0.999f ? 0.999f : b.fade;
            b.size *= powf(1.f - f, dt60);
            ++i;
        }
        else g_bolts.erase(g_bolts.begin() + i);
    }
    if (!g_bolts.empty()) g_avgFade = fadeSum / (float)g_bolts.size();
}

// ---- input ---------------------------------------------------------------------------------------------------------
static bool GameHasFocus()
{
    HWND hw = GetForegroundWindow(); if (!hw) return false;
    DWORD pid = 0; GetWindowThreadProcessId(hw, &pid);
    return pid == GetCurrentProcessId();
}
static bool KeyEdge(int vk, bool& was)
{
    bool down = (GetAsyncKeyState(vk) & 0x8000) != 0;
    bool edge = down && !was; was = down; return edge;
}

// ---- per-frame (sim thread) ------------------------------------------------------------------------------------
static bool g_audioInitTried = false;
static int  g_frameFaults = 0;

static void FrameInner()
{
    if (!g_natsResolved) { ResolveNatives(); InstallCoronaGate(); }
    if (!nTimer || !nCorona) return;

    { uint32_t t = 0; NativeCtx c; c.P(&t); Call(nTimer, c); g_now = t; }
    float dtMs = g_lastNow ? (float)(g_now - g_lastNow) : 16.6f;
    g_lastNow = g_now;
    if (dtMs < 0.f || dtMs > 500.f) dtMs = 16.6f;
    float dt60 = dtMs / (1000.f / 60.f);

    if (!g_audioInitTried) { g_audioInitTried = true; InitAudio(); LoadData(); }

    bool focus = GameHasFocus();
    NativeCtx pm; bool pauseMenu = Call(nPauseMenu, pm) && pm.resI();
    PauseAllSounds(pauseMenu || !focus);
    if (pauseMenu) return;

    NativeCtx pp; pp.I(0); if (!Call(nPlayerPlaying, pp) || !pp.resI()) return;
    { int ped = 0; NativeCtx c; c.I(0).P(&ped); Call(nPlayerChar, c); g_player = ped; }
    if (!g_player) return;
    { float x = 0, y = 0, z = 0; NativeCtx c; c.I(g_player).P(&x).P(&y).P(&z); if (Call(nCharCoords, c)) g_playerPos = { x, y, z }; }
    {
        int cam = 0; NativeCtx c; c.P(&cam); Call(nRootCam, c);
        if (cam)
        {
            float x = 0, y = 0, z = 0; NativeCtx a; a.I(cam).P(&x).P(&y).P(&z);
            if (Call(nCamPos, a)) g_camPos = { x, y, z };
            float rx = 0, ry = 0, rz = 0; NativeCtx b; b.I(cam).P(&rx).P(&ry).P(&rz);
            bool rotOk = Call(nCamRot, b);
            if (rotOk && rx == 0.f && ry == 0.f && rz == 0.f)      // root cam without rotation: ask the gameplay cam
            {
                int gc = 0; NativeCtx g; g.P(&gc); Call(nGameCam, g);
                if (gc) { NativeCtx b2; b2.I(gc).P(&rx).P(&ry).P(&rz); Call(nCamRot, b2); }
            }
            if (rotOk)
            {
                const float d2r = 0.01745329f; float p = rx * d2r, h = rz * d2r;
                g_camFwd = { -sinf(h) * cosf(p), cosf(h) * cosf(p), sinf(p) };
                g_camRight = { cosf(h), sinf(h), 0.f };
            }
        }
        else g_camPos = g_playerPos;
    }

    // test keys
    if (S.testKeys && focus)
    {
        static bool w8 = false, w9 = false, w10 = false, w11 = false;
        bool ctrl = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
        bool e8 = KeyEdge(VK_F8, w8), e9 = KeyEdge(VK_F9, w9), e10 = KeyEdge(VK_F10, w10), e11 = KeyEdge(VK_F11, w11);
        if (ctrl && e9) { SpawnDistant(); SpawnDistant(); Toast("Project Thunder CE: rayos lejanos de prueba"); }
        if (ctrl && e8)
        {
            if (!g_blackout)
            {
                StartBlackout(true); Toast("Project Thunder CE: apagon de prueba (Ctrl+F8 para terminarlo)");
                const Substation* best = nullptr; float bd = 400.f;   // test sparks too, at the closest substation
                for (auto& ss : g_substations) { float d = Len(ss.pos - g_playerPos); if (d < bd) { bd = d; best = &ss; } }
                if (best) StartSparks(best->pos);
            }
            else { EndBlackout(); Toast("Project Thunder CE: vuelve la luz"); }
        }
        if (ctrl && e11)
        {
            if (!g_forcedStorm) { NativeCtx c; c.I(W_LIGHTNING); Call(nForceWeatherNow, c); g_forcedStorm = true;
                Toast("Project Thunder CE: lluvia con truenos (Ctrl+F11 para volver al clima normal)"); }
            else { NativeCtx c; Call(nReleaseWeather, c); g_forcedStorm = false;
                Toast("Project Thunder CE: clima normal"); }
        }
        if (ctrl && e10)
        {
            float d = RndF(250.f, 700.f);
            float fx = g_camFwd.x, fy = g_camFwd.y, l = sqrtf(fx * fx + fy * fy);
            if (l < 0.01f) { fx = 0; fy = 1; l = 1; }
            SummonBolt({ g_camPos.x + fx / l * d + RndF(-60.f, 60.f), g_camPos.y + fy / l * d + RndF(-60.f, 60.f), S.height }, false);
        }
    }

    // storm logic (the original's WaitTick, every 1.25 s)
    if (S.enabled && (int32_t)(g_now - g_nextWeatherTick) >= 0)
    {
        g_nextWeatherTick = g_now + 1250;
        uint32_t oldW = 0, newW = 0; bool haveW = false;
        FP_TRY { if (g_oldWeather && g_newWeather) { oldW = *g_oldWeather; newW = *g_newWeather; haveW = true; } }
        FP_EXCEPT { g_oldWeather = g_newWeather = nullptr; }
        if (!haveW) { int w = 0; NativeCtx c; c.P(&w); Call(nCurWeather, c); oldW = newW = (uint32_t)w; }

        uint32_t hr = 0, mn = 0; NativeCtx t; t.P(&hr).P(&mn); Call(nTimeOfDay, t);
        NativeCtx cs; bool inCutscene = Call(nCutsceneDone, cs) && !cs.resI();
        NativeCtx ns; bool net = Call(nNetSession, ns) && ns.resI();
        if (mn < 57 && !net && (S.inCutscenes || !inCutscene))
        {
            // heavy rain (RAIN) without the game's thunderstorm weather: some rain spells come with thunder too
            static bool s_inRain = false, s_rainStorm = false;
            const bool heavyRain = oldW == 4 || newW == 4;
            if (heavyRain && !s_inRain) { s_inRain = true; s_rainStorm = Chance(S.rainThunderChance); Log("heavy rain starts -- %s", s_rainStorm ? "with thunder" : "without thunder"); }
            else if (!heavyRain) s_inRain = s_rainStorm = false;
            const bool lightningW = oldW == W_LIGHTNING || newW == W_LIGHTNING;
            if (oldW == W_LIGHTNING && newW == W_LIGHTNING) TrySpawn(S.chanceOngoing);
            else if (oldW != W_LIGHTNING && newW == W_LIGHTNING) TrySpawn(S.chanceBegin);
            else if (oldW == W_LIGHTNING && newW != W_LIGHTNING) TrySpawn(S.chanceEnd);
            else if (s_rainStorm) TrySpawn(S.chanceOngoing);
            const bool storm = lightningW || s_rainStorm;
            g_stormNow = storm;
            if (storm && S.decor && Chance(S.decorChance)) SpawnDistant();
            // heat lightning: far storms flashing on the horizon on cloudy / rainy nights, no thunder
            const bool night = hr >= 20 || hr < 6;
            auto cloudy = [](uint32_t w) { return w == 2 || w == 3 || w == 4 || w == 5; };
            if (!storm && S.heat && night && (cloudy(oldW) || cloudy(newW)) && Chance(S.heatChance)) SpawnHeatLightning();
        }
    }

    // thunder
    NativeCtx in; bool interior = Call(nInterior, in) && in.resI();
    for (size_t i = 0; i < g_pending.size();)
    {
        if ((int32_t)(g_now - g_pending[i].at) >= 0)
        {
            if (g_pending[i].file.empty()) StartThunder(g_pending[i].pos, interior);
            else PlayWorldSound(g_pending[i].file.c_str(), g_pending[i].pos, g_pending[i].vol, g_pending[i].audible);
            g_pending.erase(g_pending.begin() + i);
        }
        else ++i;
    }
    if (B.ok)
    {
        for (size_t i = 0; i < g_sounds.size();)
        {
            ActiveSound& s = g_sounds[i];
            if (!s.paused && B.IsActive(s.h) == BASS_ACTIVE_STOPPED) { B.Free(s.h); g_sounds.erase(g_sounds.begin() + i); continue; }
            ApplySoundParams(s, interior);
            ++i;
        }
    }

    for (size_t i = 0; i < g_pendingBolts.size();)
    {
        if ((int32_t)(g_now - g_pendingBolts[i].at) >= 0) { PendingBolt pb = g_pendingBolts[i]; g_pendingBolts.erase(g_pendingBolts.begin() + i); SummonBolt(pb.pos, false, &pb.o); }
        else ++i;
    }
    UpdateFlashes();
    SkyFlashUpdate(!g_bolts.empty(), dt60);
    DrawBolts(dt60);
    {
        static uint32_t s_nextExplCheck = 0;
        if ((S.blackout || S.sparks) && (int32_t)(g_now - s_nextExplCheck) >= 0) { s_nextExplCheck = g_now + 1000; CheckSubstationExplosions(); }
    }
    UpdateBlackout(dtMs);
    UpdateSparks();
    UpdateGenerators();
    UpdateSmoke();
    UpdateStElmo();

    if (g_toastPending && nPrintNow)
    {
        g_toastPending = false;
        static char s_t[160]; strcpy(s_t, g_toast);
        NativeCtx c; c.P("STRING").P(s_t).I(3500).I(1); Call(nPrintNow, c);
    }
}

static void OnGameFrame()
{
    if (g_frameFaults > 20) return;
    FP_TRY { FrameInner(); }
    FP_EXCEPT { if (++g_frameFaults == 20) Log("too many faults, mod stopped"); }
}

// ---- game-process hook (same call site FusionFix uses; chains with other mods on it) -------------------------------
typedef void(__cdecl* voidfn_t)();
static voidfn_t g_origGameProcess = nullptr;

__declspec(naked) void FrameStub()
{
    __asm
    {
        call  dword ptr[g_origGameProcess]
        pushad
        sub   esp, 0x80
        movups[esp + 0x00], xmm0
        movups[esp + 0x10], xmm1
        movups[esp + 0x20], xmm2
        movups[esp + 0x30], xmm3
        movups[esp + 0x40], xmm4
        movups[esp + 0x50], xmm5
        movups[esp + 0x60], xmm6
        movups[esp + 0x70], xmm7
        call  OnGameFrame
        movups xmm0, [esp + 0x00]
        movups xmm1, [esp + 0x10]
        movups xmm2, [esp + 0x20]
        movups xmm3, [esp + 0x30]
        movups xmm4, [esp + 0x40]
        movups xmm5, [esp + 0x50]
        movups xmm6, [esp + 0x60]
        movups xmm7, [esp + 0x70]
        add   esp, 0x80
        popad
        ret
    }
}

static bool AddrIsExecutable(uintptr_t a)
{
    MEMORY_BASIC_INFORMATION mbi;
    if (!VirtualQuery((void*)a, &mbi, sizeof(mbi)) || mbi.State != MEM_COMMIT) return false;
    DWORD p = mbi.Protect & 0xFF;
    return p == PAGE_EXECUTE || p == PAGE_EXECUTE_READ || p == PAGE_EXECUTE_READWRITE || p == PAGE_EXECUTE_WRITECOPY;
}
static bool InstallFrameHook()
{
    uintptr_t hits[64];
    int n = FindAll("E8 ? ? ? ? E8 ? ? ? ? E8 ? ? ? ? B9 ? ? ? ? E8 ? ? ? ? E8 ? ? ? ? E8 ? ? ? ? E8 ? ? ? ? B9", hits, 64);
    static const int callOff[7] = { 0, 5, 10, 20, 25, 30, 35 };
    uintptr_t lo = g_moduleBase, hi = g_moduleBase + g_moduleSize, chosen = 0, orig = 0;
    for (int i = 0; i < n && i < 64 && !chosen; ++i)
    {
        uintptr_t m = hits[i], t0 = 0; bool good = true;
        for (int c = 0; c < 7; ++c)
        {
            uintptr_t tgt = m + callOff[c] + 5 + *(int32_t*)(m + callOff[c] + 1);
            if (c == 0) { t0 = tgt; continue; }
            if (tgt < lo || tgt >= hi) { good = false; break; }
        }
        if (!good) continue;
        if (!(t0 >= lo && t0 < hi) && !AddrIsExecutable(t0)) continue;
        chosen = m; orig = t0;
    }
    if (!chosen) { Log("game-process call site not found -- mod inactive"); return false; }
    g_origGameProcess = (voidfn_t)orig;
    DWORD op; VirtualProtect((void*)chosen, 5, PAGE_EXECUTE_READWRITE, &op);
    *(int32_t*)(chosen + 1) = (int32_t)((uintptr_t)&FrameStub - (chosen + 5));
    VirtualProtect((void*)chosen, 5, op, &op);
    FlushInstructionCache(GetCurrentProcess(), (void*)chosen, 5);
    Log("game-process hook at %p (previous target %p)", (void*)chosen, (void*)orig);
    return true;
}

// ---- startup ----------------------------------------------------------------------------------------------------------
static DWORD WINAPI InitThread(LPVOID)
{
    // the exe is fully mapped by the time plugins load; hook right away so no frame is missed
    ResolveGlobals();
    InstallFrameHook();
    return 0;
}

BOOL APIENTRY DllMain(HMODULE h, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        g_selfInst = h;
        DisableThreadLibraryCalls(h);
        char p[MAX_PATH] = { 0 };
        GetModuleFileNameA(h, p, MAX_PATH);
        char* dot = strrchr(p, '.'); if (dot) *dot = 0;
        snprintf(g_logPath, MAX_PATH, "%s.log", p);
        snprintf(g_iniPath, MAX_PATH, "%s.ini", p);
        char* sl = strrchr(p, '\\');
        if (sl) { *sl = 0; snprintf(g_dataDir, MAX_PATH, "%s\\ProjectThunderCE", p); }
        DeleteFileA(g_logPath);
        FpSehInit();
        GetMainModuleRange();
        g_rng ^= GetTickCount() * 2654435761u; if (!g_rng) g_rng = 1;
        LoadSettings();
        Log("Project Thunder CE %s -- enabled=%d sound=%d explosions=%d", PTCE_VERSION, S.enabled, S.soundEnabled, S.explosions);
        InitThread(nullptr);
    }
    else if (reason == DLL_PROCESS_DETACH)
    {
        SkyRestoreAll();
        TcBlackoutRestore();
        g_lampsOff = 0; g_coronaGate = 0; g_tlOff = 0;
        if (g_timeModelsScanned) RestoreAllModels();
    }
    return TRUE;
}
