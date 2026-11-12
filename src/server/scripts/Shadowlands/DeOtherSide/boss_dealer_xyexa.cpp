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
 * Dealer Xy'exa (DungeonEncounter 2400, trinitycore-tracking/sl-dungeons/de-other-side/bosses/dealer-xyexa.md). One phase, no health
 * gates, a melee boss with an energy bar. Timers are Heroic: the 2026-11-10 capture (1 s clock steps), Warcraft Logs Mythic-0 medians
 * and DBM.
 *
 * Energy bar (aura 323812 in the creature addon, display power 453): about +3.3 per second from 0 at the pull. The cycle in the
 * capture is 35 s: the bar reads 100 at 31 s and 66 s, Explosive Contrivance starts at the same second, and the bar drops to 0 when
 * the cast completes. A tick that finds the bar already full starts the cast (Medium: Warcraft Logs has the first begin cast at 31.5 s
 * and the second at 66.7 s).
 *
 * Displacement Trap: the cast hits every player (Journal: "everyone in the group"), each gets a trap at their feet 2 s later (missile).
 * A player who steps on a trap casts Displacement Trap 334199 on themselves (transform, fly, no actions, disarm; 1 s) and gets
 * Contrivance Protection 345518 (4 s), and is thrown 25 yd up in 1.0 s (capture spline); the jump effect of 334199 has no destination
 * rule in its data, so the lift is a MoveJump here. The explosion slabs are 6 yd high at the floor, so an airborne player is outside.
 * Medium: players holding 345518 are skipped by both explosions even once they are back on the floor (the aura is named "Contrivance
 * Protection" and DBM says to take the trap for both explosions; the capture's only hit came after the aura had expired).
 *
 * Displaced Blastwave: 324090 -> 320326 summons an Invis Stalker 164760 in front of the boss and one stalker 164753 on every player
 * (320327); the stalkers fire the damage rectangles 1 s and 3 s later.
 *
 * Localized Explosive Contrivance: a 5 s mark on one player; when it expires a slab is created at the player's position.
 *
 * Not scripted:
 * - The Blastwave displacement. The effect data holds none (320726 has a knock back and a cone but nothing casts it in the capture,
 *   and the capture shows no movement of the player), and the orientation of the rectangles is not known.
 * - Arcane Lightning 323687 / Arcane Vulnerability 323692 (Mythic only).
 * - The airborne marker areatrigger 320183 (create properties 1409): the capture creates it 28 yd above the player with no known role.
 * - Achievement Credit 335978 at the death (no effect handler; the credit goes through the encounter and the scenario criteria).
 */

#include "AreaTrigger.h"
#include "AreaTriggerAI.h"
#include "InstanceScript.h"
#include "MotionMaster.h"
#include "PassiveAI.h"
#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "SpellScript.h"
#include "TaskScheduler.h"
#include "de_other_side.h"
#include <algorithm>
#include <iterator>
#include <list>

enum XyexaSpells
{
    SPELL_PLAYER_TESTER                     = 324007,   // applied at the pull, kept to the death; ticks 324006 (threat on everyone within 60 yd)
    SPELL_DISPLACEMENT_TRAP                 = 319619,   // instant, dummy on every player within 100 yd
    SPELL_DISPLACEMENT_TRAP_MISSILE         = 319627,   // missile (2 yd/s) that creates the trap areatrigger at the target
    SPELL_DISPLACEMENT_TRAP_PLAYER          = 334199,   // cast by the player on itself, 1 s
    SPELL_CONTRIVANCE_PROTECTION            = 345518,   // 4 s marker aura on the player
    SPELL_DISPLACED_BLASTWAVE               = 324090,   // instant, dummy
    SPELL_DISPLACED_BLASTWAVE_SUMMON        = 320326,   // dummy on every player + summons the stalker 164760
    SPELL_DISPLACED_BLASTWAVE_TARGET_SUMMON = 320327,   // summons the stalker 164753
    SPELL_BLASTWAVE_FRONT                   = 320730,   // stalker 164760, missile -> damage rectangle
    SPELL_BLASTWAVE_TARGET                  = 320329,   // stalker 164753, missile -> damage rectangle
    SPELL_LOCALIZED_EXPLOSIVE_CONTRIVANCE   = 321948,   // 5 s mark on a player
    SPELL_LOCALIZED_EXPLOSIVE_SLAB          = 342962,   // creates the slab areatrigger at a destination
    SPELL_LOCALIZED_EXPLOSION               = 342961,   // damage + 60 s DoT
    SPELL_EXPLOSIVE_CONTRIVANCE             = 320230,   // 5.0 s cast, creates the slab areatrigger at the boss
    SPELL_EXPLOSIVE_CONTRIVANCE_DAMAGE      = 320232    // damage + 60 s DoT
};

enum XyexaEvents
{
    EVENT_ENERGY_TICK = 1,
    EVENT_DISPLACEMENT_TRAP,
    EVENT_DISPLACED_BLASTWAVE,
    EVENT_LOCALIZED_EXPLOSIVE_CONTRIVANCE
};

enum XyexaCreatures
{
    NPC_BLASTWAVE_STALKER_FRONT             = 164760,
    NPC_BLASTWAVE_STALKER_TARGET            = 164753
};

enum Texts
{
    SAY_AGGRO                               = 0,
    SAY_TRAP_ENTERTAINING                   = 1,    // trap casts 1 and 4
    SAY_AWAY_WITH_YOU                       = 2,    // Blastwave
    WHISPER_LOCALIZED_EXPLOSIVE             = 3,    // raid boss whisper to the marked player
    SAY_HOLD_THIS                           = 4,    // Localized Explosive casts 1 and 3
    SAY_TRAP_FLY                            = 5,    // trap casts 2 and 5
    EMOTE_EXPLOSIVE_CONTRIVANCE             = 6,    // raid boss emote
    SAY_EXPLOSIVE_CONTRIVANCE               = 7,
    SAY_TRAP_STEP_LIGHTLY                   = 8,    // trap cast 3
    SAY_TRY_THIS_ONE                        = 9,    // Localized Explosive casts 2 and 4
    SAY_WHERE_TO_PUT_YOU                    = 10,   // Blastwave cast 4
    SAY_DEATH                               = 11
};

static constexpr uint8 TrapTexts[] = { SAY_TRAP_ENTERTAINING, SAY_TRAP_FLY, SAY_TRAP_STEP_LIGHTLY };
static constexpr uint8 LocalizedTexts[] = { SAY_HOLD_THIS, SAY_TRY_THIS_ONE };

static constexpr Milliseconds FirstDisplacementTrap = 5s;
static constexpr Milliseconds DisplacementTrapMin = 17s;
static constexpr Milliseconds DisplacementTrapMax = 19s;
static constexpr Milliseconds FirstBlastwave = 9500ms;
static constexpr Milliseconds BlastwaveMin = 17s;
static constexpr Milliseconds BlastwaveMax = 19400ms;
static constexpr Milliseconds FirstLocalizedExplosive = 14300ms;    // Medium-High: two casts in the capture, DBM 14.3 s
static constexpr Milliseconds LocalizedExplosiveInterval = 35200ms;
static constexpr Milliseconds EnergyTick = 1s;
static constexpr Milliseconds CastRetry = 500ms;
static constexpr uint32 EnergyTicksToFull = 30;
static constexpr float PlayerSearchRange = 100.0f;

static constexpr Milliseconds TrapLiftTime = 1000ms;
static constexpr float TrapLiftHeight = 25.0f;
static constexpr Milliseconds StalkerFrontDelay = 1s;
static constexpr Milliseconds StalkerTargetDelay = 3s;

// Auras the boss leaves on the party (the Contrivance DoTs last 60 s)
static constexpr uint32 PartyAuras[] =
{
    SPELL_DISPLACEMENT_TRAP_PLAYER,
    SPELL_CONTRIVANCE_PROTECTION,
    SPELL_LOCALIZED_EXPLOSIVE_CONTRIVANCE,
    SPELL_LOCALIZED_EXPLOSION,
    SPELL_EXPLOSIVE_CONTRIVANCE_DAMAGE
};

// 164450 - Dealer Xy'exa
struct boss_dealer_xyexa : public BossAI
{
    boss_dealer_xyexa(Creature* creature) : BossAI(creature, DATA_DEALER_XYEXA) { }

    void Reset() override
    {
        BossAI::Reset();
        ResetEnergy();
        _trapCasts = 0;
        _blastwaveCasts = 0;
        _localizedCasts = 0;
        Cleanup();
    }

    void JustEngagedWith(Unit* who) override
    {
        BossAI::JustEngagedWith(who);
        Talk(SAY_AGGRO);
        DoCastSelf(SPELL_PLAYER_TESTER);
        ResetEnergy();
    }

    void ScheduleTasks() override
    {
        events.ScheduleEvent(EVENT_ENERGY_TICK, EnergyTick);
        events.ScheduleEvent(EVENT_DISPLACEMENT_TRAP, FirstDisplacementTrap);
        events.ScheduleEvent(EVENT_DISPLACED_BLASTWAVE, FirstBlastwave);
        events.ScheduleEvent(EVENT_LOCALIZED_EXPLOSIVE_CONTRIVANCE, FirstLocalizedExplosive);
    }

    void JustDied(Unit* killer) override
    {
        Cleanup();
        Talk(SAY_DEATH);
        BossAI::JustDied(killer);
    }

    // The stalkers are not combat adds: they only fire their rectangle
    void JustSummoned(Creature* summon) override
    {
        summons.Summon(summon);
    }

    // The bar drops to 0 when the cast completes
    void OnSpellCast(SpellInfo const* spell) override
    {
        if (spell->Id == SPELL_EXPLOSIVE_CONTRIVANCE)
            ResetEnergy();
    }

    void ExecuteEvent(uint32 eventId) override
    {
        switch (eventId)
        {
            case EVENT_ENERGY_TICK:
                events.Repeat(EnergyTick);
                if (_energyTicks >= EnergyTicksToFull)
                {
                    if (DoCastAOE(SPELL_EXPLOSIVE_CONTRIVANCE) == SPELL_CAST_OK)
                    {
                        Talk(EMOTE_EXPLOSIVE_CONTRIVANCE);
                        Talk(SAY_EXPLOSIVE_CONTRIVANCE);
                    }
                    break;
                }
                ++_energyTicks;
                me->SetPower(POWER_ENERGY, std::min<int32>(100, int32(_energyTicks * 10 / 3)));
                break;
            case EVENT_DISPLACEMENT_TRAP:
                if (DoCastAOE(SPELL_DISPLACEMENT_TRAP) != SPELL_CAST_OK)
                {
                    events.Repeat(CastRetry);
                    break;
                }
                Talk(TrapTexts[_trapCasts++ % std::size(TrapTexts)]);
                events.Repeat(randtime(DisplacementTrapMin, DisplacementTrapMax));
                break;
            case EVENT_DISPLACED_BLASTWAVE:
                if (DoCastAOE(SPELL_DISPLACED_BLASTWAVE) != SPELL_CAST_OK)
                {
                    events.Repeat(CastRetry);
                    break;
                }
                // Low: the capture has "Away with you." three times, then "Now, where to put you?" once
                Talk(_blastwaveCasts++ % 4 == 3 ? SAY_WHERE_TO_PUT_YOU : SAY_AWAY_WITH_YOU);
                events.Repeat(randtime(BlastwaveMin, BlastwaveMax));
                break;
            case EVENT_LOCALIZED_EXPLOSIVE_CONTRIVANCE:
            {
                // Medium: the target rule is not in the data; a random player (the capture is solo)
                Unit* target = SelectTarget(SelectTargetMethod::Random, 0, PlayerSearchRange, true);
                if (!target || DoCast(target, SPELL_LOCALIZED_EXPLOSIVE_CONTRIVANCE) != SPELL_CAST_OK)
                {
                    events.Repeat(CastRetry);
                    break;
                }
                Talk(WHISPER_LOCALIZED_EXPLOSIVE, target);
                Talk(LocalizedTexts[_localizedCasts++ % std::size(LocalizedTexts)]);
                events.Repeat(LocalizedExplosiveInterval);
                break;
            }
            default:
                break;
        }
    }

private:
    void ResetEnergy()
    {
        _energyTicks = 0;
        me->SetPower(POWER_ENERGY, 0);
    }

    void Cleanup()
    {
        me->RemoveAllAreaTriggers();
        me->RemoveAurasDueToSpell(SPELL_PLAYER_TESTER);
        // The boss may be immune to players' side effects, so the auras are removed directly
        if (instance)
            for (uint32 spellId : PartyAuras)
                instance->DoRemoveAurasDueToSpellOnPlayers(spellId);
    }

    uint32 _energyTicks = 0;
    uint32 _trapCasts = 0;
    uint32 _blastwaveCasts = 0;
    uint32 _localizedCasts = 0;
};

// 164760 and 164753 - Invis Stalker: fire their blast rectangle a fixed time after they appear (capture: 1 s and 3 s after the cast)
struct npc_dos_xyexa_stalker : public NullCreatureAI
{
    npc_dos_xyexa_stalker(Creature* creature) : NullCreatureAI(creature) { }

    void IsSummonedBy(WorldObject* summoner) override
    {
        // The template has faction 0; the capture shows the faction of the boss and an uninteractible unit
        if (Unit* owner = summoner->ToUnit())
            me->SetFaction(owner->GetFaction());
        me->SetUnitFlag(UNIT_FLAG_UNINTERACTIBLE);

        bool front = me->GetEntry() == NPC_BLASTWAVE_STALKER_FRONT;
        _scheduler.Schedule(front ? StalkerFrontDelay : StalkerTargetDelay, [this, front](TaskContext /*task*/)
        {
            DoCastSelf(front ? SPELL_BLASTWAVE_FRONT : SPELL_BLASTWAVE_TARGET);
        });
    }

    void UpdateAI(uint32 diff) override
    {
        _scheduler.Update(diff);
    }

private:
    TaskScheduler _scheduler;
};

// 319619 - Displacement Trap: every player hit gets a trap at their feet
class spell_dos_xyexa_displacement_trap : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DISPLACEMENT_TRAP_MISSILE });
    }

    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        if (Unit* target = GetHitUnit())
            GetCaster()->CastSpell(target, SPELL_DISPLACEMENT_TRAP_MISSILE, CastSpellExtraArgs(TRIGGERED_FULL_MASK));
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_dos_xyexa_displacement_trap::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 324090 - Displaced Blastwave: the cast itself only chains the summon spell
class spell_dos_xyexa_displaced_blastwave : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DISPLACED_BLASTWAVE_SUMMON });
    }

    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        GetCaster()->CastSpell(GetCaster(), SPELL_DISPLACED_BLASTWAVE_SUMMON, CastSpellExtraArgs(TRIGGERED_FULL_MASK));
    }

    void Register() override
    {
        OnEffectHit += SpellEffectFn(spell_dos_xyexa_displaced_blastwave::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 320326 - Displaced Blastwave: a stalker appears on every player (the effect also summons the one in front of the boss).
// Retail has the player cast the summon on itself; here the boss does, so the summons belong to the boss.
class spell_dos_xyexa_displaced_blastwave_summon : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DISPLACED_BLASTWAVE_TARGET_SUMMON });
    }

    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        if (Unit* target = GetHitUnit())
            GetCaster()->CastSpell(target, SPELL_DISPLACED_BLASTWAVE_TARGET_SUMMON, CastSpellExtraArgs(TRIGGERED_FULL_MASK));
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_dos_xyexa_displaced_blastwave_summon::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 321948 - Localized Explosive Contrivance: one marked player. Effect 0 (the dummy the slab hangs on) is an area effect around the
// boss, so without the filter every player in range would carry the mark and explode.
class spell_dos_xyexa_localized_explosive_contrivance : public SpellScript
{
    void FilterTargets(std::list<WorldObject*>& targets)
    {
        WorldObject const* marked = GetExplTargetUnit();
        targets.remove_if([marked](WorldObject const* target) { return target != marked; });
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_dos_xyexa_localized_explosive_contrivance::FilterTargets, EFFECT_0, TARGET_UNIT_DEST_AREA_ENEMY);
    }
};

