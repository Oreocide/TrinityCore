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

#ifndef TRINITYCORE_WORLD_STATE_SCHEDULER_H
#define TRINITYCORE_WORLD_STATE_SCHEDULER_H

#include "Define.h"
#include <ctime>
#include <map>
#include <string>
#include <vector>

// Retail-shaped rotation scheduler (ScheduledInterval / ScheduledWorldStateGroup / ScheduledWorldState).
// Every interval is a fixed period counted from Reset.Epoch (+OffsetSecs); each period a group selects
// some of its rows and the selected rows' realm world states are set, all other owned states read 0.
enum class ScheduledSelectionType : uint8
{
    All             = 0,
    Sequential      = 1,    // rows by OrderIndex, one step per period (retail group 911, Ny'alotha)
    Ladder          = 2,    // treated as sequential
    WeightedRandom  = 3,    // weighted pick of SelectionCount rows (at most one per world state), seeded per group and period
    Progressive     = 5     // ModifierTree 237 unlocks, not implemented
};

struct ScheduledInterval
{
    uint32 Id = 0;
    int32 Flags = 0;
    int32 RepeatType = 0;
    int64 DurationSecs = 0;
    int64 OffsetSecs = 0;
    int32 DateAlignmentType = 0;
};

struct ScheduledWorldState
{
    uint32 Id = 0;
    uint32 GroupId = 0;
    int32 WorldStateId = 0;
    int32 Value = 0;
    int64 DurationSecs = 0;     // > 0: only on for this long from the start of the period
    uint32 Weight = 0;
    int32 UniqueCategory = 0;
    int32 Flags = 0;
    int32 OrderIndex = 0;
};

struct ScheduledWorldStateGroup
{
    uint32 Id = 0;
    int32 Flags = 0;
    ScheduledInterval const* Interval = nullptr;
    ScheduledSelectionType SelectionType = ScheduledSelectionType::All;
    uint32 SelectionCount = 0;
    int32 Priority = 0;
    int32 IndexOffset = 0;      // aligns a sequential cycle to the period index
    bool Enabled = false;
    std::string Comment;
    std::vector<ScheduledWorldState const*> States;   // sorted by OrderIndex, Id
};

namespace WorldStateScheduler
{
    TC_GAME_API void LoadFromDB();
    void Update();

    TC_GAME_API std::map<uint32, ScheduledWorldStateGroup> const& GetGroups();
    TC_GAME_API ScheduledWorldStateGroup const* GetGroup(uint32 groupId);

    // rows a group selects at time t (ignores Enabled and UniqueCategory)
    TC_GAME_API std::vector<ScheduledWorldState const*> SelectRows(ScheduledWorldStateGroup const& group, time_t t);
    TC_GAME_API int64 GetPeriodIndex(ScheduledWorldStateGroup const& group, time_t t);
    TC_GAME_API time_t GetNextBoundary();
    TC_GAME_API std::size_t GetOwnedWorldStateCount();

    // world / task quests (quest_scheduled_gate) whose every gate world state is set by an enabled group:
    // their completion lasts until the gate rotates instead of forever
    TC_GAME_API bool IsScheduledQuest(uint32 questId);
    // end of the current period of the groups that set the quest's gate
    TC_GAME_API time_t GetQuestCompletionExpiry(uint32 questId, time_t now);
    // scheduled quest whose gate does not match now
    TC_GAME_API bool IsQuestGateClosed(uint32 questId);
};

#endif // TRINITYCORE_WORLD_STATE_SCHEDULER_H
