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

#include "Containers.h"
#include "Creature.h"
#include "GameEventSender.h"
#include "GameObject.h"
#include "InstanceScenario.h"
#include "InstanceScript.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "SpellScript.h"
#include "TaskScheduler.h"
#include "TemporarySummon.h"

// Hallowfall: Priory scenario (scenario 2416, map 2716), finale of quest 78948 "A Light in the Dark".
// Retail capture 2026-11-09 13-43-38 (idx in comments). The step criteria are all "anyone triggers game event" (criteria type 92);
// the server raises those events itself, so this script does it from the same moments the capture shows.
namespace Scripts::KhazAlgar::PrioryScenario
{
    static constexpr uint32 MAP_PRIORY_SCENARIO = 2716;

    namespace Steps
    {
        static constexpr uint32 DeeperIntoTheDark   = 7009;
        static constexpr uint32 BeaconForTheLost    = 7010;
        static constexpr uint32 LongestShadow       = 7012;
        static constexpr uint32 HarbingersEnd       = 7013;
    }

    namespace GameEventIds
    {
        static constexpr uint32 NorthBrazierUsed    = 93025;    // goober Data2 of Corrupted Flame 446305
        static constexpr uint32 SouthBrazierUsed    = 93024;    // goober Data2 of Corrupted Flame 449549
        static constexpr uint32 NorthBrazierLit     = 91213;    // criteria 67322
        static constexpr uint32 SouthBrazierLit     = 89888;    // criteria 66409
        static constexpr uint32 CrusherSlain        = 89928;    // criteria 66412
        static constexpr uint32 BrokenQueenSlain    = 89930;    // criteria 66415
        static constexpr uint32 EnterThePriory      = 89931;    // criteria 66416
    }

    namespace Creatures
    {
        static constexpr uint32 WhisperingMonstrosity = 214382;
        static constexpr uint32 WrithingShade       = 214381;
        static constexpr uint32 LivingShadow        = 214379;
        static constexpr uint32 ShadeswornCrusher   = 218527;
    }

    namespace GameObjects
    {
        static constexpr uint32 HeavyShadows        = 424422;   // the two barriers
    }

    namespace SpawnGroups
    {
        static constexpr uint32 ShadeswornCrusher   = 2716001;  // manual spawn group (world SQL)
        static constexpr uint32 BrokenQueen         = 2716002;
    }

    namespace Spells
    {
        static constexpr uint32 FinaleReturn        = 441666;   // Play Movie 1019 + Teleport Units (spell_target_position, map 2601)
        static constexpr uint32 ConversationStep2   = 450810;   // Trigger Conversation 25639 (retail 874113, step 7009 start)
        static constexpr uint32 ConversationStep3   = 441654;   // Trigger Conversation 25638 (retail 877799, step 7010 start)
        static constexpr uint32 DarkBarrage         = 459246;   // the Broken Queen's periodic self aura (879610, 880361)
    }

    enum Data : uint32
    {
        DATA_BROKEN_QUEEN_DEFEATED = 1
    };

    struct WaveSpawn
    {
        uint32 Entry;
        Position Pos;
    };

    // Both waves appear in the tick the brazier is used (north 868312, south 873602), at the spots of the imported static rows.
    static WaveSpawn const NorthWave[] =
    {
        { Creatures::WhisperingMonstrosity, { 3108.70f, 1070.25f, 515.90f, 1.5218f } },
        { Creatures::WrithingShade,         { 3084.82f, 1088.75f, 515.64f, 0.3981f } }
    };
    static WaveSpawn const SouthWave[] =
    {
        { Creatures::WhisperingMonstrosity, { 2888.45f, 1119.79f, 515.57f, 3.0399f } },
        { Creatures::LivingShadow,          { 2864.84f, 1092.19f, 515.64f, 1.4455f } }
    };

