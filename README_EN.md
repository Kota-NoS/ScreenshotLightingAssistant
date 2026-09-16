# Screenshot Lighting Assistant 0.3.0

Three adjustable photography lights, an independent photography face fill, an optional persistent player face light, and optional photo effects for Skyrim SE/AE.

## New in 0.3.0

- Adds eight photo effects: Sparkles, Petals, Stars, Stars 2, Autumn Leaves, White Feathers, Black Feathers and Snow Crystals. Density, particle size, spread, opacity, brightness, vertical offset, motion, orbit and lifetime are adjustable.
- Photo effects are disabled by default. They follow the selected subject only while photography lighting is active and are removed on stop, load, cell change, subject loss, new game or return to the main menu. They do not write to game saves.
- Improves storage ownership for saved entries and settings. Read-only startup no longer creates an empty folder; an existing `storage-v2.sla` marker keeps new files in the MO2 output mod that owns it. Data from the legacy My Games location is copied once while the original files remain as a backup.
- The existing three-light system, photography face fill, persistent player face light and saved-data formats remain compatible with 0.2.0.

## 0.3.0 development history

### 0.3.0-alpha44 final tuning

- Sets the three NIF base-colour scales to `1.00` and neutralizes emitter and lifetime-fade RGB to white, letting UI brightness `1.00` show the authored texture colour and tonal range.
- Preserves fade alpha, silhouettes, UVs, motion, emission and lifetime behavior from alpha43. A dedicated compensation keeps Black Feathers' former peak contribution despite sharing the feather NIF.
- All eight DDS assets are byte-identical to alpha43. Any White-feather texture contrast adjustment is intentionally deferred until this NIF-only result is tested in game.

### Experimental in 0.3.0-alpha43

- Uses the brighter white-to-Sakura atlas selected from in-game alpha42 comparisons, while retaining alpha42's neutral NIF material.
- Repairs the missing right edges of the first two poses from the original sample and preserves the approved pale center, pink rim, root, vein and fold detail.
- Petal NIFs, eight UV poses, motion, emission balance and lifetime variants are unchanged from alpha42. The other seven effects, lighting, UI, saved formats and MO2 storage behavior are unchanged.

### Experimental in 0.3.0-alpha42

- Restores the Sakura NIF base-colour scale from `0.82` to `1.00`, making UI brightness `1.00` the actual authored baseline instead of roughly the former `1.21` result.
- Neutralizes the extra pink RGB multipliers in the petal emitter and lifetime colour modifier. The atlas now owns the white-to-blush colour range, while the existing alpha-only fade remains unchanged.
- The Sakura atlas, alpha silhouette, eight poses, motion, emission balance and lifetime variants are unchanged from alpha41. The other seven effects, lighting, UI, saved formats and MO2 storage behavior are unchanged.

### Experimental in 0.3.0-alpha41

- Lifts the midtones that made alpha40 look muted and grey against bright skies, restoring a bright white-to-pale-Sakura appearance.
- Concentrates the blush in roots, veins and folds instead of making the entire petal vivid pink. This keeps substantially more tonal detail than alpha39 while looking brighter and more colorful than alpha40.
- Petal alpha silhouettes and total coverage, all eight poses, NIFs, motion, emission balance and control ranges are byte-identical to alpha40. The other seven effects, lighting, UI, saved formats and MO2 storage behavior are unchanged.

### Experimental in 0.3.0-alpha40

- Restores the petal luminance range compressed by alpha39 to approximately alpha38's range. Roots, veins, folds and overlaps are intended to remain readable after the effect shader and scene exposure brighten the texture.
- Separates tone from color: luminance receives useful headroom and local contrast, while only a restrained low-saturation Sakura blush is added. Dark detail is not turned back into vivid pink.
- Petal alpha silhouettes and total coverage, all eight poses, NIFs, motion, emission balance and control ranges are byte-identical to alpha39. The other seven effects, lighting, UI, saved formats and MO2 storage behavior are unchanged.

### Experimental in 0.3.0-alpha39

- Reworks the petals from uniformly pink to predominantly near-white, retaining a restrained Sakura blush around authored roots, veins, overlaps and folds. This is an opacity-aware white blend rather than another global saturation adjustment.
- Translucent borders are pulled closer to white to reduce red rims in darkness, dusty peach or mauve against blue skies, and salmon or yellow-pink shifts under strong scene lighting. Internal petal shading remains visible.
- Petal alpha silhouettes and total coverage, all eight poses, NIFs, motion, emission balance and control ranges are byte-identical to alpha38. The other seven effects, lighting, UI, saved formats and MO2 storage behavior are unchanged.

