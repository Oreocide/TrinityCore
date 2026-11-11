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
 * Executor Tarvold (DungeonEncounter 2361). No phases, no health gates: Castigate every 20.6 s (Warcraft Logs, sniff and DBM agree)
 * and a Fleeting Manifestation every 30 s (Medium: three captured casts, +10 / +41 / +71 s). The add grows 10% and raises its energy
 * by 5 each second while Growing Pride damages the party; at 100 energy it dies and leaves a Residue pool, and it leaves a smaller
 * one when it is killed sooner (trinitycore-tracking/sl-dungeons/sanguine-depths/bosses/executor-tarvold.md).
 * Mythic-only actors (Sintouched Anima 328494 / 328424, entries 168882 / 168846) are not scripted, nor is the Starving Prisoner /
 * Frenzied Ghoul Teleport Self ambience of the arena (trash).
 */

#include "AreaTrigger.h"
#include "AreaTriggerAI.h"
#include "InstanceScript.h"
#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "Spell.h"
#include "TemporarySummon.h"
#include "sanguine_depths.h"

enum TarvoldSpells
{
    SPELL_CASTIGATE                 = 322554,   // 2 s cast, then a 3 s channel on the target: 322567 damages 8 yd around it every second
    SPELL_COALESCE_MANIFESTATION    = 322573,   // instant missile, 322574 summons the add 2 s later
    SPELL_GROWING_PRIDE             = 323548,   // on the add: +10% scale (323549) and 324659 party damage every second
    SPELL_RESIDUE                   = 323551,   // cast by the add: creates the pool (create properties 19739, areatrigger 23704)
    SPELL_RESIDUE_DOT               = 323573    // aura a player casts on itself while in a pool
};

enum TarvoldEvents
{
    EVENT_CASTIGATE = 1,
    EVENT_COALESCE_MANIFESTATION
};

enum Texts
{
    // Group ids as in the live creature_text rows
    SAY_CASTIGATE                   = 1,    // You will confess, one way or another.
    SAY_DEATH                       = 2,    // The sinners... still... await...
    SAY_AGGRO                       = 3     // Let us begin.
};

enum TarvoldCreatures
{
    NPC_FLEETING_MANIFESTATION      = 165556
};

enum TarvoldMisc
{
    EMOTE_STATE_PRE_PULL            = 468   // Tarvold's EmoteState until the pull (sniff); creature_addon has 0
};

static constexpr Milliseconds CastigateFirst = 4700ms;
static constexpr Milliseconds CastigateInterval = 20600ms;
static constexpr Milliseconds CoalesceFirst = 10s;
static constexpr Milliseconds CoalesceInterval = 30s;       // Medium: sniff gaps 31.0 s and 30.0 s, DBM 29.5-30 s

static constexpr Milliseconds GrowingPrideDelay = 2s;       // the add casts it 2 s after it is created
static constexpr Milliseconds EnergyTickInterval = 1s;
static constexpr int32 EnergyPerTick = 5;
static constexpr int32 EnergyMax = 100;

// The pool grows from its base radius over 5 s (OverrideScaleCurve in the sniff): 20 for the add that erupted, 5.75 for both
// killed ones (Low: n = 1 and n = 2).
static constexpr uint32 ResidueGrowTime = 5000;
static constexpr float ResidueScaleEruption = 20.0f;
static constexpr float ResidueScaleKilled = 5.75f;

// 162103 - Executor Tarvold
struct boss_executor_tarvold : public BossAI
{
    boss_executor_tarvold(Creature* creature) : BossAI(creature, DATA_EXECUTOR_TARVOLD) { }

    void Reset() override
    {
        BossAI::Reset();
        me->SetEmoteState(Emote(EMOTE_STATE_PRE_PULL));
    }

    void JustEngagedWith(Unit* who) override
    {
        BossAI::JustEngagedWith(who);
        me->SetEmoteState(EMOTE_ONESHOT_NONE);
        Talk(SAY_AGGRO);
        events.ScheduleEvent(EVENT_CASTIGATE, CastigateFirst);
        events.ScheduleEvent(EVENT_COALESCE_MANIFESTATION, CoalesceFirst);
    }

    // The add is passive and has no threat list: it must not be pulled into the fight like a normal add
    void JustSummoned(Creature* summon) override
    {
        summons.Summon(summon);
    }

    void ExecuteEvent(uint32 eventId) override
    {
        switch (eventId)
        {
            case EVENT_CASTIGATE:
                // Low: the target rule is not evidenced (the capture is a solo run)
                if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0, 100.0f, true))
                {
                    if (DoCast(target, SPELL_CASTIGATE) == SPELL_CAST_OK)
                        Talk(SAY_CASTIGATE);
                }
                events.Repeat(CastigateInterval);
                break;
            case EVENT_COALESCE_MANIFESTATION:
                DoCastSelf(SPELL_COALESCE_MANIFESTATION);
                events.Repeat(CoalesceInterval);
                break;
            default:
                break;
        }
    }

    // The pools last 120 s and the aura has no duration: both go with the fight
    void ClearPools()
    {
        me->RemoveAllAreaTriggers();
        if (instance)
            instance->DoRemoveAurasDueToSpellOnPlayers(SPELL_RESIDUE_DOT);
    }

    void JustDied(Unit* killer) override
    {
        ClearPools();
        Talk(SAY_DEATH);
        BossAI::JustDied(killer);
    }

    void EnterEvadeMode(EvadeReason why) override
    {
        ClearPools();
        BossAI::EnterEvadeMode(why);
    }
};

