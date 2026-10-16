# Frame pacing: 60 Hz of game, as many pictures as the screen wants

The simulation runs at exactly 60 Hz. The picture runs at the display's rate.
Nothing else changed, and that separation is the whole feature.

## Why the simulation cannot simply be sped up

Every timer in this engine is counted in frames. The DS ran its logic at 30 Hz;
upstream reached 60 by doubling each interval and halving each increment, by
hand, at every site:

```
_cooldownTimer = (ushort)(_data.CooldownTime * 2);          // JumpPadEntity
_timeSinceJumpPad > 7 * 2                                    // PlayerInput
_viewTiltAngleH += Values.ViewTiltIncrement * sign / 2;      // PlayerInput
_frozenTimer = _timeSinceFrozen > 60 * 2                     // PlayerEntityNetAim
```

`grep -rc "todo: FPS stuff" --include=*.cs src/` counts **806** of them. Running
the simulation at 120 would mean redoing all 806 as `* 4` / `/ 4`, and would
also move things that are not in this repository's gift:

- an intent is sent **per simulation frame**, so the wire rate would change
- `NetConfig.ProtocolVersion` would have to move, orphaning every older client
- a demo is a **count of frames** (`DemoFile`), so old recordings would replay
  at the wrong speed
- the DS behaviour this engine reproduces is defined at its own tick rate

So the simulation is not asked to move. Only the drawing is.

## The split

`Scene.OnUpdateFrame()` used to be one call doing both, and `RenderWindow` ran
it at `UpdateFrequency = 60`. It is now three methods:

| Method | Rate | Contains |
|---|---|---|
| `Scene.OnSimulationFrame()` | exactly 60 Hz | input, `NetSession.Update`, `NetHooks`, `UpdateScene`, sound, the clock, `_frameCount` |
| `Scene.OnDrawFrame()` | the display's rate | GL setup, render-item rebuild, camera, projection, `GetDrawItems` |
| `Scene.OnUpdateFrame()` | — | both, in that order |

`OnUpdateFrame` is kept **because the harness calls it**. `NetCheckClient`,
`MapAudit`, `WeaponDps` and `ThumbnailCapture` each drive one call per frame of
their own loop, so their timing is untouched by any of this: same steps, same
order, same packets on the same frame numbers.

`RenderWindow.OnRenderFrame` is where the two rates meet, on a fixed-step
accumulator in `Mods/Render/FrameTiming.cs`:

```
steps = FrameTiming.Advance(args.Time);   // 0, 1 or 2+ per drawn frame
for (i = 0; i < steps; i++) Scene.OnSimulationFrame();
Scene.OnDrawFrame();
```

### A side effect worth having

The game's speed no longer depends on whether the machine can keep up. Update
and render used to be one call, so a box managing 40 fps played the game in
slow motion. The accumulator pays what it owes: `-frametimingcheck` measures
60.000 Hz of simulation at a 40 Hz draw rate.

## Android

The same split, in `GameView.RenderLoop`. That head owns its own thread and its
own EGL context rather than using `GLSurfaceView`, so it had its own pacing
loop -- a `Thread.Sleep` to a hard `1.0 / 60.0` around one `OnUpdateFrame`,
which is why a 120 Hz phone drew 60.

It now runs the same accumulator: `FrameTiming.Advance` on the measured frame
time, N `OnSimulationFrame`, one `OnDrawFrame`. Two things differ from the
desktop:

- **Input is inside the step loop**, not beside it. `ApplyInput` works out this
  step's rising edges from the touch state, so running it per *picture* would
  turn one tap on FIRE into two presses on a 120 Hz screen.
- **In display mode the loop does not sleep at all.** `eglSwapBuffers` blocks
  until the panel is ready, and sleeping as well is double pacing -- it would
  halve the rate. `MinFrameSeconds` is only a floor so that a driver which does
  *not* block (an emulator, a surface with no vsync) spins at 500 Hz rather
  than as fast as the CPU will go.

