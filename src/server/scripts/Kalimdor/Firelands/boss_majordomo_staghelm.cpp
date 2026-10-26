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

#include "Creature.h"
#include "InstanceScript.h"
#include "Map.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "SpellAuraEffects.h"
#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "SpellScript.h"
#include "firelands.h"

// Mechanics sourced from DeadlyBossMods/DBM-Cataclysm
// (DBM-Raids-Cata/Firelands/FandralStaghelm.lua) and the firelands-cata
// preservation script (form controller / clump logic), re-verified against the
// 12.1 client spell data with wowdata (see Core Support Gate notes inline).
enum MajordomoStaghelmSpells
{
    // Majordomo Staghelm
    SPELL_ZERO_ENERGY_ZERO_REGEN    = 72242,  // MOD_POWER_REGEN_PERCENT -100% energy (Creature::Regenerate honours it)
    SPELL_FORM_CONTROLLER           = 98386,  // periodic dummy (800ms) - drives the clump check
    SPELL_CONCENTRATION             = 98256,  // heroic only: periodic trigger of 98229 (the player-side alt power aura)
    SPELL_CONCENTRATION_PLAYER      = 98229,  // ENABLE_ALT_POWER + PERIODIC_DUMMY on players
    SPELL_CLUMP_CHECK               = 98399,  // target 126 TARGET_UNIT_CASTER_AREA_ENEMY_CLUMP is NYI in core - see TODO in the controller
    SPELL_CAT_FORM                  = 98374,
    SPELL_SCORPION_FORM             = 98379,
    SPELL_ADRENALINE                = 97238,
    SPELL_FURY                      = 97235,  // TODO(core): effect 0 is aura 276 (HandleNULL in SpellAuraEffects.cpp) - only the scale effect works
    SPELL_FLAME_SCYTHE              = 98474,
    SPELL_LEAPING_FLAMES_TARGETING  = 101165,
    SPELL_LEAPING_FLAMES            = 98476,
    SPELL_LEAPING_FLAMES_SUMMON     = 101222,
    SPELL_FIERY_CYCLONE             = 98443,
    SPELL_SEARING_SEEDS             = 98450,
    SPELL_BURNING_ORBS              = 98451,
    SPELL_BURNING_ORBS_SUMMON       = 98565,

    SPELL_BERSERK                   = 47008,

    // Spirit of the Flame
    SPELL_STUN_AND_HATE             = 101224,

    // Player
    SPELL_SEARING_SEED              = 98620,
    SPELL_UNCOMMON_CONCENTRATION    = 98254,
    SPELL_RARE_CONCENTRATION        = 98253,
    SPELL_EPIC_CONCENTRATION        = 98252,
    SPELL_LEGENDARY_CONCENTRATION   = 98245
};

// Texts follow the creature_text groups in the world DB for entry 52571
enum MajordomoStaghelmTexts
{
    SAY_INTRO                   = 0,
    SAY_SCORPION_FORM           = 1,
    SAY_ANNOUNCE_SCORPION_FORM  = 2,
    SAY_CAT_FORM                = 3,
    SAY_ANNOUNCE_CAT_FORM       = 4,
    SAY_SEARING_SEEDS           = 5,
    SAY_ANNOUNCE_SEARING_SEEDS  = 6,
    SAY_BURNING_ORBS            = 7,
    SAY_ANNOUNCE_BURNING_ORBS   = 8,
    SAY_SLAY                    = 9,
    SAY_DEATH                   = 10,
    SAY_BALEROC_DIED_1          = 11,
    SAY_BALEROC_DIED_2          = 12,
    SAY_BALEROC_DIED_3          = 13
};

enum MajordomoStaghelmEvents
{
    EVENT_BERSERK = 1,
    EVENT_ALLOW_COMBAT,
    EVENT_FORM_ABILITY,
    EVENT_BALEROC_DIED_1,
    EVENT_BALEROC_DIED_2,
    EVENT_BALEROC_DIED_3
};

// Actions 0/1 are used by the encounter, 2/3 match the numbering the shared instance script header used
enum MajordomoStaghelmActions
{
    ACTION_PLAYERS_CLUSTERED        = 0,
    ACTION_PLAYERS_SPLIT            = 1,
    ACTION_BALEROC_DIED             = 2,
    ACTION_DRUID_OF_THE_FLAME_DIED  = 3
};

