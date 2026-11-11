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
 * Grand Proctor Beryllia (DungeonEncounter 2362). No phases, no health gates: a fixed 38.9 s loop of Iron Spikes, two Agonize, Rite of
 * Supremacy and Endless Torment (trinitycore-tracking/sl-dungeons/sanguine-depths/bosses/grand-proctor-beryllia.md). The offsets are
 * Warcraft Logs medians (22 Mythic+ kills, High) and match the Heroic capture within its 1 s clock step; the first loop is entered
 * part-way (Spikes 3.8 s, Rite 11.1 s, Torment 24.3 s, second Spikes 35.2 s).
 *
 * The energy bar (Power Tracker, +4 per second) is shown, not read: the casts above are on timers. The capture starts it at 60 and
 * resets it to 0 at every Rite; the timers make it reach 100 at the Rite.
 *
 * Z'rali, the naaru in the room (166642, friendly, never in combat), answers each Rite with a Shining Radiance area and is the
 * channel target of Endless Torment. Heroic and Normal use the one area; the Mythic Fragments of Radiance (328737) are not scripted.
 * Not scripted: the cosmetic Rite spokes (325390 / 325371 / 327440, 43 and 30 ground casts per Rite with no effect), the Essence of
 * Z'rali that follows the kill and the ride to General Kaal.
 */

#include "AreaTrigger.h"
#include "AreaTriggerAI.h"
#include "InstanceScript.h"
#include "ObjectAccessor.h"
#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "Spell.h"
#include "SpellAuraEffects.h"
#include "SpellAuras.h"
#include "SpellScript.h"
#include "sanguine_depths.h"
#include <array>
#include <cmath>

enum BerylliaSpells
{
    SPELL_IRON_SPIKES                   = 325254,   // 2.5 s cast, then a 2 s channel on the target: a 0.5 s periodic dummy drives the four hits
    SPELL_IRON_SPIKES_HIT_1             = 325257,   // 45.6 bp each, target = the channel target
    SPELL_IRON_SPIKES_HIT_2             = 325260,
    SPELL_IRON_SPIKES_HIT_3             = 325261,
    SPELL_IRON_SPIKES_HIT_4             = 325262,
    SPELL_AGONIZE                       = 328593,   // 6 s DoT, 2 s tick
    SPELL_RITE_OF_SUPREMACY             = 325360,   // 10 s cast, raid damage at the end
    SPELL_RITE_TELEPORT                 = 325791,   // back to the middle of the room (nearby entry) and removes the Power Tracker
    SPELL_POWER_TRACKER                 = 325852,   // periodic dummy on the boss, shows the energy bar filling
    SPELL_SHINING_RADIANCE_MISSILE      = 325752,   // Z'rali's missile, 325742 creates the area (create properties 20101, areatrigger 24045)
    SPELL_SHINING_RADIANCE_PROTECTION   = 327107,   // -75% damage taken and Agonize immunity, on the first player to touch the area
    SPELL_ENDLESS_TORMENT               = 326039,   // 3 s cast, then a 5 s channel on Z'rali: her aura ticks every 0.5 s
    SPELL_ANGUISHED_CRIES_RANDOM        = 326042,   // dummy on Z'rali, three per tick
    SPELL_ANGUISHED_CRIES_PLAYERS       = 326038,   // dummy on every player, one per tick
    SPELL_ANGUISHED_CRIES_MISSILE       = 325881,   // missile to a point, 325885 is the 12 s ground circle
    SPELL_ANGUISHED_CRIES_GROUND        = 325885
};

enum BerylliaEvents
{
    EVENT_IRON_SPIKES = 1,
    EVENT_AGONIZE,
    EVENT_RITE_OF_SUPREMACY,
    EVENT_ENDLESS_TORMENT
};

enum BerylliaTexts
{
    // Beryllia (group ids as in the live creature_text rows)
    SAY_AGGRO                           = 0,    // How dare you interrupt my research!
    SAY_RITE_OF_SUPREMACY               = 1,    // My patience is wearing thin!
    SAY_DEATH                           = 2,    // So much... left... to discover...

    // Z'rali (creature_text of 166642)
    SAY_ZRALI_LIGHT_SHINES              = 0     // The Light shines... even here!
};

enum BerylliaCreatures
{
    NPC_ZRALI                           = 166642
};

