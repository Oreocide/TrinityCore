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
 * The Manastorms (DungeonEncounter 2394, trinitycore-tracking/sl-dungeons/de-other-side/bosses/the-manastorms.md).
 *
 * Millhouse and Millificent are two BossAI of DATA_THE_MANASTORMS (as the Council of Blood): either one's pull engages both, either
 * one's wipe resets both, and the encounter is finished by the script when both are defeated. Neither dies: each is defeated by the
 * hit that would take it to 10% or below (a lethal hit is clamped to 1 health), turns friendly and leaves combat; the survivor comes
 * down to the floor and fights on alone.
 *
 * One boss stands on the floor while the other hangs on the rail (rooted, floating, under Restrictive Targeting). Millhouse keeps the
 * clock: every 32 s the two trade places (Heroic capture: 0, 32, 64, 96 s, +-1 s). Each phase repeats the same ladder counted from
 * the swap, so there is one floor ladder and one rail ladder per boss (the capture's later phases are the first ones shifted by the
 * 64 s cycle). Medium: one Heroic pull and two Warcraft Logs Heroic/Normal pulls; the Millhouse openers and Millificent's 8 s
 * cadence are High (Mythic-0 n=25 agrees).
 *
 * The swap is scripted movement (the Rocket Jump / Teleport spells target creatures by entry through DEST_NEARBY_ENTRY conditions and
 * jump_charge_params 417, none of which exist): Millificent jumps for 2001 ms with the capture's visual, Millhouse teleports one
 * second after he roots. Do not also add the data for those spells, or a boss would move twice.
 *
 * Not scripted:
 * - Experimental Squirrel Bomb 320823 (carts 164561 with the 164568 squirrels): the vehicle seats 21405-21407 exist, but there are no
 *   vehicle_template_accessory rows, the summon 320811 (properties 3713, target 138) places two carts per cast with no known rule,
 *   the dismantle spellclick 320140 is a DUMMY, and the squirrels charge while still seated in the capture. Millificent's yell
 *   (group 0) belongs to this cast and is unused.
 * - The Power Crystal beam (320780 -> 320782 -> 320785 / 320786, 336225): the crystal is summoned, but the tank-intercept targeting
 *   is not proven by the capture, so no beam is made.
 * - Buzz-Saw's root (330434) and Bleeding 320147: the capture shows Bleeding on two of the six hits of the second and third saw and on none
 *   of the first and of the throws, with no rule to read from it.
 * - Mythic: Diabolical Dooooooom 320141, Echo Finger Laser 324047 / 323877, Shadowfury 320132, Aerial Rocket Chicken Barrage 321061
 *   (320844 is not in the DB2), the damage push of the phases.
 * - The [DNT] Frostbolt Target proxy 1286159: the rail Millhouse casts Frostbolt 320008 directly.
 * - The Draw Power outro of the helper 164934 (332388, 335978 achievement credit), conversation 15437 before the pull, the Oil Spray
 *   hazard of the stalkers 170049 (a room hazard that runs outside the encounter).
 */

#include "Conversation.h"
#include "Creature.h"
#include "GameObject.h"
#include "GameObjectAI.h"
#include "GameTime.h"
#include "InstanceScript.h"
#include "Map.h"
#include "MotionMaster.h"
#include "MoveSplineInit.h"
#include "PassiveAI.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "SpellAuraEffects.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "SpellScript.h"
#include "de_other_side.h"
#include <array>

