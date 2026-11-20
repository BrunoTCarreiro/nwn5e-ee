# Custom graphics for nwn5e effects - a short guide

How to make your own picture (a magic circle, a rune band, a glyph), try it on the VFX Bench and in game, and get it into nwn5e
for good. (Written 2026-11-20 for the owner; the tool is nwn5e-tools `texture_import\`.)

## 1. Make the picture

| What | Rule |
|---|---|
| **Name** | Up to **16 characters**, lowercase letters, digits and `_` only, no spaces. Start it with **`n5_`** (nwn5e's own), e.g. `n5_circle_zot`. |
| **Size** | **Square, a power of two.** Pick by how big it is in game: up to ~2 m across: **512**; 3-6 m: **1024**; 9 m and more (Zone of Truth): **2048**. Bigger costs video memory; the game's own circles are 128-256. |
| **Border** | Leave **a few pixels of empty border** (black for a glow, transparent for a solid look) all round, so the edges never bleed into each other. |

### Two looks - pick one per picture

**Glow (additive)** - the magic-circle look: light that adds to what's behind it, see-through, never darkens.
- Draw **white / light lines on pure black**. **Black = invisible**, brightness = how strongly it glows.
- Draw it **in white and greys** if you want to colour it per effect with the Composer's **Colour** (one picture, any colour).
- Downside: on very bright ground (snow, sunlit sand) a glow washes out.

**Solid (normal)** - a painted mark that stays readable on any ground, can be dark.
- Draw it with **transparency** (alpha): fully transparent = invisible, opaque = solid.
- **The game cuts pixels below about 20% opacity** (a hard edge where a soft fade falls under 51/255), so keep faint parts above that,
  or use the glow look for soft wisps.

### Don'ts
- **No `decal 1`** in a `.txi` unless you want it unlit and **untintable** (the game then ignores the Colour).
- **No `downsamplemax` / `downsamplemin` lines** - this version of the game ignores them.
- Don't reuse a game texture's name - yours would replace it everywhere.

## 2. Turn it into a game texture - Texture Import

Double-click **`nwn5e-tools\texture_import\texture_import.bat`** (a page opens in your browser; closing its console window stops it).
1. **Drop your PNG** on the page (or Choose file). The preview shows it as the game will draw it - a glow over dark and bright ground,
   a solid one over a checkerboard with the under-20% parts cut.
2. **Name** - pre-filled from the file name and checked as you type; a button adds `n5_`; it warns if the name would replace one of
   the game's own textures.
3. **Look** - **Glow** writes a 24-bit TGA plus `<name>.txi` (`blending additive`); **Solid** writes a 32-bit TGA with its alpha
   (no `.txi`).
4. **Save to** - the game's **`ovr`** folder by default (it remembers your last folder); it asks before overwriting.

(By hand instead: an uncompressed TGA - 24-bit for a glow with a `<name>.txi` holding one line `blending additive`; 32-bit with alpha
for a solid look.)

## 3. Try it on the VFX Bench and in game (no install)

1. The texture is in the game's **`ovr`** folder (step 2 put it there). **Close the game** before saving there - it reads `ovr` when it
   starts - and start it again afterwards.
2. On the bench: **Composer** → a **Spell mesh** project → **Shapes** → for a ground circle **Disc (flat, on the ground)** (or **Ring
   (flat band)**, **Dome (half sphere)**). On its card: **Mapping: From above** (for a disc / ring), **Texture: From the library…** →
   **type your name** in the picker's box → **Use**.
3. Adjust on the card: **Radius**, **Colour** (tints a white / grey picture), **Glow** (how much it lights itself), **Opacity**, and
   **Texture motion → Turn** for a slowly rotating circle.
4. **Play in game** shows it beside your character. To use it in an effect: **Save and build**, **Back to effects**, **+ mesh** and pick
   your spell mesh.

## 4. Make it part of nwn5e (for everyone, every install)

Send the file (TGA or PNG), its name, and which look (**glow** or **solid**) to the animation session. It's added to nwn5e's data with
its `.txi`, credited as your art, and ships with the next install. **Then delete your copy from `ovr`** - otherwise an old copy there
would keep overriding the shipped one.

## Quick reference

| | Glow | Solid |
|---|---|---|
| Background | pure black | transparent (alpha 0) |
| Format | 24-bit TGA | 32-bit TGA with alpha |
| `.txi` | `blending additive` | none |
| Tint with Colour | yes (draw white / grey) | yes (draw white / grey) |
| On snow / bright ground | washes out | stays readable |
| Soft fades | fine | cut below ~20% alpha |