static constexpr Milliseconds IronSpikesFirst = 3800ms;
static constexpr Milliseconds IronSpikesSecond = 35200ms;           // the first loop is shorter than the steady 38.9 s
static constexpr Milliseconds IronSpikesInterval = 38900ms;
static constexpr Milliseconds AgonizeAfterSpikes = 4900ms;
static constexpr Milliseconds AgonizeSecondAfterSpikes = 10900ms;   // not after the first Spikes of the fight
static constexpr Milliseconds RiteFirst = 11100ms;
static constexpr Milliseconds RiteAfterSpikes = 14800ms;
static constexpr Milliseconds TormentFirst = 24300ms;
static constexpr Milliseconds TormentAfterSpikes = 27900ms;
static constexpr Milliseconds AggroYellDelay = 1s;
static constexpr Milliseconds RiteCastDelay = 0ms;
static constexpr Milliseconds RadianceDelay = 2s;                   // capture: the Rite yell, then Z'rali's yell and missile 2 s later
static constexpr int32 StartEnergy = 60;                            // capture: 64 after the first Power Tracker tick
static constexpr int32 EnergyPerTick = 4;
static constexpr float RadianceDistance = 22.0f;                    // Low: the three captured areas are 22.6-22.9 yd from the boss at different bearings
static constexpr float AnguishedCriesRadius = 45.0f;                // the 78 captured random circles are 4-49 yd from the middle (Medium)

static constexpr Position BerylliaArenaCenter = { -1511.67f, 6266.58f, 4072.75f };

static constexpr std::array<uint32, 4> IronSpikesHits =
{
    SPELL_IRON_SPIKES_HIT_1, SPELL_IRON_SPIKES_HIT_2, SPELL_IRON_SPIKES_HIT_3, SPELL_IRON_SPIKES_HIT_4
};

// 162102 - Grand Proctor Beryllia
struct boss_grand_proctor_beryllia : public BossAI
{
    boss_grand_proctor_beryllia(Creature* creature) : BossAI(creature, DATA_GRAND_PROCTOR_BERYLLIA) { }

    void Reset() override
    {
        BossAI::Reset();
        _spikesCast = 0;
        me->SetPower(POWER_ENERGY, 0);
        Cleanup();
    }

    void JustEngagedWith(Unit* who) override
    {
        BossAI::JustEngagedWith(who);
        me->SetPower(POWER_ENERGY, StartEnergy);
        DoCastSelf(SPELL_POWER_TRACKER, true);

        scheduler.Schedule(AggroYellDelay, [this](TaskContext)
        {
            Talk(SAY_AGGRO);
        });
    }

    void ScheduleTasks() override
    {
        events.ScheduleEvent(EVENT_IRON_SPIKES, IronSpikesFirst);
        events.ScheduleEvent(EVENT_RITE_OF_SUPREMACY, RiteFirst);
        events.ScheduleEvent(EVENT_ENDLESS_TORMENT, TormentFirst);
        events.ScheduleEvent(EVENT_IRON_SPIKES, IronSpikesSecond);
    }

    void JustDied(Unit* killer) override
    {
        Cleanup();
        BossAI::JustDied(killer);
        Talk(SAY_DEATH);
    }

    void EnterEvadeMode(EvadeReason why) override
    {
        Cleanup();
        BossAI::EnterEvadeMode(why);
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
            case EVENT_IRON_SPIKES:
                if (DoCastVictim(SPELL_IRON_SPIKES) != SPELL_CAST_OK)
                {
                    events.Repeat(500ms);
                    break;
                }
                ScheduleAfterSpikes();
                break;
            case EVENT_AGONIZE:
                DoCastVictim(SPELL_AGONIZE);
                break;
            case EVENT_RITE_OF_SUPREMACY:
                StartRite();
                break;
            case EVENT_ENDLESS_TORMENT:
                StartTorment();
                break;
            default:
                break;
        }
    }