    // Retail credited "Enter the Priory" at 881196 with the player at (2975.2, 1437.8, 562.2), just past barrier 2 (2976.9, 1433.7).
    // No area trigger or other packet explains it, so the box is inferred from those positions.
    static bool IsInPrioryEntrance(Player const* player)
    {
        return player->GetPositionX() >= 2968.0f && player->GetPositionX() <= 2984.0f
            && player->GetPositionY() >= 1436.0f && player->GetPositionY() <= 1446.0f
            && std::abs(player->GetPositionZ() - 562.2f) <= 6.0f;
    }

    static constexpr char const* InstanceScriptName = "instance_priory_scenario";

    class instance_priory_scenario : public InstanceMapScript
    {
    public:
        instance_priory_scenario() : InstanceMapScript(InstanceScriptName, MAP_PRIORY_SCENARIO) { }

        struct instance_priory_scenario_InstanceMapScript : public InstanceScript
        {
            struct Wave
            {
                GuidUnorderedSet Alive;
                ObjectGuid Player;
                uint32 LitEvent = 0;
                bool Started = false;
                bool Done = false;
            };

            explicit instance_priory_scenario_InstanceMapScript(InstanceMap* map) : InstanceScript(map)
            {
                SetHeaders("PRS");
                _waves[0].LitEvent = GameEventIds::NorthBrazierLit;
                _waves[1].LitEvent = GameEventIds::SouthBrazierLit;
            }

            void OnGameObjectCreate(GameObject* go) override
            {
                InstanceScript::OnGameObjectCreate(go);

                if (go->GetEntry() == GameObjects::HeavyShadows)
                {
                    if (go->GetPositionY() < 1200.0f)
                        _barrierCourtyard = go->GetGUID();  // live 59700061 (3014.6, 1084.7): gone at step 7009 (874119)
                    else
                        _barrierPriory = go->GetGUID();     // live 59700060 (2976.9, 1433.7): gone 20.7 s into step 7013 (881033)
                }
            }

            void ProcessEvent(WorldObject* obj, uint32 eventId, WorldObject* invoker) override
            {
                switch (eventId)
                {
                    case GameEventIds::NorthBrazierUsed:
                        StartWave(_waves[0], NorthWave, obj, invoker);
                        break;
                    case GameEventIds::SouthBrazierUsed:
                        StartWave(_waves[1], SouthWave, obj, invoker);
                        break;
                    default:
                        break;
                }
            }

            void OnUnitDeath(Unit* unit) override
            {
                Creature* creature = unit->ToCreature();
                if (!creature)
                    return;

                if (creature->GetEntry() == Creatures::ShadeswornCrusher)
                {
                    TriggerScenarioEvent(GameEventIds::CrusherSlain, ObjectGuid::Empty);   // 877774 kill -> 877784 criteria
                    return;
                }

                // The event comes when the second member of the wave dies (inferred from both waves: 868558, 874086).
                for (Wave& wave : _waves)
                {
                    if (wave.Done || !wave.Alive.erase(creature->GetGUID()) || !wave.Alive.empty())
                        continue;

                    wave.Done = true;
                    TriggerScenarioEvent(wave.LitEvent, wave.Player);
                }
            }

            void SetData(uint32 type, uint32 /*data*/) override
            {
                if (type == DATA_BROKEN_QUEEN_DEFEATED)
                    TriggerScenarioEvent(GameEventIds::BrokenQueenSlain, ObjectGuid::Empty);
            }

            void Update(uint32 diff) override
            {
                _scheduler.Update(diff);

                if (_checkTimer > diff)
                {
                    _checkTimer -= diff;
                    return;
                }
                _checkTimer = 500;

                InstanceScenario const* scenario = instance->GetInstanceScenario();
                if (!scenario)
                    return;

                uint32 stepId = scenario->GetStep() ? scenario->GetStep()->ID : 0;
                if (stepId != _lastStepId)
                {
                    _lastStepId = stepId;
                    OnStepStarted(stepId);
                }

                if (stepId == Steps::HarbingersEnd && _prioryOpen && !_enteredPriory)
                {
                    instance->DoOnPlayers([this](Player* player)
                    {
                        if (!_enteredPriory && player->IsAlive() && IsInPrioryEntrance(player))
                        {
                            _enteredPriory = true;
                            GameEvents::Trigger(GameEventIds::EnterThePriory, player, nullptr);
                        }
                    });
                }

                // The core has no scenario-complete hook. Retail casts 441666 on the player in the second the scenario completes
                // (SCENARIO_COMPLETED 881200, SPELL_GO 441666 at 881211).
                if (!_returnSent && scenario->IsComplete())
                {
                    _returnSent = true;
                    instance->DoOnPlayers([](Player* player)
                    {
                        player->CastSpell(player, Spells::FinaleReturn, true);
                    });
                }
            }

