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

#include "ScriptMgr.h"
#include "throne_of_the_tides.h"
#include "Containers.h"
#include "GameObject.h"
#include "MoveSpline.h"
#include "ObjectMgr.h"
#include "ScriptedCreature.h"
#include "SpellScript.h"
#include "SpellAuraEffects.h"
#include "SpellMgr.h"
#include "Player.h"
#include "InstanceScript.h"
#include "MotionMaster.h"

enum LadyNazjarTexts
{
    SAY_AGGRO       = 0,
    SAY_SLAY        = 1,
    SAY_SUMMON_ADDS = 2,
    SAY_DEATH       = 3
};

enum LadyNazjarSpells
{
    // Lady Naz'jar
    SPELL_SUMMON_GEYSER             = 75722,
    SPELL_FUNGAL_SPORES             = 76001,
    SPELL_WATERSPOUT_SUMMON_1       = 90495,
    SPELL_WATERSPOUT_SUMMON_2       = 90497,
    SPELL_WATERSPOUT_SHIELD         = 75683,
    SPELL_ACHIEVEMENT_OLD_FAITHFUL  = 94042, // Serverside spell. Handled via UpdateAchievementCriteria

    // Waterspout
    SPELL_EJECT_ALL_PASSENGERS      = 63109,
    SPELL_WATERSPOUT_TARGETING      = 90444,
    SPELL_WATERSPOUT                = 90440,

    // Geyser
    SPELL_GEYSER_VISUAL             = 75699,
    SPELL_GEYSER_ERUPTION           = 75700,

    // Naz'jar Invaders
    SPELL_WATER_WINDOW_BREAK_VISUAL = 75808,

    // Naz'jar Honor Guard
    SPELL_ARC_SLASH                 = 75907,
    SPELL_ENRAGE                    = 75998,

    // Naz'jar Tempest Witch
    SPELL_LIGHTNING_SURGE           = 75992
};

#define SPELL_SHOCK_BLAST           DUNGEON_MODE<uint32>(76008, 91477)
#define SPELL_FUNGAL_SPORES_AURA    DUNGEON_MODE<uint32>(80564, 91470)
#define SPELL_CHAIN_LIGHTNING       DUNGEON_MODE<uint32>(75813, 91450)

enum LadyNazjarEvents
{
    // Lady Naz'jar
    EVENT_SUMMON_GEYSER = 1,
    EVENT_FUNGAL_SPORES,
    EVENT_SHOCK_BLAST,
    EVENT_REENGAGE,

    // Naz'jar Honor Guard
    EVENT_ARC_SLASH,

    // Naz'jar Tempest Witch
    EVENT_CHAIN_LIGHTNING,
    EVENT_LIGHTNING_SURGE
};

class GeyserEruptionEvent : public BasicEvent
{
public:
    GeyserEruptionEvent(Creature* owner) : _owner(owner) { }

    bool Execute(uint64 /*time*/, uint32 /*diff*/) override
    {
        _owner->RemoveAllAuras();
        _owner->CastSpell(_owner, SPELL_GEYSER_ERUPTION);
        return true;
    }

private:
    Creature* _owner;
};

// Add lands from its window jump: strip the airborne state, then turn it
// aggressive a second later. Driven by the spline actually finishing -
// movespline->Duration() read right after MoveJump is stale because the
// MotionMaster only launches the jump on its next update.
static void OnNazjarAddLanded(Creature* creature)
{
    creature->RemoveAurasDueToSpell(SPELL_WATER_JUMP_THROUGH_WINDOW_VISUAL);
    creature->SetDisableGravity(false);
    creature->SetMovementAnimKitId(0);
    creature->m_Events.AddEventAtOffset([creature]()
    {
        creature->SetReactState(REACT_AGGRESSIVE);
        if (creature->IsAIEnabled())
            creature->AI()->DoZoneInCombat();
    }, 1s);
}

Position const LadyNazjarCenterPosition = { 191.5f, 802.7f, 807.6f, 4.414708f };

Position const LadyNazjarAddPositions[] =
{
    { 249.9375f, 802.25f,   828.8116f, 3.147335f },
    { 191.1979f, 745.6007f, 830.6442f, 1.532863f },
    { 191.8733f, 859.5452f, 831.4933f, 4.705678f }
};

Position const LadyNazjarAddJumpPositions[] =
{
    { 208.7847f, 802.2274f, 808.3681f },
    { 191.4826f, 784.6146f, 808.3681f },
    { 191.8229f, 820.0486f, 808.3681f }
};

enum LadyNazjarAddPoints
{
    POINT_NAZJAR_ADD_JUMP = 1
};

