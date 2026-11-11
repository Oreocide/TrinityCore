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
 * Kryxis the Voracious (DungeonEncounter 2360). One phase, no health gates
 * (trinitycore-tracking/sl-dungeons/sanguine-depths/bosses/kryxis-the-voracious.md). Offsets are Warcraft Logs medians (24 Mythic+
 * kill pulls) and agree with the 2026-11-10 Heroic capture within its 1 s clock step.
 *
 * The loop is anchored on the start of Severing Smash: Vicious Headbutt +12.1 s, Hungering Drain +17.0 s, Juggernaut Rush +21.9 s,
 * and the next Smash 38.1 s after the last. The pull behaves as if a Smash had started 6.1 s before it, and the first Smash comes at
 * 35.1 s. Smash is timer driven; the Periodic Energy Gain aura only fills the energy bar (+3 per second, back to 0 when the Smash
 * finishes).
 *
 * Second slot of a cycle (Smash +32 s): the logs show either a Headbutt at +32.0 (16 of 116 cycles) or a second Drain at +32.8
 * (86 of 116), never both, and the Heroic capture, where nobody interrupted, has the Headbutt. Medium: the second Drain is cast only
 * when the first one was interrupted, which fits both (the logs come from Mythic+ groups that interrupt; the journal tells the
 * tank and the damage dealers to interrupt). Low: whether an interrupted Drain really restarts the loop at +15.8 s.
 *
 * Juggernaut Rush: the 4 s cast is aimed at a random player (one solo capture, Medium). At its end Kryxis hits everything on the
 * line to the player's position (damage split between all players hit, journal) and charges to 5 yd short of that spot (319718's
 * destination in the capture, 5.0 yd from 319715's). The E2 dummy of 319715 has no handler in the core and no known purpose.
 *
 * Living Essence: Severing Smash leaves one orb at each player's position, which homes on Kryxis at 3 yd/s. When it reaches him
 * he is healed for 12% and gains Essence Surge; a player who touches it first takes 319687 (journal: "if reabsorbed by a player,
 * it inflicts Shadow damage to them"). The orbs are created on the players, so touching is armed 1.5 s after the spawn; the delay is
 * an inference (nobody touched an orb in the capture). Mythic only and left out: the pickup damage hits all players.
 *
 * Not scripted: Call for Help 338471 (WCL shows one cast in 24 pulls, the Heroic capture none).
 */

#include "AreaTrigger.h"
#include "AreaTriggerAI.h"
#include "G3D/Vector3.h"
#include "InstanceScript.h"
#include "ObjectAccessor.h"
#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "Spell.h"
#include "SpellInfo.h"
#include "SpellScript.h"
#include "sanguine_depths.h"

enum KryxisSpells
{
    SPELL_VICIOUS_HEADBUTT              = 319650,   // 1.5 s cast, tank hit and knock back
    SPELL_HUNGERING_DRAIN               = 319654,   // 3 s channel, interruptible; ticks 319655 every second, first tick at once
    SPELL_HUNGERING_DRAIN_TICK          = 319655,   // raid damage
    SPELL_ESSENCE_SURGE                 = 319657,   // +2% damage per stack, 8 s each
    SPELL_PERIODIC_ENERGY_GAIN          = 319661,   // +3 energy per second
    SPELL_LIVING_ESSENCE                = 319684,   // creates the orb areatrigger at a destination
    SPELL_SEVERING_SMASH                = 319685,   // 4 s cast, raid damage and knock back
    SPELL_ESSENCE_REABSORBED            = 319687,   // Shadow damage to the player who takes an orb
    SPELL_ESSENCE_ABSORPTION            = 319688,   // heals Kryxis for 12%
    SPELL_JUGGERNAUT_RUSH               = 319713,   // 4 s cast on a player, dummy effect
    SPELL_JUGGERNAUT_RUSH_DAMAGE        = 319715,   // rectangle from Kryxis to a destination
    SPELL_JUGGERNAUT_RUSH_CHARGE        = 319718    // charge to a destination
};

enum KryxisEvents
{
    EVENT_VICIOUS_HEADBUTT = 1,
    EVENT_HUNGERING_DRAIN,
    EVENT_JUGGERNAUT_RUSH,
    EVENT_SECOND_SLOT,
    EVENT_SEVERING_SMASH
};

enum Texts
{
    SAY_JUGGERNAUT_RUSH                 = 0     // raid boss emote naming the target
};

static constexpr Milliseconds HeadbuttAfterSmash = 12100ms;
static constexpr Milliseconds DrainAfterSmash = 17000ms;
static constexpr Milliseconds RushAfterSmash = 21900ms;
static constexpr Milliseconds SecondSlotAfterSmash = 32000ms;
static constexpr Milliseconds SecondDrainAfterSlot = 800ms;
static constexpr Milliseconds PullIntoCycle = 6100ms;
static constexpr Milliseconds FirstSmash = 35100ms;
static constexpr Milliseconds SmashInterval = 38100ms;
static constexpr Milliseconds CastRetry = 500ms;
static constexpr float RushRange = 100.0f;
static constexpr float RushStopDistance = 5.0f;
static constexpr float RushMinCharge = 0.5f;

static constexpr Milliseconds OrbArmDelay = 1500ms;     // Medium: inferred, see the header
static constexpr Milliseconds OrbRestTime = 2s;         // capture: the orb stays at the player's spot for 2.0 s, then its first spline
static constexpr Milliseconds OrbRepathInterval = 250ms;
static constexpr float OrbAbsorbReach = 2.5f;           // the orb's sphere; capture: absorbed about 1.6 yd from Kryxis, 4.0 s after creation

// 162100 - Kryxis the Voracious
struct boss_kryxis_the_voracious : public BossAI
{
    boss_kryxis_the_voracious(Creature* creature) : BossAI(creature, DATA_KRYXIS_THE_VORACIOUS) { }

    void Reset() override
    {
        BossAI::Reset();
        _drainFinished = true;
        me->SetPower(POWER_ENERGY, 0);
        Cleanup();
    }

    void JustEngagedWith(Unit* who) override
    {
        BossAI::JustEngagedWith(who);
        me->SetPower(POWER_ENERGY, 0);
        DoCastSelf(SPELL_PERIODIC_ENERGY_GAIN, true);
    }

    void ScheduleTasks() override
    {
        events.ScheduleEvent(EVENT_SEVERING_SMASH, FirstSmash);
        ScheduleCycle(PullIntoCycle);
    }

    void JustDied(Unit* killer) override
    {
        Cleanup();
        BossAI::JustDied(killer);
    }

    // Retail despawns Kryxis at the wipe and brings him back 30 s later at his spawn point
    void EnterEvadeMode(EvadeReason why) override
    {
        Cleanup();
        _EnterEvadeMode(why);
        _DespawnAtEvade();
    }

    void OnSpellCast(SpellInfo const* spell) override
    {
        if (spell->Id == SPELL_SEVERING_SMASH)
            me->SetPower(POWER_ENERGY, 0);
    }

    // Only a completed channel gets here, an interrupted Drain does not
    void OnChannelFinished(SpellInfo const* spell) override
    {
        if (spell->Id == SPELL_HUNGERING_DRAIN)
            _drainFinished = true;
    }

    void ExecuteEvent(uint32 eventId) override
    {
        switch (eventId)
        {
            case EVENT_VICIOUS_HEADBUTT:
                if (DoCastVictim(SPELL_VICIOUS_HEADBUTT) != SPELL_CAST_OK)
                    events.Repeat(CastRetry);
                break;
            case EVENT_HUNGERING_DRAIN:
                if (DoCastSelf(SPELL_HUNGERING_DRAIN) != SPELL_CAST_OK)
                    events.Repeat(CastRetry);
                else
                    _drainFinished = false;
                break;
            case EVENT_JUGGERNAUT_RUSH:
            {
                Unit* target = SelectTarget(SelectTargetMethod::Random, 0, RushRange, true);
                if (target && DoCast(target, SPELL_JUGGERNAUT_RUSH) == SPELL_CAST_OK)
                    Talk(SAY_JUGGERNAUT_RUSH, target);
                else
                    events.Repeat(CastRetry);
                break;
            }
            case EVENT_SECOND_SLOT:
                if (_drainFinished)
                    events.ScheduleEvent(EVENT_VICIOUS_HEADBUTT, 0ms);
                else
                    events.ScheduleEvent(EVENT_HUNGERING_DRAIN, SecondDrainAfterSlot);
                break;
            case EVENT_SEVERING_SMASH:
                if (DoCastAOE(SPELL_SEVERING_SMASH) != SPELL_CAST_OK)
                {
                    events.Repeat(CastRetry);
                    break;
                }
                CancelCycle();
                ScheduleCycle(0ms);
                events.Repeat(SmashInterval);
                break;
            default:
                break;
        }
    }

private:
    // origin is how far into the cycle the fight already is: the offsets are from the start of a Smash
    void ScheduleCycle(Milliseconds origin)
    {
        events.ScheduleEvent(EVENT_VICIOUS_HEADBUTT, HeadbuttAfterSmash - origin);
        events.ScheduleEvent(EVENT_HUNGERING_DRAIN, DrainAfterSmash - origin);
        events.ScheduleEvent(EVENT_JUGGERNAUT_RUSH, RushAfterSmash - origin);
        events.ScheduleEvent(EVENT_SECOND_SLOT, SecondSlotAfterSmash - origin);
    }

    void CancelCycle()
    {
        events.CancelEvent(EVENT_VICIOUS_HEADBUTT);
        events.CancelEvent(EVENT_HUNGERING_DRAIN);
        events.CancelEvent(EVENT_JUGGERNAUT_RUSH);
        events.CancelEvent(EVENT_SECOND_SLOT);
    }

    void Cleanup()
    {
        me->RemoveAllAreaTriggers();
        me->RemoveAurasDueToSpell(SPELL_PERIODIC_ENERGY_GAIN);
    }

    bool _drainFinished = true;
};

// 319655 - Hungering Drain: every tick gives Kryxis one stack of Essence Surge, however many players the tick hits
class spell_sd_kryxis_hungering_drain_tick : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_ESSENCE_SURGE });
    }

    void HandleSurge()
    {
        Unit* caster = GetCaster();
        caster->CastSpell(caster, SPELL_ESSENCE_SURGE, CastSpellExtraArgs(TRIGGERED_FULL_MASK));
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_sd_kryxis_hungering_drain_tick::HandleSurge);
    }
};

