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

#include "InstanceScenario.h"
#include "InstanceScript.h"
#include "Map.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "SpellScript.h"

// Hallowfall: Priory scenario (scenario 2416, map 2716), finale of quest 78948 "A Light in the Dark".
namespace Scripts::KhazAlgar::PrioryScenario
{
    static constexpr uint32 MAP_PRIORY_SCENARIO = 2716;

    namespace Spells
    {
        static constexpr uint32 FinaleReturn = 441666; // Play Movie 1019 + Teleport Units (spell_target_position, map 2601)
    }

    static constexpr char const* InstanceScriptName = "instance_priory_scenario";

    class instance_priory_scenario : public InstanceMapScript
    {
    public:
        instance_priory_scenario() : InstanceMapScript(InstanceScriptName, MAP_PRIORY_SCENARIO) { }

        struct instance_priory_scenario_InstanceMapScript : public InstanceScript
        {
            explicit instance_priory_scenario_InstanceMapScript(InstanceMap* map) : InstanceScript(map), _checkTimer(500), _returnSent(false)
            {
                SetHeaders("PRS");
            }

            void Update(uint32 diff) override
            {
                if (_returnSent)
                    return;

                if (_checkTimer > diff)
                {
                    _checkTimer -= diff;
                    return;
                }
                _checkTimer = 500;

                InstanceScenario const* scenario = instance->GetInstanceScenario();
                if (!scenario || !scenario->IsComplete())
                    return;

                // The core has no scenario-complete hook. Retail casts 441666 on the player in the second the scenario completes
                // (capture 2026-11-09 13-43-38: SCENARIO_COMPLETED idx 881200, SPELL_GO 441666 at 881211).
                _returnSent = true;
                instance->DoOnPlayers([](Player* player)
                {
                    player->CastSpell(player, Spells::FinaleReturn, true);
                });
            }

        private:
            uint32 _checkTimer;
            bool _returnSent;
        };

        InstanceScript* GetInstanceScript(InstanceMap* map) const override
        {
            return new instance_priory_scenario_InstanceMapScript(map);
        }
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
    RegisterSpellScript(spell_priory_teleport_finale);
}
