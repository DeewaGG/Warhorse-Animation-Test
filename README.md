# Senior Technical Animator Test - Make It Hurt, Make It Real
## Technical Documentation

**Candidate:** Salvador Marin Sanchez  
**Engine:** Unreal Engine 5.7  
**Module:** `SalvadorTest`  
**Date:** May 2026

---

## Table of Contents

0. [Quick Start](#0-quick-start)
1. [Design Philosophy and Approach](#1-design-philosophy-and-approach)
2. [System Architecture Overview](#2-system-architecture-overview)
3. [Feature 1: Targeted Hit Detection - ANS_Attack](#3-feature-1-targeted-hit-detection--ans_attack)
4. [Feature 2: Weapon Contact IK - ThrustSystem](#4-feature-2-weapon-contact-ik--thrustsystem)
5. [Feature 3: Procedural Hit Reactions - HitReactionComponent](#5-feature-3-procedural-hit-reactions--hitreactioncomponent)
6. [Feature 4: Targeting System - Lock-On and Visual Feedback](#6-feature-4-targeting-system--lock-on-and-visual-feedback)
7. [Feature 5: Death and Full Ragdoll Transition](#7-feature-5-death-and-full-ragdoll-transition)
8. [Feature 6: Chaos Cloth - Bandana Simulation](#8-feature-6-chaos-cloth--bandana-simulation)
9. [Shader Animation - Material-Driven Visual Feedback](#9-shader-animation--material-driven-visual-feedback)
10. [AnimGraph Architecture - ABP_Base Pipeline](#10-animgraph-architecture--abp_base-pipeline)
11. [IK Nodes Pattern - Stateless Libraries](#11-ik-nodes-pattern--stateless-libraries)
12. [Trade-offs and Design Decisions](#12-trade-offs-and-design-decisions)
13. [Hardest Technical Problems](#13-hardest-technical-problems)
14. [Known Issues and Limitations](#14-known-issues-and-limitations)
15. [Future Improvements](#15-future-improvements)
16. [Content Browser Map](#16-content-browser-map)
17. [Feature Checklist](#17-feature-checklist)
18. [Use of AI](#18-use-of-ai)

---

## 0. Quick Start

### Requirements
- Unreal Engine **5.7**
- Visual Studio 2022 with the **Game Development with C++** workload (or any other UE5-compatible compiler)

### Opening the Project
1. Right-click `SalvadorTest.uproject` -> **Generate Visual Studio project files** (or run `UnrealBuildTool -projectfiles` as described in the build notes)
2. Open `SalvadorTest.uproject` in UE 5.7
3. If prompted to compile missing modules, click **Yes**
4. Once the editor loads, open **`LV_FightArena`**
5. Press **Play** or Right Click -> Play form here

### Controls

| Input | Action |
|-------|--------|
| **Right Mouse Button** (hold) | Aim / lock on to nearest enemy |
| **Left Mouse Button** (while aiming) | Attack |
| **Mouse movement** | Camera and character rotation |
| **WASD / Keyboard arrows** | Player Movement |

No jump or crouch are implemented - they were not needed for this project's scope.

To trigger all five systems in sequence: hold RMB to lock on -> LMB to attack -> observe weapon contact IK, hit reaction, foot repositioning, and (on the third hit) ragdoll.

---

## 1. Design Philosophy and Approach

The brief asked for a hit reaction system that feels real. My first decision was to define what "real" means in a melee context: bodies do not play canned reaction animations when struck. They resist, yield, stumble, and fall under physical laws. Every feature in this submission was built around that single constraint - **no reaction animations, only physics and procedural IK**.

This is a harder road than a state machine of hit-reaction montages. Chaos physical animation blending is unpredictable at the joints it bridges between simulation and animation, and procedural IK goals that are one frame wrong ruin the illusion entirely. But the payoff is that every hit reads differently depending on where you struck, at what angle, and what the character's skeleton was doing at that moment. That variance is what makes it feel real.

The secondary goal was **authoring clarity**. If a system can only be maintained by the person who built it, it has failed. Every subsystem is either a data-driven C++ component configured entirely from editor-exposed properties, or a stateless Blueprint function library that any animator can call from a Blueprint without touching C++. The entire combat IK pipeline is accessible to content creators: the BP/ABP owns the state structs, the C++ libraries own the math.

One practical constraint worth naming: **all logic in this project was authored from scratch**. The only external content used was the character mesh, skeleton, animations, and a base scene. The physics asset (`PA_Kwang`) was built entirely from scratch - every physics body, constraint, physical animation profile, and ragdoll profile was authored and tuned manually. Getting the physical simulation to blend cleanly against animation required iterating on capsule sizing, bone groupings, and motor constraint stiffness until the partial simulation regions had no violent discontinuities at their boundaries.

---

## 2. System Architecture Overview

```
APlayableCharacter --------------------------------------------------------------+
  +- UTargetingSystemComponent    sphere-sweep lock-on (ECC_GameTraceChannel1)   |
  +- UHitImpactComponent          attacker-side IK (ThrustSystem)                |
  \- Camera / input (mouse yaw -> TurningSpeed -> ABP)                           |
                                                                                 |
ANS_Attack (AnimNotifyState)                                                     |
  +- TraceForBone()   3-pass box trace -> hit bone + direction                   |
  +- TraceForTarget() channel-3 trace -> TargetComponent slot (Top/Mid/Bot)      |
  \- NotifyTick()     dispatches HitW_Physics() and HitImpact()                <-+
                 |                              |
                 v                              v
BP_Victim                             BP_PlayerCharacter
  \- UHitReactionComponent              \- UHitImpactComponent
       Chaos physical anim                    UThrustSystemNodes
       curve-sampled stunt                    additive IK goals via reflection
       reactive foot repositioning
       low-health idle sim
       ragdoll transition

UAnimInstanceBase (C++ base)
  \- ABP_Base (Blueprint, thread-safe AnimGraph)
       LinkedLayer ALI_Character::FullBody
       LayeredBoneBlend UpperBody montage
       ModifyBone foot/pelvis/spine (additive)
       IKRig goals: feet, pelvis, hands (dom+slave) - all additive offsets
```

The hit event propagates in one direction and has no shared mutable state between attacker and victim. `ANS_Attack` is the only coupling point; it reads from both sides' components and dispatches to each independently. This means a victim can be hit by multiple attackers simultaneously without state corruption.

### 2.1 Level Setup - LV_FightArena and the Demonstration Sub-Level

The test environment is split across two levels. `LV_FightArena` is the persistent level; it references a sub-level called `Demonstration`. There are two separate test areas:

- **Near origin (0, 0, 0):** A dressed arena set used for the primary demonstration of all systems - the intended showcase environment.
- **Far coordinates (large world-space offset):** A bare landscape with a set of test dummies and a dedicated Player Start. This area exists specifically to validate that every world-space calculation in the system - ThrustSystem socket feedback, reactive foot anti-slide, pelvis delta tracking, ragdoll impulses - remains correct at large actor positions where single-precision floating-point precision degrades.

All IK math in the project operates on world-to-component-space conversions (`InverseTransformVector`, `InverseTransformPosition`). These functions operate on deltas and local offsets rather than raw world coordinates, which keeps the precision loss bounded to the component transform itself rather than accumulating across the full world-space position. The far-coordinate test area was used to catch any code paths that accidentally held raw world-space FVector values in a float, which would lose sub-centimetre precision at distances beyond ~100 000 cm from origin.

---

## 3. Feature 1: Targeted Hit Detection - ANS_Attack

`ANS_Attack` is an Anim Notify State that runs a weapon sweep every tick while the notify window is active. Three separate functions keep the logic auditable.

### 3.1 TraceForBone - Three-Pass Cascade

The core problem with fast weapon traces is temporal: `Received_NotifyTick` does not fire at a guaranteed rate, and a sword moving at high speed can visually pass through and contact a target in the time between two consecutive ticks. On that tick, a small trace may not intersect the mesh at all - the sword has already moved past the contact point - even though the hit is visually valid. The solution is to run **three traces at different sizes against the same start/end/orientation**, increasing the spatial coverage on each tick to compensate for the temporal gaps:

| Pass | Half-Extent (cm) |
|------|-----------------|
| 1    | 35 x 3 x 0      |
| 2    | 25 x 3 x 0      |
| 3    | 15 x 3 x 0      |

All three traces share identical start/end points and the weapon socket's world rotation as orientation. Each pass is attempted in sequence; the first one that returns a valid bone name (`!= "None"`) wins and short-circuits the remaining passes. The larger boxes extend the detection volume around the blade, ensuring that a hit the player clearly saw land is not missed simply because the tick fired a frame too late. The flattened Z dimension (0 extent) keeps each sweep planar to the blade, avoiding false positives behind the blade's spine.

### 3.2 TraceForTarget - Slot-Aware Contact

A second trace on `TraceTypeQuery3` (mapped to `ECC_GameTraceChannel1`) targets `UTargetComponent` objects specifically - the visual lock-on slots on enemies. Hitting a `TargetComponent` yields the `ETargetSlot` enum value (`Top`, `Mid`, `Bot`) which directly sets the `AttackSide` parameter to `HitW_Physics`. This coupling is intentional: the animator places target slots where the victim's body regions are, and the hit system reads those placements rather than guessing from bone proximity.

### 3.3 GetHitDir - Blade-Relative Direction

Hit direction is derived from the `weapon_r` socket rotation, not the velocity vector. Two modes cover the common attack geometry:

- **BladeForward**: returns the socket's up vector - correct for thrusting and overhead strikes
- **BladeFront**: returns the socket's right vector negated - correct for horizontal slashes

The `AttackStrength` float curve on the attacking montage feeds `HitStrength` via `GetCurveValue`. This means the designer can author variable impact force across the swing arc directly in the animation curve editor - peak force at the mid-swing connection point, lighter force at the start and end of the window.

### 3.4 Guard Condition

The entire tick is gated on `montage play rate > 0`. This prevents the trace from firing during a montage pause or reverse playback (which is used in the recovery phase described below). Without this guard, a reversing montage that passes back through a notify window would register phantom hits.

---

## 4. Feature 2: Weapon Contact IK - ThrustSystem

When the attacker's weapon connects with the victim, the attacker's hands need to stay on the victim, not float through it. This is the job of `UHitImpactComponent` and the `UThrustSystemNodes` library.

### 4.1 Three-Phase State Machine

At the start of `HitImpact`, the attacker's `CharacterMovement` is set to `DisableMovement`. This lock persists through all three phases and is cleared by `SetMovementMode(MOVE_Walking)` when `ThrustRecover` completes. The lock prevents the attacker from drifting away from the victim during the plant, which would otherwise pull the socket-feedback accumulator out of convergence range.

`ThrustSetUp` determines the precise IK target before Phase 1 begins. It iterates all sockets on the hit bone and selects the closest one to the raw hit location. If no socket is found on that bone, it falls back to the bone origin. If the victim has no skeletal mesh reference, it falls back to the raw hit location. Socket-based targets are more stable than raw bone origins because rigger-placed sockets sit at deliberate contact points on the blade rather than at the bone pivot.

The thrust sequence has three explicit phases managed by `FThrustState`:

**Phase 1 - Approach (`ThrustTick`):**  
`ThrustTick` converts `HitReachDelay` into a discrete frame count on the first tick (avoiding floating-point time drift across frames). Each frame, a socket-feedback error vector is computed - the world-space delta from the contact socket's current position to the live victim bone location, expressed in component space - and accumulated into a running additive IK offset that ramps from zero to full correction across the `TotalFrames` window:

```
SocketErrorCS = CompTW.InverseTransformVector(LiveTargetWorld - SocketWorldCur)
DomAdditive   = (PreviousAdditive + SocketErrorCS) x Alpha        // Alpha: 0->1
```

Rotation is handled separately: a delta quaternion between the weapon's current forward and the target direction is computed in world space, then conjugated into component space and applied additively on top of the rest rotation - ensuring the hand aims the blade at the impact point without collapsing the elbow IK chain. The slave hand copies the same additive offset so both hands move as a rigid unit.

At `Alpha = 1.0` (final approach frame) the additive goal is identical to what `ThrustPlant` would write on its first frame. **There is no discontinuity at the phase handoff.**

**Phase 2 - Plant (`ThrustPlant`):**  
On the first plant frame, three world-space anchors are computed and stored:

```
PlantedTargetBoneWorld = VictimMesh->GetSocketLocation(TargetBone) + TargetBoneOffset
PlantedDomRotWorld     = CompQ x PlantedRotCS.Quaternion()      // world-stable rotation
PlantedHandWorldPos    = -(PlantedDomRotWorld.RotateVector(SocketRelativeLocation))
```

Every subsequent plant frame re-derives the additive goal using a **socket-feedback loop** - it does not freeze the CS value. Instead it reads the contact socket's actual world position (which reflects the previous frame's IK solve), measures the error to the current target bone position, and accumulates it into the running additive goal:

```
SocketErrorCS = CompTW.InverseTransformVector(CurrentTargetWorld - SocketWorldCurrent)
DomAdditive   = GetAnimVec(AnimInst, DomLocGoal) + SocketErrorCS
```

This one-frame feedback loop converges to zero socket error regardless of how the character's spine or camera rotates between frames. The rotation goal is re-derived from the world-anchored `PlantedDomRotWorld` each frame via `CompQ.Inverse() x PlantedDomRotWorld.Quaternion()`, then expressed as an additive relative to the rest rotation:

```
RotAdditive = DomRestRot.Quaternion().Inverse() x DesiredHandRotCS
```

This keeps the rotation correct under arbitrary yaw changes while the plant holds. Hip lean follow runs proportionally to the dom additive at `HipFollowPercent`, leaning the body into the contact point. SpineLookAt alpha ramps to 1.0 across `PlantDuration`, progressively turning the torso toward the victim.

**Phase 3 - Recover (`ThrustRecover`):**  
Recovery drives the attack montage in **reverse** at `-|MontageRate| x ReverseRateMultiplier`. The frame-one skip guard (checking `bAlreadyRecovering` before reading montage position) prevents a false "reached end" trigger: on the init frame, `Montage_Play` has just been called but the animation system hasn't evaluated yet, so `Montage_GetPosition` still returns the frozen position, which would instantly re-freeze the montage. The guard lets one evaluation pass before checking completion.

Both additive goals lerp to zero independently over `ArmRecoverDuration` and `HipRecoverDuration`. SpineLookAt alpha runs at `1 - ArmAlpha`, so the spine un-rotates in sync with the arms retracting. On completion, all goals are explicitly zero-written and `ThrustEnd` resets `FThrustState` to default, ensuring no stale additive offset survives into the next attack.

### 4.2 Additive IK Goals - Why Zero is Safe

All IK Rig goals in this system are **additive offsets**. A zero additive goal is equivalent to no goal at all - the IK Rig returns the bone to whatever the animation pose dictates. This is the foundational safety property of the system: when no thrust is active (setup, end, or cancel), writing zeros restores the full animation pose cleanly without any bone snapping or rest-pose capture required.

This also means the thrust system is interrupt-safe: if `ThrustEnd` is called mid-plant (hit cancelled, character died), zeroing the goals restores the animation immediately with no artifacts.

### 4.3 Reflection-Based Goal Access

`UThrustSystemNodes` reads and writes IK goal variables on the `AnimInstance` by name using UE's property reflection system. The ABP stores the variable names as `FName` fields (`DomLocGoal = "PlantedDomLoc"`, `SlaveLocGoal = "PlantedSlaveLoc"`, etc.):

```cpp
static void SetAnimVec(UAnimInstance* Inst, FName PropName, const FVector& Val)
{
    if (FProperty* P = Inst->GetClass()->FindPropertyByName(PropName))
        if (FStructProperty* SP = CastField<FStructProperty>(P))
            if (SP->Struct == TBaseStructure<FVector>::Get())
                *SP->ContainerPtrToValuePtr<FVector>(Inst) = Val;
}
```

This decouples the thrust library from any specific ABP subclass. The same `UThrustSystemNodes` code works on any AnimInstance that has correctly-named variables - no cast, no interface, no dependency.

### 4.4 Constraint System

A `MaxDistFromBone` clamp (configurable; default 50 cm in C++, set higher in `BP_PlayerCharacter` - typically around `spine_03`) exits the plant early if the dom hand drifts beyond arm reach. Without this, a long-distance hit would produce a visibly stretched arm. The stab geometry is refined at setup via a line trace on a configurable `StabTraceChannel` from the contact socket to the raw target location: the impact point is offset by `StabDepth` along the trace direction, producing a natural blade-penetration depth rather than hand-to-surface contact.

`BlacklistedHitBones` on `UHitReactionComponent` marks bones that do not count as valid hits for HP tracking. The ThrustSystem reads the same list at hit time: when the weapon contacts a blacklisted bone, `HitImpact` passes `BlacklistedPlantDuration` (default 0.01 s) rather than the normal `PlantDuration`. The plant runs but exits almost immediately, producing a glancing read without a full embed. The recovery path is identical to a normal hit, so no separate pull-back parameters are needed and the attacker's weapon retracts cleanly from any contact point.

### 4.5 Known Behavior: Instability at Large Offsets

When the hit target is far from the attacker's hands, the IK goal coordinates become large enough to grow unstable - the hands tremble rather than holding cleanly. `MaxDistFromBone` prevents this in normal gameplay, but when the constraint is relaxed or a hit registers at unusual range the tremor surfaces.

Whether this reads as a bug or a feature depends on context and taste. At a long lunge or a strained reach, the instability can read as physical weight and effort - more convincing than a perfectly locked hold. The behaviour is intentionally left tunable: `MaxDistFromBone` and the accumulation rate give the person tweaking it full control over where clean holding ends and expressive instability begins.

---

## 5. Feature 3: Procedural Hit Reactions - HitReactionComponent

`UHitReactionComponent` is the core of the "make it hurt" requirement. It receives a hit event and produces a multi-second physical response with no animation assets involved.

### 5.1 Entry Point: HitW_Physics

```cpp
void UHitReactionComponent::HitW_Physics(
    int32 InAttackSide, FName InBoneHit, FVector InHitDir, double InHitStrength)
```

Three decisions happen immediately:

1. **HP decrement** - bones in `BlacklistedHitBones` skip HP decrement but still trigger the full stunt and recovery. The map value (`FBlacklistedBoneSettings`) carries per-bone `SimScale` (physics blend weight multiplier) and `PushForceScale` (push force multiplier), so individual bones can produce a lighter or heavier reaction independently of the HP logic. A global `BlacklistedImpulseScale` further scales the physics impulse for all blacklisted hits.
2. **Bone remapping** - `pelvis` and `None` map to `spine_01` (the fallback). The pelvis is the root of the physics hierarchy; simulating it directly causes the character to drop to the ground on the first frame. The fallback routes the simulation to a bone the physical animation profile can actually control.
3. **Physics bone selection by attack side:**

| Attack Side | Physics Bones |
|-------------|--------------|
| 0 (Top)     | `spine_03`, HitBone |
| 1 (Mid)     | `spine_01`, HitBone |
| 2 (Bot)     | `spine_01`, HitBone |

The two-bone approach - an anchor bone plus the specific hit bone - ensures the simulation region extends from a stable root rather than floating a single disconnected body. Without the anchor, the hit bone's parent chain remains fully animated while only the child simulates, creating a violent discontinuity at the first simulated joint.

### 5.2 Stunt Curve Timing

The stunt duration and simulation curve are attack-side specific:

| Side | Duration Multiplier | Curve |
|------|--------------------|----|
| Top  | `StuntTime x 1.1`  | `FC_Stunt01_Top` |
| Mid  | `StuntTime x 1.0`  | `FC_Stunt01_Mid` |
| Bot  | `StuntTime x 1.4`  | `FC_Stunt01_Bot` |

Bottom hits take 40% longer because leg hits produce wider-amplitude oscillations that need more time to settle before foot repositioning begins. The curves are authored in the `FC_` naming convention (float curves) and sampled via `UCurveSamplerNodes::SampleCurve`, which is a stateless function that advances a `FCurveSamplerState` timer and returns the interpolated value plus a completion flag.

### 5.3 Per-Tick Pipeline (Active Phase)

While the curve is running, each tick executes in order:

1. **`SimulationWeight()`** - drives physics blend weight from 0 -> curve value x MaxWeight. Bottom hits cap at 0.5 to avoid the legs fully detaching from animation; upper hits reach 1.0.
2. **`PushVictim()`** - applies `AddMovementInput` scaled by `SimValue`, pushing the character capsule in `HitDir`. Bottom hits apply zero push (the character is already stumbling). Upper hits push toward a 2D goal computed from `HitDir x 200cm` at event start.
3. **`PelvisMovement()`** - the most visually important function. It computes a pelvis displacement using the formula:

   ```
   alpha     = |SimValue x 2 - 1|          // peaks at 0, zero at 0.5 and 1.0
   displace  = HitDir x PelvisDisruption x (SimValue x 2 - 1)
   lerpedXY  = lerp(displace, StartPelvisPos, alpha)
   pelvisZ   = lerp(clamp(HitStrength x -0.07, -70, 20), StartPelvisPos.Z, alpha)
   ```

   The parabolic alpha means pelvis displacement peaks mid-stunt and returns cleanly to the rest position at completion. Z displacement is proportional to hit strength, so a light tap barely dips while a full-force hit drops the pelvis by up to 70 cm before recovering.

4. **`ReactiveSteps()`** - runs the foot IK solver against the displacing pelvis. Feet that cannot keep up with the pelvis excursion trigger a stride. The solver operates in world space and accounts for mesh rotation changes each frame.

### 5.4 Reactive Foot Repositioning - Post-Stunt Phase

When the stunt curve completes, the component does not immediately clear the stunt. Instead it enters a **repositioning phase**:

1. Physics simulation is stopped (bones return to animation).
2. The foot IK state is re-initialized with the character's current foot positions as goals.
3. `ReactiveSteps()` continues to run, now working the feet back toward their natural positions under the re-animated skeleton.
4. The stunt clears only when `UFootIKNodes::AreFeetRepositioned()` returns true - both feet have settled.

This bridges a problem endemic to physical hit reactions: feet end up planted in arbitrary locations after simulation, and returning directly to full animation without IK blending produces a snap. The repositioning phase provides a smooth procedural recovery. The detailed implementation challenge is covered in Section 13.

### 5.5 Low-Health Continuous Simulation

At `CurrentHP == 1` (one hit remaining), the system activates `bWoundedActive` after repositioning completes. Rather than random impulses, the low-health behaviour is driven by **two additive sine waves** modulating the physics blend weight each frame:

```
Wave1    = WoundedOscAmplitude  x sin(WoundedOscFrequency  x elapsed x 2pi)   // slow: laboured breathing
Wave2    = WoundedOscAmplitude2 x sin(WoundedOscFrequency2 x elapsed x 2pi)   // fast: muscle tremor

EffBlend = clamp(WoundedBlend + Wave1 + Wave2, 0, 1)
```

The two waves run at different frequencies (defaults: 0.35 Hz and 0.8 Hz) so their combined pattern never exactly repeats, producing an organic, non-mechanical feel. The base `WoundedBlend` ramps in over `WoundedTransitionTime` from 0 to `WoundedSimWeight`, so the oscillation starts subtle and reaches full amplitude gradually. Simulation runs on the `WoundedSimBone` chain (default `spine_01`).

`WoundedTickTimeout` places an upper bound on how long the simulation runs. When it fires, the physics blend fades out over `WoundedTimeoutBlendOutDuration` rather than cutting immediately. The system can also be ended early via `StopWoundedSim()` (immediate) or `BlendOutPhysics(Duration)` (smooth fade-out). The attacker calls `BlendOutPhysics` via `DisableNearbyVictimsPhysics()` at attack start, which blends out wounded physics on nearby victims within `NearbyPhysicsDisableRadius` so weapon traces can register cleanly against their mesh without the simulation interfering with collision.

`SimFinish` is aware of the wounded state: if `bWoundedActive` is true, it re-enables mid-bone physics at the current `WoundedBlend` weight after each stunt cycle ends, so a subsequent hit does not permanently disable the tremor.

---

## 6. Feature 4: Targeting System - Lock-On and Visual Feedback

### 6.1 Targeting Component Architecture

Enemies expose multiple `UTargetComponent` instances (`TopTarget`, `MidTarget`, `BotTarget`) as named children of the character Blueprint. `UTargetComponent` extends `UStaticMeshComponent`, which means the component itself can carry a mesh (the visual bracket) and has a built-in transform that follows the skeleton without any additional update logic.

The `UTargetingSystemComponent` on the player performs a sphere sweep on `ECC_GameTraceChannel1` - a dedicated collision channel - which prevents the lock-on from accidentally hitting weapon collision or environmental geometry on the default trace channels.

### 6.2 Target State Changed Delegate

`UTargetComponent` broadcasts an `OnTargetStateChanged(bool bNewState)` delegate. In `BP_Victim`, each of the four target components binds this delegate to a material parameter call:

```
Selected  -> "Color" = (1, 1, 0)   yellow
Deselected -> "Color" = (1, 0, 0)  red
```

`SetVectorParameterValueOnMaterials` propagates to all material instances on the owning mesh - no explicit material reference required. The system scales to any number of target components without modification.

### 6.3 Target Position Relay

Each `TargetComponent` drives `TargetPos` on `APlayableCharacter` through custom events (`GetTopTarget`, `GetMidTarget`, etc.). `TargetPos` feeds two systems simultaneously: the `SpineLookAt` chain in the ABP (rotating the attacker's spine toward the target) and the `ComputeHandHeightIK` function (adjusting hand height based on target elevation). Both systems read the same world-space position, so the attacker's entire upper body orientation - from spine to hands - is derived from a single authoritative target point.

### 6.4 AttackData-Driven IK

`OnAttackStarted` looks up `FAttackMontageData` from a DataTable keyed by montage name, then selects the `FTargetSlotIKData` that matches the locked slot (`TopTarget`, `MidTarget`, `BotTarget`). This table provides per-slot position offsets and rotation limits for the dominant and slave hands, plus a global pelvis influence scalar. The system is entirely data-driven: adding a new attack montage requires only a new DataTable row with per-slot IK authoring, no code changes.

---

## 7. Feature 5: Death and Full Ragdoll Transition

### 7.1 Activation Timing

`ActivateRagdoll()` is triggered at `ThrustRecover` completion - the moment the attacker's sword has fully withdrawn. This timing is deliberate: the death ragdoll begins only after the attacker's hands have physically retracted, so the corpse never snaps out of the attacker's grip.

### 7.2 Transition Sequence

```cpp
// 1. Stop stunt simulation and clear stun flag
Mesh->SetAllBodiesBelowSimulatePhysics(RootSimBone, false, true);

// 2. Disable character movement (prevents capsule from dragging the ragdoll)
CharacterMovement->DisableMovement();

// 3. Apply ragdoll physics profile and constraints
PhysicAnimComp->ApplyPhysicalAnimationProfileBelow(RootSimBone, "Ragdoll", true, true);
Mesh->SetConstraintProfileForAll("Ragdoll", true);

// 4. Enable full-body simulation from pelvis
Mesh->SetAllBodiesBelowSimulatePhysics(PelvisBoneName, true, true);

// 5. Start blend at 0 - ramp to 1 in TickComponent via RagdollTransitionCurve
Mesh->SetAllBodiesBelowPhysicsBlendWeight(PelvisBoneName, 0.f, false, true);
```

The transition curve ramps physics blend weight from 0 to 1 over `RagdollTransitionTime` (0.2 s on `BP_Victim`), driven by `TickComponent`. A custom float curve (`FC_Ragdoll`) allows non-linear easing - a fast initial collapse followed by a smooth settle. The blend is applied from `PelvisBoneName` rather than root to avoid simulating the root bone, which would fight the capsule location.

A backward impulse (`-HitDir.GetSafeNormal() x RagdollImpulse`) at death gives the corpse directional momentum away from the attacker, providing a read on the killing blow without a dedicated death animation.

### 7.3 SetDeathPlantBlend

`SetDeathPlantBlend(float Blend)` is exposed for the attacker-side BP managing thrust recovery. It allows the victim's hit bones to begin blending toward full physics while the attacker's sword is still in contact, so the corpse starts going limp visually before the sword clears rather than waiting for the recovery to complete.

---

## 8. Feature 6: Chaos Cloth - Bandana Simulation

The bandana cloth simulation was tuned with three goals: plausible response to character velocity, no clipping through the neck geometry, and stable jitter-free rest at idle.

### 8.1 Configuration Rationale

| Parameter | Value | Reasoning |
|-----------|-------|-----------|
| Density | 1.2 g/cm^3 | Slightly above water density - appropriate for dense cotton. Heavier than default prevents balloon-like floating. |
| Edge Stiffness | 0.95 | Near-rigid along edges to prevent stretching. Fabric should maintain silhouette width. |
| Bending Stiffness | 1.0 | Full bending resistance - the bandana wraps the neck and should not collapse on itself. |
| Geodesic Tethers | 1.05-1.1 | Slightly above-natural rest length. Without tethers, corners overstretch from gravity; tethers above 1.0 give the cloth a subtle gathered appearance. |
| Gravity Scale | 0.005 | Near-zero gravity. The bandana is knotted against the body - gravity should affect only the tail, not pull the whole piece down. At 1.0, gravity fights the velocity-follow system and produces unrealistic draping. |
| Damping | 0.15 | Moderate damping. Low enough to show motion detail; high enough to kill oscillation within 2-3 frames of coming to rest. |
| Linear Velocity Scale | 0.75 | At 1.0, cloth matches bone velocity instantly (stiff). At 0.0, cloth ignores the character entirely. 0.75 preserves follow-through - the tail lags on quick turns - while keeping the body of the bandana approximately in contact with the neck. |
| Iterations | 6 | Minimum count that eliminates visible constraint violation at attack animation speeds. Below 5, corner tethers visibly pop during fast starts. |
| Subdivisions | 4 | Provides sufficient vertex density for smooth deformation across the bandana's curvature. |

### 8.2 Collision and Pinning

Neck collision is handled via a simple capsule collider on the neck bone rather than per-poly, significantly reducing simulation cost while covering all practical intersection cases. Constraint painting pins the cloth firmly along the knot seam so the bandana cannot detach from the neck regardless of capsule velocity.

---

## 9. Shader Animation - Material-Driven Visual Feedback

This section covers the two materials that communicate the lock-on targeting state to the player. The design principle was to avoid a HUD-like floating UI feel: both materials are diegetic - they exist in world space, respond to the scene's lighting and depth, and read correctly from any camera angle.

### 9.1 M_TargetSelection - The Target Bracket Material

`M_TargetSelection` is a masked material applied to a plane mesh attached to each `UTargetComponent`. The shader generates all visible geometry - the mesh itself contributes only vertices and UV coordinates.

#### Scalability: Works for Any 3D Target

The core motivation for building this as a WPO-driven shader rather than a billboard sprite was **scalability**. The billboard deformation is computed entirely in the material using the mesh's UV coordinates and world-space distances - there is no dependency on the mesh being flat or axis-aligned. This means the same material can be applied to any 3D mesh: a door handle that the player can interact with, a chest to be looted, a character's specific body part. Any geometry becomes a visual target indicator just by assigning this material. No Blueprint code, no special component - just the material.

#### Billboard WPO - Camera-Space Orientation

The core problem with a flat indicator on a 3D object is that it disappears edge-on. The solution is World Position Offset that orients the mesh toward the camera regardless of the mesh's world rotation:

```
Direction = CameraPosition - ObjectWorldPosition
           (normalized, in world space, derived from UV-mapped vertex positions)
WPO = Direction x Distance
```

The direction is derived from UV coordinates remapped to `[-1, 1]` range so each vertex receives a component-proportional offset. This is equivalent to a billboard shader without the complexity of a full quaternion orientation change - it works because only one axis needs to face the camera.

#### Custom Sine Deformation

The bracket shape deforms over time using a remapped sine function rather than UE's built-in `Sine` node:

```
Input   = Time x DeformSpeed + TexCoord.R
Shifted = (sin(Input) + 1) / 2        // remap [-1, 1] -> [0, 1]
Output  = Shifted x Noise x Animation
```

The remap to `[0, 1]` is intentional. A standard sine oscillates through negative values which, multiplied against displacement, would push vertices in the opposite direction - wasted motion that never reads visually. The `[0, 1]` remap makes the deformation strictly outward, so every cycle reads as a clean pulse.

`TexCoord.R` as the phase offset gives different U-positions on the mesh different phases, producing a wave-traveling effect across the bracket. The indicator appears to flow and pulse rather than uniformly throb.

#### Noise Layer - Organic Variation

A `LowResBlurredNoise` texture pans vertically in UV space (`SpeedNoiseV = -3`) and multiplies into the deformation output. Without it, the sine deformation is perfectly periodic and reads as mechanical. The noise introduces organic variance - some pulses are stronger, some barely register. Vertical-only panning reinforces the impression of energy flowing upward through the bracket.

#### Animation Parameter

The `Animation` scalar (default 0) is the master gate: at 0, all deformation evaluates to zero and the mesh is invisible. Blueprint sets this to 1 on selection, potentially driven by a timeline for a smooth appear. This single float keeps external control minimal - one property drives the entire visual state.

#### DitherTemporalAA Opacity Mask

The bracket silhouette is defined by vertex color alpha on the mesh, threshold-compared and fed into `DitherTemporalAA`. Dithered temporal transparency gives the bracket a physically thin, gossamer quality - fully opaque masks look like UI overlays; dithered masks feel embedded in the world. The temporal accumulation resolves the dither pattern between frames, preserving full edge detail at rest.

The `Color` parameter (yellow/red from Blueprint) tints the output after the mask is applied, so the color is physically correct at the masked boundary with no color bleed into the transparent region.

### 9.2 PP_SelectTarget - Scene Post-Process Material

`PP_SelectTarget` is a post-process material that desaturates and darkens the entire scene except for the locked-on target. Its purpose is simple and focused: **make it visually unambiguous what matters in this moment**. The attacker needs to read the target clearly under any lighting conditions, at any distance, against any background. The surrounding scene becomes supporting context rather than visual noise.

#### Desaturation and Darkening Pass

```
Luminance   = (R + G + B) / DarknessIntensity    // simultaneous desaturate + darken
```

Dividing by `DarknessIntensity` (default 10) both desaturates (pushing RGB toward equal luminance) and reduces absolute brightness. This is more effective than desaturation alone: real perceptual depth perception drops in darkness, so the dimmed background recedes and the full-color target advances without any geometric change.

The `BnW` parameter (0-1) controls the lerp weight of the effect, allowing it to ease in as lock-on activates rather than appearing as a hard cut.

#### Stencil Mask - Restoring Target Color

`BP_Victim` sets `CustomDepth = true` with Stencil ID = 1. The post-process material reads the stencil buffer and uses it as the lerp alpha:

```
Output = lerp(FullSceneColor, DarkDesaturated, StencilMask x BnW)
```

Pixels where stencil = 1 (the target mesh) return unmodified scene color. All other pixels receive the dark desaturated version. The stencil read is a single texture sample per pixel - essentially free on modern hardware - making this approach far cheaper than screen-space outline techniques (Sobel filters, jump flood) that were evaluated and discarded.

---

## 10. AnimGraph Architecture - ABP_Base Pipeline

`ABP_Base` is a thread-safe AnimGraph. The update function runs on the worker thread; state variables are either `BlueprintReadOnly` (written only by `NativeUpdateAnimation`) or written by C++ components between frames via the game thread - safe because the AnimGraph evaluation reads after the game thread write completes.

### 10.1 Pose Pipeline

```
[LinkedLayer ALI_Character::FullBody]
  \- SaveCachedPose "LocomotionCache"
       \- LayeredBoneBlend (spine_01, depth 2)
            base:    LocomotionCache
            blend 0: Slot "UpperBody" on LocomotionCache
  \- SaveCachedPose "UBMontageCache"
       \- LocalToComponentSpace
            +- ModifyBone foot_r  (additive, world-space Rotation = RightFootRot)
            +- ModifyBone foot_l  (additive, LeftFootRot)
            +- LookAt head        (LookAtLocation = CamForward, 20 deg clamp)
            +- ModifyBone pelvis  (additive, SpineLookAtAdditiveRot x SpineLookAtAlpha)
            +- ModifyBone spine_01, spine_02, spine_03  (same SpineLookAt vars)
       \- ComponentToLocalSpace
  \- SaveCachedPose "LocalCache"
       \- IKRig (IK_Kwang_Fighter)
            goals: pelvis, foot_r, foot_l - additive position offsets
            goals: PlantedDomHand, PlantedSlaveHand - additive position + rotation
                   final position = PlantedSlaveLoc + SlaveHandAdditivePos
                   final rotation = ComposeRotators(PlantedSlaveRot, SlaveHandAdditiveRot)
  \- SaveCachedPose "Output"
       \- Root
```

### 10.2 ModifyBone Before IKRig - Ordering Rationale

All `ModifyBone` nodes run before the IKRig. This ordering is load-bearing:

- Foot rotation modifies apply in component space before the IKRig solves foot positions. If the IKRig ran first, its foot position solve would place the foot bone, then the additive rotation would be applied on top - but because the rotation is additive to the bind pose rather than the IK-solved pose, the rotation would be in the wrong frame of reference.
- SpineLookAt biases the spine chain toward the target before the IKRig runs, so the hand IK starts from an already-oriented torso. This reduces the required IK offset and keeps arm pose closer to the animation's intended shape.
- The `UpperBody` montage slot runs before IK modifications, so the IK system always reads the montage's bone positions and continues to solve from them correctly regardless of the active attack animation.

### 10.3 IK Goal Composition

The hand IK goals compose two additive layers:
- **Thrust system additive** (`PlantedDomLoc`, `PlantedSlaveLoc`): the socket-feedback convergence from weapon contact
- **Hand height IK additive** (`DomHandAdditivePos`, `SlaveHandAdditivePos`): the per-slot height and rotation offset from the `AttackData` table

Summing both additives at the ABP level means the two systems are independent and can both be active simultaneously. A thrust contact and a height correction compound correctly without either system knowing about the other.

### 10.4 Camera Turn -> Playrate

Camera yaw input (the mouse X axis) is multiplied by a designer-tunable `TurningSpeedMultiplier` (default `1`) and fed to `TurnValuesUpdate`, which interpolates `Turning_Speed` toward the input magnitude at speed 10. This value propagates to the ABP via `SyncPlayableCharacterData` and drives the playrate of the turn animation in the Blueprint children:

```cpp
TurnValuesUpdate(Axes.X * TurningSpeedMultiplier);
Turning_Speed = FInterpTo(Turning_Speed, |Axis|, DeltaTime, 10.f);
```

The intention is that a fast camera spin plays the turn animation faster, keeping the foot plant visually correlated with how aggressively the player is turning. Arriving at the right default required going back to Maya and readjusting the animation: the original turn animation had a foot plant cadence tuned for a fixed playrate, so at higher input speeds the feet were visually sliding. Reworking the in/out timing of the plant frames in Maya brought foot contact into alignment with the actual camera angular velocity. The `TurningSpeedMultiplier` being a single editable property means a designer can recalibrate for a different animation or sensitivity setting without touching code.

### 10.5 Virtual Bones as Animation-Stable References

Three virtual bones are added to the `SKM_Kwang` skeleton in the Skeleton editor: `VB pelvis`, `VB foot_l`, and `VB foot_r`. They are not part of the mesh and have no influence on skinning. Their sole purpose is to give `UHitReactionComponent` a stable, animation-driven world position for the pelvis and feet at any point during a hit reaction.

The reason this is necessary: when Chaos physical animation is active, `GetBoneLocation` on a simulated bone returns its physics-world position, which jumps and oscillates with the simulation. The reactive foot repositioning and pelvis delta tracking in `ReactiveSteps` need the animated pose position, not the simulated one. Because virtual bones are computed from the animation graph and are not part of the physics simulation, they always return the animation-driven position regardless of what the real bones are doing in the physics world.

```cpp
// Pelvis world position from animation, unaffected by physics simulation.
const FVector PelvisNow = Mesh->GetBoneLocation(VirtualPelvisBone);  // "VB pelvis"

// Foot IK state setup reads VB foot_l / VB foot_r for the same reason.
SetupFootIK(LFootState, Mesh, VirtualLeftFootBone, VirtualPelvisBone, ...);
```

The bone names are configurable via `VirtualPelvisBone`, `VirtualLeftFootBone`, and `VirtualRightFootBone` on `UHitReactionComponent`. Adding a virtual bone in the Skeleton editor and updating these three properties is all that is needed to retarget this system to a different skeleton.

---

## 11. IK Nodes Pattern - Stateless Libraries

Every IK system in this project follows the same architecture principle: **stateless `UBlueprintFunctionLibrary` functions receive a `UPARAM(ref)` state struct and return results; the caller owns the struct**.

```
ABP / C++ component
  \- owns: FFootIKState, FThrustState, FCurveSamplerState

UFootIKNodes::SetupFootIK(ref State, ...)     <- one-time init
UFootIKNodes::SolveFootIK(ref L, ref R, ...)  <- per-tick solve

UThrustSystemNodes::ThrustSetUp(ref State, ...)
UThrustSystemNodes::ThrustTick(ref State, ...)
UThrustSystemNodes::ThrustPlant(ref State, ...)
UThrustSystemNodes::ThrustRecover(ref State, ...)
UThrustSystemNodes::ThrustEnd(ref State, ...)
```

This pattern has concrete benefits:

1. **Thread safety** - the ABP can call these functions on the animation worker thread. No locks required because the function has no static state.
2. **Multiple instances** - `HitReactionComponent` runs two `FFootIKState` structs (L and R) simultaneously. A class-based approach would require two component instances or a footIK manager object.
3. **Testability** - any test harness can construct a struct and call the function without instantiating an Actor or AnimInstance.
4. **Blueprint accessibility** - because the functions are `BlueprintCallable`, a Blueprint AnimGraph can own and drive a state struct without any C++ boilerplate on the ABP class.

---

## 12. Trade-offs and Design Decisions

### Physics-Driven vs. Animation-Driven Reactions

**Decision:** Zero hit-reaction animations. All victim response is Chaos Physical Animation blended against the animation pose.

**Trade-off:** Physical reactions are harder to read at a glance than authored animations - a stagger animation has precise timing and directionality that a physics blend cannot guarantee. The compensation is the curve-sampled weight ramp, the pelvis displacement, and the foot repositioning system, all of which impose consistent timing and directionality on top of the physics.

**Why it was the right call:** The test asked for systems, not polish. An animation-driven hit-reaction library would produce consistent results without original engineering. Physics-driven reactions demonstrate understanding of simulation systems, IK integration, and runtime blend management that animation state machines cannot.

### Additive IK Goals - Safety Over Precision

**Decision:** All IK Rig goals are additive offsets. A zero additive goal restores the animation pose silently.

**Trade-off:** Additive goals are relative to the current animation pose, which means the "rest" reference shifts as the animation changes. During a montage, the rest-pose position of a hand bone changes frame by frame. The thrust system compensates for this via the socket-feedback loop, which re-derives the error to the target each frame rather than freezing a rest-pose reference.

**Why additive:** The safety property outweighs the complexity. Zero additive = no effect means cancels and interrupts are free - no bookkeeping, no rest-pose capture, no risk of an unwritten goal snapping a bone to mesh origin. The system is also composable: multiple additive layers can be summed at the ABP without any one layer needing to know the others exist.

### Camera Turn-Playrate Link vs. Separate Trigger

**Decision:** Link the turn animation playrate directly to the camera yaw axis, rather than triggering the turn animation at a rotation-speed threshold.

**Trade-off:** A threshold system (play turn animation if angular velocity > X) is simpler to author and does not require Maya iteration. The direct playrate link requires that the animation's foot-plant timing aligns with the playrate range the input can produce, which meant readjusting the animation in Maya and calibrating a single `TurningSpeedMultiplier` property.

**Why the playrate link:** A threshold system produces binary behavior - either the turn animation plays or it does not. At typical lock-on combat speeds, the camera barely crosses any reasonable threshold, so the character looks stiff. The playrate link produces a continuous, proportional response: a slight camera drift produces a slow lean, an aggressive camera spin produces an aggressive turn. Exposing the scale as a single editable property means any future calibration is a one-line designer tweak, not a code change.

---

## 13. Hardest Technical Problems

### 13.1 ThrustPlant - World-Anchored Hand Under Arbitrary Motion

The hardest single function in the project is `ThrustPlant`. The requirement sounds simple: keep the attacker's hand at the point of contact while the sword is embedded in the victim. The difficulty is that both characters can move and rotate simultaneously, and the IK system operates in additive component space - there is no "just freeze this bone at a world position" primitive in UE5's IK Rig.

**Problem 1: Rotation stability under yaw changes.**  
The plant rotation must remain fixed relative to the world (the sword stays aimed at the impact point) even as the character's yaw changes. If the rotation goal is stored in component space and the character yaws 90 deg, the stored CS rotation is now wrong - the hand would rotate with the character rather than holding its world orientation.

The solution: store `PlantedDomRotWorld` (the hand's desired world rotation) on the first plant frame, and re-derive the CS additive rotation every frame:

```
DesiredHandRotCS = CompQ.Inverse() x PlantedDomRotWorld.Quaternion()
RotAdditive      = DomRestRot.Quaternion().Inverse() x DesiredHandRotCS
```

The `CompQ.Inverse()` step converts the world-stable rotation into the current frame's component space. The second step converts it from absolute CS to additive (relative to rest rotation). Skipping either step produces rotation drift within one or two frames.

**Problem 2: Socket-to-hand offset under rotation.**  
The contact socket is not the same location as the hand bone - it sits at the blade's midpoint. The IK goal must position the hand such that the socket lands on the target, not the hand itself. At setup, `SocketRelativeLocation` stores the socket position in the hand bone's local space. At plant-frame-0, this offset is projected into world space using `PlantedDomRotWorld`:

```
PlantedHandWorldPos = -(PlantedDomRotWorld.Quaternion().RotateVector(SocketRelativeLocation))
```

The negation converts socket-relative-to-hand into a hand-position adjustment. If the rotation changes, this projection would be stale - but because `RotAdditive` is re-derived each frame from `PlantedDomRotWorld`, the socket lands correctly under arbitrary subsequent rotation.

**Problem 3: One-frame IK feedback convergence.**  
The IK solver does not evaluate instantaneously - on frame N, the solver reads the goal written in frame N and produces a bone position that is visible in frame N+1. If the plant goal is computed from the theoretical hand position rather than the actual solved position, the socket accumulates a small error each frame that grows visibly over the plant duration.

The solution is the socket-feedback accumulator:

```
SocketWorldCurrent = AttackerMesh->GetSocketLocation(ContactSocket)
SocketErrorCS      = CompTW.InverseTransformVector(CurrentTargetWorld - SocketWorldCurrent)
DomAdditive        = GetAnimVec(AnimInst, DomLocGoal) + SocketErrorCS
```

Reading `SocketWorldCurrent` from the mesh reads the position produced by the *previous frame's* IK solve. Measuring the error from there to the target and adding it to the running additive goal closes the loop in exactly one frame regardless of IK solver passes or convergence tolerance. No iterative solver is needed - the feedback naturally drives the error to zero.

**Problem 4: ThrustRecover init-frame false completion.**  
On the frame `ThrustRecover` is first called, `Montage_Play` is called with the reverse rate and start position. The animation system has not yet evaluated this change - `Montage_GetPosition` still returns the previously frozen value (often 0). A naive check of "did the montage reach 0?" would immediately trigger "recovery complete" on the very frame recovery started.

The fix: defer the completion check by one frame using `bAlreadyRecovering`:

```cpp
const bool bAlreadyRecovering = State.bRecovering;
// ... init block runs (sets State.bRecovering = true) ...
if (bAlreadyRecovering)     // skip completion check on init frame
{
    if (CurPos <= KINDA_SMALL_NUMBER)   // only checked from frame 2 onward
        bDone = true;
}
```

This pattern ensures the animation system has at least one evaluation cycle before the completion state is polled.

### 13.2 Reactive Foot Repositioning - Anti-Slide in Component Space

During the stunt phase, the pelvis is being displaced by `PelvisMovement()`. The foot IK holds the feet at their anchor positions. The challenge: "hold the feet still" in world space is not the same as "hold the IK goal constant in component space." When the pelvis moves in component space, the IK Rig uses that as a reference for foot reach. A constant CS goal effectively slides the foot in world space.

The solution is the `AlignFoot` lambda in `ReactiveSteps`:

```cpp
const FVector DeltaCS  = MeshTW.InverseTransformVector(PelvisNow - FrozenPelvisW);
State.AnchorGoal       = FrozenIK - DeltaCS;
```

This computes the pelvis displacement in component space and subtracts it from the frozen foot IK position. The foot goal is now always "where the foot was when it last landed, adjusted for however the pelvis has moved since." The result is a world-stationary foot through arbitrary pelvis displacement without any additional IK node.

The key detail is `InverseTransformVector` (not `InverseTransformPosition`). The delta is a direction, not a location - it must not be offset by the mesh's world position. Using `InverseTransformPosition` would add the mesh's world location to the delta on every frame, producing foot positions that diverge from world origin at any non-zero actor location. This specific type mismatch was the root cause of two failed iterations before it was identified.

The stride-landing update closes the feedback loop:

```cpp
if (bWasLStriding && !LFootState.bStriding)
{
    FrozenLeftFootIK   = LFootState.AnchorGoal;   // new frozen reference
    FrozenPelvisWorldL = PelvisNow;               // new tracking baseline
}
```

Without this, the anti-slide reference becomes stale after the first stride - the foot has moved to a new anchor, but the system is still tracking pelvis delta relative to the old one. Resetting `FrozenPelvisWorldL` at stride landing keeps the system stable through multiple reactive strides during a single stunt cycle.

---

## 14. Known Issues and Limitations

### Character Skinning

The character mesh skinning has areas that need polish. The shoulders in particular show deformation artifacts at certain poses and extreme joint angles. This is a limitation of the source asset; no better freely available alternative was found for this project's scope.

### Foot IK - No Edge Placement

The foot IK system traces straight down from each foot bone to find the ground surface. There is no look-ahead or edge detection. Near cliff edges or ledge lips, the trace may find no geometry directly below the foot even though the character is visually standing on the edge - the foot IK goal snaps to zero and the foot floats in the air. This is a known limitation of the downward-trace approach; a production implementation would add a forward-offset trace and a secondary landing-point search to handle edge cases.

### ThrustSystem Math Precision
The socket-feedback accumulator in `ThrustPlant` converges correctly in most gameplay situations but is not a formally precise IK solver. The rotation derivation (`CompQ.Inverse() x PlantedDomRotWorld`) is accurate for pure yaw changes but can accumulate small errors under combined pitch-yaw-roll motion (e.g., the character falling while planted). For this project's scope and camera constraints the error is not perceptible, but a production implementation would benefit from replacing the open-loop accumulator with a proper constraint solver or a closed-form CS goal derived entirely from the world anchor each frame.

### Slave Hand Alignment in Extreme Positions
The slave hand copies the dominant hand's additive goal with a fixed grip offset. In most attack positions this produces a convincing two-handed grip, but at extreme angles - high overhead strikes or very low stabs - the slave hand's IK chain cannot fully rotate into alignment with the dominant hand's orientation, leaving the off-hand visibly misaligned. A production solution would author per-slot rotation limits and a dedicated slave-hand rotation goal rather than inheriting the dominant hand's rotation directly.

### Two-Handed System Only
The ThrustSystem was designed and tuned for a two-handed weapon. It drives a dominant hand and a slave hand as a rigid pair with a fixed grip offset. A one-handed weapon requires a different goal structure: only the dominant hand IK goal is relevant, the slave hand should return to a rest or guard pose, and the hip follow percent needs a separate tuning pass. Adapting the system for one-handed weapons would require exposing a hand-count toggle and a separate slave-at-rest recovery path - straightforward to add but not yet implemented.

### Attack Trace Architecture - Why Line Traces from Blade Start to Tip
The attack detection in `ANS_Attack` fires line traces from the blade's start socket to its tip socket on each call rather than using the common "sweep from last frame's position to current position" technique. The fundamental issue with the swept-position approach is architectural: `Received_NotifyTick` is not a true tick. It is a function called once per animation graph evaluation frame with no persistent state between invocations - no delta time accumulation, no previous frame transform stored anywhere. There is no built-in concept of "where the bone was last frame" available within a Notify State. Storing the previous position manually was attempted, but because the function has no guaranteed execution order relative to physics or movement, the stored position could be stale, displaced, or from a different evaluation context, producing traces across unrelated geometry and phantom hits. The socket-to-socket line trace is entirely self-contained within a single call: start socket to tip socket, same frame, no cross-frame dependency. This matches the actual execution model of the Notify and produces consistent, predictable results regardless of frame rate or evaluation timing.

### Ragdoll Testing Coverage
The ragdoll transition was tested and tuned for the scenarios present in this project (single attacker, static environment, known skeleton). It has not been stress-tested for edge cases such as multiple simultaneous hits at death, death while the stunt physics are still active on a different bone group, or death at the boundary of a physics ragdoll that was already partially simulating from a previous hit. The transition logic handles these cases defensively (all simulation is stopped before ragdoll begins), but the visual quality in those edge cases has not been fully evaluated.

---

## 15. Future Improvements

### One-Handed Weapon Support
The most immediately useful extension. The ThrustSystem needs a `bTwoHanded` toggle that, when false, skips the slave hand goal entirely and blends the slave hand back to a guard or idle pose during the plant phase. Per-montage or per-slot override values in the `AttackData` table would let designers author different guard poses for different attack types without touching code.

### ThrustSystem Solver Precision
Replace the open-loop socket-feedback accumulator with a closed-form world-anchor projection for the position goal - computing `CompTW.InverseTransformPosition(PlantedHandWorldPos)` each frame as the absolute additive target rather than accumulating the error incrementally. This would eliminate the precision drift under combined rotation and make the math easier to reason about in edge cases. The current accumulator works well in practice but is harder to debug because the goal value at any frame depends on every previous frame's value.

### Interrupt and Multi-Hit Handling
Currently a second hit arriving mid-stunt restarts the stunt from the beginning cleanly, but the ThrustSystem on the attacker side does not handle a new hit while a plant is still active. Adding an explicit interrupt path in `HitImpact` - cleanly ending the current thrust before starting the new one - would prevent the rare case where two overlapping thrust states fight over the same ABP goal variables.

### VFX and Audio Hookup Points
The existing architecture has natural attachment points for effects that are not yet wired up. `HitW_Physics` fires at the exact moment of impact with bone and direction information - ideal for a blood/spark particle dispatch. `SimFinish` fires when the stunt ends - ideal for a recovery sound or ground dust effect. `ActivateRagdoll` fires at death. None of these require changes to the existing systems; they only need event dispatchers or delegates added to the component interfaces.

### Replication Considerations
The entire hit system runs client-side (physical animation, IK goals, capsule push). For a networked game, the hit event would need to be authoritative on the server and replicated to all clients; each client would then run its own physical simulation from that event. Because the simulation is deterministic enough (same curve, same bones, same impulse) the results converge across clients without needing to replicate the frame-by-frame physics state. The component structure already isolates hit state per-actor, which is the right foundation for a replicated implementation.

### Curve-Driven SpineLookAt Influence
Currently `SpineLookInfluence` is a fixed per-axis scalar. Making it a `UCurveFloat` driven by `TargetAlpha` (or attack phase) would allow the spine to respond more aggressively during the plant phase and ease out during recovery - giving the body language an arc that currently has to be approximated with fixed values.

---

## 16. Content Browser Map

```
Game/
+- Levels/
|    \- LV_FightArena          persistent level
|         \- Demonstration     sub-level (streamed): arena at origin + far-coord test area
|
+- Logic/
|    \- Blueprints/
|         +- BP_PlayerCharacter attacker pawn (input, camera, HitImpactComponent, TargetingSystem)
|         +- BP_Victim          victim character (HitReactionComponent, TargetComponents)
|         \- AC_HitReaction     (legacy Blueprint prototype - superseded by C++ component)
|
+- TechAnim/
|    +- Character/
|    |    +- SKM_Kwang          skeletal mesh
|    |    +- PA_Kwang           physics asset (hand-authored)
|    |    +- ABP_Base           base Animation Blueprint (C++ class: UAnimInstanceBase)
|    |    +- ABP_Fighter        ABP child - player locomotion + IK Rig layers
|    |    \- ABP_Victim         ABP child - victim locomotion + IK Rig layers
|    |
|    +- IK/
|    |    \- IK_Kwang_Fighter   IK Rig asset (additive goals: feet, pelvis, hands)
|    |
|    +- Notifies/
|    |    \- ANS_Attack         Anim Notify State (3-pass trace, hit dispatch)
|    |
|    \- Curves/
|         +- FC_Stunt01_Top     physics blend weight curve - upper hit
|         +- FC_Stunt01_Mid     physics blend weight curve - mid hit
|         +- FC_Stunt01_Bot     physics blend weight curve - lower hit
|         \- FC_Ragdoll         ragdoll blend-in easing curve
|
\- TechArt/
     \- Materials/
          +- M_TargetSelection  target bracket (billboard WPO, sine deform, dither mask)
          \- PP_SelectTarget    post-process (stencil desaturation, BnW param)
```

---

## 17. Feature Checklist

| Brief Requirement | Implementation | Location |
|---|---|---|
| **Targeted hits** - different reaction per body zone | `ANS_Attack::TraceForTarget` -> `ETargetSlot` -> `AttackSide` -> per-zone physics bones and stunt curve | `ANS_Attack`, `UHitReactionComponent::HitW_Physics` |
| **Weapon contact** - attacker IK stays on victim | `UThrustSystemNodes` three-phase state machine (approach -> plant -> recover) with socket-feedback convergence | `UHitImpactComponent`, `ThrustSystemNodes.cpp` |
| **Procedural reactions** - no canned hit animations | Chaos physical animation blend driven by `UCurveSamplerNodes`, pelvis displacement, reactive foot repositioning | `UHitReactionComponent`, `FootIKNodes.cpp` |
| **Cloth simulation** | Chaos Cloth on bandana with hand-tuned density, stiffness, tethers, velocity scale | `SKM_Kwang` cloth asset |
| **Death / ragdoll** | `ActivateRagdoll()` - full-body Chaos ragdoll with curve-driven blend-in and directional impulse | `UHitReactionComponent::ActivateRagdoll` |
| **Bonus: Lock-on targeting system** | Sphere-sweep on dedicated channel, `UTargetComponent` slots, material color delegate | `UTargetingSystemComponent`, `BP_Victim` |
| **Bonus: Target visual feedback** | Billboard WPO bracket (`M_TargetSelection`) + scene desaturation post-process (`PP_SelectTarget`) | `Game/TechArt/Materials/` |
| **Bonus: Low-health idle simulation** | Continuous spine physics blend driven by dual sine wave oscillation at `CurrentHP == 1` | `UHitReactionComponent::WoundedTick` |
| **Bonus: AttackData-driven hand IK** | Per-slot hand height and rotation offsets from DataTable, per-axis rotation limits | `UAnimInstanceBase::ComputeHandHeightIK`, `AttackData.h` |
| **Bonus: Camera turn -> playrate** | Mouse yaw axis -> `TurningSpeed` (editable scale `TurningSpeedMultiplier`, default 1.0) -> ABP turn animation playrate | `APlayableCharacter::TurnValuesUpdate` |

---

## 18. Use of AI

AI assistance was used in two specific areas of this project:

1. **Blueprint-to-C++ translation.** Several systems were first prototyped as Blueprint logic (`AC_HitReaction`, `BP_PlayerCharacter` event graphs, `ANS_Attack`). Once the behaviour was validated, AI was used to help translate the Blueprint graph structure into equivalent C++ code, which was then reviewed, adjusted, and integrated manually.

2. **Mathematical calculations.** For certain vector and quaternion operations - particularly in the ThrustSystem's world-to-component-space rotation derivations and the reactive foot anti-slide delta computation - AI was consulted to verify or suggest the correct formula. All formulas were understood, tested, and debugged by hand before being committed to the codebase.

All architecture decisions, system design, tuning, physics asset authoring, animation tweaks, and material authoring are original.
