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
 * Echelon (DungeonEncounter 2380). One phase. Offsets are Warcraft Logs medians (17 Mythic no-keystone pulls) and agree with the
 * 2026-11-10 Heroic capture within its 1 s clock step (trinitycore-tracking/sl-dungeons/halls-of-atonement/bosses/echelon.md):
 * Blood Torrent 4.7 s, Stone Call 9.7 s, Flesh to Stone 20.5 s and every 31.6 s, Stone Shattering Leap 3 s after each Flesh.
 *
 * The energy bar (Echelon Energize, +5 per second) is shown, not read: the cast times above come from the logs. The capture
 * resets it 10 s after Flesh to Stone, which is done here 7 s after the Leap starts.
 * Blood Torrent and Stone Call wait behind the Leap: the logs never show another cast until about 0.7 s after it lands.
 *
 * Flesh to Stone (Heroic): the target gets a Flesh to Stone slow stack every second (4 stacks at -25% each) and is turned to
 * stone for 3 s with the fourth. Mythic stuns everyone around the target (spell 319611 difficulty 23); that variant is not handled.
 * Undying Stonefiends: lethal damage leaves one at 1 health in Stone Form (stunned, heals 5% per 1.5 s for 30 s, takes no
 * damage) and every transformation hurts all players; the Leap's landing destroys the stoned ones within 8 yd.
 *
 * Not settled: how retail picks the Flesh to Stone, Leap and Blood Torrent targets (one solo capture; random players are used),
 * whether the two Blood Torrent yells alternate, and the Mythic only Villainous Bolt (not scripted).
 */

#include "AreaTrigger.h"
#include "AreaTriggerAI.h"
#include "Containers.h"
#include "InstanceScript.h"
#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "SpellAuraEffects.h"
#include "SpellAuras.h"
#include "SpellScript.h"
#include "halls_of_atonement.h"

enum EchelonSpells
{
    SPELL_ECHELON_ENERGIZE              = 319616,   // periodic dummy, +5 energy per second
    SPELL_BLOOD_TORRENT                 = 326389,   // 1.5 s cast, missile to a destination
    SPELL_BLOOD_TORRENT_MISSILE         = 319702,   // creates the pool areatrigger
    SPELL_BLOOD_TORRENT_POOL            = 319703,
    SPELL_STONE_CALL                    = 319733,
    SPELL_FLESH_TO_STONE                = 328206,   // 1.5 s cast, area dummy
    SPELL_FLESH_TO_STONE_MISSILE        = 327411,
    SPELL_FLESH_TO_STONE_AURA           = 319603,   // periodic dummy, 5 s
    SPELL_FLESH_TO_STONE_SLOW           = 319602,   // 4 stacks
    SPELL_TURNED_TO_STONE               = 319611,
    SPELL_STONE_SHATTERING_LEAP         = 319941,   // 5 s cast
    SPELL_STONE_SHATTERING_LEAP_LANDING = 319592,
    SPELL_SHATTERED                     = 344874,
    SPELL_STONE_FORM                    = 319724,
    SPELL_VOLATILE_TRANSFORMATION       = 328125,
    SPELL_STONE_DESTRUCTION             = 319965,
    SPELL_BREAK_PLAYER_TARGETING        = 140562
};

enum EchelonEvents
{
    EVENT_BLOOD_TORRENT = 1,
    EVENT_STONE_CALL,
    EVENT_FLESH_TO_STONE,
    EVENT_STONE_SHATTERING_LEAP,
    EVENT_ENERGY_RESET
};

enum EchelonActions
{
    ACTION_STONE_CALL = 1
};

enum EchelonCreatures
{
    NPC_UNDYING_STONEFIEND              = 164363
};

enum Texts
{
    SAY_AGGRO                           = 0,    // Begone, rebel filth!
    SAY_BLOOD_TORRENT                   = 1,    // Blood take you!
    SAY_STONE_CALL                      = 2,    // Slay the trespassers!
    SAY_FLESH_TO_STONE                  = 3,    // Your fate is carved in stone!
    SAY_LEAP_WHISPER                    = 4,    // raid boss whisper to the Leap's target
    SAY_BLOOD_TORRENT_2                 = 5     // The ground bleeds!
};