enum MajordomoStaghelmPhases
{
    PHASE_INTRO     = 1,
    PHASE_COMBAT    = 2
};

enum MajordomoStaghelmPoints
{
    POINT_INTRO = 0
};

enum class StaghelmForm : uint8
{
    Druid       = 0,
    Cat         = 1,
    Scorpion    = 2
};

Position const MajordomoStaghelmMovePosition = { 523.4965f, -61.987846f, 83.94701f };

constexpr float StaghelmClumpSearchRange = 200.0f;  // SpellRadius 22 used by the raid-wide selectors
constexpr float StaghelmClumpFallbackRadius = 8.0f; // SpellRadius 14, effect radius of 98399

// 52571 - Majordomo Staghelm <Archdruid of the Flame>
struct boss_majordomo_staghelm : public BossAI
{
    boss_majordomo_staghelm(Creature* creature) : BossAI(creature, DATA_MAJORDOMO_STAGHELM) { }

    void JustAppeared() override
    {
        // Grid unloading would despawn him while out of cell range for too long (his intro/respawn spot differs from the DB spawn)
        me->setActive(true);

        me->SetPowerType(POWER_ENERGY);
        me->SetPower(POWER_ENERGY, 0);
        DoCastSelf(SPELL_ZERO_ENERGY_ZERO_REGEN);

        // After a wipe the boss is respawned fresh from the template (immune); the intro is not replayed
        if (instance->GetBossState(DATA_MAJORDOMO_STAGHELM) == FAIL)
            me->RemoveUnitFlag(UNIT_FLAG_IMMUNE_TO_PC | UNIT_FLAG_IMMUNE_TO_NPC);

        // The shared instance script no longer forwards Baleroc's death to us, so watch his encounter state.
        // Only a live NOT_STARTED/IN_PROGRESS -> DONE transition plays the RP (a reload with Baleroc already dead does not).
        scheduler.Schedule(1s, [this](TaskContext task)
        {
            if (instance->GetBossState(DATA_BALEROC) == DONE)
            {
                if (_watchedBalerocAlive)
                    DoAction(ACTION_BALEROC_DIED);
                return;
            }

            _watchedBalerocAlive = true;
            task.Repeat(1s);
        });
    }

    void Reset() override
    {
        _Reset();
        _firstTransformation = true;
        _splitPlayersTicks = 0;
        _clusteredPlayersTicks = 0;
        _currentForm = StaghelmForm::Druid;
        _formSwitchCount = 0;
        _killedDruidsOfTheFlameCount = 0;
    }

    void JustDied(Unit* /*killer*/) override
    {
        _JustDied();
        me->RemoveAllDynObjects();
        instance->SendEncounterUnit(ENCOUNTER_FRAME_DISENGAGE, me);
        instance->DoRemoveAurasDueToSpellOnPlayers(SPELL_SEARING_SEEDS);
        Talk(SAY_DEATH);
    }

    void EnterEvadeMode(EvadeReason /*why*/) override
    {
        instance->SendEncounterUnit(ENCOUNTER_FRAME_DISENGAGE, me);
        instance->SetBossState(DATA_MAJORDOMO_STAGHELM, FAIL);
        instance->DoRemoveAurasDueToSpellOnPlayers(SPELL_SEARING_SEEDS);

        summons.DespawnAll();
        _EnterEvadeMode();
        _DespawnAtEvade();
    }

    void JustEngagedWith(Unit* who) override
    {
        BossAI::JustEngagedWith(who);

        DoCastSelf(SPELL_ZERO_ENERGY_ZERO_REGEN);
        DoCastSelf(SPELL_FORM_CONTROLLER);
        if (IsHeroic())
            DoCastSelf(SPELL_CONCENTRATION);

        instance->SendEncounterUnit(ENCOUNTER_FRAME_ENGAGE, me, 1);

        events.SetPhase(PHASE_COMBAT);
        events.ScheduleEvent(EVENT_BERSERK, 10min);
    }

    void KilledUnit(Unit* victim) override
    {
        if (victim->IsPlayer())
            Talk(SAY_SLAY, victim);
    }

