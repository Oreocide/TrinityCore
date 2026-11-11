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
 * Halkias, the Sin-Stained Goliath (DungeonEncounter 2401). One phase, no health gates, no texts. The fight is a ladder of Crumbling
 * Slam and Heave Debris that ends in Refracted Sinlight; every Sinlight starts the next ladder (trinitycore-tracking/sl-dungeons/
 * halls-of-atonement/bosses/halkias.md). The opening ladder (first Slam 4.7 s, Heave 12.4 s, Sinlight 33.7 s) is Warcraft Logs
 * (18 Mythic pulls) and agrees with the Heroic capture within its 1 s clock step. After a Sinlight, Mythic follows the Warcraft Logs
 * medians; Normal and Heroic follow the capture (one Heroic kill, 107 s: Medium for the Mythic ladder, Low for the Heroic one),
 * which has Sinlight Visions at +34 s where Mythic has its second Slam, and two Heaves in a row.
 *
 * Refracted Sinlight is only the 322711 cast: its two AREA_TRIGGER auras create the crossing beams on the boss, which follow his
 * position and facing. Not scripted: the beam set-up spell 322913 (it selects an entry within 113 yd, probably Refracted Aetherlight
 * 165647, which no source says how to spawn), Light of Atonement 339235 (Mythic only), and any turning of the boss during the beams.
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
#include "halls_of_atonement.h"
#include <span>

enum HalkiasSpells
{
    SPELL_CRUMBLING_SLAM                = 322936,
    SPELL_CRUMBLING_SLAM_MARK           = 1235214,  // 2.5 s dummy aura on the Slam target, applied with every Slam start in the capture
    SPELL_HEAVE_DEBRIS                  = 322943,   // 3 s cast at a player: the missile 322945 lands on the target's position
    SPELL_HEAVE_DEBRIS_LAUNCHER         = 322974,
    SPELL_REFRACTED_SINLIGHT            = 322711,   // 2 s cast, 13 s of two crossing beams attached to the boss
    SPELL_REFRACTED_SINLIGHT_TICK       = 337929,   // aura on a player inside a beam, every 250 ms
    SPELL_REFRACTED_SINLIGHT_DAMAGE     = 324044,
    SPELL_SINLIGHT_VISIONS              = 322977,   // 3 s cast, 8 s fear on a player; Normal and Heroic
    SPELL_SINLIGHT_VISIONS_LAUNCHER     = 322982,
    SPELL_GLASS_SHARDS                  = 323001,   // aura on a player inside the shards, until it has left every one

    // Release (capture idx 75275-75306, 1-2 s after the last Shard of Halkias despawned)
    SPELL_HALKIAS_DAMAGE_IMMUNITY       = 324871,   // creature_template_addon aura, -100% damage taken until the release
    SPELL_CONTAIN_GOLIATH               = 325158,   // the arena Collectors' hold on Halkias
    SPELL_COSMETIC_FEIGN_DEATH          = 325988    // the arena Collectors collapse at the release
};

enum HalkiasMisc
{
    SOUND_HALKIAS_RELEASE               = 167627
};

static constexpr float ArenaCollectorRange = 60.0f;   // the arena Collectors stand 22-25 yd from Halkias; the Shards' Collectors 155-180 yd

enum HalkiasEvents
{
    EVENT_CRUMBLING_SLAM = 1,
    EVENT_HEAVE_DEBRIS,
    EVENT_REFRACTED_SINLIGHT,
    EVENT_SINLIGHT_VISIONS
};

struct HalkiasStep
{
    uint32 EventId;
    Milliseconds Offset;
};

static constexpr Milliseconds SlamRetryInterval = 500ms;    // Crumbling Slam has melee range and the target may be out of it

// From the pull (Warcraft Logs medians, n 18; High)
static constexpr std::array<HalkiasStep, 5> OpeningLadder =
{ {
    { EVENT_CRUMBLING_SLAM,         4700ms },
    { EVENT_HEAVE_DEBRIS,           12400ms },
    { EVENT_CRUMBLING_SLAM,         18800ms },
    { EVENT_HEAVE_DEBRIS,           26100ms },
    { EVENT_REFRACTED_SINLIGHT,     33700ms }
} };