private:
    // The first Spikes only brings an Agonize: its Rite, Torment and the second Spikes were scheduled at the pull. From the second
    // Spikes on every cast of the loop hangs off it.
    void ScheduleAfterSpikes()
    {
        ++_spikesCast;
        events.ScheduleEvent(EVENT_AGONIZE, AgonizeAfterSpikes);
        if (_spikesCast == 1)
            return;

        events.ScheduleEvent(EVENT_AGONIZE, AgonizeSecondAfterSpikes);
        events.ScheduleEvent(EVENT_RITE_OF_SUPREMACY, RiteAfterSpikes);
        events.ScheduleEvent(EVENT_ENDLESS_TORMENT, TormentAfterSpikes);
        events.ScheduleEvent(EVENT_IRON_SPIKES, IronSpikesInterval);
    }

    void StartRite()
    {
        DoCastSelf(SPELL_RITE_TELEPORT, true);
        me->RemoveAurasDueToSpell(SPELL_POWER_TRACKER);
        me->SetPower(POWER_ENERGY, 0);
        Talk(SAY_RITE_OF_SUPREMACY);

        scheduler.Schedule(RiteCastDelay, [this](TaskContext)
        {
            DoCastSelf(SPELL_RITE_OF_SUPREMACY);
        });
        scheduler.Schedule(RadianceDelay, [this](TaskContext)
        {
            ThrowShiningRadiance();
        });
    }

    void StartTorment()
    {
        if (Creature* zrali = GetZrali())
            me->SetFacingToObject(zrali);

        me->SetPower(POWER_ENERGY, 0);
        DoCastSelf(SPELL_POWER_TRACKER, true);
        DoCastSelf(SPELL_ENDLESS_TORMENT);
    }

    void ThrowShiningRadiance()
    {
        Creature* zrali = GetZrali();
        if (!zrali)
            return;

        if (CreatureAI* ai = zrali->AI())
            ai->Talk(SAY_ZRALI_LIGHT_SHINES);

        float angle = frand(0.0f, 2.0f * float(M_PI));
        float x = BerylliaArenaCenter.GetPositionX() + RadianceDistance * std::cos(angle);
        float y = BerylliaArenaCenter.GetPositionY() + RadianceDistance * std::sin(angle);
        float z = BerylliaArenaCenter.GetPositionZ();
        zrali->UpdateGroundPositionZ(x, y, z);
        zrali->CastSpell(Position(x, y, z), SPELL_SHINING_RADIANCE_MISSILE, CastSpellExtraArgs(TRIGGERED_FULL_MASK));
    }

    Creature* GetZrali() const
    {
        return me->FindNearestCreature(NPC_ZRALI, 200.0f);
    }

    // The areas last 10 s and the player aura only goes with the area; Torment's aura on Z'rali goes with the channel
    void Cleanup()
    {
        me->RemoveAurasDueToSpell(SPELL_POWER_TRACKER);

        if (Creature* zrali = GetZrali())
        {
            zrali->RemoveAurasDueToSpell(SPELL_ENDLESS_TORMENT);
            zrali->RemoveAllAreaTriggers();
        }

        if (instance)
        {
            instance->DoRemoveAurasDueToSpellOnPlayers(SPELL_SHINING_RADIANCE_PROTECTION);
            instance->DoRemoveAurasDueToSpellOnPlayers(SPELL_AGONIZE);
            instance->DoRemoveAurasDueToSpellOnPlayers(SPELL_ANGUISHED_CRIES_GROUND);
            instance->DoRemoveAurasDueToSpellOnPlayers(SPELL_IRON_SPIKES);
        }
    }

    uint32 _spikesCast = 0;
};

// 325254 - Iron Spikes: the 0.5 s periodic dummy on the target releases the four hits, which resolve through the channel target
class spell_sd_beryllia_iron_spikes : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_IRON_SPIKES_HIT_1, SPELL_IRON_SPIKES_HIT_2, SPELL_IRON_SPIKES_HIT_3, SPELL_IRON_SPIKES_HIT_4 });
    }

    // The channel ends in the same update as the fourth tick and the hits need a channel to find their target, so the last
    // two hits go out together on the third tick (capture: the pairs land 1 s and 2 s after the cast, the last at 2.0 s)
    void HandlePeriodic(AuraEffect const* aurEff)
    {
        Unit* caster = GetCaster();
        uint32 tick = aurEff->GetTickNumber();
        if (!caster || tick > IronSpikesHits.size() - 1)
            return;

        caster->CastSpell(GetTarget(), IronSpikesHits[tick - 1], CastSpellExtraArgs(aurEff));
        if (tick == IronSpikesHits.size() - 1)
            caster->CastSpell(GetTarget(), IronSpikesHits[tick], CastSpellExtraArgs(aurEff));
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_sd_beryllia_iron_spikes::HandlePeriodic, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY);
    }
};

// 325852 - Power Tracker: +4 energy every second
class spell_sd_beryllia_power_tracker : public AuraScript
{
    void HandlePeriodic(AuraEffect const* /*aurEff*/)
    {
        GetTarget()->ModifyPower(POWER_ENERGY, EnergyPerTick);
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_sd_beryllia_power_tracker::HandlePeriodic, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY);
    }
};

// 325360 - Rite of Supremacy: every player the raid damage hits gets Agonize, unless a Shining Radiance protects it (capture: no
// Agonize at the completion that found the player inside the area)
class spell_sd_beryllia_rite_of_supremacy : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_AGONIZE, SPELL_SHINING_RADIANCE_PROTECTION });
    }

    void HandleHit(SpellEffIndex /*effIndex*/)
    {
        Unit* target = GetHitUnit();
        if (!target || target->HasAura(SPELL_SHINING_RADIANCE_PROTECTION))
            return;

        GetCaster()->CastSpell(target, SPELL_AGONIZE, CastSpellExtraArgs(GetSpell()));
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_sd_beryllia_rite_of_supremacy::HandleHit, EFFECT_0, SPELL_EFFECT_SCHOOL_DAMAGE);
    }
};