    void MovementInform(uint32 motionType, uint32 pointId) override
    {
        if (motionType != POINT_MOTION_TYPE)
            return;

        if (pointId == POINT_INTRO)
            events.ScheduleEvent(EVENT_ALLOW_COMBAT, 1s);
    }

    void JustSummoned(Creature* summon) override
    {
        BossAI::JustSummoned(summon);

        switch (summon->GetEntry())
        {
            case NPC_SPIRIT_OF_THE_FLAME:
                summon->CastSpell(summon, SPELL_STUN_AND_HATE);
                summon->SetCorpseDelay(0);
                summon->m_Events.AddEventAtOffset([summon]()
                {
                    if (summon->IsAIEnabled())
                        summon->AI()->DoZoneInCombat();
                }, 1s);
                break;
            case NPC_BURNING_ORB:
                summon->SetCorpseDelay(0);
                break;
            default:
                break;
        }
    }

    void DoAction(int32 action) override
    {
        switch (action)
        {
            case ACTION_PLAYERS_CLUSTERED:
                ++_clusteredPlayersTicks;
                _splitPlayersTicks = 0;
                break;
            case ACTION_PLAYERS_SPLIT:
                ++_splitPlayersTicks;
                _clusteredPlayersTicks = 0;
                break;
            case ACTION_BALEROC_DIED:
                me->setActive(true);
                events.SetPhase(PHASE_INTRO);
                events.ScheduleEvent(EVENT_BALEROC_DIED_1, 10s);
                break;
            case ACTION_DRUID_OF_THE_FLAME_DIED:
                ++_killedDruidsOfTheFlameCount;
                if (_killedDruidsOfTheFlameCount == (Is25ManRaid() ? 6 : 3))
                {
                    events.SetPhase(PHASE_INTRO);
                    Talk(SAY_INTRO);
                    me->GetMotionMaster()->MovePoint(POINT_INTRO, MajordomoStaghelmMovePosition, true, 5.f);
                }
                break;
            default:
                break;
        }

        if (action != ACTION_PLAYERS_CLUSTERED && action != ACTION_PLAYERS_SPLIT)
            return;

        if (me->HasUnitState(UNIT_STATE_CASTING))
            return;

        if ((_clusteredPlayersTicks >= 3 && _currentForm != StaghelmForm::Scorpion)
            || (_splitPlayersTicks >= 3 && _currentForm != StaghelmForm::Cat))
        {
            // Two animal forms in a row, the next switch is the Night Elf (Fiery Cyclone) transition
            if (_formSwitchCount >= 2)
            {
                me->RemoveAurasDueToSpell(SPELL_SCORPION_FORM);
                me->RemoveAurasDueToSpell(SPELL_CAT_FORM);
                me->RemoveAurasDueToSpell(SPELL_ADRENALINE);
                me->SetPower(POWER_ENERGY, 0);
                DoCastAOE(SPELL_FIERY_CYCLONE);

                // Leaving Cat form -> Searing Seeds, leaving Scorpion form -> Burning Orbs
                if (_currentForm == StaghelmForm::Cat)
                {
                    DoCastAOE(SPELL_SEARING_SEEDS);
                    Talk(SAY_SEARING_SEEDS);
                    Talk(SAY_ANNOUNCE_SEARING_SEEDS);
                }
                else
                {
                    DoCastAOE(SPELL_BURNING_ORBS);
                    Talk(SAY_BURNING_ORBS);
                    Talk(SAY_ANNOUNCE_BURNING_ORBS);
                }

                _currentForm = StaghelmForm::Druid;
                _formSwitchCount = 0;
                _clusteredPlayersTicks = 0;
                _splitPlayersTicks = 0;
                return;
            }

            bool useScorpionForm = _clusteredPlayersTicks > _splitPlayersTicks;
            Talk(useScorpionForm ? SAY_SCORPION_FORM : SAY_CAT_FORM);
            Talk(useScorpionForm ? SAY_ANNOUNCE_SCORPION_FORM : SAY_ANNOUNCE_CAT_FORM);

            me->RemoveAurasDueToSpell(SPELL_SCORPION_FORM);
            me->RemoveAurasDueToSpell(SPELL_CAT_FORM);
            DoCastSelf(useScorpionForm ? SPELL_SCORPION_FORM : SPELL_CAT_FORM);

            // Fury is applied on every switch but the first one
            if (!_firstTransformation)
                DoCastSelf(SPELL_FURY);
            else
                _firstTransformation = false;

            me->RemoveAurasDueToSpell(SPELL_ADRENALINE);
            me->SetPower(POWER_ENERGY, 0);
            events.RescheduleEvent(EVENT_FORM_ABILITY, 400ms, 0, PHASE_COMBAT);
            _currentForm = useScorpionForm ? StaghelmForm::Scorpion : StaghelmForm::Cat;
            ++_formSwitchCount;
        }
    }

