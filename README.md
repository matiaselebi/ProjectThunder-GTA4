# Project Thunder

Realistic thunderstorms for **GTA IV: The Complete Edition (1.2.0.59)** with FusionFix.

Project Thunder CE is a standalone `.asi` port of [Project Thunder IV](https://github.com/ClonkAndre/ProjectThunderIV) by ItsClonkAndre, rebuilt for the Complete Edition. You don't need IV-SDK .NET, ScriptHook or ClonksCodingLib. It doesn't touch your save games or achievements.

## Features

**Lightning and thunder**
- Branching lightning bolts during the game's thunderstorm weather. They light up the city, the sky and the clouds.
- Thunder comes from the bolt's direction, with the real sound delay. Close strikes sound like a sharp crack, distant ones like a long, low rumble.
- Echo depends on where you are. Thunder rolls between Algonquin's skyscrapers and sounds dry in parks, in Alderney and out on the water.
- Inside buildings, thunder sounds muffled.
- Bolts look for high ground: towers, antennas, the tallest building nearby, and peds holding an umbrella. If you're on a high rooftop or in a helicopter, a bolt is more likely to strike near you.
- When a bolt hits the ground there's an explosion. This never happens during missions or cutscenes, or within 35 m of the player.
- Bolts that hit water make a big splash, a flash on the water and a hiss, with no explosion.
- Distant decorative bolts appear 1.5–4 km away: cloud-to-cloud flashes and bolts that die out in the air.
- Heat lightning: silent flashes on the horizon on cloudy or rainy nights.
- St. Elmo's fire: a flickering blue glow on the roofs of nearby tall buildings during the storm.

**Effects of a strike**
- Smoke and small flames where a bolt lands.
- Car alarms go off near the strike.
- Peds react: they look toward the bolt and say something.
- Static on the car radio when a bolt strikes close by. It only plays while the radio is on.
- Distant dogs bark after a loud thunderclap.
- With NativeDualSense, the DualSense rumbles with close thunder.

**Blackouts**
A bolt (or one of your explosions) hitting an electrical substation can black out the city for 25–80 seconds:
- Lit windows, signs and neon models switch off gradually across the city.
- Street lamps, glow halos, traffic lights and distant city lights go dark.
- The night gets darker and you hear a power-down sound. Niko and nearby peds react.
- Trains slow to a stop and Pay'n'Spray shops close until the power comes back.
- Sparks and an electric-arc sound at the substation that was hit.
- Hospitals and police stations keep their lights on, with emergency generators you can hear when you're close.
- Car lights and the lightning itself stay visible.

## Requirements
- GTA IV: The Complete Edition **1.2.0.59** (Steam).
- [FusionFix](https://github.com/ThirteenAG/GTAIV.EFLC.FusionFix), or any other ASI loader.

## Installation
1. Download the latest release and copy everything inside `Main files` into your game folder, the one that contains `GTAIV.exe`. This puts `ProjectThunderCE.asi`, `ProjectThunderCE.ini` and the `ProjectThunderCE` folder in `plugins`.
2. *(Recommended)* Copy everything inside `Optional - remove vanilla thunder` into the game folder too. It removes the game's own thunder sounds and flash so they don't mix with the mod's:
   - `update\pc\audio\sfx\resident.rpf` is the rain audio without the built-in thunder.
   - `update\pc\data\timecyclemodifiers2.dat` removes the vanilla lightning flash.

To uninstall, delete those files.

## Settings
All settings are in `plugins\ProjectThunderCE.ini`. Every option has a comment above it. The sections are: `General`, `LightningBolt`, `Light`, `Sky`, `Sound`, `Danger`, `Explosion`, `Rumble`, `CarAlarms`, `Reactions`, `Blackout`, `Sparks`, `RadioStatic`, `Water`, `DistantLightning`, `Echo`, `Generators`, `Dogs`, `Storms`, `Smoke` and `StElmosFire`.

You can add your own thunder sounds (mp3, wav or ogg) to `plugins\ProjectThunderCE\Thunder\Close` and `...\Far`.

### Test keys
You can turn these off with `TestKeys=0`.

| Keys | Action |
|---|---|
| Ctrl + F11 | Start a thunderstorm, or go back to normal weather. Return to normal weather before saving. |
| Ctrl + F8 | Start or end a test blackout |
| Ctrl + F10 | Bolt in front of the camera |
| Ctrl + F9 | A burst of distant bolts |

## Known limitations
- Some screens and neon signs are painted into building textures as self-lit (emissive) surfaces, not separate objects. These can't be switched off from a plugin, so they stay lit during blackouts.
- Lowering police vision during blackouts, a feature of the original mod, isn't included.
- Only tested on 1.2.0.59 with FusionFix.

## Log
`plugins\ProjectThunderCE.log` is written next to the `.asi`. Please attach it when you report a bug.

## Building
`source\ProjectThunderCE.cpp` is a single file. With clang targeting MinGW:

```
clang++ --target=i686-w64-mingw32 -msse2 -mfpmath=sse -fms-extensions -fasm-blocks -fdeclspec -O2 -shared -static -static-libgcc -static-libstdc++ -s -o ProjectThunderCE.asi ProjectThunderCE.cpp -luser32
```

## Credits
- **ItsClonkAndre** made [Project Thunder IV](https://github.com/ClonkAndre/ProjectThunderIV). This mod is a port of it, and its design, settings, data files (substations, scripted lightning spots) and blackout sound come from it.
- The thunder sounds are taken from the GTA IV sound pack that ships with Project Thunder IV. That audio is © Rockstar Games.
- Dog barks are CC0 (public domain) recordings from [Freesound](https://freesound.org) by rickyjezz, ahill86, zeshoog, UnderlinedDesigns, Jace and magnus589, taken from the [ESC-50](https://github.com/karolpiczak/ESC-50) dataset by K. J. Piczak and processed to sound distant.
- The spark, radio static, water strike and generator sounds were synthesized for this mod.
- Audio playback uses [BASS](https://www.un4seen.com) by Un4seen Developments (`bass.dll`, free for non-commercial use).
- Thanks to the [FusionFix](https://github.com/ThirteenAG/GTAIV.EFLC.FusionFix) team; their native invoker approach and game-address research made this port possible.
- Made by Matias, with AI assistance (Claude).

## License
GPL-3.0, the same license as the original Project Thunder IV. See `LICENSE`. The source code is in `source\`.