### Experimental in 0.3.0-alpha38

- Pulls the alpha37 petal color back toward alpha36, retaining only a very pale Sakura blush. Dark backgrounds no longer emphasize a vivid pink rim, while bright outdoor lighting is less likely to turn the petals into mauve or generic pink shapes.
- Petal alpha silhouettes and total coverage, all eight poses, NIFs, motion, emission balance and control ranges are byte-identical to alpha37.
- White feathers, Black feathers, Snow crystals, the other effects, lighting, UI, saved formats and MO2 storage behavior are unchanged from alpha37.

### Experimental in 0.3.0-alpha37

- Slightly increases the pale Sakura-pink saturation while retaining the petals' white centers and delicate internal texture. The adjustment is intended to read as cherry blossoms against both dark and blue-gray outdoor backgrounds rather than as near-white shapes.
- Petal alpha silhouettes and total coverage, all eight poses, NIFs, motion, emission balance and control ranges are byte-identical to alpha36.
- White feathers, Black feathers, Snow crystals, the other effects, lighting, UI, saved formats and MO2 storage behavior are unchanged from alpha36.

### Experimental in 0.3.0-alpha36

- Changes the motion note to “A preset is a combination of the values below.”
- Retains the White-feather atlas's transparent silhouettes, four families and eight poses while widening its authored tonal range. Shafts, overlapping barbs and vane curvature now use restrained blue-gray-to-white definition, targeting a useful middle ground between dim blur and uniform white clipping.
- Updates the storage-location tooltip to describe the actual ownership rule: use the storage mod supplying `storage-v2.sla`, otherwise use MO2's normal output destination.
- Replaces the transparent padding's hidden pink RGB with neutral white so filtered edges do not pick up an unintended warm tint. Alpha coverage is byte-identical to alpha35.
- Black feathers, the other six effects, all 441 NIFs, motion, controls, lighting, saved formats and MO2 storage behavior are unchanged from alpha35.

### Experimental in 0.3.0-alpha35

- Removes the remaining explanatory line below Brightness in the Photo Effects window.
- The persistent-face launcher is now also a window toggle. Closing it while key capture is active safely cancels capture.
- When virtual Data already contains a valid `storage-v2.sla`, its resolved physical parent becomes the storage owner. After moving the complete SSLA storage folder from Overwrite into a dedicated output mod, newly-created presets and `library-view-v3.sla` stay in that mod instead of splitting back into Overwrite.
- A marker-free installation still uses virtual Data and lets MO2 select the normal destination. An arbitrary folder with the same name is never searched for or selected.
- Effect assets, appearance, motion, ranges and lighting are unchanged from alpha34.

### Experimental in 0.3.0-alpha34

- The Photo Effects window is resizable. Its motion-preset note is shortened, and the density legend is removed.
- Effect order is now Sparkles, Petals, Stars, Stars 2, Autumn leaves, White feathers, Black feathers, Snow crystals. Serialized type IDs remain unchanged for compatibility.
- Pressing P or the star launcher again closes its already-open window.
- A read-only launch no longer probes virtual Data or writes a migration marker, so it does not create an empty MO2 Overwrite folder. Directories are created only by an actual setting/preset save or a real legacy-data migration.
- Appearance, motion, ranges, lighting, DDS and NIF assets for all eight effects are unchanged from alpha33.

### Experimental in 0.3.0-alpha33

- The reported straight-edged defect was not attenuation: the first dense dendrite also crossed the source sheet's horizontal midpoint. Its downward branch continued to `y=592` but was cropped at `y=543`; the source region now includes its natural tip.
- Low-to-mid alpha and the neutral-white ice-vein tones are lifted by roughly 1.5x across all four families while retaining transparent faces. `Opacity 1.00 / Brightness 1.00` is intended to remain usable against blue-grey exterior scenery.
- No symmetry reconstruction or shape redraw is applied. The 4x8 atlas layout, all NIFs, motion, spawn ratios, seven earlier effects, control ranges and lighting are unchanged from alpha32.

### Experimental in 0.3.0-alpha32

- In alpha31, the downward primary branch of the second compact dendrite crossed the source sheet's horizontal midpoint, so the exact quadrant crop flattened its tip.
- Its source region now extends far enough downward to retain all six complete branch tips. Transparency, internal veins, size, the other three families, all Snow NIFs, motion, spawn ratios, seven earlier effects, lighting and controls remain unchanged from alpha31.

### Experimental in 0.3.0-alpha31

