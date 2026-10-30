# Edit Webcam Mocap on Layers, Not Frames

The modder wants to retime, place body parts with IK and key specific poses. Those are the right three edits. Professional game animators make exactly these edits on top of motion capture, and no studio ships captured timing as-is: the Assassin's Creed III team sped every take up 15% before editing started ([gameanim.com](https://www.gameanim.com/2014/02/11/animating-3rd-assassin/7/)). Professionals work faster because of how the work is structured, not because they have more features. The capture stays untouched on a base layer, and a mostly automatic cleanup pass runs first in a fixed order. Every artist change then lives on a layer that never overwrites the capture: additive pose changes with weights and body-part masks, IK targets whose "reach" weight is keyed in and out around contacts, and a time-remap curve anchored to a marked contact frame. For this capture rig, the binding constraint comes before the editor. At 13–27 fps the footage is below every documented minimum for fast motion: Pose2Sim asks for at least 60 fps, and Move AI uses 100–240 fps for fast limbs. A zero-phase low-pass filter cannot usefully cut above about a quarter of the frame rate (3.75 Hz at 15 fps). Until August 2026, FreeMoCap's Butterworth filter also assumed a 30 fps clock, so a 15 fps take was smoothed at 3 Hz instead of 6, which is enough to flatten a strike ([FreeMoCap #849](https://github.com/freemocap/freemocap/issues/849)). A peak that fell between frames cannot be interpolated back. The tool should therefore expect the artist to re-pose strike peaks, as Naughty Dog did when it cut mocap down to key poses to match its keyframed style ([gameanim.com](https://www.gameanim.com/2008/04/19/uncharted-mocap/)). Everything essential is closed-form math that runs comfortably in a browser: two-bone IK, slerp-blended layers, a monotone time remap, greedy key reduction and contact detection. Full-body IK with balance, physics-based posing and learned retargeting are the expensive parts, and they can be skipped.

## Animators spend their hours after the solve, not inside it

Vendor guides agree on the order of a professional pipeline, and the Vicon and Autodesk docs confirm the individual stages ([cada-edu](https://cada-edu.com/guides/motion-capture); [mimicgaming](https://www.mimicgaming.com/post/video-game-animation-gameplay-cinematic-pipeline)):

1. Capture.
2. Marker cleanup: labelling, then gap filling.
3. Solve to a skeleton.
4. Retarget to the game rig.
5. Skeleton cleanup: filtering and foot contacts.
6. Animator polish on layers.
7. Cut into game clips (cycles, start and end poses, root motion) and export.

The order inside a stage is strict too. Vicon says to fix **labelling swaps before gap filling**, because the gap filler predicts from the data on either side. It also says to check the solve after each cleanup range, not only at the end ([Vicon Shogun](https://vicon-help.atlassian.net/wiki/spaces/Shogun19/pages/13795574/Clean+up+data)).

Everything up to and including the retarget is technical work, and studios push it toward automation. Dan Lowe's GDC 2016 talk on Ubisoft Montreal's "Animate button" covered automated mocap editing on Far Cry ([YouTube](https://www.youtube.com/watch?v=eeWBlMJHR14)). The open-source Mocap Clipper, inspired by that talk, reduces clip work to a few steps: cut ranges, set start and end poses, align to the origin, aim the end facing, and bake to the rig. Rig transfer is left to technical animators ([mocap-clipper wiki](https://github.com/rBrenick/mocap-clipper/wiki)).

The artist hours come after that. Jonathan Cooper's cycle tutorial says plainly that **"Motionbuilder's layers are where the bulk of an animators work is done to bring appeal to mocap"** ([Game Anim](https://www.gameanim.com/?p=5168)). Rigging Dojo calls the job "deforming movement and art restoration vs. creating new things": the aim is to protect the performance, not replace it ([Rigging Dojo](https://www.riggingdojo.com/2021/02/01/animators-learning-motioncapture/)). Reliable time figures barely exist. The only one found is a vendor's estimate of **2–4 hours of cleanup per minute** of production capture ([MoCap Online](https://mocaponline.com/blogs/mocap-news/motion-capture-animation-guide)). The only published mocap-to-keyframe ratio is Uncharted (2007), at **about 40% mocap and 60% keyframe** ([gameanim.com](https://www.gameanim.com/2008/04/19/uncharted-mocap/)).

Webcam capture changes the first half of this pipeline but not the second.

- **No markers.** There is nothing to label. Labelling and gap filling become keypoint confidence, dropouts and left/right (L/R) swaps.
- **A different solve.** The "solve" becomes fitting a fixed-length skeleton to triangulated keypoints and deriving rotations from it.
- **Heavier skeleton cleanup.** The expensive studio stages disappear. The skeleton-level stages stay and get more work, because webcam depth and foot keypoints are unreliable.

The table maps each stage to what the tool should do.

| Stage | Professional practice | Who does it | Equivalent in the tool (webcam/FreeMoCap) |
|---|---|---|---|
| Capture | Optical or inertial stage, shot lists, rehearsals | Crew | 2–3 webcams. Frame rate and exposure are the biggest quality levers |
| Marker cleanup | Label, then gap-fill, check per range | Technician, partly scripted | Confidence gating, outlier rejection, gap fill, L/R swap fix. Automatic, with a few thresholds |
| Solve | Markers → skeleton rotations | Automatic | Fixed-length skeleton fit. Positions are filtered before rotations are derived |
| Retarget | HumanIK or IK Retargeter: rotation copy plus IK | Technical animator | Rotation copy with rest-pose offsets, root scaled by hip height, IK at contacts |
| Skeleton cleanup | Filters, foot contacts, floor | Mixed | Contact detection with an editable contact lane; floor fit |
| Polish | Additive/override layers, retiming, pushed poses | **Artist: most of the hours** | Layer stack, reach targets, time warp, pose keys |
| Clip cut and export | Cycles, start/end pose, root motion, key reduction | Increasingly automated | Loop finder, stance endpoints, root returns to origin, 30 fps bake |

The design implication is a strict one-way boundary. An automatic cleanup pipeline produces the base layer, and nothing the artist does ever writes back into it.

## Thirteen to 27 fps is below every documented minimum for fast motion

### Jitter is easy to fix; lost peaks and wrong contacts do the real damage

Markerless output is dominated by jitter, not by position error.

- **Jitter dominates.** In the SmoothNet benchmark, a raw 3D pose estimator on Human3.6M had an acceleration error of **19.17**, against 1.03–3.80 after smoothing. Mean joint position error barely changed (54.55 mm → 52.7–55.2 mm) ([SmoothNet](https://ar5iv.labs.arxiv.org/html/2112.13715)).
- **Bursts and flips.** The same paper finds slight jitter on most frames, and rarer long bursts that come with large biased errors. It also reports sudden flips when poses are stored as quaternions or angle-axis.

Beyond jitter, the documented failures cluster where combat happens:

- **Deep flexion.** Pose2Sim treats hip and knee angles beyond **90°** as "imprecise" and drops those frames when computing segment lengths. Chambered kicks and deep stances live in that range ([Pose2Sim config](https://raw.githubusercontent.com/perfanalytics/pose2sim/main/Pose2Sim/Demo_SinglePerson/Config.toml)).
- **L/R swaps.** Pose2Sim's swap handling is listed as "Not implemented yet" ([Pose2Sim](https://github.com/perfanalytics/pose2sim)).
- **Foot locks on fast legs.** DeepMotion ships a separate "Grounding" foot mode for when its Auto mode "locks the feet too long during fast leg movements". That is exactly the kick case ([DeepMotion](https://deepmotion.com/article/foot-locking)).
- **Penetration.** DeepMotion's Physics Filter exists because mocap "can occasionally result in joint penetration or crossovers" ([DeepMotion](https://deepmotion.com/article/physics-filter)).

Ranked for kicks, punches and shoves (my synthesis, not a measured result):

1. Blur and dropout on the striking wrist or ankle at full extension.
2. Peak loss caused by filtering.
3. Foot-lock misfires on pivots and kick chambers.
4. L/R swaps in spins and side-on stances.
5. Errors in deep flexion.
6. Penetration.

Steady-state jitter is the easiest artifact to fix, and it matters least in a fast clip.

### Raising the frame rate is the cheapest fix, and it happens before the editor

Every source ties fast-motion accuracy to frame rate.

- **Pose2Sim:** accuracy drops below **60 fps**, "especially for fast motion" ([Pose2Sim](https://github.com/perfanalytics/pose2sim)).
- **Move AI:** uses variable frame rates of 100–240 fps for "rapid limb motion, jumps". Shutter speed should be at least twice the frame rate and ISO as low as possible. It calls lighting "the single most important variable" ([Move AI](https://docs.move.ai/knowledge/zcam-e2-m4-mark-ii-camera-configuration-guide)).
- **FreeMoCap:** its docs ask for enough light to allow short exposures, which cuts motion blur ([FreeMoCap docs](https://freemocap.github.io/documentation/resources)).

For a 2–3 webcam rig, this argues for three things: lock exposure manually, light hard, and lower resolution or camera count until every camera holds a steady 30 fps or more. That is probably worth more than any post-process.

Setup and performance guidance from the same sources:

- **Camera layout.** Pose2Sim's two-camera layout is one camera in front and one at 45° to the side, both at hip height. It warns against top-down views, and recommends a sharp vertical movement to sync the cameras ([Pose2Sim](https://github.com/perfanalytics/pose2sim)).
- **Speed.** Perform at full speed rather than filming slow motion. QuickMagic reviewers found slow-motion footage tracks worse ([CGDive](https://cgdive.com/quickmagic-review-high-quality-ai-motion-capture/)).
- **Still pose at both ends.** Start and end each take with a second or two of standing still. That gives clean frames for bone lengths, floor height and drift alignment. Rokoko's Drift Fix similarly works best when a take starts and ends on the same spot ([Rokoko](https://support.rokoko.com/hc/en-us/articles/41620261484945-Rokoko-Studio-Update-Real-time-Locomotion-Drift-Fix)).

### Filter offline with zero phase, per joint group, and protect strike windows

The biomechanics default is a **4th-order zero-lag Butterworth** filter (a 2nd-order filter run forward and then backward) at 6 Hz. Pose2Sim's config suggests 3–6 Hz for walking and 6–15 Hz for running. Its README asks for 15 Hz or more for "very fast or impulsive movement" ([Pose2Sim config](https://raw.githubusercontent.com/perfanalytics/pose2sim/main/Pose2Sim/Demo_SinglePerson/Config.toml); [Pose2Sim](https://github.com/perfanalytics/pose2sim)).

The Clinical Gait Analysis FAQ adds three rules that bind hard at webcam rates ([Clinical Gait Analysis](https://clinicalgaitanalysis.com/faq/cutoff.html)):

- The filter "behaves strangely" once the cutoff passes **0.25 × the sampling rate**.
- The forward-and-backward design needs a cutoff correction factor of 1/0.802.
- Different body segments need different cutoffs: about 1–3 Hz for the trunk and 8–15 Hz for the feet.

At 15 fps the cutoff ceiling is 3.75 Hz, and at 30 fps it is 7.5 Hz. Below 60 fps, the "fast movement" band is simply out of reach.

The FreeMoCap bug makes this concrete ([FreeMoCap #849](https://github.com/freemocap/freemocap/issues/849)):

- **The bug.** Its post-processing Butterworth filter (6 Hz, order 4) hard-coded a 30 Hz sampling rate. The effective cutoff therefore scaled with the true frame rate: **3 Hz on a 15 fps take**.
- **The fix.** The fix shipped in PR #861, in v2.0.0-alpha.21 (August 2026).
- **The effect of the fix.** With the correct clock, a 60 fps take lost about 90% of its noise above 8 Hz, while the 0–6 Hz motion band changed by about 0.01%.

Any take that an older FreeMoCap processed at a rate other than 30 fps should be re-filtered from the raw 3D data using the true timestamps.

Even a correct filter blunts strikes. In soccer instep-kick studies, low-pass cutoffs below 20 Hz weakened motion near impact and underestimated peak accelerations by **30–70%**. A cutoff that rises during the impact worked best ([Augustus et al. 2020](https://eprints.chi.ac.uk/5020); source read via a search summary). A browser version of this is cheap:

1. Detect strike windows from speed peaks of the end joints (wrists, ankles).
2. Inside each window, crossfade over 2–3 frames from a heavily filtered track to a lightly filtered track with outliers removed.

That recipe is my inference from the impact literature, not a published tool.

The One Euro filter solves a different problem. Its cutoff rises with speed (`min_cutoff + beta·|speed|`). Casiez's tuning procedure ([Casiez](https://gery.casiez.net/1euro/)):

1. Set β = 0.
2. With the limb still, lower the minimum cutoff until jitter is acceptable.
3. During fast motion, raise β by factors of ten until lag is acceptable.

It suits live preview, but offline it measured worst of the classical smoothers (see the table below). MediaPipe also already applies it inside its own graph: image landmarks use `min_cutoff 0.05, beta 80`, and world landmarks `0.1, 40`. MediaPipe output that reaches FreeMoCap's Butterworth is therefore filtered twice ([MediaPipe](https://raw.githubusercontent.com/google-ai-edge/mediapipe/master/mediapipe/modules/pose_landmark/pose_landmark_filtering.pbtxt)).

Filter positions before deriving rotations, which avoids quaternion sign flips. If rotations must be filtered directly, first keep each quaternion in the same hemisphere as the one before it, flipping its sign when needed. This is my inference from the flip artifact SmoothNet reports.

| Filter | Best use | Measured or default settings | Caveat |
|---|---|---|---|
| Zero-phase Butterworth | Final offline bake | 4th order, 6 Hz default; cap the cutoff at 0.25×fps | Rings at gaps, so filter each NaN-free run separately |
| Savitzky-Golay | Offline smoothing that keeps peaks better | Acceleration error 1.34 on Human3.6M (window 31, order 2) | Over-smooths past ~65-frame windows |
| Gaussian | Simple smoothing | Acceleration error 2.43 (σ 3) | Blunts peaks more than Savitzky-Golay |
| One Euro | Live preview, jitter at rest | Acceleration error 3.80 offline; Pose2Sim preset 4 Hz, β 1.5 | Lag; β depends on units |

Table sources: [SmoothNet](https://ar5iv.labs.arxiv.org/html/2112.13715); [Pose2Sim config](https://raw.githubusercontent.com/perfanalytics/pose2sim/main/Pose2Sim/Demo_SinglePerson/Config.toml); [DeepWiki on Pose2Sim filtering](https://deepwiki.com/perfanalytics/pose2sim/3.4-data-filtering).

### Gaps, outliers and bone lengths are handled in a fixed order

Pose2Sim's pipeline is the most transparent open-source baseline ([Pose2Sim config](https://raw.githubusercontent.com/perfanalytics/pose2sim/main/Pose2Sim/Demo_SinglePerson/Config.toml); [Pose2Sim](https://github.com/perfanalytics/pose2sim)):

1. Reject 2D detections with likelihood under **0.3** or reprojection error over **15 px**, and require at least two cameras.
2. Run a Hampel outlier filter (7-frame window, about a 95% band) before any other filtering.
3. Linearly interpolate gaps shorter than **20 frames**, and hold the last value across longer ones.
4. Filter each continuous run separately.
5. Scale a fixed skeleton from trimmed-mean segment lengths, excluding deep-flexion frames.

Three adaptations matter for combat at webcam rates (my inferences):

- **Shorter, smoother gap fill.** 20 frames is two-thirds of a second at 30 fps. A cap of about 6–10 frames, filled with cubic rather than linear interpolation, is safer.
- **Don't mistake strikes for outliers.** A real strike looks like an outlier to a speed-based test. Run the Hampel test on how far each position sits from a short smoothed track, and flag only isolated spikes that snap back.
- **Catch L/R swaps.** A swap shows up as a frame where the left and right labels match the previous frame better when crossed than when uncrossed. Compare both pairings at each frame and relabel when the crossed one wins.

### Contacts are detected with two thresholds and edited as intervals

Foot contact is almost always defined as `height < h AND speed < v`.

- **Kimodo (NVIDIA)** computes exactly that per frame, with no smoothing and no hysteresis (a gap between the on and off thresholds that stops the state flickering) ([Kimodo](https://research.nvidia.com/labs/sil/projects/kimodo/docs/_modules/kimodo/motion_rep/feet.html)).
- **Contact-aware retargeting** counts the toe as in contact when it is **≤3 cm high and moves ≤1 cm per frame, for a 180 cm character**. Heel contact uses movement alone, and a 5-frame majority window fills holes ([Villegas et al.](https://ar5iv.labs.arxiv.org/html/2109.07431)).
- **PhysCap** caps contact at 0.1 m above the ground plane ([PhysCap](https://arxiv.org/pdf/2008.08880)).

Detectors trained on pressure insoles beat these hand-set thresholds and cope better with noise ([UnderPressure](https://arxiv.org/pdf/2208.04598)). The realistic browser version is still the threshold test, plus hysteresis and a minimum contact duration.

Locking a foot means pinning the contact point and solving two-bone leg IK. Two details matter:

- **Ramp the weight.** The IK weight must ramp over a few frames at the start and end of each contact. A jump in the ankle when contact starts or stops is the classic footskate-cleanup artifact ([Kovar et al.](https://research.cs.wisc.edu/graphics/Gallery/kovar.vol/Cleanup)).
- **Move the hips when the leg can't reach.** Rokoko added "automatic hip nudging when feet extend too far" ([Rokoko 2.2.1](https://support.rokoko.com/hc/en-us/articles/9503071465745-Rokoko-Studio-2-2-1)).

The floor itself can be fit from the lowest foot heights in slow-moving frames, using a low percentile. Pose2Sim has a `feet_on_floor` option that translates the motion onto a floor it assumes is flat ([Pose2Sim config](https://raw.githubusercontent.com/perfanalytics/pose2sim/main/Pose2Sim/Demo_SinglePerson/Config.toml)).

Automatic detection will misfire on combat, so the commercial tools all let the user override it:

- **DeepMotion** offers Auto, Always, Never and Grounding modes ([DeepMotion](https://deepmotion.com/article/foot-locking)).
- **Rokoko** puts each foot's lock keys on the timeline in two colours (green for the left foot, blue for the right). When neither foot is locked, it generates a jump arc. It tells users to review contact keys before applying any filter ([Rokoko docs](https://docs.rokoko.com/rkk-vision-documentation/get-started/edit-in-rokoko-studio); [Rokoko 2.6.1](https://support.rokoko.com/hc/en-us/articles/47324364931089-Rokoko-Studio-2-6-1-with-MAJOR-Coil-Suit-Upgrade)).

For kicks, release the lock as soon as foot speed rises (the chamber), rather than waiting for the foot to lift. For a pivot on the ball of the foot, lock the position but leave the yaw free.

### Interpolation cannot recover a peak that fell between frames

No surveyed source documents an upsampler that preserves peaks in markerless data. The consistent advice is to capture faster ([Pose2Sim](https://github.com/perfanalytics/pose2sim); [Move AI](https://docs.move.ai/knowledge/zcam-e2-m4-mark-ii-camera-configuration-guide)). DeepMotion's Speed Multiplier, built for low-frame-rate footage, warns that extreme values give "jerky or unnatural movement" ([DeepMotion](https://deepmotion.com/article/speed-multiplier)).

The practical sequence:

1. Clean at the native frame rate.
2. Upsample positions with a monotone cubic spline, which avoids an overshoot wobble at the end of a strike.
3. Re-derive rotations.
4. Re-run foot locking at the output rate.

A generic spline undershoots a strike's true extension. The tool has two options, both my inferences rather than documented practice:

- Fit a parabola through the three samples around the extreme and place a key at its peak, which falls between frames.
- Hand the peak frame to the artist as an obvious pose to push.

## Retarget by copying rotations, then let weighted IK restore contacts

Every production retargeter is a hybrid of rotation copying and IK. Autodesk's HumanIK runs in three stages ([HumanIK SDK](https://help.autodesk.com/cloudhelp/2016/ENU/HumanIK-SDK-Help/files/GUID-6987D1B4-229F-4EE0-978C-DC2D0C807D2E.htm)):

1. Copy the orientation of every joint from source to target.
2. Set each IK target's Reach, Pull and Resist values.
3. Run IK, plus "any additional special effects such as floor contact".

Two HumanIK settings matter most here ([Maya HumanIK](https://help.autodesk.com/cloudhelp/ENU/MayaCRE-CharacterAnimation/files/GUID-EB2A8BB0-801E-4251-8C4C-09986049CCBB.htm)):

- **Reach** is a percentage set per target. At 100% the body part goes to the source position, at 50% halfway between that and its own copied-rotation (FK) position, and at 0% the FK result is left alone.
- **Knee/Elbow Max Extension** (default 50) damps limbs near full extension "to prevent knee/elbow snapping".

Unreal's IK Retargeter is built the same way ([UE 5.3 retargeting](https://dev.epicgames.com/documentation/en-us/unreal-engine/ik-rig-animation-retargeting-in-unreal-engine?application_version=5.3); [TargetChainIKSettings](https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/TargetChainIKSettings?application_version=5.4)):

- **Rotation copy per chain.** "Interpolated" is the default. It matches rotations at the same relative distance along chains of different length. "One to One" is typically best for fingers.
- **Pole-vector matching**, so knees and elbows point the same way as the source.
- **IK targets** with a `blend_to_source` weight, from 0 (the retargeted location) to 1 (the exact source location).
- **Order.** "All FK operations run before the IK pass."

The tool's current scheme copies rotations, scales root travel by the hip-height ratio, and holds planted feet with two-bone IK. That is structurally correct. What it lacks:

- a continuous weight per IK target and per frame, instead of an on/off plant;
- hand IK targets;
- pole matching;
- extension damping.

### Long legs need root and pelvis handling, not just foot plants

When leg joint angles are copied, step length scales with leg length. Horizontal root travel should therefore scale by the leg-length or hip-height ratio.

- **Contact-aware retargeting** scales root velocity by the leg-height ratio ([Villegas et al.](https://ar5iv.labs.arxiv.org/html/2109.07431)).
- **Unreal's root-motion operation** scales by "relative height difference" ([UE Root Motion op](https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/IKRetargetRootMotionOpSettings?application_version=5.7)).

Scaling by overall height instead would under-scale the stride for NWN's long-legged body and leave the feet sliding.

Floating and sinking mostly come from foot-to-floor offsets. The big tools each handle it:

- **HumanIK** has **Ankle Height Compensation**. It also reads a per-character sole-to-ankle value (`FootBottomToAnkle`) when the two characters' ankles sit at different heights in their T-poses ([HumanIK SDK](https://help.autodesk.com/cloudhelp/2016/ENU/HumanIK-SDK-Help/files/GUID-6987D1B4-229F-4EE0-978C-DC2D0C807D2E.htm); [Maya HumanIK](https://help.autodesk.com/cloudhelp/ENU/MayaCRE-CharacterAnimation/files/GUID-EB2A8BB0-801E-4251-8C4C-09986049CCBB.htm)).
- **Unity** can base root height on the lowest foot, which "prevents floating problem when blending or transitioning" ([Unity](https://docs.unity.com/en-us/engine/current/manual/animation-section/animation-mecanim/avatar-creationand-setup/root-motion)).
- **Unreal** sets `affect_ik_vertical` to **0 by default**, so lowering the pelvis does not drag planted feet down with it ([TargetRootSettings](https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/TargetRootSettings?application_version=5.4)).

In practice:

1. Scale pelvis height as a change from the rest pose, with both rest heights measured from the floor.
2. Store a sole offset, so the IK puts the sole on the floor rather than the ankle joint.
3. Lower the pelvis smoothly whenever planted legs would over-extend.

### Short arms and torso break self-contacts unless hands are anchored to the body

Copied rotations should stay the default for arms, because free gestures scale naturally with arm length. Re-solve positions only where a relationship matters: a foot on the floor, a hand on the body, a hand on a prop. When you do, solve relative to the start of the chain (shoulder or hip). Unreal's `extension` setting works this way: it slides a target between the chain origin (0) and the retargeted end (1) ([TargetChainIKSettings](https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/TargetChainIKSettings?application_version=5.4)).

A shoulder-relative hand target is `T_shoulder + (S_hand − S_shoulder) × L_target_arm / L_source_arm`, where T is the target character, S the source, and L the arm length. Express it in the torso's frame, and clamp it to about 98% of arm length.

NWN's short torso, with copied spine rotations, drops the hands lower and closer to the middle of the chest. So a guard at the chin, or hands on hips, will miss their targets. The research addresses this directly:

- **Self-contacts matter.** Retargeting research calls them "important attributes of human body language" that existing methods fail to preserve ([Villegas et al.](https://arxiv.org/abs/2109.07431)).
- **Egocentric normalisation.** Molla, Galvan Debarba and Boulic express the hand's position relative to the body part it touches, normalised by the performer's body, then map it onto the target's body. In their user study, the advantage was largest "when the proportions of the animated character deviates from those of the performer" ([EPFL](https://infoscience.epfl.ch/record/229912)).

A cheap version uses capsules instead of a mesh:

1. When a source hand comes within a few centimetres of a landmark (chin, chest, hip, other hand), record the hand's position in that landmark's local frame, normalised by the landmark's size.
2. On the target, convert it back with the target landmark's size.
3. Drive the target hand's IK target to that point, easing the weight in and out.

Villegas' paper adds two safeguards worth copying ([Villegas et al.](https://ar5iv.labs.arxiv.org/html/2109.07431)):

- **Distance gate.** Enforce a contact only if the target's starting hand position is within **15 cm** of it.
- **Tolerate slight penetration** at the armpits and crotch. A strict no-penetration rule cut interpenetration to 0.35, but raised contact error to 7.94 and stiffened the motion.

### Align rest poses before anything else

Rotation copying is only correct relative to a rest pose the two skeletons share. Unreal resolves A-pose/T-pose mismatches in an editable Retarget Pose with Auto Align ([UE retargeting](https://dev.epicgames.com/documentation/en-us/unreal-engine/ik-rig-animation-retargeting-in-unreal-engine)). Rokoko's Blender add-on states that "both armatures need to be in the same pose" ([Rokoko](https://support.rokoko.com/hc/en-us/articles/4410463481489-Retarget-an-animation-in-Blender)).

The standard formula works with world-space rotations: `R_tgt(t) = R_src(t) · R_src_rest⁻¹ · R_tgt_rest`. Convert the result to a local rotation through the target's parent joint. This form does not care if the two rigs use different bone-axis conventions.

An unaligned rest pose shows up as the same error of tens of degrees in every frame of every clip, for example hands sunk into the thighs or flared out. Check it first when all clips look wrong in the same way.

To judge a retarget, Unreal shows source and target together, with preview-only offsets and debug drawing for the IK targets and the pelvis ([UE retargeting](https://dev.epicgames.com/documentation/en-us/unreal-engine/ik-rig-animation-retargeting-in-unreal-engine)). Epic's foot-sliding guide tunes plant thresholds from a plotted speed curve for each foot, whose "lower flat parts" are the plants ([UE foot sliding](https://dev.epicgames.com/documentation/en-us/unreal-engine/fix-foot-sliding-with-ik-retargeter-in-unreal-engine)).

The useful overlays for a home-made tool:

- the source skeleton, scaled by the hip ratio and planted under the target;
- an FK-only ghost, showing what IK changed;
- IK targets coloured by weight;
- a per-clip readout of the maximum foot slide during plants, sole penetration or float, and maximum limb extension.

## Four controls do almost all the editing on top of mocap

### Layers keep the capture recoverable

The standard pattern leaves the mocap untouched on the base layer.

- **Additive** layers add their values to the layers below.
- **Override** layers replace the layers below.
- **Weight** scales a layer's contribution. An override at weight 0.5 is half layer, half underlying motion ([Maya help](https://download.autodesk.com/us/maya/2011help/files/WS1a9193826455f5ff-6d855556117e9f816d2208c.htm); [Maya 2017](https://help.autodesk.com/cloudhelp/2017/ENU/Maya/files/GUID-BBCA0BC3-7608-4E86-8E9F-B4099C316156.htm)).

MotionBuilder makes new layers additive by default, and combines rotations "Per Layer" by default, which Maya calls quaternion rotation accumulation ([MotionBuilder](https://help.autodesk.com/cloudhelp/2022/ENU/MotionBuilder/files/GUID-84C2673F-828A-4D6B-8003-207B75860789.htm)). An Ohio State mocap course describes the routine: fix broken spans on an override layer, and animate its weight so the fix blends back into the capture. It also says never to switch a layer from additive to override after keying on it ([Ohio State](https://www.asc.ohio-state.edu/berezina-blackburn.1/classes/mocap/basic_propsMOBU.html)). Rigging Dojo's advice is blunter: "Animation layers are your friend and so is not being worried about baking everything down" ([Rigging Dojo](https://www.riggingdojo.com/2021/02/01/animators-learning-motioncapture/)).

Implemented per joint:

- An **additive** layer is a sparse set of keyed rotation changes ΔR, evaluated as `R = R_base · slerp(I, ΔR₁, w₁) · slerp(I, ΔR₂, w₂)…`.
- An **override** layer is `slerp(R_below, R_layer, w)`.
- **Mute** is `w = 0`.

That is a few dozen lines of code. It makes "edit on a layer" the only way to change motion, with flattening saved for export.

For webcam data, override layers have a second job. The two to five frames around a strike's peak are often too smeared to restore. Replacing them with keyed poses that blend in and out of the capture is the honest fix. A vendor guide says hand-keying beats mocap for very fast impacts that mocap smears ([MoCap Online](https://mocaponline.com/blogs/mocap-news/combat-animation-game-dev-guide)). Naughty Dog went further: it "willfully lowered the mocap quality by reducing keyframes, sometimes to only the key poses from an action", so that mocap matched its hand-keyed animation ([gameanim.com](https://www.gameanim.com/2008/04/19/uncharted-mocap/)).

### IK targets with keyed reach do the "put this hand here" work

In HumanIK, the mocap drives the FK skeleton, and a control rig of IK targets (Autodesk calls them "effectors") sits on top. Each target blends its translation and rotation toward IK. Pull, the setting that lets one target drag the rest of the body, does nothing unless the translation blend is above zero ([MotionBuilder](https://help.autodesk.com/cloudhelp/2024/ENU/MotionBuilder/files/GUID-66D7ED9D-D80E-4A2C-BFEE-AA4F819FD3DD.htm)).

Three keying modes decide how much of the body follows a drag ([Maya HumanIK keying modes](https://help.autodesk.com/cloudhelp/2023/ENU/Maya-CharacterAnimation/files/GUID-6EB8580A-0344-42F1-A722-4FDFD5BFA70C.htm)):

| Mode | What follows the drag | What gets keyed |
|---|---|---|
| **Full Body** | The whole character re-solves to reach the moved target | The whole body |
| **Body Part** | One limb | That limb only |
| **Selection** | Only the chosen targets | Only the chosen targets |

Pins hold a target's position or rotation while other parts move, and the feet are **pinned by default** when the control rig is created ([Maya Controls tab](https://help.autodesk.com/cloudhelp/2017/ENU/Maya/files/GUID-735DF85A-331C-4049-8C70-E53EFCDB4B73.htm)).

Keying reach from 0 to 1, holding it, then returning to 0 is how a hand stays on a table for thirty frames. Automatic foot locking is the same mechanism, driven by contact intervals. The cheap version per limb:

1. Compute the target point: `P_goal = lerp(P_fk, P_target, reach(t))`.
2. Solve it with closed-form two-bone IK.
3. Use the captured knee or elbow direction as the pole.

Dragging the hips and then re-solving both legs to their pins covers most "full body" needs. True full-body IK, with balance, soft pins and pull weights, is the one place this design gets expensive. Naughty Dog's alternative was an FK rig that could be manipulated as if it were IK, plus a visual arc tool for editing trajectories ([gameanim.com](https://www.gameanim.com/2008/04/19/uncharted-mocap/)).

### Key reduction turns a thousand dense keys into a few dozen poses

Key reduction comes in three families.

1. **Error-bounded curve reducers** drop keys while staying near the original curve. Maya's Key Reducer uses a precision tolerance. Maya's Simplify Curve defaults to a time tolerance of 0.05 s and a value tolerance of 0.01 ([Maya Key Reducer](https://help.autodesk.com/cloudhelp/2023/ENU/Maya-Animation/files/GUID-63E2EA0D-90E7-45EC-9AF3-D3097A8BA5B9.htm); [Simplify Curve](https://help.autodesk.com/cloudhelp/2023/ENU/Maya-Animation/files/GUID-455F76C5-F761-4007-AFFE-E0F80BEFB627.htm)).
2. **Every-nth-frame thinning** is quick but should be checked against the original ([Cineversity](https://cineversity.forums.maxon.net/post/3569)).
3. **Pose-aware "unbaking"** is the interesting one. Cascadeur works in three steps ([Cascadeur help](https://cascadeur.com/help/category/221); [80.lv](https://80.lv/articles/transform-baked-animations-into-editable-keyframes-in-cascadeur-2024-1)):
   1. Place keys where the foot's ground contacts ("fulcrums") change, and at the top of a jump.
   2. Add keys and choose an interpolation per interval, so the result stays within an "Interpolation difference" (default 20).
   3. Convert each pose into an AutoPosing pose with as few locked controllers as possible.

Academic work reaches the same goal by picking keyframes optimally ([Roberts et al.](https://scribblethink.org/Work/Pdfs/roberts_OptimalAndInteractiveKeyframeSelectionForMotionCaptureJournalversion.pdf)).

For the tool, the best fit is a whole-body variant:

1. Seed keys at contact changes and at frames where the body's speed is at a local minimum.
2. Measure the difference between the full pose and the interpolated pose at every frame.
3. Insert a key at the worst frame, and repeat until the error is under tolerance. This is the Ramer–Douglas–Peucker algorithm applied to pose distance.

Every joint then shares the same key frames, and the result reads as poses rather than scattered curve keys. Professionals keep the dense data on the base layer, and reduce only to restyle a clip or at export ([Game Anim](https://www.gameanim.com/?p=5168)). At export, measure the error against whatever interpolation NWN uses. This research did not confirm what that is.

### One time-remap curve handles speed, dead time and holds

Maya has three ways to retime a clip ([Maya retiming](https://help.autodesk.com/cloudhelp/2024/ENU/Maya-Animation/files/GUID-164A97B6-D316-4D3F-AF6B-E7BAE8F3AE11.htm)):

- **Clip scaling** changes the speed linearly.
- A **Time Warp** curve maps output frames to source frames and keeps the clip length fixed: "If you speed up one part, it will slow down another part."
- A **Speed Curve** changes the clip's duration.

The last two cannot be used together.

Practitioners use retiming this way:

- **Naughty Dog's** tools had a time-warp curve over whole animations. Animators used video reference to "accentuate poses, add punch to timing, snap extreme poses, and remove unnecessary pauses" ([gameanim.com](https://www.gameanim.com/2008/04/19/uncharted-mocap/)).
- **A fighting-game course** retimes with Maya's Scene Time Warp by "strategically removing dead frames, favoring key poses like anticipation", then bakes the result to the rig ([Gnomon](https://gnomon.thegnomonworkshop.com/workshops/animating-a-3-hit-combo-for-games-in-maya)).

One monotone remap curve, `s = W(t)` (output frame t → source time s), can express every timing edit:

| Edit | Shape of W |
|---|---|
| Remove dead time | A steep segment |
| Speed up the anticipation | Compress the approach to the extreme |
| Hold or hit-stop | A flat segment |
| Ease into an extreme | A shallow slope just before it |

Interpolate W with Fritsch–Carlson monotone cubics so it never runs backward, and sample the base pose by slerping between neighbouring dense source frames.

How layers keyed on top behave under a time warp is not documented in the sources found. Two workable rules:

- Key corrections in source time, so they follow the motion when it is retimed. Key polish in output time, so holds and pushes stay where you see them on the final timeline.
- Or follow the AAA serial order: retime, bake, then polish.

Either way, remap the contact intervals through the warp, or the foot locks will drift.

## Impact comes from the contact frame outward

### Raw capture feels floaty because of human timing, even spacing and protected extremes

Cooper's account of Assassin's Creed III states the AAA position ([gameanim.com](https://www.gameanim.com/2014/02/11/animating-3rd-assassin/7/)):

- During capture, "we still don't care about timing".
- Every take is sped up 15% as a starting point.
- Animators "reassemble the single action from the component parts".
- At every stage, animators "exaggerate and 'plus' every movement, make poses more dynamic, and adjust timing to give attacks more impact".

His older "Fast vs Slow" piece says the player character's feel "should always take precedence over the aesthetics of the animations" ([gameanim.com](https://www.gameanim.com/?p=115)). It splits an action into three phases:

- **Anticipation**, which runs up to the apex.
- **Apex**: "the frame (or window of frames) when the purpose of the particular animation has been achieved".
- **Recoil**, which returns the character to a set pose.

Anticipation is "generally longer for NPCs to allow the player time to react".

Raw mocap feels floaty for three reasons, and each maps onto one of the controls above:

| Cause | Fix |
|---|---|
| Real human speed | A global speed-up |
| Evenly spaced frames between key poses | A time warp that snaps into contact |
| Extremes a performer protecting his body never reaches | Additive pushes on the key poses |

A global "exaggerate" control that amplifies each joint's deviation from a smoothed copy of the motion is a cheap, crude extra. It is my suggestion, not documented practice, and it needs joint limits.

### Designers fix the hit frame; animators pose within it

God of War Ragnarök's combat team suggested the hit land "around frame 20", with the whole move "recovery included, done by frame sixty". Posing and pose spacing were left to the animators, who otherwise "would probably make very long animations" ([Eurogamer mirror](https://eurogamerai.pages.dev/posts/moving-kratos-a-deep-dive-into-god-of-war-ragnar-k-s-animation)). Animation director Bruno Velazquez sums it up as "the gameplay always comes first" ([80.lv](https://80.lv/articles/gameplay-is-first-santa-monica-studio-s-animation-director-on-god-of-war-ragnar-k)).

Fighting games give harder numbers, at 60 fps:

- Street Fighter V's fastest normal attacks hit on the 3rd frame ([Prima](https://primagames.com/eguides/street-fighter-v-eguide/primer/reading-frame-data)).
- Every Killer Instinct character has a 5-frame light attack, and heavy attacks are "never faster than roughly 8 or 9 frames" ([Infil](https://ki.infil.net/basics2.html)).

At NWN's 30 fps those numbers halve: a light attack lands in 1.5–2.5 frames and a heavy one in about 4. Shoves and kicks used by player characters need short anticipation. A rage roar used as a threat can hold long.

A fighting-game course says overlap (the trailing motion of body parts after the main action) should flow "from the spine out to the extremities", but be balanced so it does not weaken the attack ([Gnomon](https://gnomon.thegnomonworkshop.com/workshops/animating-a-3-hit-combo-for-games-in-maya)). Keep the striking limb crisp, and let overlap live on the head and the other side of the body.

### Hit-stop freezes both actors briefly, scales with damage, and must stay short in crowds

Sakurai's column on hit-stop describes the technique in detail ([Source Gaming translation](https://sourcegaming.info/2015/11/11/thoughts-on-hitstop-sakurais-famitsu-column-vol-490-1/)):

- **Both freeze.** Attacker and victim freeze "for the exact same amount of time".
- **Scaling.** Hit-stop is longer for more damage, up to a cap.
- **Attacker.** Keeps creeping forward, too slowly to see in any single frame.
- **Victim.** Shakes with decaying amplitude while the hurtboxes stay put. On the ground the shake is side to side, so the feet don't clip into the floor.
- **Into the hurt pose.** The victim takes **four frames** to blend from the first flinch into the hurt pose.
- **Crowds.** In games with many enemies, "If the action stopped every single time you landed an attack, you would likely end up frozen for a long time."

Published fighting-game values run from about 7 frames for light hits to 11 for heavy ones in King of Fighters XIII ([SuperCombo](https://srk.shib.live/w/The_King_of_Fighters_XIII/Character_States/Hitstun_Blockstun_state); source read via a search summary). Research on impact feel ranks hit-stop among the features with the most effect on perceived impact ([Lin et al.](https://faculty.washington.edu/zkwen/articles/lin22features.pdf)).

Nothing found shows that NWN can freeze two actors in sync at runtime, so hit-stop will probably have to be baked into the clips:

- a 2–3 frame hold at contact (at 30 fps), with a tiny drift rather than a dead freeze;
- an overshoot-and-settle on the contact pose;
- a short, decaying shake on hit reactions.

With many combatants on screen, Sakurai's warning argues for keeping those holds at the short end.

### NWN's blend window can swallow a snappy start

Kristjan Zadziuk's analysis of God of War (2018) says the team "basically let the blend do the work" going into the defence stance, using "about a three- to five-frame full-body blend" ([Game Developer](https://gamedeveloper.com/production/creating-fluid-combat-animations-for-the-new-i-god-of-war-i-)).

Community engine documentation (read via search summaries) describes NWN's animation blocks ([nwn.wiki](https://nwn.wiki/x/iYW3); [rollnw](https://rollnw.readthedocs.io/en/stable/api/struct/nw__model__Animation.html)):

- a per-animation `transtime`, default **0.25 s**, about 7.5 frames at NWN's 30 fps;
- an `animroot`;
- a per-animation list of events.

If `transtime` is the incoming crossfade, the first quarter-second of an attack is averaged with the previous pose. That would smear the start of a snappy anticipation. There are two fixes: give attack clips a shorter `transtime`, or open the clip on a near-stance pose and put the real anticipation after the blend window.

Clip conventions for combat:

- Start and end every combat clip on the same combat stance the combat idle loops on.
- Return root travel to the origin by the last frame.
- Put an animation event on the contact frame, if the engine allows it.

For loops such as the crouched idle, Cooper's cycle tutorial gives the technique ([Game Anim](https://www.gameanim.com/?p=5168)):

- Join the loop on two similar *complete actions*, not on two single poses.
- Blend across "two similarly large motions rather than two subtle ones".
- Choose the blend length knowing that "Longer durations provide most seamless results but will increase foot-sliding."

Three questions remain open, and the tool's design depends on them:

- Is `transtime` really the crossfade?
- How does NWN treat translation on the animroot?
- When does the engine resolve damage relative to the clip?

## The big tools share one small core and break at the boundaries between their subsystems

Across MotionBuilder, Maya, Cascadeur, Rokoko, Blender and Unreal, the everyday loop is the same:

1. Scrub the timeline.
2. Select a body part.
3. Drag an IK target or rotate an FK joint.
4. Pin the planted foot or hand.
5. Key on an additive layer with a weight.
6. Retime a span.

Each tool wraps that core in a contact model and a key-reduction step. No usage data exists on which features animators actually use. The split between essential and rarely needed features below is inferred from what the documentation and workflow guides emphasise.

| Tool | Core idea worth copying | Friction or complaint | Rarely needed for cleaning up one game clip |
|---|---|---|---|
| MotionBuilder / HumanIK | 2D figure panel of IK targets (Shift-click grabs a whole body part, Ctrl-click toggles one); pin buttons that act on the selection; Body Part vs Full Body keying ([MotionBuilder 2025](https://help.autodesk.com/cloudhelp/2025/ENU/MotionBuilder-Reference/files/MotionBuilder-Windows/Character-Controls/GUID-F286B854-A373-49F2-9DB3-1D788208026B.html)) | Clips in Story (its clip editor) must be baked and deleted before the control rig can be keyed ([Ohio State](https://www.asc.ohio-state.edu/berezina-blackburn.1/classes/mocap/story.html)) | Clip matching in Story, travel nodes, auxiliary targets, pin presets, hoof and finger contact types |
| Maya | Time Warp vs Speed Curve; Graph Editor | Two layer systems (Animation Layers and the Time Editor's Clip Layers) that cannot drive the same animation; the Time Editor ignores HumanIK keying modes ([Maya Clip Layers](https://help.autodesk.com/cloudhelp/2024/ENU/Maya-Animation/files/GUID-B3FFE1AA-F44D-4EC3-A570-316A3BF3DF88.htm); [Maya HIK in Time Editor](https://help.autodesk.com/cloudhelp/2022/ENU/Maya-Animation/files/GUID-A3D4A1E8-E353-4ADA-A7C2-F264DF083C39.htm)) | Clip-level layering |
| Cascadeur | Unbaking keyed at contact changes; the interval between keys as the unit of editing; trajectories drawn with dark keys and bright in-betweens, where dragging a path point marks the interval Fixed ([Cascadeur trajectories](https://cascadeur.com/help/tools/animation_tools/trajectories)) | The free tier is non-commercial and exports only .casc; retargeting is in Pro ([Cascadeur FAQ](https://cascadeur.com/blog/general/cascadeurs-new-licensing-structure-comprehensive-faq)) | AutoPhysics, physics-based AutoPosing |
| Rokoko Studio | A stack of filters fed by an editable two-colour contact lane; "review your foot contact keyframes … before applying any filters" | No foot-sliding support; no general pose editing ([Rokoko docs](https://docs.rokoko.com/rkk-vision-documentation/get-started/edit-in-rokoko-studio)) | None identified |
| Blender | NLA strips with scale and influence | Additive NLA strips give "unpredictable results, including massive distortion of the mesh"; a bug report on broken Add/Subtract playback ([BlenderArtists](https://blenderartists.org/t/using-nla-to-correct-motion-capture-anims/1169389); [T50433](https://developer.blender.org/T50433)) | General skeleton-to-skeleton retargeting, when there is only one fixed target |
| Unreal | Bake mocap onto a Control Rig with a key on every frame, then edit with pose and snapper tools and layers ([Packt](https://www.packtpub.com/en-kr/product/unreal-engine-5-character-creation-animation-and-cinematics-23-9781801812443/chapter/chapter-19-motion-capture-editing-and-cleanup-using-control-rig-and-sequencer/baking-the-stand-up-mocap-to-the-control-rig)) | Users report override layers over layered Control Rigs acting additively or not moving the mesh ([UE forum](https://forums.unrealengine.com/t/using-animation-layers-with-layered-control-rig/2268479)) | Tuning the retargeter's operation stack |

The documented complaints are almost all about these boundaries:

- two layer systems for one concept;
- a clip editor that ignores the rig's keying rules;
- a bake step that loses data;
- layering that misbehaves.

Almost none are about missing features. Unity is a good example: transferring motion onto its rig constraints does not support Humanoid rigs, and "the result of transferring motion does not precisely represent the source animation". Its Two Bone IK constraint has no listed limitations ([Unity](https://docs.unity3d.com/Packages/com.unity.animation.rigging@1.3/manual/BidirectionalMotionTransfer.html)).

The lessons for a home-made tool:

- One timeline, one layer stack, and one keying mode that every editing surface respects.
- Two-bone IK as the core constraint, because it converts between FK and IK cleanly.
- Explicit body-part masks on layers. Maya warns that a layer touching unintended attributes can leave "rig objects... pinned unintentionally" ([Maya](https://help.autodesk.com/cloudhelp/2022/ENU/Maya-Animation/files/GUID-63B2245A-5D4E-4844-A2F0-A2DFCA39F10C.htm)); MotionBuilder's Story lets each track pick the body parts it affects.
- An onion-skin ghost of the original capture, like MotionBuilder's Story ghosts, to show how far an edit has moved from the performance ([MotionBuilder ghosts](https://help.autodesk.com/cloudhelp/2017/ENU/MotionBuilder/files/GUID-DB244A01-1C04-4A79-9C42-AAAD0B9C0A7C.htm)).

## Recommended tool: one timeline, one layer stack, contacts as the source of truth

The modder's instinct is right, with three changes drawn from everything above:

- **IK placement** should be a weighted IK target whose reach is keyed, with foot targets coming from contact intervals. It should not be a move baked into the pose.
- **"Keying a pose"** should mean an additive change with falloff around a tagged extreme, leaving the capture underneath. Save override keys for strike windows the webcam smeared.
- **Retiming** should be one remap curve anchored to marked events (contact frame, recovery frame, start and end stance), not only a global speed slider.

The evaluation order follows the professional tools:

1. The cleaned base, in source time.
2. Correction layers, in source time.
3. The time warp.
4. Polish layers, in output time.
5. A single IK pass that applies contacts and reach targets after all FK edits, as Unreal runs FK before IK.
6. Root handling, then the 30 fps bake with key reduction.

| Stage | Runs automatically | Artist controls | Default to start from |
|---|---|---|---|
| Import | Re-filter FreeMoCap's raw 3D data using the true timestamps; resample to uniform time | Choose the take range | Ignore output already filtered by an older FreeMoCap |
| Cleanup | Confidence gate, Hampel outliers, gap fill, L/R swap fix, fixed bone lengths, zero-phase filter per joint group, strike-window protection | Smoothing in Hz per group (trunk / limbs / hands and feet) | 0.3 likelihood, 7-frame Hampel, fill gaps up to 6–10 frames, cutoff ≤ 0.25×fps |
| Floor and contacts | Floor fit; height-plus-speed contact detection with hysteresis | A contact lane per foot (optionally per hand) with draggable interval edges; Auto / Always / Never / Grounding per range | ~3 cm and ~1 cm/frame at 180 cm; 5-frame majority |
| Retarget | Rest-pose world-rotation copy; root scaled by hip ratio; sole offset; contact IK with eased weights; hips lowered when legs over-extend | Reach weight curve per IK target; hand-to-landmark contacts | Feet pinned during contacts; hands FK unless in contact |
| Phase markers | Suggest candidate extremes from speed minima and contacts | Tag anticipation / contact / overshoot / recovery; mark the frame where control returns | Time-to-contact readout in ms and frames |
| Retime | Nothing | Time-warp curve; "remove dead frames"; "insert hold at contact (N frames)"; global speed | ≈0.87× duration to start (AC3's +15% speed) |
| Polish | Falloff around each pose key | Additive pose changes on masked layers; override for a smeared peak; overshoot-and-settle | Layer weight 1, masked by body part |
| Clip | Loop finder over pose-plus-speed windows; root returns to origin | Loop blend length; start and end stance | Start and end on the combat stance |
| Export | Whole-body pose-key reduction measured against the engine's interpolation; 30 fps bake | Reduction tolerance | Preview from the in-game camera pitch |

| Priority | Features |
|---|---|
| Essential now | Layer stack (additive and override, weight, mute/solo, body-part mask); two-bone IK per limb with keyed reach and an FK-derived pole; foot contact lane with auto-detection and override modes; time-remap curve with hold and dead-frame tools; contact and phase markers; ghost of the original capture; correct zero-phase filtering with Hz controls; undo |
| Next | Hand-to-body contact anchoring with capsules; hips drag with feet pinned; pose-key reduction for heavy restyling; loop finder and blend; strike-window peak protection; global exaggerate control; retarget quality readouts |
| Skip or defer | Full-body IK with balance; physics-based posing (AutoPhysics-style); learned retargeting or smoothing; Story-style assembly of many clips with matching and travel nodes; retargeting to any skeleton other than the one NWN target |

## Conclusion

With webcam data, the editor's job is partly reconstruction, not only restoration. Mocap reliably supplies weight shifts, balance, transitions and the overall shape of a move. It does not reliably supply the few frames around a strike's contact, and those are the frames that carry impact. That makes the contact frame, together with the foot-contact intervals, the natural organising object for the whole data model. It anchors the IK targets, the time warp, where pose keys go, where hit-stop holds are inserted, and where the filters must back off. When every edit refers to the same marked events, retiming, IK placement and pose keys work together instead of fighting each other.

This also reframes what the capture is for. Santa Monica's own animators wore mocap suits to sketch scenes and combat moves before final capture ([GameRevolution](https://www.gamerevolution.com/features/513433-god-of-war-mature-kratos-sony-santa-monica)). A solo modder's webcam takes play the same role: a physically grounded sketch of timing and weight, to be sped up, pinned down and pushed at the extremes, not a final performance to preserve frame by frame. Three engine facts should be checked before committing to baked hit-stop, a root-return strategy or a key-reduction tolerance: what NWN's `transtime` actually blends, whether translation on the animroot moves the creature, and how keys are interpolated at playback.