        private:
            template <size_t N>
            void StartWave(Wave& wave, WaveSpawn const (&spawns)[N], WorldObject* brazier, WorldObject* invoker)
            {
                if (wave.Started)
                    return;

                wave.Started = true;
                Player* player = invoker ? invoker->ToPlayer() : nullptr;
                if (player)
                    wave.Player = player->GetGUID();

                if (GameObject* go = brazier ? brazier->ToGameObject() : nullptr)
                    go->SetFlag(GO_FLAG_NOT_SELECTABLE);

                for (WaveSpawn const& spawn : spawns)
                {
                    TempSummon* summon = instance->SummonCreature(spawn.Entry, spawn.Pos);
                    if (!summon)
                        continue;

                    wave.Alive.insert(summon->GetGUID());
                    if (player && summon->IsAIEnabled())
                        summon->AI()->AttackStart(player);
                }

                if (wave.Alive.empty())   // nothing could be summoned: do not leave the step stuck
                {
                    wave.Done = true;
                    TriggerScenarioEvent(wave.LitEvent, wave.Player);
                }
            }

            void OnStepStarted(uint32 stepId)
            {
                switch (stepId)
                {
                    case Steps::DeeperIntoTheDark:
                        DespawnBarrier(_barrierCourtyard);
                        instance->SpawnGroupSpawn(SpawnGroups::ShadeswornCrusher);
                        CastOnPlayers(Spells::ConversationStep2);
                        break;
                    case Steps::BeaconForTheLost:
                        CastOnPlayers(Spells::ConversationStep3);
                        break;
                    case Steps::LongestShadow:
                        instance->SpawnGroupSpawn(SpawnGroups::BrokenQueen);
                        break;
                    case Steps::HarbingersEnd:
                        _scheduler.Schedule(20s, [this](TaskContext)
                        {
                            DespawnBarrier(_barrierPriory);
                            _prioryOpen = true;
                        });
                        break;
                    default:
                        break;
                }
            }

            void DespawnBarrier(ObjectGuid guid)
            {
                if (GameObject* go = instance->GetGameObject(guid))
                    go->DespawnOrUnsummon(0ms, 1h);
            }

            void CastOnPlayers(uint32 spellId)
            {
                instance->DoOnPlayers([spellId](Player* player)
                {
                    player->CastSpell(player, spellId, true);
                });
            }

            // AnyoneTriggerGameEventScenario is updated through the player that sends the event (GameEvents::TriggerForPlayer).
            void TriggerScenarioEvent(uint32 eventId, ObjectGuid preferred)
            {
                Player* player = preferred.IsEmpty() ? nullptr : ObjectAccessor::GetPlayer(instance, preferred);
                if (!player)
                {
                    instance->DoOnPlayers([&player](Player* candidate)
                    {
                        if (!player)
                            player = candidate;
                    });
                }

                if (player)
                    GameEvents::Trigger(eventId, player, nullptr);
            }

            TaskScheduler _scheduler;
            Wave _waves[2];
            ObjectGuid _barrierCourtyard;
            ObjectGuid _barrierPriory;
            uint32 _checkTimer = 500;
            uint32 _lastStepId = 0;
            bool _prioryOpen = false;
            bool _enteredPriory = false;
            bool _returnSent = false;
        };