// 165556 - Fleeting Manifestation
struct npc_sd_tarvold_fleeting_manifestation : public ScriptedAI
{
    npc_sd_tarvold_fleeting_manifestation(Creature* creature) : ScriptedAI(creature), _residueCast(false) { }

    // Runs before the add is placed on the map: it must be passive before it can pick a victim
    void Reset() override
    {
        me->SetReactState(REACT_PASSIVE);
        me->SetPowerType(POWER_ENERGY);
        me->SetMaxPower(POWER_ENERGY, EnergyMax);
        me->SetPower(POWER_ENERGY, 0);
    }

    void IsSummonedBy(WorldObject* /*summoner*/) override
    {
        scheduler.Schedule(GrowingPrideDelay, [this](TaskContext task)
        {
            // creature_template_addon may already have applied it at spawn
            if (!me->HasAura(SPELL_GROWING_PRIDE) && DoCastSelf(SPELL_GROWING_PRIDE) != SPELL_CAST_OK)
                DoCastSelf(SPELL_GROWING_PRIDE, true);

            task.Schedule(EnergyTickInterval, [this](TaskContext energyTask)
            {
                if (me->HasAura(SPELL_GROWING_PRIDE))
                    me->SetPower(POWER_ENERGY, me->GetPower(POWER_ENERGY) + EnergyPerTick);

                if (me->GetPower(POWER_ENERGY) >= EnergyMax)
                {
                    CastResidue(TRIGGERED_NONE);
                    me->KillSelf(false);
                    return;
                }

                energyTask.Repeat(EnergyTickInterval);
            });
        });
    }

    // Retail's pool outlives the add: the boss owns it (and clears it at the end of the fight), the add only places it
    void CastResidue(TriggerCastFlags flags)
    {
        CastSpellExtraArgs args(flags);
        if (TempSummon const* summon = me->ToTempSummon())
            args.SetOriginalCaster(summon->GetSummonerGUID());

        // A stun or silence makes the plain cast fail: place the pool anyway, while the energy is still full
        if (me->CastSpell(me, SPELL_RESIDUE, args) != SPELL_CAST_OK && !(flags & TRIGGERED_FULL_MASK))
        {
            args.TriggerFlags = TRIGGERED_FULL_MASK;
            me->CastSpell(me, SPELL_RESIDUE, args);
        }

        _residueCast = true;
    }

    void JustDied(Unit* /*killer*/) override
    {
        scheduler.CancelAll();
        if (!_residueCast)
            CastResidue(TRIGGERED_FULL_MASK);
    }

    void UpdateAI(uint32 diff) override
    {
        scheduler.Update(diff);
    }

private:
    TaskScheduler scheduler;
    bool _residueCast;
};

// Areatrigger create properties 19739 (areatrigger 23704): Residue. Retail has the player cast 323573 on itself while it stands inside.
struct at_sd_tarvold_residue : AreaTriggerAI
{
    using AreaTriggerAI::AreaTriggerAI;

    void OnCreate(Spell const* creatingSpell) override
    {
        float finalScale = ResidueScaleKilled;
        if (creatingSpell)
            if (Unit const* add = creatingSpell->GetCaster()->ToUnit())
                if (add->GetPower(POWER_ENERGY) >= EnergyMax)
                    finalScale = ResidueScaleEruption;

        at->SetTimeToTargetScale(ResidueGrowTime);
        at->SetOverrideScaleCurve({ DBCPosition2D{ 0.0f, 1.0f }, DBCPosition2D{ 1.0f, finalScale } });
    }

    void OnUnitEnter(Unit* unit) override
    {
        if (unit->IsPlayer())
            unit->CastSpell(unit, SPELL_RESIDUE_DOT, CastSpellExtraArgs(TRIGGERED_FULL_MASK));
    }

    // The unit is already out of this areatrigger's list when OnUnitExit runs
    void OnUnitExit(Unit* unit, AreaTriggerExitReason /*reason*/) override
    {
        if (!unit->IsPlayer())
            return;

        for (AreaTrigger const* other : unit->GetInsideAreaTriggers())
            if (other != at && other->GetEntry() == at->GetEntry())
                return;

        unit->RemoveAurasDueToSpell(SPELL_RESIDUE_DOT);
    }
};

void AddSC_boss_executor_tarvold()
{
    RegisterSanguineDepthsCreatureAI(boss_executor_tarvold);
    RegisterCreatureAI(npc_sd_tarvold_fleeting_manifestation);
    RegisterAreaTriggerAI(at_sd_tarvold_residue);
}