namespace
{
enum ManastormSpells
{
    SPELL_FROSTBOLT                 = 320008,   // 2 s cast, interruptible, Millhouse in every phase
    SPELL_SUMMON_POWER_CRYSTAL      = 320787,   // 1 s cast, 326899 summons the crystal 164560 for 15 s
    SPELL_THROW_BUZZ_SAW            = 320168,   // 2 s cast from the rail, TRIGGER_MISSILE 320145
    SPELL_BUZZ_SAW                  = 320144,   // 1 s cast on the floor: CHARGE (320145 on arrival) and a 2 s PERIODIC_DUMMY aura on the target, 500 ms
    SPELL_BUZZ_SAW_HIT              = 320145,
    SPELL_CHILLED_HEART             = 342905,   // Millhouse's floor aura (+450% damage done): only he carries it
    SPELL_RESTRICTIVE_TARGETING     = 326912,   // the boss on the rail: INTERFERE_ALL_TARGETING, scale, deselect
    SPELL_ROCKET_JUMP_AURA          = 326793,   // 2 s on Millificent around each jump
    SPELL_DEFEATED_MARKER           = 345720,   // AURA_DUMMY on a defeated boss
    SPELL_MILLIFICENT_PASSIVE       = 326791    // creature_template_addon aura, lost with an evade
};

enum ManastormEvents
{
    EVENT_FROSTBOLT = 1,
    EVENT_FROSTBOLT_RAIL,
    EVENT_POWER_CRYSTAL,
    EVENT_BUZZ_SAW,
    EVENT_THROW_BUZZ_SAW,
    EVENT_FLOOR_CYCLE
};

enum Texts
{
    // Group ids as in the live creature_text rows (entry 164555: 0 bomb, 1 defeated; entry 164556: 0 crystal)
    SAY_MILLHOUSE_POWER             = 0,    // Power beyond your comprehension!
    SAY_MILLIFICENT_DEFEATED        = 1     // Wait! You can have the mojo. Millhouse is more important!
};

enum ManastormMisc
{
    NPC_POWER_CRYSTAL               = 164560,
    NPC_MANASTORM_HELPER            = 164934,
    GO_GNOMISH_MACHINATIONS         = 355436,
    CONVERSATION_MANASTORMS_END     = 15487,
    FACTION_MANASTORMS_HOSTILE      = 16,
    SPELL_VISUAL_ROCKET_JUMP        = 97313,
    POINT_ROCKET_JUMP               = 1
};

static constexpr float DefeatHealthPct = 10.0f;
static constexpr Milliseconds PhaseLength = 32s;
static constexpr Milliseconds RocketJumpTime = 2001ms;      // capture: both jumps lasted 2001 ms
static constexpr Milliseconds TeleportDelay = 1s;           // Millhouse's Teleport cast to his arrival
static constexpr Milliseconds ChilledHeartDelay = 4s;       // after his arrival (68 s and 113 s in the capture)
static constexpr Milliseconds DefeatYellDelay = 1s;         // Millificent's yell
static constexpr Milliseconds KneelDelay = 2s;
static constexpr Milliseconds EndDelay = 1s;                // the second defeat to ENCOUNTER_END (118 s -> 119 s)
static constexpr Milliseconds HelperDespawnDelay = 7s;      // after the end (126 s)
static constexpr Seconds EvadeRespawnDelay = 30s;           // Low: no wipe in the capture
static constexpr float FrostboltChaseRange = 25.0f;         // Low: the capture's Millhouse walks about the floor

// Heroic capture, offsets from the swap
static Position const MillificentRail = { 2282.6145f, -2228.0469f, 16.57554f, 0.0f };
static Position const MillhouseRail = { 2356.9514f, -2200.5261f, 16.892464f, 3.6302848f };
static Position const FloorLanding = { 2321.4045f, -2218.1301f, 5.954546f, 0.0f };
static Position const ChestPos = { 2321.2986f, -2218.1199f, 5.9545355f, 3.8113954f };

struct ManastormStep
{
    uint32 EventId;
    Milliseconds Offset;
};

static constexpr std::array<ManastormStep, 12> MillhouseFloorLadder =
{ {
    { EVENT_FROSTBOLT,          5s },
    { EVENT_FROSTBOLT,          8s },
    { EVENT_POWER_CRYSTAL,      10s },
    { EVENT_FROSTBOLT,          12s },
    { EVENT_FROSTBOLT,          14s },
    { EVENT_POWER_CRYSTAL,      17s },
    { EVENT_FROSTBOLT,          18s },
    { EVENT_FROSTBOLT,          21s },
    { EVENT_FROSTBOLT,          23s },
    { EVENT_POWER_CRYSTAL,      26s },
    { EVENT_FROSTBOLT,          27s },
    { EVENT_FROSTBOLT,          29s }
} };

static constexpr std::array<ManastormStep, 7> MillhouseRailLadder =
{ {
    { EVENT_FROSTBOLT_RAIL,     6s },
    { EVENT_FROSTBOLT_RAIL,     10s },
    { EVENT_FROSTBOLT_RAIL,     14s },
    { EVENT_FROSTBOLT_RAIL,     18s },
    { EVENT_FROSTBOLT_RAIL,     22s },
    { EVENT_FROSTBOLT_RAIL,     26s },
    { EVENT_FROSTBOLT_RAIL,     30s }
} };

static constexpr std::array<ManastormStep, 4> MillificentFloorLadder =
{ {
    { EVENT_BUZZ_SAW,           5s },
    { EVENT_BUZZ_SAW,           13s },
    { EVENT_BUZZ_SAW,           21s },
    { EVENT_BUZZ_SAW,           29s }
} };

static constexpr std::array<ManastormStep, 5> MillificentRailLadder =
{ {
    { EVENT_THROW_BUZZ_SAW,     6s },
    { EVENT_THROW_BUZZ_SAW,     12s },
    { EVENT_THROW_BUZZ_SAW,     18s },
    { EVENT_THROW_BUZZ_SAW,     24s },
    { EVENT_THROW_BUZZ_SAW,     30s }
} };

class ManastormAI : public BossAI
{
public:
    ManastormAI(Creature* creature, bool isMillhouse) : BossAI(creature, DATA_THE_MANASTORMS), _isMillhouse(isMillhouse),
        _generation(0), _defeated(false), _onRail(false), _solo(false), _evading(false), _phaseStartMs(0) { }

