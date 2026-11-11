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
 * High Adjudicator Aleez (DungeonEncounter 2403). One phase, Heroic timers from the 2026-11-10 capture (one solo kill, 100 s)
 * against Mythic no-keystone Warcraft Logs pulls (n=7); see trinitycore-tracking/sl-dungeons/halls-of-atonement/bosses/.
 *
 * Spectral Procession is cast four times in the capture and summoned a Ghastly Parishioner only twice (19 s and 82 s), so a new
 * one is summoned only while none is alive (Heroic rule inferred from two guids against four casts). The Parishioner appears
 * where retail's DEST_NEARBY_ENTRY summon put it; the summon point has no spawn here, so the position is fixed.
 * Vessels of Atonement: the capture shows each of the four static Empty Anima Vessels with its own 323749 cylinder AT (a spell
 * AT cast by the Vessel, present before the pull) and the encounter adds two more at 72 s and 95 s. The imported static
 * Vessels have no AT, so they cast 323749 when the encounter starts. A sealed static Vessel keeps its transform and loses its AT
 * until the encounter ends; a created one is removed (retail: 74 s sealed, Vessel dead at once).
 * Mythic only and left out: Unstable Anima 1236512.
 */

#include "AreaTrigger.h"
#include "AreaTriggerAI.h"
#include "Containers.h"
#include "InstanceScript.h"
#include "Map.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "SpellAuraEffects.h"
#include "SpellAuras.h"
#include "SpellScript.h"
#include "TemporarySummon.h"
#include "ThreatManager.h"
#include "halls_of_atonement.h"

enum AleezSpells
{
    SPELL_ANIMA_BOLT                    = 323538,
    SPELL_ANIMA_FOUNTAIN                = 329340,   // 2 s cast; its aura lands on the static Fountain 166497
    SPELL_ANIMA_FOUNTAIN_PERIODIC       = 338007,   // Fountain, 6 s, 1.5 s period, first tick on apply -> 338009
    SPELL_ANIMA_FOUNTAIN_TICK           = 338009,   // DUMMY on every player
    SPELL_ANIMA_FOUNTAIN_MISSILE        = 338012,   // -> 338013 pool at the player's position
    SPELL_EPHEMERAL_VISAGE              = 323741,
    SPELL_HAUNTING_FIXATION_PICKER      = 323605,
    SPELL_HAUNTING_FIXATION             = 323650,
    SPELL_PULSE_FROM_BEYOND             = 323852,
    SPELL_VESSEL_AREATRIGGER            = 323749,
    SPELL_VESSEL_SEAL                   = 335365,   // missile 335368 at a Parishioner within 5 yd + transform of the Vessel
    SPELL_VESSEL_SEAL_HIT               = 335368,
    SPELL_CONTAINER_SPAWN_VISUAL        = 1236588
};

enum AleezEvents
{
    EVENT_ANIMA_BOLT = 1,
    EVENT_SPECTRAL_PROCESSION,
    EVENT_ANIMA_FOUNTAIN,
    EVENT_NEW_VESSEL
};

enum AleezCreatures
{
    NPC_GHASTLY_PARISHIONER             = 165913,
    NPC_EMPTY_ANIMA_VESSEL              = 166034,
    NPC_ANIMA_FOUNTAIN                  = 166497
};

enum AleezActions
{
    ACTION_PARISHIONER_SEALED = 1
};

enum Texts
{
    SAY_AGGRO                           = 0,    // "Your penance is at hand!"
    SAY_SPECTRAL_PROCESSION             = 1,    // "Their pain shall enlighten you."
    SAY_DEATH                           = 2     // "My flock... needs me..."
};

static constexpr Position ParishionerSpawnPosition = { -2209.84f, 5676.07f, 4184.83f, 4.77165f };

// The two vessels the capture created (72 s, 95 s); the sequence repeats
static constexpr Position VesselPositions[] =
{
    { -2222.18f, 5670.32f, 4179.92f, 0.0f },
    { -2209.76f, 5684.62f, 4180.00f, 0.0f }
};

static constexpr Milliseconds ParishionerSealedDespawn = 5s;    // sealed at 74 s, gone at 79 s