- Replaces alpha30's similar rounded, milky-white families with four crops from the user-selected clear-ice reference.
- The families are a dense dendrite, a compact balanced dendrite, an open sparse dendrite and a small hexagonal plate. Rounded rime beads are removed, so their silhouettes remain different at shooting distance.
- Fine internal ice veins and tonal detail come from the reference, while cyan/purple fringe, backdrop pixels and near-opaque source alpha are removed. Snow NIFs, motion, spawn ratios, seven earlier effects, lighting, control ranges and storage are unchanged from alpha30.

### Experimental in 0.3.0-alpha30

- Reworks the alpha29 Snow-crystal atlas after in-game testing showed its faint centre plates disappearing and its bright branch ridges reading as bare wireframes.
- Every main and side branch is now a broad translucent ice plate layered with restrained ridges and irregular rime grains. The larger centre plates reinforce the crystalline body without turning it into a flat icon.
- The four families by eight fixed columns, 63 Snow NIFs, motion, spawn ratios, seven earlier effects, lighting, control ranges and storage are unchanged from alpha29.

### Experimental in 0.3.0-alpha29

- Adds Snow crystals as the eighth photo-effect type: a large dendrite, a medium stellar crystal, a slender needle crystal and a small hexagonal crystal drift slowly around the target.
- A faint translucent ice-blue interior is separated from slightly brighter frosted branches and edges. Outside pixels are fully transparent; no checkerboard or rectangular backdrop is embedded.
- Each family has its own particle system and repeats one fixed design through all eight atlas frames. Crystals do not pulse, resize or morph while visible; the NIF supplies only gentle rotation and natural birth/death fades.
- Snow's Gentle preset starts slightly high and descends slowly. The seven existing effects, control ranges, lighting and storage are unchanged from alpha28.

### Experimental in 0.3.0-alpha28

- Moves the preferred alpha27 brightness 0.26-0.36 appearance to the normal 1.00 setting so the effect reads as black instead of silver.
- Lowers visible median luminance from 63/255 to 25/255 and the peak from 188/255 to 70/255. Only roughly the brightest five percent remains in the upper graphite range.
- Alpha coverage, silhouettes, four families by eight views, 63 shared NIFs, motion, the six earlier effects, lighting, controls and storage are unchanged from alpha27.

### Experimental in 0.3.0-alpha27

- Pulls the broad pale-silver barbs and downy tips from alpha26 back into graphite so the effect reads as Black feathers rather than silver feathers.
- Keeps a narrow pale-silver shaft and a few selected tips. The silver-range share falls from about 24% to about 13%, while the texture peak falls from 238/255 to 188/255.
- Alpha coverage, silhouettes, four families by eight views, 63 shared NIFs, motion, the six earlier effects, lighting, controls and storage are unchanged from alpha26.

### Experimental in 0.3.0-alpha26

- Adds a dedicated neutral-grayscale DDS for Black feathers. Deep shadows remain black, barbs separate into graphite and silver-gray, and the shaft plus selected highlights rise to pale silver.
- The atlas is deterministically tone-mapped from the completed White-feather atlas, preserving alpha, fringes, asymmetric cuts, split tips and the exact four-family/eight-view layout without adding blue, purple or brown colour noise.
- White and Black feathers still share the same 63 NIFs, motion and family distribution. Only cloned Black-feather materials switch to the new DDS; White feathers, the six earlier effects, controls, lighting and storage remain unchanged from alpha25.

### Experimental in 0.3.0-alpha25

- Adds Black feathers as the seventh photo-effect type. It resolves to the same 63 NIFs, four families by eight tumble views, motion, rotation and fade as White feathers.
- No black-feather NIF or texture duplicate is added. Only a loaded Black-feather instance receives a neutral graphite material tint, preserving the white atlas's shaft, inner vane detail, soft fringes, cuts and transparent silhouette.
- A small amount of graphite lift keeps detail visible instead of crushing it to pure black. Opacity and brightness compensation remain available; all six earlier effects, lighting and storage are unchanged from alpha24.

### Experimental in 0.3.0-alpha24

- Adds sparse, shaft-aligned vane separations, asymmetric edge breaks and a subtle split tip to only the previously smooth long-feather row.
- Cut placement and depth vary across its eight tumble views instead of forming a regular saw-tooth edge. The narrow edge-on view stays shallow, and the shaft and overall silhouette remain intact.
- The three already-soft feather families, white shading, four-row/eight-frame atlas contract, NIFs, motion, rotation and fade are unchanged from alpha23. Stars 2, asynchronous mote twinkling, controls, lighting and storage are also unchanged.

### Experimental in 0.3.0-alpha23