// 319685 - Severing Smash: an orb of Living Essence is left where each player was hit
class spell_sd_kryxis_severing_smash : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_LIVING_ESSENCE });
    }

    void HandleOrb(SpellEffIndex /*effIndex*/)
    {
        Unit* target = GetHitUnit();
        if (!target || !target->IsPlayer())
            return;

        // The orb knows the player it split off from: that player does not collect it (see the areatrigger)
        CastSpellExtraArgs args(TRIGGERED_FULL_MASK);
        args.SetCustomArg(target->GetGUID());
        GetCaster()->CastSpell(target->GetPosition(), SPELL_LIVING_ESSENCE, args);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_sd_kryxis_severing_smash::HandleOrb, EFFECT_0, SPELL_EFFECT_SCHOOL_DAMAGE);
    }
};

// 319713 - Juggernaut Rush: when the cast ends the rectangle spell goes to the target's position
class spell_sd_kryxis_juggernaut_rush : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_JUGGERNAUT_RUSH_DAMAGE });
    }

    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        if (Unit* target = GetHitUnit())
            GetCaster()->CastSpell(target->GetPosition(), SPELL_JUGGERNAUT_RUSH_DAMAGE, CastSpellExtraArgs(TRIGGERED_FULL_MASK));
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_sd_kryxis_juggernaut_rush::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 319715 - Juggernaut Rush: the damage is split between every player hit; then Kryxis charges to the spot 5 yd short of the target.
// Do not also list this spell in spell_custom_attr with SHARE_DAMAGE, the division would happen twice.
class spell_sd_kryxis_juggernaut_rush_damage : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_JUGGERNAUT_RUSH_CHARGE });
    }

    void SplitDamage(SpellEffIndex /*effIndex*/)
    {
        if (int64 count = GetUnitTargetCountForEffect(EFFECT_0); count > 1)
            SetHitDamage(GetHitDamage() / count);
    }

    void HandleCharge()
    {
        WorldLocation const* dest = GetExplTargetDest();
        if (!dest)
            return;

        Unit* caster = GetCaster();
        float distance = caster->GetExactDist2d(dest) - RushStopDistance;
        if (distance < RushMinCharge)
            return;

        float angle = caster->GetAbsoluteAngle(dest->GetPositionX(), dest->GetPositionY());
        Position stop(caster->GetPositionX() + distance * std::cos(angle), caster->GetPositionY() + distance * std::sin(angle), dest->GetPositionZ(), angle);
        caster->CastSpell(stop, SPELL_JUGGERNAUT_RUSH_CHARGE, CastSpellExtraArgs(TRIGGERED_FULL_MASK));
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_sd_kryxis_juggernaut_rush_damage::SplitDamage, EFFECT_0, SPELL_EFFECT_SCHOOL_DAMAGE);
        AfterCast += SpellCastFn(spell_sd_kryxis_juggernaut_rush_damage::HandleCharge);
    }
};

