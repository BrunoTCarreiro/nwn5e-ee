# Mocap → shipped game animation: pipeline stages and editing-on-top-of-mocap techniques

> Research notes (2026-10-30). Scope: body animation for game characters, not facial. Source quality varies a lot.
> **Primary:** Autodesk MotionBuilder and Maya docs, Vicon Shogun docs, Blender manual, Jonathan Cooper's gameanim.com write-ups.
> **Secondary:** 80.lv, Game Developer, Rigging Dojo.
> **Vendor/marketing, treat as opinion:** mocaponline.com, mimicgaming, sunstrike, cada-edu, Blender add-on listings.
>
> Several findings came from search-result summaries rather than full page fetches. They are marked "(search summary)".
> The end user is a solo modder with webcam mocap and a home-made browser tool. Every section's Inferences say what is cheap to build and what needs heavy infrastructure.

## Q1. What are the standard stages from capture to a shipped game animation, who does each, and how long do they take?

### Takeaway
The vendor guides agree on the order: capture → marker cleanup (labelling, gap-fill) → solve to a skeleton → retarget to the game rig → cleanup on the skeleton (filtering, foot contacts) → animator polish on layers → cut into clips (cycles, start and end poses, root motion) → export. Their claims are opinion-level, but the Vicon and Autodesk docs back up the individual stages. The stages up to the solve, and most of the clip and retarget batch work, are technical work that can be partly automated. Ubisoft gave a whole GDC talk on automating them. Polish is artist work, and it is where "the bulk of an animator's work" goes. Reliable per-stage time figures are almost nonexistent. The only number found is a vendor estimate of about 2–4 hours of cleanup per minute of capture.