- Renames the UI label from Pop stars to Stars 2. The internal effect value, NIF and texture names, and ParticleWind NG node names remain unchanged for settings compatibility.
- Stores the Stars 2 atlas as uncompressed RGBA8 instead of BC3/DXT5, avoiding unrelated red, yellow or cyan fragments from shared 4 x 4 compression blocks on heavily magnified flat-colour edges.
- Invisible RGB texels are filled with their atlas row's own star colour. Their alpha remains zero, while linear filtering at silhouettes and inside the hollow yellow star no longer samples black or unrelated colours.
- Shape, four-colour palette, fixed size, motion, rotation and fade are unchanged from alpha22. Luminous-mote twinkling, existing controls, lighting and storage are also unchanged.

### Experimental in 0.3.0-alpha22

- Reworks the four Pop-star treatments into a genuinely hollow bright-yellow outline, muted coral pink, warm pale yellow and sky blue. Numeric flat colours keep the treatments clearly separated.
- The yellow outline has a genuinely transparent centre with no white, black, checkerboard or rectangular backing. All four retain the slightly full five-point silhouette and crisp pointed tips.
- Pop-star atlas frames now share one size and opacity, removing their scale and intensity pulse. Motion, rotation, per-particle start time and colour identity remain intact.
- Luminous-mote twinkling, six effect types, 378 NIFs, existing controls, alpha20 opacity compensation, lighting and storage are unchanged from alpha21.

### Experimental in 0.3.0-alpha21

- Luminous motes now cycle through an eight-frame twinkle atlas with randomized per-particle phase. Brightness and apparent size breathe softly without synchronizing the whole field or turning every mote fully off.
- Adds Pop stars as a sixth effect, separate from the existing luminous Magic stars. Four flat treatments mix sky-blue outline/white center, coral pink, pale lemon and pale aqua in a slightly full five-point silhouette with crisp pointed tips and no 3D gloss.
- Each treatment owns one atlas row and particle system, preventing a live star from morphing into another colour treatment. Pulse phase, spin speed and spin direction vary per particle.
- The installer contains 378 NIF variants covering six types, three capacity tiers, normal/clockwise/counterclockwise motion and seven lifetime steps. Existing controls, alpha20 opacity compensation, lighting and storage remain unchanged.
- Pop-star raster, DDS and NIF assets are original project material. The supplied watermarked stock image was used only as colour and shape direction; none of its pixels, layout or watermark is included.

### Experimental in 0.3.0-alpha20

- Alpha19 effectively attenuated opacity twice through Skyrim's effect-shader and source-alpha blend path, leaving Opacity 0.25 with Brightness 4.00 substantially darker than the standard appearance.
- Runtime now removes one extra alpha factor from material intensity, making Opacity x Brightness a useful visual-strength guide. 0.25 x 4.00 targets a contribution close to 1.00 x 1.00 while retaining translucent edge coverage and downstream post-processing differences.
- The 1.00 x 1.00 baseline and every Brightness value at full opacity are unchanged from alpha19. Subtle effects remain available at 0.25 x 1.00 and can be reduced further to 0.25 x 0.25.
- Slider ranges, NIFs, textures, emission, particle position, density, size, spread, motion and all lighting features are unchanged.

### Experimental in 0.3.0-alpha19

- Adds independent Opacity (0.25-1.00) and Brightness (0.25-4.00x) controls. Both default to 1.00, preserving the alpha18 appearance.
- Opacity controls translucency, while Brightness scales the authored material intensity. There is no automatic compensation, so intentionally subtle effects remain possible.
- As a practical guide, Opacity 0.25 with Brightness 4.00 can preserve translucent edges while recovering close to the standard visual contribution. ENB/Community Shaders, fog, exposure and the background still affect the final result.
- Both values update already emitted particles live without respawning, pausing emission or moving them. NIFs, textures, motion, density, size and spread are unchanged.

### Experimental in 0.3.0-alpha18

- Applies Particle size through each live particle's size multiplier instead of scaling the whole particle-system node.
- Prevents already emitted particles from appearing to vanish after a size change in large exterior-cell coordinates and then refilling several seconds later.
- Field spread still controls only the emitter area. Existing visuals, motion, density, NIFs, textures, lighting and saved settings are unchanged.

### Experimental in 0.3.0-alpha17

- Adds White feathers as a fifth Photography Effects type: a small down feather, long flight feather, curved feather and soft decorative plume.
- Each identity owns eight project-specific front, oblique, edge-on and back views, so a live feather tumbles naturally without morphing into a different feather.
- Feathers support continuous density, particle size, field spread, vertical offset, every motion preset and clockwise/counterclockwise orbit.
- The feather atlas, DDS and NIFs are original project assets. The supplied stock-site image is used only as visual direction and is not included or copied. The optional ParticleWind NG exclusion names all four feather systems.
- The installer contains 315 generated NIF variants covering five types, three capacity tiers, normal/clockwise/counterclockwise motion and seven lifetime steps.

