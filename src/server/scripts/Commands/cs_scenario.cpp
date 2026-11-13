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
#include "Map.h"
#include "Player.h"
#include "RBAC.h"
#include "Scenario.h"
#include "StringFormat.h"
#include "WorldSession.h"

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

    // .scenario info - scenario of the map you are on, its current step and the progress of that step's criteria
    // (SendSysMessage + StringFormat: PSendSysMessage is printf-style and printed these {} literally)
    static bool HandleScenarioInfoCommand(ChatHandler* handler)
    {
        Player* player = handler->GetSession()->GetPlayer();
        Scenario* scenario = player->GetScenario();
        if (!scenario)
        {
            handler->SendSysMessage(Trinity::StringFormat("No scenario on this map (map {}, difficulty {}).", player->GetMapId(), uint32(player->GetMap()->GetDifficultyID())));
            return true;
        }

        handler->SendSysMessage(Trinity::StringFormat("Scenario {} '{}' complete: {}", scenario->GetEntry()->ID, scenario->GetEntry()->Name[handler->GetSessionDbcLocale()], scenario->IsComplete() ? "yes" : "no"));
        ScenarioStepEntry const* step = scenario->GetStep();
        if (!step)
        {
            handler->SendSysMessage("No current step.");
            return true;
        }

        handler->SendSysMessage(Trinity::StringFormat("Current step {} (order {}) '{}', criteria tree {}, step state {}, tree complete: {}", step->ID, uint32(step->OrderIndex),
            step->Title[handler->GetSessionDbcLocale()], step->Criteriatreeid, uint32(scenario->GetStepState(step)), scenario->IsCompletedStep(step) ? "yes" : "no"));

        CriteriaTree const* tree = sCriteriaMgr->GetCriteriaTree(step->Criteriatreeid);
        if (!tree)
        {
            handler->SendSysMessage(Trinity::StringFormat("Criteria tree {} is not loaded.", step->Criteriatreeid));
            return true;
        }

        CriteriaMgr::WalkCriteriaTree(tree, [&](CriteriaTree const* node)
        {
            if (!node->Criteria)
                return;

            CriteriaProgress const* progress = scenario->GetProgress(node->Criteria);
            handler->SendSysMessage(Trinity::StringFormat("  tree {} criteria {} {} ({}) asset {}: {} / {}", node->ID, node->Criteria->ID,
                CriteriaMgr::GetCriteriaTypeString(node->Criteria->Entry->Type), uint32(node->Criteria->Entry->Type), node->Criteria->Entry->Asset.ID,
                progress ? progress->Counter : 0, node->Entry->Amount));
        });
        return true;
    }

    // .scenario event <gameEventId> - fire a game event with you as source (same path as the scenario_event_trigger emitters)
    static bool HandleScenarioEventCommand(ChatHandler* handler, uint32 gameEventId)
    {
        Player* player = handler->GetSession()->GetPlayer();
        GameEvents::Trigger(gameEventId, player, nullptr);
        handler->SendSysMessage(Trinity::StringFormat("Triggered game event {}.", gameEventId));
        return true;
    }

    // .scenario internal <criteriaId> [count] - feed CompleteInternalCriteria (asset = criteria id) count times
    static bool HandleScenarioInternalCommand(ChatHandler* handler, uint32 criteriaId, Optional<uint32> count)
    {
        Player* player = handler->GetSession()->GetPlayer();
        uint32 n = count.value_or(1);
        for (uint32 i = 0; i < n; ++i)
            player->UpdateCriteria(CriteriaType::CompleteInternalCriteria, criteriaId);
        handler->SendSysMessage(Trinity::StringFormat("Fed internal criteria {} x{}.", criteriaId, n));
        return true;
    }
};

void AddSC_scenario_commandscript()
{
    new scenario_commandscript();
}
