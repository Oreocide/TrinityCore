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
 * Hungering Destroyer (DungeonEncounter 2383). One phase: a fixed loop anchored on the pull, 101.1 s on Normal and 96.0 s on
 * Heroic, that ends with Consume; every ability restarts at the same offset in the next loop. Gluttonous Miasma runs outside the
 * loop on the boss aura 330287 (24 s period, first tick on apply). Offsets are the medians of 30 Normal and 16 Heroic Warcraft Logs
 * kills and agree with DBM, BigWigs and a retail capture (trinitycore-tracking/castle-nathria/bosses/hungering-destroyer.md).
 *
 * Not done: Growing Hunger 332295 is applied as retail does, but its aura (SPELL_AURA_MOD_DAMAGE_DONE_FOR_MECHANIC, 276) has no
 * handler in this core; Expunge damages only the marked player (retail's radius grows with missing health); Consume's drain does
 * not fall off with distance; the Power Tracker energy bar and the 600 s berserk (spell unknown) are not modelled.
 */

#include "CellImpl.h"
#include "Containers.h"
#include "GridNotifiersImpl.h"
#include "InstanceScript.h"
#include "Map.h"
#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "SpellAuraEffects.h"
#include "SpellScript.h"
#include "castle_nathria.h"

enum HungeringDestroyerSpells
{
    SPELL_HUNGERING_STRIKES_AURA    = 332294,   // proc on melee: Hungering Strikes leech
    SPELL_HUNGERING_STRIKES         = 332296,
    SPELL_GROWING_HUNGER            = 332295,
    SPELL_GLUTTONOUS_MIASMA_AURA    = 330287,   // periodic trigger of 329295 every 24 s
    SPELL_GLUTTONOUS_MIASMA         = 329298,
    SPELL_GLUTTONOUS_MIASMA_LEECH   = 330537,
    SPELL_POWER_TRACKER             = 331565,
    SPELL_OVERWHELM                 = 329774,
    SPELL_VOLATILE_EJECTION_CAST    = 334266,   // 4.5 s cast; the beam fires at each marked player when it completes
    SPELL_VOLATILE_EJECTION_MARK    = 334064,
    SPELL_VOLATILE_EJECTION_BEAM    = 334228,
    SPELL_DESOLATE                  = 329455,
    SPELL_EXPUNGE_SELECTOR          = 329719,   // every player casts Expunge on itself
    SPELL_EXPUNGE                   = 329725,
    SPELL_EXPUNGE_DAMAGE            = 329742,
    SPELL_CONSUME                   = 334522
};

enum HungeringDestroyerEvents
{
    EVENT_LOOP = 1,
    EVENT_OVERWHELM,
    EVENT_VOLATILE_EJECTION,
    EVENT_DESOLATE,
    EVENT_EXPUNGE,
    EVENT_CONSUME,
    EVENT_POWER_TRACKER,
    EVENT_MIASMA_AURA
};

enum HungeringDestroyerTexts
{
    SAY_EMOTE_DESOLATE              = 0,
    SAY_EMOTE_CONSUME               = 1
};

struct HungeringDestroyerLoop
{
    Milliseconds Period;
    std::array<Milliseconds, 7> Overwhelm;
    std::array<Milliseconds, 3> VolatileEjection;
    std::array<Milliseconds, 2> Desolate;
    std::array<Milliseconds, 2> Expunge;
    Milliseconds Consume;
};

static constexpr HungeringDestroyerLoop NormalLoop =
{
    101100ms,
    { 5300ms, 17900ms, 30500ms, 43200ms, 55800ms, 68400ms, 81100ms },
    { 10500ms, 48400ms, 86300ms },
    { 23200ms, 61100ms },
    { 33700ms, 71600ms },
    93700ms
};

static constexpr HungeringDestroyerLoop HeroicLoop =
{
    96000ms,
    { 5000ms, 17000ms, 29000ms, 41000ms, 53000ms, 65000ms, 77000ms },
    { 10000ms, 46000ms, 82000ms },
    { 22000ms, 58000ms },
    { 32000ms, 68000ms },
    89000ms
};

static constexpr Milliseconds MiasmaAuraDelay = 3s;
static constexpr Milliseconds PowerTrackerDelay = 3s + 150ms;
static constexpr Milliseconds PowerTrackerAfterConsume = 10s + 500ms;   // 4 s cast + 6 s channel + 0.5 s (retail 93.7 -> 104.2)

// 164261 - Hungering Destroyer
struct boss_hungering_destroyer : public BossAI
{
    boss_hungering_destroyer(Creature* creature) : BossAI(creature, DATA_HUNGERING_DESTROYER) { }

    HungeringDestroyerLoop const& Loop() const
    {
        return me->GetMap()->IsHeroicOrHigher() ? HeroicLoop : NormalLoop;
    }

    void JustEngagedWith(Unit* who) override
    {
        BossAI::JustEngagedWith(who);
        DoCastSelf(SPELL_HUNGERING_STRIKES_AURA, true);
        DoCastSelf(SPELL_GROWING_HUNGER, true);
        events.ScheduleEvent(EVENT_POWER_TRACKER, PowerTrackerDelay);
        events.ScheduleEvent(EVENT_MIASMA_AURA, MiasmaAuraDelay);
        events.ScheduleEvent(EVENT_LOOP, 0ms);
    }

    void ScheduleLoop()
    {
        HungeringDestroyerLoop const& loop = Loop();
        for (Milliseconds offset : loop.Overwhelm)
            events.ScheduleEvent(EVENT_OVERWHELM, offset);
        for (Milliseconds offset : loop.VolatileEjection)
            events.ScheduleEvent(EVENT_VOLATILE_EJECTION, offset);
        for (Milliseconds offset : loop.Desolate)
            events.ScheduleEvent(EVENT_DESOLATE, offset);
        for (Milliseconds offset : loop.Expunge)
            events.ScheduleEvent(EVENT_EXPUNGE, offset);
        events.ScheduleEvent(EVENT_CONSUME, loop.Consume);
        events.ScheduleEvent(EVENT_LOOP, loop.Period);
    }

    void ExecuteEvent(uint32 eventId) override
    {
        switch (eventId)
        {
            case EVENT_LOOP:
                ScheduleLoop();
                break;
            case EVENT_POWER_TRACKER:
                DoCastSelf(SPELL_POWER_TRACKER, true);
                break;
            case EVENT_MIASMA_AURA:
                DoCastSelf(SPELL_GLUTTONOUS_MIASMA_AURA, true);
                break;
            case EVENT_OVERWHELM:
                DoCastVictim(SPELL_OVERWHELM);
                break;
            case EVENT_VOLATILE_EJECTION:
                // retail starts the cast and marks the targets in the same tick
                DoCastSelf(SPELL_VOLATILE_EJECTION_CAST);
                DoCastAOE(SPELL_VOLATILE_EJECTION_MARK, true);
                break;
            case EVENT_DESOLATE:
                DoCastSelf(SPELL_DESOLATE);
                Talk(SAY_EMOTE_DESOLATE);
                break;
            case EVENT_EXPUNGE:
                DoCastAOE(SPELL_EXPUNGE_SELECTOR, true);
                break;
            case EVENT_CONSUME:
                me->RemoveAurasDueToSpell(SPELL_POWER_TRACKER);
                DoCastSelf(SPELL_CONSUME);
                Talk(SAY_EMOTE_CONSUME);
                events.ScheduleEvent(EVENT_POWER_TRACKER, PowerTrackerAfterConsume);
                break;
            default:
                break;
        }
    }
};

// 332294 - Hungering Strikes: the melee swing or ability that procs it leeches the target
class spell_hungering_destroyer_hungering_strikes : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_HUNGERING_STRIKES });
    }

    void HandleProc(AuraEffect const* /*aurEff*/, ProcEventInfo& eventInfo)
    {
        if (Unit* target = eventInfo.GetActionTarget())
            GetTarget()->CastSpell(target, SPELL_HUNGERING_STRIKES, CastSpellExtraArgs(TRIGGERED_FULL_MASK).SetTriggeringAura(GetEffect(EFFECT_0)));
    }

    void Register() override
    {
        OnEffectProc += AuraEffectProcFn(spell_hungering_destroyer_hungering_strikes::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// 329295 - Gluttonous Miasma (selector): random players other than the current tank; 2 below 15 players, 3 from 15, 4 from 23 on
// Heroic (Warcraft Logs counts by raid size); Mythic keeps the spell's own limit of 4
class spell_hungering_destroyer_gluttonous_miasma_selector : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_GLUTTONOUS_MIASMA });
    }

    void FilterTargets(std::list<WorldObject*>& targets)
    {
        Unit* caster = GetCaster();
        Unit* tank = caster->GetVictim();
        targets.remove_if([tank](WorldObject* target)
        {
            return target == tank || !target->IsPlayer();
        });

        Map* map = caster->GetMap();
        uint32 players = map->GetPlayersCountExceptGMs();
        uint32 count = players < 15 ? 2 : 3;
        if (map->IsHeroic() && players >= 23)
            count = 4;
        if (map->IsMythic())
            count = 4;
        Trinity::Containers::RandomResize(targets, count);
    }

    void HandleHit(SpellEffIndex /*effIndex*/)
    {
        GetCaster()->CastSpell(GetHitUnit(), SPELL_GLUTTONOUS_MIASMA, true);
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_hungering_destroyer_gluttonous_miasma_selector::FilterTargets, EFFECT_0, TARGET_UNIT_SRC_AREA_ENEMY);
        OnEffectHitTarget += SpellEffectFn(spell_hungering_destroyer_gluttonous_miasma_selector::HandleHit, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 329298 - Gluttonous Miasma: every 1.5 s the afflicted player drains the allies around it (330537)
class spell_hungering_destroyer_gluttonous_miasma : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_GLUTTONOUS_MIASMA_LEECH });
    }

    void HandlePeriodic(AuraEffect const* aurEff)
    {
        GetTarget()->CastSpell(GetTarget(), SPELL_GLUTTONOUS_MIASMA_LEECH, CastSpellExtraArgs(aurEff).SetOriginalCaster(GetCasterGUID()));
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_hungering_destroyer_gluttonous_miasma::HandlePeriodic, EFFECT_1, SPELL_AURA_PERIODIC_DUMMY);
    }
};

