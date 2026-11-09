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
 * The Council of Blood (DungeonEncounter 2412, Journal 2426, map 2296): Baroness Frieda 166969, Lord Stavros 166970, Castellan Niklaus
 * 166971, the Dance Controller 168870 and the adds. Spec: trinitycore-tracking/castle-nathria/bosses/council-of-blood.md and .json, the
 * per-mechanic rules in council-of-blood_followup.json (one solo Normal kill from the 2026-11-06 capture, Warcraft Logs, Journal, DBM,
 * BigWigs, the live world database). The followup overrides the spec.
 *
 * Health and death (settled, High): three separate pools. Lethal damage is clamped to 1 health and the member feigns with 29266, says its
 * death line and the survivors, in the same instant, lose their casts in flight, are healed to full and channel Anima Infusion 346706 at
 * the dead member for 3.0 s; at d+3.0 they gain Infused 332535 (first death) or Empowered 346709 (second death, +10% scale each). Timers
 * of the survivors are replaced by the column of the dead set and count from d. Oppressive Atmosphere is cleared at every death. The
 * feigned member regenerates on its own, is skipped by the shield and by the 50% check, and never casts. At the third feign: 29266 is
 * removed from all three, the member that fell last is killed with Unit::Kill by a player of its tap list (its loot) and the other two
 * with KillSelf; the arena scenery despawns; SetBossState(DONE) comes from BossAI::JustDied of the member killed first (the instance script
 * sets DONE again when none of the three is alive, harmless).
 * Loot-holder rule: the member that fell LAST is the lootable one (n = 1 in the capture, Frieda last; Warcraft Logs shows the last member
 * varies and no log shows loot ownership).
 *
 * Danse Macabre (High for the structure): the first living member at or below 50% starts it, once per phase; the flag resets at every death.
 * The leader (yells) is Niklaus, or Frieda when he is dead. Trigger+0.2 formation yell, +2.4 the members cast 330959 (-100% damage taken)
 * and jump to the balcony, +3.0 every player casts 328485 (sphere, create properties 20402) at its own position, +3.2 the raid emote,
 * +3.3 45 dancers (174878) appear, +11.77 players without the in-position marker 328500 get Wrong Moves 347219, then four windows
 * (+11.78, +15.88, +19.98, +24.09): the controller applies 331458 (4.0 s) and every player casts 344181 (box, create properties 21954) at
 * the previous box centre plus a world-axis vector of 11 yd (right (-11,0), left (+11,0), down (0,+11), forward (0,-11)); the leader yells
 * 0.2 s later. A player who enters the box (only its owner counts) loses the window and gets Fancy Footwork 346035 and the marker; a window
 * that runs out gives Wrong Moves, whose 1 s tick casts 330848 (stacks). +28.2 the markers and Wrong Moves go, the members jump back to
 * the formation, +30.0 330959 ends, +31.5 the fight goes on. The EventMap pauses for the whole window; a cast in flight at the trigger is
 * cancelled and recast 0.1 s after the resume. Oppressive Atmosphere is off from the trigger to +28.2.
 *
 * Abilities, with the evidence in the followup (q2_abilities): Dreadbolt Volley (one cast, then sets of three), Drain Essence (2 targets,
 * 3 on Mythic, tanks avoided), Prideful Eruption (marker on every player, missiles, 6 yd blasts), Soul Spikes (four spikes on the tank,
 * each radiates to the others), Duelist's Riposte, Dutiful Attendant (shield 346694 on the lowest living member, gone with the attendant),
 * Dredger Servants (five waiters throwing food), Castellan's Cadre (Veteran Stoneguard), Evasive Lunge (teleport behind the victim, a 20 yd
 * charge), Dark Recital (pairs, 8 yd leash, eruptions on the partner's spot). Entry-target effects need the condition rows of
 * boss_council_of_blood.sql (337110 effect 0, 331458, 334824). Spell effects of type DUMMY and auras of type PERIODIC_DUMMY do nothing in the
 * core, so the chains that retail drives that way (346657 -> 346661 -> 346658 -> 346660, 327503 -> 327610, 331636/331637 -> 334742,
 * 346762 -> 346681/346685, 347219 -> 330848) are in the spell scripts below.
 *
 * Core: nothing is added. Checked in SpellEffects.cpp / SpellAuraEffects.cpp: effects 28/32/140/144/164/179/203/252/254 have handlers,
 * auras 23, 40, 61, 79 and 193 are handled, 226 and 255 are handled (226 by scripts, 255 in Unit::SpellDamageBonus). jump_charge_params
 * 419 (Evasive Lunge) is missing from the live table and not in the DB2 listing of wowdata: the jump is done in script while the row
 * does not exist (checked at runtime).
 *
 * Not settled (each is left out or a marked estimate, Low unless said):
 * - Left out on purpose: Twisted Pain 346939 / Manifest Pain (Heroic and Mythic only, absent from the Normal capture), Two Left Feet
 *   346891, Castellan's Fury 346934, Dancing Fever and the Afterimages (Mythic only), Tactical Advance (unused), Berserk: DBM starts a
 *   720 s timer without a spell id or text and no log reaches it, so none is scripted (Oppressive Atmosphere is the soft enrage).
 * - Stavros (one or two dead): Waltz of Blood 346800 is cast once (first d+33.8 with one dead, 64.5 s active time with two dead) and its
 *   aura goes on the players, but the careening Waltzing Venthyr (AreaTrigger create properties 20309 is missing, the carrying entry is
 *   unknown) are not scripted: no stun and no damage. Dancing Fools 330964 / 346826 have no combat-log cast; only the Violent Uproar that
 *   follows at d+29.74 (not paused by the dance, Normal n = 8) is scripted, cast by the 41 pre-placed 176026 (15.0 s cast, 11 yd). Those sit
 *   at z 4323-4330, so a 3-dimensional 11 yd radius may never reach the floor (npc_council_waltz_vehicle seats their riders). No repeats are scripted
 *   for any of the three. The real (killable) Fool is unknown. Stavros' alone timers Recital 9.5 / 22.9 and Lunge 15.2 / 11.4 are Warcraft
 *   Logs Normal (n = 8 to 17), the Uproar +7 s on Heroic is unexplained.
 * - Who leads the dance when Niklaus is dead and Stavros alive (Frieda is used) and when Stavros is alone (no yell). Frieda's Boogie down and
 *   Prance Forward lines were never captured: she says nothing for those steps. The step is chosen at random from the four (12 steps, 3 dances).
 * - Prideful Eruption repeat with one member dead (>= 110 s, BigWigs 120, DBM 65); where the 6 yd blast lands (the player's spot when the
 *   missile arrives); Dreadbolt Volley interrupts (never seen); the Dark Recital pair count by group size (Normal 1, Heroic 2 below 16
 *   players else 3, Mythic 3) and its tank exclusion; Drain Essence's tank exclusion is a preference (random non-tank, tanks only fill).
 * - Dutiful Attendant spawn points are the five captured ones (one is chosen at random); Dredger Servants spawn at the five captured points,
 *   the fifth 0.5 s later (the rear door side on the first cast, a corner after); Castellan's Cadre summons the Stoneguard at a random
 *   non-tank and the Stoneguard's own 346788 is the knockback (Normal n = 24 for the timers, the Stoneguard's Stoneform 345968 is not cast
 *   because its end is unknown).
 * - Serve Drinks: the waiter 175474 appears every 60 s from 60 s; clicking it (Throw Wine 345304) has no script and no npc_spellclick_spells
 *   row, its purpose is unknown. Prince Renathal's line after the kill (a fresh 172652 spawns, line 6 s later) is not scripted, its spawn
 *   point is not in the evidence.
 * - Lord Stavros' live template carries the flags of his feigned state (see PENDING); until it is applied the script clears
 *   UNINTERACTIBLE and STUNNED at reset.
 */

#include "AreaTrigger.h"
#include "AreaTriggerAI.h"
#include "Containers.h"
#include "GameTime.h"
#include "InstanceScript.h"
#include "Map.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "Random.h"
#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "Spell.h"
#include "SpellAuraEffects.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "SpellScript.h"
#include "TemporarySummon.h"
#include "Vehicle.h"
#include "ThreatManager.h"
#include "castle_nathria.h"
#include <any>
#include <array>
#include <functional>
#include <list>
#include <map>

enum CouncilSpells
{
    // the members
    SPELL_PERMANENT_FEIGN_DEATH         = 29266,    // DUMMY + MOD_STUN; spell_script_names maps it to spell_gen_feign_death_all_flags_no_uninteractible
    SPELL_ANIMA_INFUSION                = 346706,   // channel, 3.0 s, from each survivor at the dead member
    SPELL_INFUSED                       = 332535,   // MOD_SCALE +10%
    SPELL_EMPOWERED                     = 346709,
    SPELL_DANCE_PROTECTION              = 330959,   // -100% damage taken during the dance
    SPELL_SOUL_SPIKES_LISTENER          = 346682,

    // Baroness Frieda
    SPELL_DREADBOLT_VOLLEY              = 337110,
    SPELL_DRAIN_ESSENCE                 = 346654,
    SPELL_DRAIN_ESSENCE_TARGETS         = 346653,   // only read for MaxAffectedTargets per difficulty
    SPELL_DRAIN_ESSENCE_DAMAGE          = 346651,
    SPELL_PRIDEFUL_ERUPTION_MARK        = 346656,
    SPELL_PRIDEFUL_ERUPTION             = 346657,
    SPELL_PRIDEFUL_ERUPTION_MISSILE     = 346661,
    SPELL_PRIDEFUL_ERUPTION_REMOVE      = 346658,
    SPELL_PRIDEFUL_ERUPTION_BLAST       = 346660,
    SPELL_SOUL_SPIKES                   = 346762,
    SPELL_SOUL_SPIKES_HIT               = 346681,
    SPELL_SOUL_SPIKES_WAVE              = 346685,

    // Castellan Niklaus
    SPELL_DUELISTS_RIPOSTE              = 346690,
    SPELL_SUMMON_DUTIFUL_ATTENDANT      = 346698,
    SPELL_SPAWN_COSMETICS               = 346697,
    SPELL_UNYIELDING_SHIELD             = 346694,
    SPELL_DREDGER_SERVANTS              = 330978,
    SPELL_CASTELLANS_CADRE              = 330965,
    SPELL_CADRE_KNOCKBACK               = 346788,
    SPELL_SINTOUCHED_BLADE              = 346790,
    SPELL_SINTOUCHED_BLADE_STACK        = 346792,
    SPELL_FIXATE                        = 330967,

    // Lord Stavros
    SPELL_EVASIVE_LUNGE                 = 327497,
    SPELL_EVASIVE_LUNGE_CHARGE          = 327503,
    SPELL_EVASIVE_LUNGE_HIT             = 327610,
    SPELL_DARK_RECITAL                  = 331634,
    SPELL_DARK_RECITAL_FIRST            = 331636,
    SPELL_DARK_RECITAL_SECOND           = 331637,
    SPELL_DARK_RECITAL_LEASH            = 331638,
    SPELL_DARK_RECITAL_ERUPTION         = 334824,
    SPELL_DARK_RECITAL_BLAST            = 334743,
    SPELL_WALTZ_OF_BLOOD                = 346800,
    SPELL_VIOLENT_UPROAR                = 346303,

    // adds
    SPELL_THROW_FOOD                    = 330968,

