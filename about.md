# Texture Forge

Texture Forge is a Windows standalone in-game texture pack builder for Geometry Dash. It does not need Texture Loader. Packs are created, edited, staged, applied, reset, exported, and imported from inside the game.

## Where To Open It

Open Geometry Dash settings, go to <cg>**Graphics**</c>, open <cg>**Advanced**</c>, then press **Texture Forge**.

There is only one entry point on purpose, so the button should be in the advanced video menu instead of the main graphics page.

The regular **Textures** button on the normal video-options page is Geometry Dash's own button. Texture Forge uses the separate **Texture Forge** button in Advanced.

## First Use

1. Press **Packs**.
2. Press **New Pack**.
3. Optional: press **Rename** to give the pack a better name.
4. Press **Back**, then **Import**.

Texture Forge does not create a starter pack automatically. You choose when to create your first pack.

## Importing An Image

1. In the **Import** page, press **Open Folder**.
2. Put your PNG, JPG, or JPEG image files in that pack's `imports` folder.
3. Return to Geometry Dash and press the left/right file arrows until the file you want is shown.
4. Press **Use File**.
5. Confirm the file.
6. If it is an image, choose whether Texture Forge should try to remove the background.
7. Choose the target category and target.

The file is copied into the pack, so the pack stays portable even if you move or delete the original file later.

Very large images are rejected before they are decoded. This protects the game from freezing if a huge photo or screenshot is imported by mistake.

## Choosing Targets

Friendly target categories are included for the player modes:

- Cube
- Ship
- Ball
- UFO
- Wave
- Robot
- Spider
- Swing
- Jetpack

Each icon number is its own target. For example, Cube 1 and Cube 2 can use different images at the same time.

There are also raw Geometry Dash PNG categories such as backgrounds, menu UI, editor, level objects, fonts, raw icons, and a full raw browser. Raw targets are for advanced use when you know the exact resource you want to replace.

Raw targets may be full sprite sheets. Texture Forge shows a warning before staging one because replacing a whole sheet with one normal image can hide parts of the game.

## Staging And Applying

Choosing a target only stages the edit inside the pack. It does not change the running game yet.

Press **Apply**, then **Apply Pack**, when you want the staged changes to become active. Texture Forge commits the staged files, mounts the pack through Geode, and reloads textures.

Apply also refreshes affected PNG texture caches and icon plist/sprite-frame caches, so changed textures should update without needing to restart the whole game.

If you edit a pack that is already active, those edits still stay staged until you press **Apply Pack** again.

## Resetting To Default

Press **Apply**, then **Reset Overrides**, or press **Reset All** on the home page, to remove applied Texture Forge overrides and reload default textures.

Reset clears every pack's copied runtime override files and refreshes affected PNG and icon sprite-frame caches back to the vanilla resources.

Resetting does not delete your packs. It only stops applying Texture Forge overrides.

## Removing One Texture

Press **Overrides** to browse the staged texture files in the current pack.

Use the arrows to choose one override, then press **Delete One**. This removal is staged too, so press **Apply Pack** when you want the deletion to affect the game.

Icon `.plist` helper files are hidden from this page. When you delete an icon PNG override, Texture Forge also removes the matching helper file so the icon does not become half-broken.

## Pack Folders

Texture Forge stores packs here:

`geode/config/doonc.texture-forge/packs`

Each pack contains:

- `pack.json` - pack name, id, and version info.
- `imports` - files you want to choose from in the import page.
- `sources` - copied originals used to keep the pack portable.
- `staged` - generated edits that are waiting for Apply.
- `resources` - the last applied Geometry Dash resource overrides.

## Exporting And Importing Packs

Use **Packs**, then **Export**, to create a portable `.textureforge` pack archive.

To import a pack archive:

1. Press **Folders**.
2. Press **Pack Imports**.
3. Put a `.textureforge` or `.zip` archive in that folder.
4. Go to **Packs** and press **Import Pack**.

## Notes

Background removal is automatic and works best on images with a simple background. If it looks wrong, import the same file again and choose **No** when asked about transparency.

Texture Forge currently imports image files only. It generates PNG texture overrides and copies the matching icon plist files automatically for icon targets.

If the mod button or changes do not show up, restart Geometry Dash so Geode reloads the installed `.geode` package.