// 319684 areatrigger (create properties 19353, areatrigger 23347, sphere 2.5, speed 3 yd/s). Capture (the one Heroic orb): it rests
// at the player's spot for 2.0 s, then homes on Kryxis (OrbitPathTarget) at 3.0 yd/s, re-aiming as he moves, and is absorbed at
// +4.0 s about 1.6 yd from him. The player it split off from stood next to it and its path and did not take it.
struct at_sd_kryxis_living_essence : AreaTriggerAI
{
    using AreaTriggerAI::AreaTriggerAI;

    void OnCreate(Spell const* creatingSpell) override
    {
        if (Unit* caster = at->GetCaster())
            at->SetPathTarget(caster->GetGUID());

        if (creatingSpell)
            if (ObjectGuid const* source = std::any_cast<ObjectGuid>(&creatingSpell->m_customArg))
                _source = *source;
    }

    // Journal: a player who touches the orb first collects it. Medium: the one capture is solo, and there the orb's own player
    // (standing at its edge, then beside its path) never collected it, so that player is left out; anyone else who touches an
    // armed orb, or whom it runs into, takes it.
    void OnUnitEnter(Unit* unit) override
    {
        if (_done || _armTimer > 0 || !unit->IsPlayer() || !unit->IsAlive() || unit->GetGUID() == _source)
            return;

        if (Unit* caster = at->GetCaster())
            caster->CastSpell(unit, SPELL_ESSENCE_REABSORBED, CastSpellExtraArgs(TRIGGERED_FULL_MASK));
        Finish();
    }

