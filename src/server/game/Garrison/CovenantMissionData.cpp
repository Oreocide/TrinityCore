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

#include "CovenantMissionData.h"
#include "DB2Stores.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "Timer.h"
#include "StringConvert.h"
#include "Util.h"
#include "World.h"
#include <map>

namespace CovenantMissionData
{
namespace
{
    std::unordered_map<uint32, CovenantMissionTemplate> _missionTemplates;
    std::unordered_map<uint32, CovenantFollowerTemplate> _followerTemplates;
    std::unordered_map<uint32, std::vector<uint32>> _chains;
}

bool IsEnabled()
{
    return sWorld->getBoolConfig(CONFIG_COVENANT_ENABLE) && sWorld->getBoolConfig(CONFIG_COVENANT_GARRISON_ENABLE)
        && sWorld->getBoolConfig(CONFIG_COVENANT_GARRISON_MISSIONS_ENABLE);
}

void Load()
{
    if (!IsEnabled())
        return;

    uint32 oldMSTime = getMSTime();

    _missionTemplates.clear();
    _followerTemplates.clear();
    _chains.clear();

    if (QueryResult result = WorldDatabase.Query("SELECT MissionID, Scalar, StartFlags FROM covenant_mission_template"))
    {
        do
        {
            Field* fields = result->Fetch();
            uint32 missionId = fields[0].GetUInt32();
            if (!sGarrMissionStore.LookupEntry(missionId))
            {
                TC_LOG_ERROR("sql.sql", "Table `covenant_mission_template` has an unknown GarrMission {}, skipped", missionId);
                continue;
            }

            CovenantMissionTemplate& missionTemplate = _missionTemplates[missionId];
            missionTemplate.Scalar = fields[1].GetFloat();
            missionTemplate.StartFlags = fields[2].GetUInt32();
        }
        while (result->NextRow());
    }

    if (QueryResult result = WorldDatabase.Query("SELECT MissionID, ItemID, ItemQuantity, CurrencyID, CurrencyQuantity, FollowerXP, BonusAbilityID, ItemFileDataID, ItemContext, ItemBonusListIDs "
        "FROM covenant_mission_reward ORDER BY MissionID, OrderIndex"))
    {
        do
        {
            Field* fields = result->Fetch();
            auto itr = _missionTemplates.find(fields[0].GetUInt32());
            if (itr == _missionTemplates.end())
            {
                TC_LOG_ERROR("sql.sql", "Table `covenant_mission_reward` has a reward for mission {} without a `covenant_mission_template` row, skipped", fields[0].GetUInt32());
                continue;
            }

            CovenantMissionReward& reward = itr->second.Rewards.emplace_back();
            reward.ItemID = fields[1].GetInt32();
            reward.ItemQuantity = fields[2].GetUInt32();
            reward.CurrencyID = fields[3].GetInt32();
            reward.CurrencyQuantity = fields[4].GetUInt32();
            reward.FollowerXP = fields[5].GetUInt32();
            reward.BonusAbilityID = fields[6].GetUInt32();
            reward.ItemFileDataID = fields[7].GetInt32();
            if (!fields[8].IsNull())
                reward.ItemContext = fields[8].GetInt32();

            for (std::string_view token : Trinity::Tokenize(fields[9].GetStringView(), ' ', false))
                if (Optional<int32> bonusListId = Trinity::StringTo<int32>(token))
                    reward.ItemBonusListIDs.push_back(*bonusListId);
        }
        while (result->NextRow());
    }

    if (QueryResult result = WorldDatabase.Query("SELECT PrevMissionID, NextMissionID FROM covenant_mission_chain ORDER BY PrevMissionID, OrderIndex"))
    {
        do
        {
            Field* fields = result->Fetch();
            uint32 nextMissionId = fields[1].GetUInt32();
            if (!_missionTemplates.contains(nextMissionId))
            {
                TC_LOG_ERROR("sql.sql", "Table `covenant_mission_chain` offers mission {} that has no `covenant_mission_template` row, skipped", nextMissionId);
                continue;
            }

            _chains[fields[0].GetUInt32()].push_back(nextMissionId);
        }
        while (result->NextRow());
    }

    if (QueryResult result = WorldDatabase.Query("SELECT GarrFollowerID, IsTroop, Quality, ItemLevelWeapon, ItemLevelArmor, Durability, HealthLevel1, HealthPerLevel FROM covenant_follower_template"))
    {
        do
        {
            Field* fields = result->Fetch();
            uint32 garrFollowerId = fields[0].GetUInt32();
            if (!sGarrFollowerStore.LookupEntry(garrFollowerId))
            {
                TC_LOG_ERROR("sql.sql", "Table `covenant_follower_template` has an unknown GarrFollower {}, skipped", garrFollowerId);
                continue;
            }

            CovenantFollowerTemplate& followerTemplate = _followerTemplates[garrFollowerId];
            followerTemplate.IsTroop = fields[1].GetUInt8() != 0;
            followerTemplate.Quality = fields[2].GetUInt8();
            followerTemplate.ItemLevelWeapon = fields[3].GetUInt32();
            followerTemplate.ItemLevelArmor = fields[4].GetUInt32();
            followerTemplate.Durability = fields[5].GetUInt32();
            followerTemplate.HealthLevel1 = fields[6].GetInt32();
            followerTemplate.HealthPerLevel = fields[7].GetInt32();
        }
        while (result->NextRow());
    }

    TC_LOG_INFO("server.loading", ">> Loaded {} covenant mission templates, {} follower templates and {} offer chains in {} ms",
        _missionTemplates.size(), _followerTemplates.size(), _chains.size(), GetMSTimeDiffToNow(oldMSTime));
}

CovenantMissionTemplate const* GetMissionTemplate(uint32 missionId)
{
    auto itr = _missionTemplates.find(missionId);
    return itr != _missionTemplates.end() ? &itr->second : nullptr;
}

CovenantFollowerTemplate const* GetFollowerTemplate(uint32 garrFollowerId)
{
    auto itr = _followerTemplates.find(garrFollowerId);
    return itr != _followerTemplates.end() ? &itr->second : nullptr;
}

std::vector<uint32> const* GetChain(uint32 prevMissionId)
{
    auto itr = _chains.find(prevMissionId);
    return itr != _chains.end() ? &itr->second : nullptr;
}
}