// When the mark runs out the slab appears at the marked player's position
class spell_dos_xyexa_localized_explosive_contrivance_aura : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_LOCALIZED_EXPLOSIVE_SLAB });
    }

    void HandleRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        if (GetTargetApplication()->GetRemoveMode() != AURA_REMOVE_BY_EXPIRE)
            return;

        Unit* caster = GetCaster();
        if (!caster || !caster->IsAlive() || !caster->IsInCombat())
            return;

        caster->CastSpell(GetTarget()->GetPosition(), SPELL_LOCALIZED_EXPLOSIVE_SLAB, CastSpellExtraArgs(TRIGGERED_FULL_MASK));
    }

    void Register() override
    {
        AfterEffectRemove += AuraEffectRemoveFn(spell_dos_xyexa_localized_explosive_contrivance_aura::HandleRemove, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }
};

// 319636 areatrigger (create properties 19347, areatrigger 23341, sphere 2.75, 70 s): it lives until a player steps on it (capture: 3 to
// 10 s). The trap lands at the target's feet, and in the capture the player stood inside it for 1 to 2 s after it appeared (1.1 to 2.5 yd
// from its centre) and was launched only after walking out and back in (3, 5 and 5 s after the trap appeared), so a unit that is
// inside when the trap appears is ignored until it leaves (Medium).
struct at_dos_xyexa_displacement_trap : AreaTriggerAI
{
    using AreaTriggerAI::AreaTriggerAI;

