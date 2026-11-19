# DX12 Plane Game

DirectX 12 flight prototype with a spherical world (5 km radius), procedural mountains,
sea, a third-person chase camera, basic throttle/stall behavior and terrain contact.
Flight starts stopped on the departure airport runway. This is an arcade flight model, not an aerodynamic simulator.

## Build and run

Requires Windows, Visual Studio C++ tools, Windows SDK and CMake 3.20+.

```powershell
cmake -S . -B build
cmake --build build --config Debug
.\build\Debug\DX12PlaneGame.exe
```

## Controls

- W / Up: nose down; S / Down: nose up
- A / Left, D / Right: bank and turn
- Shift: increase throttle; Ctrl: cut power and brake on the ground, decrease throttle in flight
- R: restart the entire route, clearing completed flights and restoring fuel
- E: hold to refuel while parked at idle in a yellow F bay
- Space: ground brakes
- F: toggle fog
- M: mute/unmute audio
- T: advance the day/night clock by three hours
- Escape: exit

Speed, altitude, throttle and fuel appear in the window title.
Controls only apply while the game has focus.

## Assets

| Path | Use |
| --- | --- |
| `data/model/Airplane/airplane.obj` | Aircraft geometry, normals and UVs |
| `data/texture/plane_diffuse.png` | Aircraft color |
| `data/texture/plane_normal.png` | Aircraft normal map |
| `data/texture/field005.jpg` | Terrain base |
| `data/texture/field004.jpg` | Terrain detail blended on slopes |
| `data/texture/sea.jpg` | Sea |

The built-in static OBJ loader requires no external library. It triangulates convex
faces, supports negative indices and computes missing normals. The supplied model's
orientation is corrected and its span is scaled to 24 units. PNG/JPG decoding uses WIC.
DDS files, original MTL paths and animations are not used.

Optional overrides: `data/plane.obj`, `data/field.png`, `data/sea.png`.
Custom OBJ models should face +Z after conversion from right-handed coordinates,
with +Y up. When no OBJ exists, optional Assimp support loads `data/plane.fbx` or
`data/model/Airplane/airplane.fbx`. A missing model uses the procedural aircraft.

After changing assets, rebuild and restart. Data is copied beside the executable on
every build; running from the repository uses its data directory.

## Source layout

```text
src/
  main.cpp
  IRenderer.h
  RendererFactory.h / .cpp
  shadersource.h
  engine/Engine.h / .cpp
  window/window.h / .cpp
  DX12/dx12renderer.h / .cpp
  model/model.h / .cpp
  model/mesh.h
  game/game.h / .cpp
  player/player.h / .cpp
  world/WorldGenerator.h / .cpp
  world/Airport.h / .cpp
  world/DayNightCycle.h / .cpp
  sound/sound.h / .cpp
  world/sea.h / .cpp
  hud/hud.h / .cpp
  hud/hudshader.h
```

`main` starts the engine. `Engine` owns the window and renderer and runs the update/render
loop. `Game` owns the player, world generation, fog and airport. `Player` handles aircraft
controls, movement, ground contact and the chase camera. `Window` handles native messages and resize events. `RendererFactory` creates the
single DirectX 12 implementation through `IRenderer`. `Model` reads static OBJ geometry.
Shader source lives in `shadersource.h`. The project has no test target or DX11 backend.
The executable uses a console entry point like the reference project; errors go to stderr.

The renderer waits for each GPU frame for a simple resource lifetime model.
Future work: building collisions, terrain LOD and richer flight physics.

## Streaming world and airport

Each launch picks a random seed. Terrain is sampled from continuous 3D noise on a
cube sphere, so returning to a released chunk reproduces the same terrain. All-around
coverage extends to 2.9 km; the forward prefetch region extends to 4.1 km. Missing
chunks are generated with a four-chunk frame budget, and distant CPU/GPU data is released.
The local terrain is generated before the first frame. This is bounded streaming on
a finite spherical planet; it is not an infinite flat world.

The airfield has a curved 1.28 km runway, markings, a taxiway, apron, two hangars and
control tower. To take off, hold Shift to raise throttle, then
pull up with S after passing roughly 200 km/h. Ground brakes use Ctrl or Space. The current
arcade ground contact follows terrain/runway height; buildings have no collision yet.

Fog starts at 650 m and reaches the sky color at 2.4 km. Adjust `Fog` in `game/game.h`.
The reference project's start/end-distance fog and XAudio2 lifecycle were adapted to
DX12; its DX11 buffers and shader bindings were not copied into the renderer.

## Audio

XAudio2 runs separately from rendering. Throttle controls the engine pitch/volume,
and airspeed controls the optional wind loop. Audio pauses when the game loses focus
or is minimized. Press M to mute it. An unavailable sound device does not stop the game.

Optional files (16-bit PCM WAV, mono or stereo):

- `data/SE/engine.wav`: looping engine recording (a synthesized tone is used if absent).
- `data/SE/wind.wav`: looping wind recording.
- `data/BGM/flight.wav`: looping background music.

