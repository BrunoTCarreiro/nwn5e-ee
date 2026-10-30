# Impact craft: turning mocap / keyframed motion into snappy, readable, impactful game combat animation

Scope note: third-person melee/unarmed body animation (shove, kick, rage roar, crouched idle) for Neverwinter Nights (keyframed clips + engine cross-blends, no physics). Frame numbers from fighting games are at 60 fps unless stated; NWN clips are authored at 30 fps (see section 6), so fighting-game frame counts roughly halve when ported.

Source-quality legend used below: **[primary]** = the practitioner's own words/site; **[secondary]** = journalism/summary of a practitioner; **[community]** = blogs, vendor guides, forum rules of thumb (treat numbers as heuristics); **[snippet-only]** = seen only in a search-result excerpt, page itself not retrieved or did not load.

---

## 1. Why raw mocap feels floaty/weak in games, and what animators change

### Takeaway
Captured performance is treated as raw material, not timing: AAA teams routinely speed every take up (Assassin's Creed III: +15% as a baseline), cut and re-assemble takes from parts, remove "dead" frames between key poses, and then exaggerate ("plus") poses and re-time for impact. Feel overrides realism, so the cleanup pass is where the snap gets added.

### Cited Findings
- On Assassin's Creed III's fight animation (Jonathan Cooper's gameanim.com; the page itself doesn't name an author), "every take is sped up 15% as a starting point" — [Animating The 3rd Assassin, p.7 — gameanim.com](https://www.gameanim.com/2014/02/11/animating-3rd-assassin/7/) [primary]
- Same source: during capture "we still don't care about timing". Timing is set in editing, where animators "reshuffle actors' positions and timing" and "reassemble the single action from the component parts" — [gameanim.com AC3 p.7](https://www.gameanim.com/2014/02/11/animating-3rd-assassin/7/) [primary]
- Same source: at every stage animators "exaggerate and 'plus' every movement, make poses more dynamic, and adjust timing to give attacks more impact" — [gameanim.com AC3 p.7](https://www.gameanim.com/2014/02/11/animating-3rd-assassin/7/) [primary]
- Same source: casting matters for impact. One stunt performer was cast to provide the Assassin's "weight and power", another because he could "sell the impact of a hit". The team ran rehearsals before shoot days, and "Rehearsals allowed for experimentation without blowing our budgets on the days of shooting." Scale: just over 30 shoots, about 70 takes a day, over roughly 3 years — [gameanim.com AC3 p.7](https://www.gameanim.com/2014/02/11/animating-3rd-assassin/7/) [primary]
- Same source: the animator is responsible for deciding "exactly what he or she needs from each mocap session" before it is shot. Animation is reviewed in a 3D viewer (Autodesk FBX Converter) rather than on video, because seeing it "from all angles" is "essential for ingame animation" — [gameanim.com AC3 p.7](https://www.gameanim.com/2014/02/11/animating-3rd-assassin/7/) [primary]
- A fighting-game mocap workflow (Gnomon workshop, Brandon Yates, "Animating a 3-Hit Combo for Games in Maya") retimes with Maya Scene Time Warp by "strategically removing dead frames, favoring key poses like anticipation". It bakes the retime down to the control rig, cleans up from the hips outward, and baked often. Its overlap pass runs from the spine out to the extremities and is balanced so it doesn't weaken the attack — [Gnomon workshop listing](https://gnomon.thegnomonworkshop.com/workshops/animating-a-3-hit-combo-for-games-in-maya) [primary course outline; lesson detail paywalled]
- Cooper's 2005 "Basics: Fast vs Slow" says the player character's "feel" "should always take precedence over the aesthetics of the animations", and that editing and second passes should account for this — [gameanim.com, Basics: Fast vs Slow](https://www.gameanim.com/?p=115) [primary]
- Same article: instant input response "translates to a weightless PC", while slow reactions make the character "seem sluggish and unresponsive". Control should be "tempered by an appropriate dose of unresponsiveness" in every action — [gameanim.com Fast vs Slow](https://www.gameanim.com/?p=115) [primary]
- Natural motion has "sharp corners and non-perfect arcs", so judge polish by playing the motion back, not by smoothing curves in isolation — [Animation Mentor blog](https://www.animationmentor.com/?p=100828) [community/education]
- Typical mocap cleanup targets are jitter/noise, foot slippage and clipping, plus pose and timing adjustments — [Upwork mocap-cleanup service listing](https://www.upwork.com/services/product/video-audio-motion-capture-cleanup-for-games-films-and-cinematics-2066155860020935213) [community; low authority]

### Inferences
- The "floaty" feel comes from three things in the raw data: real-human timing (no speed-up), evenly spaced transitional frames between key poses (no snap into the contact), and peak extremes that a performer protecting their body won't hit. The documented fixes map one-to-one: global speed-up, dead-frame removal or time-warp, and pose exaggeration.
- A tool should therefore offer, in order:
  1. A global retime (e.g. a default 0.85x duration, matching the AC3 +15%).
  2. A non-linear time-warp curve, so frames before contact can be compressed while follow-through stays roughly real-time.
  3. Key reduction to the key poses, so the result can be edited like keyframes.
  4. An additive pose-offset layer for exaggeration that bakes back to keys.
- Because NWN has no physics, all "weight" has to come from timing and spacing in the clip itself.

### Gaps
- I couldn't retrieve Cooper's Ch. 11 ("Our Project: Motion Capture") text. A bookseller snippet suggests it discusses reducing key density so mocap can be "manipulated as they would key-frames", but I couldn't pin that to a specific retrievable page — see the [Routledge 2nd ed. page](https://www.routledge.com/Game-Anim-Video-Game-Animation-Explained/Cooper/p/book/9780367707651) for the book.
- I found no sourced figure for how much anticipation is typically cut from a mocap attack (in frames or percent).
- I found no Polycount or 11 Second Club thread with concrete mocap-polish numbers.

---

## 2. Anticipation vs responsiveness, the contact frame, and frame budgets

### Takeaway
Player attacks keep anticipation short and spend frames on follow-through and recovery instead. Enemy (NPC) attacks get long anticipation as a "telegraph". Designers set the contact/hit frame and total length as goalposts, and animators pose and space inside them. God of War Ragnarök's targets were hit at about frame 20 and the whole move, recovery included, done by about frame 60. Fighting games use the startup/active/recovery split: the fastest lights hit on frame 3–5 at 60 fps, and heavies are rarely faster than about frame 8–9.

### Cited Findings
- **Cooper's 12 principles applied to games** [snippet-only: a republished excerpt of Cooper's book. The copy at jikguard.com returned unrelated content when fetched, so these quotes come from the search excerpt only]:
  - "Anticipation is a controversial topic in video games, with designers often requesting as little as possible and animators pushing for as many frames as possible."
  - Anticipation in NPC attacks ("telegraphing") is "desirably longer".
  - "Follow-through has fewer gameplay restrictions than anticipation… too long a follow-through before returning control to the player can again result in an unresponsive character". The remedy is a marked frame where the player regains control, so the follow-through still plays if no new input comes.
  - Source: [search excerpt — jikguard.com repost](https://www.jikguard.com/news/the-12-principles-of-animation-in-video-games)
- Cooper's three-phase vocabulary [primary] — [gameanim.com Fast vs Slow](https://www.gameanim.com/?p=115):
  - **Anticipation** runs up to the Apex.
  - **Apex** is "the frame (or window of frames) when the purpose of the particular animation has been achieved".
  - **Recoil** "returns the character back to a designated pose", usually the idle, so the next animation transitions smoothly.
  - Anticipation is "generally longer for NPCs to allow the player time to react", and fast for the PC "so as to not delay too long after the joypad input".
- Same article: set timing rules early in prototyping. Establish the average PC punch/kick anticipation first, then balance other actions as faster or slower relative to that baseline — [gameanim.com Fast vs Slow](https://www.gameanim.com/?p=115) [primary]
- God of War Ragnarök (Bruno Velazquez, animation director):
  - The combat team suggests the hit frame land "around frame 20", with the whole move "recovery included, done by frame sixty".
  - Within those goalposts animators are free to work on posing and pose spacing.
  - Left to their own instincts, animators "would probably make very long animations", which would not feel right for gameplay.
  - Source: [Eurogamer interview, via mirror](https://eurogamerai.pages.dev/posts/moving-kratos-a-deep-dive-into-god-of-war-ragnar-k-s-animation) [secondary; original Eurogamer page not retrieved, so check the exact wording there]
  - Also from Velazquez: "the gameplay always comes first", and animators add anticipation and weight only within gameplay limits — [80.lv](https://80.lv/articles/gameplay-is-first-santa-monica-studio-s-animation-director-on-god-of-war-ragnar-k) [secondary]
- God of War III's GDC 2010 talk (Bruno Velazquez) covered the pipeline from concept to in-game, including a section on balancing gameplay against animation — [GDC Vault](https://gdcvault.com/play/1012720/Animation-Process-of-God-of); [gameanim.com write-up](https://www.gameanim.com/2018/04/20/god-of-war-iii-animation-process) [primary listing; talk content not retrieved]
- **Fighting-game frame data definitions**:
  - Startup is the frames before an attack can hit, active is the frames it can hit, recovery is the frames after during which the attacker can't act.
  - Modern Street Fighter counts the first active frame inside startup. Older "true startup" convention: a move that hits on frame 3 has 2 startup frames.
  - Source: [Dustloop — Using Frame Data](https://dustloop.com/w/Using_Frame_Data); [SuperCombo — Frame Data](https://srk.shib.live/w/Frame_Data) [community wikis, authoritative within the FGC; convention detail via search summary]
- In Street Fighter V, the fastest normals and specials hit on the 3rd frame (startup 3 at 60 fps) — [Prima SFV eGuide](https://primagames.com/eguides/street-fighter-v-eguide/primer/reading-frame-data) [secondary]
- Killer Instinct:
  - Every character has at least one 5-frame normal, a light punch or light kick that hits on the 5th frame (training mode lists it as 4).
  - Heavy normals are "never faster than roughly 8 or 9 frames".
  - Examples: Jago's jumping MK is 8f, jumping HP is 12f, and his command overhead is 19f startup.
  - Source: [Infil's KI guide — The Basics](https://ki.infil.net/basics2.html) [community; highly regarded]
- A Gnomon fighting-style attack workshop (Brad Faucheux) frames attacks as startup/active/recovery. It uses offset keys and smears for impact, simulates hitstop, and spaces poses "for clarity in gameplay" — [Gnomon blog](https://ea.thegnomonworkshop.com/blog/animating-stylized-attacks-in-maya) [course outline]

### Inferences
- **Frame-rate conversion for NWN (30 fps):**
  - An SF/KI light hitting on frame 3–5 at 60 fps is about 50–83 ms, or 1.5–2.5 NWN frames.
  - A KI heavy at about 8–9f is about 133–150 ms, or 4–4.5 NWN frames.
  - A KI command overhead (19f) is about 317 ms, or about 9.5 NWN frames.
  - The God of War numbers' frame rate isn't stated. At 30 fps, "hit by frame 20, done by 60" is about 0.67 s to contact and 2 s total. At 60 fps it is about 0.33 s and 1 s. Treat these as a design range, not a constant.
- **Contact frame as the anchor.** Mark the contact frame first, then compress what comes before it (anticipation) and keep or shape what comes after it (follow-through/recovery).
- **Tool features this suggests:**
  - Contact-frame and "control-return/interrupt frame" markers.
  - Phase brackets (anticipation / contact / follow-through / recovery) that can each be retimed independently.
  - A readout of time-to-contact in ms.
- **Anticipation by use in nwn5e:**
  - Player-facing attacks (shove, kick): short anticipation.
  - Monster/NPC telegraphs (e.g. a rage roar used as a threat): longer anticipation.
  - The same clip may need two timings if both PCs and NPCs use it.
- **NWN fixes action timing.** NWN's combat round, not player input latency, decides when an attack resolves. "Responsiveness" there is mainly about readability and synchrony with the engine's hit/damage event, not input lag. This needs checking against the engine in the companion research.

### Gaps
- Several community guides give "2–5 frames anticipation for snappy games, up to 8–10 for heavy attacks". I couldn't attribute this rule of thumb to a specific author, so it is unverified.
- I found no published frame budgets for third-person action-RPG shoves or kicks specifically, and none from Souls-likes, Sekiro or For Honor. I found no For Honor or Sekiro animation talks with frame data in this pass.
- I couldn't confirm how NWN's combat round schedules attack-animation playback against the hit/damage event, or whether attack clips get time-scaled. The "frame-to-hit" budget for nwn5e depends on that.

---

## 3. Hit-stop / hit-pause, impact holds, shake

### Takeaway
Hitstop freezes both attacker and victim for the same few frames on contact. It scales with damage (longer for heavier hits), is capped, and is sold with a decaying shake of the victim while hitboxes stay put. Sakurai keeps the attacker creeping almost imperceptibly during the freeze, and blends the victim over 4 frames from the first flinch frame into the hurt pose. He also warns against long hitstop when there are many enemies or third parties. Published values run from about 7 frames (light) to about 11–16 frames (heavy) at 60 fps in fighting games. Action games use much less.

### Cited Findings
- Sakurai's Famitsu column Vol. 490, "Thinking About Hitstop" — [Source Gaming translation](https://sourcegaming.info/2015/11/11/thoughts-on-hitstop-sakurais-famitsu-column-vol-490-1/) [primary in translation; fan translation]:
  - **What it is:** in hitstop "both parties momentarily freeze, emphasizing the power of impact", and both freeze "for the exact same amount of time".
  - **Scaling:** "the more damage an attack inflicts, the longer the hitstop period".
  - **Per-attack modifiers:** Marth's sword tip gets more hitstop, projectiles get less, and electric attacks "automatically have an increased hitstop modifier". A cap applies ("the hitstop period will never exceed this limit").
  - **Attacker during the freeze:** "in Smash, the attacker also vibrates slightly". It also moves "at such a miniscule speed" that no single frame shows it.
  - **Victim shake:** characters "vibrate visually, but keep the hurtbox locations static". Grounded characters shake side to side, airborne ones up and down, because vertical shake on the ground would clip feet into the floor. Shake starts large and decays over the freeze, computed from the total freeze frames, and is scaled up when the camera is far away.
  - **Victim pose:** "during hitstop, we take four frames to smoothly transition from the initial flinch to the hurt animation". After the first flinch frame the character enters the painful-looking hurt pose.
  - **When to limit it:** in multi-enemy games, "If the action stopped every single time you landed an attack, you would likely end up frozen for a long time". In Smash, long freezes give a third player a chance to strike.
- Sakurai's 2022 YouTube video covers eight hit-stop techniques: shaking the hit character more, not moving the hitbox, controlling the amount, interpolating frames into the damage pose, keeping the attacker moving slightly, and varying shake with camera distance — [GoNintendo summary](https://www.gonintendo.com/contents/13581-sakurai-s-latest-game-dev-video-features-8-hit-stop-techniques) [secondary; snippet-only]
- A Capcom Street Fighter V column says both sides experience hit stop when the attack hits, and that the amount is usually set per game. Its example: 8/12/16 frames for light/medium/heavy — [Capcom CFN column (JP)](https://game.capcom.com/cfn/sfv/column/131545?lang=ja) [primary but snippet-only; the page returned 503 on fetch]
- King of Fighters XIII: hitstop of 7 frames for light attacks, 11 for heavy, variable for specials — [SuperCombo KOF XIII Hitstun/Blockstun](https://srk.shib.live/w/The_King_of_Fighters_XIII/Character_States/Hitstun_Blockstun_state) [community wiki; snippet-only]
- Dustloop's frame-data format lists hitstop as the frames both characters "freeze". An "X/+Y" entry means the defender gets Y extra frames, so the freeze can be asymmetric — [Dustloop BBTag Hyde frame data](https://dustloop.com/w/BBTag/Hyde/Frame_Data) [community wiki]
- Research on impact "feel" names hit stop among the features that most strongly affect perceived impact, and cites Martin's game-feel survey for its role in signalling gameplay-relevant events — [Lin et al., UW](https://faculty.washington.edu/zkwen/articles/lin22features.pdf) [academic]
- Community action-game values [community; which figure comes from which page is per search summary, unverified]:
  - About 30–50 ms light, 60–100 ms heavy, 100–150 ms crit — [uhiyama-lab game-feel primer](https://uhiyama-lab.com/en/notes/unity/unity-game-feel-hit-feedback/)
  - About 2–3 frames for daggers or claws up to 6–10 frames for maces or greatswords, at 60 fps — [freegamesprites juice tricks](https://freegamesprites.com/ar/news/game-feel-2d-combat-juice-tricks)
- A Gnomon workshop description calls the overshoot pose the critical "snap" that reads as impact, with the anticipation pose as the foundation — [Gnomon workshop listing](https://bit.thegnomonworkshop.com/workshops/animating-stylized-attacks-in-maya) [snippet-only]

### Inferences
- **NWN can't freeze the game clock or both actors in sync,** as far as this research shows. Hitstop would have to be baked into the clips:
  - **Attacker:** a hold of a few frames at contact. At 30 fps, 2–3 frames is about 67–100 ms, matching the community range for medium and heavy action-game hits. The pose should keep a tiny drift, not be a dead-still copy, following Sakurai's creeping attacker.
  - **Victim:** a flinch frame plus a short ease into the hurt pose (Sakurai's 4-frame blend at 60 fps is about 2 NWN frames). Keys could carry a decaying lateral shake (side-to-side for grounded characters).
  - The hold only lines up if the victim's damage animation fires at the attacker's contact frame. That depends on the engine's attack/damage event timing, which is unverified.
- **Tool features:**
  - "Insert hold at contact (N frames)" that pushes all later keys back.
  - An optional "micro-drift" so the hold isn't a dead freeze.
  - "Overshoot and settle" applied to the contact pose: push the pose 1–2 frames past contact, then settle back.
  - A decaying-shake generator for hit reactions, with amplitude and frame-count parameters.
- **Group fights:** NWN has many combatants at once, so Sakurai's multi-enemy warning applies. Keep baked holds short so attack strings don't feel sluggish.

### Gaps
- I couldn't fetch the Capcom column or the Sakurai 2022 video itself, so their exact numbers and wording are unverified.
- The Avowed (Obsidian) PC Gamer article on hit-pause was paywalled or truncated. I couldn't get its RPG-specific hitstop values.
- A blog account says Nuclear Throne's hit reaction starts with a white frame followed by 2 frames of a hit pose. I couldn't confirm it against a primary Vlambeer source. Jan Willem Nijman's 2013 talk "The Art of Screenshake" exists, but its frame-level content wasn't retrieved.
- I found no source on hit-stop in God of War, For Honor or Souls-likes specifically.
- I found nothing on smears in 3D engines without shader support. The Faucheux workshop names smears but its content is paywalled.

---

## 4. Pose-to-pose thinking on top of mocap: key poses, pushing them, layers

### Takeaway
Treat the mocap as a timing and weight guide over a small set of key poses: anticipation, contact/apex, overshoot/follow-through, recovery/return-to-stance. Reduce or warp the frames between those poses, push the poses themselves, and add overlap from spine to extremities only as long as it doesn't soften the strike.

### Cited Findings
- Retime by removing dead frames while "favoring key poses like anticipation". The workflow also highlights blend poses, striking positions and guard stances for fighting-game readability — [Gnomon, Brandon Yates](https://gnomon.thegnomonworkshop.com/workshops/animating-a-3-hit-combo-for-games-in-maya) [course outline]
- Overlap is added "from the spine out to the extremities", balanced so it doesn't weaken the attack — [Gnomon, Brandon Yates](https://gnomon.thegnomonworkshop.com/workshops/animating-a-3-hit-combo-for-games-in-maya) [course outline]
- "Exaggerate and 'plus' every movement, make poses more dynamic" — [gameanim.com AC3](https://www.gameanim.com/2014/02/11/animating-3rd-assassin/7/) [primary]
- The overshoot pose after the hit provides the "snap" that reads as impact — [Gnomon listing](https://bit.thegnomonworkshop.com/workshops/animating-stylized-attacks-in-maya) [snippet-only]
- Dead or Alive's fast movement "allows exaggerated poses" and reduces the need for multiple similar animations — [gameanim.com Fast vs Slow](https://www.gameanim.com/?p=115) [primary]
- The Apex can be a single frame or "a window of frames" in which the action's purpose is achieved — [gameanim.com Fast vs Slow](https://www.gameanim.com/?p=115) [primary]
- God of War Ragnarök animators work on "posing and pose spacing" within designer-set hit and recovery frames — [Eurogamer via mirror](https://eurogamerai.pages.dev/posts/moving-kratos-a-deep-dive-into-god-of-war-ragnar-k-s-animation) [secondary]
- The Roberts et al. (2019) research approach converts mocap into a sparse keyframe representation so it can be edited with standard keyframe tools. The authors report it beats research baselines and a commercial tool — [Roberts et al., Computational Visual Media](https://scribblethink.org/Work/Pdfs/roberts_OptimalAndInteractiveKeyframeSelectionForMotionCaptureJournalversion.pdf) [academic]

### Inferences
- **A practical tool loop:**
  1. Auto-detect candidate key poses: velocity minima, extremes of hand or foot travel, contact.
  2. Let the user tag them by phase: anticipation, contact, overshoot, settle, recovery.
  3. Retime the spans between tags.
  4. Apply an additive offset per tagged pose (push the spine twist, deepen the anticipation coil, extend the striking limb at contact, add overshoot).
  5. Propagate the offset with falloff to neighbouring frames, then bake.
- **Silhouette.** Readability from NWN's elevated third-person camera means the contact and anticipation silhouettes must read from above and at an angle. Following the AC3 "view from all angles" practice, the tool should preview key poses from the in-game camera pitch, not only from the front or side.
- **Overlap on contact.** Spine-out overlap should not delay the striking limb's arrival at contact. Keep the striking chain crisp and let overlap live on the non-striking side and the head.

### Gaps
- I found no source with concrete exaggeration magnitudes, such as degrees of extra spine twist or percent extension.
- I found no source specifically on additive or animation layers in MotionBuilder or Maya for exaggerating mocap. This is common practice, but I didn't find a citable statement in this pass.
- I found nothing on readable-silhouette guidance from Cooper's "staging" principle. The text wasn't retrieved.

---

## 5. Loops and idles from mocap (seamless cycles, breathing, combat stance)

### Takeaway
Pick a steady-momentum section, then loop by cross-blending motion (a full action), not by matching a single pose. Start and end on a recognisable pose, blend across large motions to hide the seam, and fix the sliding the blend introduces. Combat idles differ from relaxed ones in spine angle, stance width and breathing rate. Variety comes from fidget clips that return to the base idle.

### Cited Findings
- From the Seamless Mocap Cycles Tutorial (gameanim.com, 2013, originally in 3D Artist #58) — [gameanim.com](https://www.gameanim.com/?p=5168) [primary]:
  - Choose a range of at least three steps with steady momentum, avoiding the acceleration and deceleration at the ends.
  - Bake to the rig, with the key reducer off for fidelity.
  - Zero the translation on frame 1 and align travel to an axis.
  - "Identify two complete footstep actions, (NOT single key poses), that are most similar."
  - "Start and end cycles on a passing pose" so similar poses are most recognisable.
  - Loop by duplicating the clip and dragging the copy's start halfway back across the first, then cross-blending.
  - Blend length is the trade-off: "Longer durations provide most seamless results but will increase foot-sliding." Shorter blends are more stable but make the join more visible.
  - Offset the second clip forward or back to reduce sliding, without deleting the root's forward keys.
  - Fix lateral foot slide on the foot's X curve, "ensuring you don't change the initial or final pose".
  - "Blend across two similarly large motions rather than two subtle ones."
- Put the loop point where foot contact and weight transfer line up, and test by looping several times at the in-game frame rate, not in the authoring preview — [MocapOnline run-cycle guide](https://mocaponline.com/blogs/mocap-news/run-cycle-animation) [community/vendor]
- Combat idles differ from relaxed idles in spine angle, a wider foot stance and breathing rate. Make separate combat idles per weapon type (e.g. bare fists vs sword and shield) — [MocapOnline idle design guide](https://mocaponline.com/blogs/mocap-news/idle-animation-design-guide) [community/vendor]
- Add fidgets or look-arounds on a timer or random trigger, then return to the base idle — [MocapOnline idle loop article](https://mocaponline.com/blogs/mocap-news/idle-animation-loop) [community/vendor]

### Inferences
- **Crouched idle:**
  - Choose the loop span around a breath cycle with clear amplitude, so the cross-blend happens over a "large" motion, per the tutorial's advice.
  - Lock both feet and knees to the ground through the cross-fade, since sliding is the main artifact.
  - Make the idle's first and last frame the same "combat stance" pose that attack clips start and end on, so the engine's transitions blend between near-identical poses.
- **Tool features:**
  - Loop-span finder: score candidate in/out frames by pose plus velocity similarity across a window, not a single pose.
  - Cross-blend with an adjustable length.
  - Foot-lock or slide-fix pass.
  - Seamless-loop preview at 30 fps, playing the loop several times.

### Gaps
- I found no authoritative source (Cooper, GDC) giving breathing-cycle lengths or typical idle loop durations.
- The MocapOnline numbers come from a vendor blog. I found no primary source on combat-stance idle design.

---

## 6. Transitions and blending (blend times, neutral/combat stance start and end, root motion vs in place), plus NWN specifics

### Takeaway
Short full-body blends (God of War: about 3–5 frames into the shield stance) can replace many authored transitions, provided the clips start and end in a shared stance. NWN's ASCII MDL animations carry a per-animation `transtime` (default 0.25 s) and an `animroot`, and are authored at 30 fps. Clip design should target a shared stance pose at both ends and a blend window around 0.25 s.

### Cited Findings
- God of War (2018): rather than "a ton of transition animations" into the shield/defence stance, the team "basically let the blend do the work". Kristjan Zadziuk estimates it as "about a three- to five-frame full-body blend" — [Game Developer, on Zadziuk's breakdown](https://gamedeveloper.com/production/creating-fluid-combat-animations-for-the-new-i-god-of-war-i-) [secondary]
- The recoil phase returns the character to a designated pose, usually the idle, so the next animation transitions smoothly — [gameanim.com Fast vs Slow](https://www.gameanim.com/?p=115) [primary]
- Prince of Persia: Sands of Time uses several animations for one action (drawing a sword) to keep motion fluid without restricting movement — [gameanim.com Fast vs Slow](https://www.gameanim.com/?p=115) [primary]
- The fighting-game workflow identifies "blend frames" and tests every combo variation, so strings branch and still flow together — [Gnomon, Brandon Yates](https://gnomon.thegnomonworkshop.com/workshops/animating-a-3-hit-combo-for-games-in-maya) [course outline]
- MocapOnline suggests [community/vendor; no independent standard found] — [MocapOnline idle guide](https://mocaponline.com/blogs/mocap-news/idle-animation-design-guide):
  - A locomotion-to-idle blend of about 0.2–0.4 s, quicker for faster characters.
  - A large pose gap between clip end and next clip start gives a visible snap even with blending.
  - Combat idles can be layered on the upper body over a relaxed lower body.
- **NWN MDL animation blocks** [community engine docs; nwn.wiki page not fetched, structure per search summary]:
  - Layout: `newanim <anim> <model>`, then `length`, `transtime`, `animroot`, node entries, and `doneanim`.
  - `length` is in seconds. 1 s equals 30 frames at 30 fps.
  - `animroot` is the highest bone the animation drives: the root dummy for full-body clips, lower bones for partial clips.
  - Sources: [nwn.wiki](https://nwn.wiki/x/iYW3); [rollnw Animation struct docs](https://rollnw.readthedocs.io/en/stable/api/struct/nw__model__Animation.html), which list defaults of length 1.0 and transition time 0.25 and store animation events as a per-animation list.

### Inferences
- **Blend window.** A 0.25 s `transtime` is about 7.5 frames at 30 fps, longer than God of War's 3–5 frame blend (if those are 30 fps frames, 0.1–0.17 s). A long incoming blend smears the start of a snappy attack: the first anticipation frames get averaged with the previous pose. Two options follow: shorten `transtime` for attack clips where the engine honours it, or author the clip so the first 0.25 s is a near-stance pose that survives the blend, putting the real anticipation after the blend window.
- **Stance endpoints.** All combat clips (shove, kick, roar, crouched idle) should start and end on the same combat stance pose that the combat idle loops on. That shrinks the pose gap the blend has to bridge, following the vendor note about snaps from large pose gaps.
- **Root motion.** NWN moves creatures from the engine. A shove or kick that steps forward in the mocap will likely need its root translation removed or returned to origin by the last frame, so the creature doesn't pop back at clip end. The tool should offer "zero root travel / return-to-origin" and show root drift.
- **Animation events.** The rollnw docs indicate animation events per clip, which may allow a contact or hit event at the contact frame. Verify this against the engine; it would tie the baked hitstop hold (section 3) to the right moment.

### Gaps
- I didn't confirm whether `transtime` is the cross-fade duration into the clip or a window during which transitions are allowed. The search summary was ambiguous, so the nwn.wiki text needs reading.
- I didn't find how NWN handles root translation in attack clips: whether animroot translation displaces the creature or is discarded.
- I found no Cooper or GDC source on root motion vs in-place in this pass.
- I found no primary source for typical blend durations beyond God of War's 3–5 frames and the vendor's 0.2–0.4 s.

---

## 7. Coverage of the named practitioner sources (Cooper, GDC Bootcamp, God of War, For Honor, Souls/Sekiro, fighting games, Sakurai)

### Takeaway
The strongest primary material retrieved:
- Cooper's gameanim.com: the AC3 mocap pipeline, the "Fast vs Slow" anticipation/apex/recoil framework, and the mocap-cycles tutorial.
- Sakurai's hitstop column.
- God of War's designer-set hit and recovery frames, and its blend-instead-of-transition approach.
- Fighting-game frame-data conventions.

For Honor, Sekiro/Souls and GDC Animation Bootcamp combat talks were not retrieved with usable specifics.

### Cited Findings
- Cooper's *Game Anim: Video Game Animation Explained* 2nd ed. (CRC/Routledge, 2021) includes a mocap chapter enhanced with motion matching — [Routledge](https://www.routledge.com/Game-Anim-Video-Game-Animation-Explained/Cooper/p/book/9780367707651); [gameanim.com/book](https://www.gameanim.com/book/) [primary listing]
- Cooper's game-specific reading of the principles: anticipation is contested, NPC telegraphs run long, follow-through is freer but needs a control-return frame — [search excerpt](https://www.jikguard.com/news/the-12-principles-of-animation-in-video-games) [snippet-only]
- God of War III's GDC 2010 talk on its animation process (Velazquez) — [GDC Vault](https://gdcvault.com/play/1012720/Animation-Process-of-God-of) [listing only]
- PlatinumGames (Takahisa Taura, GDC 2018) presented an in-game animation flagging tool for tuning combat feel — [search result summary listing gameanim.com/GDC coverage](https://www.gameanim.com/?p=18866) [snippet-only; URL association unverified]
- A Valve animator's 80.lv article on hitting poses exists but is not combat-specific — [80.lv](https://80.lv/articles/valve-animator-talks-about-hitting-poses-in-animation/) [not fetched]

### Inferences
- The consensus across these sources:
  - Designers own the contact and recovery frame numbers; animators own posing and spacing inside them.
  - Player anticipation is minimised and NPC anticipation is extended.
  - Impact comes from snap into contact plus overshoot plus a brief freeze/hold plus a victim reaction blended in over a few frames.
  - Mocap is sped up and re-assembled, never used at captured timing.
- A tool supporting this should treat the contact frame, the control-return/recovery frame and the stance endpoints as first-class, editable markers. Retiming, holds, overshoot and loop/blend operations should all be expressed relative to them.

### Gaps
- I found no For Honor animation talk content. A GDC talk likely exists, but no frame data or technique details came up.
- I found no Sekiro or Souls-like animator interviews with concrete timing figures.
- I found no GDC Animation Bootcamp talk text specifically on "polishing mocap for combat".
- I didn't retrieve 11 Second Club or Polycount threads with concrete mocap-polish numbers.
- Sakurai's 2022 YouTube video on hit stop was seen only through a news summary.
