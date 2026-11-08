/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Lady Inerva Darkvein (DungeonEncounter 2406). No phases and no berserk. Evidence: the 2026-11-06 capture (Normal, solo, 595 s),
 * Warcraft Logs, DBM and BigWigs (trinitycore-tracking/castle-nathria/bosses/lady-inerva-darkvein.md, .json and _followup.json;
 * the follow-up overrides the first spec).
 *
 * - Containers (Desire, Bottled Anima, Sins and Suffering, Concentrated Anima; 168138 / 168366 / 168367 / 168368): the script keeps
 *   the four powers and moves them all on one 1.0 s tick (capture, every delta classified): +3 with the valve closed, +16 (Desire) or
 *   +14 (the others) while focused, -31 net with the valve open, floor 0, cap 1800. The focus is Desire for ticks 1-103, Bottled
 *   104-203, Sins 204-300 and Concentrated from 301 on. Rank 1 below 600, rank 2 from 600, rank 3 from 1200. The powers are mirrored
 *   to the container units and to world states 19162-19165 (1:1), the open-valve count to 19161; 19157 and 19160 are 1 during the
 *   fight. Lady carries one rank aura per container (326256 ... 325923, plain dummy auras) and the rank state effect is an
 *   areatrigger of create properties 9940022-9940025 that the script spawns on every rank change (the static ones are retired by
 *   boss_lady_inerva_darkvein_PENDING.sql).
 * - Focus Anima: a 3.0 s cast on the 0 / 100 / 200 / 300 s clock that interrupts a cast in progress; the raid emote follows 0.2 s
 *   after the start; 0.4 s after the cast ends the new focus valve is closed (Shutoff Valve) and locked (Disable Container) and
 *   the other valves are unlocked (Enable Container). The script sets those states itself.
 * - Valves (goobers 352740 / 352739 / 352738 / 352737): the client casts 325117 (open) or 339100 (close). The core cannot carry
 *   them (see Core gaps), so the spell scripts swallow the Open / OpenLock effects and the boss sets the game object state: a valve
 *   stays open until it is clicked again or its container is empty, an empty valve closes 0.7 s later. At the engage the valves go
 *   to READY with their dynamic flags cleared, at the end and on a wipe back to the sniffed pre-pull state.
 * - Lady's energy: only Expose Desires costs it (100). It regenerates 12.5 / 10 / 8.33 per second (Desire / Bottled / Sins or
 *   Concentrated focused) from the 4th second; the cast is taken at the next free slot of a 1.22 s grid (capture: all gaps between
 *   Lady cast starts are 0.22 + k x 1.22 s).
 * - Loose Anima 325184: on every player every 10.0 s from 1.1 s; the tick is base x (1 + 1.75 x open valves).
 * - Expose Desires 341621 / 341623 / 341625 (rank): 1.0 s cast on the victim; the hit applies Warped Desires 325382. Ranks 2 and 3
 *   also apply Shared Cognition 325936 to random raiders (Normal 1-2, Heroic 2-3, Mythic 4, victim excluded) and give every holder
 *   the copy DoT 325908 when the victim is hit. Rank 3: when Warped Desires runs out Change of Heart 340452 follows (4 s), then 325384.
 * - Bottled Anima 342280 / 342281 / 342282: 1.5 s cast, an areatrigger 24398 at the impact point, and 5.01 s later 325769 on the
 *   players inside 4 yd or, when nobody is inside, Unleashed Volatility 329618 on the raid. Rank 2 leaves a Lingering Anima pool
 *   (24030, 600 s, 325713 while inside); rank 3 drops a second bottle at the first impact (Replicated Anima).
 * - Sins and Suffering 342287 / 342288 / 342290: 2.0 s cast, 3 Sins of the Past 166766 at the end, rank 2 and 3 add 3 orbs 170544
 *   and the Anima Web (24082) 3.5 s later; the orbs roam, damage the nearest player every 3 s and go after 61 s. 4.1 s after the
 *   cast ends Lady carries Shared Suffering 325005; three players get 355553, 3 s later the link 324983. The link ends when the
 *   beams between the linked players cover every orb. At the next Sins start, if Lady still carries 325005, Indemnification
 *   331527 hits the raid and the orbs are cleaned up (345134).
 * - Concentrated Anima 342320 / 342321 / 342322: 2.0 s cast, 0.65 s later the 10 s debuff 340477 on one player (rank 2 and 3 root
 *   it); at expiry (cast start + 12.66 s) Unleashed Shadow 332668 and a Harnessed Specter 170199 at the victim; rank 3 sends 9
 *   Fragments of Shadow (24028, 3 yd/s for 100 yd) from the same point. Heroic and Mythic put 332664 on extra players, each one
 *   leaves a Conjured Manifestation 170197.
 * - Container Breach 325225: 1.0 s tick on the raid while any container is at 1800; the Concentrated container is locked from 300 s
 *   and so reaches 1800 by about 430 s at the latest. This is the only enrage-like mechanic: no berserk exists in any source.
 * - Adds: Harnessed Specter (stationary, Condemn 334017 every 2.45 s from 7.6 s while nobody is in melee, Unconscionable Guilt
 *   331573 on the engaged tank from 9.1 s every 9.5 s); Conjured Manifestation (Condemn 331550, 6 s cast, back to back).
 *
 * Core Support Gate (verdicts, SpellEffects.cpp / SpellAuraEffects.cpp):
 *   effects 2, 3, 6, 64, 77, 140 handled; 28 handled (the script summons with SummonCreature instead); 32 handled, not used;
 *   179 handled at the launch step and needs a destination (the script creates the areatriggers itself); 86 / 33 handled, but see
 *   Core gaps; auras 3, 4, 23, 26, 226, 255, 260, 285 and 395 have handlers (395 needs create properties 21630, absent: effect 2 of
 *   324983 is prevented); aura 61 is MOD_SCALE.
 *   Core gaps (nothing faked): (1) a goober with autoClose 0 resets to READY on the next update, and its OpenLock effect calls Use
 *   again, so valves cannot stay open natively; (2) the lock key lookup (GameObject::GetSpellForLock) returns only the first key
 *   spell (325117), so a client cast of 339100 is refused unless it has SPELL_ATTR8_SKIP_IS_KNOWN_CHECK (it has not): the script
 *   treats 325117 on an open valve as a close; (3) nothing in the core enforces GO_FLAG_LOCKED on a spell click, the script's
 *   CheckCast does; (4) spell target type 145 (the container beams and the Focus Anima destination) is not implemented, so the
 *   Focus Anima cast is a bare 3 s cast and the beam casts 331911 ... 332676 are not cast; (5) AreaTrigger::SetShape does not
 *   refresh the bounds radius or the vertex cache of a polygon, so the Anima Web shape is client visuals only and the contact test
 *   is made by the script; (6) AT create properties 21630 (the link beam) and 19997 (Loose Anima) are absent.
 *   Vehicle 5050 shows Energy (PowerDisplay 458); the script sets energy as the container power type with a 1800 maximum.
 *
 * Confidence: High: the container model, ranks, focus clock, valve behaviour, Loose Anima, Container Breach, Concentrated timing,
 * Bottled impact, Sins chain delays. Medium: Expose cadence (energy model), the first casts. Low (estimates, "Not settled" below).
 *
 * Not settled: the cooldown law of Bottled / Sins / Concentrated before the 50.1 s late loop (the script uses per-window values taken
 * from the observed gaps: Bottled 40 s, 20 s while focused; Sins 50 s, 64 s in the Bottled window, 35 s while focused; Concentrated
 * 70 s; 50.0 s for all three while Concentrated is focused; one non-Expose start per 6.5 s) and whether the longer values are queue
 * delays; the Expose cadence of fast group kills (12-13 s, the energy rates are the solo ones); the bottle impact point (a random
 * player plus 1.6-11 yd; the replica a random point in the room); Shared Cognition counts and tank exclusion; Change of Heart (inferred
 * from spell text only, never seen); the beam of Shared Suffering (the script tests the three segments between the linked players with
 * a 2 yd half width, three players or none) and the Anima Web contact (an edge test with 1 yd width every 0.25 s; rank 3 rotation not
 * visible in the capture); which yells fire (Expose lines once per rank, the others on each cast, the cooldown is unknown); the
 * orbs' roaming range (kept within 40 yd of the web anchor); Heroic and Mythic extra Concentrated targets (1 / 2); Conjured
 * Manifestation (faction, movement, health and cadence were never captured: it takes the boss's faction and stands still); the
 * Stalker 172661 at 72.9 s and 570.6 s (purpose unknown, spawned at the captured times); Zero-Sum Game, Hidden Desire and the fourth
 * orb (Mythic); only the newest Anima Web is tracked (an older web ends early when a newer one spawns, overlapping Lingering pools
 * share one aura); the Indemnification cleanup after a solved link; "Sin consumes your heart!" (trigger unknown, not scripted).
 */

#include "AreaTrigger.h"
#include "AreaTriggerAI.h"
#include "Containers.h"
#include "G3D/Vector3.h"
#include "GameObject.h"
#include "GameObjectAI.h"
#include "InstanceScript.h"
#include "Map.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "Random.h"
#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "SpellAuraEffects.h"
#include "SpellAuras.h"
#include "SpellMgr.h"
#include "SpellScript.h"
#include "TemporarySummon.h"
#include "castle_nathria.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <functional>

enum InervaSpells
{
    // Focus Anima, one cast per container (3.0 s)
    SPELL_FOCUS_DESIRE              = 331844,
    SPELL_FOCUS_BOTTLED             = 331870,
    SPELL_FOCUS_SINS                = 331872,
    SPELL_FOCUS_CONCENTRATED        = 331873,

    // valves
    SPELL_ANIMA_RELEASE             = 325117,   // client click, opens
    SPELL_ANIMA_RELEASE_CLOSE       = 339100,   // client click, closes

    // raid-wide
    SPELL_LOOSE_ANIMA               = 325184,
    SPELL_CONTAINER_BREACH          = 325225,

    // Expose Desires
    SPELL_EXPOSE_DESIRES_1          = 341621,
    SPELL_EXPOSE_DESIRES_2          = 341623,   // Expose Cognition
    SPELL_EXPOSE_DESIRES_3          = 341625,   // Expose Heart
    SPELL_WARPED_DESIRES            = 325382,
    SPELL_SHARED_COGNITION          = 325936,
    SPELL_SHARED_COGNITION_COPY     = 325908,
    SPELL_CHANGE_OF_HEART_MARK      = 340452,
    SPELL_CHANGE_OF_HEART           = 325384,

    // Bottled Anima
    SPELL_BOTTLED_ANIMA_1           = 342280,
    SPELL_BOTTLED_ANIMA_2           = 342281,
    SPELL_BOTTLED_ANIMA_3           = 342282,
    SPELL_BOTTLED_IMPACT            = 325769,
    SPELL_UNLEASHED_VOLATILITY      = 329618,

    // Sins and Suffering
    SPELL_SINS_AND_SUFFERING_1      = 342287,
    SPELL_SINS_AND_SUFFERING_2      = 342288,
    SPELL_SINS_AND_SUFFERING_3      = 342290,
    SPELL_SHARED_SUFFERING_MARKER   = 325005,   // on Lady until the link is solved or the next cast
    SPELL_SHARED_SUFFERING_PRE      = 355553,   // 3 s on the three players
    SPELL_SHARED_SUFFERING_LINK     = 324983,   // 60 s
    SPELL_SHARED_SUFFERING_DAMAGE   = 325004,
    SPELL_INDEMNIFICATION           = 331527,
    SPELL_INDEMNIFICATION_CLEANUP   = 345134,
    SPELL_INDEMNIFICATION_CLEANUP_2 = 332562,
    SPELL_ORB_DAMAGE                = 326040,
    SPELL_ANIMA_WEB                 = 326538,

    // Concentrated Anima
    SPELL_CONCENTRATED_ANIMA_1      = 342320,
    SPELL_CONCENTRATED_ANIMA_2      = 342321,
    SPELL_CONCENTRATED_ANIMA_3      = 342322,
    SPELL_CONCENTRATED_DEBUFF       = 340477,   // first target (Specter)
    SPELL_CONCENTRATED_DEBUFF_EXTRA = 332664,   // further targets (Manifestation)
    SPELL_UNLEASHED_SHADOW          = 332668,
    SPELL_FRAGMENTS_OF_SHADOW       = 325596,

    // adds
    SPELL_CONDEMN_SPECTER           = 334017,   // 2.0 s
    SPELL_UNCONSCIONABLE_GUILT      = 331573,
    SPELL_CONDEMN_MANIFESTATION     = 331550    // 6.0 s, interruptible
};

enum InervaCreatures
{
    NPC_CONTAINER_DESIRE            = 168138,
    NPC_CONTAINER_BOTTLED           = 168366,
    NPC_CONTAINER_SINS              = 168367,
    NPC_CONTAINER_CONCENTRATED      = 168368,
    NPC_SINS_OF_THE_PAST            = 166766,
    NPC_SINS_OF_THE_PAST_WEB        = 170544,
    NPC_HARNESSED_SPECTER           = 170199,
    NPC_CONJURED_MANIFESTATION      = 170197,
    NPC_STALKER                     = 172661
};

enum InervaGameObjects
{
    GO_VALVE_DESIRE                 = 352740,
    GO_VALVE_BOTTLED                = 352739,
    GO_VALVE_SINS                   = 352738,
    GO_VALVE_CONCENTRATED           = 352737
};

enum InervaAreaTriggers
{
    // areatrigger_create_properties Id; the Id 20155 row and the 9940023-9940025 rows come with boss_lady_inerva_darkvein.sql
    AT_PROPS_FOCUS                  = 1409,     // AT 16446, sphere r1, at the focused container
    AT_PROPS_BOTTLE                 = 20508,    // AT 24398
    AT_PROPS_POOL                   = 20081,    // AT 24030
    AT_PROPS_WEB                    = 20155,    // AT 24082
    AT_PROPS_FRAGMENT               = 20071,    // AT 24028
    AT_PROPS_FX_FIRST               = 9940022   // custom rows 9940022 (343749, rank 1), 9940023 (343750), 9940024 (343751), 9940025 (344366, full)
};

enum InervaWorldStates
{
    WORLD_STATE_ENCOUNTER_A         = 19157,
    WORLD_STATE_ENCOUNTER_B         = 19160,
    WORLD_STATE_OPEN_VALVES         = 19161,
    WORLD_STATE_POWER_FIRST         = 19162     // 19162 Desire, 19163 Bottled, 19164 Sins, 19165 Concentrated
};

enum InervaTexts
{
    SAY_AGGRO                       = 0,
    EMOTE_FOCUS_DESIRE              = 1,
    SAY_EXPOSE_1                    = 2,
    EMOTE_SINS                      = 3,
    SAY_SINS_1                      = 4,
    SAY_BOTTLED_1                   = 5,
    EMOTE_FOCUS_BOTTLED             = 6,
    SAY_EXPOSE_2                    = 7,
    EMOTE_LIFE_LINKS                = 8,
    SAY_LINKED                      = 9,
    SAY_BOTTLED_2                   = 10,
    EMOTE_FOCUS_SINS                = 11,
    SAY_BOTTLED_3                   = 12,
    SAY_SINS_2                      = 13,
    SAY_SINS_3                      = 14,
    EMOTE_FOCUS_CONCENTRATED        = 15,
    SAY_CONCENTRATED                = 16,
    EMOTE_BREACH                    = 17,
    SAY_BREACH                      = 18,
    SAY_SIN_CONSUMES                = 19,   // trigger unknown, not used
    SAY_DEATH                       = 20
};

enum InervaContainers : uint8
{
    CONTAINER_DESIRE                = 0,
    CONTAINER_BOTTLED               = 1,
    CONTAINER_SINS                  = 2,
    CONTAINER_CONCENTRATED          = 3,
    CONTAINER_COUNT                 = 4
};

static constexpr uint32 ContainerEntries[CONTAINER_COUNT] = { NPC_CONTAINER_DESIRE, NPC_CONTAINER_BOTTLED, NPC_CONTAINER_SINS, NPC_CONTAINER_CONCENTRATED };
static constexpr uint32 ValveEntries[CONTAINER_COUNT] = { GO_VALVE_DESIRE, GO_VALVE_BOTTLED, GO_VALVE_SINS, GO_VALVE_CONCENTRATED };
static constexpr uint32 FocusSpells[CONTAINER_COUNT] = { SPELL_FOCUS_DESIRE, SPELL_FOCUS_BOTTLED, SPELL_FOCUS_SINS, SPELL_FOCUS_CONCENTRATED };
static constexpr uint32 FocusEmotes[CONTAINER_COUNT] = { EMOTE_FOCUS_DESIRE, EMOTE_FOCUS_BOTTLED, EMOTE_FOCUS_SINS, EMOTE_FOCUS_CONCENTRATED };

// Lady's rank auras, per container and rank (capture: plain dummy auras that mirror the state; 325916 has no effect rows)
static constexpr uint32 RankAuras[CONTAINER_COUNT][3] =
{
    { 326256, 325914, 325915 },
    { 326257, 325916, 325917 },
    { 326259, 325924, 325925 },     // Sins
    { 326258, 325922, 325923 }      // Concentrated
};

static constexpr uint32 ExposeSpells[3] = { SPELL_EXPOSE_DESIRES_1, SPELL_EXPOSE_DESIRES_2, SPELL_EXPOSE_DESIRES_3 };
static constexpr uint32 BottledSpells[3] = { SPELL_BOTTLED_ANIMA_1, SPELL_BOTTLED_ANIMA_2, SPELL_BOTTLED_ANIMA_3 };
static constexpr uint32 SinsSpells[3] = { SPELL_SINS_AND_SUFFERING_1, SPELL_SINS_AND_SUFFERING_2, SPELL_SINS_AND_SUFFERING_3 };
static constexpr uint32 ConcentratedSpells[3] = { SPELL_CONCENTRATED_ANIMA_1, SPELL_CONCENTRATED_ANIMA_2, SPELL_CONCENTRATED_ANIMA_3 };
static constexpr uint32 BottledTexts[3] = { SAY_BOTTLED_1, SAY_BOTTLED_2, SAY_BOTTLED_3 };
static constexpr uint32 SinsTexts[3] = { SAY_SINS_1, SAY_SINS_2, SAY_SINS_3 };

// container model (capture, one 1.0 s tick for all four)
static constexpr Milliseconds ContainerTick = 1000ms;
static constexpr int32 ContainerMaxPower = 1800;
static constexpr int32 ContainerClosedFill = 3;
static constexpr int32 ContainerFocusFill = 14;
static constexpr int32 ContainerFocusFillDesire = 16;
static constexpr int32 ContainerOpenNet = -31;         // -34 vent + 3 base fill
static constexpr int32 RankTwoPower = 600;
static constexpr int32 RankThreePower = 1200;          // 1200, not 1188: the rank auras flipped at 1200 in 4 of 4
static constexpr uint32 FocusSwitchTicks[CONTAINER_COUNT] = { 1, 104, 204, 301 };
static constexpr Milliseconds FocusStarts[CONTAINER_COUNT] = { 0ms, 100s, 200s, 300s };
static constexpr Milliseconds FocusEmoteDelay = 200ms;
static constexpr Milliseconds FocusValveStepDelay = 400ms;      // after the cast ends (338749 / 338750 / 346231 at GO + 0.25 ... 0.69)
static constexpr Milliseconds FocusCastTime = 3010ms;           // START to GO, used when the cast itself is not possible
static constexpr Milliseconds ValveEmptyCloseDelay = 700ms;

// Lady's energy
static constexpr float EnergyMax = 100.0f;
static constexpr uint32 EnergyFirstTick = 4;                    // first sample 12 at 4.29 s
static constexpr float EnergyRegen[CONTAINER_COUNT] = { 12.5f, 10.0f, 8.33f, 8.33f };

// cast scheduling
static constexpr Milliseconds PollGrid = 1220ms;                // Lady cast starts sit on a 1.22 s grid
static constexpr Milliseconds NonExposeMinGap = 6500ms;         // smallest observed start-to-start between two non-Expose casts
static constexpr Milliseconds FirstReady[CONTAINER_COUNT] = { 0ms, 18300ms, 30500ms, 53300ms };   // capture
static constexpr Milliseconds LateLoopCooldown = 50000ms;       // capture: 50.0-50.1 s for all three while Concentrated is focused
static constexpr Milliseconds LooseAnimaFirst = 1100ms;
static constexpr Milliseconds LooseAnimaInterval = 10s;
static constexpr float LooseAnimaPerValve = 1.75f;

// Bottled Anima
static constexpr Milliseconds BottleImpactDelay = 5010ms;
static constexpr Milliseconds BottleATDuration = 5500ms;
static constexpr Milliseconds PoolDuration = 600s;
static constexpr float BottleImpactRadius = 4.0f;
static constexpr float BottleOffsetMin = 1.6f;
static constexpr float BottleOffsetMax = 11.0f;
static constexpr float RoomRadius = 35.0f;
static constexpr Position RoomCenter = { -1601.3f, 6998.4f, 4291.9f };

// Sins and Suffering
static constexpr Position WebAnchor = { -1601.26f, 6985.39f, 4291.37f };
static constexpr float OrbSpawnRadius = 33.0f;
static constexpr float OrbWanderRadius = 40.0f;
static constexpr Milliseconds OrbLifetime = 61s;
static constexpr Milliseconds WebDelay = 3500ms;                // after the cast ends (cast start + 5.5 s)
static constexpr Milliseconds MarkerDelay = 4100ms;             // after the cast ends (cast start + 6.1 s)
static constexpr Milliseconds LinkDelay = 3s;                   // 355553 -> 324983
static constexpr Milliseconds LinkTick = 500ms;
static constexpr float LinkBeamHalfWidth = 2.0f;                // estimate
static constexpr uint8 LinkTargets = 3;
static constexpr Milliseconds WebTick = 250ms;
static constexpr float WebContactWidth = 1.0f;                  // estimate
static constexpr float WebHeight = 2.11f;

// Concentrated Anima
static constexpr Milliseconds ConcentratedDebuffDelay = 650ms;  // after the cast ends
static constexpr uint8 FragmentCount = 9;
static constexpr float FragmentSpacing = float(M_PI) * 2.0f / 9.0f;   // 40 degrees
static constexpr float FragmentTravel = 100.0f;
static constexpr Milliseconds FragmentDuration = 30s;

// Container Breach
static constexpr Milliseconds BreachEmoteInterval = 16s;
static constexpr Milliseconds BreachYellInterval = 30500ms;

// Stalker 172661: dynamic spawns in the capture
static constexpr Position StalkerPosition = { -1626.77f, 6967.01f, 4299.28f };
static constexpr Milliseconds StalkerFirstSpawn = 72900ms;
static constexpr Milliseconds StalkerFirstLife = 68500ms;
static constexpr Milliseconds StalkerSecondSpawn = 570600ms;

// valve game object state of the capture: before the pull State 0 (ACTIVE) with these dynamic flags, at the engage READY and none
static constexpr uint32 ValveInertDynamicFlags = GO_DYNFLAG_LO_NO_INTERACT | GO_DYNFLAG_LO_STATE_TRANSITION_ANIM_DONE;   // 32896

// adds
static constexpr Milliseconds SpecterCondemnFirst = 7600ms;
static constexpr Milliseconds SpecterCondemnInterval = 2450ms;
static constexpr Milliseconds SpecterGuiltFirst = 9100ms;
static constexpr Milliseconds SpecterGuiltInterval = 9500ms;
static constexpr Milliseconds ManifestationFirst = 2s;
static constexpr Milliseconds OrbDamageFirst = 4s;
static constexpr Milliseconds OrbDamageInterval = 3s;

// everything the fight puts on players
static constexpr uint32 FightPlayerAuras[] =
{
    SPELL_LOOSE_ANIMA, SPELL_WARPED_DESIRES, SPELL_SHARED_COGNITION, SPELL_SHARED_COGNITION_COPY, SPELL_CHANGE_OF_HEART_MARK,
    SPELL_SHARED_SUFFERING_PRE, SPELL_SHARED_SUFFERING_LINK, SPELL_INDEMNIFICATION, SPELL_ANIMA_WEB, 325713,
    SPELL_CONCENTRATED_DEBUFF, SPELL_CONCENTRATED_DEBUFF_EXTRA, SPELL_UNCONSCIONABLE_GUILT
};

static uint8 RankForPower(int32 power)
{
    if (power >= RankThreePower)
        return 3;
    return power >= RankTwoPower ? 2 : 1;
}

static uint8 FocusForTick(uint32 tick)
{
    for (uint8 i = CONTAINER_COUNT; i > 0; --i)
        if (tick >= FocusSwitchTicks[i - 1])
            return i - 1;
    return CONTAINER_DESIRE;
}

static float DistanceToSegment2D(Position const& p, Position const& a, Position const& b)
{
    float abx = b.GetPositionX() - a.GetPositionX();
    float aby = b.GetPositionY() - a.GetPositionY();
    float lengthSq = abx * abx + aby * aby;
    float t = 0.0f;
    if (lengthSq > 0.0f)
        t = std::clamp(((p.GetPositionX() - a.GetPositionX()) * abx + (p.GetPositionY() - a.GetPositionY()) * aby) / lengthSq, 0.0f, 1.0f);

    float dx = p.GetPositionX() - (a.GetPositionX() + t * abx);
    float dy = p.GetPositionY() - (a.GetPositionY() + t * aby);
    return std::sqrt(dx * dx + dy * dy);
}

static Position RandomPointInDisc(Position const& center, float radius)
{
    float distance = radius * std::sqrt(frand(0.0f, 1.0f));
    float angle = frand(0.0f, 2.0f * float(M_PI));
    return Position(center.GetPositionX() + distance * std::cos(angle), center.GetPositionY() + distance * std::sin(angle), center.GetPositionZ(), 0.0f);
}

// 165521 - Lady Inerva Darkvein
struct boss_lady_inerva_darkvein : public BossAI
{
    boss_lady_inerva_darkvein(Creature* creature) : BossAI(creature, DATA_LADY_INERVA_DARKVEIN) { }

    void Reset() override
    {
        _Reset();
        ResetFight();
    }

    void JustEngagedWith(Unit* who) override
    {
        BossAI::JustEngagedWith(who);
        if (!me->IsEngaged())
            return;

        Talk(SAY_AGGRO);
        _fightActive = true;
        DoUpdateWorldStates(WORLD_STATE_ENCOUNTER_A, 1);
        DoUpdateWorldStates(WORLD_STATE_ENCOUNTER_B, 1);

        me->SetPowerType(POWER_ENERGY);
        me->SetMaxPower(POWER_ENERGY, int32(EnergyMax));
        me->SetPower(POWER_ENERGY, 0);

        for (uint8 i = 0; i < CONTAINER_COUNT; ++i)
        {
            PrepareContainer(i);
            if (GameObject* valve = GetValve(i))
            {
                // the pull makes the valves usable: State 1 and no dynamic flags (capture, 0.04 s)
                valve->SetGoState(GO_STATE_READY);
                valve->ReplaceAllDynamicFlags(0);
                valve->RemoveFlag(GO_FLAG_LOCKED);
            }
            ApplyRankAura(i, 1);
        }

        for (uint8 i = CONTAINER_BOTTLED; i < CONTAINER_COUNT; ++i)
            _nextReady[i] = FirstReady[i];
    }

    void JustSummoned(Creature* summon) override
    {
        summons.Summon(summon);
        switch (summon->GetEntry())
        {
            case NPC_SINS_OF_THE_PAST:
            case NPC_SINS_OF_THE_PAST_WEB:
                _orbs.push_back(summon->GetGUID());
                break;
            default:
                break;
        }
    }

    void JustDied(Unit* killer) override
    {
        BossAI::JustDied(killer);
        Talk(SAY_DEATH);
        ResetFight();
    }

    void EnterEvadeMode(EvadeReason why) override
    {
        ResetFight();
        BossAI::EnterEvadeMode(why);
    }

    void OnSpellCast(SpellInfo const* spell) override
    {
        uint8 rank = 0;
        if (FindRank(ExposeSpells, spell->Id, rank))
            HandleExposeCast(rank);
        else if (FindRank(BottledSpells, spell->Id, rank))
            HandleBottledCast(rank);
        else if (FindRank(SinsSpells, spell->Id, rank))
            HandleSinsCast(rank);
        else if (FindRank(ConcentratedSpells, spell->Id, rank))
            HandleConcentratedCast(rank);
    }

    void UpdateAI(uint32 diff) override
    {
        if (!_fightActive)
            return;

        if (!UpdateVictim())
            return;

        // all timers run on one clock and do not wait for Lady's casts (the container tick and the focus clock are server-side)
        _clock += Milliseconds(diff);

        while (_focusStep < CONTAINER_COUNT && _clock >= FocusStarts[_focusStep])
            StartFocus(_focusStep++);

        while (_clock >= _nextTick)
        {
            _nextTick += ContainerTick;
            RunContainerTick();
        }

        while (_clock >= _nextPoll)
        {
            _nextPoll += PollGrid;
            Poll();
        }

        if (_clock >= _nextLoose)
        {
            _nextLoose += LooseAnimaInterval;
            CastLooseAnima();
        }

        if (_webCount > 0 && _clock >= _nextWebTick)
        {
            _nextWebTick = _clock + WebTick;
            RunWebContact();
        }

        if (!_linked.empty() && _clock >= _nextLinkTick)
        {
            _nextLinkTick = _clock + LinkTick;
            RunLinkTick();
        }

        RunStalker();
    }

    // valve clicks (spell scripts) ---------------------------------------------------------------------------------------------

    int32 GetOpenValveCount() const { return std::popcount(uint32(_openMask)); }

    bool IsFightActive() const { return _fightActive; }

    int8 GetValveIndex(uint32 entry) const
    {
        for (uint8 i = 0; i < CONTAINER_COUNT; ++i)
            if (ValveEntries[i] == entry)
                return int8(i);
        return -1;
    }

    // 325117 opens, 339100 closes; the core only lets 325117 through, so 325117 on an open valve closes it
    void HandleValveClick(GameObject* valve, bool opening)
    {
        if (!_fightActive || !valve)
            return;

        int8 index = GetValveIndex(valve->GetEntry());
        if (index < 0 || valve->HasFlag(GO_FLAG_LOCKED))
            return;

        bool isOpen = (_openMask & (1 << index)) != 0;
        if (opening && !isOpen)
            SetValveOpen(uint8(index), true);
        else if (isOpen)
            SetValveOpen(uint8(index), false);
    }

    // abilities (spell scripts) --------------------------------------------------------------------------------------------------

    uint8 GetConcentratedRank() const { return _concentratedRank; }
    uint8 GetLastExposeRank() const { return _lastExposeRank; }

    // Expose hit: Shared Cognition goes to random raiders, every holder copies the damage
    void ApplySharedCognition(Unit* victim, uint8 rank)
    {
        std::vector<Player*> players = GetPlayers();

        for (Player* player : players)
            if (player->HasAura(SPELL_SHARED_COGNITION, me->GetGUID()))
                me->CastSpell(player, SPELL_SHARED_COGNITION_COPY, true);

        if (rank < 2)
            return;

        std::vector<Player*> candidates;
        for (Player* player : players)
            if (player != victim)
                candidates.push_back(player);
        Trinity::Containers::RandomShuffle(candidates);

        uint32 count = 1;
        if (me->GetMap()->IsMythic())
            count = 4;
        else if (me->GetMap()->IsHeroic())
            count = urand(2, 3);
        else if (!me->GetMap()->IsLFR())
            count = urand(1, 2);

        for (size_t i = 0; i < candidates.size() && i < count; ++i)
            me->CastSpell(candidates[i], SPELL_SHARED_COGNITION, true);
    }

    // Concentrated debuff ran out on the victim
    void HandleConcentratedExpire(Unit* victim, bool extraTarget)
    {
        if (!_fightActive || !victim)
            return;

        me->CastSpell(victim, SPELL_UNLEASHED_SHADOW, true);

        Position pos = victim->GetPosition();
        me->SummonCreature(extraTarget ? NPC_CONJURED_MANIFESTATION : NPC_HARNESSED_SPECTER, pos, TEMPSUMMON_MANUAL_DESPAWN);

        if (_concentratedRank >= 3)
            SendFragments(pos);
    }

    // Warped Desires ran out after a rank 3 Expose
    void HandleWarpedDesiresExpire(Unit* victim)
    {
        if (_fightActive && victim && _lastExposeRank >= 3)
            me->CastSpell(victim, SPELL_CHANGE_OF_HEART_MARK, true);
    }

    // orb cleanup spell (345134 / 332562)
    bool IsOrb(Unit const* unit) const
    {
        return unit->GetEntry() == NPC_SINS_OF_THE_PAST || unit->GetEntry() == NPC_SINS_OF_THE_PAST_WEB;
    }

private:
    template <size_t N>
    static bool FindRank(uint32 const (&spells)[N], uint32 spellId, uint8& rank)
    {
        for (size_t i = 0; i < N; ++i)
        {
            if (spells[i] == spellId)
            {
                rank = uint8(i + 1);
                return true;
            }
        }
        return false;
    }

    void Later(Milliseconds delay, std::function<void()> fn)
    {
        me->m_Events.AddEventAtOffset([this, serial = _serial, fn = std::move(fn)]()
        {
            if (serial == _serial && _fightActive)
                fn();
        }, delay);
    }

    std::vector<Player*> GetPlayers() const
    {
        std::vector<Player*> result;
        for (MapReference const& ref : me->GetMap()->GetPlayers())
            if (Player* player = ref.GetSource())
                if (player->IsAlive() && !player->IsGameMaster())
                    result.push_back(player);
        return result;
    }

    void DoUpdateWorldStates(int32 id, int32 value)
    {
        if (instance)
            instance->DoUpdateWorldState(id, value);
    }

    Creature* GetContainer(uint8 index) const
    {
        if (Creature* container = ObjectAccessor::GetCreature(*me, _containerGuids[index]))
            return container;
        return me->FindNearestCreature(ContainerEntries[index], 300.0f);
    }

    GameObject* GetValve(uint8 index) const
    {
        return me->FindNearestGameObject(ValveEntries[index], 300.0f);
    }

    void PrepareContainer(uint8 index)
    {
        if (Creature* container = GetContainer(index))
        {
            _containerGuids[index] = container->GetGUID();
            container->SetPowerType(POWER_ENERGY);
            container->SetMaxPower(POWER_ENERGY, ContainerMaxPower);
            container->SetPower(POWER_ENERGY, 0);
        }
    }

    // fight state ------------------------------------------------------------------------------------------------------------------

    void ResetFight()
    {
        bool wasActive = _fightActive;
        ++_serial;
        _fightActive = false;
        _clock = 0ms;
        _nextTick = ContainerTick;
        _nextPoll = PollGrid;
        _nextLoose = LooseAnimaFirst;
        _nextWebTick = 0ms;
        _nextLinkTick = 0ms;
        _nextBreachEmote = 0ms;
        _nextBreachYell = 0ms;
        _focusStep = 0;
        _focus = CONTAINER_DESIRE;
        _tick = 0;
        _energy = 0.0f;
        _lastAbilityStart = -NonExposeMinGap;
        _openMask = 0;
        _closingMask = 0;
        _breaching = false;
        _concentratedRank = 1;
        _lastExposeRank = 1;
        _exposeYellMask = 0;
        _stalkerStage = 0;
        _webCount = 0;
        _linked.clear();
        _orbs.clear();
        for (uint8 i = 0; i < CONTAINER_COUNT; ++i)
        {
            _power[i] = 0;
            _rank[i] = 0;
            _fxState[i] = 0;
            _nextReady[i] = 0ms;
        }

        RemoveAreaTrigger(_focusAT);
        RemoveAreaTrigger(_webAT);
        for (ObjectGuid& guid : _fxAT)
            RemoveAreaTrigger(guid);
        for (ObjectGuid const& guid : _pools)
        {
            ObjectGuid copy = guid;
            RemoveAreaTrigger(copy);
        }
        _pools.clear();

        if (me->IsAlive())
        {
            for (uint8 i = 0; i < CONTAINER_COUNT; ++i)
                for (uint32 aura : RankAuras[i])
                    me->RemoveAurasDueToSpell(aura);
            me->RemoveAurasDueToSpell(SPELL_SHARED_SUFFERING_MARKER);
            me->SetPowerType(POWER_ENERGY);
            me->SetMaxPower(POWER_ENERGY, int32(EnergyMax));
            me->SetPower(POWER_ENERGY, 0);
        }

        if (!me->GetMap())
            return;

        for (uint8 i = 0; i < CONTAINER_COUNT; ++i)
        {
            if (Creature* container = GetContainer(i))
            {
                container->SetPowerType(POWER_ENERGY);
                container->SetMaxPower(POWER_ENERGY, ContainerMaxPower);
                container->SetPower(POWER_ENERGY, 0);
            }

            if (GameObject* valve = GetValve(i))
            {
                // back to the sniffed pre-pull / post-kill state
                valve->SetGoState(GO_STATE_ACTIVE);
                valve->ReplaceAllDynamicFlags(ValveInertDynamicFlags);
                valve->RemoveFlag(GO_FLAG_LOCKED);
            }
        }

        if (wasActive || instance)
        {
            DoUpdateWorldStates(WORLD_STATE_ENCOUNTER_A, 0);
            DoUpdateWorldStates(WORLD_STATE_ENCOUNTER_B, 0);
            DoUpdateWorldStates(WORLD_STATE_OPEN_VALVES, 0);
            for (uint8 i = 0; i < CONTAINER_COUNT; ++i)
                DoUpdateWorldStates(WORLD_STATE_POWER_FIRST + i, 0);
        }

        for (MapReference const& ref : me->GetMap()->GetPlayers())
            if (Player* player = ref.GetSource())
                for (uint32 spellId : FightPlayerAuras)
                    player->RemoveAurasDueToSpell(spellId);
    }

    void RemoveAreaTrigger(ObjectGuid& guid)
    {
        if (guid.IsEmpty())
            return;
        if (AreaTrigger* at = ObjectAccessor::GetAreaTrigger(*me, guid))
            at->Remove();
        guid.Clear();
    }

    // containers and valves -------------------------------------------------------------------------------------------------------

    void ApplyRankAura(uint8 container, uint8 rank)
    {
        for (uint32 aura : RankAuras[container])
            me->RemoveAurasDueToSpell(aura);
        me->AddAura(RankAuras[container][rank - 1], me);
    }

    // rank state effect at the container: 343749 / 343750 / 343751 by rank, 344366 when full
    void SetContainerFx(uint8 container, uint8 state)
    {
        if (_fxState[container] == state)
            return;
        _fxState[container] = state;

        RemoveAreaTrigger(_fxAT[container]);
        Creature* creature = GetContainer(container);
        if (!creature || state == 0)
            return;

        AreaTrigger* at = AreaTrigger::CreateAreaTrigger({ AT_PROPS_FX_FIRST + state - 1u, true }, creature->GetPosition(), -1, creature, nullptr);
        if (at)
            _fxAT[container] = at->GetGUID();
    }

    void SetValveOpen(uint8 container, bool open)
    {
        GameObject* valve = GetValve(container);
        if (!valve)
            return;

        if (open)
            _openMask |= 1 << container;
        else
        {
            _openMask &= ~(1 << container);
            _closingMask &= ~(1 << container);
        }

        valve->SetGoState(open ? GO_STATE_ACTIVE : GO_STATE_READY);
        DoUpdateWorldStates(WORLD_STATE_OPEN_VALVES, GetOpenValveCount());
    }

    // 0.4 s after the Focus Anima cast ends: Shutoff Valve + Disable Container on the new focus, Enable Container on the others
    void ValveStep(uint8 focus)
    {
        for (uint8 i = 0; i < CONTAINER_COUNT; ++i)
        {
            GameObject* valve = GetValve(i);
            if (!valve)
                continue;

            if (i == focus)
            {
                if (_openMask & (1 << i))
                    SetValveOpen(i, false);
                valve->SetFlag(GO_FLAG_LOCKED);
            }
            else
                valve->RemoveFlag(GO_FLAG_LOCKED);
        }
    }

    void StartFocus(uint8 index)
    {
        // the clock start cancels a cast in progress (capture: Bottled 342282 started 199.87 s was cancelled by the 200.00 s start)
        me->InterruptNonMeleeSpells(false);

        Later(FocusEmoteDelay, [this, index]() { Talk(FocusEmotes[index]); });

        // the cast is the visible part; its end is timed here so that a refused cast cannot hold the mechanic back
        DoCastSelf(FocusSpells[index]);
        Later(FocusCastTime, [this, index]()
        {
            RemoveAreaTrigger(_focusAT);
            if (Creature* container = GetContainer(index))
                if (AreaTrigger* at = AreaTrigger::CreateAreaTrigger({ AT_PROPS_FOCUS, false }, container->GetPosition(), -1, container, nullptr))
                    _focusAT = at->GetGUID();

            Later(FocusValveStepDelay, [this, index]() { ValveStep(index); });
        });
    }

    void RunContainerTick()
    {
        ++_tick;
        _focus = FocusForTick(_tick);

        for (uint8 i = 0; i < CONTAINER_COUNT; ++i)
        {
            int32 delta = ContainerClosedFill;
            if (_openMask & (1 << i))
                delta = ContainerOpenNet;
            else if (i == _focus)
                delta = i == CONTAINER_DESIRE ? ContainerFocusFillDesire : ContainerFocusFill;

            _power[i] = std::clamp(_power[i] + delta, 0, ContainerMaxPower);

            if (Creature* container = GetContainer(i))
                container->SetPower(POWER_ENERGY, _power[i]);
            DoUpdateWorldStates(WORLD_STATE_POWER_FIRST + i, _power[i]);

            uint8 rank = RankForPower(_power[i]);
            if (rank != _rank[i])
            {
                _rank[i] = rank;
                ApplyRankAura(i, rank);
            }
            SetContainerFx(i, _power[i] >= ContainerMaxPower ? uint8(4) : rank);

            // an emptied valve closes 0.7 s later without a click
            if (_power[i] == 0 && (_openMask & (1 << i)) && !(_closingMask & (1 << i)))
            {
                _closingMask |= 1 << i;
                Later(ValveEmptyCloseDelay, [this, i]()
                {
                    if (_openMask & (1 << i))
                        SetValveOpen(i, false);
                });
            }
        }

        if (_tick >= EnergyFirstTick)
        {
            _energy = std::min(EnergyMax, _energy + EnergyRegen[_focus]);
            me->SetPower(POWER_ENERGY, int32(_energy));
        }

        RunBreach();
    }

    void RunBreach()
    {
        bool full = false;
        for (uint8 i = 0; i < CONTAINER_COUNT; ++i)
            full = full || _power[i] >= ContainerMaxPower;

        if (!full)
        {
            _breaching = false;
            return;
        }

        if (!_breaching)
        {
            _breaching = true;
            _nextBreachEmote = _clock + FocusEmoteDelay;
            _nextBreachYell = _clock + FocusEmoteDelay;
        }

        if (_clock >= _nextBreachEmote)
        {
            _nextBreachEmote += BreachEmoteInterval;
            Talk(EMOTE_BREACH);
        }

        if (_clock >= _nextBreachYell)
        {
            _nextBreachYell += BreachYellInterval;
            Talk(SAY_BREACH);
        }

        me->CastSpell(me, SPELL_CONTAINER_BREACH, true);
    }

    void CastLooseAnima()
    {
        for (Player* player : GetPlayers())
            me->CastSpell(player, SPELL_LOOSE_ANIMA, true);
    }

    // abilities --------------------------------------------------------------------------------------------------------------------

    Milliseconds AbilityCooldown(uint8 container) const
    {
        if (_focus == CONTAINER_CONCENTRATED)
            return LateLoopCooldown;

        switch (container)
        {
            case CONTAINER_BOTTLED:
                return _focus == CONTAINER_BOTTLED ? 20s : 40s;
            case CONTAINER_SINS:
                if (_focus == CONTAINER_SINS)
                    return 35s;
                return _focus == CONTAINER_BOTTLED ? 64s : 50s;
            default:
                return 70s;
        }
    }

    void Poll()
    {
        if (me->HasUnitState(UNIT_STATE_CASTING) || !me->GetVictim())
            return;

        if (_energy >= EnergyMax)
        {
            uint8 rank = _rank[CONTAINER_DESIRE] ? _rank[CONTAINER_DESIRE] : uint8(1);
            DoCastVictim(ExposeSpells[rank - 1]);
            return;
        }

        if (_clock - _lastAbilityStart < NonExposeMinGap)
            return;

        uint8 best = 0;
        for (uint8 i = CONTAINER_BOTTLED; i < CONTAINER_COUNT; ++i)
            if (_clock >= _nextReady[i] && (!best || _nextReady[i] < _nextReady[best]))
                best = i;

        if (best)
            StartAbility(best);
    }

    void StartAbility(uint8 container)
    {
        uint8 rank = _rank[container] ? _rank[container] : uint8(1);
        _lastAbilityStart = _clock;
        _nextReady[container] = _clock + AbilityCooldown(container);

        uint32 spellId = 0;
        switch (container)
        {
            case CONTAINER_BOTTLED:
                spellId = BottledSpells[rank - 1];
                break;
            case CONTAINER_SINS:
                spellId = SinsSpells[rank - 1];
                // Indemnification: the previous links were not solved
                if (me->HasAura(SPELL_SHARED_SUFFERING_MARKER))
                    Indemnify();
                break;
            default:
                spellId = ConcentratedSpells[rank - 1];
                break;
        }

        // the effect follows the cast (OnSpellCast); when the cast is refused the effect runs at once
        if (DoCastSelf(spellId) != SPELL_CAST_OK)
            OnSpellCast(sSpellMgr->GetSpellInfo(spellId, me->GetMap()->GetDifficultyID()));
    }

    void Indemnify()
    {
        me->CastSpell(me, SPELL_INDEMNIFICATION, true);
        me->CastSpell(me, SPELL_INDEMNIFICATION_CLEANUP, true);
        me->RemoveAurasDueToSpell(SPELL_SHARED_SUFFERING_MARKER);
        EndLink(true);
        RemoveAreaTrigger(_webAT);
        _webCount = 0;
    }

    void HandleExposeCast(uint8 rank)
    {
        _energy = 0.0f;
        me->SetPower(POWER_ENERGY, 0);
        _lastExposeRank = rank;

        // the yells came on 2 of 47 casts (rank 1 once, rank 2 and 3 once each): once per rank and fight
        uint8 bit = 1 << (rank - 1);
        if (!(_exposeYellMask & bit))
        {
            _exposeYellMask |= bit;
            Talk(rank == 1 ? SAY_EXPOSE_1 : SAY_EXPOSE_2);
        }
    }

    // Bottled Anima ---------------------------------------------------------------------------------------------------------------------

    void HandleBottledCast(uint8 rank)
    {
        Talk(BottledTexts[rank - 1]);
        LaunchBottle(rank, false);
    }

    Position PickBottlePoint(bool replica) const
    {
        std::vector<Player*> players = GetPlayers();
        if (!replica && !players.empty())
        {
            Player* player = Trinity::Containers::SelectRandomContainerElement(players);
            Position pos = player->GetPosition();
            float angle = frand(0.0f, 2.0f * float(M_PI));
            float distance = frand(BottleOffsetMin, BottleOffsetMax);
            pos.m_positionX = player->GetPositionX() + distance * std::cos(angle);
            pos.m_positionY = player->GetPositionY() + distance * std::sin(angle);
            pos.m_positionZ = player->GetPositionZ();
            return pos;
        }
        return RandomPointInDisc(RoomCenter, RoomRadius);
    }

    void LaunchBottle(uint8 rank, bool replica)
    {
        Position dest = PickBottlePoint(replica);
        AreaTrigger::CreateAreaTrigger({ AT_PROPS_BOTTLE, false }, dest, int32(BottleATDuration.count()), me, nullptr);

        Later(BottleImpactDelay, [this, dest, rank, replica]() { BottleImpact(dest, rank, replica); });
    }

    void BottleImpact(Position const& dest, uint8 rank, bool replica)
    {
        bool anyoneInside = false;
        for (Player* player : GetPlayers())
            if (player->GetExactDist(&dest) <= BottleImpactRadius)
                anyoneInside = true;

        // 325769 hits the players inside the circle; with nobody inside the volatility hits the whole raid
        me->CastSpell(dest, anyoneInside ? SPELL_BOTTLED_IMPACT : SPELL_UNLEASHED_VOLATILITY, true);

        if (rank >= 2)
            if (AreaTrigger* pool = AreaTrigger::CreateAreaTrigger({ AT_PROPS_POOL, false }, dest, int32(PoolDuration.count()), me, nullptr))
                _pools.push_back(pool->GetGUID());

        // rank 3: the replica falls at the first impact, 5.0 s later it lands
        if (rank >= 3 && !replica)
            LaunchBottle(rank, true);
    }

    // Sins and Suffering -----------------------------------------------------------------------------------------------------------------

    void HandleSinsCast(uint8 rank)
    {
        Talk(EMOTE_SINS);
        Talk(SinsTexts[rank - 1]);

        for (uint8 i = 0; i < 3; ++i)
        {
            Position pos = RandomPointInDisc(WebAnchor, OrbSpawnRadius);
            me->SummonCreature(NPC_SINS_OF_THE_PAST, pos, TEMPSUMMON_TIMED_DESPAWN, OrbLifetime);
        }

        if (rank >= 2)
            Later(WebDelay, [this]() { SpawnWebOrbs(); });

        Later(MarkerDelay, [this]() { MarkAndLink(); });
    }

    void SpawnWebOrbs()
    {
        Position verts[3];
        for (uint8 i = 0; i < 3; ++i)
        {
            verts[i] = RandomPointInDisc(WebAnchor, OrbSpawnRadius);
            me->SummonCreature(NPC_SINS_OF_THE_PAST_WEB, verts[i], TEMPSUMMON_TIMED_DESPAWN, OrbLifetime);
        }

        // Anima Web: a triangle on the three spawn points that stays fixed (6 of 6 casts)
        RemoveAreaTrigger(_webAT);
        AreaTrigger* web = AreaTrigger::CreateAreaTrigger({ AT_PROPS_WEB, false }, WebAnchor, -1, me, nullptr);
        if (!web)
            return;

        _webAT = web->GetGUID();
        for (uint8 i = 0; i < 3; ++i)
            _webVerts[i] = verts[i];
        _webCount = 3;
        _nextWebTick = _clock + WebTick;

        AreaTriggerShapeInfo shape;
        AreaTriggerShapeInfo::Polygon polygon;
        polygon.Height = WebHeight;
        polygon.HeightTarget = WebHeight;
        polygon.Type = AreaTriggerShapeType::Polygon;
        for (uint8 i = 0; i < 3; ++i)
        {
            TaggedPosition<Position::XY> vertex(verts[i].GetPositionX() - WebAnchor.GetPositionX(), verts[i].GetPositionY() - WebAnchor.GetPositionY());
            polygon.PolygonVertices.push_back(vertex);
            polygon.PolygonVerticesTarget.push_back(vertex);
        }
        shape.Data = polygon;
        web->SetShape(shape);
        web->UpdateShape();

        // the web goes with its orbs
        ObjectGuid guid = _webAT;
        Later(OrbLifetime - WebDelay, [this, guid]()
        {
            if (_webAT == guid)
            {
                RemoveAreaTrigger(_webAT);
                _webCount = 0;
            }
        });
    }

    // 4.1 s after the cast: Lady carries Shared Suffering; three players are marked and linked 3 s later
    void MarkAndLink()
    {
        me->CastSpell(me, SPELL_SHARED_SUFFERING_MARKER, true);

        std::vector<Player*> players = GetPlayers();
        if (players.size() < LinkTargets)
            return;     // alone the capture linked nobody

        Talk(EMOTE_LIFE_LINKS);
        Talk(SAY_LINKED);

        Trinity::Containers::RandomShuffle(players);
        std::vector<ObjectGuid> chosen;
        for (uint8 i = 0; i < LinkTargets; ++i)
        {
            chosen.push_back(players[i]->GetGUID());
            me->CastSpell(players[i], SPELL_SHARED_SUFFERING_PRE, true);
        }

        Later(LinkDelay, [this, chosen]()
        {
            _linked.clear();
            for (ObjectGuid const& guid : chosen)
            {
                Player* player = ObjectAccessor::GetPlayer(*me, guid);
                if (!player || !player->IsAlive())
                    continue;
                me->CastSpell(player, SPELL_SHARED_SUFFERING_LINK, true);
                _linked.push_back(guid);
            }
            _nextLinkTick = _clock + LinkTick;
        });
    }

    void EndLink(bool removeAuras)
    {
        if (removeAuras)
        {
            for (ObjectGuid const& guid : _linked)
                if (Player* player = ObjectAccessor::GetPlayer(*me, guid))
                    player->RemoveAurasDueToSpell(SPELL_SHARED_SUFFERING_LINK);
        }
        _linked.clear();
    }

    std::vector<Player*> LinkedPlayers()
    {
        std::vector<Player*> result;
        for (ObjectGuid const& guid : _linked)
        {
            Player* player = ObjectAccessor::GetPlayer(*me, guid);
            if (player && player->IsAlive() && player->HasAura(SPELL_SHARED_SUFFERING_LINK))
                result.push_back(player);
        }
        return result;
    }

    // every 0.5 s: damage on the linked and on those standing in a beam; the link is solved when the beams reach every orb
    void RunLinkTick()
    {
        std::vector<Player*> linked = LinkedPlayers();
        if (linked.size() < 2)
        {
            _linked.clear();
            return;
        }

        auto inBeam = [&linked](Position const& pos, float width) -> bool
        {
            for (size_t i = 0; i < linked.size(); ++i)
                for (size_t j = i + 1; j < linked.size(); ++j)
                    if (DistanceToSegment2D(pos, linked[i]->GetPosition(), linked[j]->GetPosition()) <= width)
                        return true;
            return false;
        };

        for (Player* player : linked)
            me->CastSpell(player, SPELL_SHARED_SUFFERING_DAMAGE, true);
        for (Player* player : GetPlayers())
            if (std::ranges::find(linked, player) == linked.end() && inBeam(player->GetPosition(), LinkBeamHalfWidth))
                me->CastSpell(player, SPELL_SHARED_SUFFERING_DAMAGE, true);

        uint32 orbs = 0;
        bool allCovered = true;
        for (ObjectGuid const& guid : _orbs)
        {
            Creature* orb = ObjectAccessor::GetCreature(*me, guid);
            if (!orb || !orb->IsAlive())
                continue;
            ++orbs;
            allCovered = allCovered && inBeam(orb->GetPosition(), LinkBeamHalfWidth);
        }

        if (orbs && allCovered)
        {
            me->RemoveAurasDueToSpell(SPELL_SHARED_SUFFERING_MARKER);
            EndLink(true);
        }
    }

    // every 0.25 s: a player on a strand of the web gets the 10 s aura
    void RunWebContact()
    {
        for (Player* player : GetPlayers())
        {
            if (player->HasAura(SPELL_ANIMA_WEB))
                continue;
            for (uint8 i = 0; i < 3; ++i)
            {
                if (DistanceToSegment2D(player->GetPosition(), _webVerts[i], _webVerts[(i + 1) % 3]) <= WebContactWidth)
                {
                    me->AddAura(SPELL_ANIMA_WEB, player);
                    break;
                }
            }
        }
    }

    // Concentrated Anima -----------------------------------------------------------------------------------------------------------------

    void HandleConcentratedCast(uint8 rank)
    {
        _concentratedRank = rank;
        if (rank < 3)
            Talk(SAY_CONCENTRATED);

        std::vector<Player*> players = GetPlayers();
        if (players.empty())
            return;

        Trinity::Containers::RandomShuffle(players);

        // Normal 1 target; Heroic 340477 plus 332664 on one more (4 of 6 pulls); Mythic two more (8 of 15 casts had one, 7 two)
        size_t extras = 0;
        if (me->GetMap()->IsMythic())
            extras = 2;
        else if (me->GetMap()->IsHeroic())
            extras = 1;
        extras = std::min(extras, players.size() - 1);

        std::vector<ObjectGuid> targets;
        for (size_t i = 0; i <= extras; ++i)
            targets.push_back(players[i]->GetGUID());

        Later(ConcentratedDebuffDelay, [this, targets]()
        {
            for (size_t i = 0; i < targets.size(); ++i)
                if (Player* player = ObjectAccessor::GetPlayer(*me, targets[i]))
                    if (player->IsAlive())
                        me->CastSpell(player, i == 0 ? SPELL_CONCENTRATED_DEBUFF : SPELL_CONCENTRATED_DEBUFF_EXTRA, true);
        });
    }

    // rank 3: nine projectiles 40 degrees apart from a random start angle, 3 yd/s for 100 yd, 30 s
    void SendFragments(Position const& origin)
    {
        float start = frand(0.0f, 2.0f * float(M_PI));
        for (uint8 i = 0; i < FragmentCount; ++i)
        {
            float angle = Position::NormalizeOrientation(start + i * FragmentSpacing);
            Position pos(origin.GetPositionX(), origin.GetPositionY(), origin.GetPositionZ(), angle);
            // the areatrigger script sets the path in OnInitialize
            AreaTrigger::CreateAreaTrigger({ AT_PROPS_FRAGMENT, false }, pos, int32(FragmentDuration.count()), me, nullptr);
        }
    }

    // Stalker ------------------------------------------------------------------------------------------------------------------------------

    void RunStalker()
    {
        if (_stalkerStage == 0 && _clock >= StalkerFirstSpawn)
        {
            _stalkerStage = 1;
            me->SummonCreature(NPC_STALKER, StalkerPosition, TEMPSUMMON_TIMED_DESPAWN, StalkerFirstLife);
        }
        else if (_stalkerStage == 1 && _clock >= StalkerSecondSpawn)
        {
            _stalkerStage = 2;
            me->SummonCreature(NPC_STALKER, StalkerPosition, TEMPSUMMON_MANUAL_DESPAWN);
        }
    }

    // state -----------------------------------------------------------------------------------------------------------------------------------

    Milliseconds _clock = 0ms;
    Milliseconds _nextTick = ContainerTick;
    Milliseconds _nextPoll = PollGrid;
    Milliseconds _nextLoose = LooseAnimaFirst;
    Milliseconds _nextWebTick = 0ms;
    Milliseconds _nextLinkTick = 0ms;
    Milliseconds _nextBreachEmote = 0ms;
    Milliseconds _nextBreachYell = 0ms;
    Milliseconds _lastAbilityStart = -NonExposeMinGap;
    Milliseconds _nextReady[CONTAINER_COUNT] = { };
    uint32 _serial = 0;
    uint32 _tick = 0;
    uint8 _focusStep = 0;
    uint8 _focus = CONTAINER_DESIRE;
    uint8 _openMask = 0;
    uint8 _closingMask = 0;
    uint8 _rank[CONTAINER_COUNT] = { };
    uint8 _fxState[CONTAINER_COUNT] = { };
    uint8 _concentratedRank = 1;
    uint8 _lastExposeRank = 1;
    uint8 _exposeYellMask = 0;
    uint8 _stalkerStage = 0;
    uint8 _webCount = 0;
    int32 _power[CONTAINER_COUNT] = { };
    float _energy = 0.0f;
    bool _fightActive = false;
    bool _breaching = false;
    Position _webVerts[3];
    ObjectGuid _containerGuids[CONTAINER_COUNT];
    ObjectGuid _fxAT[CONTAINER_COUNT];
    ObjectGuid _focusAT;
    ObjectGuid _webAT;
    GuidVector _pools;
    GuidVector _orbs;
    GuidVector _linked;
};

static boss_lady_inerva_darkvein* GetLadyAI(WorldObject* source)
{
    InstanceScript* instance = source ? source->GetInstanceScript() : nullptr;
    Creature* lady = instance ? instance->GetCreature(DATA_LADY_INERVA_DARKVEIN) : nullptr;
    return lady ? dynamic_cast<boss_lady_inerva_darkvein*>(lady->AI()) : nullptr;
}

// 168138, 168366, 168367, 168368 - the four containers: vehicles whose Energy bar is the container's anima. The boss keeps the
// numbers; this only prepares the unit.
struct npc_lady_inerva_container : public ScriptedAI
{
    npc_lady_inerva_container(Creature* creature) : ScriptedAI(creature) { }

    void Reset() override
    {
        me->SetReactState(REACT_PASSIVE);
        me->SetPowerType(POWER_ENERGY);
        me->SetMaxPower(POWER_ENERGY, ContainerMaxPower);
        me->SetPower(POWER_ENERGY, 0);
        me->RemoveUnitFlag2(UNIT_FLAG2_REGENERATE_POWER);
    }

    void JustAppeared() override
    {
        Reset();
    }

    void UpdateAI(uint32 /*diff*/) override { }
};

// 166766, 170544 - Sins of the Past: a 10 yd/s burst of about 18 yd, then 4 yd/s steps of 3.8-5.5 yd about every second; 326040 on
// the nearest player from 4.0 s, every 3.0 s. They are not meant to be killed (flags 33554432).
struct npc_lady_sins_of_the_past : public ScriptedAI
{
    npc_lady_sins_of_the_past(Creature* creature) : ScriptedAI(creature) { }

    void JustAppeared() override
    {
        me->SetReactState(REACT_PASSIVE);
        _damage = OrbDamageFirst;
        _step = 1500ms;
        float angle = frand(0.0f, 2.0f * float(M_PI));
        me->GetMotionMaster()->MovePoint(1, Clamp(me->GetPositionX() + 18.0f * std::cos(angle), me->GetPositionY() + 18.0f * std::sin(angle)), false, {}, 10.0f);
    }

    Position Clamp(float x, float y) const
    {
        Position pos(x, y, me->GetPositionZ(), 0.0f);
        float dx = x - WebAnchor.GetPositionX();
        float dy = y - WebAnchor.GetPositionY();
        float distance = std::sqrt(dx * dx + dy * dy);
        if (distance > OrbWanderRadius)
        {
            pos.m_positionX = WebAnchor.GetPositionX() + dx * OrbWanderRadius / distance;
            pos.m_positionY = WebAnchor.GetPositionY() + dy * OrbWanderRadius / distance;
        }
        return pos;
    }

    void UpdateAI(uint32 diff) override
    {
        _damage -= std::min(_damage, Milliseconds(diff));
        _step -= std::min(_step, Milliseconds(diff));

        if (_damage == 0ms)
        {
            _damage = OrbDamageInterval;
            if (Player* player = me->SelectNearestPlayer(300.0f))
                me->CastSpell(player, SPELL_ORB_DAMAGE, true);
        }

        if (_step == 0ms)
        {
            _step = 1000ms;
            float angle = frand(0.0f, 2.0f * float(M_PI));
            float distance = frand(3.8f, 5.5f);
            me->GetMotionMaster()->MovePoint(1, Clamp(me->GetPositionX() + distance * std::cos(angle), me->GetPositionY() + distance * std::sin(angle)), false, {}, 4.0f);
        }
    }

private:
    Milliseconds _damage = 0ms;
    Milliseconds _step = 0ms;
};

static void InheritLadyFaction(Creature* creature)
{
    InstanceScript* instance = creature->GetInstanceScript();
    Creature* lady = instance ? instance->GetCreature(DATA_LADY_INERVA_DARKVEIN) : nullptr;
    if (!lady)
        lady = creature->FindNearestCreature(BOSS_LADY_INERVA_DARKVEIN, 500.0f);
    if (lady)
        creature->SetFaction(lady->GetFaction());
}

// 170199 - Harnessed Specter: sessile; Condemn (2.0 s, raid-wide) from 7.6 s every 2.45 s while no player stands in melee, Unconscionable
// Guilt on the engaged tank from 9.1 s every 9.5 s
struct npc_lady_harnessed_specter : public ScriptedAI
{
    npc_lady_harnessed_specter(Creature* creature) : ScriptedAI(creature) { }

    void JustAppeared() override
    {
        SetCombatMovement(false);
        DoZoneInCombat();
        _events.ScheduleEvent(1, SpecterCondemnFirst);
        _events.ScheduleEvent(2, SpecterGuiltFirst);
    }

    // the victim first, else any player on the threat list that stands in melee range
    Unit* FindMeleeTarget() const
    {
        Unit* victim = me->GetVictim();
        if (victim && victim->IsAlive() && me->IsWithinMeleeRange(victim))
            return victim;

        for (ThreatReference const* ref : me->GetThreatManager().GetUnsortedThreatList())
        {
            Unit* unit = ref->GetVictim();
            if (unit && unit->IsAlive() && me->IsWithinMeleeRange(unit))
                return unit;
        }
        return nullptr;
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);

        if (!me->HasUnitState(UNIT_STATE_CASTING))
        {
            while (uint32 eventId = _events.ExecuteEvent())
            {
                Unit* melee = FindMeleeTarget();
                if (eventId == 1)
                {
                    if (!melee)
                        DoCastSelf(SPELL_CONDEMN_SPECTER);
                    _events.Repeat(SpecterCondemnInterval);
                }
                else
                {
                    if (melee)
                        DoCast(melee, SPELL_UNCONSCIONABLE_GUILT);
                    _events.Repeat(SpecterGuiltInterval);
                }
                if (me->HasUnitState(UNIT_STATE_CASTING))
                    break;
            }
        }

        UpdateVictim();
    }

private:
    EventMap _events;
};