    void Reset() override
    {
        BossAI::Reset();
        ++_generation;
        _defeated = false;
        _onRail = false;
        _solo = false;
        _evading = false;

        me->SetControlled(false, UNIT_STATE_ROOT);
        me->SetDisableGravity(false);
        SetCombatMovement(true);
        me->SetRegenerateHealth(true);
        me->RemoveUnitFlag(UNIT_FLAG_UNINTERACTIBLE);
        me->RemoveAurasDueToSpell(SPELL_RESTRICTIVE_TARGETING);
        me->RemoveAurasDueToSpell(SPELL_DEFEATED_MARKER);

        // The template is friendly; retail's pair is hostile from the first sight and friendly once the encounter is over
        if (instance && instance->GetBossState(DATA_THE_MANASTORMS) == DONE)
        {
            me->SetFaction(FACTION_FRIENDLY);
            me->SetReactState(REACT_PASSIVE);
            return;
        }

        me->SetFaction(FACTION_MANASTORMS_HOSTILE);
        me->SetReactState(REACT_AGGRESSIVE);
        if (_isMillhouse)
        {
            if (!me->HasAura(SPELL_CHILLED_HEART))
                me->AddAura(SPELL_CHILLED_HEART, me);
        }
        else if (!me->HasAura(SPELL_MILLIFICENT_PASSIVE))
            me->AddAura(SPELL_MILLIFICENT_PASSIVE, me);
    }

    void JustEngagedWith(Unit* who) override
    {
        BossAI::JustEngagedWith(who);
        if (_defeated || !instance || instance->GetBossState(DATA_THE_MANASTORMS) != IN_PROGRESS)
            return;

        if (ManastormAI* peer = GetPeer())
            if (peer->me->IsAlive() && !peer->_defeated && !peer->me->IsEngaged())
                DoZoneInCombat(peer->me);
        DoZoneInCombat();

        instance->SendEncounterUnit(ENCOUNTER_FRAME_ENGAGE, me, _isMillhouse ? 1 : 2);

        // Millhouse starts on the floor, Millificent's first act is her jump to the rail
        if (_isMillhouse)
        {
            StartFloorPhase();
            ScheduleSwap();
        }
        else
            GoToRail();
    }

    void AttackStart(Unit* who) override
    {
        if (_isMillhouse && !_onRail)
            AttackStartCaster(who, FrostboltChaseRange);
        else
            BossAI::AttackStart(who);
    }

    void JustSummoned(Creature* summon) override
    {
        summons.Summon(summon);
    }

    void OnSpellCast(SpellInfo const* spell) override
    {
        if (spell->Id == SPELL_SUMMON_POWER_CRYSTAL)
            Talk(SAY_MILLHOUSE_POWER);
    }