    void UpdateAI(uint32 diff) override
    {
        scheduler.Update(diff);

        if (!UpdateVictim() && !events.IsInPhase(PHASE_INTRO))
            return;

        events.Update(diff);

        if (me->HasUnitState(UNIT_STATE_CASTING))
            return;

        while (uint32 eventId = events.ExecuteEvent())
        {
            switch (eventId)
            {
                case EVENT_BERSERK:
                    DoCastSelf(SPELL_BERSERK);
                    break;
                case EVENT_ALLOW_COMBAT:
                    DoCastSelf(SPELL_ZERO_ENERGY_ZERO_REGEN);
                    me->RemoveUnitFlag(UNIT_FLAG_IMMUNE_TO_PC | UNIT_FLAG_IMMUNE_TO_NPC);
                    break;
                case EVENT_FORM_ABILITY:
                    if (me->GetPower(POWER_ENERGY) >= me->GetMaxPower(POWER_ENERGY))
                    {
                        // The client spell data carries no power cost for these, so spend the energy here
                        SpellCastResult result = SPELL_FAILED_DONT_REPORT;
                        if (_currentForm == StaghelmForm::Scorpion)
                            result = DoCastVictim(SPELL_FLAME_SCYTHE);
                        else if (_currentForm == StaghelmForm::Cat)
                            result = DoCastAOE(SPELL_LEAPING_FLAMES_TARGETING, CastSpellExtraArgs().AddSpellMod(SPELLVALUE_MAX_TARGETS, 1));

                        if (result == SPELL_CAST_OK)
                            me->SetPower(POWER_ENERGY, 0);
                    }
                    events.Repeat(400ms);
                    break;
                case EVENT_BALEROC_DIED_1:
                    Talk(SAY_BALEROC_DIED_1);
                    events.ScheduleEvent(EVENT_BALEROC_DIED_2, 11s);
                    break;
                case EVENT_BALEROC_DIED_2:
                    Talk(SAY_BALEROC_DIED_2);
                    events.ScheduleEvent(EVENT_BALEROC_DIED_3, 11s);
                    break;
                case EVENT_BALEROC_DIED_3:
                    Talk(SAY_BALEROC_DIED_3);
                    if (!me->IsEngaged())
                        me->setActive(false);
                    break;
                default:
                    break;
            }

            if (me->HasUnitState(UNIT_STATE_CASTING))
                return;
        }
    }

private:
    bool _firstTransformation = true;
    bool _watchedBalerocAlive = false;
    uint32 _splitPlayersTicks = 0;
    uint32 _clusteredPlayersTicks = 0;
    StaghelmForm _currentForm = StaghelmForm::Druid;
    uint8 _formSwitchCount = 0;
    uint8 _killedDruidsOfTheFlameCount = 0;
};

// 53619 - Druid of the Flame (reports its death so the boss can count them for the intro)
struct npc_majordomo_staghelm_druid_of_the_flame : public ScriptedAI
{
    npc_majordomo_staghelm_druid_of_the_flame(Creature* creature) : ScriptedAI(creature) { }

    void JustDied(Unit* /*killer*/) override
    {
        if (Creature* staghelm = me->FindNearestCreature(NPC_MAJORDOMO_STAGHELM, 250.0f, true))
            if (staghelm->IsAIEnabled())
                staghelm->AI()->DoAction(ACTION_DRUID_OF_THE_FLAME_DIED);
    }
};

