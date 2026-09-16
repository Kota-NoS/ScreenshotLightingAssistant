# 0.3.0-alpha44 — neutral Star, Feather and Snow materials

- Magic Stars, White Feathers and Snow Crystals now use `BSEffectShaderProperty::baseColorScale = 1.00`, neutral-white emitter RGB and neutral-white `BSPSysSimpleColorModifier` RGB keys.
- Their existing fade alpha keys remain respectively `0.00 / 0.94 / 0.00`, `0.00 / 0.92 / 0.00` and `0.00 / 0.86 / 0.00`. Shape, UV layout, motion, emission and lifetime data are unchanged.
- White and Black Feathers share the same NIF. `PhotoEffectTint(BlackFeathers)` compensates for the removed shared-NIF attenuation so the dedicated black atlas retains approximately alpha43's peak RGB contribution.
- Builder reload verification rejects non-neutral values for these three families and Sakura. Exactly 189 of 441 generated effect NIFs should differ from alpha43; all eight DDS assets must remain byte-identical.

# 0.3.0-alpha43 — approved Sakura atlas and repaired poses

- The artist-edited `sakura_petal_atlas.png` is now the source of truth. Its corresponding packaged DDS is the exact BC7 UNORM asset used for the accepted in-game comparison.
- The right silhouettes of atlas cells `(0,0)` and `(0,1)` were restored from the original sample. All eight visible alpha bounds remain inside their 256 x 256 UV cells, so filtering cannot sample a neighboring pose.
- The NIF-side alpha42 material normalization remains unchanged. All 441 effect NIFs and the other seven effect textures are byte-identical to alpha42.
- `prepare_effect_textures.py` validates every Sakura cell and preserves the hand-finished PNG instead of regenerating it from the older raw sample. Because Pillow does not encode BC7, its rebuild path emits a DXT5 fallback; release packaging uses the tested artist-exported BC7 DDS.

# 0.3.0-alpha42 — neutral Sakura particle material

- Sakura `BSEffectShaderProperty::baseColorScale` is restored from `0.82` to `1.00`. Runtime brightness `1.00` therefore uses the authored baseline instead of requiring approximately `1 / 0.82 = 1.22` to cancel the NIF attenuation.
- Sakura emitter RGB and all three `BSPSysSimpleColorModifier` RGB keys are neutral white. Their alpha keys remain `0.00 / 0.92 / 0.00`, preserving the existing lifetime fade while removing the second pink multiplication over the atlas.
- Builder reload verification now rejects any generated Sakura NIF whose base scale, emitter colour, or fade RGB/alpha differs from those invariants.
- The alpha41 Sakura DDS is unchanged. Petal silhouettes, eight poses, motion, emission, lifetime variants, the other seven effects, lighting, UI and storage behavior are unchanged. Renderer confirmation is still required.

# 0.3.0-alpha41 — lifted Sakura midtones with localized blush

- Alpha40 retained useful luminance contrast but its median RGB of approximately `(215, 205, 209)` was too dark and too neutral in-game.
- `prepare_sakura_material()` now remaps luminance with `138 + 112 * pow(clamp((L - 95) / 160), 1.14)` and applies a restrained pink separation that is strongest in authored roots, veins and folds.
- Target visible-pixel median RGB is approximately `(233, 211, 221)`, median luminance `216`, and luminance standard deviation `19`, between alpha39's flat bright result and alpha40's detailed but muted result.
- PNG and decoded DXT5 alpha bytes remain identical to alpha40. Petal NIFs, eight poses, motion, all other effect assets, lighting, UI and storage behavior are unchanged.

# 0.3.0-alpha40 — independent Sakura luminance and chroma

