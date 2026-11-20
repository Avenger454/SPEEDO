# SPEEDO development

This is the SPEEDO application repository for the 91 Firebird display project.
Development is authorized here; the frozen baseline is Avenger454/Base-4Inch.
Preserve the validated 720 x 720 display initialization, DSI compatibility wrapper and linker flags.
Do not modify Base-4Inch as part of gauge development.
Keep demo distances separate from persistent mileage.

## Default sweep style for future gauge designs

- Use a narrow, opaque bright red (#FF0000) radial needle spanning the space between the inner and outer blue arcs.
- Default width is 7 px on a 720 x 720 face; scale proportionally on other displays.
- Calibrate needle endpoints to each face's arcs and its angle mapping to the scale; do not reuse another face's radii blindly.
- Prefer this over a moving dash or animated filled arc unless the user requests another style.
- Preserve bounded old/new needle invalidation, local point coordinates, unchanged-value skips, and static backgrounds.
- Keep the slow 30-second up/down demo sweep unless requested otherwise.