static constexpr Milliseconds BloodTorrentFirst = 4700ms;
static constexpr Milliseconds BloodTorrentInterval = 19400ms;   // Medium: logs show 17 to 25 s, the long gaps are queueing
static constexpr Milliseconds StoneCallFirst = 9700ms;
static constexpr Milliseconds StoneCallInterval = 48800ms;
static constexpr Milliseconds FleshToStoneFirst = 20500ms;
static constexpr Milliseconds FleshToStoneInterval = 31600ms;
static constexpr Milliseconds LeapAfterFlesh = 3s;
static constexpr Milliseconds LeapHold = 6700ms;                // 5 s cast, 1 s jump, 0.7 s; other casts wait this long
static constexpr Milliseconds EnergyResetAfterLeap = 7s;
static constexpr Milliseconds StonefiendWestDelay = 1s;
static constexpr Milliseconds StonefiendCorpseTime = 8s;
static constexpr int32 EnergyPerTick = 5;
static constexpr float LeapShatterRadius = 8.0f;
static constexpr float BloodTorrentRange = 60.0f;
static constexpr float LeapRange = 100.0f;

static constexpr Position StonefiendEastWall[] =
{
    { -2175.648f, 5500.088f, 4170.372f },
    { -2175.755f, 5471.127f, 4170.372f },
    { -2175.368f, 5485.679f, 4170.372f }
};

static constexpr Position StonefiendWestWall[] =
{
    { -2231.693f, 5472.635f, 4170.4f },
    { -2231.710f, 5485.155f, 4170.4f },
    { -2232.216f, 5497.861f, 4170.4f }
};

// 164185 - Echelon
struct boss_echelon : public BossAI
{
    boss_echelon(Creature* creature) : BossAI(creature, DATA_ECHELON) { }

    void Reset() override
    {
        BossAI::Reset();
        _torrentCasts = 0;
        me->SetPower(POWER_ENERGY, 0);
        Cleanup();
    }

    void JustEngagedWith(Unit* who) override
    {
        BossAI::JustEngagedWith(who);
        Talk(SAY_AGGRO);
        me->SetPower(POWER_ENERGY, 0);
        DoCastSelf(SPELL_ECHELON_ENERGIZE, true);
    }

    void ScheduleTasks() override
    {
        events.ScheduleEvent(EVENT_BLOOD_TORRENT, BloodTorrentFirst);
        events.ScheduleEvent(EVENT_STONE_CALL, StoneCallFirst);
        events.ScheduleEvent(EVENT_FLESH_TO_STONE, FleshToStoneFirst);
    }

    void JustDied(Unit* killer) override
    {
        BossAI::JustDied(killer);
        Cleanup();
    }

    void JustSummoned(Creature* summon) override
    {
        BossAI::JustSummoned(summon);
        DoZoneInCombat(summon);
    }

    void DoAction(int32 action) override
    {
        if (action != ACTION_STONE_CALL)
            return;

        // retail: three at the east wall when the cast completes, three at the west wall a second later
        for (Position const& pos : StonefiendEastWall)
            SummonStonefiend(pos);

        scheduler.Schedule(StonefiendWestDelay, [this](TaskContext)
        {
            for (Position const& pos : StonefiendWestWall)
                SummonStonefiend(pos);
        });
    }

    void UpdateAI(uint32 diff) override
    {
        scheduler.Update(diff);
        BossAI::UpdateAI(diff);
    }

    void ExecuteEvent(uint32 eventId) override
    {
        switch (eventId)
        {
            case EVENT_BLOOD_TORRENT:
                if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0, BloodTorrentRange, true))
                {
                    // the missile only has a destination: the pool lands where the player stands
                    me->CastSpell(target->GetPosition(), SPELL_BLOOD_TORRENT);
                    Talk((_torrentCasts % 2) ? SAY_BLOOD_TORRENT_2 : SAY_BLOOD_TORRENT);
                    ++_torrentCasts;
                }
                events.Repeat(BloodTorrentInterval);
                break;
            case EVENT_STONE_CALL:
                DoCastSelf(SPELL_STONE_CALL);
                Talk(SAY_STONE_CALL);
                events.Repeat(StoneCallInterval);
                break;
            case EVENT_FLESH_TO_STONE:
                DoCastSelf(SPELL_FLESH_TO_STONE);
                Talk(SAY_FLESH_TO_STONE);
                events.ScheduleEvent(EVENT_STONE_SHATTERING_LEAP, LeapAfterFlesh);
                events.Repeat(FleshToStoneInterval);
                break;
            case EVENT_STONE_SHATTERING_LEAP:
                if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0, LeapRange, true))
                {
                    Talk(SAY_LEAP_WHISPER, target);
                    DoCast(target, SPELL_STONE_SHATTERING_LEAP);
                    events.SetMinimalDelay(EVENT_BLOOD_TORRENT, LeapHold);
                    events.SetMinimalDelay(EVENT_STONE_CALL, LeapHold);
                    events.ScheduleEvent(EVENT_ENERGY_RESET, EnergyResetAfterLeap);
                }
                break;
            case EVENT_ENERGY_RESET:
                me->SetPower(POWER_ENERGY, 0);
                break;
            default:
                break;
        }
    }