    // An update runs OnUpdate, then reports the units already inside: those first entries are the occupants and are skipped
    void OnUpdate(uint32 /*diff*/) override
    {
        if (_updates < 2)
            ++_updates;
    }

    void OnUnitEnter(Unit* unit) override
    {
        if (_updates < 2)
            return;

        if (!unit->IsPlayer() || !unit->IsAlive() || unit->HasAura(SPELL_DISPLACEMENT_TRAP_PLAYER) || unit->HasAura(SPELL_CONTRIVANCE_PROTECTION))
            return;

        // The capture has the player cast both on itself, the marker first
        unit->CastSpell(unit, SPELL_CONTRIVANCE_PROTECTION, CastSpellExtraArgs(TRIGGERED_FULL_MASK));
        unit->CastSpell(unit, SPELL_DISPLACEMENT_TRAP_PLAYER, CastSpellExtraArgs(TRIGGERED_FULL_MASK));

        Position lift = unit->GetPosition();
        lift.m_positionZ += TrapLiftHeight;
        unit->GetMotionMaster()->MoveJump(EVENT_JUMP, lift, TrapLiftTime, {}, {}, {}, true);

        at->Remove();
    }

private:
    uint32 _updates = 0;
};

// A slab hits every player standing in it (the cylinders are 6 yd high), except those still under Contrivance Protection
static void HitPlayerInSlab(AreaTrigger* at, Unit* unit, uint32 spellId)
{
    if (!unit->IsPlayer() || !unit->IsAlive() || unit->HasAura(SPELL_CONTRIVANCE_PROTECTION))
        return;

    Unit* caster = at->GetCaster();
    if (!caster || !caster->IsAlive())
        return;

    // The capture shows the boss casting both (SPELL_START and SPELL_GO), so try a plain cast first; a boss that is busy casts triggered
    if (caster->CastSpell(unit, spellId, CastSpellExtraArgs(TRIGGERED_NONE)) != SPELL_CAST_OK)
        caster->CastSpell(unit, spellId, CastSpellExtraArgs(TRIGGERED_FULL_MASK));
}