// From the start of a Sinlight cast, Mythic (Warcraft Logs medians, n 9-16; Medium)
static constexpr std::array<HalkiasStep, 5> MythicCycle =
{ {
    { EVENT_CRUMBLING_SLAM,         19500ms },
    { EVENT_HEAVE_DEBRIS,           27100ms },
    { EVENT_CRUMBLING_SLAM,         34100ms },
    { EVENT_HEAVE_DEBRIS,           40500ms },
    { EVENT_REFRACTED_SINLIGHT,     48800ms }
} };

// From the start of a Sinlight cast, Normal and Heroic (one Heroic capture; Low)
static constexpr std::array<HalkiasStep, 6> HeroicCycle =
{ {
    { EVENT_CRUMBLING_SLAM,         19000ms },
    { EVENT_HEAVE_DEBRIS,           28000ms },
    { EVENT_SINLIGHT_VISIONS,       34000ms },
    { EVENT_HEAVE_DEBRIS,           39000ms },
    { EVENT_CRUMBLING_SLAM,         45000ms },
    { EVENT_REFRACTED_SINLIGHT,     53000ms }
} };

// 165408 - Halkias
struct boss_halkias : public BossAI
{
    boss_halkias(Creature* creature) : BossAI(creature, DATA_HALKIAS) { }

    bool IsReleased() const
    {
        return instance && instance->GetData(DATA_HALKIAS_RELEASED);
    }

    // Retail order: the immunity aura goes, the arena Collectors collapse and let go, then the PC immunity flags clear.
    void Release(bool announce)
    {
        me->RemoveAurasDueToSpell(SPELL_HALKIAS_DAMAGE_IMMUNITY);
        me->RemoveAurasDueToSpell(SPELL_CONTAIN_GOLIATH);
        me->SetImmuneToPC(false);
        me->RemoveUnitFlag3(UNIT_FLAG3_UNK0);

        if (!announce)
            return;

        me->PlayDirectSound(SOUND_HALKIAS_RELEASE);

        std::list<Creature*> collectors;
        me->GetCreatureListWithEntryInGrid(collectors, NPC_DEPRAVED_COLLECTOR, ArenaCollectorRange);
        for (Creature* collector : collectors)
            if (collector->IsAlive() && !collector->IsInCombat())
                collector->CastSpell(collector, SPELL_COSMETIC_FEIGN_DEATH, true);
    }

    void JustAppeared() override
    {
        BossAI::JustAppeared();
        if (IsReleased())
            Release(false);
    }

    // Reaching home reloads the template addon auras, the immunity included
    void JustReachedHome() override
    {
        BossAI::JustReachedHome();
        if (IsReleased())
            Release(false);
    }

    void DoAction(int32 action) override
    {
        if (action == ACTION_HALKIAS_RELEASE)
            Release(true);
    }

    void JustEngagedWith(Unit* who) override
    {
        BossAI::JustEngagedWith(who);
        ScheduleSteps(OpeningLadder);
    }

    void ScheduleSteps(std::span<HalkiasStep const> steps)
    {
        for (HalkiasStep const& step : steps)
            events.ScheduleEvent(step.EventId, step.Offset);
    }

    bool IsMythicDifficulty() const
    {
        return me->GetMap()->IsMythic() || me->GetMap()->IsMythicPlus();
    }

    void ExecuteEvent(uint32 eventId) override
    {
        switch (eventId)
        {
            case EVENT_CRUMBLING_SLAM:
                if (DoCastVictim(SPELL_CRUMBLING_SLAM) != SPELL_CAST_OK)
                {
                    events.Repeat(SlamRetryInterval);
                    break;
                }
                DoCastVictim(SPELL_CRUMBLING_SLAM_MARK, true);
                break;
            case EVENT_HEAVE_DEBRIS:
                DoCastAOE(SPELL_HEAVE_DEBRIS_LAUNCHER, true);
                break;
            case EVENT_REFRACTED_SINLIGHT:
                DoCastSelf(SPELL_REFRACTED_SINLIGHT);
                if (IsMythicDifficulty())
                    ScheduleSteps(MythicCycle);
                else
                    ScheduleSteps(HeroicCycle);
                break;
            case EVENT_SINLIGHT_VISIONS:
                DoCastAOE(SPELL_SINLIGHT_VISIONS_LAUNCHER, true);
                break;
            default:
                break;
        }
    }

    // The shards last 150 s and their aura has no duration: both go with the fight
    void ClearZones()
    {
        me->RemoveAllAreaTriggers();
        if (instance)
        {
            instance->DoRemoveAurasDueToSpellOnPlayers(SPELL_GLASS_SHARDS);
            instance->DoRemoveAurasDueToSpellOnPlayers(SPELL_REFRACTED_SINLIGHT_TICK);
        }
    }

