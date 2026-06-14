# Texture Forge

Source for the `doonc.texture-forge` Geode mod.

## What v0.4.2 does

- Fixes custom icon apply behavior by clearing the selected icon's Geometry Dash color/glow helper layers before drawing the imported image into the icon's primary frame(s). This makes photo-style imports show as real textures instead of being hidden by the normal player-color mask.
- Repairs older Texture Forge icon sheets during Apply/startup mount by keeping the already-imported primary image and clearing the old secondary/glow/helper layers.
- Normalizes Texture Forge-overridden icon sprites to white at runtime, so the garage preview, icon grid, and exposed in-level player icons do not tint custom icon images green, blue, or other selected player colors.
- Keeps unrelated icons in the same sheet untouched, so changing Cube 1 does not blank or rewrite Cube 2, Ship 1, or other icon sheets.

## What v0.4.1 does

- Stops using Geometry Dash's `resetAllIcons()` during Apply/Reset, so applying a pack no longer changes your selected cube, ship, ball, UFO, wave, robot, spider, swing, or jetpack back to the first/default icon.
- Shows zero-based icon resources as normal one-based in-game targets. For example, `player_00` is shown as **Cube 1** instead of being hidden as **Cube 0**.

## What v0.4.0 does

- Adds one **Texture Forge** button in Geometry Dash's **Advanced Video Options** menu.
- Styles the Advanced-menu Texture Forge button like Geometry Dash's gray **Apply** button, measuring against the real Apply button at runtime so it uses the same compact size and baseline.
- Adds an Advanced-menu info button just above the upper-left edge of the Texture Forge button that explains what Texture Forge does and why the normal **Textures** button is not part of this mod.
- Adds a first in-game **Icon Editor** from the Import page. Pick the pack, press **Icon Editor**, choose the target, draw/edit the texture, save it into the pack, then press **Apply Pack**.
- The editor includes pencil, eraser, fill, line, rectangle, circle, solid/outline mode, grid toggle, brush size controls, mirror, rotate, copy, paste, undo, redo, size controls, hex color entry, and target-shaped canvases.
- Saves editor drawings into the pack's `editor/saves` folder so they appear in the import file picker and stay portable with the pack.
- Shows icon targets with in-game numbering, mapping zero-based files like `player_00` to normal labels like **Cube 1**.
- Moves the import preview higher so it no longer sits behind the **Use File** button.
- Writes imported images into the chosen icon sheet while clearing that selected icon's extra color/glow helper frames, so the imported art is not covered by Geometry Dash's normal recolor layers.
- Removes exact icon frame names from Geometry Dash's sprite-frame cache before reloading an icon sheet, so frames like `ship_02_001.png` and `player_01_001.png` are replaced immediately instead of staying vanilla.
- Verifies sampled icon frame names after reload and warns if the game needs a restart to pick up the changed icon sheet.
- Keeps contain-mode image imports inside the fitted rectangle instead of stretching edge pixels into the padding area.
- Mounts the active texture pack first, reloads Geometry Dash textures, then refreshes affected PNG and icon plist/sprite-frame caches from the exact physical pack files.
- Adds built-in Texture Loader-style plist stacking for non-icon spritesheets, so partial non-icon sheets can fall back through lower packs and vanilla resources without making Texture Loader a dependency.
- Logs every generated icon PNG/plist pair and every refreshed sprite-frame plist so icon apply problems can be traced in `geode/latest.log`.
- Mounts the active pack's `resources/icons` folder too, so Geometry Dash icon sheets resolve when the game asks for `player_00.png` instead of `icons/player_00.png`.
- Shows a success toast after **Apply Pack** so it is clear the click was handled.
- Clears every pack's applied runtime resources when **Reset All** restores default textures, without removing missing sprite-frame files.
- Runs as a standalone Geode mod with no Texture Loader dependency.
- Marks the current package as Windows-only until other platform binaries are built and tested.
- Stores packs under `geode/config/doonc.texture-forge/packs`.
- Supports multiple portable packs, each with `imports`, `sources`, staged edits, and committed `resources`.
- Lets the user choose an image import before choosing the target.
- Supports friendly icon/gameplay categories and a raw Geometry Dash PNG target browser.
- Supports optional transparent-background processing for imported PNG/JPG files.
- Rejects oversized images before decoding them.
- Stages changes inside the pack and only changes the running game when **Apply Pack** is pressed.
- Exports and imports Texture Forge pack archives.
- Provides **Reset Overrides** to clear the currently applied Texture Forge pack.
- Provides **Overrides** to remove a single staged texture from a pack, including correctly removing the final staged texture.
- Hides icon `.plist` helper files from the override list and removes paired icon helper files automatically.
- Uses `early-load` so the last applied pack can be restored when the mod loads.

## Workflow

1. Open Geometry Dash settings.
2. Open **Graphics**, then **Advanced Video Options**.
3. Press **Texture Forge**.
4. Create or select a pack.
5. Press **Import Files** or **Open Folder** and place PNG/JPG/JPEG files in that pack's `imports` folder.
6. Use the file arrows to choose the exact file, or press **Icon Editor** and choose a target before drawing a new texture.
7. Confirm whether image backgrounds should be removed for imported images.
8. Choose a target category and target.
9. Press **Apply Pack** when ready.

Raw targets can replace an entire Geometry Dash sprite sheet. Texture Forge warns before staging raw targets because a normal photo can blank parts of the sheet.

## Pack Layout

Each pack is self-contained:

- `pack.json` describes the pack.
- `imports` is the inbox for new files.
- `editor/saves` stores drawings made in the in-game editor.
- `sources` stores copied originals so the pack remains portable.
- `staged` contains uncommitted generated Geometry Dash resource overrides.
- `resources` contains the last applied Geometry Dash resource overrides.