No WAV assets were present in the supplied reference directory, so none were copied.
Rebuild and restart after adding recordings. Comments in the source use Turkish first,
then English; `.clang-format` and CRLF settings keep the source style consistent.

## Day/night cycle

A full day lasts ten real minutes, starting at 06:30. `T` advances the clock by three
hours. The clock is shown in the window title. Change `DayLengthSeconds` or the
starting hour in `world/DayNightCycle.h` to tune the cycle.

The sun moves around the spherical world. Its local elevation controls daylight,
surface illumination, sea highlights and fog. Sunrise and sunset warm the horizon
and direct light toward orange; the sun disc and halo follow the same direction.
At night, `data/texture/night.jpg` repeats across six flat sky planes. Their frame
follows the planet's local up direction. The texture keeps its aspect ratio and uses
small mirrored tiles rather than spherical or panoramic UVs. Tile density scales with
viewport resolution instead of enlarging the 640x480 image over the entire screen.
A brighter blue ambient floor keeps night terrain and the aircraft readable.
Missing night textures fall back to a dark sky. Sky rendering uses its own DX12
pipeline with depth writes disabled, so it cannot cover terrain or the aircraft.

The running game can lock `build/Release/DX12PlaneGame.exe` during linking. Close it
before rebuilding that path. This change was also built independently at
`build/daynight/Release/DX12PlaneGame.exe` while the original executable was locked.

## Ocean waves and flight HUD

The sea uses two moving sine waves, adapted from the reference sea implementation.
The vertex shader displaces vertices along the planet's radial up direction and
computes the corresponding surface normals. Pixel ripples, moving UVs, Fresnel color
and sun highlights provide smaller detail. `world/sea.h` supplies wave amplitudes,
frequencies and speeds to shader compilation. The sea mesh has 256x512 segments.
No vertex-buffer rebuild is needed each frame.

The HUD follows the supplied layout: a translucent altitude/speed panel at top left,
clock and date at top center, a central reticle and a throttle arc at bottom right.
Values come from the player and game clock; speed uses km/h and altitude uses metres
above sea level. The calendar starts on Nov 16, 2026 and advances at midnight, including
when T skips across midnight. Labels follow the English reference image.

HUD geometry and layout are in `hud/hud.cpp`; the DX12 overlay shader is in
`hud/hudshader.h`. A Segoe UI font atlas is generated in memory with GDI, then sampled
by the DX12 HUD pipeline. No DirectX 11 rendering or external UI package is used.
The overlay scales to the viewport and uses alpha blending with depth disabled.
It was checked from 640x360 to 3840x2160 using an offscreen layout preview.

This version was built at `build/hud/Release/DX12PlaneGame.exe` to keep the executable
paths used by earlier running builds available.

## Nearby airports and refueling

ALPHA and BRAVO use seed-based random positions and runway headings, 3.5-5.5 km apart. Both have flattened terrain,
a runway, taxiway, apron and a yellow fuel-service bay with an F marking. The same
airport frames drive mesh placement, terrain flattening and ground contact.

The mission assigns the navigation target. The HUD lists both airport distances,
shows a turn direction for the assigned target and marks airports in the forward view.
The fuel panel shows remaining fuel, an estimated cruise range and refueling status.

To refuel, taxi onto the yellow F bay on the apron, stop below 1 m/s, reduce throttle
to idle with Ctrl, then hold E. Ctrl cuts power and brakes on the ground; Space is an additional brake key. Refueling is blocked
while airborne, outside the bay, moving too fast or above idle power. An empty tank
takes roughly 8.3 seconds to fill. Both airports provide the same service.

Fuel consumption is configured in `src/player/fuel.h`. A full tank has 100 units;
full throttle consumes 0.45 units/second. The offscreen check covered an 8 km round trip
at 80 m/s plus 60 seconds of approach time at 65% throttle, leaving 53.2% fuel.
Actual fuel use depends on speed, throttle and time spent maneuvering. HUD range is
an estimate using 100 m/s and 65% throttle.

Latest build for this feature: `build/airports/Release/DX12PlaneGame.exe`.

## Random airport missions and ground braking

Airport position, heading and separation are deterministic for each world seed.
A new game chooses a fresh seed. ALPHA is the starting runway and BRAVO is the first
mandatory destination. Fly to the assigned airport, land, taxi into its yellow F bay,
stop and reduce throttle to idle. The flight is then counted and the return destination
is assigned automatically. Passing overhead, parking at the wrong airport or remaining
in the same bay cannot complete another flight.

The HUD shows the current flight number, locked destination, distance and a completion
notice. Manual destination switching with N and teleporting with H are disabled.
R restarts the route from ALPHA and clears its progress, so restarting cannot skip a leg.

Holding Ctrl on the ground immediately cuts engine power and applies wheel brakes.
In flight, Ctrl keeps its throttle-reduction behavior and does not apply wheel brakes.
Hold E while parked at idle in a fuel bay to replenish the tank. The distance limit
keeps both airport legs within the existing fuel budget, including approach reserves.

Latest build: `build/routes/Release/DX12PlaneGame.exe`.