    // the controller and the dance
    SPELL_OPPRESSIVE_ATMOSPHERE         = 334952,   // PERIODIC_TRIGGER_SPELL 10 s -> 334909
    SPELL_OPPRESSIVE_ATMOSPHERE_STACK   = 334909,
    SPELL_SERVE_DRINKS                  = 345368,   // PERIODIC_TRIGGER_SPELL 60 s -> 345370 (SEND_EVENT)
    SPELL_DANCE_SPHERE                  = 328485,   // AreaTrigger create properties 20402, sphere r3, 8 s
    SPELL_DANCE_BOX                     = 344181,   // AreaTrigger create properties 21954, box 4x4x5, 4 s
    SPELL_TAKEN_POSITION                = 328495,   // grants 328500 (FORCE_CAST)
    SPELL_DANCE_MARKER                  = 328500,
    SPELL_DANCE_WINDOW                  = 331458,
    SPELL_FANCY_FOOTWORK                = 346035,
    SPELL_WRONG_MOVES_DRIVER            = 347219,
    SPELL_WRONG_MOVES                   = 330848
};

enum CouncilCreatures
{
    NPC_DUTIFUL_ATTENDANT               = 175992,
    NPC_BEGRUDGING_WAITER               = 169925,
    NPC_DANCER                          = 174878,
    NPC_SERVING_WAITER                  = 175474,
    NPC_VETERAN_STONEGUARD              = 169924,
    NPC_DANCING_FOOL                    = 176026,
    NPC_FOOL_CARRIER                    = 176017,
    NPC_FOOL_RIDER                      = 169727,
    NPC_STONEWROUGHT_GUARDIAN           = 176015,
    NPC_FEASTING_VENTHYR                = 171145,
    NPC_FEASTING_VENTHYR_SMALL          = 171146,
    NPC_DANCE_CONTROLLER                = 168870
};

enum CouncilTexts
{
    // Baroness Frieda
    SAY_FRIEDA_AGGRO                    = 0,
    SAY_FRIEDA_FORMATION                = 1,
    SAY_FRIEDA_DANCE_END                = 4,
    SAY_FRIEDA_DEATH                    = 5,
    SAY_FRIEDA_STEP_RIGHT               = 6,
    SAY_FRIEDA_STEP_LEFT                = 7,

    // Lord Stavros
    SAY_STAVROS_PARTNERS                = 0,
    SAY_STAVROS_DEATH                   = 1,

    // Castellan Niklaus
    SAY_NIKLAUS_FORMATION               = 0,
    SAY_NIKLAUS_DANCE_END               = 4,
    SAY_NIKLAUS_DEATH                   = 6,
    SAY_NIKLAUS_STEP_RIGHT              = 7,
    SAY_NIKLAUS_STEP_LEFT               = 8,
    SAY_NIKLAUS_STEP_DOWN               = 9,
    SAY_NIKLAUS_STEP_FORWARD            = 10,

    // Dance Controller (RaidBossEmote)
    SAY_CONTROLLER_PLACES               = 0
};

enum CouncilEvents
{
    // Frieda
    EVENT_DREADBOLT = 1,
    EVENT_DRAIN_ESSENCE,
    EVENT_PRIDEFUL_ERUPTION,
    EVENT_SOUL_SPIKES,

    // Niklaus
    EVENT_RIPOSTE,
    EVENT_ATTENDANT,
    EVENT_DREDGER_SERVANTS,
    EVENT_CADRE,

    // Stavros
    EVENT_LUNGE,
    EVENT_RECITAL,
    EVENT_WALTZ,

    // the controller
    EVENT_DANCE_FORMATION_YELL,
    EVENT_DANCE_CLEAR_STACKS,
    EVENT_DANCE_HIDE,
    EVENT_DANCE_SPHERES,
    EVENT_DANCE_EMOTE,
    EVENT_DANCE_DANCERS,
    EVENT_DANCE_WRONG_START,
    EVENT_DANCE_STEP,
    EVENT_DANCE_STEP_YELL,
    EVENT_DANCE_DANCER_MOVE,
    EVENT_DANCE_OFF,
    EVENT_DANCE_ATMOSPHERE_BACK,
    EVENT_DANCE_DANCERS_LEAVE,
    EVENT_DANCE_RELEASE,
    EVENT_DANCE_RESUME,

    // adds
    EVENT_ATTENDANT_CHOOSE,
    EVENT_WAITER_AGGRO,
    EVENT_THROW_FOOD,
    EVENT_FIXATE,
    EVENT_SINTOUCHED_BLADE
};

enum CouncilData
{
    DATA_DANCE_AVAILABLE                = 1,    // GetData on the controller: 1 when this phase's dance has not been used
    DATA_DANCE_ACTIVE                   = 2,    // GetData on the controller: 1 from the trigger to the resume
    DATA_DANCE_REQUEST                  = 3,    // SetData on the controller: value = the member that crossed 50%
    DATA_FIGHT_RESET                    = 4,
    DATA_ENCOUNTER_START                = 5,
    DATA_MEMBER_DIED                    = 6,    // value = number of feigned members
    DATA_FIGHT_END                      = 7,

    ACTION_DANCE_START                  = 1,    // members: run to the formation, stop fighting, pause
    ACTION_DANCE_HIDE                   = 2,    // members: 330959 and the jump to the balcony
    ACTION_DANCE_RETURN                 = 3,    // members: jump back to the formation
    ACTION_DANCE_RELEASE                = 4,    // members: 330959 off
    ACTION_DANCE_RESUME                 = 5,    // members: fight again
    ACTION_SERVE_DRINKS                 = 6     // controller: the 60 s tick
};

enum CouncilPoints
{
    POINT_PULL                          = 1,
    POINT_FORMATION                     = 2,
    POINT_BALCONY                       = 3,
    POINT_JUMP                          = 4,
    POINT_DANCER                        = 5,
    POINT_SHIELD                        = 6
};

enum CouncilMember : uint8
{
    MEMBER_FRIEDA                       = 0,
    MEMBER_STAVROS                      = 1,
    MEMBER_NIKLAUS                      = 2,
    MEMBER_COUNT                        = 3
};

static constexpr uint32 CouncilMemberData[MEMBER_COUNT] = { DATA_BARONESS_FRIEDA, DATA_LORD_STAVROS, DATA_CASTELLAN_NIKLAUS };
static constexpr uint8 DeadFrieda = 1;
static constexpr uint8 DeadStavros = 2;
static constexpr uint8 DeadNiklaus = 4;

// the first value counts from the encounter start or from the death of a member (d), the second is the repeat; Warcraft Logs Normal
// medians checked against the capture (council-of-blood.md, timer table keyed by the dead set)
struct CouncilTimer
{
    Milliseconds First;
    Milliseconds Repeat;
};

static constexpr Milliseconds DreadboltGapSet = 5350ms;         // spacing inside a set of three
static constexpr Milliseconds DreadboltGapBetweenSets = 16070ms;
static constexpr Milliseconds DreadboltGapFirst = 21800ms;      // from the lone first cast to the first set

// Stavros alone, first casts only
static constexpr Milliseconds WaltzFirstOneDead = 33800ms;
static constexpr Milliseconds WaltzFirstTwoDead = 64500ms;      // active time, 96.2 s with the dance pause
static constexpr Milliseconds UproarAfterSecondDeath = 29740ms; // not paused by the dance (n = 1)

// the pull and the dance
static constexpr Milliseconds PullJumpTime = 1000ms;
static constexpr Milliseconds DanceHideJumpTime = 2000ms;
static constexpr Milliseconds DanceReturnJumpTime = 1000ms;
static constexpr Milliseconds RecastAfterDance = 100ms;
static constexpr Milliseconds InfusionTime = 3000ms;
static constexpr float DanceStepLength = 11.0f;
static constexpr Milliseconds DanceStepInterval = 4100ms;       // 15.88 - 11.78
static constexpr uint32 DanceStepCount = 4;
static constexpr float DancerStepSpeed = 4.49f;                 // 11 yd in 2.45 s
static constexpr Milliseconds DancerRunTime = 6500ms;
static constexpr float StartDancePct = 50.0f;

// formation line (y 6347) and balcony points (z 4307.8) of the capture, in the order Frieda, Stavros, Niklaus
static Position const CouncilFormation[MEMBER_COUNT] =
{
    { -2093.87f, 6346.62f, 4298.08f, 1.5708f },
    { -2075.84f, 6347.67f, 4298.08f, 1.5708f },
    { -2111.32f, 6347.67f, 4298.08f, 1.5708f }
};
static Position const CouncilBalcony[MEMBER_COUNT] =
{
    { -2093.47f, 6314.36f, 4307.80f, 1.5708f },
    { -2071.97f, 6319.10f, 4307.80f, 1.5708f },
    { -2114.37f, 6318.67f, 4307.80f, 1.5708f }
};

static Position const DancerCorners[4] =
{
    { -2165.2f, 6389.2f, 4304.4f }, { -2022.1f, 6389.1f, 4304.4f }, { -2165.3f, 6417.1f, 4304.4f }, { -2021.9f, 6416.6f, 4304.4f }
};
static Position const DancerExit = { -2154.1f, 6402.7f, 4300.0f };

// the 45 grid slots of the capture (identical in all three dances), floor z 4298.3
static constexpr float DanceFloorZ = 4298.3f;
static constexpr float DancerGrid[][2] =
{
    { -2110.1f, 6372.8f }, { -2109.8f, 6427.6f }, { -2104.4f, 6357.1f },
    { -2104.4f, 6367.5f }, { -2104.4f, 6378.4f }, { -2104.3f, 6389.3f },
    { -2104.3f, 6422.3f }, { -2104.3f, 6433.2f }, { -2104.2f, 6411.3f },
    { -2104.2f, 6444.0f }, { -2099.0f, 6405.6f }, { -2098.9f, 6362.4f },
    { -2098.9f, 6383.9f }, { -2098.9f, 6394.8f }, { -2098.9f, 6416.8f },
    { -2098.9f, 6427.5f }, { -2098.9f, 6449.7f }, { -2098.7f, 6438.8f },
    { -2093.6f, 6367.5f }, { -2093.6f, 6389.4f }, { -2093.6f, 6444.3f },
    { -2093.5f, 6378.2f }, { -2093.5f, 6400.3f }, { -2093.5f, 6422.1f },
    { -2093.4f, 6411.3f }, { -2093.4f, 6433.2f }, { -2088.1f, 6362.4f },
    { -2088.1f, 6372.9f }, { -2088.1f, 6383.9f }, { -2088.1f, 6394.8f },
    { -2088.1f, 6405.8f }, { -2088.1f, 6416.6f }, { -2087.9f, 6438.4f },
    { -2087.9f, 6449.8f }, { -2087.8f, 6427.6f }, { -2082.9f, 6357.2f },
    { -2082.7f, 6367.5f }, { -2082.7f, 6378.4f }, { -2082.5f, 6389.7f },
    { -2082.5f, 6411.4f }, { -2082.5f, 6422.3f }, { -2082.5f, 6433.2f },
    { -2082.5f, 6444.3f }, { -2077.0f, 6373.0f }, { -2076.9f, 6427.7f }
};

// Serve Drinks: the three corners where the clickable waiter appeared
static Position const ServingWaiterPoints[] =
{
    { -2163.7f, 6414.7f, 4303.8f }, { -2163.7f, 6391.7f, 4303.3f }, { -2022.2f, 6391.2f, 4303.6f }
};

// Dutiful Attendant: the five distinct points of the eight captured spawns, floor z
static Position const AttendantPoints[] =
{
    { -2105.2f, 6400.4f, 4298.3f }, { -2093.5f, 6346.3f, 4298.3f }, { -2109.9f, 6351.2f, 4298.3f }, { -2066.2f, 6362.1f, 4298.3f },
    { -2120.5f, 6384.3f, 4298.3f }
};
static constexpr Milliseconds AttendantChooseDelay = 1500ms;    // 346693 comes 1.5 s after the spawn