- Alpha39's white blend reduced visible-pixel luminance standard deviation from alpha38's `23.0` to `7.4`, leaving insufficient texture headroom before the effect shader and scene exposure clipped the petal to a flat value.
- `prepare_sakura_material()` now sharpens source luminance lightly, remaps it with `110 + 138 * pow(clamp((L - 95) / 160), 1.18)`, then adds a small low-saturation blush derived from localized source red detail.
- The resulting visible-pixel luminance standard deviation is approximately `22.8`, with median luminance approximately equal to alpha38, while chroma remains far below alpha38. Fully-transparent padding uses neutral RGB `(236, 236, 238)`.
- PNG and decoded DXT5 alpha bytes remain identical to alpha39. Petal NIFs, eight poses, motion, all other effect assets, lighting, UI and storage behavior are unchanged.

# 0.3.0-alpha39 — opacity-aware Sakura white/blush distribution

- `prepare_sakura_material()` replaces global saturation expansion with an alpha-aware blend toward blush white `(255, 250, 252)`.
- Original atlas RGB weight is `0.35 * (0.45 + 0.55 * opacity)`: opaque roots, veins and folds retain more authored variation, while translucent edges approach white and resist colored filtering rims.
- PNG and decoded DXT5 alpha bytes remain identical to alpha38. Petal NIFs, eight poses, motion, all other effect assets, lighting, UI and storage behavior are unchanged.

# 0.3.0-alpha38 — pale Sakura-petal balance

- `prepare_sakura_material()` reduces the alpha37 saturation expansion from 1.45 to 1.18, deliberately staying close to the alpha36 source while retaining a visible Sakura blush.
- The same transform covers hidden RGB; PNG and decoded DXT5 alpha remain identical to alpha37.
- Petal NIFs, eight poses, motion, all other effect assets, lighting, UI and storage behavior are unchanged.

# 0.3.0-alpha37 — restrained Sakura-petal saturation

- `prepare_sakura_material()` runs immediately after the normal petal atlas preparation and expands color saturation by 1.45 while preserving the authored luminance structure.
- The same transform covers hidden RGB so bilinear filtering cannot introduce a differently colored seam around translucent edges.
- PNG and decoded DXT5 alpha coverage remain identical to alpha36. Petal NIFs, eight poses, motion and all other effect assets are unchanged.

# 0.3.0-alpha36 — white-feather tonal headroom

- The motion-preset note now explicitly begins with “A preset”.
- `prepare_white_feather_material` expands the White-feather atlas's visible luminance range after its existing silhouette/pose preparation. A restrained cool-gray floor preserves shafts and vane structure while white highlights remain white.
- The White atlas alpha channel and coverage remain byte-identical to alpha35. Hidden RGB in fully-transparent padding is neutralized to prevent the former pink value from entering filtered borders.
- Black feathers are still derived before the White-only tone map, keeping their DDS byte-identical to alpha35. The other seven effect DDS assets, every NIF, runtime motion, controls, lighting, storage behavior and serialized formats are unchanged.

# 0.3.0-alpha35 — explicit storage ownership and final UI trimming

- A valid existing `storage-v2.sla` is opened through virtual Data and resolved with `GetFinalPathNameByHandleW`. Its physical parent becomes the storage root, allowing newly-created per-preset files and view metadata to remain in a dedicated MO2 output mod after the complete storage folder has been moved there.
- Resolution is marker-gated. A same-named directory alone is not enough, and a marker-free install continues through virtual Data and MO2's normal Overwrite/configured-output behavior.
- The persistent-face launcher toggles its window and cancels any in-menu keyboard/gamepad capture when closing. The Photo Effects brightness explanation is removed.
- Effect assets, particle/runtime behavior, controls and serialized formats are unchanged from alpha34.

# 0.3.0-alpha34 — photo-effect UI finish and lazy mutable storage