// 320230 areatrigger (create properties 19415, areatrigger 23403, cylinder r 100 h 6 z-offset 3, 1 s)
struct at_dos_xyexa_explosive_slab : AreaTriggerAI
{
    using AreaTriggerAI::AreaTriggerAI;

    void OnUnitEnter(Unit* unit) override
    {
        HitPlayerInSlab(at, unit, SPELL_EXPLOSIVE_CONTRIVANCE_DAMAGE);
    }
};

// 342962 areatrigger (create properties 21843, areatrigger 26053, cylinder r 100 h 6 z-offset 3, 1 s): every player at the level of the
// marked player
struct at_dos_xyexa_localized_slab : AreaTriggerAI
{
    using AreaTriggerAI::AreaTriggerAI;

    void OnUnitEnter(Unit* unit) override
    {
        HitPlayerInSlab(at, unit, SPELL_LOCALIZED_EXPLOSION);
    }
};

void AddSC_boss_dealer_xyexa()
{
    RegisterDeOtherSideCreatureAI(boss_dealer_xyexa);
    RegisterCreatureAI(npc_dos_xyexa_stalker);
    RegisterSpellScript(spell_dos_xyexa_displacement_trap);
    RegisterSpellScript(spell_dos_xyexa_displaced_blastwave);
    RegisterSpellScript(spell_dos_xyexa_displaced_blastwave_summon);
    RegisterSpellAndAuraScriptPair(spell_dos_xyexa_localized_explosive_contrivance, spell_dos_xyexa_localized_explosive_contrivance_aura);
    RegisterAreaTriggerAI(at_dos_xyexa_displacement_trap);
    RegisterAreaTriggerAI(at_dos_xyexa_explosive_slab);
    RegisterAreaTriggerAI(at_dos_xyexa_localized_slab);
}