// 170197 - Conjured Manifestation: stationary caster of Condemn (6.0 s, interruptible). Faction, movement and cadence were never
// captured: it takes Lady's faction and casts back to back.
struct npc_lady_conjured_manifestation : public ScriptedAI
{
    npc_lady_conjured_manifestation(Creature* creature) : ScriptedAI(creature) { }

    void JustAppeared() override
    {
        InheritLadyFaction(me);
        SetCombatMovement(false);
        DoZoneInCombat();
        _delay = ManifestationFirst;
    }

    void UpdateAI(uint32 diff) override
    {
        if (_delay > 0ms)
        {
            _delay -= std::min(_delay, Milliseconds(diff));
            return;
        }

        if (!me->HasUnitState(UNIT_STATE_CASTING))
            DoCastSelf(SPELL_CONDEMN_MANIFESTATION);
    }

private:
    Milliseconds _delay = 0ms;
};

// 352740, 352739, 352738, 352737 - the container valves: before the pull they carry State 0 and the dynamic flags 32896 (capture)
struct go_lady_inerva_valve : public GameObjectAI
{
    go_lady_inerva_valve(GameObject* go) : GameObjectAI(go) { }

    void InitializeAI() override
    {
        InstanceScript* instance = me->GetInstanceScript();
        if (!instance || instance->GetBossState(DATA_LADY_INERVA_DARKVEIN) != IN_PROGRESS)
            me->ReplaceAllDynamicFlags(ValveInertDynamicFlags);
    }
};