// Dredger Servants: four at the captured corner points, the fifth 0.5 s later
static Position const WaiterPoints[] =
{
    { -2165.9f, 6415.4f, 4304.1f }, { -2165.4f, 6390.4f, 4304.0f }, { -2021.7f, 6416.6f, 4304.4f }, { -2021.7f, 6392.0f, 4303.1f }
};
static Position const WaiterRearDoor = { -2043.4f, 6486.8f, 4300.1f };
static constexpr Milliseconds WaiterFifthDelay = 500ms;
static constexpr Milliseconds WaiterAggroDelay = 2500ms;
static constexpr Milliseconds ThrowFoodInterval = 2400ms;
static constexpr float ThrowFoodRange = 40.0f;

// Evasive Lunge: the charge follows the teleport by 0.26 s; the jump of the missing jump_charge_params 419 is 20 yd in 0.5 s
static constexpr Milliseconds LungeChargeDelay = 260ms;
static constexpr int32 LungeJumpParamsId = 419;
static constexpr float LungeJumpLength = 20.0f;
static constexpr Milliseconds LungeJumpTime = 500ms;

// Castellan's Cadre and the Stoneguard (Normal, Warcraft Logs n = 15 and 6)
static constexpr Milliseconds StoneguardBladeFirst = 7000ms;
static constexpr Milliseconds StoneguardBladeRepeat = 7300ms;
static constexpr Milliseconds StoneguardFixate = 12s;

// Soul Spikes: 346681 follows each tick of the aura by 0.16 s; 346685 0.02 s after that
static constexpr Milliseconds SoulSpikeDelay = 160ms;

// Dark Recital: the leash of the pair, the eruption radius is in the spell
static constexpr float RecitalLeash = 8.0f;
static constexpr uint32 HeroicRecitalSmallGroup = 16;

// triggered flags that keep the cast time (a boss cast without range, line of sight and cooldown checks)
static constexpr TriggerCastFlags CouncilTimedCast = TriggerCastFlags(TRIGGERED_IGNORE_GCD | TRIGGERED_IGNORE_SPELL_AND_CATEGORY_CD | TRIGGERED_IGNORE_POWER_COST);

// the auras of the fight that a wipe or the kill removes from the players
static constexpr uint32 CouncilPlayerAuras[] =
{
    SPELL_OPPRESSIVE_ATMOSPHERE_STACK, SPELL_DREADBOLT_VOLLEY, SPELL_DRAIN_ESSENCE_DAMAGE, SPELL_PRIDEFUL_ERUPTION_MARK, SPELL_SOUL_SPIKES,
    SPELL_SOUL_SPIKES_HIT, SPELL_DUELISTS_RIPOSTE, SPELL_EVASIVE_LUNGE_HIT, SPELL_DARK_RECITAL_FIRST, SPELL_DARK_RECITAL_SECOND,
    SPELL_DANCE_WINDOW, SPELL_TAKEN_POSITION, SPELL_DANCE_MARKER, SPELL_FANCY_FOOTWORK, SPELL_WRONG_MOVES_DRIVER, SPELL_WRONG_MOVES,
    SPELL_FIXATE
};

class CouncilMemberAI;

// Players the script may pick: living, no game masters; the tanks (the victim of a council member or a Tank specialization) only fill
// the list when fewer non-tanks exist (retail avoided the tanks in Warcraft Logs for Drain Essence and Dark Recital; the solo capture
// picked its only player).
static std::vector<Player*> CouncilPickPlayers(Creature* reference, size_t count)
{
    GuidSet tanks;
    if (InstanceScript* instance = reference->GetInstanceScript())
        for (uint32 data : CouncilMemberData)
            if (Creature* member = instance->GetCreature(data))
                if (Unit* victim = member->GetVictim())
                    tanks.insert(victim->GetGUID());

    std::vector<Player*> others;
    std::vector<Player*> tankPlayers;
    for (MapReference const& ref : reference->GetMap()->GetPlayers())
    {
        Player* player = ref.GetSource();
        if (!player || !player->IsAlive() || player->IsGameMaster())
            continue;

        ChrSpecializationEntry const* spec = player->GetPrimarySpecializationEntry();
        if (tanks.contains(player->GetGUID()) || (spec && spec->GetRole() == ChrSpecializationRole::Tank))
            tankPlayers.push_back(player);
        else
            others.push_back(player);
    }

    Trinity::Containers::RandomShuffle(others);
    Trinity::Containers::RandomShuffle(tankPlayers);

    std::vector<Player*> result;
    for (std::vector<Player*>* group : { &others, &tankPlayers })
        for (Player* player : *group)
            if (result.size() < count)
                result.push_back(player);
    return result;
}

static std::vector<Player*> CouncilLivingPlayers(Creature* reference)
{
    std::vector<Player*> result;
    for (MapReference const& ref : reference->GetMap()->GetPlayers())
    {
        Player* player = ref.GetSource();
        if (player && player->IsAlive() && !player->IsGameMaster())
            result.push_back(player);
    }
    return result;
}

// ---------------------------------------------------------------------------------------------------------------------------------
// The three members share one base: every member is a BossAI of DATA_COUNCIL_OF_BLOOD, so each one's death can end the encounter and
// the member killed last holds the loot. The once-per-phase dance flag, the dance itself and Oppressive Atmosphere live in the
// Dance Controller (npc_council_dance_controller); the members talk to it with SetData / GetData / DoAction.
// ---------------------------------------------------------------------------------------------------------------------------------
class CouncilMemberAI : public BossAI
{
public:
    CouncilMemberAI(Creature* creature, uint8 member) : BossAI(creature, DATA_COUNCIL_OF_BLOOD), _member(member) { }

    uint8 GetMember() const { return _member; }
    Creature* GetCouncilCreature() const { return me; }
    bool IsFeigned() const { return _feigned || _feigning; }

    void Reset() override
    {
        _Reset();
        ResetFight();
    }

    void JustEngagedWith(Unit* who) override
    {
        BossAI::JustEngagedWith(who);
        if (!instance || instance->GetBossState(DATA_COUNCIL_OF_BLOOD) != IN_PROGRESS)
            return;

        // the three fight from the first pull
        for (uint8 i = 0; i < MEMBER_COUNT; ++i)
        {
            CouncilMemberAI* peer = GetMemberAI(i);
            if (peer && peer != this && peer->me->IsAlive() && !peer->me->IsEngaged())
                CreatureAI::DoZoneInCombat(peer->me);
        }
        DoZoneInCombat();

        if (Creature* controller = GetController())
            controller->AI()->SetData(DATA_ENCOUNTER_START, 0);

        StartPull();
    }

    void UpdateAI(uint32 diff) override
    {
        scheduler.Update(diff);

        // a feigned member stays out of the fight; during the dance the timers are held (the EventMap is not updated)
        if (_feigned || _feigning)
            return;

        if (_danceHidden)
        {
            // a wipe inside the dance still ends the encounter
            UpdateVictim();
            return;
        }

        BossAI::UpdateAI(diff);
    }

    // Lethal damage is clamped to 1 health and the member feigns (retail never lets a member reach 0 until the third). The 50% check
    // starts the dance through the controller.
    void DamageTaken(Unit* /*attacker*/, uint32& damage, DamageEffectType /*damageType*/, SpellInfo const* /*spellInfo*/) override
    {
        if (_feigned || _feigning)
        {
            damage = 0;
            return;
        }

        uint32 const health = me->GetHealth();
        if (damage >= health)
        {
            damage = health ? health - 1 : 0;
            if (_danceHidden)
                _pendingFeign = true;
            else
                QueueFeign();
            return;
        }

        if (int64(health) - int64(damage) <= int64(me->CountPctFromMaxHealth(StartDancePct)))
            RequestDance();
    }

    void JustDied(Unit* killer) override
    {
        BossAI::JustDied(killer);
        RemovePlayerAuras();
    }

    // A wipe resets all three together; a feigned member ignores it while one of the others still fights.
    void EnterEvadeMode(EvadeReason why) override
    {
        if (_evading)
            return;

        if (_feigned && AnyoneFighting())
            return;

        for (uint8 i = 0; i < MEMBER_COUNT; ++i)
            if (CouncilMemberAI* member = GetMemberAI(i))
                if (member->me->IsAlive())
                    member->EvadeSelf(why);

        if (Creature* controller = GetController())
            controller->AI()->SetData(DATA_FIGHT_RESET, 0);
    }

    void DoAction(int32 action) override
    {
        switch (action)
        {
            case ACTION_DANCE_START:
                DanceStart();
                break;
            case ACTION_DANCE_HIDE:
                if (!_feigned)
                {
                    DoCastSelf(SPELL_DANCE_PROTECTION, true);
                    // a jump only deactivates an earlier MovePoint, which would resume after the landing
                    me->GetMotionMaster()->Clear();
                    me->GetMotionMaster()->MoveJump(POINT_BALCONY, CouncilBalcony[_member], DanceHideJumpTime);
                }
                break;
            case ACTION_DANCE_RETURN:
                if (!_feigned)
                {
                    me->GetMotionMaster()->Clear();
                    me->GetMotionMaster()->MoveJump(POINT_FORMATION, CouncilFormation[_member], DanceReturnJumpTime);
                }
                break;
            case ACTION_DANCE_RELEASE:
                me->RemoveAurasDueToSpell(SPELL_DANCE_PROTECTION);
                break;
            case ACTION_DANCE_RESUME:
                DanceResume();
                break;
            default:
                break;
        }
    }

    // A member crossed the line while the others still fight: its timers are replaced (called from the feigning member)
    void OnMemberFeigned(CouncilMemberAI* dead, uint8 feignedCount)
    {
        if (_feigned || _feigning)
            return;

        events.Reset();
        _recastEvent = 0;
        me->InterruptNonMeleeSpells(false);
        me->SetFullHealth();
        if (instance)
            instance->DoRemoveAurasDueToSpellOnPlayers(SPELL_PRIDEFUL_ERUPTION_MARK);

        // the channel is a visual: the buff comes from the clock, the channel breaks when the survivor is moved
        me->CastSpell(dead->me, SPELL_ANIMA_INFUSION, CastSpellExtraArgs(TRIGGERED_FULL_MASK));
        uint32 const buff = feignedCount <= 1 ? SPELL_INFUSED : SPELL_EMPOWERED;
        scheduler.Schedule(InfusionTime, [this, buff](TaskContext)
        {
            if (!_feigned && !_feigning && me->IsAlive())
                DoCastSelf(buff, true);
        });

        SchedulePhase();
    }

    virtual void ScheduleAbilities(uint8 deadMask) = 0;
    virtual void ExecuteAbility(uint32 eventId) = 0;
    virtual uint32 EventForSpell(uint32 /*spellId*/) const { return 0; }
    virtual void OnDanceStarted() { }
    virtual void OnFightReset() { }

    void ExecuteEvent(uint32 eventId) override
    {
        ExecuteAbility(eventId);
    }

protected:
    CouncilMemberAI* GetMemberAI(uint8 member) const
    {
        if (!instance)
            return nullptr;

        Creature* creature = instance->GetCreature(CouncilMemberData[member]);
        if (!creature || !creature->IsAIEnabled())
            return nullptr;

        return dynamic_cast<CouncilMemberAI*>(creature->AI());
    }

    Creature* GetController()
    {
        Creature* controller = ObjectAccessor::GetCreature(*me, _controllerGuid);
        if (!controller)
        {
            controller = me->FindNearestCreature(NPC_DANCE_CONTROLLER, 500.0f);
            if (controller)
                _controllerGuid = controller->GetGUID();
        }
        return controller && controller->IsAIEnabled() ? controller : nullptr;
    }

    // bit 1 Frieda, bit 2 Stavros, bit 4 Niklaus
    uint8 DeadMask() const
    {
        uint8 mask = 0;
        for (uint8 i = 0; i < MEMBER_COUNT; ++i)
            if (CouncilMemberAI* member = GetMemberAI(i))
                if (member->IsFeigned())
                    mask |= uint8(1 << i);
        return mask;
    }