- The Photo Effects window is now a normal constrained, scrollable ImGui window instead of `AlwaysAutoResize`; its initial size is 420x650 and users can resize it within the main viewport.
- Display order is mapped separately from `PhotoEffectType`, keeping Stars/Stars 2 and White/Black feathers adjacent without changing persisted numeric IDs.
- P and the star launcher toggle their corresponding windows. The motion-preset note and density legend are shortened/removed.
- `PrepareStorageLocation` performs no write probe, directory creation or marker write when no legacy data exists. Actual setting/preset writers remain responsible for creating parent directories. A real legacy migration still probes, imports without overwrite, and writes the one-time marker.
- MO2 virtual Data cannot identify a writable physical source-mod directory; actual writes still route to Overwrite. Effect assets and runtime rendering are byte-identical to alpha33.

# 0.3.0-alpha33 — complete dense-dendrite crop and exterior baseline

- The straight cutoff in the alpha32 screenshot identifies a second crop error rather than attenuation. The upper-left dense reference component reaches `y=592`, while its quadrant ended at `y=543`.
- Both upper source regions now extend to 60% of the 1086px sheet (`y=652`). Their connected components end at `y=592` and `y=567`, leaving 60px and 85px of verified transparent margin. Generation fails if either component again approaches within eight pixels of the lower crop boundary.
- No rotational union, symmetry reconstruction or shape redraw is applied; the selected photographic ice structure is retained.
- All four Snow rows lift low/mid alpha from the alpha32 remap's `7 + 112*effective^1.16` (cap 118) to `10 + 164*effective^1.10` (cap 174), and shift visible RGB toward neutral white ice. This targets readable `1.00 / 1.00` output on blue-grey exterior backgrounds while retaining partial-alpha faces.
- Atlas layout, Snow NIFs, motion, spawn ratios, earlier DDS assets, controls and lighting remain unchanged. Renderer confirmation is still required.

# 0.3.0-alpha32 — complete compact-dendrite crop

- The alpha31 upper-right reference region ended at the source sheet's exact horizontal midpoint (`y=543`), but that connected crystal continued through `y=566`. Its downward primary branch was therefore truncated at the crop boundary.
- Only that source region now extends to 60% of the sheet height. The connected-component bounds become complete without reaching the separate lower-right hex plate.
- The reconstructed RGB/alpha treatment, atlas layout, target sizes, all Snow NIFs and every earlier DDS remain unchanged. Renderer confirmation is still required for the corrected tip.

# 0.3.0-alpha31 — selected clear-ice source and distinct families

- In-game alpha30 screenshots showed that its shared rounded rime language made all four families collapse into one six-lobed icon, while alpha around 92-172 made the broad surfaces read as milky sugar rather than clear ice.
- The shipping atlas now crops the four connected crystals from `assets/reference/snow_crystal_material_reference.png`: dense dendrite, compact dendrite, open sparse dendrite and hexagonal plate. Generation specks are discarded with a connected-component pass.
- Source luminance retains photographic veins and internal facets, but every visible and hidden RGB value is rebuilt in a neutral ice-blue range. Source alpha is remapped to 0-122; visible row means are roughly 69-84 instead of alpha30's 102-106, with no opaque pixels.
- All eight columns in a row remain byte-identical. Snow NIFs and all seven prior DDS assets remain byte-identical to alpha30; renderer appearance still requires the alpha31 in-game checklist.

# 0.3.0-alpha30 — frosted plate-body snow crystals

- In-game alpha29 screenshots confirmed that the tiny low-alpha centre face vanished while the higher-alpha shafts survived, leaving a clean but unintended skeletal symbol.
- `prepare_snow_crystal_atlas` now constructs broad tapered alpha bodies for every main and side branch, wide tip facets and centre plates, then layers low-contrast ridges and deterministic asymmetric rime grains over them. Visible RGB ranges from cool transparent ice to soft white-blue frost; no visible or hidden pixel uses black or purple.
- The atlas remains uncompressed RGBA, and all eight columns in each row remain byte-identical. The Snow NIFs and every pre-alpha29 asset stay byte-identical to alpha29; renderer appearance still requires the alpha30 in-game checklist.