// 98386 - Form Controller
class spell_majordomo_staghelm_form_controller : public AuraScript
{
    void HandlePeriodicDummy(AuraEffect const* /*aurEff*/)
    {
        // TODO(core): SPELL_CLUMP_CHECK (98399) selects targets with TARGET_UNIT_CASTER_AREA_ENEMY_CLUMP (126), which
        // SpellInfo.cpp flags TARGET_SELECT_CATEGORY_NYI, so casting it would select nothing. Until that target type is
        // implemented the same check (players within the 8yd effect radius of each player, >= 7 on 10-man / 18 on
        // 25-man) is evaluated here on the controller tick and reported through the same boss DoAction.
        Creature* boss = GetTarget()->ToCreature();
        if (!boss || !boss->IsAIEnabled())
            return;

        uint32 threshold = boss->GetMap()->Is25ManRaid() ? 18 : 7;
        float radius = StaghelmClumpFallbackRadius;
        if (SpellInfo const* clumpCheck = sSpellMgr->GetSpellInfo(SPELL_CLUMP_CHECK, boss->GetMap()->GetDifficultyID()))
            if (float effectRadius = clumpCheck->GetEffect(EFFECT_0).CalcRadius(boss).Max; effectRadius > 0.0f)
                radius = effectRadius;

        std::vector<Player*> players;
        boss->GetPlayerListInGrid(players, StaghelmClumpSearchRange);
        std::erase_if(players, [](Player const* player) { return player->IsGameMaster(); });

        bool hasClumpedPlayers = false;
        for (Player const* player : players)
        {
            uint32 nearby = 0;
            for (Player const* other : players)
                if (player->IsWithinDist(other, radius))
                    ++nearby;

            if (nearby >= threshold)
            {
                hasClumpedPlayers = true;
                break;
            }
        }

        boss->AI()->DoAction(hasClumpedPlayers ? ACTION_PLAYERS_CLUSTERED : ACTION_PLAYERS_SPLIT);
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_majordomo_staghelm_form_controller::HandlePeriodicDummy, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY);
    }
};

// 101165 - Leaping Flames (targeting)
class spell_majordomo_staghelm_leaping_flames_targeting : public SpellScript
{
    bool Validate(SpellInfo const* spellInfo) override
    {
        return ValidateSpellInfo({ SPELL_LEAPING_FLAMES_SUMMON, SPELL_LEAPING_FLAMES }) && ValidateSpellEffect({ { spellInfo->Id, EFFECT_0 } });
    }

    static bool IsRangedSpec(Player const* player)
    {
        // Leaping Flames prioritises ranged classes and specs
        switch (player->GetClass())
        {
            case CLASS_HUNTER:
            case CLASS_MAGE:
            case CLASS_WARLOCK:
            case CLASS_PRIEST:
            case CLASS_EVOKER:
                return true;
            case CLASS_SHAMAN:
                return player->GetPrimarySpecialization() != ChrSpecialization::ShamanEnhancement;
            case CLASS_PALADIN:
                return player->GetPrimarySpecialization() == ChrSpecialization::PaladinHoly;
            case CLASS_DRUID:
                return player->GetPrimarySpecialization() != ChrSpecialization::DruidFeral
                    && player->GetPrimarySpecialization() != ChrSpecialization::DruidGuardian;
            case CLASS_MONK:
                return player->GetPrimarySpecialization() == ChrSpecialization::MonkMistweaver;
            default:
                return false;
        }
    }

    void FilterTargets(std::list<WorldObject*>& targets)
    {
        if (targets.empty())
            return;

        std::list<WorldObject*> targetsCopy = targets;
        targets.remove_if([&](WorldObject const* target)
        {
            if (target == GetCaster()->GetVictim())
                return true;

            Player const* player = target->ToPlayer();
            return !player || !IsRangedSpec(player);
        });

        // No ranged candidates: fall back to any non-tank player
        if (targets.empty())
        {
            targets = std::move(targetsCopy);
            targets.remove_if([&](WorldObject const* target)
            {
                return target == GetCaster()->GetVictim() || !target->IsPlayer();
            });
        }

        // Only a single target is ever picked
        if (targets.size() > 1)
            Trinity::Containers::RandomResize(targets, 1);
    }