        InstanceScript* GetInstanceScript(InstanceMap* map) const override
        {
            return new instance_priory_scenario_InstanceMapScript(map);
        }
    };

    // 214368 - The Broken Queen. Retail never kills her: at 880381 she is at 1 HP, immune (flags + 0x300), stand state 7, threat
    // cleared, and the step criterion 66415 follows 5.0 s later (880465).
    struct npc_priory_broken_queen : public ScriptedAI
    {
        npc_priory_broken_queen(Creature* creature) : ScriptedAI(creature), _defeated(false) { }

        void Reset() override
        {
            _scheduler.CancelAll();
            if (_defeated)
                return;

            me->SetImmuneToAll(false, false);
            me->SetStandState(UNIT_STAND_STATE_STAND);
            me->SetReactState(REACT_AGGRESSIVE);
        }

        void JustEngagedWith(Unit* /*who*/) override
        {
            DoCastSelf(Spells::DarkBarrage);
            _scheduler.Schedule(37s, [this](TaskContext context)   // second cast about 37 s after the first in the capture
            {
                DoCastSelf(Spells::DarkBarrage);
                context.Repeat(37s);
            });
        }

        void DamageTaken(Unit* /*attacker*/, uint32& damage, DamageEffectType /*damageType*/, SpellInfo const* /*spellInfo*/) override
        {
            if (_defeated)
            {
                damage = 0;
                return;
            }

            if (damage < me->GetHealth())
                return;

            damage = me->GetHealth() - 1;
            Defeat();
        }

        void UpdateAI(uint32 diff) override
        {
            _scheduler.Update(diff);

            if (_defeated)
                return;

            UpdateVictim();
        }

    private:
        void Defeat()
        {
            _defeated = true;
            _scheduler.CancelAll();
            me->SetImmuneToAll(true, false);
            me->SetReactState(REACT_PASSIVE);
            me->AttackStop();
            me->GetThreatManager().ClearAllThreat();
            me->CombatStop(true);
            EngagementOver();
            me->SetStandState(UNIT_STAND_STATE_DEAD);

            _scheduler.Schedule(5s, [this](TaskContext)
            {
                if (InstanceScript* instance = me->GetInstanceScript())
                    instance->SetData(DATA_BROKEN_QUEEN_DEFEATED, 1);
            });
        }

        TaskScheduler _scheduler;
        bool _defeated;
    };

    // 437038 - Teleport Finale
    // Effect 0 is SPELL_EFFECT_TELEPORT_TO_LFG_DUNGEON (LFG dungeon 2534, map 2716 difficulty 12), which the core leaves to EffectNULL.
    class spell_priory_teleport_finale : public SpellScript
    {
        bool Validate(SpellInfo const* spellInfo) override
        {
            return sSpellMgr->GetSpellTargetPosition(spellInfo->Id, EFFECT_0) != nullptr;
        }

        void HandleTeleport(SpellEffIndex effIndex)
        {
            Player* player = GetHitPlayer();
            if (!player)
                return;

            SpellTargetPosition const* dest = sSpellMgr->GetSpellTargetPosition(GetSpellInfo()->Id, effIndex);
            if (!dest)
                return;

            // MapDifficulty 5405 (map 2716, difficulty 12) has no reset schedule, so the core would put a second run back into the
            // finished instance of the first one.
            player->SetRecentInstance(dest->GetMapId(), 0);
            player->TeleportTo(*dest, TELE_TO_NONE, {}, GetSpellInfo()->Id);
        }

        void Register() override
        {
            OnEffectHitTarget += SpellEffectFn(spell_priory_teleport_finale::HandleTeleport, EFFECT_0, SPELL_EFFECT_TELEPORT_TO_LFG_DUNGEON);
        }
    };
}

void AddSC_instance_priory_scenario()
{
    using namespace Scripts::KhazAlgar::PrioryScenario;
    new instance_priory_scenario();
    RegisterCreatureAI(npc_priory_broken_queen);
    RegisterSpellScript(spell_priory_teleport_finale);
}