// 325117 - Anima Release (open), 339100 - Close: the client casts them on a valve. The default Open and OpenLock effects would
// reset the goober on its next update and call Use twice, so the script replaces them with the boss's valve state.
class spell_lady_inerva_anima_release : public SpellScript
{
    SpellCastResult CheckValve()
    {
        GameObject* valve = GetExplTargetGObj();
        if (!valve || valve->HasFlag(GO_FLAG_LOCKED))
            return SPELL_FAILED_BAD_TARGETS;

        boss_lady_inerva_darkvein* lady = GetLadyAI(GetCaster());
        if (!lady || !lady->IsFightActive())
            return SPELL_FAILED_BAD_TARGETS;

        return SPELL_CAST_OK;
    }

    void PreventDefault(SpellEffIndex effIndex)
    {
        PreventHitDefaultEffect(effIndex);
    }

    void HandleClick(SpellEffIndex effIndex)
    {
        PreventHitDefaultEffect(effIndex);
        if (boss_lady_inerva_darkvein* lady = GetLadyAI(GetCaster()))
            lady->HandleValveClick(GetHitGObj(), GetSpellInfo()->Id == SPELL_ANIMA_RELEASE);
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(spell_lady_inerva_anima_release::CheckValve);
        OnEffectHitTarget += SpellEffectFn(spell_lady_inerva_anima_release::HandleClick, EFFECT_0, SPELL_EFFECT_ACTIVATE_OBJECT);
        OnEffectHitTarget += SpellEffectFn(spell_lady_inerva_anima_release::PreventDefault, EFFECT_1, SPELL_EFFECT_OPEN_LOCK);
    }
};