    bool AnyoneFighting() const
    {
        for (uint8 i = 0; i < MEMBER_COUNT; ++i)
            if (CouncilMemberAI* member = GetMemberAI(i))
                if (member != this && !member->IsFeigned() && member->me->IsAlive() && member->me->IsEngaged())
                    return true;
        return false;
    }

    void EvadeSelf(EvadeReason why)
    {
        if (_evading)
            return;

        _evading = true;
        ResetFight();
        BossAI::EnterEvadeMode(why);
        // the spawn point is on the balcony, a walk home from the floor would not find a path
        if (me->IsAlive())
        {
            if (me->GetExactDist(me->GetHomePosition()) > 5.0f)
                me->NearTeleportTo(me->GetHomePosition());
            me->SetFullHealth();
        }
        _evading = false;
    }

    void SchedulePhase()
    {
        events.Reset();
        ScheduleAbilities(DeadMask());
    }

    void StartPull()
    {
        SchedulePhase();
        if (_member == MEMBER_FRIEDA)
        {
            DoCastSelf(SPELL_SOUL_SPIKES_LISTENER, true);
            scheduler.Schedule(200ms, [this](TaskContext) { Talk(SAY_FRIEDA_AGGRO); });
        }

        // all three jump from the balcony to the formation line (parabolic, about 1 s) and walk into melee
        me->AttackStop();
        me->SetReactState(REACT_PASSIVE);
        me->GetMotionMaster()->Clear();
        me->GetMotionMaster()->MoveJump(POINT_PULL, CouncilFormation[_member], PullJumpTime);
        scheduler.Schedule(PullJumpTime + 100ms, [this](TaskContext) { BackToFight(); });
    }

    void BackToFight()
    {
        if (_feigned || _feigning || !me->IsAlive())
            return;

        me->SetReactState(REACT_AGGRESSIVE);
        if (Unit* victim = me->SelectVictim())
            AttackStart(victim);
    }

    void ResetFight()
    {
        scheduler.CancelAll();
        _feigned = false;
        _feigning = false;
        _pendingFeign = false;
        _danceHidden = false;
        _recastEvent = 0;
        _dreadboltCasts = 0;
        events.Reset();

        if (!me->IsAlive())
            return;

        for (uint32 spellId : { SPELL_PERMANENT_FEIGN_DEATH, SPELL_DANCE_PROTECTION, SPELL_INFUSED, SPELL_EMPOWERED, SPELL_SOUL_SPIKES_LISTENER })
            me->RemoveAurasDueToSpell(spellId);

        // the live template of Lord Stavros carries the flags of his feigned state until the PENDING file is applied
        me->RemoveUnitFlag(UnitFlags(UNIT_FLAG_UNINTERACTIBLE | UNIT_FLAG_STUNNED));
        me->SetReactState(REACT_AGGRESSIVE);
        me->SetCanMelee(true);

        if (!me->IsInCombat())
            me->SetFullHealth();

        RemovePlayerAuras();
        OnFightReset();
    }

    void RemovePlayerAuras()
    {
        if (!instance)
            return;

        for (uint32 spellId : CouncilPlayerAuras)
            instance->DoRemoveAurasDueToSpellOnPlayers(spellId);
    }

    void Later(Milliseconds delay, std::function<void()> fn)
    {
        scheduler.Schedule(delay, [fn = std::move(fn)](TaskContext) { fn(); });
    }

    // the 50% line: one dance per phase, asked of the controller (it holds the flag)
    void RequestDance()
    {
        Creature* controller = GetController();
        if (!controller)
            return;

        if (controller->AI()->GetData(DATA_DANCE_AVAILABLE))
            controller->AI()->SetData(DATA_DANCE_REQUEST, _member);
    }

    // The cast in flight is cancelled; the timer of that ability is recast 0.1 s after the resume (capture: Drain Essence started
    // 0.3 s before the trigger and was recast at the resume).
    void DanceStart()
    {
        if (_feigned || _feigning)
            return;

        _danceHidden = true;
        _recastEvent = 0;
        if (Spell const* spell = me->GetCurrentSpell(CURRENT_GENERIC_SPELL))
            _recastEvent = EventForSpell(spell->GetSpellInfo()->Id);

        me->InterruptNonMeleeSpells(false);
        if (_recastEvent)
            events.RescheduleEvent(_recastEvent, RecastAfterDance);
        _recastEvent = 0;

        me->AttackStop();
        me->SetReactState(REACT_PASSIVE);
        me->GetMotionMaster()->Clear();
        me->GetMotionMaster()->MovePoint(POINT_FORMATION, CouncilFormation[_member], false);
        OnDanceStarted();
    }

    void DanceResume()
    {
        _danceHidden = false;
        if (_feigned || _feigning)
            return;

        me->SetReactState(REACT_AGGRESSIVE);
        if (Unit* victim = me->SelectVictim())
            AttackStart(victim);

        if (_pendingFeign)
        {
            _pendingFeign = false;
            QueueFeign();
        }
    }

    // The feign runs one tick after the damage (not inside DamageTaken): events reset, casts cancelled, 29266, the death line, then the
    // survivors are infused. The third feign ends the encounter.
    void QueueFeign()
    {
        if (_feigning || _feigned)
            return;

        _feigning = true;
        scheduler.Schedule(0ms, [this](TaskContext) { FeignDeath(); });
    }

    virtual void SayDeath() = 0;

    void FeignDeath()
    {
        _feigned = true;
        _feigning = false;
        events.Reset();
        me->InterruptNonMeleeSpells(false);
        DoCastSelf(SPELL_PERMANENT_FEIGN_DEATH, true);
        SayDeath();

        uint8 feignedCount = 0;
        for (uint8 i = 0; i < MEMBER_COUNT; ++i)
            if (CouncilMemberAI* member = GetMemberAI(i))
                if (member->IsFeigned())
                    ++feignedCount;

        if (Creature* controller = GetController())
            controller->AI()->SetData(DATA_MEMBER_DIED, feignedCount);

        if (feignedCount >= MEMBER_COUNT)
        {
            Later(20ms, [this]() { Finale(); });
            return;
        }

        for (uint8 i = 0; i < MEMBER_COUNT; ++i)
            if (CouncilMemberAI* member = GetMemberAI(i))
                if (member != this)
                    member->OnMemberFeigned(this, feignedCount);
    }

    // Third feign: the feigns end, the member that fell last is killed by a player of its tap list (its loot), the others die with
    // KillSelf, the arena is cleared. The kill must come before the lock: BossAI::JustDied (SetBossState DONE) writes the encounter
    // into every player's lock and Unit::Kill skips locked tappers when it rolls personal loot (same order as the Stone Legion Generals).
    void Finale()
    {
        if (Creature* controller = GetController())
            controller->AI()->SetData(DATA_FIGHT_END, 0);

        Creature* creatures[MEMBER_COUNT] = { };
        for (uint8 i = 0; i < MEMBER_COUNT; ++i)
        {
            creatures[i] = instance ? instance->GetCreature(CouncilMemberData[i]) : nullptr;
            if (creatures[i])
            {
                creatures[i]->RemoveAurasDueToSpell(SPELL_PERMANENT_FEIGN_DEATH);
                creatures[i]->RemoveAurasDueToSpell(SPELL_INFUSED);
                creatures[i]->RemoveAurasDueToSpell(SPELL_EMPOWERED);
                creatures[i]->RemoveAurasDueToSpell(SPELL_SOUL_SPIKES_LISTENER);
            }
        }

        // this member is the one that fell last
        Creature* last = me;
        Player* tapper = nullptr;
        for (Creature* source : { last, creatures[MEMBER_FRIEDA], creatures[MEMBER_STAVROS], creatures[MEMBER_NIKLAUS] })
        {
            if (!source || tapper)
                continue;

            for (ObjectGuid const& guid : source->GetTapList())
                if (Player* player = ObjectAccessor::GetPlayer(*source, guid))
                {
                    tapper = player;
                    if (player->IsAlive())
                        break;
                }
        }

        if (tapper)
            Unit::Kill(tapper, last);
        else
            last->KillSelf();

        for (Creature* other : creatures)
            if (other && other != last && other->IsAlive())
                other->KillSelf();

        RemovePlayerAuras();
        ClearArena();
    }

    // retail despawns the audience, their carriers and riders, the guardians, the feasting guests and the clickable waiter at the end
    void ClearArena()
    {
        for (uint32 entry : { NPC_DANCING_FOOL, NPC_FOOL_CARRIER, NPC_FOOL_RIDER, NPC_STONEWROUGHT_GUARDIAN, NPC_FEASTING_VENTHYR,
            NPC_FEASTING_VENTHYR_SMALL, NPC_SERVING_WAITER, NPC_DANCER })
        {
            std::list<Creature*> creatures;
            me->GetCreatureListWithEntryInGrid(creatures, entry, 500.0f);
            for (Creature* creature : creatures)
                creature->DespawnOrUnsummon(0ms, 7 * DAY * 1s);
        }
    }

    uint8 const _member;
    bool _feigned = false;
    bool _feigning = false;
    bool _pendingFeign = false;
    bool _danceHidden = false;
    bool _evading = false;
    uint32 _recastEvent = 0;
    uint32 _dreadboltCasts = 0;
    ObjectGuid _controllerGuid;
};

// 166969 - Baroness Frieda
struct boss_baroness_frieda : public CouncilMemberAI
{
    boss_baroness_frieda(Creature* creature) : CouncilMemberAI(creature, MEMBER_FRIEDA) { }

    // Normal medians, keyed by the dead set (council-of-blood.md); with Stavros alone dead nothing changes for Frieda's Drain
    void ScheduleAbilities(uint8 deadMask) override
    {
        _dreadboltCasts = 0;
        _drainRepeat = 25700ms;
        _eruptionRepeat = 110s;

        switch (deadMask)
        {
            case 0:
                events.ScheduleEvent(EVENT_DREADBOLT, 5800ms);
                events.ScheduleEvent(EVENT_DRAIN_ESSENCE, 15800ms);
                break;
            case DeadStavros:
                events.ScheduleEvent(EVENT_DREADBOLT, 4500ms);
                events.ScheduleEvent(EVENT_DRAIN_ESSENCE, 9500ms);
                events.ScheduleEvent(EVENT_PRIDEFUL_ERUPTION, 38s);
                _drainRepeat = 20s;
                break;
            case DeadNiklaus:
                events.ScheduleEvent(EVENT_DREADBOLT, 20500ms);
                events.ScheduleEvent(EVENT_DRAIN_ESSENCE, 9500ms);
                events.ScheduleEvent(EVENT_PRIDEFUL_ERUPTION, 38s);
                _drainRepeat = 20s;
                break;
            default:
                // Frieda alone (Empowered): Soul Spikes joins; Twisted Pain is Heroic and Mythic only and left out
                events.ScheduleEvent(EVENT_DREADBOLT, 4500ms);
                events.ScheduleEvent(EVENT_DRAIN_ESSENCE, 9500ms);
                events.ScheduleEvent(EVENT_PRIDEFUL_ERUPTION, 23800ms);
                events.ScheduleEvent(EVENT_SOUL_SPIKES, 35s);
                _drainRepeat = 41500ms;
                _eruptionRepeat = 41400ms;
                break;
        }
    }

    uint32 EventForSpell(uint32 spellId) const override
    {
        switch (spellId)
        {
            case SPELL_DREADBOLT_VOLLEY:    return EVENT_DREADBOLT;
            case SPELL_DRAIN_ESSENCE:       return EVENT_DRAIN_ESSENCE;
            case SPELL_PRIDEFUL_ERUPTION:   return EVENT_PRIDEFUL_ERUPTION;
            case SPELL_SOUL_SPIKES:         return EVENT_SOUL_SPIKES;
            default:                        return 0;
        }
    }

    void SayDeath() override { Talk(SAY_FRIEDA_DEATH); }