`Surface.SetFrameRate` (API 30+, best-effort, guarded and caught) tells
SurfaceFlinger what the surface intends, because a phone that can do 120 often
sits at 60 until something asks. Below API 30 the FPS limit still caps the
loop; it just cannot raise the panel.

**None of the Android side has run on a device.** It builds, and the shared
code under it is the same code the desktop measurements were taken on, but the
emulator available here has no extracted game files and so cannot load a match
-- the gap `.claude/android/ANDROID-PORT.md` already describes. Treat the
Android frame rate as untested rather than working.

## The world is carried on, not interpolated

Every picture shows the world at its own instant (`Mods/Render/
ScenePresentation`, `MotionCarry`). The last step recorded every entity's pose
(`EndStep`, keyed by entity; `Scene::AddEntity` starts a new life with no
history); a picture part of the way to the next step moves what each entity
*drew* -- its render items, by their matrix stack or their transform, and the
single particles it added -- by that entity's last-step motion, translation and
rotation, carried that fraction of a step on. A billboard only moves. A replica
on a client is read off the playout clock (`NetSmoothing::AheadShift`), which
already holds that instant; a player simulated here moves on along its own
step; an effect particle is drawn where its speed takes it, unless it stops at
what it hits. A player is carried by its drawn pose (`PlayerEntityPresentation`):
in alt form that is the ball's own transform, roll and spin included -- carried
by position alone, the ball moved every frame and rolled at 60 Hz -- and
Kanden's segments are each drawn part of the way toward the pose the game's own
update gives them next (`MotionCarry::Toward`): at the end of each step
`UpdateStinglarvaSegments` is run once more, one frame on and with the body
moved on by a step, and everything it touched is put back. Carrying them on by
their last motion overshot once a loop -- the wiggle's angle is
`359 * k / (cycle - 1)`, so the DS holds it still for the step it wraps -- and
the tail snapped back. `DrawKandenAlt` draws the carried matrices and puts the
step's back. A morph is not the body's motion, so the body is not carried
across the step its form changes; the camera is, since it moves through one.
Your own body is drawn in first person too while the camera switches, through
a morph and an unmorph (`PlayerDraw`'s `drawBiped`), so it is carried until the
switch ends; left on the step under a carried camera, the unmorphing body shook.
And the unmorph's fade reads the frame the picture shows
(`AnimationBlend::FrameShown`), not the integer animation frame, which moves at
the DS's 30 Hz and stepped the body's transparency with it. Nothing older than the step is shown, so nothing is delayed, and
the entity is never touched: draws compute node matrices that the simulation
reads back, so moving the entity would have leaked the picture into the game.
`-maptest -bots -drawrate N` draws each step at forced instants 0, 1/N, ...
and its report is the same as `-drawrate 1`'s, with the number of drawn items
carried on its `FRAMETIMING` line. `FruityPrime.MotionCarry` checks the
arithmetic.

What follows is why the earlier blend was removed, and why this is not it.

### The interpolation that was removed

There was. Entity transforms and the camera were blended between their last two
simulated states across `FrameTiming.Alpha`, so that a 144 Hz picture of a
60 Hz world showed 144 distinct positions a second rather than 60 shown twice.
It was removed **completely** -- `EntityBase`'s capture and blend, `CameraInfo`'s
`ModGetDrawView`, `Scene.CaptureDrawState`, `ModAttachToDrawnView`, the setting,
the `-interpolation` switch and the harness assertions that went with it.

It was removed because of what it did to **pooled entities**, which is most of
the things a shot is made of. `BeamProjectileEntity`, `BeamEffectEntity` and the
effect entries are taken off a free list and reused: an entity coming back into
the world still holds the transform history of its last life, at wherever that
one died. The blend guards catch a jump of more than 24 units, so a reuse
*further* away than that was drawn correctly -- and a reuse nearer than that,
which is the common case in a firefight, was drawn somewhere between the two.
That is exactly what was reported: impact effects that did not land on the wall,
and shot artifacts drifting about in all directions while moving and firing.

Putting it back needs an answer for entity reuse first -- `ModResetDrawState`
existed and nothing called it from the pooling paths -- and is not worth it for
what it buys. The extra frames are still worth having without it: the camera and
the world are sampled at 60 but the *input-to-photon* path is not, and the
picture is still drawn at the display's rate.

### Animation curves are blended, and why that is not the same thing

A player choice: the settings' **Animation** slider (Display -> Performance)
sets how often a draw may show a new pose between two frames, in Hz, on the
**same stops as the FPS limit** (`FrameTiming::RateStops`) but with a value of
its own -- an unlimited frame rate with 30 Hz animation is a valid answer. The
interval between a model's two ticks, in seconds, times the rate is how many
slices it is cut into, and the blend steps from slice to slice: 30 Hz on 30 Hz
frames is one slice, which is the frame itself (the DS's own look), 120 Hz is
four, and the stops' *Unlimited* reads as **Display rate**, blending at any
point. `AnimationBlend::Rate` holds it; `launcher.txt` keeps it as
`animation_rate=display|N` and `-animrate display|N` overrides it for a run.
Ticks are noted at any rate, so a change shows on the next draw.

The game advances integer animation frames at the DS's 30 Hz (`EntityBase::
Process` on every other step). `Mods/Render/AnimationBlend` draws three of a
model's animation channels *between* the frame they are on and the frame the
next tick will show, by how far the draw has run toward that tick: skeletal
poses (`Model::AnimateNodes`/`AnimateNodes2`), material colour and alpha
(`AnimateMaterials`) and texcoord scroll/scale/rotation (the texcoord matrix in
`EntityBase::GetTexcoordMatrix`). Hunters, enemies, bosses, platforms, rooms'
animated surfaces and the launcher's hunter all go through one `Pass` hook per
channel and one in `InterpolateAnimation`; each channel reads its own slot.
Texture swaps stay on integer frames -- there is nothing between two pictures.
It answers both objections above:

- **It looks ahead, never behind.** No previous state is kept. The next frame is
  asked of the game's own `UpdateAnimFrames` (loop, reverse, ping-pong, no-loop,
  pause) when a tick lands, and a draw blends a channel only while its slot's
  frame and flags and its group still match what that tick left. A pooled model that was reset
  or re-animated simply is not blended until its next tick.
- **Nothing the simulation reads changes.** Integer frames are untouched, and the
  node matrices and material colours a blended pass wrote are put back to their
  integer values when the draw ends (`DrawScope`), unless something later in the
  draw replaced them -- a platform's attached collision and Fire Spawn's wrists
  read matrices a draw left behind, and Gorea's states read its arms' alpha. A
  texcoord matrix is returned into the draw item and leaves nothing behind. `Enemy18Entity::EnemyGetDrawInfo` derives a shot origin from its
  pose during the draw, so it opts out (`Suppress`). Measured: `-maptest -bots
  -drawrate 3` reports the same match before and after.

Time comes from `AnimationClock`: the simulation's steps once per 60 Hz step
(`SceneAnimation::Step` in `OnSimulationFrame`), and a draw sits
`FrameTiming::StepPhase()` past the newest step (`SceneAnimation::Draw` in
`GetDrawItems`). Each instance records the interval between its last two ticks,
so a 30 Hz and a 60 Hz cadence both blend correctly. `FruityPrime.AnimationBlend`
tests it, and `fruity_animation_blend_tests <AMHP1 dir>` adds every hunter's
real idle, poses and colours.

What is still drawn at the simulation's rate, deliberately: entity and camera
transforms and effect particles (60 Hz, see above -- pooled), HUD sprite frames
and texture swaps (discrete), and effect spawns and beam trail samples (30 Hz,
the DS's own cadence).

### Mouse look turns at the display's rate

The simulation turned the aim once per 60 Hz step, from the mouse motion
collected before it, and built the camera in that step. A frame drawn with no
step behind it -- most of them above 60 Hz -- showed the view the last step
left: on a 144 Hz screen the picture of a turn was a mean 5.2 ms and up to
13.9 ms behind the mouse, and its speed went in steps.

`Mods/Input/FrameAim` turns the actual aim at the display's rate. The mouse is
read once a frame at one fixed instant -- the frame's input pump, after the
latency wait -- and `Scene::ModApplyFrameInput`, before the frame's steps, turns
the aim by the motion since the last application through the game's own
`UpdateAimY`/`UpdateAimX`, then rebuilds what a step builds from an aim (the
gun's basis, `UpdateAimVecs`, the muzzle and charge effects). A step applies
only the share of its mouse delta the frames have not (`ModStepMouseAim`); its
other readers of that delta -- input seen, HUD sway, the aim frame -- still see
the whole of it. The camera the last step built is turned to the frames' aim
in `Scene::TransformCamera` (`FrameAim::View`). With no frame application the
step's arithmetic is bit-for-bit what it was. Only where turning in pieces is
turning at once: first person, mouse aim, exact rules, fixed crosshair (Pro
mode HUD). The DS-style crosshair is presented instead (below). `-frameaim off`
is the control for both.

Two earlier shapes were tried and dropped, and why matters. **Predicting** the
next step's turn for the picture alone (and turning the gun to match) was exact
to 0.0001 degrees, and still made the map and the gun shake: **reading the
mouse again right before each step and each draw** put the sample at a
different point of every frame -- later on a frame with a step, by however long
the step took -- so the picture turned by uneven amounts at an even refresh.
Reading once, at the same instant every frame, is what makes a turn even.

**The frame's camera takes `UpVector`, never `TrueUp`.** `CameraInfo::Update`
makes `TrueUp` perpendicular to the step's own sight line, which near straight
up or down is that sight line's horizontal heading. Reused for a frame whose aim
had turned since, it rolled the picture -- by up to 40 degrees against the
camera the next step builds -- and the step put it back: a shake that grew the
nearer the view was to the vertical. From `UpVector` LookAt makes the up the
step would make for the new target; the check compares the whole view, roll
included, and holds it within 0.001 degrees above 70 degrees of pitch.

**And the strafing lean turns with the heading.** `UpdateCameraFirst` leans
`UpVector` across the step's heading while strafing (`_viewTiltAngleH`), so
its horizontal part points along that heading's perpendicular. Kept as it was
while the frames turned the heading, the lean pointed the wrong way -- up to
0.28 degrees of roll against the step's camera -- and the world wobbled when
turning while strafing. `FrameAim::View` turns the horizontal part of
`UpVector` about the vertical by the heading's own turn; the check holds it
within 0.0001 degrees.

### The DS-style crosshair is presented, not turned

Without the Pro mode HUD the view trails the aim: every `UpdateAimY` and
`UpdateAimX` ends in `UpdateAimFacing`, which holds the view inside 15 degrees
of the aim and then eases it a tenth of the way there -- twice a step, once
per axis. Turning that in pieces changes the ease, so the frames do not turn
the aim. Each picture **shows** it instead, and the step finds exactly the
state it left.

`Scene::ModPresentFrameInput`, after the frame's steps, has the player save
what a picture changes, then run the next step's own turn for the motion since
the snapshot that step will consume -- the same `UpdateAimY`/`UpdateAimX` --
so the crosshair is where the mouse has put it. The view reads the same ease
in continuous time. A step keeps k = 0.81 of the view's offset from the aim
and lags an aim that moved by d by b·d -- b = 0.81 for a pitch, which both of
the step's eases see, and 0.9 for a yaw, which only `UpdateAimX`'s does --
so over a fraction p of a step the offset kept is k^p and the lag is
c(p)·d, c(p) = b(1 - k^p) / ((1 - k)p). A steady turn therefore keeps a
steady offset and the view moves evenly, and at p = 1 it is the step's own
view for that aim: the picture is written as that view plus terms that
vanish there, so the two meet exactly. Against the 15 degree envelope the
step clamps the offset and eases it back every time, a 60 Hz sawtooth, so a
picture is never further behind the aim than the steps on either side of it
leave it. The first version eased from the last step's view towards the
next one's along the ease's own curve, which is exact at both ends and
turned the view about 10% faster early in each step than late -- the world
trembled at 60 Hz. Zoomed, the step puts the view on the aim, and so does
the picture. The gun is rebuilt from that view, `Scene::ModPresentFrameReticle`
projects the reticle through the picture's own camera, and the
`FrameAimPresentation` the render loop holds puts every field back once
`OnRenderFrame` is done -- unless a respawn or a room load wrote an aim of its
own meanwhile, which is then the one kept. Nothing here is behind the mouse:
the crosshair is the frame's reading, and the view is ahead of the last step,
never between two old ones.

### Idle camera sway follows its curve between steps

Idle sway advances at 60 Hz (`SwayIncrement` is halved per step). Its camera
direction used to remain on those steps while mouse frame aim was eligible,
even with no mouse motion. `Mods/Input/PlayerEntityIdleSway` now owns its step
and shared cosine sampler. A picture reads the current segment at its own
phase, clamps at the known endpoint, and carries the camera and gun together.
The next random target is chosen only by the simulation. Both HUD styles use
this presentation; fresh mouse motion takes priority, and zoom, morph cameras,
cutscenes, frozen players and the idle-sway setting keep their existing gates.
The picture restores the step's camera, aim, gun and muzzle afterwards.
`-aimcheck` covers intermediate poses, endpoint/rollover agreement, gun placement,
mouse takeover and unchanged state/RNG.

The HUD sway (`UpdateHudShift`, an average of eight steps' motion, rounded to
DS pixels) stays the step's.

### The free camera moves by the frame's time

`Scene::MoveRoamCamera` used to run once a step, a step's worth of movement
each time. It is a picture and nothing else -- no entity, no network, no
random number -- so it now runs once a frame, before the steps, scaled by the
frame's own time in steps (`FrameTiming::FrameSteps`, capped as the steps
are). A second's flight covers the same ground at any frame rate. Where the
fixed-step loop is not running (the harness clients), it is still called from
the step, as before.

### A watched player is shown where the playout has them now

Spectating, the camera is the watched player's own, and the step builds it
**before** `NetHooks::AfterSimulation` moves and turns that replica to the
snapshot: so the view was a step behind the body it belonged to, and then
held still until the next step.

`NetPlayerBridge::ApplyState` notes what a replica's camera was built from
before it moves it (`ModNoteCameraSource`). A picture then moves the eye to
where the body is now, and on along the playout clock to the picture's
instant -- `NetSmoothing::SampleAhead` reads the same ring the step reads, up
to one step past the read point and **never past the newest snapshot**, so
nothing is guessed and nothing is held back -- and turns the sight line to
the facing the snapshot applied. The view is `FrameAim::Moved`, the same
LookAt the step makes there (within 0.0002 degrees), the watched gun and
muzzle are carried with it rigidly, and it is all put back after the picture.
The ball's camera trails rather than looks along the facing, so in alt form
only the eye moves.

What this cannot do is turn a watched player between snapshots: their aim
reaches this machine 60 times a second, and turning it in between would be a
guess or a delay.

### Your own eye is carried on between steps

The step builds your camera from your body (`UpdateCamera`: position, walking
bob, landing dip), so between steps the eye stood still while the view turned
every frame: walking or strafing while turning, the eye jumped forward 60
times a second under a smoothly turning view and the whole map shook -- which
the free camera, moving its eye every frame, never did. Your body has no
future to read, so a picture carries the eye on along its own last step's
motion (`CameraInfo::Position - PrevPosition`) by the fraction of a step it is
into (`PlayerEntity::ModPresentOwnView`), the gun and muzzle with it, and puts
the step's camera back once it is drawn. Nothing is behind the body; the error
is the next step's change of speed, at most, and a jump no body makes in a
step (`MotionCarry::JumpDistance`: a teleport, a respawn) is not carried.

What the camera looks at is carried the same way, from the pose history the
step records (`PlayerPoseHistory`: eye and target): an alt-form camera swings
round the ball and Trace's turns with the mouse, and carried by its eye alone
the view still turned 60 times a second under a ball and a world that moved
every frame -- the worst of the alt-form shake. Only where this frame's own
mouse turns the sight line (FrameAim) does the target simply go with the eye.
The gun rides the carried view rigidly (`ModCarryCamera`, shared with the
watched view). On a steady walk the
picture is the camera the step builds at that instant (0.0001 cm).

The body is carried the same way (`Position - PrevPosition`, and the collision
volume the shadow is drawn from), because wherever it is drawn -- the ball, a
third-person view -- a carried camera over a body left on the step makes the
body shake on screen, which is what alt form did until it was.

**And an effect placed between steps has to be drawn there.**
`EffectEntry::Transform` writes each element's `OwnTransform`; the `Transform`
its attached particles are drawn with is copied from it by
`Scene::ProcessEffects`, once a step. A picture that moved the gun moved the
charge glow's `OwnTransform` with it and left the glow drawn where the step
had it, trailing the muzzle. Every placement a picture makes goes through
`ModPlaceGunEffects`, which copies the transform too.

What still moves at the simulation's rate: what an entity draws from anything
but its own transform (a shot's trail is carried with the shot; an entity that
rewrites its transform while drawing is not carried), another player's facing
and a watched player's aim, and the screen shake, which `CameraInfo::Update`
applies on every other camera update -- 30 Hz, as the DS did.

Measured by `-aimcheck` (`AimCheck::CheckFrameAim`, real player): frames along
a mouse path then a step end where one step over the whole path ends, a step
after the frames adds nothing (it fires along the aim on screen), the view
looks along the frames' aim, and the gun and muzzle are where the step puts
them. On a constant 180 degree/s turn read once a frame, the picture's age goes
to 0, the per-frame turn becomes even, and what is seen matches where the next
shot goes (see the check's `FRAMEAIM schedule` lines for the before).

For the DS-style crosshair (`FRAMEAIM ds`): a picture leaves the step's state
exactly as it was, the crosshair shown is the aim the next step turns to, the
view reaches the step's own as the step runs and starts from it, held inside
the envelope, and 240 steps with pictures between them are bit-identical to
the same steps without. At 240 Hz the crosshair's age goes from a mean 6.25 ms
to 0, the view moves on every frame where it stood still on 450 of 600, and a
steady turn's per-frame step varies by 0.1% at 60 degrees a second and 0.8%
at 180, against the envelope.
For a watched player (`FRAMEAIM watched`): the picture is the camera a step
builds where the player is shown, and their gun stays where it sits in it.
`FruityPrime.NetworkSmoothingTimeline` checks that a picture's playout read is
at most a step ahead, never past the newest snapshot, and counted nowhere.

## Timers that live in the draw pass

Two were found by audit and both are handled by counting steps owed rather than
by moving the code:

- **`UpdateFade`** decrements `_fadeDelay` and calls `EndFade`, which ends
  cutscenes and changes rooms. Left as one per drawn frame it would expire two
  and a half times early on a 144 Hz screen. It now consumes
  `_pendingFadeSteps`.
- **`ProcessEffects`** advances particles, and is called from inside
  `GetDrawItems` where its ordering against the entity pass matters. It now
  runs `_pendingEffectSteps` times, in the same place.
- **The pause map's animations** (`GetPauseMapRenderItems`) advance from
  `Scene.FrameTime` in the draw call; the branch hands them the steps actually
  taken.

`grep -rn "\.FrameTime"` over the rest of the tree finds 46 uses and none of
them in a draw-path method.

A second audit, of every `Draw*`/`GetDrawInfo` method that advances anything,
found five more that played faster the faster the screen, all behind an
even-frame or `ProcessFrame` check that was once-per-frame only while drawing
was:

- **Gorea 1's arm regeneration, Gorea 1B's grapple beam and mind tricks**
  advanced their models from the draw on every draw of an even step.
  `SceneAnimation::TickFromDraw` ticks at most once per step, on the
  simulation clock, so they are blended like the rest.
- **The pause map's Octolith icons and the quit dialog's button**
  (`DrawPauseMenuForeground`, called from `OnRenderFrame`) ran
  `HudObjectInstance::ProcessAnimation`, a 1/60 s timer, once per picture.
  `SceneAnimation::StepsForDraw` runs it once per step taken since it last ran.

Material colour updates in those draws (`UpdateArmMaterials` and the like) are
recomputed from simulation timers each time and are not affected.

**Frame advance** (the debug single-step) is forced back to one step per
picture: the request to advance is consumed *after* the frame is drawn
(`AfterRenderFrame`), so a picture drawn with no step behind it would eat it
before the simulation ever saw it.

## Settings

| Where | What |
|---|---|
| Launcher → Settings → Performance | **FPS limit**, a slider directly under Render scale over the stops Display (VSync) / 30 / 60 / 75 / 90 / 100 / 120 / 144 / 165 / 180 / 200 / 240 / Unlimited |
| `settings.json` | `FrameRateCap` (`display` or a number) |
| `-fpscap N` / `-fpscap display` | for the paths that never open a launcher |

**Display (VSync) is the default**, and is the only tear-free setting: an
explicit number turns VSync off, because asking for 120 on a 144 Hz screen with
VSync on gets you 72. OpenTK 4.9 no longer separates its update and render
ticks -- `RenderFrequency` is deprecated and the two callbacks fire together --
so `UpdateFrequency` on the window is the *frame* rate, and 0 means "as fast as
it will go".

The on-screen FPS counter reports the **picture**, since `CountFrame` runs in
`Scene.OnRenderFrame`. The simulation rate is not visible to a player at all,
which is why it goes to the debug log.

## How it is tested

Neither half needs a 144 Hz monitor.

**`FruityPrime -frametimingcheck`** runs the accumulator alone against frame
times chosen rather than measured. The half that can silently be wrong is
arithmetic: a game running at 60.4 Hz loses a second every two and a half
minutes, is invisible in a screenshot, and is fatal to a match clock.

**`FruityPrime -maptest "ROOM" -players 8 -drawrate N`** draws each simulation
step N times, which is what a 144 Hz screen does to a 60 Hz game. It asserts the
one thing that can silently be wrong: that the simulation's frame counter did
not move during a draw (`draws advancing the game: 0` -- any other number is a
`MAPFAIL`). Everything the MAPTEST line reports must be **identical** to the
`-drawrate 1` run, and that is meant literally: same frame count, same
`spawned 8/8`, same `moved`, same `deaths`, same affliction results, same lit
percentages to a decimal. Drawing the world three or four times as often must
change nothing about what the world did.

**The scoreboard is drawn on every `-maptest` run.** It is opened by *holding* a
button, and the main player's buttons are refilled from the keyboard at the top
of every simulation step, so nothing the tour writes to `Controls.Pause`
survives to be read -- which is why the one screen a player opens by holding a
button was the one screen no check had ever drawn.
`PlayerEntity.ModForceScoreboard` is the way in, and `MapAudit.StepScoreboard`
holds it open over two windows of each run. A run that never drew it is a
`MAPFAIL`, so the coverage cannot quietly go away.

**`FruityPrime -room "ROOM" -fpscap N -debuglog`** is how the *output* rate is
confirmed to be what it claims. The log's `frametiming` lines carry both rates
and the steps-per-frame histogram, and the histogram is the proof: a frame that
ran **zero** simulation steps is a picture that a 60 Hz loop would never have
produced. Measured here (WSL, Mesa llvmpipe, software rasteriser, `-fpscap 240`):

```
sim 60.14 Hz / draw 81.0 Hz, 1321 steps over 1848 frames, 2 dropped, 0 stalls,
steps per frame [534, 1310, 3, 0, 0, 1], cap 240
```

534 of 1848 frames drew without a step behind them. The simulation held 60.14 Hz
while the picture ran at 81 -- and 81 is this box's software rasteriser, not the
loop: the cap was 240 and dropping the render scale to a quarter only moved it
to 88.

## The rule the split actually rests on

**Nothing in the draw pass may change the game, and nothing the draw pass owns
may be cleared outside it.** The first half is asserted by `-drawrate`. The
second half is the one that bit, and it bit hard enough to be worth stating on
its own.

Visual effects built in `GetDrawInfo()` must not advance mutable game state or
consume global RNG according to render count. Lockjaw trail noise is derived
from its simulation tick and link identity, so repeated draws of one tick have
the same geometry and leave `Rng.Rng1` unchanged.

`-frametimingcheck` checks the deterministic noise without game assets. With
Lockjaw links active, `-maptest "TEST ARENA" -hunter Sylux -drawrate 2` (or 4)
also compares the actual `TrailMulti` vertices across draws of one simulation
step and checks RNG specifically around `BombEntity.GetDrawInfo()`. The report
prints the number of active trail checks; zero means that run did not cover the
effect.

The single-particle table -- `Scene.AddSingleParticle`, filled by
`BeamProjectileEntity.Draw*`, `PlayerScan.DrawScanModels` and `PlayerDraw`'s
death sparks -- is written during the entity draws and read at the end of the
same pass. Its counter was reset in `OnSimulationFrame`, where it had always
lived when a step and a picture were the same call. Once they were not, a
picture with no step behind it -- most of them, at any rate above 60 -- drew the
previous frame's particles again at the positions they had *then*, and went on
adding to a table nothing had emptied until all 200 entries were used and every
new particle was silently dropped. On screen: shot trails doubling and drifting,
and the first Shock Coil of a fight not appearing at all. The reset now sits at
the top of `OnDrawFrame`, next to the render-item clear it belongs with.

When adding anything with per-frame draw state, ask where it is cleared. If the
answer is "in the step", it is wrong.

### The counter both halves have to agree on

The same split broke effects in a way nothing about it looks like a frame-rate
bug, and it is the more instructive of the two.

Effect elements spawn particles on every *other* step -- the DS ran effects at
30 Hz, and upstream's doubling to 60 is a parity check. An element records the
parity it was created with (`entry.Parity`), so that **its first advance is a
spawning one**; a great many elements put their whole burst out on that first
advance, through a function that returns its value once and zero thereafter.
Miss it and the element emits nothing at all, for its whole life.

Both halves used to read `_frameCount`, and upstream incremented it *after*
`GetDrawItems()`. So a spawn during step N and the `ProcessEffects` that
followed it later in the same frame both saw N, and the parity matched. Moving
the increment into `OnSimulationFrame` put it **before** the draw: every
element was created at N and first advanced at N+1, every first advance failed
its own parity check, and the bursts stopped.

What that looks like from a chair: no flash on the tip of a charging Missile,
and no explosion where a rocket hits a wall -- while the *continuous* elements
of the same effects, the smoke and the debris, carry on exactly as before. So
it reads as "some of the effect is missing", and every number the harness
measured was unmoved: the shots still flew, hit, and did damage.

`_effectFrame` is the fix: a counter the effect system owns, bumped once at the
top of each simulation step, read by both the spawn and the advance, and passed
into `ProcessEffects(effectFrame)` so a catch-up frame advances each owed step
under its own number.

Measured on MP3 PROVING GROUND, 8 players, the same deterministic tour:

| | Effect particles spawned |
|---|---|
| Broken (parity off by one step) | **776** |
| Fixed | **1254** |

The 478 are first-advance bursts, and the rate is otherwise identical -- which
is why nothing else moved. `-maptest` now reports `effect particles` and
`MAPFAIL`s on zero.