    void JustDied(Unit* killer) override
    {
        ClearZones();
        BossAI::JustDied(killer);
    }

    void EnterEvadeMode(EvadeReason why) override
    {
        ClearZones();
        BossAI::EnterEvadeMode(why);
    }
};

// 322974 (Heave Debris) and 322982 (Sinlight Visions) - instant launchers: pick one player within 100 yd and start the real cast on it
template <uint32 FollowUpSpellId>
class spell_hoa_halkias_random_launcher : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ FollowUpSpellId });
    }

    void FilterTargets(std::list<WorldObject*>& targets)
    {
        targets.remove_if([](WorldObject const* target) { return !target->IsPlayer(); });
        Trinity::Containers::RandomResize(targets, 1);
    }

    void HandleHit(SpellEffIndex /*effIndex*/)
    {
        GetCaster()->CastSpell(GetHitUnit(), FollowUpSpellId, CastSpellExtraArgs(TRIGGERED_NONE));
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_hoa_halkias_random_launcher::FilterTargets, EFFECT_0, TARGET_UNIT_SRC_AREA_ENEMY);
        OnEffectHitTarget += SpellEffectFn(spell_hoa_halkias_random_launcher::HandleHit, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

using spell_hoa_halkias_heave_debris_launcher = spell_hoa_halkias_random_launcher<SPELL_HEAVE_DEBRIS>;
using spell_hoa_halkias_sinlight_visions_launcher = spell_hoa_halkias_random_launcher<SPELL_SINLIGHT_VISIONS>;

// 337929 - Refracted Sinlight: the beam tick (324044 shadow damage every 250 ms)
class spell_hoa_halkias_refracted_sinlight_tick : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_REFRACTED_SINLIGHT_DAMAGE });
    }

    void HandlePeriodic(AuraEffect const* aurEff)
    {
        if (Unit* caster = GetCaster())
            caster->CastSpell(GetTarget(), SPELL_REFRACTED_SINLIGHT_DAMAGE, CastSpellExtraArgs(aurEff));
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_hoa_halkias_refracted_sinlight_tick::HandlePeriodic, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY);
    }
};

// A player inside carries the zone's aura until it has left every areatrigger of the same entry (the shards overlap, the two beams
// cross). The unit is already out of this areatrigger's list when OnUnitExit runs.
template <uint32 ZoneSpellId>
struct at_hoa_halkias_zone : AreaTriggerAI
{
    using AreaTriggerAI::AreaTriggerAI;

    void OnUnitEnter(Unit* unit) override
    {
        if (!unit->IsPlayer())
            return;

        if (Unit* caster = at->GetCaster())
            caster->CastSpell(unit, ZoneSpellId, CastSpellExtraArgs(TRIGGERED_FULL_MASK));
    }

    void OnUnitExit(Unit* unit, AreaTriggerExitReason /*reason*/) override
    {
        if (!unit->IsPlayer())
            return;

        for (AreaTrigger const* other : unit->GetInsideAreaTriggers())
            if (other != at && other->GetEntry() == at->GetEntry() && other->GetCasterGuid() == at->GetCasterGuid())
                return;

        unit->RemoveAurasDueToSpell(ZoneSpellId, at->GetCasterGuid());
    }
};

// Areatrigger create properties 19683 (areatrigger 23808): Crumbling Slam and Heave Debris
using at_hoa_halkias_glass_shards = at_hoa_halkias_zone<SPELL_GLASS_SHARDS>;
// Areatrigger create properties 19661 and 19662 (areatrigger 23803): the two beams of Refracted Sinlight
using at_hoa_halkias_sinlight_beam = at_hoa_halkias_zone<SPELL_REFRACTED_SINLIGHT_TICK>;

void AddSC_boss_halkias()
{
    RegisterHallsOfAtonementCreatureAI(boss_halkias);
    RegisterSpellScript(spell_hoa_halkias_heave_debris_launcher);
    RegisterSpellScript(spell_hoa_halkias_sinlight_visions_launcher);
    RegisterSpellScript(spell_hoa_halkias_refracted_sinlight_tick);
    RegisterAreaTriggerAI(at_hoa_halkias_glass_shards);
    RegisterAreaTriggerAI(at_hoa_halkias_sinlight_beam);
}
