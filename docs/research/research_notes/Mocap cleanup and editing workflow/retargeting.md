# Retargeting performer motion onto a differently-proportioned game character (NWN humanoid), and preserving contacts

Scope note: the target case is an average-proportion performer → NWN's heroic humanoid (~1.9 m, long legs, short torso and arms). The current tool copies joint rotations, scales root travel by the hip-height ratio, and holds planted feet with two-bone leg IK. That is already the "hybrid" design the commercial tools use (see Q1). The findings below cover the parts it still lacks: pelvis/floor handling, contact detection, hand/self contacts, rest-pose alignment and visualisation. Tool-specific parameter names are quoted so they can be reused as UI labels or defaults.

---

## Q1. Rotation-based vs position/IK-based vs hybrid retargeting, and how HumanIK, Unreal IK Retargeter, Unity Humanoid and Blender add-ons do it

### Takeaway
Every production retargeter is a hybrid. It first copies rotations (FK) so the target keeps the performer's joint angles and style. Then it runs an IK pass that moves end effectors (feet, sometimes hands) toward goals derived from the source, with a per-effector weight between "keep the FK result" (0) and "match the source position exactly" (1). Pure position copying is not used for the body. HumanIK solves in exactly this order: copy orientations, set effector Reach/Pull/Resist, then run IK plus floor contact. Unreal exposes the same design as chain FK modes plus IK goals with "Blend to Source".

