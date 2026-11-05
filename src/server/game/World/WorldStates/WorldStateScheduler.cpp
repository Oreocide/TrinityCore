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

#include "WorldStateScheduler.h"
#include "Config.h"
#include "DatabaseEnv.h"
#include "GameEventMgr.h"
#include "GameTime.h"
#include "Log.h"
#include "MapUtils.h"
#include "ObjectMgr.h"
#include "ResetClock.h"
#include "SharedDefines.h"
#include "Timer.h"
#include "World.h"
#include "WorldStateMgr.h"
#include <algorithm>
#include <limits>
#include <tuple>
#include <unordered_map>
#include <unordered_set>

namespace
{
struct Data
{
    std::unordered_map<uint32, ScheduledInterval> Intervals;
    std::unordered_map<uint32, ScheduledWorldState> States;
    std::map<uint32, ScheduledWorldStateGroup> Groups;
    std::vector<ScheduledWorldStateGroup const*> GroupsByPriority;   // enabled only
    std::unordered_set<int32> OwnedWorldStates;
    std::unordered_map<int32, std::vector<ScheduledWorldStateGroup const*>> GroupsByWorldState;   // enabled only
    std::unordered_map<uint32, std::vector<std::pair<int32, int32>>> QuestGates;                     // quest -> (world state, value)
    time_t NextBoundary = 0;
    bool Enabled = true;
    uint64 Seed = 0;
};

Data& GetData()
{
    static Data data;
    return data;
}

// splitmix64, so tools can reproduce the picks
uint64 NextRandom(uint64& state)
{
    uint64 z = (state += 0x9E3779B97F4A7C15ull);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

int64 PositiveModulo(int64 value, int64 divisor)
{
    int64 result = value % divisor;
    return result < 0 ? result + divisor : result;
}

// world states some other system writes; the scheduler must never overwrite them
bool IsOwnedElsewhere(int32 worldStateId, std::unordered_set<int32> const& gameEventStates, std::string* reason)
{
    if (WorldStateTemplate const* worldStateTemplate = WorldStateMgr::GetWorldStateTemplate(worldStateId))
    {
        if (!worldStateTemplate->MapIds.empty())
        {
            *reason = "map scoped world_state template (new maps would reseed it from the template)";
            return true;
        }
        if (worldStateTemplate->ScriptId)
        {
            *reason = "world_state template has a ScriptName";
            return true;
        }
    }

    if (gameEventStates.contains(worldStateId))
    {
        *reason = "set by a game_event / holiday";
        return true;
    }

    switch (worldStateId)
    {
        case WS_CURRENT_PVP_SEASON_ID:
        case WS_PREVIOUS_PVP_SEASON_ID:
        case WS_TEAM_IN_INSTANCE_ALLIANCE:
        case WS_TEAM_IN_INSTANCE_HORDE:
        case WS_WAR_MODE_HORDE_BUFF_VALUE:
        case WS_WAR_MODE_ALLIANCE_BUFF_VALUE:
            *reason = "set by core";
            return true;
        default:
            break;
    }

    return false;
}

// selected rows plus their on-window, appended to values (first writer wins, groups run by priority)
void Evaluate(time_t now, std::unordered_map<int32, int32>& values, time_t& nextBoundary)
{
    Data& data = GetData();
    std::unordered_set<int32> usedCategories;
    nextBoundary = std::numeric_limits<time_t>::max();

    for (ScheduledWorldStateGroup const* group : data.GroupsByPriority)
    {
        int64 index = WorldStateScheduler::GetPeriodIndex(*group, now);
        time_t periodStart = Trinity::ResetClock::PeriodStart(index, group->Interval->DurationSecs, group->Interval->OffsetSecs);
        nextBoundary = std::min(nextBoundary, time_t(periodStart + group->Interval->DurationSecs));

        std::vector<ScheduledWorldState const*> rows = WorldStateScheduler::SelectRows(*group, now);
        for (ScheduledWorldState const* row : rows)
        {
            if (row->UniqueCategory && !usedCategories.insert(row->UniqueCategory).second)
                continue;

            if (row->DurationSecs > 0)
            {
                time_t windowEnd = periodStart + row->DurationSecs;
                if (now >= windowEnd)
                    continue;

                nextBoundary = std::min(nextBoundary, windowEnd);
            }

            values.try_emplace(row->WorldStateId, row->Value);
        }
    }

    for (int32 worldStateId : data.OwnedWorldStates)
        values.try_emplace(worldStateId, 0);
}

void Apply(time_t now)
{
    Data& data = GetData();
    std::unordered_map<int32, int32> values;
    Evaluate(now, values, data.NextBoundary);

    uint32 changed = 0;
    for (auto const& [worldStateId, value] : values)
    {
        if (WorldStateMgr::GetValue(worldStateId, nullptr) == value)
            continue;

        WorldStateMgr::SetValue(worldStateId, value, false, nullptr);
        ++changed;
    }

    if (changed)
        TC_LOG_DEBUG("misc", "WorldStateScheduler: {} world states changed, next boundary at {}", changed, data.NextBoundary);
}
}

void WorldStateScheduler::LoadFromDB()
{
    uint32 oldMSTime = getMSTime();

    Data& data = GetData();
    data.Intervals.clear();
    data.States.clear();
    data.Groups.clear();
    data.GroupsByPriority.clear();
    data.OwnedWorldStates.clear();
    data.GroupsByWorldState.clear();
    data.QuestGates.clear();
    data.NextBoundary = 0;
    data.Enabled = sConfigMgr->GetBoolDefault("WorldStateScheduler.Enable", true);
    data.Seed = uint64(sConfigMgr->GetInt64Default("WorldStateScheduler.Seed", 0));

    if (!data.Enabled)
    {
        TC_LOG_INFO("server.loading", ">> World state scheduler disabled by config");
        return;
    }

    //                                                  0       1          2              3              4                5
    if (QueryResult result = WorldDatabase.Query("SELECT ID, Flags, RepeatType, DurationSecs, OffsetSecs, DateAlignmentType FROM scheduled_interval"))
    {
        do
        {
            Field* fields = result->Fetch();
            ScheduledInterval& interval = data.Intervals[fields[0].GetUInt32()];
            interval.Id = fields[0].GetUInt32();
            interval.Flags = fields[1].GetInt32();
            interval.RepeatType = fields[2].GetInt32();
            interval.DurationSecs = fields[3].GetInt64();
            interval.OffsetSecs = fields[4].GetInt64();
            interval.DateAlignmentType = fields[5].GetInt32();
        } while (result->NextRow());
    }

    //                                                  0       1              2                    3              4               5           6          7         8
    if (QueryResult result = WorldDatabase.Query("SELECT ID, Flags, ScheduledIntervalID, SelectionType, SelectionCount, Priority, IndexOffset, Enabled, Comment FROM scheduled_world_state_group"))
    {
        do
        {
            Field* fields = result->Fetch();
            uint32 id = fields[0].GetUInt32();
            ScheduledWorldStateGroup& group = data.Groups[id];
            group.Id = id;
            group.Flags = fields[1].GetInt32();
            group.Interval = Trinity::Containers::MapGetValuePtr(data.Intervals, fields[2].GetUInt32());
            group.SelectionType = ScheduledSelectionType(fields[3].GetUInt8());
            group.SelectionCount = fields[4].GetUInt32();
            group.Priority = fields[5].GetInt32();
            group.IndexOffset = fields[6].GetInt32();
            group.Enabled = fields[7].GetBool();
            group.Comment = fields[8].GetString();

            if (!group.Enabled)
                continue;

            if (!group.Interval || group.Interval->DurationSecs <= 0)
            {
                TC_LOG_ERROR("sql.sql", "Table `scheduled_world_state_group` has group {} with ScheduledIntervalID {} that does not exist or has no duration, group disabled.", id, fields[2].GetUInt32());
                group.Enabled = false;
                continue;
            }

            switch (group.SelectionType)
            {
                case ScheduledSelectionType::All:
                case ScheduledSelectionType::Sequential:
                case ScheduledSelectionType::Ladder:
                case ScheduledSelectionType::WeightedRandom:
                    break;
                default:
                    TC_LOG_ERROR("sql.sql", "Table `scheduled_world_state_group` has group {} with unsupported SelectionType {}, group disabled.", id, uint32(group.SelectionType));
                    group.Enabled = false;
                    break;
            }
        } while (result->NextRow());
    }

    //                                                  0         1              2           3            4           5              6           7          8
    if (QueryResult result = WorldDatabase.Query("SELECT ID, GroupID, WorldStateID, Value, DurationSecs, Weight, UniqueCategory, Flags, OrderIndex FROM scheduled_world_state"))
    {
        do
        {
            Field* fields = result->Fetch();
            uint32 id = fields[0].GetUInt32();
            uint32 groupId = fields[1].GetUInt32();
            ScheduledWorldStateGroup* group = Trinity::Containers::MapGetValuePtr(data.Groups, groupId);
            if (!group)
            {
                TC_LOG_ERROR("sql.sql", "Table `scheduled_world_state` has row {} with GroupID {} that does not exist, skipped.", id, groupId);
                continue;
            }

            ScheduledWorldState& state = data.States[id];
            state.Id = id;
            state.GroupId = groupId;
            state.WorldStateId = fields[2].GetInt32();
            state.Value = fields[3].GetInt32();
            state.DurationSecs = fields[4].GetInt64();
            state.Weight = fields[5].GetUInt32();
            state.UniqueCategory = fields[6].GetInt32();
            state.Flags = fields[7].GetInt32();
            state.OrderIndex = fields[8].GetInt32();
            group->States.push_back(&state);
        } while (result->NextRow());
    }

    std::unordered_set<int32> gameEventStates;
    for (GameEventData const& gameEvent : sGameEventMgr->GetEventMap())
        if (gameEvent.WorldStateId)
            gameEventStates.insert(*gameEvent.WorldStateId);

    uint32 enabledCount = 0;
    for (auto& [groupId, group] : data.Groups)
    {
        std::ranges::sort(group.States, [](ScheduledWorldState const* left, ScheduledWorldState const* right)
        {
            return std::tie(left->OrderIndex, left->Id) < std::tie(right->OrderIndex, right->Id);
        });

        if (!group.Enabled)
            continue;

        if (group.States.empty())
        {
            TC_LOG_ERROR("sql.sql", "Table `scheduled_world_state_group` has enabled group {} without rows in `scheduled_world_state`, group disabled.", groupId);
            group.Enabled = false;
            continue;
        }

        for (ScheduledWorldState const* state : group.States)
        {
            std::string reason;
            if (IsOwnedElsewhere(state->WorldStateId, gameEventStates, &reason))
            {
                TC_LOG_ERROR("sql.sql", "Table `scheduled_world_state` row {} (group {}) uses world state {}: {}. Group disabled.", state->Id, groupId, state->WorldStateId, reason);
                group.Enabled = false;
                break;
            }
        }

        if (!group.Enabled)
            continue;

        for (ScheduledWorldState const* state : group.States)
        {
            data.OwnedWorldStates.insert(state->WorldStateId);
            std::vector<ScheduledWorldStateGroup const*>& groups = data.GroupsByWorldState[state->WorldStateId];
            if (std::ranges::find(groups, &group) == groups.end())
                groups.push_back(&group);
        }

        data.GroupsByPriority.push_back(&group);
        ++enabledCount;
    }

    std::ranges::stable_sort(data.GroupsByPriority, std::greater<>(), &ScheduledWorldStateGroup::Priority);

    //                                                  0           1          2
    if (QueryResult result = WorldDatabase.Query("SELECT QuestID, WorldStateID, Value FROM quest_scheduled_gate"))
    {
        do
        {
            Field* fields = result->Fetch();
            uint32 questId = fields[0].GetUInt32();
            if (!sObjectMgr->GetQuestTemplate(questId))
            {
                TC_LOG_ERROR("sql.sql", "Table `quest_scheduled_gate` has quest {} that does not exist, skipped.", questId);
                continue;
            }

            data.QuestGates[questId].emplace_back(fields[1].GetInt32(), fields[2].GetInt32());
        } while (result->NextRow());
    }

    // only quests whose whole gate is rotated by an enabled group get period completions, the others stay one-time
    std::erase_if(data.QuestGates, [&](auto const& questGates)
    {
        return std::ranges::any_of(questGates.second, [&](std::pair<int32, int32> const& gate) { return !data.OwnedWorldStates.contains(gate.first); });
    });

    TC_LOG_INFO("server.loading", ">> Loaded {} scheduled intervals, {} scheduled world state groups ({} enabled, {} world states), {} scheduled world states and {} gated quests in {} ms",
        data.Intervals.size(), data.Groups.size(), enabledCount, data.OwnedWorldStates.size(), data.States.size(), data.QuestGates.size(), GetMSTimeDiffToNow(oldMSTime));

    Apply(GameTime::GetGameTime());
}

void WorldStateScheduler::Update()
{
    Data& data = GetData();
    if (!data.Enabled || data.GroupsByPriority.empty())
        return;

    time_t now = GameTime::GetGameTime();
    if (now < data.NextBoundary)
        return;

    Apply(now);
}

std::map<uint32, ScheduledWorldStateGroup> const& WorldStateScheduler::GetGroups()
{
    return GetData().Groups;
}

ScheduledWorldStateGroup const* WorldStateScheduler::GetGroup(uint32 groupId)
{
    return Trinity::Containers::MapGetValuePtr(GetData().Groups, groupId);
}

int64 WorldStateScheduler::GetPeriodIndex(ScheduledWorldStateGroup const& group, time_t t)
{
    return Trinity::ResetClock::PeriodIndex(t, group.Interval->DurationSecs, group.Interval->OffsetSecs);
}

std::vector<ScheduledWorldState const*> WorldStateScheduler::SelectRows(ScheduledWorldStateGroup const& group, time_t t)
{
    std::vector<ScheduledWorldState const*> selected;
    if (!group.Interval || group.Interval->DurationSecs <= 0 || group.States.empty())
        return selected;

    int64 index = GetPeriodIndex(group, t);
    std::size_t count = std::min<std::size_t>(std::max<uint32>(group.SelectionCount, 1), group.States.size());

    switch (group.SelectionType)
    {
        case ScheduledSelectionType::All:
            selected = group.States;
            break;
        case ScheduledSelectionType::Sequential:
        case ScheduledSelectionType::Ladder:
        {
            int64 size = int64(group.States.size());
            int64 first = PositiveModulo(index + group.IndexOffset, size);
            for (std::size_t i = 0; i < count; ++i)
                selected.push_back(group.States[std::size_t((first + int64(i)) % size)]);
            break;
        }
        case ScheduledSelectionType::WeightedRandom:
        {
            uint64 random = (GetData().Seed << 32) ^ (uint64(group.Id) * 0x100000001B3ull) ^ uint64(index);
            std::vector<ScheduledWorldState const*> candidates;
            for (ScheduledWorldState const* state : group.States)
                if (state->Weight)
                    candidates.push_back(state);

            while (selected.size() < count && !candidates.empty())
            {
                uint64 totalWeight = 0;
                for (ScheduledWorldState const* state : candidates)
                    totalWeight += state->Weight;

                uint64 roll = NextRandom(random) % totalWeight;
                auto itr = candidates.begin();
                for (; itr != candidates.end(); ++itr)
                {
                    if (roll < (*itr)->Weight)
                        break;
                    roll -= (*itr)->Weight;
                }

                // one value per world state: a group may hold several values of each of several states (retail 941: 4 slots x 4 sets)
                int32 worldStateId = (*itr)->WorldStateId;
                selected.push_back(*itr);
                std::erase_if(candidates, [worldStateId](ScheduledWorldState const* state) { return state->WorldStateId == worldStateId; });
            }
            break;
        }
        default:
            break;
    }

    return selected;
}

time_t WorldStateScheduler::GetNextBoundary()
{
    return GetData().NextBoundary;
}

std::size_t WorldStateScheduler::GetOwnedWorldStateCount()
{
    return GetData().OwnedWorldStates.size();
}

bool WorldStateScheduler::IsScheduledQuest(uint32 questId)
{
    return GetData().QuestGates.contains(questId);
}

time_t WorldStateScheduler::GetQuestCompletionExpiry(uint32 questId, time_t now)
{
    Data& data = GetData();
    time_t expiry = std::numeric_limits<time_t>::max();
    if (std::vector<std::pair<int32, int32>> const* gates = Trinity::Containers::MapGetValuePtr(data.QuestGates, questId))
        for (auto const& [worldStateId, value] : *gates)
            if (std::vector<ScheduledWorldStateGroup const*> const* groups = Trinity::Containers::MapGetValuePtr(data.GroupsByWorldState, worldStateId))
                for (ScheduledWorldStateGroup const* group : *groups)
                    expiry = std::min(expiry, Trinity::ResetClock::NextBoundary(now, group->Interval->DurationSecs, group->Interval->OffsetSecs));

    return expiry;
}

bool WorldStateScheduler::IsQuestGateClosed(uint32 questId)
{
    Data& data = GetData();
    std::vector<std::pair<int32, int32>> const* gates = Trinity::Containers::MapGetValuePtr(data.QuestGates, questId);
    if (!gates)
        return false;

    return std::ranges::any_of(*gates, [](std::pair<int32, int32> const& gate) { return WorldStateMgr::GetValue(gate.first, nullptr) != gate.second; });
}
