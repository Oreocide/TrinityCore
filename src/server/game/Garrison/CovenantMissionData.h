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

#ifndef TRINITYCORE_COVENANT_MISSION_DATA_H
#define TRINITYCORE_COVENANT_MISSION_DATA_H

#include "Define.h"
#include "Optional.h"
#include <unordered_map>
#include <vector>

/*
 * World data of the covenant command table missions (CovenantGarrison.Missions.Enable). GarrMission.db2 carries no rewards and no offer rules,
 * so they come from the world tables covenant_mission_template, covenant_mission_reward, covenant_mission_chain and covenant_follower_template
 * (rows from the nov18c capture, see analysis/core_design/m5/world_m5.sql). Nothing is read while the key is off.
 */
struct CovenantMissionReward
{
    int32 ItemID = 0;
    uint32 ItemQuantity = 0;
    int32 CurrencyID = 0;
    uint32 CurrencyQuantity = 0;
    uint32 FollowerXP = 0;
    uint32 BonusAbilityID = 0;
    int32 ItemFileDataID = 0;
    Optional<int32> ItemContext;                // set = the reward ItemInstance carries an ItemBonus block
    std::vector<int32> ItemBonusListIDs;
};

struct CovenantMissionTemplate
{
    float Scalar = 1.0f;
    uint32 StartFlags = 0;
    std::vector<CovenantMissionReward> Rewards;
};

struct CovenantFollowerTemplate
{
    bool IsTroop = false;
    uint8 Quality = 0;
    uint32 ItemLevelWeapon = 0;
    uint32 ItemLevelArmor = 0;
    uint32 Durability = 0;
    int32 HealthLevel1 = 0;
    int32 HealthPerLevel = 0;

    int32 GetMaxHealth(uint32 level) const { return HealthLevel1 + int32(level > 0 ? level - 1 : 0) * HealthPerLevel; }
};

namespace CovenantMissionData
{
    constexpr uint32 DEFAULT_TROOP_LEVEL = 1;

    // CovenantGarrison.Missions.Enable together with Covenant.Enable and CovenantGarrison.Enable
    TC_GAME_API bool IsEnabled();

    // reads the world tables (no-op while IsEnabled() is false)
    TC_GAME_API void Load();

    TC_GAME_API CovenantMissionTemplate const* GetMissionTemplate(uint32 missionId);
    TC_GAME_API CovenantFollowerTemplate const* GetFollowerTemplate(uint32 garrFollowerId);
    // missions offered when prevMissionId is rewarded (0 = the missions offered first), in OrderIndex order
    TC_GAME_API std::vector<uint32> const* GetChain(uint32 prevMissionId);
}

#endif // TRINITYCORE_COVENANT_MISSION_DATA_H