    void OnDanceStarted() override
    {
        // the markers of a cancelled Prideful Eruption go with the cast (capture: removed 0.05 s after the trigger)
        if (instance)
            instance->DoRemoveAurasDueToSpellOnPlayers(SPELL_PRIDEFUL_ERUPTION_MARK);
    }

    // Drain Essence: the targets are chosen when the cast starts (retail's dummy 346653), the damage aura lands when it completes
    void OnSpellStart(SpellInfo const* spell) override
    {
        if (spell->Id != SPELL_DRAIN_ESSENCE)
            return;

        size_t count = 2;
        if (SpellInfo const* targets = sSpellMgr->GetSpellInfo(SPELL_DRAIN_ESSENCE_TARGETS, me->GetMap()->GetDifficultyID()))
            if (targets->MaxAffectedTargets)
                count = targets->MaxAffectedTargets;

        _drainTargets.clear();
        for (Player* player : CouncilPickPlayers(me, count))
            _drainTargets.push_back(player->GetGUID());
    }

    void OnSpellCast(SpellInfo const* spell) override
    {
        if (spell->Id != SPELL_DRAIN_ESSENCE)
            return;

        for (ObjectGuid const& guid : _drainTargets)
            if (Player* player = ObjectAccessor::GetPlayer(*me, guid))
                if (player->IsAlive())
                    me->CastSpell(player, SPELL_DRAIN_ESSENCE_DAMAGE, true);
        _drainTargets.clear();
    }

    void ExecuteAbility(uint32 eventId) override
    {
        switch (eventId)
        {
            case EVENT_DREADBOLT:
            {
                DoCastSelf(SPELL_DREADBOLT_VOLLEY, CouncilTimedCast);

                // one lone cast, 21.8 s later a set of three spaced 5.35 s, 16.07 s between the sets (sniff 26 casts, Warcraft Logs)
                uint32 const cast = _dreadboltCasts++;
                if (cast == 0)
                    events.Repeat(DreadboltGapFirst);
                else
                    events.Repeat((cast - 1) % 3 == 2 ? DreadboltGapBetweenSets : DreadboltGapSet);
                break;
            }
            case EVENT_DRAIN_ESSENCE:
                DoCastSelf(SPELL_DRAIN_ESSENCE, CouncilTimedCast);
                events.Repeat(_drainRepeat);
                break;
            case EVENT_PRIDEFUL_ERUPTION:
            {
                // a marker on every player (retail's 346655), the cast 346657 sends a missile to each marked player
                std::vector<Player*> players = CouncilLivingPlayers(me);
                if (players.empty())
                {
                    events.Repeat(1s);
                    break;
                }

                for (Player* player : players)
                    me->CastSpell(player, SPELL_PRIDEFUL_ERUPTION_MARK, true);
                DoCastSelf(SPELL_PRIDEFUL_ERUPTION, CouncilTimedCast);
                events.Repeat(_eruptionRepeat);
                break;
            }
            case EVENT_SOUL_SPIKES:
            {
                Unit* victim = me->GetVictim();
                if (!victim)
                {
                    events.Repeat(1s);
                    break;
                }

                me->CastSpell(victim, SPELL_SOUL_SPIKES, CastSpellExtraArgs(CouncilTimedCast));
                events.Repeat(41400ms);
                break;
            }
            default:
                break;
        }
    }

private:
    Milliseconds _drainRepeat = 25700ms;
    Milliseconds _eruptionRepeat = 110s;
    GuidVector _drainTargets;
};

// 166971 - Castellan Niklaus
struct boss_castellan_niklaus : public CouncilMemberAI
{
    boss_castellan_niklaus(Creature* creature) : CouncilMemberAI(creature, MEMBER_NIKLAUS) { }

    void ScheduleAbilities(uint8 deadMask) override
    {
        _riposteRepeat = 21400ms;
        _attendantRepeat = 51400ms;

        switch (deadMask)
        {
            case 0:
                events.ScheduleEvent(EVENT_ATTENDANT, 7200ms);
                events.ScheduleEvent(EVENT_RIPOSTE, 18600ms);
                break;
            case DeadStavros:
                events.ScheduleEvent(EVENT_ATTENDANT, 8100ms);
                events.ScheduleEvent(EVENT_RIPOSTE, 12300ms);
                events.ScheduleEvent(EVENT_DREDGER_SERVANTS, 16600ms);
                _riposteRepeat = 8600ms;
                break;
            case DeadFrieda:
                events.ScheduleEvent(EVENT_DREDGER_SERVANTS, 8s);
                events.ScheduleEvent(EVENT_RIPOSTE, 12300ms);
                events.ScheduleEvent(EVENT_ATTENDANT, 42200ms);
                _riposteRepeat = 17100ms;
                break;
            default:
                // Niklaus alone (Empowered): Castellan's Cadre joins; Castellan's Fury is Mythic only and left out
                events.ScheduleEvent(EVENT_RIPOSTE, 12300ms);
                events.ScheduleEvent(EVENT_DREDGER_SERVANTS, 8s);
                events.ScheduleEvent(EVENT_CADRE, 16600ms);
                events.ScheduleEvent(EVENT_ATTENDANT, 25200ms);
                _riposteRepeat = 8600ms;
                _attendantRepeat = 25700ms;
                break;
        }
    }

    uint32 EventForSpell(uint32 spellId) const override
    {
        switch (spellId)
        {
            case SPELL_DUELISTS_RIPOSTE:        return EVENT_RIPOSTE;
            case SPELL_SUMMON_DUTIFUL_ATTENDANT: return EVENT_ATTENDANT;
            case SPELL_DREDGER_SERVANTS:        return EVENT_DREDGER_SERVANTS;
            case SPELL_CASTELLANS_CADRE:        return EVENT_CADRE;
            default:                            return 0;
        }
    }

    void SayDeath() override { Talk(SAY_NIKLAUS_DEATH); }

    // The summons come when the cast completes (attendant 2.0 s, waiters 1.0 s); a cast cancelled by the dance or a death summons nothing
    void OnSpellCast(SpellInfo const* spell) override
    {
        switch (spell->Id)
        {
            case SPELL_SUMMON_DUTIFUL_ATTENDANT:
                me->SummonCreature(NPC_DUTIFUL_ATTENDANT, Trinity::Containers::SelectRandomContainerElement(AttendantPoints), TEMPSUMMON_MANUAL_DESPAWN);
                break;
            case SPELL_DREDGER_SERVANTS:
                for (Position const& point : WaiterPoints)
                    me->SummonCreature(NPC_BEGRUDGING_WAITER, point, TEMPSUMMON_MANUAL_DESPAWN);
                // the fifth: the rear door side on the first cast, a corner after (capture, three casts)
                scheduler.Schedule(WaiterFifthDelay, [this, first = (_servantCasts++ == 0)](TaskContext)
                {
                    if (_feigned || _feigning)
                        return;

                    Position const& point = first ? WaiterRearDoor : Trinity::Containers::SelectRandomContainerElement(WaiterPoints);
                    me->SummonCreature(NPC_BEGRUDGING_WAITER, point, TEMPSUMMON_MANUAL_DESPAWN);
                });
                break;
            case SPELL_CASTELLANS_CADRE:
            {
                // one Veteran Stoneguard at a random non-tank; its own cast is the knockback (346788, source 169924 in every log)
                std::vector<Player*> targets = CouncilPickPlayers(me, 1);
                Position const spot = targets.empty() ? me->GetPosition() : targets.front()->GetPosition();
                me->SummonCreature(NPC_VETERAN_STONEGUARD, spot, TEMPSUMMON_MANUAL_DESPAWN);
                break;
            }
            default:
                break;
        }
    }

    void JustSummoned(Creature* summon) override
    {
        BossAI::JustSummoned(summon);
        if (summon->GetEntry() == NPC_VETERAN_STONEGUARD && me->IsEngaged())
            CreatureAI::DoZoneInCombat(summon);
    }

    void OnFightReset() override { _servantCasts = 0; }

    void ExecuteAbility(uint32 eventId) override
    {
        switch (eventId)
        {
            case EVENT_RIPOSTE:
            {
                Unit* victim = me->GetVictim();
                if (!victim)
                {
                    events.Repeat(1s);
                    break;
                }

                me->CastSpell(victim, SPELL_DUELISTS_RIPOSTE, CastSpellExtraArgs(CouncilTimedCast));
                events.Repeat(_riposteRepeat);
                break;
            }
            case EVENT_ATTENDANT:
                DoCastSelf(SPELL_SUMMON_DUTIFUL_ATTENDANT, CouncilTimedCast);
                events.Repeat(_attendantRepeat);
                break;
            case EVENT_DREDGER_SERVANTS:
                DoCastSelf(SPELL_DREDGER_SERVANTS, CouncilTimedCast);
                events.Repeat(51400ms);
                break;
            case EVENT_CADRE:
                DoCastSelf(SPELL_CASTELLANS_CADRE, CouncilTimedCast);
                events.Repeat(51400ms);
                break;
            default:
                break;
        }
    }

private:
    Milliseconds _riposteRepeat = 21400ms;
    Milliseconds _attendantRepeat = 51400ms;
    uint32 _servantCasts = 0;
};

// 166970 - Lord Stavros
struct boss_lord_stavros : public CouncilMemberAI
{
    boss_lord_stavros(Creature* creature) : CouncilMemberAI(creature, MEMBER_STAVROS) { }

    void ScheduleAbilities(uint8 deadMask) override
    {
        _lungeRepeat = 21400ms;
        _recitalRepeat = 51400ms;

        switch (deadMask)
        {
            case 0:
                events.ScheduleEvent(EVENT_LUNGE, 8600ms);
                events.ScheduleEvent(EVENT_RECITAL, 24300ms);
                break;
            case DeadNiklaus:
                events.ScheduleEvent(EVENT_LUNGE, 12300ms);
                events.ScheduleEvent(EVENT_RECITAL, 16600ms);
                events.ScheduleEvent(EVENT_WALTZ, WaltzFirstOneDead);
                _lungeRepeat = 8600ms;
                _recitalRepeat = 68s;
                break;
            case DeadFrieda:
                events.ScheduleEvent(EVENT_LUNGE, 10900ms);
                events.ScheduleEvent(EVENT_RECITAL, 26600ms);
                events.ScheduleEvent(EVENT_WALTZ, WaltzFirstOneDead);
                _lungeRepeat = 17400ms;
                _recitalRepeat = 68s;
                break;
            default:
                // Stavros alone (Empowered), Normal, Warcraft Logs n = 8 to 17: Violent Uproar from the pre-placed audience, once
                events.ScheduleEvent(EVENT_RECITAL, 9500ms);
                events.ScheduleEvent(EVENT_LUNGE, 15200ms);
                events.ScheduleEvent(EVENT_WALTZ, WaltzFirstTwoDead);
                _lungeRepeat = 11400ms;
                _recitalRepeat = 22900ms;
                scheduler.Schedule(UproarAfterSecondDeath, [this](TaskContext)
                {
                    if (!_feigned && !_feigning)
                        ViolentUproar();
                });
                break;
        }
    }

    uint32 EventForSpell(uint32 spellId) const override
    {
        switch (spellId)
        {
            case SPELL_EVASIVE_LUNGE:   return EVENT_LUNGE;
            case SPELL_DARK_RECITAL:    return EVENT_RECITAL;
            case SPELL_WALTZ_OF_BLOOD:  return EVENT_WALTZ;
            default:                    return 0;
        }
    }

    void SayDeath() override { Talk(SAY_STAVROS_DEATH); }

    // Dark Recital's yell comes with the cast start; the pairs are chosen when the cast completes
    void OnSpellStart(SpellInfo const* spell) override
    {
        if (spell->Id == SPELL_DARK_RECITAL)
            Talk(SAY_STAVROS_PARTNERS);
    }