    void ExecuteEvent(uint32 eventId) override
    {
        switch (eventId)
        {
            case EVENT_FROSTBOLT:
                DoCastVictim(SPELL_FROSTBOLT);
                break;
            case EVENT_FROSTBOLT_RAIL:
                if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0, 100.0f, true))
                    DoCast(target, SPELL_FROSTBOLT);
                break;
            case EVENT_POWER_CRYSTAL:
                DoCastSelf(SPELL_SUMMON_POWER_CRYSTAL);
                break;
            case EVENT_BUZZ_SAW:
                if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0, 100.0f, true))
                    DoCast(target, SPELL_BUZZ_SAW);
                break;
            case EVENT_THROW_BUZZ_SAW:
                if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0, 100.0f, true))
                    DoCast(target, SPELL_THROW_BUZZ_SAW);
                break;
            case EVENT_FLOOR_CYCLE:
                // The survivor has no partner to swap with: its floor phase repeats
                _phaseStartMs = GameTime::GetGameTimeMS();
                ScheduleFloorLadder();
                events.ScheduleEvent(EVENT_FLOOR_CYCLE, PhaseLength);
                break;
            default:
                break;
        }
    }

    // A lethal hit leaves 1 health; the hit that takes the boss to 10% or below lands and defeats it. The defeat itself runs on the next
    // update, outside the damage code, and not on the BossAI scheduler (it would wait for a cast).
    void DamageTaken(Unit* /*attacker*/, uint32& damage, DamageEffectType /*damageType*/, SpellInfo const* /*spellInfo*/) override
    {
        if (_defeated)
        {
            damage = 0;
            return;
        }

        uint32 const health = me->GetHealth();
        if (damage >= health)
            damage = health ? health - 1 : 0;

        if (health - damage > me->CountPctFromMaxHealth(DefeatHealthPct))
            return;

        _defeated = true;
        QueueIfCurrent(1ms, [this]() { OnDefeated(); });
    }

    // Only a death that bypassed the damage clamp (a game master, a script) gets here
    void JustDied(Unit* /*killer*/) override
    {
        _defeated = true;
        events.Reset();
        summons.DespawnAll();

        ManastormAI* peer = GetPeer();
        if (!peer || !peer->me->IsAlive() || peer->_defeated)
            EndEncounter();
        else
            peer->OnPartnerDefeated();
    }

    void UpdateAI(uint32 diff) override
    {
        if (_defeated)
            return;

        BossAI::UpdateAI(diff);
    }

    // A wipe resets both: the defeated one included, and the one on the rail cannot walk home. Both despawn and come back from their
    // spawn points with their addon auras.
    void EnterEvadeMode(EvadeReason /*why*/) override
    {
        if (_evading)
            return;

        _evading = true;
        ManastormAI* peer = GetPeer();
        if (peer)
            peer->_evading = true;

        if (instance)
        {
            instance->SendEncounterUnit(ENCOUNTER_FRAME_DISENGAGE, me);
            if (peer)
                instance->SendEncounterUnit(ENCOUNTER_FRAME_DISENGAGE, peer->me);
            instance->DoRemoveAurasDueToSpellOnPlayers(SPELL_BUZZ_SAW);
        }

        ++_generation;
        summons.DespawnAll();
        _DespawnAtEvade(EvadeRespawnDelay);
        if (peer)
        {
            ++peer->_generation;
            peer->summons.DespawnAll();
            peer->_DespawnAtEvade(EvadeRespawnDelay);
        }
    }