# 0.3.0-alpha29 — translucent fixed-identity snow crystals

- `prepare_snow_crystal_atlas` deterministically draws a 2048 x 1024 uncompressed RGBA atlas with four sixfold families: large dendritic, medium stellar, needle-like and small hexagonal. Fully transparent padding keeps an ice-blue hidden RGB so bilinear edges cannot sample black or unrelated colours.
- The faint plate, frosted underlay, narrow ridge, branches and tips use separate partial-alpha levels. This suggests translucent ice without relying on scene refraction, while leaving the background genuinely transparent.
- Each of the four particle systems is locked to one atlas row. All eight columns in a row are byte-identical, so the existing subtexture machinery cannot pulse, resize or morph a live crystal. Rotation remains independent in the NIF.
- Snow adds 63 NIFs (three capacity tiers x normal/CW/CCW x seven lifetime variants), bringing the installer to 441 NIFs across seven mesh families. The previous 378 NIFs and seven DDS assets remain byte-identical. Renderer appearance still requires the alpha29 in-game checklist.

# 0.3.0-alpha28 — black-at-1.00 feather tone map

- In-game alpha27 screenshots showed that brightness 1.00 still read as silver, while 0.26-0.36 produced the intended black-feather appearance. The deterministic remap now uses `5 + 65 * n^3.2` instead of `8 + 180 * n^3.2`.
- On the source atlas this changes visible median luminance from 63 to 25 and the peak from 188 to 70. About 5.46% of visible pixels remain at or above 64; none reaches the silver range at or above 128. Alpha remains byte-identical.
- Runtime texture replacement, the fallback tint, all 378 packaged NIFs and the other six packaged DDS assets are unchanged. Renderer appearance still requires the alpha28 in-game checklist.

# 0.3.0-alpha27 — restrained black-feather highlights

- In-game alpha26 screenshots showed that the dedicated atlas preserved detail but lifted too broad an area into pale silver at brightness 1.00. The deterministic remap now uses `8 + 180 * n^3.2` instead of `8 + 230 * n^2.8`.
- On the source atlas this changes visible median luminance from 89 to 63, the peak from 238 to 188, and pixels at or above 128 from about 24% to about 13%. Deep black coverage and alpha remain unchanged.
- Runtime texture replacement, the fallback tint, all 378 packaged NIFs and the other six packaged DDS assets are unchanged. Renderer appearance still requires the alpha27 in-game checklist.

# 0.3.0-alpha26 — high-contrast black-feather texture on shared NIFs

- `prepare_black_feather_atlas` derives a neutral grayscale atlas from the final White-feather PNG. It preserves the alpha channel exactly, applies a deterministic high-power luminance curve with light local sharpening, and replaces fully transparent padding RGB with neutral black.
- Runtime still resolves both feather types to the same `white_feathers_*` NIFs. For `BlackFeathers` only, `BSShaderManager::GetTexture` loads `black_feather_atlas.dds` and replaces the cloned effect material's source texture; the material RGB tint stays identity so texture contrast is not compressed.
- The installer remains at 378 NIFs and adds one DDS. Renderer appearance and cross-runtime texture replacement still require the alpha26 in-game checklist.

# 0.3.0-alpha25 — black feathers through shared white-feather assets

- `PhotoEffectType::BlackFeathers` is appended as numeric value 6, preserving the serialized values of all six existing types.
- Both feather types resolve to the same `white_feathers_*` NIFs and the same four particle-system and emitter names. The installer therefore remains at 378 NIFs and adds no black-feather DDS.
- Runtime captures each cloned shader's authored RGB and brightness scale. It restores authored RGB for all existing effects, while Black feathers multiply it by the neutral graphite tint `(0.12, 0.125, 0.14)` before applying the existing opacity and brightness compensation.
- The tint is per cloned material instance and a type change respawns the effect, preventing black state from leaking into White feathers. Renderer appearance still requires the alpha25 in-game checklist.

