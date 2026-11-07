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
 * Growing Hunger (capture and 60 Normal / 20 Heroic Warcraft Logs pulls): each Hungering Strikes proc on the same target as the
 * last one adds a stack (18 s, refreshed by every stack), the first proc on another target removes the aura and adds none. The
 * core applies the aura (SPELL_AURA_MOD_DAMAGE_DONE_FOR_MECHANIC, mechanic 35) to the leech at the spell's 7.5% per stack; retail
 * measures about 8.1%. Expunge: when the 6 s mark expires every player inside that player's circle takes the eruption at full
 * damage; the circle is 5 yd while the marked player is at full health and 6 yd with any health missing (larger losses were never
 * captured). Berserk: Mythic only, at 420 s (Warcraft Logs, DBM, BigWigs); Normal and Heroic pulls never reached a berserk.
 *
 * Not researched yet: Consume's drain falloff with distance and the Power Tracker energy bar.
 */

#include "AreaTrigger.h"
#include "AreaTriggerAI.h"
#include "CellImpl.h"
#include "Containers.h"
#include "GridNotifiersImpl.h"
#include "InstanceScript.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "SpellAuraEffects.h"
#include "SpellAuras.h"
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
    SPELL_CONSUME                   = 334522,
    SPELL_BERSERK                   = 26662
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
    EVENT_MIASMA_AURA,
    EVENT_BERSERK
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
static constexpr Milliseconds MythicBerserk = 420s;
static constexpr Milliseconds ExpungeScaleStep = 500ms;                 // retail re-aims the circle every 0.5 s
static constexpr float ExpungeScaleFullHealth = 5.0f;
static constexpr float ExpungeScaleMissingHealth = 6.0f;

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
        // Growing Hunger comes with the first Hungering Strikes proc (capture: 0.031 s)
        DoCastSelf(SPELL_HUNGERING_STRIKES_AURA, true);
        events.ScheduleEvent(EVENT_POWER_TRACKER, PowerTrackerDelay);
        events.ScheduleEvent(EVENT_MIASMA_AURA, MiasmaAuraDelay);
        events.ScheduleEvent(EVENT_LOOP, 0ms);
        if (me->GetMap()->IsMythic())
            events.ScheduleEvent(EVENT_BERSERK, MythicBerserk);
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
            case EVENT_BERSERK:
                DoCastSelf(SPELL_BERSERK, true);
                break;
            default:
                break;
        }
    }
};

// 332294 - Hungering Strikes: the melee swing or ability that procs it (0.5 s proc cooldown) leeches the target; Growing Hunger
// gains a stack before the leech when the target is the same as last time, and is removed when it is not
class spell_hungering_destroyer_hungering_strikes : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_HUNGERING_STRIKES, SPELL_GROWING_HUNGER });
    }

    void HandleProc(AuraEffect const* aurEff, ProcEventInfo& eventInfo)
    {
        Unit* boss = GetTarget();
        Unit* target = eventInfo.GetActionTarget();
        if (!target)
            return;

        if (!_lastTarget.IsEmpty() && _lastTarget != target->GetGUID())
            boss->RemoveAurasDueToSpell(SPELL_GROWING_HUNGER);
        else
            boss->CastSpell(boss, SPELL_GROWING_HUNGER, CastSpellExtraArgs(aurEff));
        _lastTarget = target->GetGUID();

        boss->CastSpell(target, SPELL_HUNGERING_STRIKES, CastSpellExtraArgs(aurEff));
    }

    void Register() override
    {
        OnEffectProc += AuraEffectProcFn(spell_hungering_destroyer_hungering_strikes::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }

    ObjectGuid _lastTarget;
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

// 329725 - Expunge: when the 6 s aura expires, every player inside the marked player's circle (the marked player too) takes a full
// eruption; overlapping circles hit a player once per circle (Warcraft Logs: no split, hits per detonation grow with raid size)
class spell_hungering_destroyer_expunge : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_EXPUNGE_DAMAGE });
    }

    // runs before the default handler removes the areatrigger, so its inside list is still there
    void HandleRemove(AuraEffect const* aurEff, AuraEffectHandleModes /*mode*/)
    {
        if (GetTargetApplication()->GetRemoveMode() != AURA_REMOVE_BY_EXPIRE)
            return;

        Unit* boss = GetCaster();
        if (!boss)
            return;

        Unit* owner = GetTarget();
        std::vector<Unit*> victims = { owner };
        for (AreaTrigger const* at : boss->GetAreaTriggers(SPELL_EXPUNGE))
        {
            if (at->GetAuraEffect() != aurEff)
                continue;

            for (ObjectGuid const& guid : at->GetInsideUnits())
                if (Player* player = ObjectAccessor::GetPlayer(*owner, guid))
                    if (player != owner && player->IsAlive())
                        victims.push_back(player);
        }

        for (Unit* victim : victims)
            boss->CastSpell(victim, SPELL_EXPUNGE_DAMAGE, true);
    }

    void Register() override
    {
        OnEffectRemove += AuraEffectRemoveFn(spell_hungering_destroyer_expunge::HandleRemove, EFFECT_0, SPELL_AURA_AREA_TRIGGER, AURA_EFFECT_HANDLE_REAL);
    }
};

// 329725 - Expunge (areatrigger create properties 20517): every 0.5 s, and when the marked player's health crosses full, retail
// re-aims the circle's scale from its current value to 5 (full health) or 6 (any health missing) over 0.5 s
struct at_hungering_destroyer_expunge : AreaTriggerAI
{
    using AreaTriggerAI::AreaTriggerAI;

    void OnCreate(Spell const* /*creatingSpell*/) override
    {
        at->SetTimeToTargetScale(ExpungeScaleStep.count());
        UpdateScale();
    }

    void OnUpdate(uint32 diff) override
    {
        _sinceUpdate += Milliseconds(diff);
        if (_sinceUpdate >= ExpungeScaleStep || IsOwnerAtFullHealth() != _fullHealth)
            UpdateScale();
    }

    bool IsOwnerAtFullHealth() const
    {
        Unit* owner = at->GetTarget();
        return !owner || owner->IsFullHealth();
    }

    void UpdateScale()
    {
        _sinceUpdate = 0ms;
        _fullHealth = IsOwnerAtFullHealth();
        std::array<DBCPosition2D, 2> points =
        { {
            { 0.0f, at->CalcCurrentScale() },
            { 1.0f, _fullHealth ? ExpungeScaleFullHealth : ExpungeScaleMissingHealth }
        } };
        at->SetOverrideScaleCurve(points);
    }

private:
    Milliseconds _sinceUpdate = 0ms;
    bool _fullHealth = true;
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
    RegisterAreaTriggerAI(at_hungering_destroyer_expunge);
}