// 40586 - Lady Naz'jar
struct boss_lady_nazjar : public BossAI
{
    boss_lady_nazjar(Creature* creature) : BossAI(creature, BOSS_LADY_NAZJAR), _waterspoutPhaseCount(0), _killedAdds(0), _isInTransition(false) { }

    void JustEngagedWith(Unit* who) override
    {
        BossAI::JustEngagedWith(who);
        Talk(SAY_AGGRO);
        HandleDoor(false);
        instance->SendEncounterUnit(ENCOUNTER_FRAME_ENGAGE, me);
        events.ScheduleEvent(EVENT_SUMMON_GEYSER, 11s, 16s);
        events.ScheduleEvent(EVENT_FUNGAL_SPORES, 13s, 19s);
        events.ScheduleEvent(EVENT_SHOCK_BLAST, 13s, 16s);
    }

    void Reset() override
    {
        _Reset();
        me->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_INTERRUPT, true);
    }

    void KilledUnit(Unit* killed) override
    {
        if (killed->GetTypeId() == TYPEID_PLAYER)
            Talk(SAY_SLAY);
    }

    void EnterEvadeMode(EvadeReason /*why*/) override
    {
        _EnterEvadeMode();
        HandleDoor(true);
        instance->SendEncounterUnit(ENCOUNTER_FRAME_DISENGAGE, me);
        instance->DoRemoveAurasDueToSpellOnPlayers(SPELL_FUNGAL_SPORES_AURA);
        instance->DoRemoveAurasDueToSpellOnPlayers(SPELL_LIGHTNING_SURGE);
        instance->SetData(DATA_LADY_NAZJAR_GEYSERS, NOT_STARTED);
        summons.DespawnAll();
        _DespawnAtEvade();
    }

    void JustDied(Unit* /*killer*/) override
    {
        _JustDied();
        Talk(SAY_DEATH);
        HandleDoor(false);
        instance->SendEncounterUnit(ENCOUNTER_FRAME_DISENGAGE, me);
        instance->DoRemoveAurasDueToSpellOnPlayers(SPELL_FUNGAL_SPORES_AURA);
        instance->DoRemoveAurasDueToSpellOnPlayers(SPELL_LIGHTNING_SURGE);
        instance->SetData(DATA_LADY_NAZJAR_GEYSERS, NOT_STARTED);
    }

    void JustSummoned(Creature* summon) override
    {
        summons.Summon(summon);

        switch (summon->GetEntry())
        {
            case NPC_GEYSER:
                summon->CastSpell(summon, SPELL_GEYSER_VISUAL);
                summon->m_Events.AddEvent(new GeyserEruptionEvent(summon), summon->m_Events.CalculateTime(Milliseconds(6000)));
                break;
            case NPC_WATERSPOUT:
                summon->CastSpell(summon, SPELL_WATERSPOUT_TARGETING);
                summon->m_Events.AddEventAtOffset([summon]()
                {
                    summon->CastSpell(summon, SPELL_EJECT_ALL_PASSENGERS);
                    summon->RemoveAllAuras();
                    summon->DespawnOrUnsummon(5s);
                }, 20s + 500ms);
                break;
            default:
                break;
        }
    }

    void SummonedCreatureDies(Creature* summon, Unit* killer) override
    {
        if (summon->GetEntry() == NPC_NAZJAR_HONOR_GUARD || summon->GetEntry() == NPC_NAZJAR_TEMPEST_WITCH)
        {
            _killedAdds++;
            if (_killedAdds == 3 && me->HasAura(SPELL_WATERSPOUT_SHIELD))
            {
                me->RemoveAurasDueToSpell(SPELL_WATERSPOUT_SHIELD);
                me->RemoveAurasDueToSpell(SPELL_WATERSPOUT_SUMMON_1);
                me->RemoveAurasDueToSpell(SPELL_WATERSPOUT_SUMMON_2);
                events.RescheduleEvent(EVENT_REENGAGE, 1ms);
            }

            if (killer && killer->GetEntry() == NPC_GEYSER)
                instance->DoUpdateCriteria(CriteriaType::BeSpellTarget, SPELL_ACHIEVEMENT_OLD_FAITHFUL, 0, summon);
        }
    }

    void OnSpellCast(SpellInfo const* spell) override
    {
        if (spell->Id == SPELL_SHOCK_BLAST)
            me->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_INTERRUPT, true);
    }

    void OnSpellFailed(SpellInfo const* spell) override
    {
        if (spell->Id == SPELL_SHOCK_BLAST)
            me->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_INTERRUPT, true);
    }

    void DamageTaken(Unit* /*attacker*/, uint32& damage, DamageEffectType /*damageType*/, SpellInfo const* /*spellInfo*/) override
    {
        if (damage >= me->GetHealth() && (_waterspoutPhaseCount < 2 || _isInTransition))
            damage = me->GetHealth() - 1;

        if ((me->HealthBelowPctDamaged(60, damage) && _waterspoutPhaseCount == 0)
            || (me->HealthBelowPctDamaged(30, damage) && _waterspoutPhaseCount == 1))
        {
            _isInTransition = true;
            events.Reset();
            me->InterruptNonMeleeSpells(true);
            me->NearTeleportTo(LadyNazjarCenterPosition);
            instance->SetData(DATA_LADY_NAZJAR_GEYSERS, IN_PROGRESS);
            Talk(SAY_SUMMON_ADDS);
            if (IsHeroic())
            {
                DoCastSelf(SPELL_WATERSPOUT_SUMMON_1);
                DoCastSelf(SPELL_WATERSPOUT_SUMMON_2);
            }
            DoCastSelf(SPELL_WATERSPOUT_SHIELD);
            events.ScheduleEvent(EVENT_REENGAGE, 1min + 1s + 500ms);

            _killedAdds = 0;
            for (uint8 i = 0; i < 3; i++)
            {
                if (Creature* invader = DoSummon((i == 0 ? NPC_NAZJAR_HONOR_GUARD : NPC_NAZJAR_TEMPEST_WITCH), LadyNazjarAddPositions[i], 4s))
                {
                    invader->SetReactState(REACT_PASSIVE);
                    invader->SetDisableGravity(true);
                    invader->SetMovementAnimKitId(3);
                    invader->CastSpell(invader, SPELL_WATER_WINDOW_BREAK_VISUAL);
                    invader->GetMotionMaster()->MoveJump(POINT_NAZJAR_ADD_JUMP, LadyNazjarAddJumpPositions[i], 16.0f, {}, 11.0f);
                    ExecuteOnMovementEnd(invader, [invader]() { OnNazjarAddLanded(invader); });
                }
            }

            _waterspoutPhaseCount++;
        }
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim())
            return;

        events.Update(diff);

        if (me->HasUnitState(UNIT_STATE_CASTING))
            return;

        while (uint32 eventId = events.ExecuteEvent())
        {
            switch (eventId)
            {
                case EVENT_SUMMON_GEYSER:
                    if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0, 50.0f, true))
                        DoCast(target, SPELL_SUMMON_GEYSER);
                    events.Repeat(11s, 16s);
                    break;
                case EVENT_FUNGAL_SPORES:
                    if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0, 50.0f, true))
                        DoCast(target, SPELL_FUNGAL_SPORES);
                    events.Repeat(13s, 19s);
                    break;
                case EVENT_SHOCK_BLAST:
                    me->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_INTERRUPT, false);
                    DoCastVictim(SPELL_SHOCK_BLAST);
                    events.Repeat(15s, 16s);
                    break;
                case EVENT_REENGAGE:
                    me->SetReactState(REACT_AGGRESSIVE);
                    if (Unit* target = me->GetVictim())
                        AttackStart(target);

                    _isInTransition = false;
                    instance->SetData(DATA_LADY_NAZJAR_GEYSERS, NOT_STARTED);
                    events.ScheduleEvent(EVENT_SUMMON_GEYSER, 11s, 16s);
                    events.ScheduleEvent(EVENT_FUNGAL_SPORES, 13s, 19s);
                    events.ScheduleEvent(EVENT_SHOCK_BLAST, 13s, 16s);
                    break;
                default:
                    break;
            }
        }
    }