private:
    void SummonStonefiend(Position const& pos)
    {
        me->SummonCreature(NPC_UNDYING_STONEFIEND, pos, TEMPSUMMON_CORPSE_TIMED_DESPAWN, StonefiendCorpseTime);
    }

    void Cleanup()
    {
        me->RemoveAllAreaTriggers();
        if (!instance)
            return;

        instance->DoRemoveAurasDueToSpellOnPlayers(SPELL_FLESH_TO_STONE_AURA);
        instance->DoRemoveAurasDueToSpellOnPlayers(SPELL_FLESH_TO_STONE_SLOW);
        instance->DoRemoveAurasDueToSpellOnPlayers(SPELL_TURNED_TO_STONE);
        instance->DoRemoveAurasDueToSpellOnPlayers(SPELL_BLOOD_TORRENT_POOL);
        instance->DoRemoveAurasDueToSpellOnPlayers(SPELL_SHATTERED);
    }

    uint32 _torrentCasts = 0;
};

// 164363 - Undying Stonefiend
struct npc_echelon_undying_stonefiend : public ScriptedAI
{
    npc_echelon_undying_stonefiend(Creature* creature) : ScriptedAI(creature) { }

    void DamageTaken(Unit* /*attacker*/, uint32& damage, DamageEffectType /*damageType*/, SpellInfo const* /*spellInfo*/) override
    {
        if (me->HasAura(SPELL_STONE_FORM))
        {
            damage = 0;
            return;
        }

        if (damage < me->GetHealth())
            return;

        // Undying Stone (328124): reaching 0 health explodes and turns the Stonefiend to stone instead of killing it
        damage = me->GetHealth() - 1;
        DoCastSelf(SPELL_VOLATILE_TRANSFORMATION, true);
        DoCastSelf(SPELL_STONE_FORM, true);
    }

    void UpdateAI(uint32 /*diff*/) override
    {
        if (!UpdateVictim())
            return;
    }
};

// 319616 - Echelon Energize
class spell_hoa_echelon_energize : public AuraScript
{
    void HandlePeriodic(AuraEffect const* /*aurEff*/)
    {
        GetTarget()->ModifyPower(POWER_ENERGY, EnergyPerTick);
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_hoa_echelon_energize::HandlePeriodic, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY);
    }
};

// 319733 - Stone Call: effect 0 carries the summon spell 319734 and effect 1 the count (6); the Stonefiends are summoned by the AI
class spell_hoa_echelon_stone_call : public SpellScript
{
    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        if (Creature* caster = GetCaster()->ToCreature())
            if (CreatureAI* ai = caster->AI())
                ai->DoAction(ACTION_STONE_CALL);
    }

    void Register() override
    {
        OnEffectHit += SpellEffectFn(spell_hoa_echelon_stone_call::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 328206 - Flesh to Stone: the area dummy hits one random player, who gets the missile
class spell_hoa_echelon_flesh_to_stone : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_FLESH_TO_STONE_MISSILE });
    }

    void FilterTargets(std::list<WorldObject*>& targets)
    {
        targets.remove_if([](WorldObject const* target) { return !target->IsPlayer(); });
        Trinity::Containers::RandomResize(targets, 1);
    }

    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        GetCaster()->CastSpell(GetHitUnit(), SPELL_FLESH_TO_STONE_MISSILE, true);
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_hoa_echelon_flesh_to_stone::FilterTargets, EFFECT_0, TARGET_UNIT_DEST_AREA_ENEMY);
        OnEffectHitTarget += SpellEffectFn(spell_hoa_echelon_flesh_to_stone::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 319603 - Flesh to Stone: every second the target slows itself one stack more; the fourth stack turns it to stone
class spell_hoa_echelon_flesh_to_stone_aura : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_FLESH_TO_STONE_SLOW, SPELL_TURNED_TO_STONE });
    }

    void HandlePeriodic(AuraEffect const* aurEff)
    {
        Unit* target = GetTarget();
        target->CastSpell(target, SPELL_FLESH_TO_STONE_SLOW, aurEff);

        Aura const* slow = target->GetAura(SPELL_FLESH_TO_STONE_SLOW);
        if (slow && slow->GetStackAmount() >= slow->GetSpellInfo()->StackAmount && !target->HasAura(SPELL_TURNED_TO_STONE))
            target->CastSpell(target, SPELL_TURNED_TO_STONE, aurEff);
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_hoa_echelon_flesh_to_stone_aura::HandlePeriodic, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY);
    }
};

