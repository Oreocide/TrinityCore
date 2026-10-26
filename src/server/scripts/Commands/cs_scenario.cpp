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

/* ScriptData
Name: scenario_commandscript
%Complete: 100
Comment: scenario debugging commands (patterns/SCENARIO_FOUNDATION.md)
Category: commandscripts
EndScriptData */

#include "ScriptMgr.h"
#include "Chat.h"
#include "ChatCommand.h"
#include "CriteriaHandler.h"
#include "DB2Stores.h"
#include "GameEventSender.h"
#include "Player.h"
#include "RBAC.h"
#include "Scenario.h"

using namespace Trinity::ChatCommands;

class scenario_commandscript : public CommandScript
{
public:
    scenario_commandscript() : CommandScript("scenario_commandscript") { }

    std::span<ChatCommandBuilder const> GetCommands() const override
    {
        static ChatCommandTable scenarioCommandTable =
        {
            { "info",     HandleScenarioInfoCommand,     rbac::RBAC_PERM_COMMAND_DEBUG, Console::No },
            { "event",    HandleScenarioEventCommand,    rbac::RBAC_PERM_COMMAND_DEBUG, Console::No },
            { "internal", HandleScenarioInternalCommand, rbac::RBAC_PERM_COMMAND_DEBUG, Console::No },
        };
        static ChatCommandTable commandTable =
        {
            { "scenario", scenarioCommandTable },
        };
        return commandTable;
    }

    // .scenario info - scenario of the map you are on and its current step
    static bool HandleScenarioInfoCommand(ChatHandler* handler)
    {
        Player* player = handler->GetSession()->GetPlayer();
        Scenario const* scenario = player->GetScenario();
        if (!scenario)
        {
            handler->PSendSysMessage("No scenario on this map (map {}, difficulty {}).", player->GetMapId(), uint32(player->GetMap()->GetDifficultyID()));
            return true;
        }

        handler->PSendSysMessage("Scenario {} '{}' complete: {}", scenario->GetEntry()->ID, scenario->GetEntry()->Name[handler->GetSessionDbcLocale()], scenario->IsComplete() ? "yes" : "no");
        if (ScenarioStepEntry const* step = scenario->GetStep())
            handler->PSendSysMessage("Current step {} (order {}) '{}', criteria tree {}", step->ID, uint32(step->OrderIndex), step->Title[handler->GetSessionDbcLocale()], step->Criteriatreeid);
        return true;
    }

    // .scenario event <gameEventId> - fire a game event with you as source (same path as the scenario_event_trigger emitters)
    static bool HandleScenarioEventCommand(ChatHandler* handler, uint32 gameEventId)
    {
        Player* player = handler->GetSession()->GetPlayer();
        GameEvents::Trigger(gameEventId, player, nullptr);
        handler->PSendSysMessage("Triggered game event {}.", gameEventId);
        return true;
    }

    // .scenario internal <criteriaId> [count] - feed CompleteInternalCriteria (asset = criteria id) count times
    static bool HandleScenarioInternalCommand(ChatHandler* handler, uint32 criteriaId, Optional<uint32> count)
    {
        Player* player = handler->GetSession()->GetPlayer();
        uint32 n = count.value_or(1);
        for (uint32 i = 0; i < n; ++i)
            player->UpdateCriteria(CriteriaType::CompleteInternalCriteria, criteriaId);
        handler->PSendSysMessage("Fed internal criteria {} x{}.", criteriaId, n);
        return true;
    }
};

void AddSC_scenario_commandscript()
{
    new scenario_commandscript();
}
