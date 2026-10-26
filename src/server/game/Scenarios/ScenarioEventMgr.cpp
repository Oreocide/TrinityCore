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

#include "ScenarioEventMgr.h"
#include "DatabaseEnv.h"
#include "GameEventSender.h"
#include "Log.h"
#include "Player.h"
#include "Scenario.h"
#include "ScenarioMgr.h"
#include "Timer.h"

ScenarioEventMgr* ScenarioEventMgr::Instance()
{
    static ScenarioEventMgr instance;
    return &instance;
}

void ScenarioEventMgr::LoadFromDB()
{
    uint32 oldMSTime = getMSTime();

    _triggers.clear();
    std::fill(std::begin(_hasTriggers), std::end(_hasTriggers), false);

    //                                                   0            1           2      3           4
    QueryResult result = WorldDatabase.Query("SELECT TriggerType, TriggerArg, MapId, ScenarioId, EventId FROM scenario_event_trigger");
    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 scenario event triggers. DB table `scenario_event_trigger` is empty.");
        return;
    }

    do
    {
        Field* fields = result->Fetch();

        uint8 typeId = fields[0].GetUInt8();
        if (!typeId || typeId >= uint8(ScenarioEventTriggerType::Max))
        {
            TC_LOG_ERROR("sql.sql", "Table `scenario_event_trigger` has invalid TriggerType {}, skipped.", typeId);
            continue;
        }

        ScenarioEventTriggerType type = ScenarioEventTriggerType(typeId);
        uint32 arg = fields[1].GetUInt32();
        Row row;
        row.MapId = fields[2].GetUInt32();
        row.ScenarioId = fields[3].GetUInt32();
        row.EventId = fields[4].GetUInt32();

        if (!row.EventId)
        {
            TC_LOG_ERROR("sql.sql", "Table `scenario_event_trigger` (TriggerType {}, TriggerArg {}) has EventId 0, skipped.", typeId, arg);
            continue;
        }

        _triggers.emplace(MakeKey(type, arg), row);
        _hasTriggers[size_t(type)] = true;
    }
    while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded {} scenario event triggers in {} ms", _triggers.size(), GetMSTimeDiffToNow(oldMSTime));
}

void ScenarioEventMgr::Trigger(ScenarioEventTriggerType type, uint32 arg, Player* player, WorldObject* other /*= nullptr*/) const
{
    if (!HasTriggers(type) || !player)
        return;

    auto range = _triggers.equal_range(MakeKey(type, arg));
    if (range.first == range.second)
        return;

    Scenario const* scenario = player->GetScenario();
    for (auto itr = range.first; itr != range.second; ++itr)
    {
        Row const& row = itr->second;
        if (row.MapId && row.MapId != player->GetMapId())
            continue;

        if (row.ScenarioId && (!scenario || scenario->GetEntry()->ID != row.ScenarioId))
            continue;

        GameEvents::Trigger(row.EventId, player, other);
    }
}