private:
    ManastormAI* GetPeer() const
    {
        if (!instance)
            return nullptr;

        Creature* peer = instance->GetCreature(_isMillhouse ? DATA_MILLIFICENT_MANASTORM : DATA_MILLHOUSE_MANASTORM);
        if (!peer || !peer->IsAIEnabled())
            return nullptr;

        return dynamic_cast<ManastormAI*>(peer->AI());
    }

    // Deferred work that a wipe, a respawn or a defeat since the call must cancel (me->m_Events has no BossAI validator)
    template <typename Callable>
    void QueueIfCurrent(Milliseconds delay, Callable&& callable)
    {
        me->m_Events.AddEventAtOffset([this, generation = _generation, callable = std::forward<Callable>(callable)]()
        {
            if (generation == _generation)
                callable();
        }, delay);
    }

    template <size_t N>
    void ScheduleLadder(std::array<ManastormStep, N> const& ladder)
    {
        for (ManastormStep const& step : ladder)
            events.ScheduleEvent(step.EventId, step.Offset);
    }

    void CastSelf(uint32 spellId)
    {
        if (DoCastSelf(spellId) != SPELL_CAST_OK)
            DoCastSelf(spellId, CastSpellExtraArgs(TRIGGERED_FULL_MASK));
    }

    void ScheduleFloorLadder()
    {
        if (_isMillhouse)
            ScheduleLadder(MillhouseFloorLadder);
        else
            ScheduleLadder(MillificentFloorLadder);
    }

    // Millhouse's clock; a swap queued before a wipe or a defeat does nothing
    void ScheduleSwap()
    {
        QueueIfCurrent(PhaseLength, [this]() { DoSwap(); });
    }

    void DoSwap()
    {
        ManastormAI* peer = GetPeer();
        if (!peer || _defeated || peer->_defeated)
            return;

        if (_onRail)
        {
            peer->GoToRail();
            GoToFloor();
        }
        else
        {
            GoToRail();
            peer->GoToFloor();
        }
        ScheduleSwap();
    }

    void StartFloorPhase()
    {
        _onRail = false;
        _phaseStartMs = GameTime::GetGameTimeMS();
        events.Reset();
        ScheduleFloorLadder();
        if (_solo)
            events.ScheduleEvent(EVENT_FLOOR_CYCLE, PhaseLength);
    }

    void GoToRail()
    {
        _onRail = true;
        events.Reset();
        me->InterruptNonMeleeSpells(false);
        SetCombatMovement(false);
        me->GetMotionMaster()->Clear();
        if (_isMillhouse)
            me->RemoveAurasDueToSpell(SPELL_CHILLED_HEART);
        CastSelf(SPELL_RESTRICTIVE_TARGETING);

        if (_isMillhouse)
        {
            me->SetDisableGravity(true);
            me->SetControlled(true, UNIT_STATE_ROOT);
            QueueIfCurrent(TeleportDelay, [this]()
            {
                if (_onRail)
                    me->NearTeleportTo(MillhouseRail);
            });
            ScheduleLadder(MillhouseRailLadder);
        }
        else
        {
            RocketJump(MillificentRail);
            QueueIfCurrent(RocketJumpTime, [this]()
            {
                if (!_onRail)
                    return;

                me->SetDisableGravity(true);
                me->SetControlled(true, UNIT_STATE_ROOT);
                me->SetFacingTo(me->GetAbsoluteAngle(FloorLanding.GetPositionX(), FloorLanding.GetPositionY()));
            });
            ScheduleLadder(MillificentRailLadder);
        }
    }

    void GoToFloor()
    {
        _onRail = false;
        _phaseStartMs = GameTime::GetGameTimeMS();
        events.Reset();
        me->InterruptNonMeleeSpells(false);
        me->RemoveAurasDueToSpell(SPELL_RESTRICTIVE_TARGETING);
        SetCombatMovement(true);

        if (_isMillhouse)
        {
            // He stays rooted and floating on the rail until the teleport lands: unrooted, his DB waypoint path could start a spline
            QueueIfCurrent(TeleportDelay, [this]()
            {
                if (_onRail)
                    return;

                Position dest = FloorLanding;
                if (Unit* victim = me->GetVictim())
                    dest.SetOrientation(dest.GetAbsoluteAngle(victim->GetPositionX(), victim->GetPositionY()));
                me->NearTeleportTo(dest);
                me->SetDisableGravity(false);
                me->SetControlled(false, UNIT_STATE_ROOT);
                ResumeChase();
            });
            QueueIfCurrent(ChilledHeartDelay, [this]()
            {
                if (!_onRail && !me->HasAura(SPELL_CHILLED_HEART))
                    CastSelf(SPELL_CHILLED_HEART);
            });
        }
        else
        {
            me->SetControlled(false, UNIT_STATE_ROOT);
            me->SetDisableGravity(false);
            me->GetMotionMaster()->Clear();
            RocketJump(FloorLanding);
            QueueIfCurrent(RocketJumpTime, [this]() { ResumeChase(); });
        }

        ScheduleFloorLadder();
        if (_solo)
            events.ScheduleEvent(EVENT_FLOOR_CYCLE, PhaseLength);
    }

    void RocketJump(Position const& dest)
    {
        CastSelf(SPELL_ROCKET_JUMP_AURA);

        Movement::SpellEffectExtraData visual;
        visual.SpellVisualId = SPELL_VISUAL_ROCKET_JUMP;
        me->GetMotionMaster()->MoveJump(POINT_ROCKET_JUMP, dest, RocketJumpTime, {}, {}, {}, false, false, {}, nullptr, &visual);
    }

    void ResumeChase()
    {
        if (_defeated || _onRail)
            return;

        if (Unit* victim = me->GetVictim())
        {
            me->GetMotionMaster()->Clear();
            if (_isMillhouse)
                me->GetMotionMaster()->MoveChase(victim, FrostboltChaseRange);
            else
                me->GetMotionMaster()->MoveChase(victim);
        }
    }

    // The boss leaves the fight: friendly, out of combat, the lethal hit's 1 health kept
    void OnDefeated()
    {
        ManastormAI* peer = GetPeer();
        bool const last = !peer || peer->_defeated || !peer->me->IsAlive();

        events.Reset();
        me->InterruptNonMeleeSpells(false);
        me->RemoveAurasDueToSpell(SPELL_CHILLED_HEART);
        me->RemoveAurasDueToSpell(SPELL_RESTRICTIVE_TARGETING);
        me->SetRegenerateHealth(false);
        me->SetReactState(REACT_PASSIVE);

        if (last)
        {
            // The capture: the second boss turns friendly with the encounter's end one second after its last hit
            QueueIfCurrent(EndDelay, [this]() { EndEncounter(); });
            return;
        }

        if (instance)
            instance->SendEncounterUnit(ENCOUNTER_FRAME_DISENGAGE, me);
        BecomeFriendly();
        me->SetUnitFlag(UNIT_FLAG_UNINTERACTIBLE);
        CastSelf(SPELL_DEFEATED_MARKER);

        if (!_isMillhouse)
        {
            QueueIfCurrent(DefeatYellDelay, [this]() { Talk(SAY_MILLIFICENT_DEFEATED); });
            QueueIfCurrent(KneelDelay, [this]() { me->SetStandState(UNIT_STAND_STATE_KNEEL); });
        }

        peer->OnPartnerDefeated();
    }

    void BecomeFriendly()
    {
        me->SetFaction(FACTION_FRIENDLY);
        me->SetReactState(REACT_PASSIVE);
        me->AttackStop();
        me->CombatStop(false);
        EngagementOver();
    }

    // The other boss is defeated: the clock stops and this one comes down to fight on alone
    void OnPartnerDefeated()
    {
        _solo = true;
        if (_onRail)
        {
            GoToFloor();
            return;
        }

        uint32 const elapsed = GameTime::GetGameTimeMS() - _phaseStartMs;
        Milliseconds const remaining = elapsed < PhaseLength.count() ? PhaseLength - Milliseconds(elapsed) : 1ms;
        events.ScheduleEvent(EVENT_FLOOR_CYCLE, remaining);
    }

    // Capture order (all in one second): the markers, ENCOUNTER_END success, the hatch, the chest and the conversation
    void EndEncounter()
    {
        if (!instance || instance->GetBossState(DATA_THE_MANASTORMS) == DONE)
            return;

        ManastormAI* peer = GetPeer();
        for (ManastormAI* ai : { this, peer })
        {
            if (!ai)
                continue;

            ai->_defeated = true;
            ai->events.Reset();
            ai->summons.DespawnAll();
            ai->me->InterruptNonMeleeSpells(false);
            ai->me->RemoveUnitFlag(UNIT_FLAG_UNINTERACTIBLE);
            ai->me->RemoveAurasDueToSpell(SPELL_CHILLED_HEART);
            ai->me->RemoveAurasDueToSpell(SPELL_RESTRICTIVE_TARGETING);
            if (ai->me->IsAlive())
            {
                instance->SendEncounterUnit(ENCOUNTER_FRAME_DISENGAGE, ai->me);
                ai->BecomeFriendly();
                if (!ai->me->HasAura(SPELL_DEFEATED_MARKER))
                    ai->CastSelf(SPELL_DEFEATED_MARKER);
            }
        }

        instance->SetBossState(DATA_THE_MANASTORMS, DONE);
        instance->DoRemoveAurasDueToSpellOnPlayers(SPELL_BUZZ_SAW);
        SummonChest();
        Conversation::CreateConversation(CONVERSATION_MANASTORMS_END, me, me->GetPosition(), ObjectGuid::Empty);

        // The helper leaves seven seconds later
        QueueIfCurrentOrNot(HelperDespawnDelay, [this]()
        {
            if (Creature* helper = me->FindNearestCreature(NPC_MANASTORM_HELPER, 100.0f))
                helper->DespawnOrUnsummon(0ms, 24h);
        });
    }

    // The end's follow-ups run after _defeated is set, so they cannot depend on the generation of a wipe that no longer can happen
    template <typename Callable>
    void QueueIfCurrentOrNot(Milliseconds delay, Callable&& callable)
    {
        me->m_Events.AddEventAtOffset(std::forward<Callable>(callable), delay);
    }

    // The core makes the chest's personal loot at its first use for everyone in the tap list
    void SummonChest()
    {
        GameObject* chest = me->SummonGameObject(GO_GNOMISH_MACHINATIONS, ChestPos, QuaternionData::fromEulerAnglesZYX(ChestPos.GetOrientation(), 0.0f, 0.0f), 1h, GO_SUMMON_TIMED_DESPAWN);
        if (!chest)
            return;

        GuidUnorderedSet tapList;
        me->GetMap()->DoOnPlayers([&tapList](Player* player)
        {
            tapList.insert(player->GetGUID());
        });
        chest->SetTapList(std::move(tapList));
    }

    bool const _isMillhouse;
    uint32 _generation;
    bool _defeated;
    bool _onRail;
    bool _solo;
    bool _evading;
    uint32 _phaseStartMs;
};