# 0.3.0-alpha24 — directional long-feather vane separation

- `prepare_effect_textures.py` now applies deterministic, supersampled alpha cuts only to row 2 of the four-row white-feather atlas. Six sparse cuts follow the long-feather length with per-frame offsets and asymmetric side selection; broad views also receive a shallow split tip.
- The processor only subtracts alpha, rejects empty or excessive edits, and caps removed coverage at 5.5%. Narrow edge-on frames receive reduced cut depth and no tip split.
- Rows 1, 3 and 4, all RGB detail, the 2048 x 1024 BC3/DXT5 atlas contract, and all 63 white-feather NIFs remain unchanged. Renderer appearance still requires the alpha24 in-game checklist.

# 0.3.0-alpha23 — clean Stars 2 edges and UI naming

- The UI label is now `星2` / `Stars 2`. `PhotoEffectType::PopStars`, numeric serialization, `pop_stars` paths and all runtime node names remain unchanged so alpha22 settings and ParticleWind exclusions continue to resolve.
- `pop_star_atlas.dds` is now uncompressed 32-bit RGBA rather than BC3/DXT5. This removes shared 4 x 4 colour endpoints that could appear as unrelated coloured blocks on a highly magnified flat-colour silhouette.
- Every transparent texel in each atlas row carries that row's flat RGB colour while retaining alpha zero. Bilinear samples at visible edges and the hollow centre therefore stay hue-consistent.
- The atlas geometry, alpha coverage, four RGB values, four rows, eight fixed frames and the complete 378-NIF mapping are unchanged. Renderer appearance still requires the alpha23 in-game checklist.

# 0.3.0-alpha22 — fixed-size Pop stars with a clearer flat palette

- The four Pop-star atlas rows now use exact flat RGBA treatments: hollow bright yellow `(255,255,102)`, coral pink `(220,120,133)`, warm pale yellow `(251,240,122)` and sky blue `(153,204,255)`.
- Row 0 is drawn as a true alpha hole rather than a white-filled centre. All eight frames in every row are identical in scale and opacity, eliminating the alpha21 Pop-star pulse while retaining row identity, independent birth time, movement and rotation.
- `PopStars` still uses four particle systems and the same 4 x 8 atlas layout, so the 378-NIF runtime map and ParticleWind exclusions are unchanged. Luminous-mote twinkling is unchanged.
- Renderer appearance still requires the alpha22 in-game checklist.

# 0.3.0-alpha21 — asynchronous twinkle and flat pop stars

- Luminous motes now use an eight-frame horizontal atlas. `BSPSysSubTexModifier` randomizes both the starting frame and loop phase so each live particle softly changes size and alpha on its own timing; the dimmest frame remains visible to avoid a field-wide blackout.
- `PopStars` is a sixth `PhotoEffectType`, mapped to `pop_stars`. Four internal particle systems lock each particle to one atlas row: sky-blue outline/white center, coral pink, pale lemon and pale aqua. Frames within a row only pulse size and alpha, so a star cannot morph into another colour treatment.
- The shipping atlas is deterministically redrawn from the original generated design direction. It contains genuine alpha and flat project-authored colour blocks; no stock-reference pixels, watermark or checkerboard are included.
- Pop stars inherit the existing live density, particle-size, spread, opacity, brightness, vertical-position and motion controls. Normal variants remain in world space; orbit variants remain target-local. ParticleWind NG exclusions name all four systems.
- Six types x three capacity tiers x three motion spaces x seven lifetime steps produce 378 packaged NIF variants. Renderer appearance still requires the alpha21 in-game checklist.

# 0.3.0-alpha20 — compensate the extra effect-shader alpha factor