// 341621, 341623, 341625 - Expose Desires: the hit puts Warped Desires on the victim; ranks 2 and 3 spread Shared Cognition and every
// holder copies the damage
class spell_lady_inerva_expose_desires : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_WARPED_DESIRES, SPELL_SHARED_COGNITION, SPELL_SHARED_COGNITION_COPY });
    }

    void HandleHit(SpellEffIndex /*effIndex*/)
    {
        Unit* victim = GetHitUnit();
        if (!victim)
            return;

        uint8 rank = GetSpellInfo()->Id == SPELL_EXPOSE_DESIRES_3 ? 3 : GetSpellInfo()->Id == SPELL_EXPOSE_DESIRES_2 ? 2 : 1;
        GetCaster()->CastSpell(victim, SPELL_WARPED_DESIRES, true);
        if (boss_lady_inerva_darkvein* lady = GetLadyAI(GetCaster()))
            lady->ApplySharedCognition(victim, rank);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_lady_inerva_expose_desires::HandleHit, EFFECT_0, SPELL_EFFECT_SCHOOL_DAMAGE);
    }
};

// 325382 - Warped Desires: after a rank 3 Expose, running out brings Change of Heart (inferred from the rank 3 spell text)
class spell_lady_inerva_warped_desires : public AuraScript
{
    void HandleRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        if (GetTargetApplication()->GetRemoveMode() != AURA_REMOVE_BY_EXPIRE)
            return;