    void HandleDummyEffect(SpellEffIndex effIndex)
    {
        Unit* caster = GetCaster();
        Unit* target = GetHitUnit();
        if (!caster || !target)
            return;

        caster->CastSpell(nullptr, SPELL_LEAPING_FLAMES_SUMMON, TRIGGERED_FULL_MASK);
        caster->CastSpell(target, GetSpellInfo()->GetEffect(effIndex).TriggerSpell, TRIGGERED_FULL_MASK);
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_majordomo_staghelm_leaping_flames_targeting::FilterTargets, EFFECT_0, TARGET_UNIT_SRC_AREA_ENEMY);
        OnEffectHitTarget += SpellEffectFn(spell_majordomo_staghelm_leaping_flames_targeting::HandleDummyEffect, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 98476 - Leaping Flames
class spell_majordomo_staghelm_leaping_flames : public SpellScript
{
    bool Validate(SpellInfo const* spellInfo) override
    {
        return ValidateSpellEffect({ { spellInfo->Id, EFFECT_0 } })
            && ValidateSpellInfo({ uint32(spellInfo->GetEffect(EFFECT_0).TriggerSpell) });
    }

    void HandleJump(SpellEffIndex effIndex)
    {
        Unit* caster = GetCaster();
        if (!caster->IsAlive())
            return;

        // JUMP_DEST is a launch effect: drop the ground fire (TriggerSpell 98535) where the jump ends
        Position const& pos = GetHitDest() ? static_cast<Position const&>(*GetHitDest()) : caster->GetPosition();
        caster->CastSpell(pos, GetSpellInfo()->GetEffect(effIndex).TriggerSpell, CastSpellExtraArgsInit{
            .TriggerFlags = TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_DONT_REPORT_CAST_ERROR
        });
    }

    void Register() override
    {
        OnEffectLaunch += SpellEffectFn(spell_majordomo_staghelm_leaping_flames::HandleJump, EFFECT_0, SPELL_EFFECT_JUMP_DEST);
    }
};

// 98450 - Searing Seeds
class spell_majordomo_staghelm_searing_seeds : public SpellScript
{
    void CalculateDuration(std::list<WorldObject*>& targets)
    {
        // 10 seconds + 5 seconds per target on 10-man, + 2 seconds per target on 25-man
        uint32 baseValue = 10 * IN_MILLISECONDS;
        uint32 offsetValue = (GetCaster()->GetMap()->Is25ManRaid() ? 2 : 5) * IN_MILLISECONDS;
        uint8 targetCount = 0;

        _durationForTarget.clear();
        for (WorldObject* target : targets)
        {
            ++targetCount;
            _durationForTarget[target->GetGUID()] = baseValue + offsetValue * targetCount;
        }
    }

    void HandleAuraDuration(SpellEffIndex /*effIndex*/)
    {
        auto itr = _durationForTarget.find(GetHitUnit()->GetGUID());
        if (itr == _durationForTarget.end())
            return;

        if (Aura* aura = GetHitAura())
        {
            aura->SetMaxDuration(itr->second);
            aura->SetDuration(itr->second);
        }
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_majordomo_staghelm_searing_seeds::CalculateDuration, EFFECT_0, TARGET_UNIT_SRC_AREA_ENEMY);
        OnEffectHitTarget += SpellEffectFn(spell_majordomo_staghelm_searing_seeds::HandleAuraDuration, EFFECT_0, SPELL_EFFECT_APPLY_AURA);
    }

private:
    std::unordered_map<ObjectGuid, uint32> _durationForTarget;
};

class spell_majordomo_staghelm_searing_seeds_aura : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_SEARING_SEED });
    }

    void HandleRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        if (GetTargetApplication()->GetRemoveMode() == AURA_REMOVE_BY_EXPIRE)
            GetTarget()->CastSpell(GetTarget(), SPELL_SEARING_SEED, TRIGGERED_FULL_MASK);
    }

    void Register() override
    {
        AfterEffectRemove += AuraEffectRemoveFn(spell_majordomo_staghelm_searing_seeds_aura::HandleRemove, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }
};