### Cited Findings
**Autodesk HumanIK (Maya / MotionBuilder / SDK)**
- The solver runs in three stages. (1) It "copies the orientation of all Nodes in the source character's HIKCharacterState" to the target; only orientation is mentioned, not translation. (2) It sets each effector's Reach, Pull and Resist values from the target's character properties. (3) It runs the IK solver, which applies Reach/Pull/Resist and "performs any additional special effects such as floor contact." The solving step can be set to 0 to skip the IK pass entirely. — [HumanIK SDK: Retargeting Solving](https://help.autodesk.com/cloudhelp/2016/ENU/HumanIK-SDK-Help/files/GUID-6987D1B4-229F-4EE0-978C-DC2D0C807D2E.htm)
- **Reach** is set per effector (head, chest, elbows, wrists, hands, knees, ankles, feet). It is the percentage of how far a body part goes toward the source position: 100% = full reach, 50% = halfway between the source position and its own (FK) position, 0% = same as off. **Pull** sets an effector's priority relative to the others and applies only when reach translation > 0; the docs advise leaving it alone for normal retargeting. **Stiffness** controls how strongly an effector keeps its original position when pulled. — [Maya HumanIK Retarget Specific attributes](https://help.autodesk.com/cloudhelp/ENU/MayaCRE-CharacterAnimation/files/GUID-EB2A8BB0-801E-4251-8C4C-09986049CCBB.htm); [Maya: Adjust retargeting parameters](https://help.autodesk.com/cloudhelp/2022/ENU/Maya-CharacterAnimation/files/GUID-B44C7337-EF51-4BEC-B3D7-C8EB5957847B.htm)
- **Match Source** makes the solver compensate for scale differences between source and target. **Action Space Comp Mode** (Off / Auto (default) / User, value default 100) calculates and applies scale compensation. — [Maya HumanIK Retarget Specific attributes](https://help.autodesk.com/cloudhelp/ENU/MayaCRE-CharacterAnimation/files/GUID-EB2A8BB0-801E-4251-8C4C-09986049CCBB.htm)
- Limb-safety settings. **Knee/Elbow Max Extension** (default 50) "damps the leg/arm at full extension to prevent knee/elbow snapping". **Knee/Elbow Compression**: at 100% the extended limb pushes the hips/chest instead of bending. — [Maya HumanIK Retarget Specific attributes](https://help.autodesk.com/cloudhelp/ENU/MayaCRE-CharacterAnimation/files/GUID-EB2A8BB0-801E-4251-8C4C-09986049CCBB.htm)

**Unreal IK Retargeter (UE5; restructured into an "op stack" in 5.6+)**
- Chains are defined on both source and target IK Rigs (Chain Name, Start Bone, End Bone, optional IK Goal) and matched by fuzzy name. The pelvis is set explicitly ("Set Pelvis") so root motion "can be defined and transferred in a proportional way". Operations are modular and reorderable, and custom ops can be written in C++. IK can optionally maintain "precise hand or foot contact points". — [UE: IK Rig Animation Retargeting](https://dev.epicgames.com/documentation/en-us/unreal-engine/ik-rig-animation-retargeting-in-unreal-engine)
- **FK rotation modes** (`RetargetRotationMode`):
  - **Interpolated** (default) normalizes both chains by length and matches rotations at the same normalized distance along the chain. It "typically provides the best result" for different bone counts or proportions.
  - **One to One** copies bone by bone from the chain root; extra target bones stay at their reference pose. It is typically best for fingers.
  - **One to One Reversed** works the same way but starts from the tip.
  - **None** leaves rotations at the reference pose.
  
  `rotation_alpha` scales the rotation applied. — [UE 5.3 IK Rig Retargeting](https://dev.epicgames.com/documentation/en-us/unreal-engine/ik-rig-animation-retargeting-in-unreal-engine?application_version=5.3); [Python API TargetChainFKSettings 5.4](https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/TargetChainFKSettings?application_version=5.4)
- **FK translation modes**: **None** (default and "usually the best choice": target bone translations stay at the retarget pose), **Globally Scaled** (source bone offset × the skeleton's global scale), **Absolute** (exact source positions). — [UE 5.3 IK Rig Retargeting](https://dev.epicgames.com/documentation/en-us/unreal-engine/ik-rig-animation-retargeting-in-unreal-engine?application_version=5.3); [TargetChainFKSettings](https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/TargetChainFKSettings?application_version=5.4)
- **Pole Vector Matching** (0–1) twists the chain root so the chain's pole (knee/elbow) direction matches the source without moving the end bone. There is also `pole_vector_maintain_offset` and `pole_vector_offset` (degrees). — [TargetChainFKSettings](https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/TargetChainFKSettings?application_version=5.4)
- **IK goal settings** per chain:
  - `blend_to_source` "Blends IK goal position from retargeted location (0) to source bone location (1)", with per-axis `blend_to_source_weights`. The docs compare it to "reach" in other tools.
  - `static_offset` (global), `static_local_offset`, `static_rotation_offset`.
  - `extension` (0–5, default 1) "Brings IK goal closer (0) or further (1+) from origin of chain".
  - `affected_by_ik_warping`: typically on for feet, off for hands.
  - `scale_vertical` scales the goal's vertical component.
  
  "All FK operations run before the IK pass." — [Python API TargetChainIKSettings 5.4](https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/TargetChainIKSettings?application_version=5.4); [UE 5.3 IK Rig Retargeting](https://dev.epicgames.com/documentation/en-us/unreal-engine/ik-rig-animation-retargeting-in-unreal-engine?application_version=5.3)

**Unity Humanoid / Mecanim**
- Range of motion is defined per "Muscle". For example, Head-Nod and Head-Tilt default to −40..40°, and ranges can be narrowed to add stiffness. With **Translate DoF** disabled, "Unity animates the bones using only rotations". Translation DoF exists only for Chest, UpperChest, Neck, Upper Legs and Shoulders, and costs an extra retarget step. — [Unity Manual: Muscle definitions](https://docs.unity.com/en-us/engine/current/manual/animation-section/animation-mecanim/avatar-creationand-setup/muscle-definitions)
- The Body Transform (centre of mass) and orientation "are the only world-space curves stored in the Animation Clip. Everything else: muscle curves and IK goals (Hands and Feet) are stored relative to the body transform." In other words, Unity stores rotations (muscles) and end-effector goals side by side: a hybrid representation. — [Unity Manual: How Root Motion works](https://docs.unity.com/en-us/engine/current/manual/animation-section/animation-mecanim/avatar-creationand-setup/root-motion)
- The avatar `HumanDescription` has `armStretch` / `legStretch` ("Amount by which the arm's/leg's length is allowed to stretch when using IK"), `feetSpacing` ("Modification to the minimum distance between the feet"), and twist-distribution fields (`upperArmTwist`, `lowerArmTwist`, `upperLegTwist`, `lowerLegTwist`). — [Unity Scripting API: HumanDescription](https://docs.unity3d.com/450/Documentation/ScriptReference/HumanDescription.html)

**Blender add-ons**
- **Rokoko Studio Live retargeting**: pick source and target armatures, press "Build Bone List", then check and fix the mapping. "Use Pose" picks the pose both armatures must share for a correct retarget (Rest or Current). Enable "Auto Scale" when the rigs differ in size. If feet sink or float, or the hips stay fixed, try disabling Auto Scale and/or using "Current". — [Rokoko support: Retarget an animation in Blender](https://support.rokoko.com/hc/en-us/articles/4410463481489-Retarget-an-animation-in-Blender)
- **Auto-Rig Pro Remap** retargets "according to the bones names matches and bones original orientation". It has "an interactive offset tool to remap to different characters proportions", IK-retargeting options, and a "Smart IK Pole" that is now on by default for legs. A fix noted that IK pole height for 3-bone legs "was not scaling linearly with the character's height". — [Superhive: Auto-Rig Pro](https://superhivemarket.com/products/auto-rig-pro); [Auto-Rig Pro version history](https://superhivemarket.com/products/auto-rig-pro/versions)

**Research baselines**
- Gleicher 1998 treats key features as constraints and uses a spacetime solver to compute "an adapted motion that re-establishes these constraints while preserving the frequency characteristics of the original signal". It highlights "characters of changing sizes [that] maintain both steady footplants and hand connections". — [SIGGRAPH History: Gleicher 1998](https://history.siggraph.org/?p=116535); [Gleicher preprint](https://research.cs.wisc.edu/graphics/Courses/cs-838-1999/Papers/retarget-preprint.pdf)
- Choi & Ko 2000, "Online motion retargetting", is a per-frame method built on inverse rate control. It tracks several end-effector trajectories at once and uses the kinematic redundancy to minimise joint-angle differences from the original, which keeps the original's high-frequency detail. — [IEEE Xplore (1999 conference version)](https://ieeexplore.ieee.org/document/803346); [Wiley abstract index](https://acnpsearch.tweb-dev.unibo.it/singlejournalindex/78937055)

**ML methods (brief; not needed for a solo tool)**
- Skeleton-Aware Networks (SAN, Aberman et al. 2020) retarget between different skeletons offline and need IK post-processing for feet. Villegas reports that SAN+IK improves foot accuracy but "sacrifices the geometry quality". — [ar5iv: Contact-Aware Retargeting](https://ar5iv.labs.arxiv.org/html/2109.07431)
- Villegas et al. 2021 use "a novel geometry-conditioned recurrent network with an encoder-space optimization strategy" to preserve self-contacts and reduce interpenetration. — [arXiv 2109.07431](https://arxiv.org/abs/2109.07431)
- Non-ML takeaways from these papers that a solo developer can reuse: their contact *detection thresholds*, height normalisation, the 15 cm enforcement gate and the "tolerate armpit/crotch penetration" weighting. Their networks are not needed for any of those.

### Inferences
- The current tool (copy rotations → scale root → two-bone leg IK on plants) is structurally the same as HumanIK and Unreal. What it lacks are per-effector weights ("Reach" / "Blend to Source") instead of an on/off plant, hand effectors, pole matching, and limb-extension damping.
- Implementation recipe (a reading of HumanIK's documented order):
  1. Per frame, copy rotations as FK (see Q5 for rest-offset math).
  2. Place the pelvis (Q2).
  3. Compute an IK goal per limb.
  4. Blend each goal between the target's FK end position and a "retargeted source" position with a weight w ∈ [0, 1].
  5. Solve analytic two-bone IK with pole matching.
  
  Keep w per effector and per frame, so contacts can raise it to 1 and free motion can leave it at 0.
- Use "Interpolated"-style chain mapping (normalise chain length, sample source rotations at the same fractional distance) only where bone counts differ, typically the spine/neck. Use one-to-one for limbs.

### Gaps
- Epic's newest "Retargeting Operation Stack" page (5.6–5.8) would not render, so the exact op list and op parameters for 5.6+ (beyond Pelvis Motion, Stride Warp, Root Motion and Speed Plant settings found in the Python API) are not confirmed here. A "Pin Bone" op could not be confirmed.
- Unity's normalisation of muscle values to −1..1 and its `humanScale` body-position normalisation are not described on the pages fetched.

---

## Q2. Root motion, hip-height scaling, pelvis offset, and keeping the character from floating or sinking

### Takeaway
The standard rule is to scale the pelvis/root translation by a ratio of target to source size. Unreal's root motion uses "relative height difference"; the contact-aware paper uses the *leg-height* ratio, which is what the tool already does. Tools add horizontal and vertical scale multipliers, blend-to-source, static offsets, an explicit floor/crotch offset, and an option for IK goals to ignore vertical pelvis changes. Most floating and sinking comes from ankle-to-floor offsets and rest-pose height mismatches. HumanIK fixes it with **Ankle Height Compensation** and **Hips Level**; Unity with root-Y "Based upon Feet"; Unreal with "Snap Character to Ground" and a pelvis floor constraint.

### Cited Findings
- **Unreal root / pelvis settings**:
  - `scale_horizontal` / `scale_vertical` (default 1) scale root motion in the XY plane and along Z.
  - `blend_to_source` (0–1) pulls the root toward the exact source root location.
  - `translation_offset` is a component-space offset.
  - `affect_ik_horizontal` (default 1) / `affect_ik_vertical` (default **0**) control whether root modifications also move IK goal positions.
  
  — [Python API TargetRootSettings 5.4](https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/TargetRootSettings?application_version=5.4). In 5.6+ these became the **Pelvis Motion op** (same fields, plus explicit `source_pelvis_bone` / `target_pelvis_bone`) — [IKRetargetPelvisMotionOpSettings 5.6](https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/IKRetargetPelvisMotionOpSettings?application_version=5.6); [TargetRootSettings 5.7 (deprecated note)](https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/TargetRootSettings?application_version=5.7)
- **Pelvis Motion op (C++)**:
  - `FloorConstraintWeight` (0–1).
  - `bUseGroundFalloff`: the floor constraint's effect weakens as the pelvis rises.
  - `GroundFalloffHeightPercent` (0.1–1.0): the height where the floor constraint starts blending in, as a percentage of the pelvis height in the retarget pose.
  - `SourceCrotchOffset` / `TargetCrotchOffset`: a negative vertical cm offset from each pelvis bone.
  - `BlendToAbsoluteOffset` blends "from motion scaled by the size of the character, to the absolute delta motion from the source".
  - `ScaleHorizontal` / `ScaleVertical` have a UI range of 0–3.
  
  — [FIKRetargetPelvisMotionOpSettings](https://dev.epicgames.com/documentation/unreal-engine/API/Plugins/IKRig/FIKRetargetPelvisMotionOpSetting-)
- **Unreal Root Motion op**: "Copy From Source Root" copies source root motion "but scales it according to relative height difference". "Generate From Target Pelvis" derives root motion from the retargeted pelvis, with an option to keep the root-to-pelvis offset from the target retarget pose. Root height can be "Copy Height From Source" or "Snap To Ground" (Z=0). — [IKRetargetRootMotionOpSettings 5.7 (via search summary)](https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/IKRetargetRootMotionOpSettings?application_version=5.7)
- Unreal's retarget-pose editor has **Snap Character to Ground**, which translates the skeleton vertically to restore its height above the ground. — [UE: IK Rig Animation Retargeting](https://dev.epicgames.com/documentation/en-us/unreal-engine/ik-rig-animation-retargeting-in-unreal-engine)
- Anecdotal: a UE5 forum user fixed inaccurate retargeted hip placement by setting the Root chain's Translation Mode to "Globally Scaled". — [Unreal forum thread](https://forums.unrealengine.com/t/ue5-retargeted-root-motion-animations-have-innacurate-pelvis-hip-placement/514656/35)
- **HumanIK**:
  - **Hips Level Mode**: Auto (default) / Off / User; Hips Level is relative to the centre of gravity.
  - **Ankle Height Comp Mode**: Auto (default) keeps "the feet relating correctly to the floor"; User sets the foot–floor distance.
  - **Feet Spacing Mode**: Auto adjusts the spacing between the target's ankles.
  - **Mass Center Compensation** (default 80) "Smooths leg movement and reduces heavy footfalls".
  - **Hips Enforce Gravity** (default 60%) forces the hips between the feet.
  
  — [Maya HumanIK Retarget Specific attributes](https://help.autodesk.com/cloudhelp/ENU/MayaCRE-CharacterAnimation/files/GUID-EB2A8BB0-801E-4251-8C4C-09986049CCBB.htm)
- In the HumanIK SDK, the source property set is used only to read `HIKFootBottomToAnkleId`, when foot contact is used and the ankles "are not at the same distance from the floor" in the default T-stances. In other words, sole-to-ankle height is a per-character calibration value. — [HumanIK SDK: Retargeting Solving](https://help.autodesk.com/cloudhelp/2016/ENU/HumanIK-SDK-Help/files/GUID-6987D1B4-229F-4EE0-978C-DC2D0C807D2E.htm)
- **Unity**: the Root Transform is the Body Transform (centre of mass) projected onto the ground plane. Root Y "Based Upon: **Feet**" makes the Root Transform Y "match the lowest foot Y for all frames … which prevents floating problem when blending or transitioning". "Bake Into Pose" for XZ on idles "will stop the accumulation of small deltas drifting after many evaluations". — [Unity Manual: How Root Motion works](https://docs.unity.com/en-us/engine/current/manual/animation-section/animation-mecanim/avatar-creationand-setup/root-motion)
- **Rokoko**: feet sinking or floating, or hips staying fixed, are the symptoms that call for toggling Auto Scale or the Use Pose option. — [Rokoko support](https://support.rokoko.com/hc/en-us/articles/4410463481489-Retarget-an-animation-in-Blender)
- **Research practice**: Villegas et al. scale root velocity by the leg-height ratio and divide foot and end-effector terms by character height h. — [Contact-Aware Retargeting of Skinned Motion (ar5iv)](https://ar5iv.labs.arxiv.org/html/2109.07431). Gleicher's initial estimate likewise "scale[s] translational parameters" and adds a translation to define the centre of scaling. — [WPI summary of Gleicher 1998](https://davis.wpi.edu/~matt/courses/retarget)

### Inferences
- **Hip-height vs overall-height ratio.** When leg joint angles are copied, step length scales with leg length. The horizontal root ratio should therefore be leg/hip-height-based, as the tool does now. Overall height would under-scale travel for a long-legged NWN body and make feet slide forward. This is consistent with the Villegas choice.
- **Vertical pelvis**, a recipe to avoid float/sink:
  - `target_hip_z(t) = target_rest_hip_z + (source_hip_z(t) − source_rest_hip_z) × k_v`, where `k_v` is the hip-height ratio (or a user `scale_vertical`).
  - Measure both rest heights from the *floor*, not from the origin.
  - Calibrate the source floor per take, e.g. the median of the lowest foot height during detected plant frames. Mocap floors are often a few cm off.
  - Store a per-character sole-to-ankle offset, as HumanIK's `FootBottomToAnkle` does, so planted IK goals put the *sole* on the floor rather than the ankle joint.
- **Hip lowering.** When IK goals are held on the floor but the scaled pelvis puts the hips too high (straight legs, popping knees), lower the pelvis so that min(leg length − ε) holds, applied smoothly. This is what HumanIK's Max Extension / Compression knobs and Unreal's floor constraint address in different ways. Conversely, never let the pelvis go below `crotch offset` above the floor.
- **Keep vertical root changes out of foot goals** (Unreal's default `affect_ik_vertical = 0`). If the user drops the pelvis to fix knee pops, planted feet must not drop with it.

### Gaps
- No official Epic text gives the exact formula of the base root retarget (whether absolute pelvis height or delta-from-rest is scaled, and by what ratio). The Unreal claims above are parameter docs only, and the 5.7 Root Motion op text came via a search summary rather than a full fetch.
- The exact behaviour of Unreal's pelvis "floor constraint" (hard clamp vs vertical scale near the floor) is not documented on the pages fetched.

---

## Q3. Preserving contacts: feet planting, hands on body or each other, hands on props, self-penetration

### Takeaway
Contacts are *detected on the source* with cheap thresholds: speed for feet; distance or velocity agreement for hand-to-body. They are then *re-imposed on the target as IK goals*, with smooth blend-in/out. Unreal's Speed Planting is a speed threshold plus spring unplant. The contact-aware paper gives concrete thresholds: feet ≤3 cm high and ≤1 cm/frame at 180 cm; self-contact when nearest vertices are <0.2 cm apart or velocities agree (cos > 0.9). For self-contact on a different body, the key cheap idea is **egocentric normalisation** (Molla et al. 2018): express the hand's position relative to the touched body part, normalised by the *performer's* body, then map it onto the *target* body.

### Cited Findings
**Foot planting**
- **Unreal Speed Planting**:
  - `speed_curve_name` is a source-animation curve holding the end-effector speed.
  - `speed_threshold` (range 0–100, default 15) is "The maximum speed a source bone can be moving while being considered 'planted'". The target IK goal "will not be allowed to move whenever the source bone speed drops below this threshold".
  - On unplant, a spring with `unplant_stiffness` and `unplant_critical_damping` (0 = none, 1 = critically damped, no oscillation) pulls the goal back to its retargeted position.
  
  — [Python API TargetChainSpeedPlantSettings 5.4](https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/TargetChainSpeedPlantSettings?application_version=5.4)
- Epic's workflow:
  1. Generate speed curves with a Motion Extractor Modifier per foot bone (Translation Speed, XYZ). "the lower flat parts of the curve are where the foot is stationary, whereas the peaks are where it is in motion".
  2. Set the threshold "slightly higher than where the curve stays flat". The Japanese page's example uses 30.
  3. Add an IK goal on the end bone of each leg chain, attached to a Full Body IK solver.
  
  Foot sliding appears "when source and target leg lengths and stances differ a lot". — [UE: Fix Foot Sliding with IK Retargeter](https://dev.epicgames.com/documentation/en-us/unreal-engine/fix-foot-sliding-with-ik-retargeter-in-unreal-engine)
- **Stride warping** works only on goals that opt in via `affected_by_ik_warping`, typically on for feet and off for hands:
  - Warp Forwards <1 shortens strides, >1 lengthens them.
  - Warp Splay <1 pulls goals toward their average location.
  - Sideways Offset pushes goals apart or together.
  - Direction Source is "Goals" for bipeds.
  
  In 5.6+ it is a Stride Warp op with its own `alpha`. — [UE 5.3 IK Rig Retargeting](https://dev.epicgames.com/documentation/en-us/unreal-engine/ik-rig-animation-retargeting-in-unreal-engine?application_version=5.3); [IKRetargetStrideWarpingOpSettings 5.7](https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/IKRetargetStrideWarpingOpSettings?application_version=5.7)
- **Contact-aware retargeting (Villegas et al., ICCV 2021), foot contact heuristics**:
  - Toe contact = height ≤ 3 cm AND frame-to-frame displacement ≤ 1 cm, at a 180 cm character scale.
  - Heel contact = displacement ≤ 1 cm only, because height thresholds were unreliable with high-heel captures.
  - A 5-frame sliding window fills in contact when more than half the frames are in contact.
  - Foot energy: E_foot = Σ (1/h)(‖ġ_j‖² + ‖g_jʸ‖²) over contact joints, i.e. zero velocity and zero height for planted joints, normalised by height h.
  
  — [Contact-Aware Retargeting (ar5iv full text)](https://ar5iv.labs.arxiv.org/html/2109.07431)

**Self-contact (hands on body or each other) and interpenetration**
- Villegas et al.: "Self-contacts, such as when hands touch each other or the torso or the head" are "important attributes of human body language", and "existing methods do not model or preserve these contacts". Typical artifact: "a hand passing into the torso". — [arXiv abstract 2109.07431](https://arxiv.org/abs/2109.07431)
- Their self-contact detection:
  - Vertices are grouped per joint by skinning weight > 0.5.
  - A hand group is tested against other body groups with a triangle BVH.
  - Two groups are in contact if the average cosine similarity of their per-vertex global velocities is > 0.9 OR their nearest vertices are < 0.2 cm apart.
  - The top 3 closest vertex pairs are kept per contact.
  - Source-to-target vertex correspondence uses a KD-tree on (skinning weights + offset from parent joint in reference pose).
  - Test-time optimisation only enforces a contact if the initial output is within 15 cm of the target contact point.
  
  — [ar5iv full text](https://ar5iv.labs.arxiv.org/html/2109.07431)
- Their interpenetration term uses cone distance fields over colliding triangles, weighted by geodesic distance, so slight penetrations at the underarms and crotch "are not objectionable". In an ablation, the strict version without geodesic weighting cut interpenetration to 0.35 but raised contact error to 7.94 and produced stiff motion. Contact-only modelling risks a hand passing *through* the torso to reach a contact; penetration-only modelling loses meaningful contacts. Bulky characters showed about twice the interpenetration and contact error of skinny ones. — [ar5iv full text](https://ar5iv.labs.arxiv.org/html/2109.07431)
- **Egocentric mapping (Molla, Galvan Debarba, Boulic, IEEE TVCG 2018)** proposes "an egocentric normalization of the body-part relative distances to preserve the consistency of self contacts for a large variety of human-like target characters". It uses "a low-cost constraint relaxation technique relying on analytic inverse kinematics", fast enough for online performance animation. The user-study advantage is largest "when the proportions of the animated character deviates from those of the performer". — [EPFL Infoscience record 229912](https://infoscience.epfl.ch/record/229912)
- The earlier poster (Egocentric Normalization of Kinematic Path, EG 2015) normalises vectors from body-surface points to the hand, which "allows for morphological adaptation of these vectors onto the target characters, hence preserving the semantics in postures with and without body-contact". — [EG 2015 poster PDF](https://diglib7.eg.org/bitstream/handle/10.2312/egp.20151037.013-014/013-014.pdf?sequence=1)
- A 2023 EGVE follow-up (Delahaye, Herbelin, Boulic) extends this to finger-level self-contact. It describes the 2017/2018 work as "based on a crude approximation of the user's body surface". — [EGVE 2023 PDF](https://diglib7.eg.org/bitstream/handle/10.2312/egve20231311/037-044.pdf)

**Hands on props / other characters**
- Gleicher's constraints come from the character, the environment and the motion, for example feet touching the ground in a walk or hands holding a ladder rung while climbing. The paper highlights hand connections being kept across size changes. — [WPI summary](https://davis.wpi.edu/~matt/courses/retarget); [SIGGRAPH History](https://history.siggraph.org/?p=116535)
- 3ds Max Character Studio already "adjust[ed] keyframes to maintain footplants and balance" when re-applying motion to a new character (as noted in Gleicher's preprint). — [Gleicher preprint](https://research.cs.wisc.edu/graphics/Courses/cs-838-1999/Papers/retarget-preprint.pdf) (via search excerpt)

### Inferences
- **Foot plant detector for the tool**:
  - Planted if speed < threshold (start near 15–30 cm/s scaled to the performer, per Unreal) AND height above the calibrated floor < ~3 cm × (performer height / 180 cm) for toes (Villegas thresholds).
  - Detect toe and heel separately (heel by velocity only).
  - Close gaps with a 5-frame majority filter.
  - Add hysteresis or minimum durations so plants don't flicker.
  - While planted, hold the goal at its retargeted position from the first plant frame. On release, spring or ease back to the free retargeted goal (critically damped, like Unreal's unplant spring) rather than snapping.
- **Self-contact recipe**, a cheap version of Molla's idea using capsules instead of a mesh:
  1. Model the performer and target bodies as a handful of capsules or landmarks: head/chin, chest, belly, hips L/R, thighs, the other hand.
  2. Per source frame, for each hand, find the nearest landmark and its distance d.
  3. If d < τ (a few cm, normalised by performer size), mark a contact and record the hand position in that landmark's local frame, normalised by the landmark's size (e.g. capsule radius and length).
  4. On the target, de-normalise with the target landmark's size, transform to world space, and use it as the hand IK goal with weight w → 1, eased in and out over ~3–6 frames.
  
  This keeps "hands on hips" on the NWN hips even though its shorter arms would otherwise leave them in the air or in the belly.
- **Clap / hands together**: when the source hand-to-hand distance is under a threshold, set both hand goals around the midpoint of the two target FK hand positions, offset by the source separation. This conserves symmetry and avoids one arm doing all the reaching.
- **Guard at the chin**: anchor the hand to the head/chin landmark rather than to the shoulder. A short NWN torso changes the shoulder-to-chin vector, so rotation copying alone misplaces the fist.
- **Two-handed weapon / prop**: treat the off-hand as a contact in the main hand's (or prop's) frame. Solve the main hand by FK or reach, then put the off-hand at its grip offset, kept constant over the clip. This is the same "re-impose the source constraint" logic as Gleicher's hand connections.
- **Interpenetration guard**: after IK, push hand and forearm capsules out of torso and thigh capsules. Tolerate small penetrations at the armpits and crotch (Villegas' geodesic weighting rationale); a strict no-penetration rule stiffens the motion.
- **Gate contacts by distance**: only enforce a contact when the target's FK hand is within ~15 cm (scaled) of the goal, as Villegas' 15 cm gate does. Otherwise the arm makes large, unnatural reaches for incidental near-touches.

### Gaps
- Exact Molla et al. 2018 normalisation formulas (which body-surface points, how weights blend multiple references) were not extracted; only abstracts and poster blurbs were read.
- No concrete source describes how commercial tools (HumanIK, Unreal) handle hand-to-body self-contact. HumanIK's per-effector Reach applies only toward the *source position*, not body-relative positions. That is an observation from the docs read, not a documented limitation.
- Unreal's Speed Threshold units (cm/s presumably) are not stated on the pages read.

---

## Q4. Different arm and leg length ratios: when to keep rotations vs re-solve positions, reach effectors, scaling hand positions relative to the shoulder

### Takeaway
Keep rotations by default. They preserve style, and on different proportions they place end effectors proportionally farther or nearer. Re-solve positions only where a *relationship* matters: a foot on the floor, a hand on the body, a hand on a prop. When you do re-solve, compute the goal *relative to the target chain's root* (hip or shoulder), with the reach scaled by the limb-length ratio. Unreal encodes this as a goal that `extension` slides between the chain origin and the end. HumanIK's Reach blends from the FK position to the absolute source position.

### Cited Findings
- Unreal IK goal `extension` "Brings IK goal closer (0) or further (1+) from origin of chain". At 0 the effector sits at the chain origin (shoulder or hip), at 1 at the retargeted end, and >1 stretches the limb. `blend_to_source` = 1 moves the goal to the exact *source* bone location instead. — [TargetChainIKSettings](https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/TargetChainIKSettings?application_version=5.4)
- Unreal recommends IK goals to "stabilize limbs or chains which may not be retargeting with good accuracy", and Interpolated rotation mode for chains with different proportions or bone counts. — [UE 5.3 IK Rig Retargeting](https://dev.epicgames.com/documentation/en-us/unreal-engine/ik-rig-animation-retargeting-in-unreal-engine?application_version=5.3)
- HumanIK Reach at 50% "averages between the source position and its own". Max Extension (default 50) damps limbs near full extension "to prevent knee/elbow snapping". — [Maya HumanIK Retarget Specific attributes](https://help.autodesk.com/cloudhelp/ENU/MayaCRE-CharacterAnimation/files/GUID-EB2A8BB0-801E-4251-8C4C-09986049CCBB.htm)
- Unity stores hand and foot IK goals relative to the body transform, and allows limited limb stretch (`armStretch`, `legStretch`) "when using IK". — [Unity Root Motion manual](https://docs.unity.com/en-us/engine/current/manual/animation-section/animation-mecanim/avatar-creationand-setup/root-motion); [HumanDescription](https://docs.unity3d.com/450/Documentation/ScriptReference/HumanDescription.html)
- Ubisoft's IK Rig (Bereznyak, GDC 2016) was built "to share any animations between any characters". An 80.lv write-up says character scale and proportions "can now be changed", and that captured animation otherwise "only work[s] on this rig", which limits proportion variety. — [GDC Vault: IK Rig: Procedural Pose Animation](https://gdcvault.com/play/1023279/IK-Rig-Procedural-Pose); [80.lv article](https://80.lv/articles/how-ubisoft-revolutionises-animation-in-games)
- Choi & Ko's online method tracks end-effector trajectories while minimising joint-angle deviation, i.e. it keeps rotations except where effector positions must hold. — [IEEE Xplore 803346](https://ieeexplore.ieee.org/document/803346)
- Gleicher lists per-frame IK's drawbacks: it "Does not consider the motion as a whole", gives "undesirable artifacts, e.g. jerkiness", and has "No frame-to-frame consistency". Spacetime solves can "look-ahead or behind". The full spacetime solve needed ">4000" constraint equations for a simple walk and ignores physics. — [WPI summary of Gleicher 1998](https://davis.wpi.edu/~matt/courses/retarget)

### Inferences
- **Decision rule for the NWN body**:
  - Legs: always FK + IK on plants. Long legs make copied rotations overshoot or undershoot the floor in proportion to leg length, so plants need IK.
  - Spine/neck: FK only, Interpolated mapping if bone counts differ.
  - Arms: FK by default, switching to hand IK only during detected contacts (Q3) or prop grips.
  - Free gestures (waving, pointing) stay FK. Hand position relative to the shoulder then scales by arm length, which is fine.
- **Shoulder-relative hand goal** (for reach-type retargeting):
  `goal = T_shoulder + (S_hand − S_shoulder) × (L_target_arm / L_source_arm)`
  - Express the vector in the torso frame, so a shorter NWN torso changes where the shoulder is but not the direction of the reach.
  - Clamp |goal − T_shoulder| to ≤ ~0.98 × L_target_arm, or use a soft-IK falloff (cf. HumanIK Max Extension) to avoid elbow snapping.
  - Leg goals: `goal = T_hip + (S_foot − S_hip) × (L_target_leg / L_source_leg)`, then override vertical on plant frames with floor + sole offset.
- **Lower-jitter per-frame IK**: compute IK per frame, take the *displacement* from the FK-copy pose (joint-angle deltas or goal deltas), low-pass filter it over time, and add it back. This keeps high-frequency detail from the copied rotations while adding smooth corrections. It mirrors Gleicher's smooth B-spline displacement idea and Choi & Ko's "minimise deviation" objective, but it is the author's synthesis, not a quoted method.
- A short torso with copied spine rotations puts the hands lower and closer to the chest centre. Expect more hand-in-belly penetration in crossed-arm and hands-on-chest poses, and therefore more need for the Q3 capsule push-out.

### Gaps
- Unreal's exact formula for the "retargeted location" of an IK goal (whether source end position is normalised by source chain length and re-scaled by target chain length from the target chain start) is not stated in the docs fetched, though `extension` semantics imply chain-origin-relative goals.
- The Bereznyak GDC talk content was gated, so the IK Rig's specific proportion-scaling rules are not available.

---

## Q5. Bind pose / T-pose / A-pose differences, characterisation, per-bone rotation offsets

### Takeaway
Retargeting copies rotations *relative to a shared reference pose*. If source and target reference poses differ (A-pose vs T-pose, bent vs straight elbows, different bone axis conventions), every frame inherits a constant error unless a "retarget pose" or "characterisation" step aligns them first. Unreal makes this an editable Retarget Pose with auto-align. HumanIK characterises in a T-stance. Rokoko requires both armatures in the same pose.

### Cited Findings
- Unreal: pose differences such as an A-pose source and T-pose target are resolved in Edit Retarget Pose mode. "Matching these retarget poses will increase the accuracy of the retargeting." **Auto Align** (All / Selected / Selected and Children) offers methods Direction, Local Rotation Axes, Global Rotation Axes and Mesh. **Retarget Pose Blend** previews from the reference pose (0) to the retarget pose (1). Poses can be imported from a sequence frame (default 0) or a Pose Asset. — [UE: IK Rig Animation Retargeting](https://dev.epicgames.com/documentation/en-us/unreal-engine/ik-rig-animation-retargeting-in-unreal-engine); [UE 5.3 IK Rig Retargeting](https://dev.epicgames.com/documentation/en-us/unreal-engine/ik-rig-animation-retargeting-in-unreal-engine?application_version=5.3)
- Unreal **Override Sets** allow state-driven retarget variations, including a different retarget pose via an "Override Retarget Pose" op. Anim curves can drive retarget properties at runtime. — [UE: IK Rig Animation Retargeting](https://dev.epicgames.com/documentation/en-us/unreal-engine/ik-rig-animation-retargeting-in-unreal-engine)
- HumanIK references the characters' "default T-stances", for example when the source and target ankles sit at different heights above the floor. — [HumanIK SDK: Retargeting Solving](https://help.autodesk.com/cloudhelp/2016/ENU/HumanIK-SDK-Help/files/GUID-6987D1B4-229F-4EE0-978C-DC2D0C807D2E.htm)
- Unity Body Orientation is "an average of the lower and upper body orientation relative to the Avatar T-Pose". — [Unity Root Motion manual](https://docs.unity.com/en-us/engine/current/manual/animation-section/animation-mecanim/avatar-creationand-setup/root-motion)
- Rokoko: "both armatures need to be in the same pose", chosen via Use Pose (Rest / Current). — [Rokoko support](https://support.rokoko.com/hc/en-us/articles/4410463481489-Retarget-an-animation-in-Blender). Auto-Rig Pro remaps using bone names and "bones original orientation". — [Superhive: Auto-Rig Pro](https://superhivemarket.com/products/auto-rig-pro)
- Unreal FK "Pole Vector Matching" with `pole_vector_maintain_offset` keeps the original source/target pole offset. This is a per-chain twist offset for mismatched knee/elbow axes. — [TargetChainFKSettings](https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/TargetChainFKSettings?application_version=5.4)

### Inferences
- **Per-bone offset math** (standard practice, author's statement, not quoted from a source). With global (model-space) rotations, R_src_rest/R_tgt_rest from the *aligned* retarget poses:
  - Global-delta copy: `R_tgt(t) = R_src(t) · R_src_rest⁻¹ · R_tgt_rest`. Convert to local via the target parent: `r_tgt_local = R_tgt_parent(t)⁻¹ · R_tgt(t)`.
  - This "global delta" form is insensitive to differing local bone-axis conventions. Copying local rotations directly is only safe when both rigs share axis conventions and rest pose.
- **Characterisation step for the tool**:
  1. Pick or construct a source frame in the target's rest stance, or pose the target into the performer's calibration stance.
  2. Store R_src_rest and R_tgt_rest per mapped bone.
  3. Offer an "align direction" helper that rotates each target bone so its bone vector matches the source bone vector (like Unreal's "Direction" auto-align).
  4. Store sole-to-floor and hip heights here too (Q2).
- An A-pose/T-pose mismatch that is not aligned produces a constant arm error of tens of degrees throughout the clip. With NWN's short arms it shows up as hands sunk into the thighs or flared out. Check this first when "all clips look wrong the same way".

### Gaps
- The NWN humanoid's actual rest pose (T or A, elbow bend) and local bone axes were not researched here.
- MotionBuilder's characterisation-pose requirements were not fetched.

---

## Q6. Visualising the source over the target to judge the retarget

### Takeaway
Unreal shows source and target side by side with preview-only mesh offset/scale, debug draw for ops (pelvis, IK goals, chains), a retarget-pose blend slider and hierarchy filters. Contact-aware research measures quality with contact error and interpenetration metrics. For a home tool, the useful overlays are a source skeleton scaled by the hip-height ratio and planted at the target's root, plus goal and contact markers.

### Cited Findings
- Unreal Asset Settings cover "mesh offsets and scale (preview only), skeleton visibility, Debug Draw", and "Ignore Root Lock in Preview", which "affects only the preview". Hierarchy filters: Hide Bones Not in a Chain / Not Retargeted / Retargeted. — [UE: IK Rig Animation Retargeting](https://dev.epicgames.com/documentation/en-us/unreal-engine/ik-rig-animation-retargeting-in-unreal-engine)
- The Pelvis Motion op has its own debug draw (`enable_debug_draw`, `debug_draw_size`, `debug_draw_thickness`). — [IKRetargetPelvisMotionOpSettings 5.6](https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/IKRetargetPelvisMotionOpSettings?application_version=5.6); [FIKRetargetPelvisMotionOpSettings](https://dev.epicgames.com/documentation/unreal-engine/API/Plugins/IKRig/FIKRetargetPelvisMotionOpSetting-)
- Retarget Pose Blend (0 = reference pose, 1 = retarget pose) is a visual check of alignment. — [UE: IK Rig Animation Retargeting](https://dev.epicgames.com/documentation/en-us/unreal-engine/ik-rig-animation-retargeting-in-unreal-engine)
- Villegas et al. evaluate with interpenetration, vertex-contact MSE and foot-contact accuracy. For example, SAN scored interpenetration 1.43 and foot accuracy 0.63, and their method 0.81 interpenetration and 3.87 contact MSE. These are useful as the *kinds* of numeric checks to report. — [ar5iv full text](https://ar5iv.labs.arxiv.org/html/2109.07431)
- In Epic's speed-curve workflow, plotting the per-foot speed curve shows plants as "lower flat parts". This is a direct visual for tuning thresholds. — [UE: Fix Foot Sliding](https://dev.epicgames.com/documentation/en-us/unreal-engine/fix-foot-sliding-with-ik-retargeter-in-unreal-engine)

### Inferences
- **Overlay modes for the tool**, with a toggle:
  - (a) Raw source at true scale, pelvis-aligned. Shows the proportion gap.
  - (b) Source uniformly scaled by the hip-height ratio, anchored at the floor under the target pelvis. Matches what root scaling assumes, so foot-position differences in this view are what IK must fix.
  - (c) "Per-segment" view: source bone directions drawn with target bone lengths, i.e. the pure FK-copy result before IK. Diffing (c) against the final pose shows exactly what IK and contacts changed.
- Draw the extra layers:
  - IK goals as spheres, coloured by weight w.
  - Detected contacts on a timeline track (feet: plant bars; hands: contact bars naming the landmark).
  - Lines from each hand to its anchor landmark during contacts.
  - A floor-penetration / float readout per foot (sole height in cm).
  - Knee/elbow extension % (to catch snapping).
- Per-clip numeric report, as in the research metrics:
  - max foot slide during plants (cm)
  - max sole penetration or float (cm)
  - hand-contact error (cm)
  - frames with hand/torso capsule penetration
  - max limb extension

### Gaps
- No primary source was found on how HumanIK/MotionBuilder or Blender add-ons draw the source skeleton over the target (scaled by height vs hips vs per segment). The overlay modes above are the author's proposals, not documented tool features.