- Alpha19 wrote the requested value to `BSEffectShaderMaterial::baseColor.alpha` and multiplied the authored `baseColorScale` by Brightness. In-game comparison shows the effect-shader/source-alpha path attenuates low opacity once more than the UI's simple product model predicts.
- Runtime now applies `authoredBaseColorScale * (brightness / opacity)` while leaving `baseColor.alpha` at the requested value. This cancels one internal alpha factor: the intended post-blend contribution remains approximately `opacity * brightness`, while coverage and soft edges still use the requested opacity.
- At full opacity the divisor is one, so the alpha19 1.00 baseline and 0.25-4.00 Brightness response are unchanged. At minimum opacity the internal scale can reach 16 only for the explicit 0.25 x 4.00 setting.
- The helper is clamped to the existing slider ranges and unit-tested independently. No NIF, texture, shader flags, particle lifecycle or transform is changed. The exact result still requires the alpha20 in-game checklist because ENB/Community Shaders, fog, exposure and bloom remain downstream.

# 0.3.0-alpha19 — independent live opacity and brightness

- Each discovered `NiParticleSystem` now retains its `BSEffectShaderProperty` and the material's authored `baseColorScale` as runtime-only state.
- Opacity writes `BSEffectShaderMaterial::baseColor.alpha`; brightness multiplies the captured authored `baseColorScale`. This preserves the different calibrated brightness of motes, petals, stars, leaves and feathers instead of flattening them to one absolute value.
- Both values are written to existing systems on the regular game-thread update path. They do not rebuild the particle field, change emission, reset subtexture animation or transform particle positions.
- Ranges are deliberately independent: opacity 0.25-1.00 and brightness 0.25-4.00. The product is only an approximate visual-strength guide because the renderer, ENB/Community Shaders, fog and exposure remain downstream.
- No NIF or texture rebuild is required. Compile-time and unit checks cannot prove renderer response; use the alpha19 in-game checklist.

# 0.3.0-alpha18 — exterior-safe live particle sizing

- Alpha17 scaled each `NiParticleSystem` by `size / spread`. For world-space systems this also transforms stored particle positions; the large coordinates used by exterior cells can therefore move live particles out of view when size changes.
- Runtime now keeps the particle-system world scale at one by using `1 / spread` below the scaled emitter node, and writes the requested size to every `NiPSysData::sizes` slot.
- Existing and newly emitted billboards receive the same multiplier without changing their world positions. Spread remains isolated to the emitter node.
- This is a runtime-only change. NIFs, textures, shader flags, motion values, effect identities and all lighting paths are unchanged. In-game verification is required because the order of Skyrim's particle update and the SKSE game task is runtime behavior.

# 0.3.0-alpha17 — original identity-preserving white feathers

- `PhotoEffectType::WhiteFeathers` is the fifth independent effect type and keeps all earlier numeric values unchanged.
- Four particle systems each own one row of a project-specific 8x4 transparent atlas: small down, long flight, curved and decorative plume. The looping subtexture modifier stays inside that row, preserving identity while showing front, oblique, edge-on and back views.
- Runtime discovery, continuous emission rate, particle-size/spread compensation, gravity motion, orbit and cleanup treat the four systems as one logical effect.
- The source image was generated specifically for this project and normalized by `tools/prepare_effect_textures.py`; the user's stock-site image was not copied. ParticleWind NG compatibility names the four feather systems.
- Five visuals, three capacity tiers, normal/clockwise/counterclockwise motion and seven lifetime steps produce 315 packaged NIFs. Structural checks cannot prove final appearance; use the alpha17 in-game checklist.

# 0.3.0-alpha16 — fixed leaf identity with out-of-plane tumble

- Autumn leaves now use four particle systems, one for each leaf identity. Every system owns one row of a project-specific 8x4 atlas; all eight cells in that row depict front, oblique, edge-on and back views of the same leaf.
- A looping `BSPSysSubTexModifier` advances only within the owning row. This restores the visible tumble variation the user liked without allowing a maple leaf to become a rounded, narrow or curled leaf during its lifetime.
- Runtime emission-rate, particle-size, spread and gravity updates treat the four systems as one logical effect and divide the requested total emission rate evenly between them. Petal behavior is unchanged.
- ParticleWind NG compatibility names the four leaf systems. Structural inspection verifies four systems, eight offsets per system, a single atlas row per system and the authored tumble-frame fields. Final appearance still requires the alpha16 in-game checklist.