    void OnSpellCast(SpellInfo const* spell) override
    {
        switch (spell->Id)
        {
            case SPELL_DARK_RECITAL:
                PairPlayers();
                break;
            case SPELL_EVASIVE_LUNGE:
                scheduler.Schedule(LungeChargeDelay, [this](TaskContext)
                {
                    if (_feigned || _feigning || _danceHidden)
                        return;

                    Unit* victim = me->GetVictim();
                    if (!victim)
                        return;

                    me->CastSpell(victim, SPELL_EVASIVE_LUNGE_CHARGE, true);
                    // jump_charge_params 419 is not in the live table: without it the effect does nothing, so the 20 yd charge is made here
                    if (!sObjectMgr->GetJumpChargeParams(LungeJumpParamsId))
                    {
                        Position dest = me->GetPosition();
                        me->MovePositionToFirstCollision(dest, LungeJumpLength, 0.0f);
                        me->GetMotionMaster()->MoveJump(POINT_JUMP, dest, LungeJumpTime, 0.0f, 0.5f);
                    }
                });
                break;
            default:
                break;
        }
    }

    ObjectGuid GetPartner(ObjectGuid const& player) const
    {
        auto itr = _partners.find(player);
        return itr != _partners.end() ? itr->second : ObjectGuid::Empty;
    }

    void OnFightReset() override { _partners.clear(); }

    void ExecuteAbility(uint32 eventId) override
    {
        switch (eventId)
        {
            case EVENT_LUNGE:
            {
                Unit* victim = me->GetVictim();
                if (!victim)
                {
                    events.Repeat(1s);
                    break;
                }

                me->CastSpell(victim, SPELL_EVASIVE_LUNGE, CastSpellExtraArgs(CouncilTimedCast));
                events.Repeat(_lungeRepeat);
                break;
            }
            case EVENT_RECITAL:
                DoCastSelf(SPELL_DARK_RECITAL, CouncilTimedCast);
                events.Repeat(_recitalRepeat);
                break;
            case EVENT_WALTZ:
                // first cast only (no repeat is evidenced); the aura goes on the players, the careening Venthyr are not scripted
                DoCastSelf(SPELL_WALTZ_OF_BLOOD, CouncilTimedCast);
                break;
            default:
                break;
        }
    }

private:
    // pairs: Normal one, Heroic two below 16 players else three, Mythic three; a pair needs two players. Tanks only fill.
    void PairPlayers()
    {
        _partners.clear();
        uint32 pairs = 1;
        if (me->GetMap()->IsMythic())
            pairs = 3;
        else if (me->GetMap()->IsHeroic())
            pairs = me->GetMap()->GetPlayersCountExceptGMs() < HeroicRecitalSmallGroup ? 2 : 3;

        std::vector<Player*> players = CouncilPickPlayers(me, pairs * 2);
        for (size_t i = 0; i + 1 < players.size(); i += 2)
        {
            Player* first = players[i];
            Player* second = players[i + 1];
            _partners[first->GetGUID()] = second->GetGUID();
            _partners[second->GetGUID()] = first->GetGUID();
            me->CastSpell(first, SPELL_DARK_RECITAL_FIRST, true);
            me->CastSpell(second, SPELL_DARK_RECITAL_SECOND, true);
        }
    }

    // Violent Uproar: every one of the 41 pre-placed 176026 casts it (15.0 s, hits players within 11 yd of the caster)
    void ViolentUproar()
    {
        std::list<Creature*> fools;
        me->GetCreatureListWithEntryInGrid(fools, NPC_DANCING_FOOL, 500.0f);
        for (Creature* fool : fools)
            if (fool->IsAlive())
                fool->CastSpell(fool, SPELL_VIOLENT_UPROAR, CastSpellExtraArgs(CouncilTimedCast));
    }

    Milliseconds _lungeRepeat = 21400ms;
    Milliseconds _recitalRepeat = 51400ms;
    std::map<ObjectGuid, ObjectGuid> _partners;
};

// ---------------------------------------------------------------------------------------------------------------------------------
// The Dance Controller: an invisible stalker in the arena. It holds the aura of Oppressive Atmosphere and Serve Drinks, the once-per-phase
// flag of the dance and runs the whole Danse Macabre timeline (every step is relative to the trigger).
// ---------------------------------------------------------------------------------------------------------------------------------
struct npc_council_dance_controller : public ScriptedAI
{
    npc_council_dance_controller(Creature* creature) : ScriptedAI(creature), _summons(creature) { }

    void Reset() override
    {
        ResetState();
    }

    uint32 GetData(uint32 id) const override
    {
        switch (id)
        {
            case DATA_DANCE_AVAILABLE:  return !_danceUsed && !_danceActive ? 1 : 0;
            case DATA_DANCE_ACTIVE:     return _danceActive ? 1 : 0;
            default:                    return 0;
        }
    }

    void SetData(uint32 id, uint32 value) override
    {
        switch (id)
        {
            case DATA_ENCOUNTER_START:
                if (!_started)
                {
                    _started = true;
                    DoCastSelf(SPELL_OPPRESSIVE_ATMOSPHERE, true);
                    DoCastSelf(SPELL_SERVE_DRINKS, true);
                }
                break;
            case DATA_DANCE_REQUEST:
                if (!_danceUsed && !_danceActive && _started)
                    StartDance(uint8(value));
                break;
            case DATA_MEMBER_DIED:
                // Oppressive Atmosphere is cleared at every death and the dance of the next phase becomes available
                _danceUsed = false;
                ClearStacks();
                break;
            case DATA_FIGHT_RESET:
            case DATA_FIGHT_END:
                ResetState();
                break;
            default:
                break;
        }
    }

    void DoAction(int32 action) override
    {
        if (action != ACTION_SERVE_DRINKS || !_started)
            return;

        if (Creature* waiter = me->SummonCreature(NPC_SERVING_WAITER, Trinity::Containers::SelectRandomContainerElement(ServingWaiterPoints), TEMPSUMMON_MANUAL_DESPAWN))
            _summons.Summon(waiter);
    }

    void UpdateAI(uint32 diff) override
    {
        events.Update(diff);
        while (uint32 eventId = events.ExecuteEvent())
            ExecuteEvent(eventId);
    }

private:
    void ResetState()
    {
        events.Reset();
        _started = false;
        _danceUsed = false;
        _danceActive = false;
        _chain.clear();
        _dancers.clear();
        _dancerPositions.clear();
        _stepIndex = 0;
        _summons.DespawnAll();
        me->RemoveAurasDueToSpell(SPELL_OPPRESSIVE_ATMOSPHERE);
        me->RemoveAurasDueToSpell(SPELL_SERVE_DRINKS);
        ClearStacks();
    }

    void ClearStacks()
    {
        if (InstanceScript* instance = me->GetInstanceScript())
            instance->DoRemoveAurasDueToSpellOnPlayers(SPELL_OPPRESSIVE_ATMOSPHERE_STACK);
    }

    CouncilMemberAI* GetMemberAI(uint8 member) const
    {
        InstanceScript* instance = me->GetInstanceScript();
        Creature* creature = instance ? instance->GetCreature(CouncilMemberData[member]) : nullptr;
        if (!creature || !creature->IsAIEnabled())
            return nullptr;

        return dynamic_cast<CouncilMemberAI*>(creature->AI());
    }

    void AllMembers(int32 action)
    {
        for (uint8 i = 0; i < MEMBER_COUNT; ++i)
            if (CouncilMemberAI* member = GetMemberAI(i))
                if (member->GetCouncilCreature()->IsAlive())
                    member->DoAction(action);
    }

    // Niklaus leads, Frieda when he is dead; nobody is evidenced when only Stavros is left (no yell)
    CouncilMemberAI* GetLeader() const
    {
        for (uint8 i : { uint8(MEMBER_NIKLAUS), uint8(MEMBER_FRIEDA) })
            if (CouncilMemberAI* member = GetMemberAI(i))
                if (!member->IsFeigned() && member->GetCouncilCreature()->IsAlive())
                    return member;
        return nullptr;
    }

    void StartDance(uint8 /*caller*/)
    {
        _danceUsed = true;
        _danceActive = true;
        _stepIndex = 0;
        _chain.clear();

        // Oppressive Atmosphere is off until trigger+28.2 and its stacks go at +0.5
        me->RemoveAurasDueToSpell(SPELL_OPPRESSIVE_ATMOSPHERE);
        AllMembers(ACTION_DANCE_START);

        events.ScheduleEvent(EVENT_DANCE_FORMATION_YELL, 200ms);
        events.ScheduleEvent(EVENT_DANCE_CLEAR_STACKS, 500ms);
        events.ScheduleEvent(EVENT_DANCE_HIDE, 2400ms);
        events.ScheduleEvent(EVENT_DANCE_SPHERES, 3000ms);
        events.ScheduleEvent(EVENT_DANCE_EMOTE, 3200ms);
        events.ScheduleEvent(EVENT_DANCE_DANCERS, 3300ms);
        events.ScheduleEvent(EVENT_DANCE_WRONG_START, 11770ms);
        events.ScheduleEvent(EVENT_DANCE_STEP, 11780ms);
        events.ScheduleEvent(EVENT_DANCE_OFF, 28200ms);
        events.ScheduleEvent(EVENT_DANCE_ATMOSPHERE_BACK, 28190ms);
        events.ScheduleEvent(EVENT_DANCE_DANCERS_LEAVE, 28400ms);
        events.ScheduleEvent(EVENT_DANCE_RELEASE, 30s);
        events.ScheduleEvent(EVENT_DANCE_RESUME, 31500ms);
    }

    void ExecuteEvent(uint32 eventId)
    {
        switch (eventId)
        {
            case EVENT_DANCE_FORMATION_YELL:
                if (CouncilMemberAI* leader = GetLeader())
                    leader->Talk(leader->GetMember() == MEMBER_NIKLAUS ? SAY_NIKLAUS_FORMATION : SAY_FRIEDA_FORMATION);
                break;
            case EVENT_DANCE_CLEAR_STACKS:
                ClearStacks();
                break;
            case EVENT_DANCE_HIDE:
                AllMembers(ACTION_DANCE_HIDE);
                break;
            case EVENT_DANCE_SPHERES:
                // every player casts the sphere at its own position; the owner's entry gives the in-position marker
                for (Player* player : CouncilLivingPlayers(me))
                {
                    _chain[player->GetGUID()] = player->GetPosition();
                    player->CastSpell(player->GetPosition(), SPELL_DANCE_SPHERE, CastSpellExtraArgs(TRIGGERED_FULL_MASK));
                }
                break;
            case EVENT_DANCE_EMOTE:
                Talk(SAY_CONTROLLER_PLACES);
                break;
            case EVENT_DANCE_DANCERS:
                SummonDancers();
                break;
            case EVENT_DANCE_WRONG_START:
                // 328497: a player without the in-position marker gets Wrong Moves at once
                for (Player* player : CouncilLivingPlayers(me))
                    if (!player->HasAura(SPELL_DANCE_MARKER))
                        me->CastSpell(player, SPELL_WRONG_MOVES_DRIVER, true);
                break;
            case EVENT_DANCE_STEP:
                DanceStep();
                if (++_stepIndex < DanceStepCount)
                    events.Repeat(DanceStepInterval);
                break;
            case EVENT_DANCE_STEP_YELL:
                StepYell();
                break;
            case EVENT_DANCE_DANCER_MOVE:
                MoveDancers(_stepVector);
                break;
            case EVENT_DANCE_ATMOSPHERE_BACK:
                DoCastSelf(SPELL_OPPRESSIVE_ATMOSPHERE, true);
                break;
            case EVENT_DANCE_OFF:
                // 331006 / 347218: the markers and Wrong Moves go for everyone; the leader says the end line, the members jump back.
                // A step window still running (the last one ends 0.1 s before this, later after a slow tick) goes too, before its
                // expiry can give Wrong Moves after the dance.
                events.CancelEvent(EVENT_DANCE_STEP);
                for (Player* player : CouncilLivingPlayers(me))
                    for (uint32 spellId : { SPELL_DANCE_WINDOW, SPELL_DANCE_MARKER, SPELL_TAKEN_POSITION, SPELL_WRONG_MOVES_DRIVER, SPELL_WRONG_MOVES })
                        player->RemoveAurasDueToSpell(spellId);
                if (CouncilMemberAI* leader = GetLeader())
                    leader->Talk(leader->GetMember() == MEMBER_NIKLAUS ? SAY_NIKLAUS_DANCE_END : SAY_FRIEDA_DANCE_END);
                AllMembers(ACTION_DANCE_RETURN);
                break;
            case EVENT_DANCE_DANCERS_LEAVE:
                SendDancersAway();
                break;
            case EVENT_DANCE_RELEASE:
                AllMembers(ACTION_DANCE_RELEASE);
                break;
            case EVENT_DANCE_RESUME:
                _danceActive = false;
                _chain.clear();
                AllMembers(ACTION_DANCE_RESUME);
                break;
            default:
                break;
        }
    }