// 165410 - High Adjudicator Aleez
struct boss_high_adjudicator_aleez : public BossAI
{
    boss_high_adjudicator_aleez(Creature* creature) : BossAI(creature, DATA_HIGH_ADJUDICATOR_ALEEZ) { }

    void JustEngagedWith(Unit* who) override
    {
        BossAI::JustEngagedWith(who);
        Talk(SAY_AGGRO);
        _vesselIndex = 0;
        ActivateStaticVessels();
        events.ScheduleEvent(EVENT_ANIMA_BOLT, 7s);                 // WCL Mythic 5.7 s, capture 7.0 s: Medium
        events.ScheduleEvent(EVENT_SPECTRAL_PROCESSION, 19s);       // WCL Mythic 15.7 s, capture 19.0 s: Medium
        events.ScheduleEvent(EVENT_ANIMA_FOUNTAIN, 21s);            // WCL 20.7 s, capture 21.0 s: Medium
        events.ScheduleEvent(EVENT_NEW_VESSEL, 72s);                // WCL 71.0 s, capture 72.0 s: Low
    }

    void JustSummoned(Creature* summon) override
    {
        switch (summon->GetEntry())
        {
            case NPC_GHASTLY_PARISHIONER:
                BossAI::JustSummoned(summon);
                break;
            case NPC_EMPTY_ANIMA_VESSEL:
                summons.Summon(summon);
                summon->CastSpell(summon, SPELL_CONTAINER_SPAWN_VISUAL, true);
                summon->CastSpell(summon, SPELL_VESSEL_AREATRIGGER, true);
                break;
            default:
                summons.Summon(summon);
                break;
        }
    }

    void SummonedCreatureDespawn(Creature* summon) override
    {
        BossAI::SummonedCreatureDespawn(summon);
        if (summon->GetEntry() == NPC_GHASTLY_PARISHIONER)
            RemoveFixations();
    }

    void ExecuteEvent(uint32 eventId) override
    {
        switch (eventId)
        {
            case EVENT_ANIMA_BOLT:
                DoCastVictim(SPELL_ANIMA_BOLT);
                events.Repeat(8500ms);                              // capture 8/9 s alternating, WCL 7.7 s: Medium
                break;
            case EVENT_SPECTRAL_PROCESSION:
                Talk(SAY_SPECTRAL_PROCESSION);
                if (!summons.HasEntry(NPC_GHASTLY_PARISHIONER))
                    me->SummonCreature(NPC_GHASTLY_PARISHIONER, ParishionerSpawnPosition);
                events.Repeat(21s);                                 // capture 20/22/21 s, WCL 20.6 s: Medium
                break;
            case EVENT_ANIMA_FOUNTAIN:
                DoCastSelf(SPELL_ANIMA_FOUNTAIN);
                events.Repeat(24s);                                 // capture 23/26/25 s, WCL 23.1 s: Medium
                break;
            case EVENT_NEW_VESSEL:
                me->SummonCreature(NPC_EMPTY_ANIMA_VESSEL, VesselPositions[_vesselIndex++ % std::size(VesselPositions)]);
                events.Repeat(23s);                                 // one gap in the capture (95 s), WCL 20.6 s: Low
                break;
            default:
                break;
        }
    }

    void GetStaticVessels(std::list<Creature*>& vessels) const
    {
        me->GetCreatureListWithEntryInGrid(vessels, NPC_EMPTY_ANIMA_VESSEL, 100.0f);
        vessels.remove_if([](Creature const* vessel) { return vessel->IsSummon(); });
    }

    void ActivateStaticVessels()
    {
        std::list<Creature*> vessels;
        GetStaticVessels(vessels);
        for (Creature* vessel : vessels)
            vessel->CastSpell(vessel, SPELL_VESSEL_AREATRIGGER, true);
    }

    void ResetStaticVessels()
    {
        std::list<Creature*> vessels;
        GetStaticVessels(vessels);
        for (Creature* vessel : vessels)
        {
            vessel->RemoveAreaTrigger(SPELL_VESSEL_AREATRIGGER);
            vessel->RemoveAurasDueToSpell(SPELL_VESSEL_SEAL);
        }
    }

    void RemoveFixations()
    {
        for (MapReference const& ref : me->GetMap()->GetPlayers())
            if (Player* player = ref.GetSource())
                player->RemoveAurasDueToSpell(SPELL_HAUNTING_FIXATION);
    }