        if (boss_lady_inerva_darkvein* lady = GetLadyAI(GetTarget()))
            lady->HandleWarpedDesiresExpire(GetTarget());
    }

    void Register() override
    {
        AfterEffectRemove += AuraEffectRemoveFn(spell_lady_inerva_warped_desires::HandleRemove, EFFECT_0, SPELL_AURA_PERIODIC_DAMAGE, AURA_EFFECT_HANDLE_REAL);
    }
};

// 340452 - Change of Heart: four seconds after the mark the victim detonates
class spell_lady_inerva_change_of_heart : public AuraScript
{
    void HandleRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        if (GetTargetApplication()->GetRemoveMode() != AURA_REMOVE_BY_EXPIRE)
            return;

        if (Unit* caster = GetCaster())
            caster->CastSpell(GetTarget(), SPELL_CHANGE_OF_HEART, true);
    }

    void Register() override
    {
        AfterEffectRemove += AuraEffectRemoveFn(spell_lady_inerva_change_of_heart::HandleRemove, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }
};

// 325184 - Loose Anima: each tick is base x (1 + 1.75 x open valves)
class spell_lady_inerva_loose_anima : public AuraScript
{
    void HandlePeriodic(AuraEffect const* aurEff)
    {
        if (!_base)
            _base = aurEff->GetAmount();

        int32 open = 0;
        if (boss_lady_inerva_darkvein* lady = GetLadyAI(GetTarget()))
            open = lady->GetOpenValveCount();

        if (AuraEffect* effect = GetEffect(EFFECT_0))
            effect->SetAmount(int32(std::lround(float(_base) * (1.0f + LooseAnimaPerValve * float(open)))));
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_lady_inerva_loose_anima::HandlePeriodic, EFFECT_0, SPELL_AURA_PERIODIC_DAMAGE);
    }