private:
    void HandleDoor(bool open)
    {
        GameObject* door = instance->GetGameObject(DATA_ABYSSAL_MAW_DOOR_2);
        if (!door)
            return;

        door->SetGoState(open ? GO_STATE_ACTIVE : GO_STATE_READY);
    }

    uint8 _waterspoutPhaseCount;
    uint8 _killedAdds;
    bool _isInTransition;
};

// 40633 - Naz'jar Honor Guard
struct npc_nazjar_nazjar_honor_guard : public ScriptedAI
{
    npc_nazjar_nazjar_honor_guard(Creature* creature) : ScriptedAI(creature), _enraged(false) { }

    void JustEngagedWith(Unit* /*who*/) override
    {
        _events.ScheduleEvent(EVENT_ARC_SLASH, 5s, 6s);
    }

    void DamageTaken(Unit* /*attacker*/, uint32& damage, DamageEffectType /*damageType*/, SpellInfo const* /*spellInfo*/) override
    {
        if (me->HealthBelowPctDamaged(35, damage) && !_enraged)
        {
            DoCastSelf(SPELL_ENRAGE);
            _enraged = true;
        }
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim())
            return;

        _events.Update(diff);

        if (me->HasUnitState(UNIT_STATE_CASTING))
            return;

        while (uint32 eventId = _events.ExecuteEvent())
        {
            switch (eventId)
            {
                case EVENT_ARC_SLASH:
                    DoCastAOE(SPELL_ARC_SLASH);
                    _events.Repeat(5s, 6s);
                    break;
                default:
                    break;
            }
        }
    }