    void JustDied(Unit* killer) override
    {
        Talk(SAY_DEATH);
        RemoveFixations();
        ResetStaticVessels();
        BossAI::JustDied(killer);
    }

    void EnterEvadeMode(EvadeReason why) override
    {
        RemoveFixations();
        ResetStaticVessels();
        BossAI::EnterEvadeMode(why);
    }

private:
    uint8 _vesselIndex = 0;
};

// 165913 - Ghastly Parishioner
struct npc_hoa_ghastly_parishioner : public ScriptedAI
{
    npc_hoa_ghastly_parishioner(Creature* creature) : ScriptedAI(creature) { }

    void JustAppeared() override
    {
        me->SetCanFly(true);
        me->SetDisableGravity(true);
        if (!me->HasAura(SPELL_EPHEMERAL_VISAGE))
            DoCastSelf(SPELL_EPHEMERAL_VISAGE, true);

        // Pulse from Beyond and the first fixation come 1 s after it appears (capture 19 s -> 20 s); the check also picks a
        // new victim when the fixated player dies
        _scheduler.Schedule(1s, [this](TaskContext task)
        {
            if (!me->HasAura(SPELL_PULSE_FROM_BEYOND))
                DoCastSelf(SPELL_PULSE_FROM_BEYOND, true);
            if (!me->GetThreatManager().GetFixateTarget())
                DoCastSelf(SPELL_HAUNTING_FIXATION_PICKER, true);
            task.Repeat(1s);
        });
    }

    void IsSummonedBy(WorldObject* summoner) override
    {
        // creature_template.faction of the entry is 0
        if (Unit* owner = summoner->ToUnit())
            me->SetFaction(owner->GetFaction());
    }

    void DoAction(int32 action) override
    {
        if (action != ACTION_PARISHIONER_SEALED)
            return;

        _scheduler.CancelAll();
        me->RemoveAurasDueToSpell(SPELL_PULSE_FROM_BEYOND);
        if (Unit* victim = me->GetThreatManager().GetFixateTarget())
            victim->RemoveAurasDueToSpell(SPELL_HAUNTING_FIXATION, me->GetGUID());
        me->GetThreatManager().ClearFixate();
        me->SetReactState(REACT_PASSIVE);
        me->AttackStop();
        me->DespawnOrUnsummon(ParishionerSealedDespawn);
    }

    void EnterEvadeMode(EvadeReason /*why*/) override
    {
        me->DespawnOrUnsummon();
    }

    void UpdateAI(uint32 diff) override
    {
        _scheduler.Update(diff);

        if (!UpdateVictim())
            return;
    }

private:
    TaskScheduler _scheduler;
};

// 323605 - Haunting Fixation (picker, cast by the Parishioner)
class spell_hoa_aleez_haunting_fixation_picker : public SpellScript
{
    void FilterTargets(std::list<WorldObject*>& targets)
    {
        targets.remove_if([](WorldObject const* target)
        {
            Player const* player = target->ToPlayer();
            return !player || !player->IsAlive();
        });

        if (targets.size() > 1)
            Trinity::Containers::RandomResize(targets, 1);
    }

    void HandleHit(SpellEffIndex /*effIndex*/)
    {
        Unit* caster = GetCaster();
        Unit* target = GetHitUnit();
        if (!caster || !target)
            return;

        // MOD_FIXATE of 323650 only fixates a unit that is already on the caster's threat list
        caster->EngageWithTarget(target);
        caster->CastSpell(target, SPELL_HAUNTING_FIXATION, true);
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_hoa_aleez_haunting_fixation_picker::FilterTargets, EFFECT_0, TARGET_UNIT_SRC_AREA_ENTRY);
        OnEffectHitTarget += SpellEffectFn(spell_hoa_aleez_haunting_fixation_picker::HandleHit, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 323853 - Pulse from Beyond (damage on all players)
class spell_hoa_aleez_pulse_from_beyond : public SpellScript
{
    void FilterTargets(std::list<WorldObject*>& targets)
    {
        targets.remove_if([](WorldObject const* target)
        {
            return !target->IsPlayer();
        });
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_hoa_aleez_pulse_from_beyond::FilterTargets, EFFECT_0, TARGET_UNIT_SRC_AREA_ENTRY);
    }
};

// 329340 - Anima Fountain (AURA_DUMMY on the static Fountain 166497 starts its volley)
class spell_hoa_aleez_anima_fountain : public SpellScript
{
    void FilterTargets(std::list<WorldObject*>& targets)
    {
        targets.remove_if([](WorldObject const* target)
        {
            return target->GetEntry() != NPC_ANIMA_FOUNTAIN;
        });
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_hoa_aleez_anima_fountain::FilterTargets, EFFECT_0, TARGET_UNIT_SRC_AREA_ENTRY);
    }
};