    // a window: the aura 331458 on every player (the condition rows keep it to players), and every player casts the box at the previous
    // centre plus the step vector; the leader yells 0.2 s after the tile and the dancers move 0.4 s after it
    void DanceStep()
    {
        static constexpr float Vectors[4][2] = { { -DanceStepLength, 0.0f }, { DanceStepLength, 0.0f }, { 0.0f, DanceStepLength }, { 0.0f, -DanceStepLength } };
        _step = uint8(urand(0, 3));
        _stepVector = Position(Vectors[_step][0], Vectors[_step][1], 0.0f);

        DoCastSelf(SPELL_DANCE_WINDOW, true);
        for (Player* player : CouncilLivingPlayers(me))
        {
            auto itr = _chain.find(player->GetGUID());
            if (itr == _chain.end())
                continue;

            Position& centre = itr->second;
            centre.Relocate(centre.GetPositionX() + _stepVector.GetPositionX(), centre.GetPositionY() + _stepVector.GetPositionY(), centre.GetPositionZ());
            player->CastSpell(centre, SPELL_DANCE_BOX, CastSpellExtraArgs(TRIGGERED_FULL_MASK));
        }

        events.ScheduleEvent(EVENT_DANCE_STEP_YELL, 200ms);
        events.ScheduleEvent(EVENT_DANCE_DANCER_MOVE, 400ms);
    }

    // Niklaus has all four lines; Frieda's Shimmy right and Sashay left were captured, her Boogie down and Prance Forward were not
    void StepYell()
    {
        CouncilMemberAI* leader = GetLeader();
        if (!leader)
            return;

        static constexpr int32 NiklausLines[4] = { SAY_NIKLAUS_STEP_RIGHT, SAY_NIKLAUS_STEP_LEFT, SAY_NIKLAUS_STEP_DOWN, SAY_NIKLAUS_STEP_FORWARD };
        if (leader->GetMember() == MEMBER_NIKLAUS)
            leader->Talk(NiklausLines[_step]);
        else if (_step == 0)
            leader->Talk(SAY_FRIEDA_STEP_RIGHT);
        else if (_step == 1)
            leader->Talk(SAY_FRIEDA_STEP_LEFT);
    }

    // 45 Belligerent Waiters from the four corners run to the grid in 6.5 s and repeat every step; they only demonstrate
    void SummonDancers()
    {
        _dancers.clear();
        _dancerPositions.clear();
        for (size_t i = 0; i < std::size(DancerGrid); ++i)
        {
            Creature* dancer = me->SummonCreature(NPC_DANCER, DancerCorners[i % std::size(DancerCorners)], TEMPSUMMON_MANUAL_DESPAWN);
            if (!dancer)
                continue;

            _summons.Summon(dancer);
            Position const slot(DancerGrid[i][0], DancerGrid[i][1], DanceFloorZ);
            _dancers.push_back(dancer->GetGUID());
            _dancerPositions.push_back(slot);
            float const speed = dancer->GetExactDist(slot) / std::chrono::duration_cast<std::chrono::duration<float>>(DancerRunTime).count();
            dancer->GetMotionMaster()->MovePoint(POINT_DANCER, slot, false, {}, speed);
        }
    }

    void MoveDancers(Position const& vector)
    {
        for (size_t i = 0; i < _dancers.size(); ++i)
        {
            Creature* dancer = ObjectAccessor::GetCreature(*me, _dancers[i]);
            if (!dancer)
                continue;

            Position& slot = _dancerPositions[i];
            slot.Relocate(slot.GetPositionX() + vector.GetPositionX(), slot.GetPositionY() + vector.GetPositionY(), slot.GetPositionZ());
            dancer->GetMotionMaster()->MovePoint(POINT_DANCER, slot, false, {}, DancerStepSpeed);
        }
    }

    void SendDancersAway()
    {
        for (ObjectGuid const& guid : _dancers)
            if (Creature* dancer = ObjectAccessor::GetCreature(*me, guid))
            {
                dancer->GetMotionMaster()->MovePoint(POINT_DANCER, DancerExit, false);
                dancer->DespawnOrUnsummon(6s);
            }
        _dancers.clear();
        _dancerPositions.clear();
    }

    EventMap events;
    SummonList _summons;
    bool _started = false;
    bool _danceUsed = false;
    bool _danceActive = false;
    uint32 _stepIndex = 0;
    uint8 _step = 0;
    Position _stepVector;
    std::map<ObjectGuid, Position> _chain;      // each player's last box centre (chained from its sphere)
    GuidVector _dancers;
    std::vector<Position> _dancerPositions;
};

// ---------------------------------------------------------------------------------------------------------------------------------
// Adds
// ---------------------------------------------------------------------------------------------------------------------------------

// 175992 - Dutiful Attendant: 1.5 s after it appears it picks the living member with the lowest health percent (a feigned member is
// skipped), runs to it and puts Unyielding Shield 346694 on it; the shield goes with the attendant
struct npc_council_dutiful_attendant : public ScriptedAI
{
    npc_council_dutiful_attendant(Creature* creature) : ScriptedAI(creature) { }

    void JustAppeared() override
    {
        ScriptedAI::JustAppeared();
        me->SetReactState(REACT_PASSIVE);
        DoCastSelf(SPELL_SPAWN_COSMETICS, true);
        _events.ScheduleEvent(EVENT_ATTENDANT_CHOOSE, AttendantChooseDelay);
    }

    void MovementInform(uint32 type, uint32 id) override
    {
        if (type != POINT_MOTION_TYPE || id != POINT_SHIELD)
            return;

        if (Creature* member = ObjectAccessor::GetCreature(*me, _target))
            if (member->IsAlive())
            {
                me->CastSpell(member, SPELL_UNYIELDING_SHIELD, true);
                _shielded = true;
            }
    }

    void JustDied(Unit* /*killer*/) override
    {
        RemoveShield();
    }

    void OnDespawn() override
    {
        RemoveShield();
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);
        if (_events.ExecuteEvent() == EVENT_ATTENDANT_CHOOSE)
            Choose();
    }

private:
    void Choose()
    {
        InstanceScript* instance = me->GetInstanceScript();
        if (!instance)
            return;

        Creature* weakest = nullptr;
        float lowest = 101.0f;
        for (uint32 data : CouncilMemberData)
        {
            Creature* member = instance->GetCreature(data);
            if (!member || !member->IsAlive())
                continue;

            // the feigned member regenerates from 1 health and is not a target
            CouncilMemberAI* ai = member->IsAIEnabled() ? dynamic_cast<CouncilMemberAI*>(member->AI()) : nullptr;
            if (!ai || ai->IsFeigned())
                continue;

            if (member->GetHealthPct() < lowest)
            {
                lowest = member->GetHealthPct();
                weakest = member;
            }
        }

        if (!weakest)
            return;

        _target = weakest->GetGUID();
        me->GetMotionMaster()->MovePoint(POINT_SHIELD, weakest->GetPosition(), true);
    }

    void RemoveShield()
    {
        if (!_shielded)
            return;

        _shielded = false;
        if (Creature* member = ObjectAccessor::GetCreature(*me, _target))
            member->RemoveAurasDueToSpell(SPELL_UNYIELDING_SHIELD, me->GetGUID());
    }

    EventMap _events;
    ObjectGuid _target;
    bool _shielded = false;
};

// 169925 - Begrudging Waiter (Dredger Servants): 2.5 s after the spawn they run at the players and throw food at a random player
// every 2.4 s (1.0 s cast, 40 yd)
struct npc_council_begrudging_waiter : public ScriptedAI
{
    npc_council_begrudging_waiter(Creature* creature) : ScriptedAI(creature) { }

    void JustAppeared() override
    {
        ScriptedAI::JustAppeared();
        me->SetReactState(REACT_PASSIVE);
        _events.ScheduleEvent(EVENT_WAITER_AGGRO, WaiterAggroDelay);
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);
        while (uint32 eventId = _events.ExecuteEvent())
        {
            switch (eventId)
            {
                case EVENT_WAITER_AGGRO:
                    me->SetReactState(REACT_AGGRESSIVE);
                    DoZoneInCombat();
                    _events.ScheduleEvent(EVENT_THROW_FOOD, 100ms);
                    break;
                case EVENT_THROW_FOOD:
                {
                    // the instant dummy 348575 picks a random player (tanks included: 13% of the hits in Warcraft Logs)
                    std::vector<Player*> players = CouncilLivingPlayers(me);
                    if (!players.empty() && !me->HasUnitState(UNIT_STATE_CASTING))
                    {
                        Player* target = Trinity::Containers::SelectRandomContainerElement(players);
                        if (me->GetExactDist(target) <= ThrowFoodRange)
                            me->CastSpell(target, SPELL_THROW_FOOD, CastSpellExtraArgs(CouncilTimedCast));
                    }
                    _events.ScheduleEvent(EVENT_THROW_FOOD, ThrowFoodInterval);
                    break;
                }
                default:
                    break;
            }
        }

        UpdateVictim();
    }

private:
    EventMap _events;
};

// 169924 - Veteran Stoneguard (Castellan's Cadre): its spawn is the knockback; it fixates a random player for 12 s at a time and casts
// Sintouched Blade (1.5 s cast, then +50% damage done per cast) 7.0 s after it appears and every 7.3 s
struct npc_council_veteran_stoneguard : public ScriptedAI
{
    npc_council_veteran_stoneguard(Creature* creature) : ScriptedAI(creature) { }

    void IsSummonedBy(WorldObject* /*summoner*/) override
    {
        DoCastSelf(SPELL_CADRE_KNOCKBACK, true);
        _events.ScheduleEvent(EVENT_FIXATE, 500ms);
        _events.ScheduleEvent(EVENT_SINTOUCHED_BLADE, StoneguardBladeFirst);
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);
        while (uint32 eventId = _events.ExecuteEvent())
        {
            switch (eventId)
            {
                case EVENT_FIXATE:
                    if (std::vector<Player*> targets = CouncilPickPlayers(me, 1); !targets.empty())
                    {
                        me->CastSpell(targets.front(), SPELL_FIXATE, true);
                        me->GetThreatManager().FixateTarget(targets.front());
                    }
                    _events.Repeat(StoneguardFixate);
                    break;
                case EVENT_SINTOUCHED_BLADE:
                    if (Unit* victim = me->GetVictim())
                    {
                        me->CastSpell(victim, SPELL_SINTOUCHED_BLADE, CastSpellExtraArgs(CouncilTimedCast));
                        _events.ScheduleEvent(EVENT_SINTOUCHED_BLADE + 100, 1500ms);
                    }
                    _events.Repeat(StoneguardBladeRepeat);
                    break;
                case EVENT_SINTOUCHED_BLADE + 100:
                    DoCastSelf(SPELL_SINTOUCHED_BLADE_STACK, true);
                    break;
                default:
                    break;
            }
        }