    void OnUpdate(uint32 diff) override
    {
        if (_done)
            return;

        Unit* caster = at->GetCaster();
        if (!caster || !caster->IsAlive())
        {
            Finish();
            return;
        }

        if (_armTimer > 0)
            _armTimer = diff >= _armTimer ? 0 : _armTimer - diff;

        if (_restTimer > 0)
        {
            _restTimer = diff >= _restTimer ? 0 : _restTimer - diff;
            if (_restTimer > 0)
                return;

            UpdateMovement(caster);
            _repathTimer = uint32(OrbRepathInterval.count());
            return;
        }

        if (at->GetExactDist2d(caster) <= OrbAbsorbReach)
        {
            caster->CastSpell(caster, SPELL_ESSENCE_ABSORPTION, CastSpellExtraArgs(TRIGGERED_FULL_MASK));
            caster->CastSpell(caster, SPELL_ESSENCE_SURGE, CastSpellExtraArgs(TRIGGERED_FULL_MASK));
            Finish();
            return;
        }

        if (_repathTimer > diff)
            _repathTimer -= diff;
        else
        {
            _repathTimer = uint32(OrbRepathInterval.count());
            UpdateMovement(caster);
        }
    }

private:
    void UpdateMovement(Unit const* caster)
    {
        std::vector<G3D::Vector3> path;
        path.emplace_back(at->GetPositionX(), at->GetPositionY(), at->GetPositionZ());
        path.emplace_back(caster->GetPositionX(), caster->GetPositionY(), caster->GetPositionZ());
        at->InitSplines(path);
    }

    void Finish()
    {
        _done = true;
        at->Remove();
    }

    ObjectGuid _source;
    uint32 _armTimer = uint32(OrbArmDelay.count());
    uint32 _restTimer = uint32(OrbRestTime.count());
    uint32 _repathTimer = uint32(OrbRepathInterval.count());
    bool _done = false;
};

void AddSC_boss_kryxis_the_voracious()
{
    RegisterSanguineDepthsCreatureAI(boss_kryxis_the_voracious);
    RegisterSpellScript(spell_sd_kryxis_hungering_drain_tick);
    RegisterSpellScript(spell_sd_kryxis_severing_smash);
    RegisterSpellScript(spell_sd_kryxis_juggernaut_rush);
    RegisterSpellScript(spell_sd_kryxis_juggernaut_rush_damage);
    RegisterAreaTriggerAI(at_sd_kryxis_living_essence);
}
