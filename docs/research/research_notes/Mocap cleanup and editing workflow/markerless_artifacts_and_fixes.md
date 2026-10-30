# Markerless (video/webcam) mocap: typical artifacts and standard cleanup techniques

Research notes, 2026-10-30. Scope: body motion (hands as a bonus), low-cost markerless capture (FreeMoCap-style multi-webcam rigs and commercial video-to-mocap services), with parameters concrete enough to build a JavaScript cleanup tool. Provenance: items marked "(search snippet)" come from a search-engine summary of the page and were not checked against the full page text. Everything else was read from the cited page.

---

## 1. What artifacts do markerless systems produce, and which are worst for fast combat moves?

### Takeaway
The documented artifacts are high-frequency jitter (mostly small, plus rare long bursts that come with large biased errors), foot sliding caused by errors in the global trajectory and foot joints, limb or orientation flips during fast rotation, motion blur on fast limbs, interpenetration, weak hand and foot keypoints, imprecise deep hip/knee flexion, and calibration or scale errors. For combat, the worst are blur and dropouts on the striking limb, skeleton flips during spins, wrong foot-lock decisions while a leg leaves the ground, and filters cutting off the impact peak. Most of these get worse below about 60 fps.

### Cited Findings
- **Jitter is unevenly distributed.** "Most frames suffer from slight jitters while long-term significant jitters will be accompanied by large biased errors." The same paper notes sudden flips when poses are represented as angle-axis or quaternions. On AIST++ (dance) with the SPIN estimator, the worst-1% acceleration error is 195.48 (SmoothNet paper; the summary gives Accel in mm/frame²). — [SmoothNet, Zeng et al. (arXiv 2112.13715)](https://ar5iv.labs.arxiv.org/html/2112.13715)
- **Raw per-frame estimates are very jittery.** A 3D pose estimator (FCN) on Human3.6M had acceleration error 19.17 against 1.03–3.80 after smoothing, while MPJPE (position error) barely moved (54.55 → 52.7–55.2 mm). So jitter, not average position error, is the dominant artifact. — [SmoothNet](https://ar5iv.labs.arxiv.org/html/2112.13715)
- **Foot sliding in video-to-mocap "arises from errors in predicting global trajectory and joint positions, especially in the feet."** — [arXiv 2601.19036 (2026 physics-based refinement paper)](https://arxiv.org/html/2601.19036v1) (search snippet)
- **Sparse, noisy keypoints make joint angles hard to estimate.** OpenCap's follow-up paper reports that raw video keypoints gave mean 9.6° and max 43.1° joint-angle error on benchmark movements, against 5.3°/11.5° for the original marker enhancer and 4.1°/8.7° for the new one. The original enhancer did much worse on movements outside its training set (the abstract snippet gives mean 40.4° and max 252.0° in the unseen-movement comparison). — [Falisse/Uhlrich et al., "Marker Data Enhancement for Markerless Motion Capture" (PMC11275905)](https://pmc.ncbi.nlm.nih.gov/articles/PMC11275905) (search snippet)
- **Deep flexion is flagged as unreliable.** Pose2Sim's kinematics config treats "Hip and knee angles beyond [90°]" as "imprecise" and drops those frames when computing segment lengths (`large_hip_knee_angles = 90`). Chambered kicks, knee strikes and deep stances sit in that range. — [Pose2Sim Config.toml](https://raw.githubusercontent.com/perfanalytics/pose2sim/main/Pose2Sim/Demo_SinglePerson/Config.toml)
- **Left/right swaps:** Pose2Sim's `handle_LR_swap` option is listed as "Not implemented yet", so a leading open-source multi-camera pipeline leaves L/R swaps uncorrected. — [Pose2Sim README](https://github.com/perfanalytics/pose2sim)
- **Below 60 Hz, accuracy drops, "especially for fast motion"** (Pose2Sim recommends at least 60 fps). — [Pose2Sim README](https://github.com/perfanalytics/pose2sim)
- **Fast rotation:** "Spins, flips, and whip-fast turns blur the silhouette across frames and cause the skeleton to flip orientation mid-move." The suggested fix is a higher frame rate, or performing slightly slower and retiming. — [invideo / Uthana AI-mocap guides](https://invideo.io/blog/ai-motion-capture/) (search snippet; the attribution between these two pages is uncertain)
- **Single-camera tools lose precision on fast motion and heavy occlusion.** — [Kreatli tool roundup](https://kreatli.com/blog/best-ai-powered-motion-capture-tools) (search snippet)
- **QuickMagic loses accuracy with low light, compression and motion blur** — [Automateed review](https://www.automateed.com/quickmagic-review) (search snippet). It "may struggle to accurately capture movements where hands are planted on the ground," because it is optimised for foot placement — [CGDive review](https://cgdive.com/quickmagic-review-high-quality-ai-motion-capture/)
- **Interpenetration:** DeepMotion says mocap "often exaggerates movement," which "can occasionally result in joint penetration or crossovers." Its Physics Filter targets self-collisions and clipping. — [DeepMotion Physics Filter](https://deepmotion.com/article/physics-filter)
- **Foot sliding varies by vendor.** A comparison says Move AI is the most accurate for complex poses but "sometimes shows more foot sliding" than QuickMagic. — [TATO Studio comparison](https://tato.studio/blog/best-ai-video-to-mocap) (search snippet)
- **Foot-lock heuristics misfire on fast leg motion.** DeepMotion recommends its "Grounding" mode for when "Auto locks the feet too long during fast leg movements." That is exactly the kick or step-in case. — [DeepMotion Foot Locking](https://deepmotion.com/article/foot-locking)
- **Hands and fingers are weak.** Rokoko Vision 3.0 launched without finger tracking — [Neolemon roundup](https://www.neolemon.com/blog/best-ai-motion-capture-tools-for-character-animation/) (search snippet). One open-source project's code comments report raw WiLoR finger jitter at 11× a real hand's, reduced to 1.4× by a Butterworth smoother with "wrist error unchanged" — [s&box HumanoidMocap CleanupSettings.cs](https://sbox.game/notpointless/chomnr_humanoid_mocap/source?file=HumanoidMocap%2FCore%2FMotion%2FCleanupSettings.cs) (anecdotal, single project)
- **Heel and toe keypoints carry weight downstream.** Pose2Sim's LSTM marker augmentation needs ankles, heels, toes, hips and knees, and fails if NaNs remain. — [Pose2Sim README](https://github.com/perfanalytics/pose2sim)
- **Calibration and scale bounds:** Pose2Sim wants intrinsic reprojection error "close to or below 0.5 px" and extrinsic error below 1 cm (acceptable up to 2.5 cm). Scaling depends on a correct `participant_height` (default 1.7 m). — [Pose2Sim README](https://github.com/perfanalytics/pose2sim), [Config.toml](https://raw.githubusercontent.com/perfanalytics/pose2sim/main/Pose2Sim/Demo_SinglePerson/Config.toml). Move AI asks for a calibration score below 10 px. — [Move AI docs](https://docs.move.ai/knowledge/what-is-a-good-pixel-score-for-a-calibration-1) (search snippet)
- **Filters cut impact peaks in kicks.** In soccer instep-kick studies, conventional low-pass filters underestimated peak foot accelerations by 30–70% around ball impact. — [Augustus et al., J Biomech 2020](https://eprints.chi.ac.uk/5020) (search snippet)

### Inferences
- **Severity ranking for combat clips (kicks, punches, shoves).** This is synthesised from the findings above.
  1. Blur and dropout on the striking limb. The wrist or ankle is the fastest point, so it is blurred and its detection confidence collapses at full extension.
  2. Peak loss from filtering, made worse by low fps.
  3. Foot-lock misfires: a pivot or support foot that slides, or a kicking foot locked too long.
  4. L/R swaps during spins, side-on stances and crossed legs.
  5. Deep-flexion errors on chambered knees.
  6. Penetration on punches crossing the body.

  Steady-state jitter is the easiest artifact to fix and matters less in fast clips.
- At 13–30 fps a punch's extension (roughly 100–200 ms; this is my estimate, not sourced) spans only about 2–6 frames. One dropped or blurred frame removes a large share of the strike, so gap-filling and peak handling matter more than smoothing.
- **Depth errors.** Monocular tools must infer depth, so expect forward/back errors on strikes toward the camera. I found no source quantifying this (see Gaps).

### Gaps
- No source quantified depth (z) error for monocular tools or L/R swap rates for 2–3-webcam rigs.
- No source measured motion-blur effects at webcam shutter speeds. Webcams usually use auto-exposure with long shutters indoors; this is unverified.
- No user reports specifically evaluating markerless output for kicks or punches were found (see section 6).

---

## 2. Filtering: Butterworth, One Euro, Savitzky-Golay, Gaussian — parameters and trade-offs

### Takeaway
The biomechanics default is a 4th-order zero-lag Butterworth (a 2nd-order filter run forward and backward) at about 6 Hz for gait. Fast movement calls for 6–15 Hz, and impulsive movement for more than 15 Hz. The cutoff must stay below about 0.25 × the sampling rate, which caps webcam rigs hard: about 3.75 Hz at 15 fps and 7.5 Hz at 30 fps. One Euro, an adaptive low-pass, suits jitter at rest because its cutoff rises with speed. Offline, it is less accurate than a zero-phase filter. Every low-pass blunts strike peaks; the impact literature shows 30–70% peak loss at cutoffs below 20 Hz.

### Cited Findings
**Butterworth (zero-lag) defaults and cutoffs**
- **Pose2Sim defaults:** Butterworth `cut_off_frequency = 6` Hz, `order = 4`, commented "3-6 Hz: Walking, slow movements; 6-15 Hz: Running, fast movements". Its other filters:
  - `Butterworth_on_speed` at 10 Hz, order 4
  - `acc_minimizing` at 6 Hz (used for inverse kinematics output)
  - Gaussian `sigma_kernel = 1`
  - Median `kernel_size = 3`
  - LOESS `nb_values_used = 5`
  - Kalman `trust_ratio = 500`, `smooth = true`
  - GCV spline with `cut_off_frequency = 'auto'`

  — [Pose2Sim Config.toml](https://raw.githubusercontent.com/perfanalytics/pose2sim/main/Pose2Sim/Demo_SinglePerson/Config.toml)
- **Pose2Sim README:** "15+ Hz for very fast or impulsive movement." `acc_minimizing` "is smooth but blunts peaks." GCV spline auto-cutoff "can be buggy" for a triangular wave plus drift. — [Pose2Sim README](https://github.com/perfanalytics/pose2sim)
- **Pose2Sim's Butterworth "is sensitive to outliers and can ring at gaps."** Each continuous sequence is filtered separately, provided it meets a minimum length. — [DeepWiki: Pose2Sim data filtering](https://deepwiki.com/perfanalytics/pose2sim/3.4-data-filtering)
- **Users find Pose2Sim's built-in filter weak:** it is "not particularly strong and the data can be still noisy." — [FLESH Effort processing notebook](https://sarkadava.github.io/FLESH_Effort/03_TS_processing/01_TS_processing_motion.html) (search snippet)
- **FreeMoCap's default post-processing filter is a Butterworth at 6 Hz, order 4.** Its `FilterConfig.sampling_rate` was hard-coded at 30.0, so the effective cutoff was `requested × true_fps / 30`: 12 Hz at 60 fps, 3 Hz at 15 fps. With the correct clock on a 60 fps take, above-8 Hz noise fell about 90% (wrists 5.40 → 0.50 mm RMS, ankles 4.51 → 0.39 mm), while the 0–6 Hz motion band changed about 0.01%. The fix shipped in PR #861 / v2.0.0-alpha.21 (closed Aug 11, 2026). — [FreeMoCap issue #849](https://github.com/freemocap/freemocap/issues/849)
- **An older FreeMoCap (1.2.2, July 2024) log shows** `sampling_rate=30.0, cutoff_frequency=7.0, order=4, run_butterworth_filter=True`. This conflicts with the 6 Hz in #849, probably a version change. — [FreeMoCap issue #620](https://github.com/freemocap/freemocap/issues/620) (search snippet)
- **Two-pass correction:** practitioners use "a two pass 2nd order Butterworth filter with correction for the cutoff (TwoPassFactor = 1/.802)."
- **Upper limit on cutoff:** "Once beyond (0.25 * SAMPLING_RATE), the Butterworth digital filter behaves strangely."
- **Segment-specific cutoffs:** trunk markers 1–3 Hz, feet 8–15 Hz depending on activity.
- **Yu's sampling-rate rule:** `Fc = (1.4845 + 0.1523·Fs^½)²`, said to explain "over 75%" of the variation in optimal cutoff.
- **Raise the sampling rate when accelerations matter:** "Where the accelerations become very important, we find that it's often worthwhile to raise the sampling frequency."
- **Winter's heel-marker spectrum** has ">99% of the power … below the 7th harmonic."

  — all six items above from [Clinical Gait Analysis FAQ: choice of cut-off frequency](https://clinicalgaitanalysis.com/faq/cutoff.html)
- **Running is higher.** In stance-phase running data, residual analysis gave an optimal 10 Hz, while spectral analysis put 99% of signal power at 25 Hz and 95% at 10 Hz. — [ISBS proceedings (Konstanz OJS)](https://ojs.ub.uni-konstanz.de/cpa/article/view/2197/2055) (search snippet; exact paper not opened)
- **Residual analysis tends to choose cutoffs that are too low.** Yu et al. (1999) found residual-analysis cutoffs "significantly lower than the optimum, especially when the sampling frequency is high" — [Yu et al. 1999](https://acnpsearch.tweb-dev.unibo.it/singlejournalindex/63953635) (search snippet). A widely used implementation notes it "results in oversmoothing" — [Duarte, ResidualAnalysis notebook](https://nbviewer.jupyter.org/github/demotu/BMC/blob/master/notebooks/ResidualAnalysis.ipynb) (search snippet)
- **Automatic cutoff (Kinovea):** a 2nd-order Butterworth run forward and backward, with the cutoff chosen per trajectory where the residuals are least autocorrelated (the Challis approach). — [Kinovea kinematics docs](https://kinovea.org/help/en/measurement/kinematics.html)

**Impact and strike peaks**
- In soccer instep kicking, filters with cutoffs below 20 Hz "attenuated motion near impact," while cutoffs above 70 Hz retained lower-leg motion. Conventional filters underestimated peak accelerations by 30–70%. Truncating data at impact gave valid pre-impact kinematics. A time-frequency (fractional Fourier) filter with a time-varying cutoff that rises during impact performed best. — [Augustus et al. 2020 (J Biomech)](https://eprints.chi.ac.uk/5020) (search snippet)
- In Nunome et al. 2006, the Butterworth "failed to remove noisy oscillation from the baseline," other smoothers "lost the peaks of rapid changes," and a time-frequency filter worked better. — [Nunome 2006, impact-phase kinematics of instep kicking](https://people.stfx.ca/smackenz/courses/HK474/Midterm/Nunome%202006%20Impact%20phase%20kinematics%20of%20instep%20kicking%20in%20soccer.pdf) (search snippet)

**One Euro filter (Casiez, Roussel, Vogel, CHI 2012, DOI 10.1145/2207676.2208639)**
- **Tuning:** set β = 0 and fcmin to "a reasonable middle-ground value such as 1 Hz". Adjust fcmin with the limb still or moving slowly until jitter is acceptable, then move fast and raise β, starting around 0.001 or 0.0001 and changing by factors of 10, until lag is acceptable. "Decreasing fcmin reduces jitter but increases lag." If high-speed lag is a problem, raise β; if slow-speed jitter is a problem, lower fcmin. — [Casiez 1€ filter page](https://gery.casiez.net/1euro/)
- **The cutoff adapts as `min_cutoff + beta × |speed|`**, with a separate `d_cutoff` for the low-passed derivative. — [MyoGestic filter API docs](https://nsquaredlab.github.io/MyoGestic/api/filters/) (search snippet)
- **Example and JS port:** one Python package's example uses freq 120, mincutoff 1.0, beta 0.1, dcutoff 1.0 — [oneeurofilter (PyPI)](https://pypi.org/project/oneeurofilter). A TypeScript/JS implementation exists on npm — [1eurofilter (npm)](https://www.npmjs.com/package/1eurofilter)
- **MediaPipe Pose's built-in One Euro parameters:**
  - Image landmarks: `min_cutoff 0.05, beta 80, derivate_cutoff 1.0` ("near-zero smoothing for static points, ~0.94 for fast-moving ones")
  - World landmarks (meters): `min_cutoff 0.1, beta 40, derivate_cutoff 1.0, disable_value_scaling true`
  - Auxiliary ROI landmarks: `min_cutoff 0.01, beta 10` (heavier smoothing to keep the crop stable)
  - Visibility scores: low-pass `alpha 0.1`

  — [MediaPipe pose_landmark_filtering.pbtxt](https://raw.githubusercontent.com/google-ai-edge/mediapipe/master/mediapipe/modules/pose_landmark/pose_landmark_filtering.pbtxt)
- **Pose2Sim's One Euro preset:** `cut_off_frequency = 4` Hz, `beta = 1.5`, `d_cut_off_frequency = 1.0` Hz. — [Pose2Sim Config.toml](https://raw.githubusercontent.com/perfanalytics/pose2sim/main/Pose2Sim/Demo_SinglePerson/Config.toml)

**Savitzky-Golay and Gaussian against One Euro (offline 3D pose)**
- On Human3.6M, acceleration error / MPJPE after each filter was:
  - Savitzky-Golay: 1.34 / 53.48
  - Gaussian: 2.43 / 53.67
  - One Euro: 3.80 / 55.20
  - Learned SmoothNet: 1.03 / 52.72

  The settings matched on MPJPE were Savitzky-Golay window 31, polyorder 2; Gaussian σ 3, window 31; One Euro cutoff 0.04. The aggressive settings matched on accel were Savitzky-Golay window 257; Gaussian σ 4, window 129; One Euro cutoff 1e-4, β 0.7. Filters "suffer from over-smoothness when the kernel size is larger than 65." Low-pass filters struggle with high-frequency movements; the learned smoother handles them better. — [SmoothNet (arXiv 2112.13715)](https://ar5iv.labs.arxiv.org/html/2112.13715)
- **MMPose (the RTMPose framework)** ships an offline/online `Smoother`, configured e.g. `dict(type='GaussianFilter', window_size=3)`, with preset configs under `configs/_base_/filters/`. — [MMPose smoother source](https://mmpose-ly015.readthedocs.io/en/latest/_modules/mmpose/core/post_processing/smoother.html)
- **Practitioner advice:** over-filtering makes motion "floaty and robotic." Use adaptive filtering that smooths more at low velocities. — [MoCap Online cleanup workflow](https://mocaponline.com/blogs/mocap-news/mocap-data-cleanup-workflow) (search snippet)
- **DeepMotion's AI Motion Smoothing** "may result in slightly reduced animation accuracy for certain frames," so apply it selectively. — [DeepMotion Motion Smoothing](https://deepmotion.com/article/motion-smoothing)

### Inferences
- **Implementation recipe for a JS tool, offline and zero-phase.** These are my reasoning, built from the findings above.
  1. Run a 2nd-order Butterworth low-pass forward, then backward over the reversed output. Correct the design cutoff by `fc_design = fc_target / 0.802` so the two-pass −3 dB point lands on target. Pad or reflect about 3× the filter length at each end to limit edge transients.
  2. Filter each NaN-free run separately, as Pose2Sim does. Never filter across unfilled gaps.
  3. Clamp `fc_target ≤ 0.25·fps`, which is 3.75 Hz at 15 fps, 7.5 Hz at 30 fps, 15 Hz at 60 fps.
  4. Yu's rule gives a data-free starting cutoff: about 4.3 Hz at 15 fps, 5.4 Hz at 30, 7.1 Hz at 60, 9.9 Hz at 120 (my arithmetic). Cross-check with residual analysis or residual whiteness (Kinovea/Challis), remembering residual analysis tends to choose too low.
- **Per-joint cutoffs:** about 3–4 Hz for pelvis, trunk and head, and the highest the frame rate allows for wrists and ankles (feet 8–15 Hz per the Clinical Gait Analysis FAQ). A uniform 6 Hz is the main cause of mushy strikes.
- **Keeping strikes sharp offline.** Detect strike windows from the peak speed of the distal joint (e.g. wrist or ankle speed above an adaptive threshold such as median + 3·MAD). Then blend between a heavily filtered track (static and slow phases) and a lightly filtered or raw-but-outlier-cleaned track inside the window, crossfading over 2–3 frames. This is a simple stand-in for the time-varying-cutoff filter in the impact literature.
- **One Euro offline:** run it forward and backward and average to cancel its lag, or use it only for live preview. Offline it measured worse than SG or Gaussian (SmoothNet table), so the final bake should use zero-phase Butterworth or SG.
- **One Euro units:** β scales with units/s, so MediaPipe's β = 80 (normalised image units) and β = 40 (meters) are not interchangeable with Casiez's 0.001–0.1 starting range. Re-tune for the tool's unit system.
- **Savitzky-Golay:** keep the window short in frames for combat, about 5–9 frames at 30 fps (my suggestion), with polyorder 2–3. It preserves peaks better than a Gaussian of equal width. Windows past about 65 frames over-smooth (SmoothNet).
- **Smooth positions, then re-solve rotations.** Filtering positions before solving rotations avoids quaternion sign flips. If rotations are filtered directly, enforce quaternion hemisphere continuity (flip q when dot(q_t, q_{t−1}) < 0) first. This follows from the flip artifact SmoothNet reports.

### Gaps
- I did not open the original One Euro paper, so the exact α formula is unverified in this session. The commonly implemented form is `α = 1/(1 + τ/Te)` with `τ = 1/(2π·fc)`; verify against DOI 10.1145/2207676.2208639.
- No source gave cutoff values tuned specifically for punches or martial-arts kicks from markerless data. The soccer-kick impact studies used marker or high-speed data at sampling rates I did not confirm.
- MMPose's exact default One Euro and Savitzky-Golay parameters (in `configs/_base_/filters/`) were not retrieved.

---

## 3. Gap filling and outlier removal

### Takeaway
The open-source baseline (Pose2Sim) is a fixed order:
1. Reject low-confidence views (likelihood < 0.3) and high reprojection error (> 15 px).
2. Run a Hampel outlier filter (7-frame window, about 2σ / 95%).
3. Linearly interpolate gaps under 20 frames and leave or hold longer ones.
4. Filter each continuous run separately.
5. Enforce bone lengths with IK on a skeleton scaled from robust mean segment lengths.

### Cited Findings
- **Pose2Sim triangulation gates:**
  - `likelihood_threshold_triangulation = 0.3` ("if 2D likelihood is below, estimations for this camera won't be accepted")
  - `reproj_error_threshold_triangulation = 15` px
  - `min_cameras_for_triangulation = 2`
  - `interp_if_gap_smaller_than = 20` frames ("do not interpolate larger gaps")
  - `interpolation = 'linear'` (options linear, slinear, quadratic, cubic, none)
  - `fill_large_gaps_with = 'last_value'` (or 'nan', 'zeros')
  - `min_chunk_size = 10`
  - Person association: likelihood 0.3, reprojection error 20 px; multi-person reconstruction error 0.1 m

  — [Pose2Sim Config.toml](https://raw.githubusercontent.com/perfanalytics/pose2sim/main/Pose2Sim/Demo_SinglePerson/Config.toml)
- **Hampel outlier rejection is on by default** ("before other filtering methods. Can be slow"), with a 95% interval and a 7-frame window. Pose detections with average likelihood below 0.5 are dropped. If results are poor, first raise `min_cameras_for_triangulation`. — [Pose2Sim README](https://github.com/perfanalytics/pose2sim)
- **Pose2Sim's outlier step** replaces a value more than 2 standard deviations from the median with the median. Butterworth "can ring at gaps." — [DeepWiki: Pose2Sim data filtering](https://deepwiki.com/perfanalytics/pose2sim/3.4-data-filtering)
- **Bone lengths:** Pose2Sim scales the model from mean segment lengths. Before averaging it removes frames with hip or knee flexion above 90° and trims the 50% most extreme segment values (`trimmed_extrema_percent = 50`). It assumes `right_left_symmetry = true`, which can be disabled, e.g. for a prosthesis. — [Pose2Sim Config.toml](https://raw.githubusercontent.com/perfanalytics/pose2sim/main/Pose2Sim/Demo_SinglePerson/Config.toml), [README](https://github.com/perfanalytics/pose2sim)
- **Marker augmentation** (a Stanford LSTM, 47 virtual markers) gives "a more stable, but less precise output." It is mostly beneficial with fewer than 4 cameras and fails with NaNs present. — [Pose2Sim README](https://github.com/perfanalytics/pose2sim)
- **Joint-limit enforcement:** DeepMotion's Physics Filter reinforces joint limits and keeps movement "within realistic ranges," preventing unnatural stretching or bending. — [DeepMotion Physics Filter](https://deepmotion.com/article/physics-filter)
- **Visibility smoothing:** MediaPipe low-pass-filters per-landmark visibility (α = 0.1) separately from coordinates. — [MediaPipe pose_landmark_filtering.pbtxt](https://raw.githubusercontent.com/google-ai-edge/mediapipe/master/mediapipe/modules/pose_landmark/pose_landmark_filtering.pbtxt)

### Inferences
- **JS outlier and gap pipeline,** per joint, per axis (or on 3D speed). These are my reasoning.
  1. Mark frames with confidence below 0.3 as missing.
  2. Hampel filter: window k = 3 (7 frames). `med = median(window)`, `σ = 1.4826·MAD`. Flag if `|x − med| > 2·σ` (about 95%) and replace with NaN, so it can be re-interpolated, rather than with the median.
  3. Bone-length gate: flag a joint when a segment length deviates more than about 15–20% from the robust (trimmed-mean) length. The threshold is my suggestion, not sourced.
  4. Fill gaps of 20 frames or fewer (Pose2Sim). For a 30 fps webcam, about 6–10 frames may be the safer cap, because 20 frames is 0.67 s at 30 fps. Use cubic (Catmull-Rom or monotone Hermite) for positions, since linear produces velocity kinks. For longer gaps, hold or blend toward an IK-solved pose.
  5. Filter continuous runs.
  6. Fit to a fixed-length skeleton. Run FABRIK or CCD per chain to the filtered joint targets, or simply rescale each child vector to its canonical length from the root outward.
- **L/R swap detection:** a swap shows as a step change where left and right of a pair (ankles, knees, wrists) exchange positions. It can be detected when `|L_t − R_{t−1}| + |R_t − L_{t−1}| < |L_t − L_{t−1}| + |R_t − R_{t−1}|` (a cheaper assignment). Re-label, then re-run the Hampel step. This is my design; Pose2Sim leaves it unimplemented.
- **Combat caveat:** a real strike looks like an outlier to a speed-based Hampel test. Run Hampel on position residuals against a short smoothed track, not on velocity. Require the flag to be isolated (one or two frames that snap back), whereas a strike is monotonic over several frames.

### Gaps
- No source documented FreeMoCap's current gap-filling or rigid-bone steps; the GitHub post-processing parameter file returned 404. Older FreeMoCap is believed to do interpolation, Butterworth filtering, and rotation to a "good clean frame", but this was not verified in this session.
- No published bone-length tolerance threshold for flagging markerless outliers was found.

---

## 4. Foot contact detection, foot locking/IK, ground plane estimation

### Takeaway
The standard heuristic: a foot joint (ankle and toe, sometimes the heel) is in contact when its height above the floor is below a threshold and its speed is below a threshold. Lock it by pinning the contact position and solving two-bone leg IK, adjusting the hips when the leg can't reach. Release when contact ends or the foot drifts too far, and blend at contact transitions. Learned contact detectors beat threshold heuristics. For combat, the heuristic needs per-limb fast-leg overrides, since DeepMotion warns that Auto mode locks too long during fast leg motion.

### Cited Findings
- **PhysCap:** a foot is in contact only when its height above the calibrated ground plane is under 0.1 m. Contact constraints enforce zero velocity on the forefoot and heel links. Wrong contact estimates produce unnatural motion. — [PhysCap (arXiv 2008.08880)](https://arxiv.org/pdf/2008.08880) (search snippet)
- **NVIDIA Kimodo / UnderPressure code:** `foot_detect_from_pos_and_vel` uses at most 2 foot joints per side (ankle and toe) and computes the L2 norm of the joint velocity (already multiplied by 1/dt) and the joint height (y). Contact = `speed < vel_thres AND height < height_thresh`, per frame, with no smoothing or hysteresis. Thresholds are caller-supplied. — [Kimodo feet.py](https://research.nvidia.com/labs/sil/projects/kimodo/docs/_modules/kimodo/motion_rep/feet.html)
- **GenMM's contact utility** uses a height bound of 0.25 and a velocity threshold of 0.018, with units unstated and scene-dependent. — [GenMM contact.py](https://huggingface.co/spaces/wyysf/GenMM_bak/blob/main/utils/contact.py) (search snippet)
- **Benchmark metrics vary.** SAGA counts skating when the heel is near the ground and both heels' speed exceeds a threshold. GAMMA uses a height threshold above the floor plus a speed threshold. "A standard metric for physical plausibility has not been established." — [Human Motion Generation survey (arXiv 2307.10894)](https://arxiv.org/pdf/2307.10894) (search snippet)
- **Learned detectors:** UnderPressure trains on pressure-insole ground truth, estimates ground reaction forces, and derives contacts from them. It reports that it "significantly outperforms heuristic approaches based on height and velocity thresholds" and is more robust to noise. It also proposes a footskate cleanup. — [UnderPressure (arXiv 2208.04598)](https://arxiv.org/pdf/2208.04598) (search snippet)
- **Lock/unlock logic:** lock a foot with an analytical two-joint IK solver once its velocity is near zero; unlock when the contact flag is off or the foot drifts too far. — [Motion Matching in Unity (arXiv 2310.05215)](https://arxiv.org/pdf/2310.05215) (search snippet)
- **Footskate cleanup (Kovar, Schreiner, Gleicher)** addresses ankle discontinuities when contact state changes, which blending avoids. — [Footskate Cleanup for Motion Capture Editing](https://research.cs.wisc.edu/graphics/Gallery/kovar.vol/Cleanup) (search snippet)
- **Physics contact helps.** Physics-based markerless capture with contact reduced jitter and foot slide, preserved the global pattern, and still allowed abrupt changes in motion. — [Walking on Thin Air (arXiv 1812.01203)](https://arxiv.org/pdf/1812.01203) (search snippet). A 2026 paper proposes keyframed contact constraints to fix foot sliding — [arXiv 2601.19036](https://arxiv.org/html/2601.19036v1) (search snippet)
- **Ground plane:** Pose2Sim `feet_on_floor = false` by default. If true, "markers will be translated so that the feet are on the floor (assuming that the floor is flat)." Its OpenSim model adds six contact spheres per foot. — [Pose2Sim Config.toml](https://raw.githubusercontent.com/perfanalytics/pose2sim/main/Pose2Sim/Demo_SinglePerson/Config.toml), [README](https://github.com/perfanalytics/pose2sim)
- **Commercial foot-lock behaviours:**
  - DeepMotion: Auto (switches between locking and gliding), Always, Never (airborne or underwater), and Grounding (no lock, but kept on the ground; "when Auto locks the feet too long during fast leg movements") — [DeepMotion Foot Locking](https://deepmotion.com/article/foot-locking), [V2.6 release](https://blog.deepmotion.com/2021/04/22/v26/)
  - Plask's Foot Lock "determines when the feet are in contact with the ground and adjusts the foot and hip positions accordingly." It is a yes/no toggle, unavailable in multi-person mode — [Plask docs: Extract Motion](https://plask.ai/docs/16) (search snippet)
  - Rokoko Studio: lock start/end keys are editable on a timeline (green = left locked, blue = right, grey = none; sliding unsupported). "Treadmill" mode removes locking for upper-body-only work — [Rokoko Vision docs: Edit in Rokoko Studio](https://docs.rokoko.com/rkk-vision-documentation/get-started/edit-in-rokoko-studio) (search snippet). Studio 2.2.1 restored Knee Pop Smoothing and added "automatic hip nudging when feet extend too far" — [Rokoko Studio 2.2.1 notes](https://support.rokoko.com/hc/en-us/articles/9503071465745-Rokoko-Studio-2-2-1) (search snippet)
  - iClone: Reach Target plus a footstep system to stabilise sliding feet — [80.lv iClone 8 AI-mocap article](https://80.lv/articles/getting-ai-mocap-ready-for-production-with-reallusion-s-iclone-8) (search snippet)

### Inferences
- **JS foot-lock algorithm.** These are my reasoning; thresholds are starting points, not sourced unless stated.
  1. **Floor estimation:** fit the floor from candidate stance frames (low foot speed). Take the 5th–10th percentile of ankle/toe heights per foot, or RANSAC-fit a plane to toe and heel positions in those frames, to correct a tilted calibration. Translate so the floor is at y = 0, as Pose2Sim's `feet_on_floor` does.
  2. **Contact score per foot joint:** `contact = height < h_thr AND speed < v_thr`. Use PhysCap's 0.1 m as an upper bound for h_thr; about 3–5 cm above the estimated floor for toe/heel suits clean data. For v_thr, about 0.15–0.3 m/s is common in practice, unverified. Note that GenMM's 0.018 is per-frame displacement in unstated units.
  3. **Hysteresis and minimum duration:** enter contact at v < v_thr, exit at v > 1.5·v_thr. Require at least 3–4 frames (about 100 ms) of contact and fill 1–2 frame holes. Kimodo shows the bare heuristic has none of this.
  4. **Lock:** while in contact, the target is the mean contact position (or the position at contact onset), with y clamped to 0. Solve two-bone IK for hip → knee → ankle, using the knee pole from the filtered knee position. If the target is out of reach, shift the pelvis toward it, as Plask and Rokoko's hip nudging do.
  5. **Blend:** crossfade IK weight 0 → 1 over 2–4 frames at onset and 1 → 0 at release, which avoids the ankle pops Kovar describes.
  6. **Penetration:** outside contact, clamp foot joints to y ≥ 0 by raising the pelvis or the leg chain.
- **Combat specifics:**
  - Allow heel-only or toe-only contact. Pivots on the ball of the foot for roundhouse kicks rotate around the toe, so lock the position but free the yaw.
  - Release the lock as soon as the foot's speed exceeds threshold (kick chamber) rather than waiting for height.
  - Provide a per-segment manual override (Always / Never / Grounding, as DeepMotion does) on a timeline, as Rokoko does.

### Gaps
- No primary source gave a vetted m/s velocity threshold for contact. The UnderPressure heuristic baselines' values and Kimodo's defaults were not retrieved.
- No source covered contact detection specifically at 13–30 fps, where velocity estimates are coarse.

---

## 5. Up-sampling low-frame-rate captures and keeping impacts intact

### Takeaway
No surveyed source documents a peak-preserving upsampler for markerless data. The consistent advice is to capture faster (60–120+ fps, with shutter at least 2× fps), because content above Nyquist (fps/2) is lost. Interpolation can smooth playback but cannot restore a punch peak that fell between frames. DeepMotion's Speed Multiplier and output-rate options (QuickMagic, 24–120 fps) exist, but neither claims to recover lost detail.

### Cited Findings
- **Capture rate is the documented fix.**
  - Pose2Sim: ≥60 Hz; "Below that, accuracy drops, especially for fast motion" — [Pose2Sim README](https://github.com/perfanalytics/pose2sim)
  - Move AI: VFR at 100/120/200/240 fps for "rapid limb motion, jumps, or any action where inter-frame blur would degrade tracking" — [Move AI ZCam E2 configuration guide](https://docs.move.ai/knowledge/zcam-e2-m4-mark-ii-camera-configuration-guide)
  - Biomechanics: "Where the accelerations become very important … raise the sampling frequency" — [Clinical Gait Analysis FAQ](https://clinicalgaitanalysis.com/faq/cutoff.html)
- **DeepMotion's Speed Multiplier** targets "low-frame-rate footage and slow-motion sequences." Raising it "artificially generat[es] the frame rate capture needs." Low frame rate causes "choppy, less detailed capture," and extreme values give "jerky or unnatural movement." No numeric range is given. — [DeepMotion Speed Multiplier](https://deepmotion.com/article/speed-multiplier)
- **QuickMagic** reportedly lets users choose 24/30/60/120 fps output plus "built-in anti-penetration". — [topai.tools directory listing](https://topai.tools/t/quickmagic-motion-capture) (search snippet; directory claim, not official docs)
- **The impact literature keeps peaks by:**
  - truncating filtering at the impact frame (valid pre-impact data, post-impact information lost), or
  - time-varying cutoffs that rise during the impact.

  — [Augustus et al. 2020](https://eprints.chi.ac.uk/5020) (search snippet)
- **Filters lose "the peaks of rapid changes."** — [Nunome 2006](https://people.stfx.ca/smackenz/courses/HK474/Midterm/Nunome%202006%20Impact%20phase%20kinematics%20of%20instep%20kicking%20in%20soccer.pdf) (search snippet)

### Inferences
- **Upsampling recipe (e.g. 15/30 → 60 fps).** This is my reasoning.
  1. Clean at the native rate first: outliers, gaps, zero-phase filtering with the cutoff ≤ 0.25·fps.
  2. Upsample positions with a centripetal Catmull-Rom or monotone cubic Hermite spline. Monotone avoids overshoot, which reads as a jitter "wobble" at a strike's end.
  3. Re-solve rotations from the upsampled positions, or SLERP/SQUAD between hemisphere-aligned quaternions.
  4. Re-run foot locking at the target rate.
- **Peaks:** the true peak extension of a strike usually falls between samples at 15–30 fps. Two options:
  - Peak-snap, for authoring: detect the extremum of the distal joint's distance from the shoulder or hip, fit a parabola through the 3 samples around it, and place a key at the sub-frame vertex. This restores extension timing and amplitude to the parabola's estimate.
  - Leave peaks alone and let the animator push the extension key manually.

  Both beat a generic spline, which undershoots the peak.
- **No frame-interpolation preprocessing.** Video frame interpolation (synthesising frames before pose estimation) is a possible but unverified route, and it tends to blur fast limbs. Prefer capturing at a higher rate.
- **Multi-webcam rigs at about 13–30 fps:** reducing resolution or camera count to raise effective fps is likely worth more than any post-process, since Pose2Sim and Move AI both tie fast-motion accuracy to frame rate.

### Gaps
- I found no paper or tool doc on upsampling markerless pose specifically, or on peak-preserving interpolation of strikes.
- I found no source on why multi-webcam USB rigs drop to about 13 fps; bandwidth and exposure are the likely causes. One FreeMoCap fork notes "USB hubs typically don't work."

---

## 6. How FreeMoCap, Rokoko Vision, Move.ai, DeepMotion, Plask, Radical and QuickMagic handle these, and what users report for combat

### Takeaway
- **Open-source pipelines** expose classical, tunable steps. FreeMoCap uses a Butterworth at 6 Hz, order 4; it was mis-scaled by a 30 fps clock bug until v2.0.0-alpha.21 in August 2026. Pose2Sim has the richest configurable chain.
- **Commercial tools** expose coarse switches: foot lock modes or toggles, an "AI motion smoothing" amount, a physics/anti-penetration filter, speed multipliers, and timeline lock editing.
- **Combat reports:** I found no credible user evaluations of any of these tools for kicks or punches. Reviews converge on "fast motion, blur and occlusion degrade quality."

### Cited Findings
- **FreeMoCap:**
  - Default Butterworth 6 Hz, order 4. The sampling rate was hard-coded at 30, fixed in PR #861 / v2.0.0-alpha.21 on Aug 11, 2026 — [issue #849](https://github.com/freemocap/freemocap/issues/849)
  - 1.2.2 logged 7 Hz, order 4, `run_butterworth_filter=True` — [issue #620](https://github.com/freemocap/freemocap/issues/620) (search snippet)
  - Docs: enough light for short exposures to reduce motion blur. Most testing was on low-quality webcams. Spread cameras for distinct viewpoints but avoid overly wide angles. Show the ChArUco board 5–10 s per camera pair, which gives about 200 shared views at 30 fps. The board must match the exact layout (`DICT_4X4_250`) — [FreeMoCap docs: resources](https://freemocap.github.io/documentation/resources), [calibration troubleshooting](https://freemocap.github.io/documentation/calibration-troubleshooting.html) (search snippet; split between the two pages uncertain)
- **MediaPipe (a FreeMoCap backend)** applies its own One Euro smoothing inside the graph (parameters in section 2), so its output is already smoothed before FreeMoCap's Butterworth. — [MediaPipe pose_landmark_filtering.pbtxt](https://raw.githubusercontent.com/google-ai-edge/mediapipe/master/mediapipe/modules/pose_landmark/pose_landmark_filtering.pbtxt)
- **RTMPose / MMPose** provides optional Gaussian, One Euro and Savitzky-Golay temporal smoothers via `Smoother`. — [MMPose smoother source](https://mmpose-ly015.readthedocs.io/en/latest/_modules/mmpose/core/post_processing/smoother.html)
- **Rokoko Vision / Studio:**
  - Cleanup happens in Rokoko Studio: foot-lock filter, locomotion filter (real-time since Studio 2.5, on by default, with "Legacy Locomotion" as an option), Drift Fix (2.5.x; works best when a take starts and ends in the same spot, may add artifacts), Knee Pop Smoothing, hip nudging, root locking, elevation tracking — [Rokoko Studio locomotion & Drift Fix update](https://support.rokoko.com/hc/en-us/articles/41620261484945-Rokoko-Studio-Update-Real-time-Locomotion-Drift-Fix), [Rokoko Studio product page](https://www.rokoko.com/products/studio) (search snippets)
  - Vision's dual-camera mode was sunset about 30 days after Vision 3.0 launched — [Rokoko Vision (JP product page)](https://www.rokoko.com/ja/products/vision) (search snippet)
- **DeepMotion Animate 3D:**
  - Foot Locking modes Auto, Always, Never, Grounding; Auto on all plans, Always/Grounding need Professional, Never needs Innovator — [Foot Locking](https://deepmotion.com/article/foot-locking)
  - Physics Filter: joint limits, self-collision, clipping — [Physics Filter](https://deepmotion.com/article/physics-filter)
  - AI Motion Smoothing: adjustable; a search snippet says 0.1–1.0, which the help page does not state; trades accuracy for smoothness — [Motion Smoothing](https://deepmotion.com/article/motion-smoothing)
  - Speed Multiplier for low-fps or slow-motion footage — [Speed Multiplier](https://deepmotion.com/article/speed-multiplier)
  - The V2.4 release tuned the Physics Filter for smoother motion and less mesh clipping — [V2.4 release](https://blog.deepmotion.com/2021/02/11/animate3d-v24/) (search snippet)
- **Move AI (Move Pro, multi-camera):**
  - 50/100 or 60/120 fps matched to mains lighting; GoPro linear lens, HyperSmooth off; cameras evenly spaced; calibration before action takes — [Move AI GoPro setup guide](https://help.move.ai/en/articles/8857611-gopro-setup-guide) (search snippet)
  - Shutter ≥2× fps; at least 256 px subject height at the far edge of the volume; ISO ≤4000; lighting "the single most important variable" — [ZCam E2 guide](https://docs.move.ai/knowledge/zcam-e2-m4-mark-ii-camera-configuration-guide)
  - Calibration score <10 px — [Move AI docs](https://docs.move.ai/knowledge/what-is-a-good-pixel-score-for-a-calibration-1) (search snippet)
  - Comparisons rate it most accurate for complex poses, but with occasional extra foot sliding — [TATO Studio](https://tato.studio/blog/best-ai-video-to-mocap) (search snippet)
- **Plask:** a Foot Lock toggle adjusts feet and hips (not in multi-person mode) — [Plask docs](https://plask.ai/docs/16) (search snippet). Rated budget tier, "useful for blocking and previs" — [TATO Studio](https://tato.studio/blog/best-ai-video-to-mocap) (search snippet). A 2022 article said editing was limited to simple keyframes plus smoothing — [CG Channel 2022](https://www.cgchannel.com/?p=133754) (search snippet, dated)
- **QuickMagic:**
  - Reviewers: "excels at detecting when a foot is planted on the ground"; output described as "grounded and stable" — [CGDive review](https://cgdive.com/quickmagic-review-high-quality-ai-motion-capture/)
  - Weak on hands planted on the ground; slow-motion footage "can also pose challenges"; film at normal speed — [CGDive review](https://cgdive.com/quickmagic-review-high-quality-ai-motion-capture/)
  - Clip limits: 30 s free, 60 s paid — [CGDive review](https://cgdive.com/quickmagic-review-high-quality-ai-motion-capture/)
  - 24/30/60/120 fps output and anti-penetration — [topai.tools](https://topai.tools/t/quickmagic-motion-capture) (directory claim)
- **RADiCAL:** placed in the high-quality tier alongside Move AI — [TATO Studio](https://tato.studio/blog/best-ai-video-to-mocap) (search snippet). No documentation of its smoothing or contact options was found.
- **Blender ecosystem:**
  - BlendMo is a free Blender extension for cleaning markerless data — [BlendMo (SCU senior project)](https://scholarcommons.scu.edu/idp_senior/100) (search snippet)
  - The MoCap AI add-on offers Gaussian smoothing at None/Low/Medium/High — [Superhive: mocap ai](https://superhivemarket.com/products/mocap-ai?search_id=50504016) (search snippet)
  - Tutorials smooth jitter with Graph Editor Gaussian smoothing — [Animost mocap cleanup tutorial](https://animost.com/tutorials/mocap-cleanup-and-edit/) (search snippet; attribution uncertain)

### Inferences
- **Common feature set to copy into a JS tool,** matching the minimum surface every commercial tool exposes:
  1. A smoothing strength slider. Map it to cutoff Hz, not an opaque 0–1, with a per-joint-group override.
  2. A foot-lock mode per foot per time range (Auto / Always / Never / Grounding) with editable lock keys on a timeline, as Rokoko does.
  3. A ground-plane snap.
  4. Anti-penetration (feet-below-floor clamp, self-collision optional).
  5. A Drift Fix (align start and end root positions) for loops.
  6. Hip nudge when IK can't reach.
- **FreeMoCap data recorded before alpha.21 at anything other than 30 fps had a shifted cutoff.** A 60 fps take was filtered at 12 Hz (under-smoothed) and a 15 fps take at 3 Hz (heavily over-smoothed, strikes flattened). A tool importing FreeMoCap output should prefer the raw (unfiltered) 3D data and re-filter with the correct fps.

### Gaps
- No credible user report (Reddit, Polycount, Blender Artists) evaluating any of these tools on kicks, punches or shoves was found. The search results returned vendor or affiliate roundups only. Treat any quality claims about combat as unverified.
- No RADiCAL documentation on smoothing or foot contact was found.
- Move AI's in-app processing filters (beyond capture specs) were not found.
- QuickMagic has no official documentation of its smoothing or foot-lock controls.
- I could not open Rokoko's "Filters explained" support article (HTTP 403), so its per-filter parameters are unknown.

---

## 7. What practitioners recommend capturing differently to reduce cleanup

### Takeaway
Capture faster, sharper and better lit:
- 60 fps minimum; 100–120+ fps for fast limbs.
- Shutter at least 2× fps (1/240 s at 120 fps).
- Strong lighting and low ISO.
- The whole body, including feet, in frame for every camera.
- About 6 cameras around the volume, or 2 cameras at front and 45° at hip height. No top-down views.
- Good calibration: under 0.5 px intrinsic, under 1 cm extrinsic.
- A sharp sync gesture.
- Perform at real speed rather than filming slow-motion footage.

### Cited Findings
- **Pose2Sim recommendations:**
  - Frame rate ≥60 Hz.
  - "6 is generally a good camera number"; complex motion needs more, gait fewer.
  - Two cameras: front plus one at 45° to the side, both at hip level (alternatives: front plus 90°, or two front cameras at hip and eye level).
  - Top views directly above subjects "do not yield good results."
  - Sync with a sharp vertical movement; keep captures at least 5 s and at most a few minutes to limit drift.
  - Intrinsic error ≤0.5 px, extrinsic <1 cm; recompute intrinsics after zooming; 10 or more spread-out points for the scene-coordinate method.

  — [Pose2Sim README](https://github.com/perfanalytics/pose2sim)
- **Move AI recommendations:**
  - Shutter "at least 2x your capture frame rate" (1/100–1/120 at 50/60 fps, 1/200 at 100, 1/240 at 120).
  - Higher VFR for rapid limb motion.
  - At least 256 px of subject height at the furthest point.
  - 4K preferred for keypoint detection.
  - ISO "as low as possible; max 4000."
  - Lighting "the single most important variable."
  - Flicker reduction matched to the mains region.

  — [Move AI ZCam E2 guide](https://docs.move.ai/knowledge/zcam-e2-m4-mark-ii-camera-configuration-guide). Also: linear lens, stabilisation off, cameras evenly spaced, calibration before takes — [Move AI GoPro guide](https://help.move.ai/en/articles/8857611-gopro-setup-guide) (search snippet)
- **FreeMoCap:** enough light for short exposures to reduce blur; distinct but not overly wide camera angles; 5–10 s of ChArUco per camera pair; avoid glare on the board. — [FreeMoCap docs](https://freemocap.github.io/documentation/resources) (search snippet)
- **QuickMagic (CGDive review):** well-lit, high resolution, faster shutter against motion blur, the whole body including both feet in frame for the entire clip. Slow-motion footage causes problems, so film at normal speed and retime in Blender or Maya, or speed up slow footage before upload. — [CGDive review](https://cgdive.com/quickmagic-review-high-quality-ai-motion-capture/)
- **Conflicting advice on pace:** one guide suggests a higher frame rate, "or perform the move slightly slower and retime the motion data afterward," for spins and whip-fast moves. — [invideo/Uthana guide](https://invideo.io/blog/ai-motion-capture/) (search snippet). This is performing slower, which differs from filming slow-motion footage (a high-speed camera clip played slowed), which CGDive warns against.
- **Clean input:** a Rokoko-related tutorial recommends a wide view, clear separation from the background, minimal motion blur, and starting each take with a T-pose. — [Animost tutorial](https://animost.com/tutorials/mocap-cleanup-and-edit/) (search snippet; attribution uncertain)
- **Rokoko Drift Fix** works best when a take starts and ends in the same spot. — [Rokoko Studio Drift Fix update](https://support.rokoko.com/hc/en-us/articles/41620261484945-Rokoko-Studio-Update-Real-time-Locomotion-Drift-Fix) (search snippet)
- **Pose2Sim's own scaling** drops deep-flexion frames, so a standing neutral pose in each take gives clean segment-length frames. — [Pose2Sim Config.toml](https://raw.githubusercontent.com/perfanalytics/pose2sim/main/Pose2Sim/Demo_SinglePerson/Config.toml)

### Inferences
- **For a webcam combat-capture protocol.** These are my synthesis.
  1. Lock exposure manually (auto-exposure lengthens the shutter indoors). Light the volume brightly so a 1/250 s or faster shutter works.
  2. Drop resolution until all cameras hold 30 fps or more steadily; 60 fps if the webcams support it.
  3. Place 3–4 cameras at hip height around the performer: front, ±45–90°, plus one behind if possible, to reduce L/R swaps and occlusion of the rear limb.
  4. Start and end each take with 1–2 s of A- or T-pose standing still (scaling, floor estimate, drift fix) and a sharp sync motion such as a jump or clap with arms overhead.
  5. Wear fitted clothing that contrasts with the background (unsourced; see Gaps).
  6. Perform strikes at full speed for timing, and optionally record a slower "clarity pass" for pose reference. Don't upload slow-motion video.
  7. Keep feet in frame on kicks and leave margin above the head for high kicks.
- The 60 fps floor from Pose2Sim and the 100–120 fps "rapid limb motion" guidance from Move AI mean 13–30 fps webcams are below every documented recommendation for fast movement. The JS tool should expect manual peak fixing on strikes.

### Gaps
- No source found gives clothing recommendations (fitted versus loose, colour, contrast) for these tools. Move AI and Rokoko likely document this, but it was not retrieved.
- No source quantifies how much a T-pose or standing calibration improves markerless output.
- No source quantifies performing at reduced speed against real speed for markerless strike accuracy.
