Current assets:
model/Airplane/airplane.obj: aircraft geometry (no Assimp required).
texture/plane_diffuse.png and texture/plane_normal.png: aircraft surface.
texture/field005.jpg: terrain base; texture/field004.jpg: slope detail.
texture/sea.jpg: sea surface.

Rebuild and restart after changes. Data is copied beside the executable each build.
Optional overrides: plane.obj, field.png, sea.png in this directory.
FBX import is optional via Assimp when no OBJ model exists.
The supplied aircraft orientation is corrected and its span scaled to 24 units.
DDS files, original MTL paths and animations are not used.

texture/night.jpg: star map used by the day/night sky.