# 0.3.0-alpha15 — fixed leaf identity

- Autumn leaves now use eight particle systems inside each NIF. Each system owns one atlas offset and a one-frame `BSPSysSubTexModifier`, so an emitted leaf cannot advance into another silhouette.
- Runtime discovery, emission-rate updates, particle-size compensation and gravity motion now operate on all eight leaf systems as one logical photo effect. The requested total leaf emission rate is divided evenly among them.
- Petals retain the existing eight-frame flipbook modifier. ParticleWind NG compatibility names all eight leaf systems separately.
- Structural inspection verifies eight leaf systems, one atlas cell per system and fixed frame fields. In-game rendering still requires the alpha15 checklist.

# 0.3.0-alpha14 — autumn leaves and ParticleWind NG compatibility

- `PhotoEffectType::AutumnLeaves` is a fourth independent effect type. It uses its own runtime mesh stem, particle-system/emitter names and Japanese/English UI strings without changing the existing three type values.
- The project-specific 4x2 transparent leaf atlas contains eight yellow, orange and russet leaves. `tools/prepare_effect_textures.py` normalizes it into a 1024x512 DXT5 DDS; the NIF uses randomized atlas phase, rotation direction and rotation speed.
- ParticleWind NG compatibility is declarative and optional: `SKSE/Plugins/ParticleWind/SLA_NoWind.json` maps each exact SLA particle-system name to `0.0`. No ParticleWind DLL/API is linked or required.
- Four visuals, three capacity tiers, normal/clockwise/counterclockwise motion and seven lifetime steps produce 252 packaged NIFs. Unit tests and NIF structural inspection cannot prove appearance or third-party config loading; use the alpha14 in-game checklist.

# 0.3.0-alpha13 — original magic-star effect

- `PhotoEffectType::MagicStars` is a third independent effect type. It uses its own runtime mesh stem, particle-system/emitter names and Japanese/English UI text while retaining the existing motion, cleanup and target-selection paths.
- The source star image was generated specifically for this project, normalized into a transparent 256x256 DXT5 DDS by `tools/prepare_effect_textures.py`, and referenced as `textures\ScreenshotLightingAssistant\effects\magic_star.dds`. No ParticleFieldSE or other mod asset is copied.
- `tools/sparkle_nif_builder/create_sparkle_nif.cpp` builds the star particle hierarchy with nifly. Nifly is used only as the NIF authoring library; the actual particle layout, shader values, rotation, color and texture reference are authored by this project.
- Stars use randomized initial angles, spin directions and spin rates. Their density anchors are 3/6/10 particles per second, continuously interpolated by the same safe posed-float controller path introduced in alpha12.
- Three visuals, three capacity tiers, normal/clockwise/counterclockwise motion and seven lifetime steps now produce 189 packaged NIFs. Unit tests and NIF structural inspection pass; final appearance and lifecycle still require the supplied in-game alpha13 checklist.

# 0.3.0-alpha12 — continuous runtime emission rate

- Density is a continuous 1.00–3.00 value. Piecewise interpolation preserves the previous Low, Standard and High anchors exactly: luminous motes use 4/8/14 particles per second and petals use 3/6/10.
- Runtime always loads the matching high-capacity NIF for type, lifetime and orbit mode, then locates the authored posed `NiFloatInterpolator` through the particle system's `NiInterpController` and changes only its `floatValue`.
- The lookup accepts only a posed float (`floatData == nullptr`) and never uses a guessed subclass layout or raw offset. If the expected controller is unavailable, the effect remains on the safe static high-density NIF and logs one bounded warning after the existing 30 attempts.
- Density changes no longer recreate the particle field. Type, lifetime, orbit mode, direction and cell transitions retain their existing replacement and cleanup behavior. All scene-graph and controller writes remain on the existing one-shot game-task path.
- Unit tests cover both effect-specific anchor rates, interpolation, bounds and the unchanged motion model. A clean build cannot prove Skyrim evaluates the edited interpolator live; the alpha12 checklist requires in-game verification and keeps alpha11 as the immediate rollback.