        UpdateVictim();
    }

private:
    EventMap _events;
};

// ---------------------------------------------------------------------------------------------------------------------------------
// Spell scripts
// ---------------------------------------------------------------------------------------------------------------------------------

// 346657 - Prideful Eruption: the completed cast sends a missile (346661) to every marked player (the dummy effect of the cast carries
// no trigger of its own in the core)
class spell_council_prideful_eruption : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_PRIDEFUL_ERUPTION_MARK, SPELL_PRIDEFUL_ERUPTION_MISSILE });
    }

    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        Unit* target = GetHitUnit();
        if (target && target->HasAura(SPELL_PRIDEFUL_ERUPTION_MARK))
            GetCaster()->CastSpell(target, SPELL_PRIDEFUL_ERUPTION_MISSILE, true);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_council_prideful_eruption::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 346658 - Prideful Eruption: when the missile arrives the marker goes (effect 0) and the 6 yd blast 346660 lands at the player's spot
class spell_council_prideful_eruption_missile : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_PRIDEFUL_ERUPTION_BLAST });
    }

    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        if (Unit* target = GetHitUnit())
            GetCaster()->CastSpell(target->GetPosition(), SPELL_PRIDEFUL_ERUPTION_BLAST, true);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_council_prideful_eruption_missile::HandleDummy, EFFECT_1, SPELL_EFFECT_DUMMY);
    }
};

// 346762 - Soul Spikes: every tick of the aura (+0.0, +1.5, +3.0, +4.5) puts 346681 on the tank 0.16 s later and 346685 at its spot
class spell_council_soul_spikes : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_SOUL_SPIKES_HIT, SPELL_SOUL_SPIKES_WAVE });
    }

    void HandlePeriodic(AuraEffect const* /*aurEff*/)
    {
        Unit* caster = GetCaster();
        Unit* victim = GetTarget();
        if (!caster || !victim)
            return;

        caster->m_Events.AddEventAtOffset([caster, victimGuid = victim->GetGUID()]()
        {
            Unit* target = ObjectAccessor::GetUnit(*caster, victimGuid);
            if (!target || !target->IsAlive())
                return;

            caster->CastSpell(target, SPELL_SOUL_SPIKES_HIT, true);
            // the victim is left out of the radiating hit (HitTargetsCount 0 for the tank in the capture)
            caster->CastSpell(target->GetPosition(), SPELL_SOUL_SPIKES_WAVE,
                CastSpellExtraArgs(TRIGGERED_FULL_MASK).SetCustomArg(std::vector<ObjectGuid>{ victimGuid }));
        }, SoulSpikeDelay);
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_council_soul_spikes::HandlePeriodic, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY);
    }
};

// 346685 - Soul Spikes: radiates to the other players only
class spell_council_soul_spikes_wave : public SpellScript
{
    void FilterTargets(std::list<WorldObject*>& targets)
    {
        auto const* guids = std::any_cast<std::vector<ObjectGuid>>(&GetSpell()->m_customArg);
        if (!guids)
            return;

        targets.remove_if([guids](WorldObject const* target)
        {
            return std::find(guids->begin(), guids->end(), target->GetGUID()) != guids->end();
        });
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_council_soul_spikes_wave::FilterTargets, EFFECT_0, TARGET_UNIT_DEST_AREA_ENEMY);
    }
};

// 327503 - Evasive Lunge: the players in the 20 x 5 yd line (dummy effect 0, enemy rect) get the hit 327610
class spell_council_evasive_lunge : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_EVASIVE_LUNGE_HIT });
    }

    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        if (Unit* target = GetHitUnit())
            GetCaster()->CastSpell(target, SPELL_EVASIVE_LUNGE_HIT, true);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_council_evasive_lunge::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 331636, 331637 - Dark Recital: every 0.5 s the partner's spot erupts (334824), and a player farther than 8 yd from the partner takes 331638
class spell_council_dark_recital : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DARK_RECITAL_LEASH, SPELL_DARK_RECITAL_ERUPTION });
    }

    void HandlePeriodic(AuraEffect const* /*aurEff*/)
    {
        Unit* caster = GetCaster();
        Unit* target = GetTarget();
        if (!caster || !caster->IsCreature() || !target || !caster->ToCreature()->IsAIEnabled())
            return;

        boss_lord_stavros* ai = dynamic_cast<boss_lord_stavros*>(caster->ToCreature()->AI());
        if (!ai)
            return;

        Unit* partner = ObjectAccessor::GetUnit(*caster, ai->GetPartner(target->GetGUID()));
        if (!partner || !partner->IsAlive())
            return;

        if (target->GetExactDist(partner) > RecitalLeash)
            caster->CastSpell(target, SPELL_DARK_RECITAL_LEASH, true);
        caster->CastSpell(partner->GetPosition(), SPELL_DARK_RECITAL_ERUPTION, true);
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_council_dark_recital::HandlePeriodic, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY);
    }
};

// 334824 - Dark Recital: the eruption at the partner's spot hits the players within 2.5 yd (dummy effect 0, players by condition)
class spell_council_dark_recital_eruption : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DARK_RECITAL_BLAST });
    }

    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        if (Unit* target = GetHitUnit())
            GetCaster()->CastSpell(target, SPELL_DARK_RECITAL_BLAST, true);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_council_dark_recital_eruption::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 347219 - Wrong Moves: the 1 s tick takes Fancy Footwork and casts the stacking tick 330848; ending the aura (a box was entered, or the
// dance is over) ends the ticks
class spell_council_wrong_moves : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_WRONG_MOVES, SPELL_FANCY_FOOTWORK });
    }

    void HandlePeriodic(AuraEffect const* /*aurEff*/)
    {
        Unit* target = GetTarget();
        target->RemoveAurasDueToSpell(SPELL_FANCY_FOOTWORK);
        (GetCaster() ? GetCaster() : target)->CastSpell(target, SPELL_WRONG_MOVES, true);
    }

    void HandleRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        GetTarget()->RemoveAurasDueToSpell(SPELL_WRONG_MOVES);
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_council_wrong_moves::HandlePeriodic, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY);
        AfterEffectRemove += AuraEffectRemoveFn(spell_council_wrong_moves::HandleRemove, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }
};

// 331458 - Dance Step Window: when it runs out (the player did not enter the box, entering removes it) the player gets Wrong Moves
class spell_council_dance_window : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_WRONG_MOVES_DRIVER });
    }

    void HandleRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        if (GetTargetApplication()->GetRemoveMode() != AURA_REMOVE_BY_EXPIRE)
            return;

        Unit* target = GetTarget();
        if (target->IsAlive())
            (GetCaster() ? GetCaster() : target)->CastSpell(target, SPELL_WRONG_MOVES_DRIVER, true);
    }

    void Register() override
    {
        AfterEffectRemove += AuraEffectRemoveFn(spell_council_dance_window::HandleRemove, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }
};

// 176026 - Dancing Fools and 176017 - Waltzing Venthyr: each audience vehicle carries a Waltzing Venthyr 176017, which carries a rider
// 169727 (capture: seat 0 both times, 41 of 41, present before the pull). vehicle_template_accessory needs a spellclick row on the vehicle,
// which would make the audience clickable, so the vehicles install their passenger themselves with the hardcoded ride spell.
struct npc_council_waltz_vehicle : public ScriptedAI
{
    npc_council_waltz_vehicle(Creature* creature) : ScriptedAI(creature) { }

    void JustAppeared() override
    {
        Vehicle* kit = me->GetVehicleKit();
        if (!kit || kit->GetPassenger(0))
            return;

        uint32 const passenger = me->GetEntry() == NPC_DANCING_FOOL ? NPC_FOOL_CARRIER : NPC_FOOL_RIDER;
        kit->InstallAccessory(passenger, 0, true, TEMPSUMMON_CORPSE_TIMED_DESPAWN, 30000, VEHICLE_SPELL_RIDE_HARDCODED);
    }

    void UpdateAI(uint32 /*diff*/) override { }
};

// 345368 - Serve Drinks: the 60 s tick (retail triggers 345370, a SEND_EVENT nothing in the core listens to) spawns the clickable waiter
class spell_council_serve_drinks : public AuraScript
{
    void HandlePeriodic(AuraEffect const* /*aurEff*/)
    {
        PreventDefaultAction();
        if (Creature* controller = GetTarget()->ToCreature())
            if (controller->IsAIEnabled())
                controller->AI()->DoAction(ACTION_SERVE_DRINKS);
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_council_serve_drinks::HandlePeriodic, EFFECT_0, SPELL_AURA_PERIODIC_TRIGGER_SPELL);
    }
};

// ---------------------------------------------------------------------------------------------------------------------------------
// Areatrigger scripts: the step tiles are cast by the player and only its owner counts
// ---------------------------------------------------------------------------------------------------------------------------------

// 328485 (create properties 20402) - the starting sphere: standing in it gives "You have taken your position" and the marker 328500
struct at_council_dance_sphere : AreaTriggerAI
{
    using AreaTriggerAI::AreaTriggerAI;

    void OnUnitEnter(Unit* unit) override
    {
        if (!unit->IsPlayer() || unit->GetGUID() != at->GetCasterGuid())
            return;

        unit->CastSpell(unit, SPELL_TAKEN_POSITION, true);
        if (!unit->HasAura(SPELL_DANCE_MARKER))
            unit->CastSpell(unit, SPELL_DANCE_MARKER, true);
    }
};

// 344181 (create properties 21954) - the step box: entering it removes the window and Wrong Moves and gives Fancy Footwork (+3% haste,
// stacking) and the marker
struct at_council_dance_box : AreaTriggerAI
{
    using AreaTriggerAI::AreaTriggerAI;

    void OnUnitEnter(Unit* unit) override
    {
        if (!unit->IsPlayer() || unit->GetGUID() != at->GetCasterGuid())
            return;

        unit->RemoveAurasDueToSpell(SPELL_DANCE_WINDOW);
        unit->RemoveAurasDueToSpell(SPELL_WRONG_MOVES_DRIVER);
        unit->CastSpell(unit, SPELL_FANCY_FOOTWORK, true);
        if (!unit->HasAura(SPELL_DANCE_MARKER))
            unit->CastSpell(unit, SPELL_DANCE_MARKER, true);
    }
};

void AddSC_boss_council_of_blood()
{
    RegisterCastleNathriaCreatureAI(boss_baroness_frieda);
    RegisterCastleNathriaCreatureAI(npc_council_waltz_vehicle);
    RegisterCastleNathriaCreatureAI(boss_lord_stavros);
    RegisterCastleNathriaCreatureAI(boss_castellan_niklaus);
    RegisterCastleNathriaCreatureAI(npc_council_dance_controller);
    RegisterCastleNathriaCreatureAI(npc_council_dutiful_attendant);
    RegisterCastleNathriaCreatureAI(npc_council_begrudging_waiter);
    RegisterCastleNathriaCreatureAI(npc_council_veteran_stoneguard);
    RegisterSpellScript(spell_council_prideful_eruption);
    RegisterSpellScript(spell_council_prideful_eruption_missile);
    RegisterSpellScript(spell_council_soul_spikes);
    RegisterSpellScript(spell_council_soul_spikes_wave);
    RegisterSpellScript(spell_council_evasive_lunge);
    RegisterSpellScript(spell_council_dark_recital);
    RegisterSpellScript(spell_council_dark_recital_eruption);
    RegisterSpellScript(spell_council_wrong_moves);
    RegisterSpellScript(spell_council_dance_window);
    RegisterSpellScript(spell_council_serve_drinks);
    RegisterAreaTriggerAI(at_council_dance_sphere);
    RegisterAreaTriggerAI(at_council_dance_box);
}
