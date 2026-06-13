# Texture Forge Mod History

This folder stores built `.geode` packages before they are overwritten by later builds.

## Archived Packages

| Version | File | Notes |
| --- | --- | --- |
| 0.3.11 | `doonc.texture-forge-v0.3.11.geode` | Recovered from the current build/installed package on 2026-06-12. |
| 0.3.13 | `doonc.texture-forge-v0.3.13.geode` | Includes the physical icon plist/png reload path and the Advanced-menu info icon position adjustment. |
| 0.3.14 | `doonc.texture-forge-v0.3.14.geode` | Moves the Advanced-menu info icon slightly higher and left to sit just above the Texture Forge button's upper-left edge. |
| 0.3.15 | `doonc.texture-forge-v0.3.15.geode` | Applies the icon-cache criticism: mount first, reload textures, then refresh icon plist/sprite-frame caches; adds icon generation/reload logs. |
| 0.3.16 | `doonc.texture-forge-v0.3.16.geode` | Moves the Advanced-menu info icon higher above the Apply/Texture Forge button row. |
| 0.3.17 | `doonc.texture-forge-v0.3.17.geode` | Nudges the Advanced-menu info icon slightly farther left while keeping the higher placement. |
| 0.3.18 | `doonc.texture-forge-v0.3.18.geode` | Nudges the Advanced-menu info icon one layout unit back to the right. |
| 0.3.19 | `doonc.texture-forge-v0.3.19.geode` | Reloads icon sprite frames by exact frame key, remounts the runtime pack after texture reload, and fixes contain-mode image padding. |
| 0.3.13 source | `texture-forge-source-v0.3.13-clean.zip` | Clean source export with `src`, `include`, and config/docs only; excludes `build/` and compiled outputs. |
| 0.3.15 source | `texture-forge-source-v0.3.15-clean.zip` | Clean source export for v0.3.15 with `src`, `include`, and config/docs only; excludes `build/` and compiled outputs. |

## Missing Older Builds

Older Texture Forge builds were not found as separate `.geode` files on disk. The normal Geode build/install path writes to `build/doonc.texture-forge.geode` and then installs the same filename into Geometry Dash, so past versions were overwritten unless they were copied somewhere else.

If an older `.geode` package is found later, copy it into this folder with this naming pattern:

```text
doonc.texture-forge-vX.Y.Z.geode
```

Keep the version in the filename matching the `version` field inside that package's `mod.json`.