private:
    int32 _base = 0;
};

// 340477, 332664 - Concentrated Anima debuff: rank 1 does not root (capture: root flag 0 for the four rank 1 casts, 1 for ranks 2 and
// 3); at expiry the explosion, the add and for rank 3 the fragments
class spell_lady_inerva_concentrated_debuff : public AuraScript
{
    void HandleRootApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        boss_lady_inerva_darkvein* lady = GetLadyAI(GetTarget());
        if (!lady || lady->GetConcentratedRank() < 2)
            PreventDefaultAction();
    }

    void HandleRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        if (GetTargetApplication()->GetRemoveMode() != AURA_REMOVE_BY_EXPIRE)
            return;

        if (boss_lady_inerva_darkvein* lady = GetLadyAI(GetTarget()))
            lady->HandleConcentratedExpire(GetTarget(), GetId() == SPELL_CONCENTRATED_DEBUFF_EXTRA);
    }

    void Register() override
    {
        OnEffectApply += AuraEffectApplyFn(spell_lady_inerva_concentrated_debuff::HandleRootApply, EFFECT_1, SPELL_AURA_MOD_ROOT, AURA_EFFECT_HANDLE_REAL);
        AfterEffectRemove += AuraEffectRemoveFn(spell_lady_inerva_concentrated_debuff::HandleRemove, EFFECT_0, SPELL_AURA_PERIODIC_DAMAGE, AURA_EFFECT_HANDLE_REAL);
    }
};

