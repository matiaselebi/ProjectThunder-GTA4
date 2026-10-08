# Changelog

## 1.7
- Full city-wide blackout, ported from the original. It now also turns off every glow halo (street lamps, signs, windows) and the traffic lights. Trains slow to a stop and Pay'n'Spray shops close. Car lights and the lightning itself stay visible.
- Lit windows and signs now switch off even while you're looking at them. Before, they only went dark once the camera turned away.
- Radio static no longer plays with the radio off or for distant decorative bolts. Its default range is now 800 m.
- After a save is loaded, the Pay'n'Spray state is reset, so a game saved during a blackout can't keep the shops closed.

## 1.6
- Main bolts sometimes stay on screen for up to 1 second, flickering, before they fade. Distant bolts fade right away.
- Quieter dogs.

## 1.5
- Bolts always reach the ground. The game's per-frame light buffer used to fill up at night and cut off the bottom of the bolt.
- Distant bolts come in a burst of 1–2 about every 10 s and light up the sky less.
- Dogs bark from farther away and are quieter.
- The test blackout key moved to Ctrl+F8.

## 1.4
- Optional thunder during heavy rain (`[Storms]`). It's off by default, so only the game's thunderstorm weather is affected.
- Smoke and small flames where a bolt lands.
- St. Elmo's fire on tall rooftops.

## 1.3
- Lightning striking water.
- Distant decorative bolts and heat lightning.
- Thunder echo that depends on the surroundings.
- Emergency generators at hospitals and police stations during blackouts.
- Distant dogs barking after loud thunder.

## 1.2
- Sparks at the substation that was hit.
- Radio static from nearby strikes.

## 1.1
- Separate close and far thunder sounds, plus support for custom sound packs.
- DualSense rumble with close thunder (via NativeDualSense).
- Bolts strike high points.
- Car alarms near strikes.
- Thunder sounds muffled indoors.

## 1.0
- First release: a port of Project Thunder IV as a standalone `.asi` for the Complete Edition.