// 98229 - Concentration (heroic, player side of the boss aura 98256)
class spell_majordomo_staghelm_concentration : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo(
        {
            SPELL_UNCOMMON_CONCENTRATION,
            SPELL_RARE_CONCENTRATION,
            SPELL_EPIC_CONCENTRATION,
            SPELL_LEGENDARY_CONCENTRATION
        });
    }

    void HandleRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        RemoveConcentration(GetTarget());
    }

    void HandlePeriodic(AuraEffect const* /*aurEff*/)
    {
        Unit* target = GetTarget();
        uint32 power = target->GetPower(POWER_ALTERNATE_POWER);
        if (_oldPower == power)
            return;

        uint32 oldConcentration = _currentConcentrationSpellId;
        uint32 newConcentration = 0;
        if (power >= 100)
            newConcentration = SPELL_LEGENDARY_CONCENTRATION;
        else if (power >= 75)
            newConcentration = SPELL_EPIC_CONCENTRATION;
        else if (power >= 50)
            newConcentration = SPELL_RARE_CONCENTRATION;
        else if (power >= 25)
            newConcentration = SPELL_UNCOMMON_CONCENTRATION;

        if (oldConcentration != newConcentration)
        {
            if (oldConcentration)
                target->RemoveAurasDueToSpell(oldConcentration);
            if (newConcentration)
                target->CastSpell(target, newConcentration, TRIGGERED_FULL_MASK);
            _currentConcentrationSpellId = newConcentration;
        }

        _oldPower = power;
    }

    // TODO(core): whether 98229 has proc data (SpellProcEntry) that reaches this handler is unverified
    void HandleProc(AuraEffect const* /*aurEff*/, ProcEventInfo& /*eventInfo*/)
    {
        PreventDefaultAction();
        RemoveConcentration(GetTarget());
        GetTarget()->SetPower(POWER_ALTERNATE_POWER, 0);
    }

    void Register() override
    {
        AfterEffectRemove += AuraEffectRemoveFn(spell_majordomo_staghelm_concentration::HandleRemove, EFFECT_0, SPELL_AURA_ENABLE_ALT_POWER, AURA_EFFECT_HANDLE_REAL);
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_majordomo_staghelm_concentration::HandlePeriodic, EFFECT_1, SPELL_AURA_PERIODIC_DUMMY);
        OnEffectProc += AuraEffectProcFn(spell_majordomo_staghelm_concentration::HandleProc, EFFECT_0, SPELL_AURA_ENABLE_ALT_POWER);
    }

private:
    void RemoveConcentration(Unit* target)
    {
        target->RemoveAurasDueToSpell(SPELL_UNCOMMON_CONCENTRATION);
        target->RemoveAurasDueToSpell(SPELL_RARE_CONCENTRATION);
        target->RemoveAurasDueToSpell(SPELL_EPIC_CONCENTRATION);
        target->RemoveAurasDueToSpell(SPELL_LEGENDARY_CONCENTRATION);
        _currentConcentrationSpellId = 0;
        _oldPower = 0;
    }

    uint32 _oldPower = 0;
    uint32 _currentConcentrationSpellId = 0;
};

// 98451 - Burning Orbs (summons five orbs via 98565)
class spell_majordomo_staghelm_burning_orbs : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_BURNING_ORBS_SUMMON });
    }

    void HandleDummyEffect(SpellEffIndex /*effIndex*/)
    {
        Unit* caster = GetCaster();
        for (uint8 i = 0; i < 5; ++i)
            caster->CastSpell(nullptr, SPELL_BURNING_ORBS_SUMMON, TRIGGERED_FULL_MASK);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_majordomo_staghelm_burning_orbs::HandleDummyEffect, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

void AddSC_boss_majordomo_staghelm()
{
    RegisterFirelandsAI(boss_majordomo_staghelm);
    RegisterCreatureAI(npc_majordomo_staghelm_druid_of_the_flame);

    RegisterSpellScript(spell_majordomo_staghelm_form_controller);
    RegisterSpellScript(spell_majordomo_staghelm_leaping_flames_targeting);
    RegisterSpellScript(spell_majordomo_staghelm_leaping_flames);
    RegisterSpellAndAuraScriptPair(spell_majordomo_staghelm_searing_seeds, spell_majordomo_staghelm_searing_seeds_aura);
    RegisterSpellScript(spell_majordomo_staghelm_concentration);
    RegisterSpellScript(spell_majordomo_staghelm_burning_orbs);
}
