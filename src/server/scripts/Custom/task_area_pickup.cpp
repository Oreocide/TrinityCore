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
 * Task quest area pickup: world quests and bonus objectives (QuestType 3) are added to the quest log when a player
 * enters the quest's area and dropped again when the player leaves it. Progress of a dropped quest is kept in memory
 * and put back when it is added again. Task quests without a quest ender are rewarded as soon as they are complete.
 *
 * The quest's area is its AreaGroupID (quest_template, from retail's quest query): the player's area, subarea or any
 * of their parents is in the group's AreaGroupMember list. Retail sniffs matched this on every add (52/52) and every
 * drop that was not a reward (67/67) where the capture recorded the area (Oreocide/TrinityCore#77). Quests without an
 * AreaGroupID fall back to the map outline (quest_poi blobs with 3+ points): added near it, dropped well past it.
 * Quests with only single-point POIs (world bosses, rares) use a radius around the points: retail adds those at a
 * fixed distance per quest that no data source holds (40-150 yd in sniffs, typically ~95), so one radius stands in.
 *
 * Measured on retail sniffs (trinitycore-tracking/docs/sniff-analysis/poi-areatriggers/REPORT.md): outline adds happen
 * a median 45 yd outside the outline, drops a median 110 yd past it; progress survives a drop; the reward arrives with
 * the last credit (SMSG_QUEST_GIVER_QUEST_COMPLETE, no turn-in).
 *
 * Which quests: task quests with an area group or an outline and no quest starter. Quests in quest_scheduled_gate only while
 * their gate is open (WorldStateScheduler); ungated ones only when QuestInfoID is 0 and the quest is not reset by
 * the scheduler (always-on bonus objectives). Each blob's PlayerConditionID must hold. Real players only: sessions
 * without a socket (playerbots, headless QA) are skipped unless their account is listed in SocketlessAccounts.
 *
 * Keys (worldserver.conf.d), read at startup and on reload config:
 *   TaskAreaPickup.Enable             0     master switch (pickup, drop and auto-reward)
 *   TaskAreaPickup.AddMargin          45    yd outside the outline at which the quest is added
 *   TaskAreaPickup.DropMargin         110   yd outside the outline at which the quest is dropped
 *   TaskAreaPickup.PointAddRadius     95    yd from a single-point POI (no area group, no outline) to add the quest
 *   TaskAreaPickup.PointDropRadius    200   yd from it to drop the quest
 *   TaskAreaPickup.Interval           1000  ms between checks per player
 *   TaskAreaPickup.AutoReward         1     reward complete task quests that have no quest ender
 *   TaskAreaPickup.SocketlessAccounts ""    account ids without a socket that still take part (QA characters)
 *   TaskAreaPickup.ExcludeQuests      ""    quest ids never added (unused / dev task quests that carry an outline)
 *
 * Only quests this script added in the current session are dropped; auto-reward follows the same player rule.
 */

#include "ScriptMgr.h"
#include "Config.h"
#include "ConditionMgr.h"
#include "DatabaseEnv.h"
#include "DB2Stores.h"
#include "Log.h"
#include "Map.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "QuestDef.h"
#include "StringConvert.h"
#include "StringFormat.h"
#include "Util.h"
#include "WorldSession.h"
#include "WorldStateScheduler.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <limits>
#include <cmath>
#include <deque>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

namespace
{
constexpr uint32 QUEST_TYPE_TASK = 3;
constexpr float MIN_MOVE_YD = 3.0f;
constexpr uint32 SCAN_STEP_MS = 500;

struct Blob
{
    int32 ConditionId = 0;
    float MinX = 0.0f, MinY = 0.0f, MaxX = 0.0f, MaxY = 0.0f;
    std::vector<std::pair<float, float>> Points;
};

struct TaskArea
{
    uint32 QuestId = 0;
    bool Point = false; // single-point POIs only: radius instead of outline margins
    float MinX = 0.0f, MinY = 0.0f, MaxX = 0.0f, MaxY = 0.0f;
    std::vector<Blob> Blobs;
};

struct Settings
{
    std::atomic<bool> Enable{ false };
    std::atomic<bool> AutoReward{ true };
    std::atomic<float> AddMargin{ 45.0f };
    std::atomic<float> DropMargin{ 110.0f };
    std::atomic<float> PointAddRadius{ 95.0f };
    std::atomic<float> PointDropRadius{ 200.0f };
    std::atomic<uint32> Interval{ 1000 };
    std::mutex AccountLock;
    std::unordered_set<uint32> SocketlessAccounts;
    std::unordered_set<uint32> ExcludeQuests;
};

struct PlayerState
{
    uint32 SinceCheck = 0;
    uint32 MapId = 0;
    float X = 0.0f, Y = 0.0f;
    uint32 AreaId = 0;
    bool Checked = false;
};

struct GroupTask
{
    std::unordered_set<uint32> Areas;
    std::unordered_set<uint32> Maps;
};

// read-only after AddSC_task_area_pickup (built once, before any map updates)
std::unordered_map<uint32 /*mapId*/, std::vector<TaskArea>> AreasByMap;
std::unordered_map<uint32 /*questId*/, std::pair<uint32 /*mapId*/, std::size_t /*index*/>> AreaByQuest;
std::unordered_map<uint32 /*questId*/, GroupTask> GroupByQuest;
std::unordered_map<uint32 /*areaId*/, std::vector<uint32 /*questId*/>> GroupQuestsByArea;
std::unordered_set<uint32> ScanMaps;
std::unordered_set<uint32> GatedQuests;
std::unordered_set<uint32> StarterQuests;
std::deque<std::string> ScriptNames;

Settings Config;

// per player, touched from several map threads
std::mutex StateLock;
std::unordered_map<ObjectGuid, PlayerState> States;
std::unordered_map<ObjectGuid, std::unordered_map<uint32 /*questId*/, std::vector<std::pair<uint32 /*objectiveId*/, int32>>>> SavedProgress;
// quests this script added to the player's log this session: the only ones it ever drops (a quest from a spell,
// item, script or an earlier session stays put)
std::unordered_map<ObjectGuid, std::unordered_set<uint32>> Owned;

bool IsOwned(Player const* player, uint32 questId)
{
    std::scoped_lock lock(StateLock);
    auto itr = Owned.find(player->GetGUID());
    return itr != Owned.end() && itr->second.contains(questId);
}

void SetOwned(Player const* player, uint32 questId, bool owned)
{
    std::scoped_lock lock(StateLock);
    if (owned)
        Owned[player->GetGUID()].insert(questId);
    else if (auto itr = Owned.find(player->GetGUID()); itr != Owned.end())
        itr->second.erase(questId);
}

bool IsExcluded(uint32 questId)
{
    std::scoped_lock lock(Config.AccountLock);
    return Config.ExcludeQuests.contains(questId);
}

bool Inside(float x, float y, std::vector<std::pair<float, float>> const& poly)
{
    bool in = false;
    for (std::size_t i = 0, j = poly.size() - 1; i < poly.size(); j = i++)
    {
        auto [xi, yi] = poly[i];
        auto [xj, yj] = poly[j];
        if ((yi > y) != (yj > y) && x < (xj - xi) * (y - yi) / (yj - yi) + xi)
            in = !in;
    }
    return in;
}

float EdgeDistance(float x, float y, std::vector<std::pair<float, float>> const& poly)
{
    float best = std::numeric_limits<float>::max();
    for (std::size_t i = 0, j = poly.size() - 1; i < poly.size(); j = i++)
    {
        auto [x1, y1] = poly[j];
        auto [x2, y2] = poly[i];
        float dx = x2 - x1, dy = y2 - y1;
        float len = dx * dx + dy * dy;
        float t = len > 0.0f ? std::clamp(((x - x1) * dx + (y - y1) * dy) / len, 0.0f, 1.0f) : 0.0f;
        best = std::min(best, std::hypot(x - x1 - t * dx, y - y1 - t * dy));
    }
    return best;
}

// distance outside the quest's outline for this player: 0 inside, max float when no blob applies to the player
float OutsideDistance(Player const* player, TaskArea const& area, float x, float y)
{
    float best = std::numeric_limits<float>::max();
    for (Blob const& blob : area.Blobs)
    {
        if (blob.ConditionId && !ConditionMgr::IsPlayerMeetingCondition(player, blob.ConditionId))
            continue;
        if (Inside(x, y, blob.Points))
            return 0.0f;
        best = std::min(best, EdgeDistance(x, y, blob.Points));
    }
    return best;
}

bool HasEnder(uint32 questId)
{
    auto creatures = sObjectMgr->GetCreatureQuestInvolvedRelationReverseBounds(questId);
    auto gameObjects = sObjectMgr->GetGOQuestInvolvedRelationReverseBounds(questId);
    return creatures.begin() != creatures.end() || gameObjects.begin() != gameObjects.end();
}

bool IsAreaTask(Quest const* quest)
{
    return quest->GetQuestType() == QUEST_TYPE_TASK
        && (GroupByQuest.contains(quest->GetQuestId()) || AreaByQuest.contains(quest->GetQuestId()))
        && !StarterQuests.contains(quest->GetQuestId());
}

// the player's area and its parents (subarea -> area -> zone)
std::vector<uint32> AreaChain(uint32 areaId)
{
    std::vector<uint32> chain;
    while (areaId && chain.size() < 8)
    {
        chain.push_back(areaId);
        AreaTableEntry const* area = sAreaTableStore.LookupEntry(areaId);
        if (!area)
            break;
        areaId = area->ParentAreaID;
    }
    return chain;
}

bool InGroupArea(GroupTask const& group, uint32 mapId, std::vector<uint32> const& chain)
{
    return group.Maps.contains(mapId) && std::ranges::any_of(chain, [&](uint32 areaId) { return group.Areas.contains(areaId); });
}

bool IsTaskOpen(Quest const* quest)
{
    uint32 questId = quest->GetQuestId();
    if (GatedQuests.contains(questId))
        return WorldStateScheduler::IsScheduledQuest(questId) && !WorldStateScheduler::IsQuestGateClosed(questId);

    return quest->GetQuestInfoID() == 0 && !quest->IsResetByScheduler();
}

bool TakesPart(Player const* player)
{
    WorldSession const* session = player->GetSession();
    if (!session->GetRemoteAddress().empty())
        return true;

    std::scoped_lock lock(Config.AccountLock);
    return Config.SocketlessAccounts.contains(session->GetAccountId());
}

void RestoreProgress(Player* player, Quest const* quest)
{
    std::vector<std::pair<uint32, int32>> saved;
    {
        std::scoped_lock lock(StateLock);
        auto playerItr = SavedProgress.find(player->GetGUID());
        if (playerItr == SavedProgress.end())
            return;
        auto questItr = playerItr->second.find(quest->GetQuestId());
        if (questItr == playerItr->second.end())
            return;
        saved = std::move(questItr->second);
        playerItr->second.erase(questItr);
    }

    uint16 slot = player->FindQuestSlot(quest->GetQuestId());
    if (slot >= MAX_QUEST_LOG_SIZE)
        return;

    for (QuestObjective const& objective : quest->Objectives)
        for (auto const& [objectiveId, value] : saved)
            if (objective.ID == objectiveId && value > player->GetQuestSlotObjectiveData(slot, objective))
                player->SetQuestObjectiveData(objective, value);

    if (player->CanCompleteQuest(quest->GetQuestId()))
        player->CompleteQuest(quest->GetQuestId());
}

void AddTask(Player* player, Quest const* quest)
{
    player->AddQuest(quest, nullptr);
    SetOwned(player, quest->GetQuestId(), true);
    RestoreProgress(player, quest);
    TC_LOG_DEBUG("scripts", "TaskAreaPickup: added quest {} to {}", quest->GetQuestId(), player->GetGUID().ToString());
}

void DropTask(Player* player, Quest const* quest)
{
    uint32 questId = quest->GetQuestId();
    uint16 slot = player->FindQuestSlot(questId);
    if (slot >= MAX_QUEST_LOG_SIZE)
        return;

    std::vector<std::pair<uint32, int32>> progress;
    for (QuestObjective const& objective : quest->Objectives)
        if (int32 value = player->GetQuestSlotObjectiveData(slot, objective))
            progress.emplace_back(objective.ID, value);

    if (!progress.empty())
    {
        std::scoped_lock lock(StateLock);
        SavedProgress[player->GetGUID()][questId] = std::move(progress);
    }

    SetOwned(player, questId, false);
    player->RemoveActiveQuest(questId);
    if (quest->GetLimitTime())
        player->RemoveTimedQuest(questId);
    player->SendForceSpawnTrackingUpdate(questId);
    player->DespawnPersonalSummonsForQuest(questId);
    TC_LOG_DEBUG("scripts", "TaskAreaPickup: dropped quest {} from {}", questId, player->GetGUID().ToString());
}

// an owned task quest the player is no longer in (or near, for outlines) on this map
bool ShouldDrop(Player const* player, uint32 questId, std::vector<uint32> const& chain, float x, float y)
{
    if (auto group = GroupByQuest.find(questId); group != GroupByQuest.end())
        return !InGroupArea(group->second, player->GetMapId(), chain);

    auto [mapId, index] = AreaByQuest.at(questId);
    if (mapId != player->GetMapId())
        return true;
    TaskArea const& area = AreasByMap.at(mapId)[index];
    return OutsideDistance(player, area, x, y) > (area.Point ? Config.PointDropRadius.load() : Config.DropMargin.load());
}

bool TryAdd(Player* player, uint32 questId)
{
    Quest const* quest = sObjectMgr->GetQuestTemplate(questId);
    if (!quest || player->GetQuestStatus(questId) != QUEST_STATUS_NONE || !IsTaskOpen(quest) || IsExcluded(questId))
        return false;
    if (!player->CanTakeQuest(quest, false) || !player->CanAddQuest(quest, false))
        return false;
    AddTask(player, quest);
    return true;
}

void CheckPlayer(Player* player, std::vector<TaskArea> const* areas)
{
    float x = player->GetPositionX(), y = player->GetPositionY();
    float addMargin = Config.AddMargin.load(), pointRadius = Config.PointAddRadius.load();
    std::vector<uint32> chain = AreaChain(player->GetAreaId());

    // drops: task quests in the log that this system owns, now outside their area
    std::vector<Quest const*> drops;
    for (uint8 slot = 0; slot < MAX_QUEST_LOG_SIZE; ++slot)
    {
        uint32 questId = player->GetQuestSlotQuestId(slot);
        if (!questId)
            continue;
        Quest const* quest = sObjectMgr->GetQuestTemplate(questId);
        if (!quest || !IsAreaTask(quest) || !IsOwned(player, questId) || player->GetQuestStatus(questId) == QUEST_STATUS_COMPLETE)
            continue;
        if (ShouldDrop(player, questId, chain, x, y))
            drops.push_back(quest);
    }
    for (Quest const* quest : drops)
        DropTask(player, quest);

    // adds: open task quests whose area group holds the player's area
    for (uint32 areaId : chain)
        if (auto itr = GroupQuestsByArea.find(areaId); itr != GroupQuestsByArea.end())
            for (uint32 questId : itr->second)
                if (GroupByQuest.at(questId).Maps.contains(player->GetMapId()))
                    TryAdd(player, questId);

    // adds: open task quests (without an area group) whose outline the player is in or near
    if (!areas)
        return;
    for (TaskArea const& area : *areas)
    {
        float margin = area.Point ? pointRadius : addMargin;
        if (x < area.MinX - margin || x > area.MaxX + margin || y < area.MinY - margin || y > area.MaxY + margin)
            continue;
        if (player->GetQuestStatus(area.QuestId) != QUEST_STATUS_NONE || OutsideDistance(player, area, x, y) > margin)
            continue;
        TryAdd(player, area.QuestId);
    }
}

class task_area_pickup_map : public WorldMapScript
{
public:
    task_area_pickup_map(char const* name, uint32 mapId) : WorldMapScript(name, mapId), _mapId(mapId) { }

    void OnUpdate(Map* map, uint32 diff) override
    {
        if (!Config.Enable.load())
            return;

        auto areaItr = AreasByMap.find(_mapId);
        std::vector<TaskArea> const* areas = areaItr != AreasByMap.end() ? &areaItr->second : nullptr;

        // scan the map's players once per step instead of every map tick; a world map has one Map, or one per team on
        // faction-split maps (instance id = TeamId), each updated by one thread, so each gets its own counter
        uint32 interval = Config.Interval.load();
        uint32& sinceScan = _sinceScan[std::min<std::size_t>(map->GetInstanceId(), _sinceScan.size() - 1)];
        sinceScan += diff;
        if (sinceScan < std::min<uint32>(interval, SCAN_STEP_MS))
            return;
        diff = sinceScan;
        sinceScan = 0;

        for (MapReference const& ref : map->GetPlayers())
        {
            Player* player = ref.GetSource();
            if (!player || !player->IsInWorld() || player->IsBeingTeleported() || player->isDead() || player->IsInFlight()
                || !TakesPart(player))
                continue;

            bool check = false;
            {
                std::scoped_lock lock(StateLock);
                PlayerState& state = States[player->GetGUID()];
                state.SinceCheck += diff;
                if (state.SinceCheck >= interval)
                {
                    state.SinceCheck = 0;
                    bool moved = !state.Checked || state.MapId != _mapId || state.AreaId != player->GetAreaId()
                        || std::hypot(player->GetPositionX() - state.X, player->GetPositionY() - state.Y) >= MIN_MOVE_YD;
                    if (moved)
                    {
                        state.Checked = true;
                        state.MapId = _mapId;
                        state.AreaId = player->GetAreaId();
                        state.X = player->GetPositionX();
                        state.Y = player->GetPositionY();
                        check = true;
                    }
                }
            }

            if (check)
                CheckPlayer(player, areas);
        }
    }

private:
    uint32 _mapId;
    std::array<uint32, 3> _sinceScan = { };
};

class task_area_pickup_player : public PlayerScript
{
public:
    task_area_pickup_player() : PlayerScript("task_area_pickup_player") { }

    void OnQuestStatusChange(Player* player, uint32 changedQuestId) override
    {
        Quest const* quest = sObjectMgr->GetQuestTemplate(changedQuestId);
        if (!quest || quest->GetQuestType() != QUEST_TYPE_TASK)
            return;

        QuestStatus newStatus = player->GetQuestRewardStatus(changedQuestId) ? QUEST_STATUS_REWARDED : player->GetQuestStatus(changedQuestId);

        if (newStatus == QUEST_STATUS_REWARDED || newStatus == QUEST_STATUS_NONE)
        {
            std::scoped_lock lock(StateLock);
            if (auto itr = SavedProgress.find(player->GetGUID()); itr != SavedProgress.end() && newStatus == QUEST_STATUS_REWARDED)
                itr->second.erase(quest->GetQuestId());
            if (auto itr = Owned.find(player->GetGUID()); itr != Owned.end())
                itr->second.erase(quest->GetQuestId());
            return;
        }

        if (newStatus != QUEST_STATUS_COMPLETE || !Config.Enable.load() || !Config.AutoReward.load() || HasEnder(quest->GetQuestId())
            || !TakesPart(player))
            return;

        // reward on the player's own next update, not inside the status change
        uint32 questId = quest->GetQuestId();
        player->m_Events.AddEventAtOffset([player, questId]()
        {
            Quest const* quest = sObjectMgr->GetQuestTemplate(questId);
            if (!quest || player->GetQuestStatus(questId) != QUEST_STATUS_COMPLETE || !player->CanRewardQuest(quest, false))
                return;
            player->RewardQuest(quest, LootItemType::Item, 0, player, false);
        }, 0ms);
    }

    // a task quest from another map is dropped when the player changes map (retail drops it on leaving the area); the
    // new map may have no scan, so this is the only check there
    void OnMapChanged(Player* player) override
    {
        if (!Config.Enable.load() || !TakesPart(player))
            return;

        auto onOtherMap = [player](uint32 questId)
        {
            if (auto group = GroupByQuest.find(questId); group != GroupByQuest.end())
                return !group->second.Maps.contains(player->GetMapId());
            return AreaByQuest.at(questId).first != player->GetMapId();
        };

        std::vector<Quest const*> drops;
        for (uint8 slot = 0; slot < MAX_QUEST_LOG_SIZE; ++slot)
            if (uint32 questId = player->GetQuestSlotQuestId(slot))
                if (Quest const* quest = sObjectMgr->GetQuestTemplate(questId))
                    if (IsAreaTask(quest) && IsOwned(player, questId) && player->GetQuestStatus(questId) != QUEST_STATUS_COMPLETE
                        && onOtherMap(questId))
                        drops.push_back(quest);
        for (Quest const* quest : drops)
            DropTask(player, quest);
    }

    void OnLogout(Player* player) override
    {
        std::scoped_lock lock(StateLock);
        States.erase(player->GetGUID());
        SavedProgress.erase(player->GetGUID());
        Owned.erase(player->GetGUID());
    }
};

class task_area_pickup_world : public WorldScript
{
public:
    task_area_pickup_world() : WorldScript("task_area_pickup_world") { }

    void OnConfigLoad(bool /*reload*/) override
    {
        Config.Enable = sConfigMgr->GetBoolDefault("TaskAreaPickup.Enable", false);
        Config.AutoReward = sConfigMgr->GetBoolDefault("TaskAreaPickup.AutoReward", true);
        Config.AddMargin = sConfigMgr->GetFloatDefault("TaskAreaPickup.AddMargin", 45.0f);
        Config.DropMargin = std::max(Config.AddMargin.load(), sConfigMgr->GetFloatDefault("TaskAreaPickup.DropMargin", 110.0f));
        Config.PointAddRadius = sConfigMgr->GetFloatDefault("TaskAreaPickup.PointAddRadius", 95.0f);
        Config.PointDropRadius = std::max(Config.PointAddRadius.load(), sConfigMgr->GetFloatDefault("TaskAreaPickup.PointDropRadius", 200.0f));
        Config.Interval = std::max<uint32>(250, sConfigMgr->GetIntDefault("TaskAreaPickup.Interval", 1000));

        auto readIds = [](char const* key)
        {
            std::unordered_set<uint32> ids;
            std::string list = sConfigMgr->GetStringDefault(key, "");
            for (std::string_view part : Trinity::Tokenize(list, ',', false))
            {
                while (!part.empty() && part.front() == ' ')
                    part.remove_prefix(1);
                while (!part.empty() && part.back() == ' ')
                    part.remove_suffix(1);
                if (Optional<uint32> id = Trinity::StringTo<uint32>(part))
                    ids.insert(*id);
            }
            return ids;
        };
        std::unordered_set<uint32> accounts = readIds("TaskAreaPickup.SocketlessAccounts");
        std::unordered_set<uint32> excluded = readIds("TaskAreaPickup.ExcludeQuests");

        std::scoped_lock lock(Config.AccountLock);
        Config.SocketlessAccounts = std::move(accounts);
        Config.ExcludeQuests = std::move(excluded);
    }
};

void BuildIndex()
{
    if (QueryResult result = WorldDatabase.Query("SELECT DISTINCT QuestID FROM quest_scheduled_gate"))
        do
            GatedQuests.insert(result->Fetch()[0].GetUInt32());
        while (result->NextRow());

    for (auto const& [entry, questId] : *sObjectMgr->GetCreatureQuestRelationMapHACK())
        StarterQuests.insert(questId);
    for (auto const& [entry, questId] : *sObjectMgr->GetGOQuestRelationMapHACK())
        StarterQuests.insert(questId);

    std::size_t blobs = 0;
    for (auto const& [questId, quest] : sObjectMgr->GetQuestTemplates())
    {
        if (quest->GetQuestType() != QUEST_TYPE_TASK || StarterQuests.contains(questId))
            continue;

        // retail's own pickup area: used instead of the outline when the quest has one
        if (uint32 groupId = quest->GetAreaGroupID())
        {
            GroupTask group;
            for (uint32 areaId : sDB2Manager.GetAreasForGroup(groupId))
                if (AreaTableEntry const* area = sAreaTableStore.LookupEntry(areaId))
                {
                    group.Areas.insert(areaId);
                    group.Maps.insert(area->ContinentID);
                }
            if (!group.Areas.empty())
            {
                for (uint32 areaId : group.Areas)
                    GroupQuestsByArea[areaId].push_back(questId);
                ScanMaps.insert(group.Maps.begin(), group.Maps.end());
                GroupByQuest[questId] = std::move(group);
                continue;
            }
        }

        QuestPOIData const* poi = sObjectMgr->GetQuestPOIData(questId);
        if (!poi)
            continue;

        // outlines (3+ points) when the quest has any, else its single points (world bosses, rares)
        bool hasOutline = std::ranges::any_of(poi->Blobs, [](QuestPOIBlobData const& data) { return data.Points.size() >= 3; });
        std::unordered_map<uint32, TaskArea> byMap;
        for (QuestPOIBlobData const& data : poi->Blobs)
        {
            if (data.Points.empty() || (hasOutline && data.Points.size() < 3))
                continue;
            Blob blob;
            blob.ConditionId = data.PlayerConditionID;
            blob.MinX = blob.MinY = std::numeric_limits<float>::max();
            blob.MaxX = blob.MaxY = std::numeric_limits<float>::lowest();
            for (QuestPOIBlobPoint const& point : data.Points)
            {
                blob.Points.emplace_back(float(point.X), float(point.Y));
                blob.MinX = std::min(blob.MinX, float(point.X));
                blob.MinY = std::min(blob.MinY, float(point.Y));
                blob.MaxX = std::max(blob.MaxX, float(point.X));
                blob.MaxY = std::max(blob.MaxY, float(point.Y));
            }
            TaskArea& area = byMap[data.MapID];
            if (area.Blobs.empty())
            {
                area.QuestId = questId;
                area.Point = !hasOutline;
                area.MinX = blob.MinX; area.MinY = blob.MinY; area.MaxX = blob.MaxX; area.MaxY = blob.MaxY;
            }
            area.MinX = std::min(area.MinX, blob.MinX);
            area.MinY = std::min(area.MinY, blob.MinY);
            area.MaxX = std::max(area.MaxX, blob.MaxX);
            area.MaxY = std::max(area.MaxY, blob.MaxY);
            area.Blobs.push_back(std::move(blob));
        }

        // a quest's outlines on more than one map: keep the map with the most blobs
        TaskArea* pick = nullptr;
        uint32 pickMap = 0;
        for (auto& [mapId, area] : byMap)
            if (!pick || area.Blobs.size() > pick->Blobs.size())
                pick = &area, pickMap = mapId;
        if (!pick)
            continue;

        blobs += pick->Blobs.size();
        std::vector<TaskArea>& list = AreasByMap[pickMap];
        AreaByQuest[questId] = { pickMap, list.size() };
        list.push_back(std::move(*pick));
        ScanMaps.insert(pickMap);
    }

    TC_LOG_INFO("server.loading", ">> TaskAreaPickup: {} task quests with area groups ({} areas), {} with outlines ({} blobs), "
        "{} maps, {} gated quests", GroupByQuest.size(), GroupQuestsByArea.size(), AreaByQuest.size(), blobs, ScanMaps.size(),
        GatedQuests.size());
}
}

void AddSC_task_area_pickup()
{
    BuildIndex();

    for (uint32 mapId : ScanMaps)
    {
        MapEntry const* entry = sMapStore.LookupEntry(mapId);
        if (!entry || !entry->IsWorldMap())
            continue;
        ScriptNames.push_back(Trinity::StringFormat("task_area_pickup_map_{}", mapId));
        new task_area_pickup_map(ScriptNames.back().c_str(), mapId);
    }

    new task_area_pickup_player();
    new task_area_pickup_world();
}