class spell_hoa_aleez_anima_fountain_aura : public AuraScript
{
    void HandleApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        Unit* target = GetTarget();
        target->CastSpell(target, SPELL_ANIMA_FOUNTAIN_PERIODIC, true);
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(spell_hoa_aleez_anima_fountain_aura::HandleApply, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }
};

// 338009 - Anima Fountain (one missile per player per tick)
class spell_hoa_aleez_anima_fountain_tick : public SpellScript
{
    void HandleHit(SpellEffIndex /*effIndex*/)
    {
        Unit* caster = GetCaster();
        Unit* target = GetHitUnit();
        if (caster && target)
            caster->CastSpell(target, SPELL_ANIMA_FOUNTAIN_MISSILE, true);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_hoa_aleez_anima_fountain_tick::HandleHit, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 335365 - Vessel of Atonement (missile at a Parishioner in reach of the Vessel)
class spell_hoa_aleez_vessel_seal_cast : public SpellScript
{
    void FilterTargets(std::list<WorldObject*>& targets)
    {
        targets.remove_if([](WorldObject const* target)
        {
            return target->GetEntry() != NPC_GHASTLY_PARISHIONER;
        });
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_hoa_aleez_vessel_seal_cast::FilterTargets, EFFECT_0, TARGET_UNIT_SRC_AREA_ENTRY);
    }
};

// 335368 - Vessel of Atonement (the missile lands on the Parishioner)
class spell_hoa_aleez_vessel_seal : public SpellScript
{
    void HandleHit(SpellEffIndex /*effIndex*/)
    {
        Creature* parishioner = GetHitCreature();
        if (!parishioner || parishioner->GetEntry() != NPC_GHASTLY_PARISHIONER || !parishioner->IsAIEnabled())
            return;

        parishioner->AI()->DoAction(ACTION_PARISHIONER_SEALED);

        Creature* vessel = GetCaster() ? GetCaster()->ToCreature() : nullptr;
        if (!vessel)
            return;

        if (vessel->IsSummon())
            vessel->DespawnOrUnsummon(1s);
        else
            vessel->RemoveAreaTrigger(SPELL_VESSEL_AREATRIGGER);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_hoa_aleez_vessel_seal::HandleHit, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 19769 - Vessel of Atonement cylinder (AT 23731, created by 323749 on the Vessel)
struct at_hoa_aleez_vessel : AreaTriggerAI
{
    using AreaTriggerAI::AreaTriggerAI;

    void OnUnitEnter(Unit* unit) override
    {
        if (_sealed || unit->GetEntry() != NPC_GHASTLY_PARISHIONER || !unit->IsAlive())
            return;

        Unit* caster = at->GetCaster();
        if (!caster)
            return;

        _sealed = true;
        caster->CastSpell(caster, SPELL_VESSEL_SEAL, true);
    }

private:
    bool _sealed = false;
};

void AddSC_boss_high_adjudicator_aleez()
{
    RegisterHallsOfAtonementCreatureAI(boss_high_adjudicator_aleez);
    RegisterHallsOfAtonementCreatureAI(npc_hoa_ghastly_parishioner);
    RegisterSpellScript(spell_hoa_aleez_haunting_fixation_picker);
    RegisterSpellScript(spell_hoa_aleez_pulse_from_beyond);
    RegisterSpellAndAuraScriptPair(spell_hoa_aleez_anima_fountain, spell_hoa_aleez_anima_fountain_aura);
    RegisterSpellScript(spell_hoa_aleez_anima_fountain_tick);
    RegisterSpellScript(spell_hoa_aleez_vessel_seal_cast);
    RegisterSpellScript(spell_hoa_aleez_vessel_seal);
    RegisterAreaTriggerAI(at_hoa_aleez_vessel);
}