// 319602 - Flesh to Stone: the spell data holds +25 on a speed decrease, which the core ignores; the slow is 25% per stack
class spell_hoa_echelon_flesh_to_stone_slow : public AuraScript
{
    void CalcAmount(AuraEffect const* /*aurEff*/, SpellEffectValue& amount, bool& /*canBeRecalculated*/)
    {
        amount = -std::abs(amount);
    }

    void Register() override
    {
        DoEffectCalcAmount += AuraEffectCalcAmountFn(spell_hoa_echelon_flesh_to_stone_slow::CalcAmount, EFFECT_0, SPELL_AURA_MOD_DECREASE_SPEED);
    }
};

// 319592 - Stone Shattering Leap (landing): Shattered on every player hit; every Stonefiend in Stone Form within 8 yd is shattered
class spell_hoa_echelon_stone_shattering_leap_landing : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_SHATTERED, SPELL_STONE_DESTRUCTION, SPELL_BREAK_PLAYER_TARGETING });
    }

    void HandleShattered(SpellEffIndex /*effIndex*/)
    {
        if (Unit* target = GetHitUnit())
            GetCaster()->CastSpell(target, SPELL_SHATTERED, true);
    }

    void HandleShatter()
    {
        WorldLocation const* dest = GetExplTargetDest();
        if (!dest)
            return;

        std::list<Creature*> stonefiends;
        GetCaster()->GetCreatureListWithEntryInGrid(stonefiends, NPC_UNDYING_STONEFIEND, 100.0f);
        for (Creature* stonefiend : stonefiends)
        {
            if (!stonefiend->IsAlive() || !stonefiend->HasAura(SPELL_STONE_FORM) || stonefiend->GetExactDist(dest) > LeapShatterRadius)
                continue;

            stonefiend->CastSpell(stonefiend, SPELL_BREAK_PLAYER_TARGETING, true);
            stonefiend->CastSpell(stonefiend, SPELL_STONE_DESTRUCTION, true);
        }
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_hoa_echelon_stone_shattering_leap_landing::HandleShattered, EFFECT_0, SPELL_EFFECT_SCHOOL_DAMAGE);
        AfterCast += SpellCastFn(spell_hoa_echelon_stone_shattering_leap_landing::HandleShatter);
    }
};

// 319702 areatrigger (create properties 19355, 75 s on Heroic): players inside take the Blood Torrent pool aura
struct at_hoa_echelon_blood_torrent : AreaTriggerAI
{
    using AreaTriggerAI::AreaTriggerAI;

    void OnUnitEnter(Unit* unit) override
    {
        Unit* caster = at->GetCaster();
        if (caster && unit->IsPlayer())
            caster->CastSpell(unit, SPELL_BLOOD_TORRENT_POOL, true);
    }

    void OnUnitExit(Unit* unit, AreaTriggerExitReason /*reason*/) override
    {
        if (!unit->IsPlayer() || IsInsideAnotherPool(unit))
            return;

        unit->RemoveAurasDueToSpell(SPELL_BLOOD_TORRENT_POOL, at->GetCasterGuid());
    }

private:
    bool IsInsideAnotherPool(Unit const* unit) const
    {
        Unit* caster = at->GetCaster();
        if (!caster)
            return false;

        for (AreaTrigger const* pool : caster->GetAreaTriggers(SPELL_BLOOD_TORRENT_MISSILE))
            if (pool != at && pool->GetInsideUnits().contains(unit->GetGUID()))
                return true;

        return false;
    }
};

void AddSC_boss_echelon()
{
    RegisterHallsOfAtonementCreatureAI(boss_echelon);
    RegisterHallsOfAtonementCreatureAI(npc_echelon_undying_stonefiend);
    RegisterSpellScript(spell_hoa_echelon_energize);
    RegisterSpellScript(spell_hoa_echelon_stone_call);
    RegisterSpellScript(spell_hoa_echelon_flesh_to_stone);
    RegisterSpellScript(spell_hoa_echelon_flesh_to_stone_aura);
    RegisterSpellScript(spell_hoa_echelon_flesh_to_stone_slow);
    RegisterSpellScript(spell_hoa_echelon_stone_shattering_leap_landing);
    RegisterAreaTriggerAI(at_hoa_echelon_blood_torrent);
}