// 324983 - Shared Suffering link: effect 2 would create areatrigger 21630 (the beam), which has no create properties; the script
// tests the beams itself
class spell_lady_inerva_shared_suffering : public AuraScript
{
    void PreventAreaTrigger(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        PreventDefaultAction();
    }

    void Register() override
    {
        OnEffectApply += AuraEffectApplyFn(spell_lady_inerva_shared_suffering::PreventAreaTrigger, EFFECT_2, SPELL_AURA_AREA_TRIGGER, AURA_EFFECT_HANDLE_REAL);
    }
};

// 345134, 332562 - Indemnification cleanup: removes the Sins of the Past orbs in range (the conditions name them) 0.4 s later
class spell_lady_inerva_orb_cleanup : public SpellScript
{
    void HandleScript(SpellEffIndex /*effIndex*/)
    {
        if (Creature* orb = GetHitCreature())
            orb->DespawnOrUnsummon(400ms);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_lady_inerva_orb_cleanup::HandleScript, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT);
    }
};

// Lingering Anima pool (create properties 20081, sphere r4, 600 s): 325713 (25 damage per second, screen effect) while inside
struct at_lady_inerva_lingering_anima : AreaTriggerAI
{
    using AreaTriggerAI::AreaTriggerAI;

    void OnUnitEnter(Unit* unit) override
    {
        Unit* caster = at->GetCaster();
        if (caster && unit->IsPlayer() && unit->IsAlive())
            caster->CastSpell(unit, 325713, true);
    }