private:
    EventMap _events;
    bool _enraged;
};

// 44404 - Naz'jar Tempest Witch
struct npc_nazjar_nazjar_tempest_witch : public ScriptedAI
{
    npc_nazjar_nazjar_tempest_witch(Creature* creature) : ScriptedAI(creature) { }

    void AttackStart(Unit* who) override
    {
        AttackStartCaster(who, 15.0f);
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        me->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_INTERRUPT, true);
        _events.ScheduleEvent(EVENT_CHAIN_LIGHTNING, 1s, 2s);
        _events.ScheduleEvent(EVENT_LIGHTNING_SURGE, 14s, 16s);
    }

    void OnSpellCast(SpellInfo const* spell) override
    {
        if (spell->Id == SPELL_CHAIN_LIGHTNING)
            me->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_INTERRUPT, true);
    }

    void OnSpellFailed(SpellInfo const* spell) override
    {
        if (spell->Id == SPELL_CHAIN_LIGHTNING)
            me->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_INTERRUPT, true);
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim())
            return;

        _events.Update(diff);

        if (me->HasUnitState(UNIT_STATE_CASTING))
            return;

        while (uint32 eventId = _events.ExecuteEvent())
        {
            switch (eventId)
            {
                case EVENT_CHAIN_LIGHTNING:
                    if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0, 50.0f, true))
                    {
                        me->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_INTERRUPT, false);
                        DoCast(target, SPELL_CHAIN_LIGHTNING);
                    }
                    _events.Repeat(2s + 500ms);
                    break;
                case EVENT_LIGHTNING_SURGE:
                    if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0, 50.0f, true))
                        DoCast(target, SPELL_LIGHTNING_SURGE);
                    _events.Repeat(14s, 16s);
                    break;
                default:
                    break;
            }
        }
    }

private:
    EventMap _events;
};

// 90440 - Waterspout
class spell_nazjar_waterspout : public SpellScript
{
    void HandleScriptEffect(SpellEffIndex effIndex) const
    {
        if (Unit* caster = GetCaster())
            GetHitUnit()->CastSpell(caster, GetSpellInfo()->GetEffect(effIndex).BasePoints, true);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_nazjar_waterspout::HandleScriptEffect, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT);
    }
};

// 90444 - Waterspout Targeting
class spell_nazjar_waterspout_targeting : public SpellScript
{
    void FilterTargets(std::list<WorldObject*>& targets) const
    {
        if (targets.empty())
            return;

        InstanceScript* instance = GetCaster()->GetInstanceScript();
        if (!instance)
            return;

        if (Creature* nazjar = instance->GetCreature(BOSS_LADY_NAZJAR))
        {
            targets.remove_if([nazjar](WorldObject const* obj) -> bool
            {
                return nazjar->GetVictim() == obj;
            });
        }

        if (!targets.empty())
            Trinity::Containers::RandomResize(targets, 1);
    }

    void HandleScriptEffect(SpellEffIndex effIndex) const
    {
        if (Unit* caster = GetCaster())
            caster->CastSpell(GetHitUnit(), GetSpellInfo()->GetEffect(effIndex).BasePoints, true);
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_nazjar_waterspout_targeting::FilterTargets, EFFECT_0, TARGET_UNIT_SRC_AREA_ENEMY);
        OnEffectHitTarget += SpellEffectFn(spell_nazjar_waterspout_targeting::HandleScriptEffect, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT);
    }
};

void AddSC_boss_lady_nazjar()
{
    RegisterThroneOfTheTidesCreatureAI(boss_lady_nazjar);
    RegisterThroneOfTheTidesCreatureAI(npc_nazjar_nazjar_honor_guard);
    RegisterThroneOfTheTidesCreatureAI(npc_nazjar_nazjar_tempest_witch);
    RegisterSpellScript(spell_nazjar_waterspout);
    RegisterSpellScript(spell_nazjar_waterspout_targeting);
}