### Experimental in 0.3.0-alpha16

- Organizes autumn leaves into four identities, each with eight project-specific tumble views of the same leaf. A maple leaf cannot morph into a rounded or narrow leaf, but its front, oblique, edge-on and back appearance changes naturally.
- Each internal particle system is restricted to one atlas row, preserving leaf identity for the particle's lifetime while retaining varied starting phase, spin direction and spin speed.
- Petal flipbook variation and all existing effect, density, size, spread, motion, orbit and lighting behavior remain unchanged.
- The leaf atlas, DDS and NIFs are original project assets and do not use material from another mod. The optional ParticleWind NG exclusion now names the four leaf systems.

### Experimental in 0.3.0-alpha15

- Splits autumn leaves into eight independent internal particle systems so a live leaf never morphs into another atlas image.
- Each leaf retains its birth silhouette until it fades while preserving varied angle, spin direction, spin speed, motion controls, density, size, spread and orbit.
- Petal flipbook variation remains unchanged.
- The optional ParticleWind NG exclusion file now names all eight leaf particle systems.

### Experimental in 0.3.0-alpha14

- Adds Autumn leaves as a fourth Photography Effects type. Eight original yellow, orange and russet leaf images tumble with varied starting angles, spin speeds and directions.
- Leaves support continuous density, particle size, field spread, vertical offset, every motion preset and clockwise/counterclockwise orbit.
- The leaf atlas, DDS and generated NIFs were created specifically for this project. No Skyrim or third-party mod asset is used.
- `SKSE/Plugins/ParticleWind/SLA_NoWind.json` excludes all four SLA particle systems from ParticleWind NG's weather force when that optional plugin is installed. ParticleWind NG is not required.
- The installer contains 252 generated NIF variants covering four types, three capacity tiers, normal/clockwise/counterclockwise motion and seven lifetime steps.

### Experimental in 0.3.0-alpha13

- Adds Stars as a third Photography Effects type. Five-point stars with a white core and subtle pale-gold/icy-blue edges receive varied starting angles, spin speeds and spin directions.
- Stars support continuous density, particle size, field spread, vertical offset, every motion preset and clockwise/counterclockwise orbit. Their 1.00/2.00/3.00 density anchors use 3/6/10 particles per second.
- The star image, DDS and NIFs were created specifically for this project. No mesh or texture from ParticleFieldSE or another mod is used.
- The installer contains 189 generated NIF variants covering three types, three capacity tiers, normal/clockwise/counterclockwise motion and seven lifetime steps.

### Experimental in 0.3.0-alpha12

- Density is now a free 1.00–3.00 slider. The 1.00, 2.00 and 3.00 anchors exactly retain alpha11 Low, Standard and High output, with continuous interpolation between them.
- Sparkles interpolate the old 4/8/14 emission rates while Petals interpolate 3/6/10. Density changes apply live without replacing existing particles.
- The runtime loads the high-capacity NIF and changes only its posed float emission-rate interpolator. Motion, particle size and field spread remain independent.

### Experimental in 0.3.0-alpha11