    void OnUnitExit(Unit* unit, AreaTriggerExitReason /*reason*/) override
    {
        // overlapping pools are not tracked: leaving any pool ends the aura until the next enter
        unit->RemoveAurasDueToSpell(325713);
    }
};

// Fragments of Shadow (create properties 20071, cylinder r1, travels 100 yd at 3 yd/s): 80 damage once per player touched
struct at_lady_inerva_fragment : AreaTriggerAI
{
    using AreaTriggerAI::AreaTriggerAI;

    // set up before the areatrigger enters the map so that the client receives the moving create
    void OnInitialize() override
    {
        Position const& pos = at->GetPosition();
        std::vector<G3D::Vector3> path;
        path.emplace_back(pos.GetPositionX(), pos.GetPositionY(), pos.GetPositionZ());
        path.emplace_back(pos.GetPositionX() + FragmentTravel * std::cos(pos.GetOrientation()),
            pos.GetPositionY() + FragmentTravel * std::sin(pos.GetOrientation()), pos.GetPositionZ());
        at->InitSplines(path);
    }

    void OnUnitEnter(Unit* unit) override
    {
        Unit* caster = at->GetCaster();
        if (!caster || !unit->IsPlayer() || !unit->IsAlive())
            return;

        if (std::ranges::find(_hit, unit->GetGUID()) != _hit.end())
            return;

        _hit.push_back(unit->GetGUID());
        caster->CastSpell(unit, SPELL_FRAGMENTS_OF_SHADOW, true);
    }

private:
    GuidVector _hit;
};

void AddSC_boss_lady_inerva_darkvein()
{
    RegisterCastleNathriaCreatureAI(boss_lady_inerva_darkvein);
    RegisterCreatureAI(npc_lady_inerva_container);
    RegisterCreatureAI(npc_lady_sins_of_the_past);
    RegisterCreatureAI(npc_lady_harnessed_specter);
    RegisterCreatureAI(npc_lady_conjured_manifestation);
    RegisterGameObjectAI(go_lady_inerva_valve);
    RegisterSpellScript(spell_lady_inerva_anima_release);
    RegisterSpellScript(spell_lady_inerva_expose_desires);
    RegisterSpellScript(spell_lady_inerva_warped_desires);
    RegisterSpellScript(spell_lady_inerva_change_of_heart);
    RegisterSpellScript(spell_lady_inerva_loose_anima);
    RegisterSpellScript(spell_lady_inerva_concentrated_debuff);
    RegisterSpellScript(spell_lady_inerva_shared_suffering);
    RegisterSpellScript(spell_lady_inerva_orb_cleanup);
    RegisterAreaTriggerAI(at_lady_inerva_lingering_anima);
    RegisterAreaTriggerAI(at_lady_inerva_fragment);
}
