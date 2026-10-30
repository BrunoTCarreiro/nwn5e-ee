# Mocap cleanup & editing: how the dedicated tools structure their UI (MotionBuilder, Maya, Cascadeur, Rokoko, Blender, Unreal, Unity)

Research notes for someone designing a minimal home-made mocap editor for game-character body animation. Researched 2026-10-30.
Source-quality legend: "(doc)" = official documentation page opened or quoted; "(snippet)" = taken from a search-result summary of the linked page, which I did not open in full, so verify it before relying on it; "(forum)" = anecdotal user report; "(vendor)" = a seller with a commercial interest.

---

## 1. Autodesk MotionBuilder: HumanIK control rig, keying modes, pinning, floor contact, Story, layers. Which parts are central to mocap cleanup?

### Takeaway
MotionBuilder's cleanup loop is: characterize, plot the mocap onto the HumanIK control rig, then drag IK effectors on a 2D "character figure" panel or in the viewport. A keying mode (Full Body / Body Part / Selection) decides how much of the body follows and gets keyed, and pins (translation/rotation) hold the feet and hands still. Story (tracks, clips, Match, track Weight, body-part track masks, TimeWarp) is for **assembling and blending** clips. It is not where frame-level cleanup happens: you plot out of Story before you can key the rig.

### Cited Findings
- The Character Controls → **Controls tab** "displays the character with its Control rig and HumanIK controls". Toolbar: keying/manipulation modes **Full Body**, **Full Body – No Pull** and **Body Part**; a skeleton-display toggle; **Pin Translation** and **Pin Rotation** (select an IK effector cell, then click the pin button); **Release Children Pins** (temporarily releases all pinning); a pin-presets menu with "Create Preset" that saves the current translation+rotation pinning. Effector look is set via Edit > Controls > Rig Look. (doc) — [Autodesk MotionBuilder 2025: Controls tab](https://help.autodesk.com/cloudhelp/2025/ENU/MotionBuilder-Reference/files/MotionBuilder-Windows/Character-Controls/GUID-F286B854-A373-49F2-9DB3-1D788208026B.html)
- Effector selection on the figure panel: **click** selects one effector; **Shift-click** adds that effector *and every effector in the same body part*; **Ctrl-click** toggles single effectors; **Shift-Ctrl-click** adds or removes the whole body part. Clicking empty space selects every effector in the current view, and double-clicking empty space deselects. **Delete** removes an auxiliary/pivot effector. Right-clicking an IK effector cell gives "Create Aux Effector" / "Create Aux Pivot". Baking is a menu item: **Bake To Skeleton / Bake To Control Rig**. (doc) — [MotionBuilder 2025: Controls tab](https://help.autodesk.com/cloudhelp/2025/ENU/MotionBuilder-Reference/files/MotionBuilder-Windows/Character-Controls/GUID-F286B854-A373-49F2-9DB3-1D788208026B.html)
- Keying-mode semantics (HumanIK, shared by Maya and MotionBuilder). **Full Body**: moving one effector pulls the rest of the body, and the whole character is keyed. **Body Part**: only the selected limb or region is keyed; e.g. a left-wrist effector keys the wrist, elbow and shoulder. **Selection**: only the selected effectors are keyed, and a "reach" setting controls how much the rest of the body reaches for them. (snippet) — [Maya 2023 HumanIK keying modes](https://help.autodesk.com/cloudhelp/2023/ENU/Maya-CharacterAnimation/files/GUID-6EB8580A-0344-42F1-A722-4FDFD5BFA70C.htm); [Maya LT 2016 Controls tab toolbar](https://help.autodesk.com/cloudhelp/2016/ENU/MayaLT/files/GUID-735DF85A-331C-4049-8C70-E53EFCDB4B73.htm)
- When a HumanIK control rig is created, the **foot IK effectors are pinned in translation and rotation by default**. The search summary also gave a Pin Translation hotkey (Ctrl+W) from the Maya 2016 help; I did not verify that hotkey. (snippet) — [Maya 2016/2017 Controls tab toolbar](https://help.autodesk.com/cloudhelp/2017/ENU/Maya/files/GUID-735DF85A-331C-4049-8C70-E53EFCDB4B73.htm)
- **Floor contact** is a solver/characterization property, not a per-frame tool. With no floor defined, hands and feet use the plane Y=0. There are 4 contact types (Normal = 6 markers per hand/foot; Ankle/Wrist, Toe/Finger Base and Hoof = 4). Contact runs as an approximate pass *before* the Pull (full-body) engine, so limbs aren't pulled through the floor. Left and right sides **cannot** have different settings. Contact markers can be shown in the viewport and the contact engine run to preview results. (doc/snippet) — [MotionBuilder 2024 Floor Contact](https://help.autodesk.com/cloudhelp/2024/ENU/MotionBuilder/files/GUID-0E400563-9AEA-4041-8921-977A13E2F8AF.htm); [HumanIK SDK: Contact Types](https://help.autodesk.com/cloudhelp/2016/ENU/HumanIK-SDK-Help/files/GUID-3955695A-D38E-485E-BADA-FEF73F9A3B4F.htm); [HumanIK SDK: Foot and Hand Contact](https://help.autodesk.com/cloudhelp/2016/ENU/HumanIK-SDK-Help/files/GUID-DBE08C49-E87A-4053-A3D0-00C813122EE6.htm); [HumanIK SDK: Full-body IK](https://help.autodesk.com/cloudhelp/2016/ENU/HumanIK-SDK-Help/api/group___i_k.html)
- **Story workflow** (Ohio State mocap course):
  - Add a Character Animation Track and drag clips onto the character's hips or into Story, or right-click → "Insert Animation File(s)".
  - Order the clips, then click **Match** and choose the control effector the blend solves relative to. Clips should end and start **on the same foot** for realistic blends.
  - For **partial blending**, add a second track, use its **Figure icon** to pick the body parts it affects, uncheck "Full Body", and **animate the track Weight**.
  - For detailed transition editing, **Process Track to New Clip → Plot Whole Scene to Current Take**, then *delete the character track and all Story clips*: "keyframes can't be created on the control rig until this is done".
  - Reverse a clip with the "Reverse" checkbox.
  - **Retime per body part** in FCurves: enable TimeWarp, create a timewarp, select channels, click **Apply TimeWarp**, then move the timewarp keys. Swapping the first and last keys reverses time.

  (doc, course) — [OSU: MotionBuilder Story](https://www.asc.ohio-state.edu/berezina-blackburn.1/classes/mocap/story.html)
- Story **ghosts** are wireframe skeletons drawn in the viewer: track ghosts (the result of a whole track), clip ghosts, and travelling ghosts/travel nodes. Matching uses a clip's match object, or by default the travel node. (doc) — [MotionBuilder 2017: Work with ghosts in Story](https://help.autodesk.com/cloudhelp/2017/ENU/MotionBuilder/files/GUID-DB244A01-1C04-4A79-9C42-AAAD0B9C0A7C.htm); [MotionBuilder 2017: Match clips](https://help.autodesk.com/cloudhelp/2017/ENU/MotionBuilder/files/GUID-3A90587F-8C6F-4783-9A0E-71E773323B41.htm)
- User opinions (forum):
  - A studio animator "preferred [MotionBuilder] over maya for the initial stages of cleanup and editing".
  - A game-cleanup user says MotionBuilder's layers "exist in other packages but are smoother to work with", and that its take-based organization beats Maya's file-based approach.

  — [Polycount: help mocap cleanup in motion builder](https://polycount.com/discussion/222080/help-mocap-cleanup-in-motion-builder); [Polycount comment](https://polycount.com/discussion/comment/1976455)
- A Ubisoft senior animator's Polycount post describes a **two-layer rig**: a minimal "animation skeleton" (spine, head, arms, legs, fingers) that carries the exported keys, plus a character-specific skeleton for cloth, straps and holsters. This keeps exported data small and reusable across characters. (forum) — [Polycount comment](https://polycount.com/discussion/comment/1995129)
- A vendor blog calls MotionBuilder purpose-built for importing, retargeting, editing and exporting large volumes of mocap. (vendor) — [MoCap Online: 3D animation software comparison](https://mocaponline.com/blogs/mocap-news/3d-animation-software-comparison)

### Inferences
- Central to cleanup: (1) the control rig with effectors, (2) Body Part keying as the everyday mode (Full Body for posture or root fixes), (3) foot/hand pins, (4) animation layers for non-destructive offsets, (5) a timeline/FCurves for retiming. Story, Match and ghosts are central to *clip assembly and blending*, which is less important for single-clip cleanup.
- Floor contact is "set once per character" plumbing. A minimal editor can replace it with a ground-plane clamp in the IK solve.
- The Story → plot → delete-clips → key sequence is a mode boundary that users must learn. A minimal editor should avoid separating "assembly mode" and "edit mode" this way.
- The figure panel (a 2D body diagram with clickable effectors, plus Shift-click to grab a whole limb) is the best-documented selection affordance. It is worth copying because picking small joints in a 3D viewport is error-prone.

### Gaps
- I did not open the MotionBuilder Animation Layers documentation, so the layer UI (weight slider, additive vs override, mute/solo, merge) is unconfirmed from primary sources.
- The "Reach" (translation/rotation reach) sliders in the MotionBuilder Character Controls were not found in the 2025 Controls-tab page. They live on another page I did not open.
- I found no data on how often working animators use Story vs layers vs Body Part keying. The "central" ranking is inference.
- I did not verify the current MotionBuilder price/licensing, which is Autodesk subscription and not indie-friendly.

---

## 2. Maya: animation layers, Time Warp, HumanIK in Maya, Graph Editor on mocap, Time Editor

### Takeaway
Maya has the same HumanIK keying modes and pins as MotionBuilder. Its clip system (the Time Editor) uses its own **Clip Layers**, which cannot drive the same animation as regular Animation Layers. It also **ignores HIK keying modes**, a documented friction point. Maya's selling point for mocap is the Graph Editor and general rigging/tooling, not mocap-specific UX.

### Cited Findings
- HumanIK keying-mode icons sit at the top of Maya's HumanIK window (same Full Body / Body Part / Selection semantics as above). (snippet) — [Maya 2023: Keying Mode icons](https://help.autodesk.com/cloudhelp/2023/ENU/Maya-CharacterAnimation/files/GUID-6EB8580A-0344-42F1-A722-4FDFD5BFA70C.htm)
- The Time Editor uses **Clip Layers** for non-destructive edits on clips, and you cannot drive the same animation with both Clip Layers and standard Animation Layers. Looped clips must be grouped before you add layers to them. (snippet) — [Maya 2024: Add Clip Layers to the Time Editor](https://help.autodesk.com/cloudhelp/2024/ENU/Maya-Animation/files/GUID-B3FFE1AA-F44D-4EC3-A570-316A3BF3DF88.htm)
- The Time Editor **does not respect HIK keying modes**: you must manually include the right controllers in a clip before keying into it. (snippet) — [Maya 2022: Animate HumanIK characters in the Time Editor](https://help.autodesk.com/cloudhelp/2022/ENU/Maya-Animation/files/GUID-A3D4A1E8-E353-4ADA-A7C2-F264DF083C39.htm)
- On a HumanIK rig, make sure an override/additive layer drives only the attributes you intend, "or rig objects can get pinned unintentionally". (snippet) — [Maya 2022: Working with Time Editor clips](https://help.autodesk.com/cloudhelp/2022/ENU/Maya-Animation/files/GUID-63B2245A-5D4E-4844-A2F0-A2DFCA39F10C.htm)
- Pose clips can be built from existing animation files *or motion capture data*. (snippet) — [Maya 2023: Create animation from Pose clips](https://help.autodesk.com/cloudhelp/2023/ENU/Maya-Animation/files/GUID-83A752C8-49A1-436B-8CA9-C4FBB939B8B1.htm)
- A vendor blog calls Maya's Graph Editor "best-in-class" and notes HumanIK handles mocap retargeting. (vendor) — [MoCap Online comparison](https://mocaponline.com/blogs/mocap-news/3d-animation-software-comparison)

### Inferences
- The Maya docs show the cost of having **two layer systems** (Animation Layers vs Time Editor Clip Layers) and a clip editor that bypasses the rig's keying modes. A minimal editor should have exactly one layer concept, and every editing surface should respect the active keying mode.
- "Unintentional pinning" when a layer touches unintended attributes argues for layers that are **masked by body part** explicitly (like MotionBuilder Story's Figure-icon mask), not implicitly by whatever got keyed.

### Gaps
- I did not research Maya's Time Warp (the scene-level time-warp curve) or Graph Editor practice on dense mocap curves (e.g. Euler filter, simplify curve, butterworth filter). No primary source was opened.
- No user-forum data on Maya mocap cleanup pain points was collected beyond the vendor blog.

---

## 3. Cascadeur: AutoPosing, fulcrum points, AutoPhysics, trajectory editing, Animation Unbaking, timeline/interval model, free vs paid

### Takeaway
Cascadeur's mocap answer is **Animation Unbaking**: it converts a key-on-every-frame clip into sparse keys plus **interpolation intervals**, keyed where foot "fulcrum" contacts change. That turns mocap into something editable like hand-keyed animation, with **AutoPosing** (a full-body solver driven by a few controllers) for pose edits. Editing happens per **interval on the timeline**: Interval Edit Mode edits many frames at once, and Trajectory Edit Mode lets you drag motion paths directly. The free tier is non-commercial and exports only Cascadeur's own .casc format. Indie is the cheapest commercial tier; retargeting may be Pro-only.

### Cited Findings
- **Animation Unbaking** "takes a baked (fully keyframed) animation and automatically transforms it into an un-baked one, with fewer keyframes and various types of interpolation… used for tasks like cleaning mocap." Steps:
  1. **Auto Interpolation Keys**: keys where fulcrums change, plus a key at the top of a jump.
  2. **Adjust Keys and Interpolations**: adds keys and sets interpolation intervals and kinematic types.
  3. **Auto Unlock**: converts each pose to an AutoPosing pose with as few locked controllers as possible.

  Settings: Interpolation difference (default 20), AutoPosing precision (default 1), Fingers relative difference (default 1), Adjust fingers lock state (default on). Since 2024.2 it can run per **Animation Track**, so different body parts can get different unbaking levels. Manually rigged characters need a **Fulcrum Group** per ground-touching body set (e.g. one per hoof for a horse). (doc) — [Cascadeur help: Animation Unbaking](https://cascadeur.com/help/category/221)
- 2025.1 release notes (per search snippet): unbaking now supports *not fully baked* animations, and sliders were added to control the outcome. (snippet) — [Cascadeur help (release-notes category)](https://cascadeur.com/help/category/279)
- **Trajectory editing**: select point controller(s), select an **interval on the Timeline**, then enable **Trajectory Edit Mode** on the toolbar. On the path, *darker green points are keyframes and brighter points are interpolated frames*. Dragging an interpolated point switches that interval's interpolation to **Fixed**. "Set Trajectory Interval" pins the displayed trajectory so it stays visible when you select a different interval. (snippet of doc page) — [Cascadeur help: Trajectories](https://cascadeur.com/help/tools/animation_tools/trajectories)
- **Interval Edit Mode** adjusts multiple frames simultaneously. Its button was moved onto the Timeline, and it works with the Tween Machine and Copy Tools to blend animations. **AutoPosing** "generates character poses based on user-applied transformations". AutoPosing controllers can be edited in Trajectory Edit Mode, and a **Direction Controller** stops AutoPosing from rotating the character's facing. **Fulcrum points** (contacts such as landing spots) are mostly auto-detected, and their state can be forced per point in object properties. (snippet) — [Cascadeur help: Tools](https://cascadeur.com/help/tools); [Cascadeur help category 16](https://cascadeur.com/help/category/16); [Cascadeur help category 286](https://cascadeur.com/help/category/286)
- Cascadeur's free **Mocap Cleanup Course** has a lesson on keys, tracks and interpolation, and a "Best Tools for Mocap Cleanup" lesson covering Interval Edit Mode and Unbaking. (doc/course) — [Cascadeur skill path: mocap cleanup course](https://cascadeur.com/ru/skill-path/mocapcleanupcourse); third-party walkthrough: [3dart.it Cascadeur mocap cleanup tutorial](https://3dart.it/en/cascadeur-mocap-cleanup-and-editing-tutorial)
- **Licensing** (sources conflict on price):
  - Free = full toolset but non-commercial and **.casc-only export**.
  - Indie = commercial use under $100k/yr revenue. Reported prices: $12/mo or $96/yr, $99/yr, $8/mo annual, or $19/mo depending on source and date. Indie gained Finger AutoPosing and Layout Customization, and removed the 300-frame and joints limits.
  - Pro (~$49/mo or ~$396–399/yr) adds **Scene Linking, Animation Retargeting** and enhanced environment interaction in physics.
  - Per a 2025.3 report, quadruped characters need a paid plan.

  (snippet; verify on cascadeur.com) — [Cascadeur licensing FAQ](https://cascadeur.com/blog/general/cascadeurs-new-licensing-structure-comprehensive-faq); [Game World Observer 2024](https://gameworldobserver.com/2024/03/21/cascadeur-animation-unbaking-autophysics-new-pricing-model); [CGWorld JP on 2025.3](https://cgworld.jp/flashnews/01-202512-Cascadeur20253.html); [Toolradar pricing](https://toolradar.com/tools/cascadeur/pricing)
- Reviews:
  - A Product Hunt reviewer calls mocap cleanup Cascadeur's strength, saying it makes raw data look hand-keyed.
  - Cascadeur's own indie spotlight shows a creator who still had to fix legs, align a weapon and retime after unbaking (vendor).
  - I found **no sourced UI complaints**.

  — [Product Hunt reviews](https://www.producthunt.com/products/cascadeur/reviews); [Cascadeur blog: Shadows of Chroma Tower](https://cascadeur.com/blog/general/indie-spotlight-shadows-of-chroma-tower)

### Inferences
- Cascadeur's key idea for a minimal editor: **contact events (fulcrum changes) are the natural key positions** for mocap. Auto-placing keys at foot plant/lift and jump apexes, then interpolating between them, turns 1,000 dense keys into a few dozen editable poses.
- The **interval** (the span between two keys, carrying an interpolation type) is the unit of editing on the timeline, not the single frame. This maps well to a simple UI: click a span, then retime it or change its interpolation.
- Trajectory display with key/interp colour coding, plus "dragging the path makes the interval Fixed", gives direct-manipulation retiming and path editing without a curve editor.
- For an indie: retargeting is Pro-only and the free tier exports .casc only, so Cascadeur Free cannot be the shipping step of a pipeline.

### Gaps
- AutoPhysics specifics (what it corrects on mocap) were not researched in a primary source.
- The exact current names for "pinned" vs "locked" controllers in AutoPosing are unverified. The docs say "locked controllers"; "pin" wasn't found.
- Current official prices were not confirmed from the cascadeur.com pricing page.

---

## 4. Rokoko Studio: filters, foot locking, retargeting, what editing it offers

### Takeaway
Rokoko Studio's editing model is a **filter stack plus an editable foot-contact lane**, not pose keyframing. You review the per-foot lock keys on the timeline (colour-coded), fix them, then apply filters such as Locomotion, Drift Fix, Treadmill, Knee Pop Smoothing and Toe bend. Retargeting into DCCs goes through plugins, e.g. the free Blender add-on with game-skeleton presets.

### Cited Findings
- Some filters work live and some only on playback. Locomotion, Treadmill, Toe bend and **Drift Fix** work in both; **Knee Pop Smoothing** is playback-only. (snippet; fetch returned 403) — [Rokoko: Filters explained](https://support.rokoko.com/hc/en-us/articles/4410415474321-Filters-explained)
- Foot-lock keys are adjusted **directly on the timeline**: **green = left foot locked, blue = right foot locked**. When neither foot is locked, a **jump arc** is generated from the end of one lock to the start of the next. **Sliding is not supported.** (snippet) — [Rokoko docs: Edit in Rokoko Studio](https://docs.rokoko.com/rkk-vision-documentation/get-started/edit-in-rokoko-studio); [Rokoko: Filters explained](https://support.rokoko.com/hc/en-us/articles/4410415474321-Filters-explained)
- Drift Fix "works best when you physically start and stop your recording in the exact same location". Treadmill mode removes foot locking/locomotion, which suits upper-body clips. (snippet) — [Rokoko: Real-time Locomotion & Drift Fix](https://support.rokoko.com/hc/en-us/articles/41620261484945-Rokoko-Studio-Update-Real-time-Locomotion-Drift-Fix)
- Studio 2.5.0 made a new Locomotion engine the default. Old clips load with "Legacy Locomotion"; to upgrade one, disable it and click **"Revert to original values"** on the timeline to regenerate foot locks. (snippet) — [Rokoko Studio 2.5.0 release notes](https://support.rokoko.com/hc/en-us/articles/39602279983633-Rokoko-Studio-2-5-0)
- Studio 2.6.1 notes recommend that you "review your foot contact keyframes… confirm they are set correctly… before applying any filters". (snippet) — [Rokoko Studio 2.6.1](https://support.rokoko.com/hc/en-us/articles/47324364931089-Rokoko-Studio-2-6-1-with-MAJOR-Coil-Suit-Upgrade)
- The Rokoko Blender add-on is free, with retarget presets for Mixamo, UE4 Mannequin and UE5 Manny. A reviewer says it can lag behind new Blender releases. (vendor-blog) — [MoCap Online: Blender animation add-ons 2026](https://mocaponline.com/blogs/mocap-news/blender-animation-addons-guide)

### Inferences
- The **two-colour foot-contact lane** is the most copyable element here. It turns foot-slide fixing into editing *contact intervals* on a timeline, with the solver doing the IK, instead of hand-keying foot positions.
- "Review contacts, then run filters" implies the contact lane is the **source of truth** that the filters consume: a good architectural pattern for a minimal editor (contacts → locomotion/IK solve → output).
- Rokoko's lack of slide support and of general pose editing explains why users bring Rokoko data into another tool (Blender, Cascadeur, MotionBuilder) for real cleanup.

### Gaps
- I could not confirm whether Rokoko Studio offers any per-joint pose keyframing or layer editing. The "Filters explained" page returned 403.
- Rokoko Studio plan pricing and which filters are paywalled were not researched.

---

## 5. Blender: NLA, layer add-ons, Rokoko retargeting add-on, Auto-Rig Pro Remap, common mocap cleanup workflow and pain points

### Takeaway
Blender has no mocap-specific rig/effector layer. Cleanup is done with retarget add-ons (free Rokoko; paid Auto-Rig Pro Remap) plus the NLA for layering. The NLA's **additive blending has a long record of unreliable results**, and editing dense baked F-curves is a known weakness.

### Cited Findings
- A BlenderArtists user correcting a mocap walk with additive NLA strips got "unpredictable results, including massive distortion of the mesh" or a cancelled animation. (forum) — [BlenderArtists: Using NLA to correct motion capture anims](https://blenderartists.org/t/using-nla-to-correct-motion-capture-anims/1169389)
- Bug report T50433: NLA Add/Subtract blend modes produce broken playback. The developer noted that removing the existing NLA stack's effect from the current pose is "a known issue that's on my todo list". (doc/bug tracker) — [Blender T50433](https://developer.blender.org/T50433)
- A 2011 Blender animation-system proposal by Joshua Leung (Aligorith) lists "conversion + simplification for keyframe editing of baked mocap curves" and NLA baking as needed improvements. (doc) — [Blender Wiki: Aligorith GSoC 2011 proposal](https://wiki.blender.org/index.php/User:Aligorith/GSoC2011_Proposal_1AnimSysPolish)
- Vendor guidance:
  - Raw mocap "almost always" needs cleanup, and foot sliding is the most common artifact.
  - Typical import problems are scale, axis mismatch and T-pose vs A-pose; "never retarget before resolving scale".
  - Suggested workflow: push each imported action to the NLA as a strip and combine animations with strip blending and timing.

  (vendor) — [MoCap Online: Blender motion capture guide](https://mocaponline.com/blogs/mocap-news/blender-motion-capture-guide)
- Retargeting options:
  - The Rokoko add-on is free with game-skeleton presets.
  - Auto-Rig Pro Remap (paid, ~$40, unverified) maps bone names and orientations between arbitrary skeletons.
  - One blog's head-to-head (ARP, MrMannequin, Rokoko) rated Rokoko most accurate, provided the frame matches the target and "CURRENT POSE" is selected. Another reviewer prefers ARP.
  - ARP Spatial Retargeter uses world-space markers and works only with ARP rigs.

  (vendor/opinion) — [MoCap Online: add-ons guide](https://mocaponline.com/blogs/mocap-news/blender-animation-addons-guide); [cgdive: Blender retargeting add-ons](https://cgdivetest.super.site/sub-categories/blender-retargeting-addons); [Superhive: ARP Spatial Retargeter](https://superhivemarket.com/products/arp-spatial-retargeter)

### Inferences
- Blender's pain points (additive layering that breaks, dense curves that are hard to edit) are exactly what MotionBuilder layers and Cascadeur unbaking solve. A minimal editor should get **reliable additive offset layers** and **key reduction** right before anything else.
- Retargeting is a separate, fragile step in Blender (scale, rest-pose, bone mapping). A home-made editor targeting one fixed game skeleton can skip general retargeting entirely.

### Gaps
- No Reddit/r/animation threads were returned. Community sentiment is from BlenderArtists and vendors only.
- I did not research Blender 4.4+ "slotted/layered actions" (the new animation data model) and whether it fixes NLA additive problems. That needs a check against current release notes.
- I found no primary-source description of a "standard" Blender mocap cleanup workflow (e.g. Simplify F-curves, Smooth keys, Clean Channels usage).

---

## 6. Unreal Engine (IK Retargeter, Control Rig FK/IK baking, Sequencer additive/override layers) and Unity (Animation Rigging)

### Takeaway
In UE5 you retarget with the IK Retargeter, **bake mocap onto a Control Rig in Sequencer** (a key on every frame), and either edit those dense keys with pose/snapper tools or layer additive/override edits on top. Layered control rigs and Sequencer animation layers arrived around 5.4–5.5, and users report rough edges with override layers. Unity's equivalent (Animation Rigging "Bidirectional Motion Transfer") **does not support Humanoid rigs** and is lossy.

### Cited Findings
- A UE5 book chapter on mocap cleanup bakes mocap onto a MetaHuman Control Rig in a **dedicated Level Sequence**. Baking "transfers animation frame by frame… placing a keyframe on every frame". The goal is to "keep as much of the subtle, realistic core of the mocap data as possible". Cleanup used the **Poses and Snapper tools** in the Animation window, and lowering the Near Clipping Plane helped in tight spaces. (book) — [Packt: UE5 Character Creation, Animation and Cinematics, Ch.19](https://www.packtpub.com/en-kr/product/unreal-engine-5-character-creation-animation-and-cinematics-23-9781801812443/chapter/chapter-19-motion-capture-editing-and-cleanup-using-control-rig-and-sequencer/baking-the-stand-up-mocap-to-the-control-rig); [Packt Ch.19 summary](https://www.packtpub.com/en-kr/product/unreal-engine-5-character-creation-animation-and-cinematics-23-9781801812443/chapter/chapter-19-motion-capture-editing-and-cleanup-using-control-rig-and-sequencer/summary)
- Epic's marketing page says Sequencer gained **non-destructive animation layers**, with a choice of additive or override and **animatable layer weights**. A forum post describes the override type as new in 5.5, and users report an override layer over a layered Control Rig not moving the mesh, or behaving additively. (vendor + forum) — [Epic: animation software](https://www.unrealengine.com/try-free/animation-software-2?lang=de); [UE forum: animation layers with layered control rig](https://forums.unrealengine.com/t/using-animation-layers-with-layered-control-rig/2268479)
- On UE 5.0.1, adding an additive Control Rig track stopped the underlying animation track, which forced users to bake first. (forum) — [UE forum: Sequencer additive track](https://forums.unrealengine.com/t/sequencer-additive-track/553082)
- A forum answer: Control Rig can edit any imported anim, but it needs a **backwards (reverse) solve** setup. Do major edits, bake out sequences, and use a lighter rig for foot/hand IK. (forum) — [UE forum: combining external animations, Control Rig and Sequencer](https://forums.unrealengine.com/t/best-workflow-for-combining-external-animations-control-rig-and-sequencer-in-ue5/2746352)
- A third-party tutorial describes native mixer tracks with additive/override layers in UE 5.8, and warns that layers "survive re-delivery; they don't guarantee they still look right". (third-party; version claim unverified) — [StraySpark: UE5.8 animation mixer tutorial](https://www.strayspark.studio/blog/animation-mixer-ue5-8-sequencer-tutorial)
- **IK Retargeter**:
  - The pelvis/root is set separately from chains (right-click a bone → **Set Pelvis** in the IK Rig editor) so root motion transfers proportionally.
  - Per-chain IK goals are optional, used for Speed Planting, Stride Warping or Blend to Source. FK chain settings run before the IK pass.
  - In 5.7 the Python API marks older root/chain settings deprecated in favour of an op stack.

  (doc) — [Epic: IK Rig animation retargeting](https://dev.epicgames.com/documentation/en-us/unreal-engine/ik-rig-animation-retargeting-in-unreal-engine); [FTargetChainSettings 5.5](https://dev.epicgames.com/documentation/unreal-engine/API/Plugins/IKRig/Retargeter/FTargetChainSettings?application_version=5.5); [IKRetargeter Python API 5.7](https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/IKRetargeter?application_version=5.7)
- **Unity Animation Rigging**: each constraint exposes **"Transfer motion to constraint"** and **"Transfer motion to skeleton"**, applied per constraint, per Rig, or for the whole RigBuilder hierarchy. **Humanoid is not supported**, physics-based constraints (Damped Transform) cannot be inferred, and "the result of transferring motion does not precisely represent the source animation". Two Bone IK has no listed limitations. (doc) — [Unity Animation Rigging 1.3: Bidirectional Motion Transfer](https://docs.unity3d.com/Packages/com.unity.animation.rigging@1.3/manual/BidirectionalMotionTransfer.html)

### Inferences
- Engine-side tools follow the same pattern as MotionBuilder: **bake dense data onto an FK/IK rig, then layer edits on top**. The weak link is always the "bake onto rig" step (reverse solve required, lossy in Unity, track conflicts in early UE5).
- For a minimal editor, a **two-bone IK per limb with a pole/knee direction** is the one constraint that round-trips cleanly (Unity lists no limitations for it). It is a sound core for foot/hand fixing.

### Gaps
- I did not research UE "Animation Modifiers" (editor-time scripts run on anim sequences, e.g. for foot-sync markers).
- The exact version history of Sequencer layers (5.4 vs 5.5 vs later) is inconsistent across sources; Epic release notes were not opened.

---

## 7. Across tools: the minimal control set animators rely on vs niche features; common usability complaints

### Takeaway
Every tool converges on the same core: **scrub timeline → select body part → drag an IK effector or rotate an FK joint → pin the planted foot/hand → key (or edit on an additive layer with a weight) → retime a span**. That core is wrapped in a contact model (MotionBuilder floor contact, Rokoko foot-lock lane, Cascadeur fulcrums) and a key-reduction step (Cascadeur unbaking; MotionBuilder layers over dense data). Clip-assembly systems (Story, Time Editor, NLA), physics and retarget-chain tuning are secondary for single-clip game cleanup. The documented complaints are mostly about **mode/system boundaries** and **unreliable layering**, not about missing features.

### Cited Findings
- Core controls evidenced across tools:
  - Effectors, pins and keying modes: [MotionBuilder Controls tab](https://help.autodesk.com/cloudhelp/2025/ENU/MotionBuilder-Reference/files/MotionBuilder-Windows/Character-Controls/GUID-F286B854-A373-49F2-9DB3-1D788208026B.html).
  - Track weight and per-body-part masking: [OSU Story](https://www.asc.ohio-state.edu/berezina-blackburn.1/classes/mocap/story.html).
  - Additive/override layers with animatable weights: [Epic](https://www.unrealengine.com/try-free/animation-software-2?lang=de).
  - Interval-based editing and trajectories: [Cascadeur Trajectories](https://cascadeur.com/help/tools/animation_tools/trajectories).
  - Foot-contact timeline lane: [Rokoko Edit in Studio](https://docs.rokoko.com/rkk-vision-documentation/get-started/edit-in-rokoko-studio).
  - Per-part retiming via TimeWarp: [OSU Story](https://www.asc.ohio-state.edu/berezina-blackburn.1/classes/mocap/story.html).
- Foot sliding is the most common mocap artifact (vendor) — [MoCap Online Blender guide](https://mocaponline.com/blogs/mocap-news/blender-motion-capture-guide). Rokoko asks users to review foot contacts before any filter — [Rokoko 2.6.1](https://support.rokoko.com/hc/en-us/articles/47324364931089-Rokoko-Studio-2-6-1-with-MAJOR-Coil-Suit-Upgrade).
- Documented friction and complaints:
  - **Maya Time Editor ignores HIK keying modes**, and its Clip Layers can't share animation with Animation Layers — [Maya 2022 HIK in Time Editor](https://help.autodesk.com/cloudhelp/2022/ENU/Maya-Animation/files/GUID-A3D4A1E8-E353-4ADA-A7C2-F264DF083C39.htm); [Maya 2024 Clip Layers](https://help.autodesk.com/cloudhelp/2024/ENU/Maya-Animation/files/GUID-B3FFE1AA-F44D-4EC3-A570-316A3BF3DF88.htm).
  - **MotionBuilder Story clips must be plotted and deleted before the control rig can be keyed** — [OSU Story](https://www.asc.ohio-state.edu/berezina-blackburn.1/classes/mocap/story.html).
  - **Blender NLA additive strips distort or cancel animation** — [BlenderArtists](https://blenderartists.org/t/using-nla-to-correct-motion-capture-anims/1169389); [T50433](https://developer.blender.org/T50433).
  - **UE additive/override layer oddities** — [UE forum](https://forums.unrealengine.com/t/using-animation-layers-with-layered-control-rig/2268479); [UE forum 5.0.1](https://forums.unrealengine.com/t/sequencer-additive-track/553082).
  - **Rokoko: no foot sliding support** — [Rokoko Edit in Studio](https://docs.rokoko.com/rkk-vision-documentation/get-started/edit-in-rokoko-studio).
  - **Unity: no Humanoid support for motion transfer** — [Unity docs](https://docs.unity3d.com/Packages/com.unity.animation.rigging@1.3/manual/BidirectionalMotionTransfer.html).
  - **Dense baked curves are hard to edit** — [Blender 2011 proposal](https://wiki.blender.org/index.php/User:Aligorith/GSoC2011_Proposal_1AnimSysPolish); the motivation for [Cascadeur Unbaking](https://cascadeur.com/help/category/221).

### Inferences
- **Proposed essential set** for a minimal editor:
  1. Timeline with scrub, play/loop and range select.
  2. Body-part picking via a 2D figure panel plus viewport click.
  3. Move/rotate gizmo on IK effectors (hands, feet, hips) and FK rotate on spine/neck/head.
  4. A Body Part vs Full Body keying toggle.
  5. Pin translation/rotation per hand/foot, feet pinned by default.
  6. One additive offset layer per edit, with a weight and a body-part mask.
  7. A contact lane per foot (and optionally per hand).
  8. Key reduction ("unbake") at contacts.
  9. Span retime (stretch an interval).
  10. Root/hip height and drift offset.
  11. Undo.
- **Likely niche for this scope**: Story-style multi-clip assembly with Match and travel nodes, physics (AutoPhysics), auxiliary effectors/pivots, pin presets, toe/finger contact, quadruped contact types, clip ghosts, general retarget-chain tuning, and multiple layer types beyond additive.
- The best-supported complaint pattern is **having two systems for the same concept** (two layer types; a clip editor and a rig editor with different keying rules). One timeline and one layer stack, with every editor respecting the same mode, is the main design lesson.

### Gaps
- I found **no quantitative data** (surveys, telemetry) on which features animators use daily. The essential/niche split is an inference from documentation emphasis and workflow guides.
- I found no sourced complaints specifically about "too many panels" or **undo** behaviour in these tools. Those claims would need forum mining beyond this pass.
- No Reddit, 11 Second Club or CGSociety threads were captured.

---

## 8. Interaction details worth copying: selection on the 3D character, gizmos, shortcuts, how keys/pins/layers show on a timeline

### Takeaway
The most concrete, copyable interaction patterns are:
- **MotionBuilder's figure panel**: a 2D body diagram of effectors, where Shift-click grabs a whole limb and Ctrl-click toggles single effectors.
- **Pin toggle buttons** acting on the current effector selection.
- **Cascadeur's colour-coded trajectory**: dark keys, light in-betweens, and dragging a path point marks the interval Fixed.
- **Rokoko's per-foot coloured contact bars** on the timeline.
- **MotionBuilder Story's per-track body-part mask, animatable weight and ghost skeletons**.

### Cited Findings
- Figure-panel selection modifiers (click / Shift = whole body part / Ctrl = toggle / Shift+Ctrl = toggle body part; empty-space click = all in view; double-click = deselect). Pins are applied by selecting an effector cell and clicking Pin Translation / Pin Rotation. Release Children Pins is a temporary override. — [MotionBuilder 2025 Controls tab](https://help.autodesk.com/cloudhelp/2025/ENU/MotionBuilder-Reference/files/MotionBuilder-Windows/Character-Controls/GUID-F286B854-A373-49F2-9DB3-1D788208026B.html)
- Feet pinned by default on control-rig creation — [Maya 2017 Controls tab toolbar](https://help.autodesk.com/cloudhelp/2017/ENU/Maya/files/GUID-735DF85A-331C-4049-8C70-E53EFCDB4B73.htm) (snippet)
- Trajectory colours (darker green = key, brighter = interpolated frame). Interval selection on the Timeline drives which trajectory segment is editable. — [Cascadeur Trajectories](https://cascadeur.com/help/tools/animation_tools/trajectories) (snippet)
- Interval Edit Mode button lives **on the Timeline** — [Cascadeur help: Tools](https://cascadeur.com/help/tools) (snippet)
- Foot-lock keys on the timeline: green = left locked, blue = right locked. Unlocked gaps get an automatic jump arc. "Revert to original values" sits on the timeline. — [Rokoko Edit in Studio](https://docs.rokoko.com/rkk-vision-documentation/get-started/edit-in-rokoko-studio); [Rokoko 2.5.0](https://support.rokoko.com/hc/en-us/articles/39602279983633-Rokoko-Studio-2-5-0) (snippet)
- Story tracks expose a Figure icon to choose affected body parts, a "Full Body" checkbox and an animatable Weight. Ghosts show track/clip results as wireframe skeletons in the viewer. — [OSU Story](https://www.asc.ohio-state.edu/berezina-blackburn.1/classes/mocap/story.html); [MotionBuilder ghosts](https://help.autodesk.com/cloudhelp/2017/ENU/MotionBuilder/files/GUID-DB244A01-1C04-4A79-9C42-AAAD0B9C0A7C.htm)
- Floor-contact markers can be displayed in the viewport while tuning — [HumanIK SDK Contact Types](https://help.autodesk.com/cloudhelp/2016/ENU/HumanIK-SDK-Help/files/GUID-3955695A-D38E-485E-BADA-FEF73F9A3B4F.htm)
- In UE, lowering the camera Near Clipping Plane was needed to animate in tight spaces, a small viewport detail that mattered in practice — [Packt Ch.19 summary](https://www.packtpub.com/en-kr/product/unreal-engine-5-character-creation-animation-and-cinematics-23-9781801812443/chapter/chapter-19-motion-capture-editing-and-cleanup-using-control-rig-and-sequencer/summary)

### Inferences
- Suggested timeline layout for a minimal editor:
  - Top: one row of key diamonds for the active layer.
  - Below: a contact lane per foot as coloured bars (Rokoko-style), which double as pins.
  - Interval selection: click-drag on the ruler to select a span, then retime or set interpolation (Cascadeur-style).
  - Layer list: name, weight slider, mute, and body-part mask chips (Story-style).
- Selection: viewport click on the limb plus a fixed 2D figure panel with the ~10 effectors (hips, chest, head, 2 hands, 2 elbows-as-pole, 2 feet, 2 knees-as-pole). Shift-click selects the whole limb.
- Gizmo: standard move/rotate. The DCC-standard W/E/R move/rotate/scale hotkeys are common convention, but **I found no source for them in this pass**, so treat them as an assumption.
- Show a ghost (onion skin) of the original mocap under the edited pose. MotionBuilder's ghosts support this as a useful pattern for judging how far an edit departs from the capture.

### Gaps
- I did not source specific default hotkeys for MotionBuilder, Cascadeur or Rokoko (other than the unverified Maya Ctrl+W pin hotkey).
- I captured no screenshots or primary descriptions of Cascadeur's overall screen layout (viewport, timeline and panels).
- I found no source on undo granularity (per drag vs per key) in any tool.