// 164555 - Millificent Manastorm
struct boss_millificent_manastorm : public ManastormAI
{
    boss_millificent_manastorm(Creature* creature) : ManastormAI(creature, false) { }
};

// 164556 - Millhouse Manastorm (the Arcatraz one is npc_millhouse_manastorm)
struct boss_millhouse_manastorm_dos : public ManastormAI
{
    boss_millhouse_manastorm_dos(Creature* creature) : ManastormAI(creature, true) { }
};

// 164934 - Manastorm: the helper at the arena's centre does nothing in the fight
struct npc_dos_manastorm_helper : public NullCreatureAI
{
    using NullCreatureAI::NullCreatureAI;
};

// 355436 - Crate of Gnomish Machinations: the core makes the personal loot at the first use for the tap list. A player left out of it
// (an empty roll) would make the core rebuild and replace everyone's loot, so only a use before the first or by a player with loot
// goes through (the same as Kaal's Armaments).
struct go_dos_gnomish_machinations : public GameObjectAI
{
    using GameObjectAI::GameObjectAI;

    bool OnGossipHello(Player* player) override
    {
        return !me->m_personalLoot.empty() && !me->m_personalLoot.contains(player->GetGUID());
    }
};

// 320144 - Buzz-Saw: the aura on the charged player drops a Buzz-Saw hit 320145 from Millificent every 500 ms (the capture shows six hits
// over the aura's 2 s; Bleeding 320147 follows on only two of them and its rule is not known, so it is not applied)
class aura_dos_buzz_saw : public AuraScript
{
    void HandlePeriodic(AuraEffect const* /*aurEff*/)
    {
        if (Unit* caster = GetCaster())
            caster->CastSpell(GetTarget(), SPELL_BUZZ_SAW_HIT, CastSpellExtraArgs(TRIGGERED_FULL_MASK));
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(aura_dos_buzz_saw::HandlePeriodic, EFFECT_1, SPELL_AURA_PERIODIC_DUMMY);
    }
};
}

void AddSC_boss_the_manastorms()
{
    RegisterDeOtherSideCreatureAI(boss_millificent_manastorm);
    RegisterDeOtherSideCreatureAI(boss_millhouse_manastorm_dos);
    RegisterCreatureAI(npc_dos_manastorm_helper);
    RegisterGameObjectAI(go_dos_gnomish_machinations);
    RegisterSpellScript(aura_dos_buzz_saw);
}