- Corrects the remaining vertical-direction inversion in target-local Orbit meshes.
- Tunes Fall from Above to the tested reference motion: speed 0.74, vertical -0.55 and floatiness 0.30.
- Presents the existing three density levels as a 1–3 slider (Low, Standard, High; superseded by alpha12's continuous control).
- Separates Particle size (0.50–2.00x) from field Spread (0.50–4.00x).

- A compact sparkle icon opens a separate Photography Effects window. The feature is disabled by default, and its Type control selects Sparkles, Petals or Stars.
- Sparkles use white-to-pale-gold motes, Petals use a pale-pink profile, and Stars use a white core with pale-gold/icy-blue edges; all are original project assets.
- Gentle, Fall from Above, Wind, Updraft and Orbit presets are included. Each is an ordinary combination of vertical offset, speed, signed vertical and horizontal movement, floatiness, sway, randomness, lifetime and optional orbit.
- Petals use an original eight-view atlas with varied starting phases to reduce the flat, synchronized look. Sway is varied per particle instead of bending the entire field in lockstep.
- Motion Details exposes speed, signed vertical and horizontal movement, floatiness, sway, randomness and lifetime, plus subject-centered orbiting with clockwise or counterclockwise direction. Negative vertical values fall, zero hovers and positive values rise.
- Density, particle size, field spread and vertical offset are adjustable independently. Changing lifetime safely replaces the field with a different lifetime NIF, so existing particles briefly disappear.
- The emitter follows the subject, while already-emitted motes, petals or stars remain in world space instead of sticking to the subject or camera. Their motion remains visible with a fixed camera.
- Up/down and left/right use the camera basis captured when photography starts or when Realign Camera is used. The field is removed when photography stops, loading or a location transition begins, or the subject is lost.
- Alpha settings are session-only and are not included in presets. The effect uses no ESP, Papyrus script or Skyrim save data.
- No meshes or textures from ParticleFieldSE or any other mod are included. The petal and star images, particle NIFs and textures were created specifically for this project.
- The installer includes 189 generated NIF variants covering normal/clockwise/counterclockwise motion, three types, three capacity tiers and all lifetime steps. Removing the dedicated fall mesh does not remove the eight-frame petal appearance or per-particle rotation variation. This build is intended for in-game visual and lifecycle testing. Keep version 0.2.0 available for normal use.

## Changed in 0.2.0

- Added a compact person icon that opens a separate Persistent Face Light window without changing the circular photography-light layout.
- Added a feature master switch at the top of the Persistent Face Light window. Turning it off extinguishes the persistent light and ignores both assigned hotkeys while preserving the player-light state, values and bindings. Turning it back on resumes the preserved state.
- The persistent light is player-only, white and shadowless. Its placement is now always head-facing; intensity, range and height remain independently adjustable.
- An optional keyboard hotkey switches it on or off. Select Change and press one keyboard key without closing the framework menu; press Esc to cancel without changing the current assignment. Clear removes the assignment. SKSE Menu Framework's dedicated input callback handles this in-menu capture.
- A separate gamepad button can be assigned. Hold it for about 0.6 seconds during normal gameplay to toggle once. A short press does nothing, and holding the button cannot retrigger until it is released.
- Its feature state, light state, both hotkeys and values are stored in `persistent-face.sla`, independently of Skyrim saves, and restored after restarting the game. Older beta profiles remain readable and migrate to head-facing placement.
- The persistent player light is removed during loading and recreated only after the player, cell and renderer are safe again. Ordinary photography lights still stop on loading/location changes and do not auto-restart.
- When photography targets the player and its photography face light is enabled, that light temporarily replaces the persistent one to prevent duplicate illumination. Stopping photography restores the persistent profile. An NPC can still use the photography face light while the persistent light remains on the player.
- Lighting, persistence, transitions, in-menu assignment, gamepad long-press behavior, the master switch, head-facing migration and the revised status UI all passed the supplied in-game checklist. Version 0.2.0 promotes the same feature code tested in beta4.

## Changed in 0.1.29

- Added Reset values beside the Settings heading. It resets only the selected light's intensity (1.00), range (4.0m), distance (2.20m), height and fine offsets (0.00m), and shadow bias (1 Default).
- Direction, type, color, shadow mode and power are preserved. The reset is one Undo/Redo history action and is disabled for unplaced lights or values already at defaults.
- RC1 clean-install, restart-persistence and one-time migration checks passed in game. The RC2 Reset values control also passed live verification before this release.
- Updated DLL/UI version presentation and replaced the trial startup log with a neutral release log.
- Corrected the CommonLibSSE-NG attribution to `alandtse/CommonLibSSE-NG v6.1.0`; its exception text and original MIT notice are included.
- Removed the PDB from the MO2 archive. Debug symbols are supplied separately and are not required to play.
- Added concise Japanese/English release documentation and a final clean-install/upgrade checklist.

## Changed in 0.1.28 (historical)

- Added the user's exact `Candle Space` scene as a built-in sample, including fine offsets and its disabled Head-facing face-light settings. Japanese UI labels it キャンドル空間照明. It remains separate from user-created Saved presets.
- English is now the default only when no language preference exists. Existing JP/EN preferences remain authoritative after updating.
- Mutable files now target virtual `Data/SKSE/Plugins/ScreenshotLightingAssistant/`. Under MO2 they should normally appear in `Overwrite/SKSE/Plugins/ScreenshotLightingAssistant/`. A non-MO2 install uses the physical Data path and falls back to the previous My Games location if Data is not writable.
- On first successful use, language, saved entries, list metadata and Archived files are copied from the old SKSE-log location. Old files remain untouched as a rollback backup, and a marker prevents repeated imports.
- Normal-width preset cards are about 11% shorter than 0.1.27 while retaining the current font size. Diagram, padding and information rows were compacted together.
- 44,816 automated checks and the SE/AE DLL build passed. Live MO2 redirection, lighting output, localization and UI scaling still require in-game verification.

## Changed in 0.1.27 (historical)

- Select a card to apply now appears to the right of Saved presets instead of taking a separate row. With no user presets, it appears to the right of Samples.
- At narrow widths the guidance is omitted rather than pushing headings and cards down. Managing / lighting stays unchanged follows the same rule.
- Saved lights and presets remain external files under the SKSE log directory. They are independent of save files and carry across new games.

## Changed in 0.1.26 (historical)

- Head facing is now the default face-light basis for newly initialized scenes. Camera remains available. Existing SCENE 1 saved presets still decode as Camera and are not rewritten.
- Added Rim lighting Blue and Rim lighting Warm to Samples. Blue applies the authored three-light setup plus its Head-facing face fill; Warm applies its three manual lights and keeps the current face-light settings.
- Built-in samples do not create entries under Saved presets, so they remain separate from user registrations.
- Select a card to apply now appears on its own row below the Manage and Keep open checkboxes.
- Reopening a light/preset registration dialog or editing its name clears the previous registration error. Persistent storage-load warnings remain separate.

## Changed in 0.1.25 (historical)

- Open the face settings button below the center of the diagram. Choose Camera (the 0.1.25 default) or Head facing. Switching does not turn the face light on.
- Camera retains the original camera-side placement and world-up height. Head facing places the fill 0.45m ahead of the animated head, following turns, pitch and tilt; height follows the head's local up direction.
- Only the face fill is affected. The three main lights, camera and actor orientation are not modified. Face color remains white and shadowless; intensity/range controls are unchanged.
- Undo/Redo, bookmarks and presets saved with face light include the mode. Samples and presets without face light retain the current face settings.
- Older presets load unchanged as Camera. Only new presets including Head facing use SCENE 2; builds before 0.1.25 cannot read those entries and leave their files untouched. Other presets remain SCENE 1. Back up the Presets folder before updating.
- In side-by-side layouts, Fine position and removal now belong to the left diagram column, sharing vertical space with favorite 5 instead of following both columns. Narrow layouts still stack/wrap safely.

## Changed in 0.1.24 (historical)

- The placed-light removal button now sits beside Fine position: Light N with a theme-matched line-drawn trash icon. It removes only that placed light, not saved entries, and can be undone. Narrow windows wrap the action row.
- Favorites now hold up to five saved lights. A sixth requires choosing which favorite to replace.
- Keep open beside Manage defaults to OFF (apply and close). Enable it to apply presets immediately without closing the gallery. Closing with X keeps the last applied lighting; it does not cancel it.
- The gallery no longer dims the scene or blocks the main lighting controls. Start lighting first to compare illumination. Manage mode still prevents card application.
- Gallery position, size and Keep open are remembered for this game session only. The first opening is centered; remembered geometry is clamped to the current viewport.
- List metadata now saves to library-view-v3.sla, importing v2 or v1 if v3 is absent without changing the older file. Downgrading to 0.1.23 uses the older list settings. Saved light/preset file formats are unchanged.

## Changed in 0.1.23

- Both upper and lower controls now read Bookmark [Set] [Restore]; Japanese reads 栞を [登録] [戻す].
- The group uses measured label/button widths to fit beside camera alignment when possible; narrow windows wrap only when needed.
- Bookmark behavior, hover help and disabled states are unchanged. Set replaces the one temporary bookmark; Restore restores its lighting settings.
- Check both languages and window resizing in game. The previous release's English UI and NPC application have been user-tested; this new bookmark layout has not yet been checked in game.

## Install

Close Skyrim. Back up the previous `ScreenshotLightingAssistant` storage folder and keep your previous installer for rollback. Install **ScreenshotLightingAssistant-0.3.0-MO2.zip** with MO2; disable the previous DLL. The archive root contains `SKSE/Plugins` and `docs`, with no extra `Data` wrapper. Do not install the separate source or symbols ZIP in MO2.

Requires SKSE64, Address Library, SKSE Menu Framework 3, and the Microsoft Visual C++ 2015–2022 Redistributable (x64). This build targets SE/AE, not VR. No ESP, game INI edits or changes to other mods are included.

## Controls

Open Screenshot Lighting Assistant > Lighting in the mod control panel. Use EN at the top for English and JP to return to Japanese. The choice is saved across restarts. Custom names and NPC names are not translated.

- Select a slot (1-3), direction and type. Built-in order: Studio White, Studio Warm, Studio Blue, Candle.
- Start lighting to display the setup. Lights stay on when the menu closes.
- Each light has independent color, intensity, range, distance, height, fine offsets, shadows and power.
- Reset values restores only intensity, range, distance, height, fine offsets and shadow bias. It keeps direction, type, color, shadow mode and power, and can be undone.
- Shadow bias appears only in Spot mode. Hover for its explanation. It is depth bias, not a shadow blur control.
- Save light stores type and color only. Favorites > Manage > Rename changes a saved light's name.
- P / Choose preset opens the same gallery. Save preset stores the full three-light setup, optionally including the face light. Enable Manage in the gallery to rename, reorder or remove your presets.
- The person icon beside P opens Persistent Face Light settings. Enabling Player keeps an independent face light on the player outside photography sessions; assign a keyboard key or a long-press gamepad button for quick on/off control.
- Undo/Redo and the bookmark cover lighting values. The bookmark lasts only until game exit; use Save preset for durable storage.
- Preset diagrams use the older 140px proportions, uniformly enlarged, with name and light information on the right.

## NPC subjects

Choose subject opens a distance-sorted list of loaded humanoid NPCs within 30m of the player (up to 64). Refresh to rescan; there is no continuous background NPC scan. Select a candidate by name, distance and reference ID. Use player restores the player as subject.

Console selection and crosshair target are optional alternatives. In free camera or while a menu is open, the game may not supply a crosshair target; use the nearby list instead.

Changing the subject switches the lights off but keeps their settings. Close the subject window, check the displayed name, then press Start lighting. The three lights follow the subject's chest/head anchor; the face light uses that subject's head and the selected placement mode. Camera alignment still controls the three-light arrangement.

Only one photography subject is supported at a time. This mod does not freeze actors or change AI, schedules, poses or NPC records. It never force-loads NPCs. Subject loss or cell changes stop the photography lights; loading/main-menu transitions clear subject selection back to the player. Photography lights never restart automatically. The optional persistent player face light is separate and safely recreates after transitions. Subjects, handles and camera transforms are not written to presets or game saves.

## Storage and recovery

The selected storage path is shown by hovering over Storage location at the bottom. Version 0.1.28 targets virtual `Data/SKSE/Plugins/ScreenshotLightingAssistant/Presets`; after an actual write, MO2 normally places a new file in `Overwrite/SKSE/Plugins/ScreenshotLightingAssistant/Presets`. Alpha34 and later do not create that directory during a read-only launch.

Alpha35 treats an existing valid `storage-v2.sla` as an explicit ownership marker. Move the complete `SKSE/Plugins/ScreenshotLightingAssistant/` folder, including that marker, from Overwrite into a dedicated output mod. On the next launch, the plugin resolves the physical folder which supplies the marker and stores newly-created `.slalight`, `.slaset`, and `library-view-v3.sla` files beside it. A same-named folder without the marker is deliberately ignored because several mods can contribute the same virtual directory. Without a marker, MO2's ordinary Overwrite or configured output-mod behavior remains authoritative. Storage remains independent of Skyrim saves.

The first successful 0.1.28 launch copies the previous language preference, entries, list metadata and Archived directory without deleting the originals. `storage-v2.sla` prevents later re-import and now also owns the destination folder, so removing an entry does not make an old copy reappear. Back up the complete folder as desired.

Existing .slalight/.slaset formats and saved light type IDs are unchanged. Favorites and preset order retain their file-based identity when an entry is renamed. Duplicate or invalid names are refused (120 UTF-8 bytes maximum, no control characters or ##). Historical entries keep the names recorded at that time.

Before renaming, an exact backup is placed in Archived with a filename ending in -before-rename-<original filename>. The original entry is atomically replaced under its existing filename. To restore a backup manually, close Skyrim, preserve the current file separately, then restore the backup under the corresponding original filename. Simply copying it into Presets with a different name creates a separate registration.

The language preference is stored one directory above Presets in `ui-language.sla`; the persistent player face-light profile is stored beside it in `persistent-face.sla`. Unknown/corrupt preferences are preserved rather than overwritten. Invalid saved entries are reported without deleting their files.

## Verification boundary

The MSVC DLL build and automated checks pass. The 0.1.29 photography features and beta2 persistent-light behavior were exercised in game by the author. Beta3's gamepad input still requires in-game verification. This does not claim compatibility with every game build or mod combination. Keep only one version of this DLL enabled.

This release does not claim compatibility with every game build or mod combination. See `RELEASE_CHECKLIST_JA.md` for the Japanese verification record and `ENGINE_NOTES.md` for implementation details. Source licensing is in `LICENSE` and `THIRD_PARTY_NOTICES.md`.

## Author and license

Copyright © 2026 kota (@kotaSkyrim)  
Licensed under GPL-3.0-or-later. See `LICENSE` and `THIRD_PARTY_NOTICES.md`.