# 0.3.0-alpha11 — independent emitter spread and particle scale

- Target-local orbit meshes use the direct `NiPSysGravityModifier` axis, unlike the inverted world-space variants. Alpha11 selects the convention from `settings.orbit` so the signed vertical control remains consistent in both modes.
- Fall from Above now uses the motion values selected in the user's reference video: speed 0.74, vertical -0.55, floatiness 0.30, high source offset 0.85m and lifetime 1.50x.
- Density remains the same three prebuilt NIF tiers and is exposed as a bounded integer slider; this avoids multiplying the existing 126 verified mesh variants.
- The particle hierarchy separates scale responsibilities: the emitter node receives field spread, while its child particle system receives `particleSize / spread`. The resulting particle-system world scale follows particle size while the emitter-node scale controls spawn coverage.
- Scene-graph writes stay on the existing one-shot game-task path. Unit and DLL tests cannot prove how Skyrim renders the separated hierarchy for both world-space and orbit particles; the alpha11 checklist explicitly verifies this in game.

# 0.3.0-alpha10 — corrected engine axis and expanded motion range

- In-game alpha9 testing established that `NiPSysGravityModifier` moved particles opposite the axis written by the runtime. Alpha10 negates the requested vertical and camera-right vector before storing it, so positive UI values mean up/right and negative values mean down/left.
- Horizontal movement expands from ±1.00 to ±2.00. With vertical movement at zero, the endpoints produce a pure camera-left or camera-right vector; combining both controls produces diagonals.
- Motion speed expands from 2.00x to 4.00x and the runtime force ceiling rises from 14 to 36 so values above the old limit remain effective. Orbit uses the same wider speed range.
- Preset values and the shared petal atlas/rotation system are unchanged. The corrected axis makes Fall from Above descend and Updraft rise without sacrificing the eight-frame petal appearance.
- Unit/model and build checks cannot prove the final direction, high-speed visual stability or cleanup in Skyrim. The supplied alpha10 checklist requires those in-game checks.

# 0.3.0-alpha9 — orthogonal particle motion controls

- Motion presets are now ordinary combinations of vertical offset, speed, signed vertical movement, signed camera-relative horizontal movement, floatiness, sway, randomness, lifetime and optional orbit. Editing one of those values selects Custom.
- Generated particles start with zero baked linear velocity. Runtime applies the requested signed force, so vertical 0 is a true hover, positive values rise and negative values fall. Floatiness independently softens acceleration without changing direction.
- Fall from Above no longer depends on a dedicated high emitter mesh. Its preset places the shared field above the subject and requests negative vertical movement. Updraft uses the same field with positive movement, avoiding the previous near-stall caused by competing baked velocity and gravity.
- The project-owned petal atlas, randomized subtexture phase and per-particle rotation remain common to every preset. Removing the dedicated fall mesh does not remove petal facing or rotation variation.
- Clockwise and counterclockwise orbit variants retain target-local simulation and bounded runtime root rotation. Non-orbit particles remain world-space after emission while the source follows the subject.
- Normal, clockwise and counterclockwise variants are generated for both types, three densities and seven lifetime steps: 126 NIFs in total. Lifetime, orbit mode or orbit direction changes recreate only the owned particle field; other controls update live.
- Scene-graph changes remain on the existing one-shot game-task path. Unit/model, DLL and NIF-structure checks cannot establish final visual speed, direction or cleanup in Skyrim; the supplied alpha9 in-game checklist is required.