// 325791 - Rite of Supremacy: the teleport finds an entry in the room (the Anomalous Rift 175529, which stands 3.7 yd above the
// floor); the capture puts the boss on the ground at her spawn point
class spell_sd_beryllia_rite_teleport : public SpellScript
{
    void SetDest(SpellDestination& dest)
    {
        if (Creature* caster = GetCaster()->ToCreature())
            dest.Relocate(caster->GetHomePosition());
    }

    void Register() override
    {
        OnDestinationTargetSelect += SpellDestinationTargetSelectFn(spell_sd_beryllia_rite_teleport::SetDest, EFFECT_0, TARGET_DEST_NEARBY_ENTRY);
    }
};

// 326042 - Anguished Cries: Z'rali throws a missile to a random point of the room, three times per tick
class spell_sd_beryllia_anguished_cries_random : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_ANGUISHED_CRIES_MISSILE });
    }

    void HandleHit(SpellEffIndex /*effIndex*/)
    {
        Unit* caster = GetCaster();
        Position dest = caster->GetRandomPoint(BerylliaArenaCenter, AnguishedCriesRadius);
        caster->CastSpell(dest, SPELL_ANGUISHED_CRIES_MISSILE, CastSpellExtraArgs(GetSpell()));
    }

    void Register() override
    {
        OnEffectHit += SpellEffectFn(spell_sd_beryllia_anguished_cries_random::HandleHit, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 326038 - Anguished Cries: a missile to the position of every player, once per tick
class spell_sd_beryllia_anguished_cries_players : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_ANGUISHED_CRIES_MISSILE });
    }

    void HandleHit(SpellEffIndex /*effIndex*/)
    {
        if (Unit* target = GetHitUnit())
            GetCaster()->CastSpell(target->GetPosition(), SPELL_ANGUISHED_CRIES_MISSILE, CastSpellExtraArgs(GetSpell()));
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_sd_beryllia_anguished_cries_players::HandleHit, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// Areatrigger create properties 20101 (areatrigger 24045): the first player to touch the area is protected for as long as it stays
// inside (capture: the aura appeared, went on leaving and came back on entering again). One player at a time.
struct at_sd_beryllia_shining_radiance : AreaTriggerAI
{
    using AreaTriggerAI::AreaTriggerAI;

    void OnUnitEnter(Unit* unit) override
    {
        if (!unit->IsPlayer() || HasProtectedPlayer())
            return;

        Unit* caster = at->GetCaster();
        if (!caster)
            return;

        caster->CastSpell(unit, SPELL_SHINING_RADIANCE_PROTECTION, CastSpellExtraArgs(TRIGGERED_FULL_MASK));
        _protected = unit->GetGUID();
    }

    void OnUnitExit(Unit* unit, AreaTriggerExitReason /*reason*/) override
    {
        if (unit->GetGUID() == _protected)
            Release(unit);
    }

    void OnRemove() override
    {
        if (Unit* player = ObjectAccessor::GetUnit(*at, _protected))
            Release(player);
    }

private:
    bool HasProtectedPlayer() const
    {
        Unit const* player = ObjectAccessor::GetUnit(*at, _protected);
        return player && player->HasAura(SPELL_SHINING_RADIANCE_PROTECTION, at->GetCasterGuid());
    }

    void Release(Unit* player)
    {
        player->RemoveAurasDueToSpell(SPELL_SHINING_RADIANCE_PROTECTION, at->GetCasterGuid());
        _protected.Clear();
    }

    ObjectGuid _protected;
};

void AddSC_boss_grand_proctor_beryllia()
{
    RegisterSanguineDepthsCreatureAI(boss_grand_proctor_beryllia);
    RegisterSpellScript(spell_sd_beryllia_iron_spikes);
    RegisterSpellScript(spell_sd_beryllia_power_tracker);
    RegisterSpellScript(spell_sd_beryllia_rite_of_supremacy);
    RegisterSpellScript(spell_sd_beryllia_rite_teleport);
    RegisterSpellScript(spell_sd_beryllia_anguished_cries_random);
    RegisterSpellScript(spell_sd_beryllia_anguished_cries_players);
    RegisterAreaTriggerAI(at_sd_beryllia_shining_radiance);
}