### Cited Findings
**Stage order and definitions**
- Vendor guides describe one consistent order: capture, solve, cleanup, retarget, animator polish, then engine integration. "Solving" turns raw marker or sensor data into joint rotations, followed by fixing gaps, jitter and foot sliding (search summary) — [cada-edu](https://cada-edu.com/guides/motion-capture), [mimicgaming](https://www.mimicgaming.com/post/video-game-animation-gameplay-cinematic-pipeline)

**Marker stage (Vicon Shogun Post)**
- **Order is fixed.** "Before filling gaps, you must find and fix any labeling swaps or errors in your data. Gap-filling in Shogun relies on the data before and after the gaps to predict marker location." (search summary) — [Vicon Shogun 1.9: Clean up data](https://vicon-help.atlassian.net/wiki/spaces/Shogun19/pages/13795574/Clean+up+data)
- **Check as you go.** Vicon advises verifying the solve after each cleanup range is finished, not only at the end. (search summary) — [Vicon Shogun 1.9](https://vicon-help.atlassian.net/wiki/spaces/Shogun19/pages/13795574/Clean+up+data)
- **Gap-fill is iterative and partly scriptable.** Automated fill may leave gaps. Those are filled by hand, or Max Deviation is raised and the fill re-run. Rigid gap fill can be scripted (`selectMarkersForRigidFill`, `autoFillGaps`), which allows batch processing. (search summary) — [Vicon Shogun 1.13](https://vicon-help.atlassian.net/wiki/spaces/Shogun113/pages/84216114)

**Retarget stage**
- The main problem is proportion mismatch. A large creature and a slim character cannot share one actor's motion without adjusting stride, arm reach and ground contact (search summary) — [cada-edu](https://cada-edu.com/guides/motion-capture)
- Rig mismatches are a common source of elbow pops and shoulder shears (search summary) — [sunstrike](https://sunstrikestudios.com/en/blog/motion_capture_for_games_and_film/)
- **Tools.** Rigging Dojo lists MotionBuilder HIK (HumanIK), Maya HIK, and custom-rig tools (mGear, MRS), with FBX between packages. It calls Maya's HIK "buggy in Maya and missing features but works well enough" and recommends Red9 for mocap work in Maya — [Rigging Dojo](https://www.riggingdojo.com/2021/02/01/animators-learning-motioncapture/)
- **Book coverage.** Cooper's *Game Anim* (2nd ed., 2021), Ch. 11 "Our Project: Motion Capture", has sections on mocap retargeting, shot lists, rehearsals, mocap previz and working with actors. The visible table of contents has no explicit cleanup or retiming headings — [kriso.ee TOC](https://www.kriso.ee/cgi-bin/shop/9781003147893_pe.html), [Routledge](https://www.routledge.co.uk/product/isbn/9780367707651)

**Game-specific cleanup and polish**
- Game cleanup filters subtle noise more aggressively than film cleanup, because that noise is invisible at gameplay camera distances (search summary, vendor) — [mocaponline tutorial](https://mocaponline.com/blogs/mocap-news/motion-capture-tutorial-guide)
- **Animator mindset.** Editing mocap is "deforming movement and art restoration vs. creating new things". The goal is "not losing what all the $$$ and time and work went into getting that performance" — [Rigging Dojo](https://www.riggingdojo.com/2021/02/01/animators-learning-motioncapture/)
- **Where the artist time goes.** "Motionbuilder's layers are where the bulk of an animators work is done to bring appeal to mocap." Asymmetry and acceleration are added later, in-engine — [Game Anim: Seamless Mocap Cycles (3D Artist #58, 2013)](https://www.gameanim.com/?p=5168)

**Clip stage at AAA: Ubisoft Montreal, partly automated**
- Dan Lowe's GDC 2016 Animation Bootcamp talk "The 'Animate' Button – Mocap Automation Techniques at Ubisoft Montreal" covered automated motion editing from Ubisoft Montreal's work on Far Cry — [YouTube](https://www.youtube.com/watch?v=eeWBlMJHR14), [mocap-clipper wiki](https://github.com/rBrenick/mocap-clipper/wiki)
- The open-source Mocap Clipper tool says its design was "very influenced by" that talk. Its workflow:
  1. Import mocap.
  2. Cut it into clips in Maya's Time Editor.
  3. Set each clip's start and end pose.
  4. Align the skeleton to the world origin.
  5. Use aim buttons to set the facing of a cycle's end position.
  6. Optionally run an "Adjustment Blend".
  7. "Bake to Rig".

  Pose application and rig transfer are left to the studio's Technical Animator or Technical Artist — [mocap-clipper wiki](https://github.com/rBrenick/mocap-clipper/wiki)

**Who does what**
- In the MotionBuilder cycle tutorial, the animator does both the clip and cycle work and the layer polish — [Game Anim](https://www.gameanim.com/?p=5168)
- Mocap Clipper assigns rig and pose transfer to Technical Animators/Artists — [mocap-clipper wiki](https://github.com/rBrenick/mocap-clipper/wiki)
- Rigging Dojo says animators should know these tools even if they mainly hand-animate — [Rigging Dojo](https://www.riggingdojo.com/2021/02/01/animators-learning-motioncapture/)

**Time figures (all weak)**
- **Vendor estimate.** A technical animator spends 2–4 hours cleaning one minute of production-quality capture (search summary, vendor, not measured) — [mocaponline animation guide](https://mocaponline.com/blogs/mocap-news/motion-capture-animation-guide)
- **Looping one cycle.** The core loop-blend technique "really only takes a few minutes once you get the hang of it", but making cycles final takes much more work — [Game Anim](https://www.gameanim.com/?p=5168)
- **Capture-to-edit density at Naughty Dog.** Uncharted (2007) captured at 120 fps. The default copy setting placed one key every three frames — [Game Anim: Uncharted Mocap (GDC 2008)](https://www.gameanim.com/2008/04/19/uncharted-mocap/)

### Inferences
**What the AAA division of labour looks like**
- Stages 1–4 (capture, marker cleanup, solve, retarget) are mostly technical work for mocap technicians, technical animators and batch scripts, with semi-automatic tools.
- Stages 5–6 (skeleton cleanup and polish) are where animators spend their time.
- Clip cutting, origin alignment, start/end poses and root motion are increasingly automated (Ubisoft, Mocap Clipper), with an animator choosing the frame ranges.

**What changes for webcam mocap**
- There is no marker stage. Labelling and gap filling become keypoint confidence, dropouts and per-frame jitter.
- The "solve" step is keypoints → joint rotations, for example MediaPipe → IK fit.
- For a solo modder, the expensive stages (stage, marker labelling, multi-actor solving) disappear. The skeleton-level stages remain, and they need more filtering and foot-contact work, because webcam depth is unreliable.

**Cheap pipeline for a browser tool**, in roughly this order:
1. Per-joint low-pass or One-Euro filter.
2. Automatic foot-contact detection and lock.
3. Root-motion extraction.
4. Clip trimming and loop blend.
5. Additive layer for polish.
6. Key reduction at export.

Each of these is a few hundred lines. None needs server infrastructure.

### Gaps
- No primary-source time breakdown per stage (technician hours vs animator hours) was found. The 2–4 h/min figure is a single vendor claim.
- I could not get the content of Dan Lowe's GDC 2016 talk itself (the Vault and video were not readable). Its specific automation methods are unconfirmed.
- Cooper's *Game Anim* mocap chapter text was not accessible. Only the table of contents was.
- No reliable source on the exact handoff points between mocap technicians and animators at named studios (Naughty Dog, Remedy, CDPR).

## Q2. How do animation layers (additive vs override) work over mocap, with weights and muting?

### Takeaway
The standard pattern keeps the mocap untouched on the base layer. Corrections and exaggerations go on one or more **additive** layers, whose values are added on top of what lies beneath. Spans of broken data go on **override** layers, which replace what lies beneath, with the layer weight animated so the fix blends in and back out to the capture. Weight scales a layer's contribution. Muting a layer shows the raw capture again, so the capture is never destroyed.

### Cited Findings
**Additive math**
- In Maya, Additive layers add their animation to the layers below for the same attributes. "If AnimLayer1 and AnimLayer2 both contain the translateX attribute for a sphere, the resulting translateX value for the sphere is the sum of the translateX values from both layers." — [Maya 2011 help: Animation layer modes](https://download.autodesk.com/us/maya/2011help/files/WS1a9193826455f5ff-6d855556117e9f816d2208c.htm), [Maya 2017](https://help.autodesk.com/cloudhelp/2017/ENU/Maya/files/GUID-BBCA0BC3-7608-4E86-8E9F-B4099C316156.htm)

**Override math**
- An Override layer replaces the animation of the layers beneath it that share attributes. With translateX 10 on one layer and 15 on another, both Override, "the sphere will have a translateX value of 15." — [Maya help](https://download.autodesk.com/us/maya/2011help/files/WS1a9193826455f5ff-6d855556117e9f816d2208c.htm)

**Weight and Passthrough**
- With Passthrough on, Weight 0.5 means half the layer's animation plays in the result, mixed with half from the layers below. Weight 0 means no contribution.
- With Passthrough off, the layer is opaque regardless of Weight.
- Attributes of the layer "are multiplied by the layer's Weight value". (search summary)

  — [Maya layer modes](https://help.autodesk.com/cloudhelp/2017/ENU/Maya/files/GUID-BBCA0BC3-7608-4E86-8E9F-B4099C316156.htm)

**Non-blendable attributes**
- Boolean and enum attributes (for example visibility) are always calculated in Override mode, even on Additive layers (search summary) — [Maya 2011 help](https://download.autodesk.com/us/maya/2011help/files/WS1a9193826455f5ff-6d855556117e9f816d2208c.htm)

**MotionBuilder specifics**
- MotionBuilder has two layer modes, Additive and Override, and Override has a Passthrough toggle — [MotionBuilder 2024: Animation layer modes](https://help.autodesk.com/cloudhelp/2024/ENU/MotionBuilder/files/GUID-C68A451F-FD65-4CFB-ACBC-C663B6F3A649.htm)
- Defaults and rotation accumulation (search summary):
  - New layers default to Additive.
  - Override layers default to Passthrough on with weight 100.
  - Rotation accumulation defaults to "Per Layer" (called Quaternion Rotation Accumulation in Maya), which differs from Maya's default.
  - Scale Additive accumulation is not supported.

  — [MotionBuilder 2022: Setting the animation layer modes](https://help.autodesk.com/cloudhelp/2022/ENU/MotionBuilder/files/GUID-84C2673F-828A-4D6B-8003-207B75860789.htm)

**Mocap practice: Ohio State course (search summary, informal and older)**
- Additive layers modify the captured motion. Override layers replace it.
- For Override fixes, animate the layer weight so it blends back to the original motion.
- Never switch a layer from Additive to Override after keying on it.
- Typical cleanup: run Gimbal Killer on rotation curves, fix bad spans on an Override layer, then cut the take into clips and blend them in the Story tool.

  — [Ohio State, Basic Prop mocap in MotionBuilder](https://www.asc.ohio-state.edu/berezina-blackburn.1/classes/mocap/basic_propsMOBU.html)

**Practitioner advice**
- "Animation layers are your friend and so is not being worried about baking everything down." — [Rigging Dojo](https://www.riggingdojo.com/2021/02/01/animators-learning-motioncapture/)
- Animation Mentor's MotionBuilder workflow: create a new Take, turn off Story Mode, work on the plotted (baked) animation, and use animation layers for non-destructive adjustment on top (search summary) — [Animation Mentor blog](https://www.animationmentor.com/?p=99261)

**Layers used as helpers**
- In the cycle tutorial, a temporary second layer moves the character backward linearly so a forward-travelling cycle can be viewed in place. That layer is removed before export.
- The polish example raises the shoulders and tilts the head on AnimLayer1 with AutoKey, using Reach R 100%.

  — [Game Anim](https://www.gameanim.com/?p=5168)

**Layers at Naughty Dog**
- "A layer manager was devised to allow for subdivisions in Maya's pre-existing visual layers." — [Game Anim: Uncharted Mocap](https://www.gameanim.com/2008/04/19/uncharted-mocap/)

**Blender**
- An action strip "plays the keyframes inside an action". Multiple strips can reference the same action.
- A strip can be shorter than its action ("through cropping, speeding up, or both") or longer ("through extending, slowing down, or both").

  — [Blender Manual: NLA Strips](https://docs.blender.org/manual/en/latest/editors/nla/strips.html)

### Inferences
**Minimal implementation, per joint, per frame**
- Base: `R_base(t)`, the dense mocap local rotation (quaternion), and `p_base(t)` for the root or hips translation.
- Additive layer `i` stores a sparse keyed *delta* `ΔR_i(t)` and `Δp_i(t)`, interpolated between its own keys and identity or zero elsewhere.
- Evaluate: `R = R_base · slerp(I, ΔR_1, w_1) · slerp(I, ΔR_2, w_2) …` and `p = p_base + Σ w_i·Δp_i`.
- Override layer: `R = slerp(R_below, R_layer, w)`. Animate `w` from 0→1→0 around the broken span.
- Mute: `w = 0`. Solo: evaluate base plus that layer only.
- Muting, solo and weights are trivial once the evaluator is a layer stack. This is **cheap**: a few dozen lines.

**Why additive layers suit mocap**
- The delta keys are sparse (a handful per correction), while the base stays dense.
- The capture can always be recovered, by muting or deleting the layer.

**Implementation choices**
- Local-space, per-joint quaternion multiplication matches MotionBuilder's "Per Layer" or quaternion accumulation. Adding Euler angles risks gimbal problems, which is why Gimbal Killer appears in the workflow.
- Make "edit on a layer" the only way to change mocap in the tool. Bake (flatten) the stack only at export.
- Animation booleans or enums (for example event flags) must be override-only.

**Unverified detail** (from prior knowledge of the Blender manual, not checked this session)
- NLA strips have blend types Replace/Combine/Add/Subtract/Multiply, Influence (animatable), Blend In/Out, Playback Scale, Repeat, and Animated Strip Time.
- "Combine" uses quaternion-multiply semantics for rotations.

### Gaps
- No Autodesk page found that states the Override-with-weight equation explicitly. `lerp`/`slerp(below, layer, weight)` is implied by the "0.5 = half and half" description, not stated.
- The Blender NLA sidebar page (Blend types, Influence, Strip Time) could not be extracted. The fetched pages returned only navigation.
- No studio-specific layer conventions found, for example a fixed "fix / exaggerate / contact" layer naming scheme.

## Q3. How are control rigs used on mocap: FK/IK switching, full-body IK effectors, pinning, contacts, "reach", and foot locking?

### Takeaway
In MotionBuilder and Maya HumanIK, mocap drives the skeleton (FK), and a control rig of IK **effectors** sits on top. Each effector has a translation and a rotation blend ("IK Blend T/R", historically "Reach T/R") that sets how much the limb follows the effector instead of the underlying FK or mocap pose. Full Body keying mode lets dragging one effector (hand, foot, hips) solve the whole body. **Pinning** holds an effector's translation and/or rotation while other parts are moved. Animating Reach or Blend to 100% over a frame range "holds" a hand or foot to a spot. Foot locking is the same idea applied automatically during planted frames. Feet are pinned by default, and MotionBuilder also has a floor-contact system with stiffness.

### Cited Findings
**Keying modes**
- **Full Body.** A moved effector pulls the whole character. Translating a wrist effector solves the entire body to reach it.
- **Body Part.** The solve and the keys are limited to one limb. A wrist move keys the wrist, elbow and shoulder.
- **Selection.** Only the chosen effector is affected, and the HumanIK Controls govern how much the rest of the body helps.

  (search summary) — [Maya 2023 HumanIK Keying Modes](https://help.autodesk.com/cloudhelp/2023/ENU/Maya-CharacterAnimation/files/GUID-6EB8580A-0344-42F1-A722-4FDFD5BFA70C.htm), [Maya 2017 Controls tab toolbar](https://help.autodesk.com/cloudhelp/2017/ENU/Maya/files/GUID-735DF85A-331C-4049-8C70-E53EFCDB4B73.htm)

**Pinning (search summary)**
- Translation pinning locks an effector's position. Rotation pinning locks its orientation, and each can be pinned independently.
- When a HumanIK Control rig is created, the foot effectors are pinned in translation and rotation **by default**.
- "Release All Pinning" temporarily releases pins on the selected effector and the effectors below it in the chain.

  — [Maya 2017 Controls tab toolbar](https://help.autodesk.com/cloudhelp/2017/ENU/Maya/files/GUID-735DF85A-331C-4049-8C70-E53EFCDB4B73.htm)
- Pins are soft constraints. The system holds the pinned value as closely as its limits allow rather than absolutely. Pinning the chest effector removes body twist (search summary, Japanese tutorial) — [zenn.dev](https://zenn.dev/guncys/articles/c4875549ec567a)

**IK Blend T/R and Pull (MotionBuilder)**
- Each effector cell shows "the amount of translation blend" (left half) and rotation blend (right half) "between the IK rig and the FK rig effectors".
- "Pull is applied over IK translation". If the effector does not blend toward IK, "the Pull slider has no affect."

  — [MotionBuilder 2024: Character representation](https://help.autodesk.com/cloudhelp/2024/ENU/MotionBuilder/files/GUID-66D7ED9D-D80E-4A2C-BFEE-AA4F819FD3DD.htm)
- FK effectors in a Control rig have no Reach properties of their own. They reference the Blend properties of the corresponding IK effector (search summary) — [MotionBuilder 2023: Reach feedback on FK effectors](https://help.autodesk.com/cloudhelp/2023/ENU/MotionBuilder/files/GUID-39BA0697-3379-4DE5-B3EF-E97942250058.htm)
- In retargeting, Reach attributes "define the translation and rotation reach between the target character and the source character", per effector (head, chest, elbows, wrists, hands, knees, ankles, feet) (search summary) — [Maya 2014 Getting Started](https://download.autodesk.com/us/maya/Maya_2014_GettingStarted/files/GUID-D3D76D87-F901-485A-B468-23EFBB9531BB.htm)

**Floor contact**
- MotionBuilder has a feet floor-contact setup. Contact markers show only in X-Ray display mode.
- "Feet Contact Stiffness" defines how stiff the foot becomes as soon as any part of it contacts the floor (search summary).

  — [MotionBuilder 2017: Defining foot floor contact](https://help.autodesk.com/cloudhelp/2017/ENU/MotionBuilder/files/GUID-5E37B306-7A43-42C3-82CC-E620F7020A71.htm), [Feet Floor Contact Setup properties](https://help.autodesk.com/cloudhelp/2017/ENU/MotionBuilder/files/GUID-D0730CDE-1FC4-45EC-AA40-02C7631FA256.htm)
- A commercial MotionBuilder editing course teaches "Understanding Animation Layers", "Understanding Pinning", "Walk Cycle Fix Foot-Floor Contact" (20:17) and "Walk Cycle Fix Foot Slide" as a sequence. This suggests the standard order is layers → pinning → contact fixes (search summary; lesson content not accessed) — [Mocappy's Academy course page](https://mocappys.teachable.com/p/motionbuilder-fundamentals-motion-editing-101)

**Foot sliding at indie scale**
- Foot sliding is described as the most visible mocap artifact. In Blender, add-ons such as Auto-Rig Pro include foot-locking utilities (search summary, vendor) — [mocaponline cleanup workflow](https://mocaponline.com/blogs/mocap-news/mocap-data-cleanup-workflow)

**Blender foot-lock add-ons (vendor claims, unverified)**
- **Foot Flow** "detects planted frames" and "Creates cleanup ranges around planted contacts and transitions". "Step Lock" "Locks planted frames to a stable reference so the foot stays in place while the body moves". Locking can be restricted per axis. Floor height can be set or detected from "the lowest visible foot geometry". "Fast Smooth" removes high-frequency vibration on planted feet. Results bake to the action, an NLA strip, or a new animation layer. No thresholds or algorithms are disclosed — [Foot Flow](https://foot-flow.pages.dev/)
- **Inverse Lock Bone** recalculates the *root* position so the foot stays planted, leaving the pose mostly unchanged. This is a root-offset approach rather than leg IK (search summary) — [Inverse Lock Bone FAQ](https://superhivemarket.com/products/inverse-lock-bone/faq)
- **Mocap Bone Lock** locks bones, especially feet, to prevent sliding or drift (search summary) — [Mocap Bone Lock](https://superhivemarket.com/products/mocap-bone-lock/docs)

**Lateral sliding from a loop blend**
- Fixed by hand-editing the foot controller's Translation X curve: "move the foot inward before it lands", without changing the first or last pose — [Game Anim](https://www.gameanim.com/?p=5168)
- Rigging Dojo names foot planting as the main case where deleting keys is acceptable — [Rigging Dojo](https://www.riggingdojo.com/2021/02/01/animators-learning-motioncapture/)

**FK manipulated like IK at Naughty Dog**
- Uncharted had an FK rig that could be manipulated as if it were IK, and a "visual arc tool" showing a point's trajectory with editable keyframed positions — [Game Anim: Uncharted Mocap](https://www.gameanim.com/2008/04/19/uncharted-mocap/)

### Inferences
**Minimal per-limb "reach" or pin**

| Step | What to do |
|---|---|
| 1 | Sample the mocap pose (FK). |
| 2 | Compute the limb end's world position `P_fk`. |
| 3 | Effector target `P_eff(t)` is a keyed world position (or the frozen contact point). |
| 4 | `P_goal = lerp(P_fk, P_eff, reachT(t))`. |
| 5 | Run analytic two-bone IK (thigh–shin–foot, upper arm–forearm–hand). Use the FK knee or elbow direction as the pole, so the capture's knee direction is kept. |
| 6 | Optionally slerp the end rotation toward the effector's by `reachR(t)`. |

- Animating `reachT` 0→1 over a few frames, holding it at 1, then 1→0 gives the "hold the hand on the table from frame 40 to 70" behaviour.
- **Cost: cheap.** Two-bone IK is closed-form, and blending is a lerp.

**Automatic foot lock**
1. **Detect contact** per foot. Use foot-joint (or toe) height below a threshold above the floor, and horizontal speed below a threshold. Add hysteresis and a minimum duration to avoid flicker.
2. **Pick the lock point.** For each contact interval, use the mean or median world position, with Y snapped to the floor.
3. **Lock.** Set `P_eff` to that point and ramp `reachT` up and down over about 2–4 frames at the interval edges.
4. **Solve** with two-bone IK.

- If the leg cannot reach (overextension), lower the hips slightly or let the lock slip. This is the "root offset" alternative used by Inverse Lock Bone.
- **Cost: cheap to moderate.** Most of the work is tuning thresholds for noisy webcam data.

**Full-body IK (drag the hips or one hand and the whole body follows)**
- This is a different scale of problem.
- A rough version is possible in the browser: FABRIK or CCD over the spine plus limbs, with pinned feet as constraints, iterated each drag.
- Matching HumanIK quality (balance, joint limits, soft pins, pull weights) is **heavy**.

**Cheap middle ground for a solo tool**
- Hips or COM drag with feet pinned. Translate the hips, then two-bone-IK both legs back to their pinned points.
- Per-limb reach with keyed weights.
- Additive-layer FK rotation tweaks.

### Gaps
- Autodesk's pages fetched here do not give the exact solver behaviour for intermediate Blend or Pull values, or how Pull distributes motion through the body.
- Whether the "Reach T/R" label (older versions and tutorials) is literally the same property as the current "IK Blend T/R" is not confirmed by a primary source. The 2013 tutorial uses "Reach R 100%", and the current docs use "IK Blend T/R".
- No published thresholds for foot-contact detection from any commercial tool. Foot Flow does not disclose them.

## Q4. How does key reduction turn dense per-frame mocap into a few editable poses, and when do animators do it?

### Takeaway
There are three families.
1. **Error-bounded curve reducers** (Maya Key Reducer and Simplify Curve, MotionBuilder's Key Reducer) drop keys while staying within a tolerance of the original curve.
2. **Naive every-nth-frame thinning** (Cascadeur's built-in nth-key reduction since 2025.2).
3. **Pose-aware "unbaking"** (Cascadeur 2024.1). Keyframes are placed at "fulcrum points", FK or IK interpolation is chosen per interval, and extra keys are added to stay close to the original.

AAA animators often keep dense data on the base layer and edit on sparse layers. Some studios deliberately reduce mocap to key poses so it can be hand-edited and matched to keyframed style. Naughty Dog did this on Uncharted.

### Cited Findings
**Maya filters (curve reducers)**
- **Key Reducer precision** is the error limit between the source and output curves. "Greater values reduce precision. Lower values increase precision."
- **Precision Mode** is "Absolute" (discards keys that don't match the error tolerance) or "Percentage" (discards keys within a percentage of the curve value).

  — [Maya 2023: Key Reducer Filter options](https://help.autodesk.com/cloudhelp/2023/ENU/Maya-Animation/files/GUID-63E2EA0D-90E7-45EC-9AF3-D3097A8BA5B9.htm)
- **Simplify Curve.**
  - Time Tolerance: "The amount (in seconds) that the timing for the keys is averaged… The higher the Time Tolerance, the sparser your keys and the less your resulting curve resembles the original curve." Default 0.05.
  - Value Tolerance (working units): default 0.01.
  - Autodesk says users "may have to experiment".

  — [Maya 2023: Simplify Curve options](https://help.autodesk.com/cloudhelp/2023/ENU/Maya-Animation/files/GUID-455F76C5-F761-4007-AFFE-E0F80BEFB627.htm)
- **Butterworth filter.** Rigging Dojo recommends Maya's Butterworth filters to fix curves while "not losing the shape or performance". This is a smoothing filter, distinct from key reduction — [Rigging Dojo](https://www.riggingdojo.com/2021/02/01/animators-learning-motioncapture/)

**Keeping fidelity on bake**
- The cycle tutorial bakes motion to the control rig with MotionBuilder's **Constant Key Reducer turned off** "to keep maximum fidelity". So reduction is a conscious choice at bake time — [Game Anim](https://www.gameanim.com/?p=5168)

**Cascadeur "Animation Unbaking" (2024.1)**
- Converts baked animation (mocap, store clips) into editable keys in three steps, run with one button or manually:
  1. Keyframes are set based on "Fulcrum Points", a preparation stage that makes sure "the Fulcrum intervals are determined correctly".
  2. It "choose[s] IK or FK for the intervals, and place[s] additional keyframes to keep the animation close to the original", and selects the interpolation.
  3. An AutoPosing adjustment: key points are locked, and AutoPosing smooths noise and jitter.
- Result: "a fully editable keyframe animation as if it was made from zero in Cascadeur". The article does not say how fulcrum points are detected.

  — [80.lv](https://80.lv/articles/transform-baked-animations-into-editable-keyframes-in-cascadeur-2024-1)

**Cascadeur nth-key reduction**
- A third-party plugin page says Cascadeur 2025.2 has a built-in nth-keyframe reduction command, which is why the plugin is no longer maintained.
- The plugin itself offered:
  - "Uniform" (keep every nth key)
  - "Use selected" (limit the reduction to a selected interval on the selected layers)
  - "Unflagged" (keep only flagged keys)

  Exact current Cascadeur menu paths are unconfirmed — [nemaron csc-key-reduction](https://nemaron.gumroad.com/l/csc-key-reduction)

**Caution on naive reduction**
- On the Cineversity forum, a user says deleting every nth key can work under time pressure but is not always a good idea, and the result should be checked against the original (search summary) — [Cineversity forum](https://cineversity.forums.maxon.net/post/3569)

**AAA example of deliberate reduction (Naughty Dog, Uncharted)**
- Mocap looked mismatched beside hand-keyed animation, so "they willfully lowered the mocap quality by reducing keyframes, sometimes to only the key poses from an action."
- Default copy density was one key every three frames from 120 fps capture.

  — [Game Anim: Uncharted Mocap](https://www.gameanim.com/2008/04/19/uncharted-mocap/)

### Inferences
**Three cheap algorithms**
- **(a) Greedy error-bounded insertion**, per channel or per joint.
  1. Start with the first and last frame as keys.
  2. Interpolate between them and find the frame with the maximum error against the dense data.
  3. If the error is above tolerance, insert that frame as a key and recurse.

  This is Ramer–Douglas–Peucker on the time axis. Use quaternion angle error for rotations and positional error at the joint end for translations. It is very cheap and gives Maya-Key-Reducer-like behaviour.
- **(b) Shared pose keys.** Run the same idea on a whole-body pose-distance metric (sum of per-joint angle errors, or world-space joint position error). The result is keys at the same frames for all joints. That is far easier to edit as "poses" than per-curve keys at different frames, and it approximates "reduce to key poses" or Cascadeur's fulcrum idea.
- **(c) Extremes-first seeding.** Seed keys at local extrema of joint velocity (velocity near zero = extreme poses, contacts), then run (a) to meet tolerance.

**When to reduce**
- Keep dense mocap as the base layer and put edits on sparse layers. This is the non-destructive approach.
- Reduce to key poses only when you want to restyle heavily, or for export size and format limits.

**NWN-specific considerations**
- For a game format that stores per-node keys, reduction at *export* also cuts file size.
- Pick tangents or interpolation that match the engine. If the engine interpolates linearly, the reducer must measure error against linear interpolation, not Bézier.

### Gaps
- MotionBuilder's own Key Reducer filter parameters were not found in primary docs this session. Only Maya's were.
- Cascadeur's fulcrum-point detection algorithm is not documented publicly in the sources found.
- No data on what fraction of keys AAA studios typically keep after reduction.

## Q5. How is mocap retimed: time warp curves, speeding up anticipation, holding impact frames, removing dead time, and how does retiming interact with layers?

### Takeaway
Retiming is done with a **time-remap curve** that maps output frame to source frame:
- Maya: the Time Editor Time Warp, the `timeWarp` command, or a scene `animCurveTT`.
- MotionBuilder: Story-tool clips.
- Blender: NLA strip scale and strip time.

A warp keeps the clip length fixed and redistributes time, so speeding one part slows another. A **speed curve** changes the clip's duration instead. Linear clip scaling is the simplest case.

Animators use retiming to snap into extremes, add "punch", remove pauses and dead time, and hold impact frames. Naughty Dog's Uncharted tools included a time-warp curve for exactly this. How layers keyed on top behave under a warp is not documented in the sources found. The safe practice, and what bake-and-remove forum advice implies, is to retime first, bake, then layer, or to key layers in output time.

### Cited Findings
**Maya Time Editor: three retime mechanisms**
- **Clip scaling** is a linear speed change. "shrink a clip on a track to half its size and it plays at double the speed."
- **Time Warp.**
  - Remaps which input frame plays at each output frame, shown as a diagonal line where "the horizontal axis of the curve represents output frames of the clip".
  - Clip length is unchanged. "If you speed up one part, it will slow down another part because it is frame to frame mapping."
  - Example: a warp can make key 17 play at frame 50.
  - Time Warp can make a clip play backward.
- **Speed Curve.**
  - Sets per-section playback speed (a flat line at 1 means normal speed).
  - "Changing the speed curve changes the duration of the clip." Forward only.
- **Choosing between them.** Time Warp is for precision and Speed Curve for general changes. They are mutually exclusive. Converting back and forth is discouraged because it creates many keyframes.

  — [Maya 2024: Retiming animation in the Time Editor](https://help.autodesk.com/cloudhelp/2024/ENU/Maya-Animation/files/GUID-164A97B6-D316-4D3F-AF6B-E7BAE8F3AE11.htm)

**Maya `timeWarp` command and curve editing**
- The command creates a time-warp input on a set of animation curves. Warp frames can be added, moved or deleted, and each span's interpolation set to linear, easeIn or easeOut. Example: `cmds.timeWarp('pCylinder1','pSphere1', f=[1,20])` (search summary) — [Maya timeWarp command](https://help.autodesk.com/cloudhelp/2016/ENU/Maya-Tech-Docs/Commands/timeWarp.html)
- The time-warp curve is edited in the Graph Editor with the Insert Keys and Move tools. Reset restores it and Delete removes it (search summary) — [Maya 2017: Create and Edit Time Warps](https://help.autodesk.com/cloudhelp/2017/ENU/Maya/files/GUID-B4797F65-EDB9-4AF0-BFF0-3F1A1297FD14.htm)

**Bake-and-remove in practice (user forum)**
- No built-in bake-and-remove option was confirmed. One reply recommends baking every frame to preserve tangents. Another gives a manual `animCurveTT` method, confirmed only on Maya 2018 (search summary) — [Autodesk forum](https://forums.autodesk.com/t5/maya-animation-and-rigging-forum/retiming-a-whole-scene-then-removing-the-retime-curve/m-p/9917985)

**MotionBuilder and Blender**
- MotionBuilder's Story tool is "the closest equivalent" to Maya's Time Editor, and "has the idea of Takes, layers and clip editing" — [Rigging Dojo](https://www.riggingdojo.com/2021/02/01/animators-learning-motioncapture/)
- Maya's Time Editor is described as a way to "blend, edit and time warp animations in a non-destructive and non-linear" way. Blender's NLA supports clip-based editing — [Rigging Dojo](https://www.riggingdojo.com/2021/02/01/animators-learning-motioncapture/)
- Blender NLA strips can be shorter or longer than their action by "cropping, speeding up" or "extending, slowing down" — [Blender Manual: NLA Strips](https://docs.blender.org/manual/en/latest/editors/nla/strips.html)

**AAA use (Naughty Dog, Uncharted)**
- A time-warp curve let animators adjust a whole animation's timing at once.
- Using video reference (the "Poor Man's mocap"), animators would "accentuate poses, add punch to timing, snap extreme poses, and remove unnecessary pauses".

  — [Game Anim: Uncharted Mocap](https://www.gameanim.com/2008/04/19/uncharted-mocap/)

**Combat timing rules of thumb (vendor and school blogs, not standards)**
- Hold the peak impact pose about 2–3 frames — [mocaponline creature guide](https://mocaponline.com/blogs/mocap-news/creature-animation-games-guide)
- Add a short hit-stop of 1–3 frames on connection — [mocaponline combat guide](https://mocaponline.com/blogs/mocap-news/combat-animation-game-dev-guide)
- Move the hit frame earlier while keeping a longer follow-through — [sunstrike timing](https://sunstrikestudios.com/en/blog/timing_in_animation/)
- Anticipation on responsive actions about 2–4 frames — [mocaponline platformer guide](https://mocaponline.com/blogs/mocap-news/platformer-animation-guide)
- Held anticipation on a player character can feel like input lag — [Artemisia College](https://www.artemisiacollege.com/feeds/blog/fighting-game-animation)
- Hand-keying is better than mocap for very fast impacts, where mocap can smear — [mocaponline combat guide](https://mocaponline.com/blogs/mocap-news/combat-animation-game-dev-guide)

**Blend-driven transitions (God of War 2018)**
- Kristjan Zadziuk's analysis says Kratos' transition into the defence stance uses a blend rather than many authored transitions: "it looks like it's about a three- to five-frame full-body blend." — [Game Developer](https://gamedeveloper.com/production/creating-fluid-combat-animations-for-the-new-i-god-of-war-i-)

### Inferences
**Minimal time-remap implementation**
- Store a monotonic remap `s = W(t)` (output frame t → source time s) as a few keys (t_k, s_k).
- Interpolate with piecewise-linear or monotone-cubic (Fritsch–Carlson) interpolation, so it never runs backward unless that is intended.
- Evaluate the base pose at fractional `s` by slerping between neighbouring dense frames.

**The edits as warp operations**
- **Remove dead time.** Key `W` so a span of source frames maps to fewer output frames, or cut the range outright.
- **Speed up anticipation.** Compress the source frames before the extreme into fewer output frames.
- **Hold an impact (hit-stop).** A flat segment in `W`: several output frames sample the same `s`.
- **Ease into an extreme.** Slope below 1 just before it.

**Speed curve versus warp**
- A speed curve is `W(t) = ∫ speed(t) dt`. It changes duration.
- A warp with fixed end keys keeps duration. Offer whichever is easier to reason about in the UI.

**Layer interaction (design choice for the tool)**
- **Option A: layers keyed in source time** (evaluated at `s`). Fixes follow the motion when retimed, and an impact correction stays on the impact. This suits corrections like contact fixes and pose fixes.
- **Option B: layers keyed in output time** (evaluated at `t`). Exaggeration and holds stay where you see them on the final timeline. This suits final polish.
- Simplest workable rule: corrections in source time, polish layers in output time. Or the AAA-style serial workflow: retime → bake → polish layers.

**Cost**
- Cheap. A remap curve plus fractional sampling.
- Foot locks must be computed *after* retiming, or remapped with the curve, or contacts will drift.

### Gaps
- No primary source on how MotionBuilder's Story-tool time warps behave with animation layers, or which order (warp then layer, or layer then warp) Autodesk evaluates.
- I could not verify Blender's "Animated Strip Time" semantics from the manual this session.
- No primary AAA source (GDC talk text) on hit-stop being authored in the animation versus done at runtime. The frame counts above are vendor rules of thumb. My understanding (unsourced) is that hit-stop is commonly a runtime game-speed freeze rather than baked frames.

## Q6. How do animators exaggerate extremes and polish mocap, and what is the typical ratio of mocap to hand-keyed work in AAA vs indie?

### Takeaway
Polish means pushing key poses (anticipation, contact or impact, recovery) on additive layers. Shapes and silhouettes are adjusted (shoulders, head tilt, spine arcs), timing gets "punch" with snapped extremes and removed pauses, and arcs are checked. Real actors are physically limited, so exaggeration is keyed on top. For stylised characters or non-humans, mocap may be only blocking or reference.

Ratios are rarely published. The one solid data point is Uncharted (2007): about 40% mocap and 60% keyframe overall. God of War (2018) mixed both per character: Kratos and Baldur were hybrid, while Atreus and creatures were keyed. No reliable indie ratio was found.

### Cited Findings
**Naughty Dog, Uncharted (GDC 2008 write-up)**
- **Ratio.** "The entire project ended up being around 40% mocap vs 60% keyframe." Capture was done at House of Moves.
- **Exaggeration.** Animators keyed over mocap to add exaggeration, because real actors are limited by physics.
- **Video reference.** Animators used "Poor Man's mocap" (video reference) to accentuate poses, add punch to timing, snap extreme poses and remove unnecessary pauses.
- **Tools.**
  - A pose and animation library (iToons) for pasting body and hand poses.
  - A visual arc tool.
  - A time-warp curve.
- **Runtime techniques.** "Partials" (independently animated bone sets) for reloads, additive animation, and about 150 cover poses varied by health or duress.

  — [Game Anim: Uncharted Mocap](https://www.gameanim.com/2008/04/19/uncharted-mocap/)

**God of War (2018)**
- **Per-character split.** An animator's reel labels Kratos and Baldur shots as a hybrid of mocap and keyframe, and Atreus and the dragon as keyframed. This is a single animator's breakdown, not an official figure — [Fabian Johnston reel](https://vimeo.com/275152806)
- **Combat performer.** Stuntman Eric Jacobus performed the combat motion capture for Kratos, Baldur and others — [Eric Jacobus](https://ericjacobus.com/2018/04/09/how-to-fight-like-kratos-with-eric-jacobus/)
- **Animators in suits.** At GDC 2019, animation director Bruno Velazquez said animators wore mocap suits to sketch scenes and combat moves. This also helped with things that could not be captured, such as non-humanoid dragons and the World Serpent — [GameRevolution GDC coverage](https://www.gamerevolution.com/features/513433-god-of-war-mature-kratos-sony-santa-monica)

**Polish examples from the cycle tutorial**
- A final pass example: raise the shoulders and tilt the head down on frame 1 on AnimLayer1.
- Layers are "where the bulk of an animators work is done to bring appeal to mocap". Asymmetry and acceleration are added later, in-engine.

  — [Game Anim](https://www.gameanim.com/?p=5168)

**Restoration versus replacement**
- Editing restores the performance rather than replacing it ("art restoration vs. creating new things") — [Rigging Dojo](https://www.riggingdojo.com/2021/02/01/animators-learning-motioncapture/)
- An iAnimate GDC event (2013, with Colin Graham, David Lam, Darryl Purdy and Michael Carr) is cited as showing studios "gone from hating mocap to embracing it". Also referenced: the GDC "Animation Bootcamp: Tricks of the Trade" panel (Amy Drobeck, Jean-Denis Haas, Billy Harper, Jay Hosfelt, Alex M. Lehmann, Simon Unger), and a Richard Lico talk on using mocap-style editing to speed up hand animation. The talks' contents were not accessed — [Rigging Dojo](https://www.riggingdojo.com/2021/02/01/animators-learning-motioncapture/)

**Vendor and school guidance on the general pattern**
- Mocap for natural base moves, keyframes for stylised attacks or character-specific shape — [animost](https://animost.com/tutorials/using-mocap-data-for-game-animation/), [mocaponline creature guide](https://mocaponline.com/blogs/mocap-news/creature-animation-games-guide)
- Exaggerated poses may go beyond anatomical limits so they read at full speed. Check deformed frames against hit and hurt volumes — [Artemisia College](https://www.artemisiacollege.com/feeds/blog/fighting-game-animation)
- Mocap is harder to push into stylised motion — [RMCAD](https://www.rmcad.edu/blog/motion-capture-vs-keyframe-animation-choosing-the-right-approach/)

### Inferences
**Minimal "push the extremes" tool**
1. Auto-detect candidate extremes from the dense data. Use frames where hand, foot or hip speed is at a local minimum, or where a joint's angular velocity changes sign. Contacts come from the foot-lock detector.
2. Let the user select one and add an additive layer key there (a rotation delta on spine, shoulders, head, arms), with zero keys some frames either side so the push eases in and out.
3. Add a global "exaggerate" slider. It scales each joint's deviation from a smoothed version of the motion (`R = R_smooth · (R_smooth⁻¹·R_base)^k` with k>1). This amplifies extremes automatically. It is cheap but crude, and needs per-joint limits.

- The Naughty Dog list (accentuate poses, snap extremes, remove pauses) maps onto three already-cheap primitives: an additive-layer key at the extreme, a time-warp segment that shortens the approach, and a warp segment that removes the pause.

**AAA versus indie**
- AAA has dedicated stages, technicians and HIK-class full-body IK, and retimes in Story or Time Editor with large layer stacks.
- Solo or indie practice, as described by add-on vendors and tutorials, leans on automation: foot-lock add-ons, Cascadeur unbaking or AutoPosing, nth-key reduction. It does less hand polish.
- For webcam mocap the base is noisier, so the balance shifts toward more filtering and contact cleanup, and toward keyframed overrides for fast or impact moments, which webcam capture smears.

**Heavy-infrastructure items to avoid or defer in a home-made browser tool**
- Full-body IK with balance.
- Physics-based AutoPosing (Cascadeur-style).
- Motion matching.
- Learned retargeting.

**Cheap, high-value items**
- Additive and override layers with weights and mute.
- Two-bone-IK reach and foot lock.
- Error-bounded key reduction.
- Time-remap curve.
- Loop blend.
- Root-motion bake.

### Gaps
- No primary-source mocap/keyframe ratio for any AAA game in the last ~10 years: God of War (2018) and Ragnarök, TLOU Part II, Control/Alan Wake 2, Cyberpunk 2077. Only Uncharted (2007) has a stated figure.
- No reliable source on indie ratios or indie polish time.
- No primary GDC transcript on the layer-by-layer exaggeration process (which joints, which layers, how many layers) at Naughty Dog, Ubisoft, Santa Monica, Remedy or CD Projekt. The Rigging Dojo-referenced talks were not accessible.

### Supplementary: loop making and root motion (stage detail referenced in Q1)
These two stages were asked about as part of the pipeline, and they matter for a game export.

**Seamless cycles in MotionBuilder (Game Anim / 3D Artist #58)**
- **Capture.** Direct actors with goals ("you are chasing after a stolen purse"), not speeds. Run diagonally across the volume for more steps per take.
- **Choose the range.** At least 3 steady steps, starting and ending on a passing pose.
- **Bake.** Bake to the control rig with Constant Key Reducer off.
- **Align.** Zero the root translation on frame 1 and rotate the travel path onto Z.
- **Find the join.** Find the two most similar *complete footsteps*, not single poses.
- **Blend onto itself.** Paste a copy of the clip after itself and drag it back so the blend happens "BEFORE the starting footstep". The cycle then ends on the start pose with matching momentum.
- **Blend length.** Longer blends are smoother but slide the feet more. Shorter blends are stable but the seam shows.
- **Root keys.** Keep the root's forward keys, or the character drifts unnaturally. View the cycle in place with a temporary counter-motion layer.
- **Lateral slide.** Fix it on the foot controller's X curve.

  — [Game Anim](https://www.gameanim.com/?p=5168)

**Blender (PureMotion add-on docs, vendor)**
- Bake movement onto a root bone so the character stays in place, using the hips or pelvis as the source. Add a root bone and bake in-place before exporting to Unreal.
- A loop finder scores candidate matches (lower is better), followed by a loop blend with a default of 10 blend frames.

  — [PureMotion docs](https://superhivemarket.com/products/puremotion/docs)

**Inference: minimal loop and root-motion implementation**
- **Loop.**
  1. Compute a pose-distance matrix over the chosen range: joint positions in a hip-relative, heading-aligned frame, plus velocities.
  2. Pick the frame pair (i, j) with the lowest distance and roughly one or more full gait cycles apart.
  3. Crossfade the last N frames toward the motion leading into frame i, in root-relative space, using slerp on rotations and a smoothstep weight.
  4. Re-apply the foot lock afterwards.
- **Root motion.**
  1. Project the hips onto the ground plane and smooth it to get the root trajectory, plus a smoothed heading yaw.
  2. Write that to the root bone and subtract it from the hips.
  3. For in-place export, drop the root channel. For root-motion export, keep it.
- **Cost.** Both are cheap.
- **Unknown for NWN.** Whether the target engine (NWN:EE models) wants in-place or root-driven locomotion was not checked here.