// 334266 - Volatile Ejection: when the 4.5 s cast completes, the beam fires at every player the boss marked with 334064
class spell_hungering_destroyer_volatile_ejection : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_VOLATILE_EJECTION_MARK, SPELL_VOLATILE_EJECTION_BEAM });
    }

    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        Unit* caster = GetCaster();
        std::vector<Unit*> marked;
        for (MapReference const& ref : caster->GetMap()->GetPlayers())
            if (Player* player = ref.GetSource())
                if (player->HasAura(SPELL_VOLATILE_EJECTION_MARK, caster->GetGUID()))
                    marked.push_back(player);

        for (Unit* target : marked)
        {
            target->RemoveAurasDueToSpell(SPELL_VOLATILE_EJECTION_MARK, caster->GetGUID());
            caster->CastSpell(target, SPELL_VOLATILE_EJECTION_BEAM, true);
        }
    }

    void Register() override
    {
        OnEffectHit += SpellEffectFn(spell_hungering_destroyer_volatile_ejection::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 329719 - Expunge (selector): every player hit casts Expunge on itself, with the boss as the original caster
class spell_hungering_destroyer_expunge_selector : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_EXPUNGE });
    }

    void HandleHit(SpellEffIndex /*effIndex*/)
    {
        Unit* target = GetHitUnit();
        target->CastSpell(target, SPELL_EXPUNGE, CastSpellExtraArgs(TRIGGERED_FULL_MASK).SetOriginalCaster(GetCaster()->GetGUID()));
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_hungering_destroyer_expunge_selector::HandleHit, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 329725 - Expunge: when the 6 s aura expires, the boss strikes the marked player
class spell_hungering_destroyer_expunge : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_EXPUNGE_DAMAGE });
    }

    void AfterRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        if (GetTargetApplication()->GetRemoveMode() != AURA_REMOVE_BY_EXPIRE)
            return;

        if (Unit* boss = GetCaster())
            boss->CastSpell(GetTarget(), SPELL_EXPUNGE_DAMAGE, true);
    }

    void Register() override
    {
        AfterEffectRemove += AuraEffectRemoveFn(spell_hungering_destroyer_expunge::AfterRemove, EFFECT_0, SPELL_AURA_AREA_TRIGGER, AURA_EFFECT_HANDLE_REAL);
    }
};

void AddSC_boss_hungering_destroyer()
{
    RegisterCastleNathriaCreatureAI(boss_hungering_destroyer);
    RegisterSpellScript(spell_hungering_destroyer_hungering_strikes);
    RegisterSpellScript(spell_hungering_destroyer_gluttonous_miasma_selector);
    RegisterSpellScript(spell_hungering_destroyer_gluttonous_miasma);
    RegisterSpellScript(spell_hungering_destroyer_volatile_ejection);
    RegisterSpellScript(spell_hungering_destroyer_expunge_selector);
    RegisterSpellScript(spell_hungering_destroyer_expunge);
}
