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

#include "CovenantGarrison.h"
#include "AutoCombat.h"
#include "CovenantMissionData.h"
#include "ConditionMgr.h"
#include "DB2Stores.h"
#include "DB2Structure.h"
#include "GameTime.h"
#include "Garrison.h"
#include "GarrisonPackets.h"
#include "Log.h"
#include "Player.h"
#include "Util.h"
#include "World.h"
#include <algorithm>
#include <map>
#include <set>

/*
 * Command table missions of the covenant garrison (type 111), M5. Layouts and orders come from the nov18c capture
 * (analysis/core_design/m5/PACKETS_nov18c_raw.md). The auto-combat (M6, CovenantGarrison.Missions.AutoCombat.Enable) answers CMSG_GARRISON_COMPLETE_MISSION with
 * SMSG_GARRISON_COMPLETE_MISSION_RESULT only for fights whose every rule the nov18c capture pins (AutoCombat.h); any other mission is completed server side
 * and the client gets no completion reply (or the AutoRoll path).
 */


namespace
{
    // StartTime of a mission that was not started, as retail sends it (01/01/0001)
    constexpr time_t MISSION_NOT_STARTED = -62169984000LL;

    // spells of the auto-combat as the engine wants them (GarrAutoSpell + GarrAutoSpellEffect), built once
    std::map<int32, AutoCombat::Spell> const& GetAutoCombatSpells()
    {
        static std::map<int32, AutoCombat::Spell> const spells = []
        {
            std::map<int32, AutoCombat::Spell> result;
            for (GarrAutoSpellEntry const* entry : sGarrAutoSpellStore)
            {
                AutoCombat::Spell& spell = result[int32(entry->ID)];
                spell.Id = int32(entry->ID);
                spell.Cooldown = entry->Cooldown;
                spell.Duration = entry->Duration;
                spell.SchoolMask = uint32(entry->SchoolMask);
            }

            for (GarrAutoSpellEffectEntry const* entry : sGarrAutoSpellEffectStore)
            {
                auto itr = result.find(int32(entry->SpellID));
                if (itr == result.end())
                    continue;

                AutoCombat::SpellEffect& effect = itr->second.Effects.emplace_back();
                effect.EffectIndex = entry->EffectIndex;
                effect.Effect = entry->Effect;
                effect.Points = entry->Points;
                effect.TargetType = entry->TargetType;
            }

            for (auto& [id, spell] : result)
                std::sort(spell.Effects.begin(), spell.Effects.end(), [](AutoCombat::SpellEffect const& a, AutoCombat::SpellEffect const& b) { return a.EffectIndex < b.EffectIndex; });

            return result;
        }();

        return spells;
    }

    bool MakeCombatant(GarrAutoCombatantEntry const* combatant, int board, uint32 level, int32 health, AutoCombat::Combatant& out)
    {
        if (!combatant)
            return false;

        out.Board = int8(board);
        out.Role = uint8(combatant->Role);
        out.Attack = combatant->AttackBase + combatant->AttackGainPerLevel * int32(level);
        out.MaxHealth = combatant->HealthBase + combatant->HealthGainPerLevel * int32(level);
        out.Health = health < 0 ? out.MaxHealth : health;
        out.AttackSpell = combatant->AttackSpellID;
        for (int32 ability : { combatant->AbilitySpellID, combatant->AbilitySpellID2 })
            if (ability)
                out.Abilities.push_back(ability);
        return true;
    }
}

bool CovenantGarrison::AreMissionsEnabled()
{
    return CovenantMissionData::IsEnabled();
}

WorldPackets::Garrison::GarrisonFollower const* CovenantGarrison::GetFollower(uint64 dbId) const
{
    auto itr = _followers.find(dbId);
    return itr != _followers.end() ? &itr->second : nullptr;
}

CovenantGarrison::Mission* CovenantGarrison::GetMission(int32 missionRecId)
{
    for (auto& [dbId, mission] : _missions)
        if (mission.PacketInfo.MissionRecID == missionRecId)
            return &mission;

    return nullptr;
}

CovenantGarrison::Mission const* CovenantGarrison::GetMission(int32 missionRecId) const
{
    for (auto const& [dbId, mission] : _missions)
        if (mission.PacketInfo.MissionRecID == missionRecId)
            return &mission;

    return nullptr;
}

bool CovenantGarrison::HasMission(uint32 missionId) const
{
    return GetMission(int32(missionId)) != nullptr;
}

uint32 CovenantGarrison::GetMinHeroLevel() const
{
    uint32 level = 0;
    for (auto const& [dbId, follower] : _followers)
        level = level ? std::min(level, follower.FollowerLevel) : follower.FollowerLevel;

    return level ? level : CovenantMissionData::DEFAULT_TROOP_LEVEL;
}

uint32 CovenantGarrison::GetMaxHeroLevel() const
{
    uint32 level = 0;
    for (auto const& [dbId, follower] : _followers)
        level = std::max(level, follower.FollowerLevel);

    return level ? level : CovenantMissionData::DEFAULT_TROOP_LEVEL;
}

uint32 CovenantGarrison::GetXpToNextLevel(uint32 level)
{
    for (GarrFollowerLevelXPEntry const* entry : sGarrFollowerLevelXPStore)
        if (entry->GarrFollowerTypeID == FOLLOWER_TYPE_COVENANT_SANCTUM && entry->FollowerLevel == level)
            return entry->XpToNextLevel;

    return 0;
}

bool CovenantGarrison::CanStartMission(Mission const& mission) const
{
    // the capture lists every mission (offered or running) as startable: the bit is the mission's PlayerCondition, not the cost
    GarrMissionEntry const* missionEntry = sGarrMissionStore.LookupEntry(mission.PacketInfo.MissionRecID);
    return missionEntry && ConditionMgr::IsPlayerMeetingCondition(_owner, missionEntry->PlayerConditionID);
}

void CovenantGarrison::FillMissionReward(CovenantMissionReward const& reward, WorldPackets::Garrison::GarrisonMissionReward& packetReward) const
{
    packetReward.ItemID = reward.ItemID;
    packetReward.ItemQuantity = reward.ItemQuantity;
    packetReward.CurrencyID = reward.CurrencyID;
    packetReward.CurrencyQuantity = reward.CurrencyQuantity;
    packetReward.FollowerXP = reward.FollowerXP;
    packetReward.GarrMssnBonusAbilityID = reward.BonusAbilityID;
    packetReward.ItemFileDataID = reward.ItemFileDataID;
    if (reward.ItemID)
    {
        packetReward.ItemInstance.emplace();
        packetReward.ItemInstance->ItemID = reward.ItemID;
        if (reward.ItemContext)
        {
            packetReward.ItemInstance->ItemBonus.emplace();
            packetReward.ItemInstance->ItemBonus->Context = ItemContext(*reward.ItemContext);
            packetReward.ItemInstance->ItemBonus->BonusListIDs = reward.ItemBonusListIDs;
        }
    }
}

bool CovenantGarrison::AddFollower(uint32 garrFollowerId)
{
    if (!AreMissionsEnabled())
        return false;

    GarrFollowerEntry const* followerEntry = sGarrFollowerStore.LookupEntry(garrFollowerId);
    if (!followerEntry || followerEntry->GarrTypeID != int8(GARR_TYPE_ID))
        return false;

    CovenantFollowerTemplate const* followerTemplate = CovenantMissionData::GetFollowerTemplate(garrFollowerId);
    if (!followerTemplate || followerTemplate->IsTroop)
    {
        TC_LOG_WARN("entities.player", "CovenantGarrison::AddFollower: player {} cannot get follower {}: no hero row in covenant_follower_template (health data comes from the capture only)",
            _owner->GetGUID().ToString(), garrFollowerId);
        return false;
    }

    WorldPackets::Garrison::GarrisonAddFollowerResult addFollowerResult;
    addFollowerResult.GarrTypeID = GARR_TYPE_ID;

    for (auto const& [dbId, follower] : _followers)
    {
        if (follower.GarrFollowerID == garrFollowerId)
        {
            addFollowerResult.Result = GARRISON_ERROR_FOLLOWER_EXISTS;
            _owner->SendDirectMessage(addFollowerResult.Write());
            return false;
        }
    }

    uint64 dbId = GenerateDbId();
    WorldPackets::Garrison::GarrisonFollower& follower = _followers[dbId];
    follower.DbID = dbId;
    follower.GarrFollowerID = garrFollowerId;
    follower.Quality = followerTemplate->Quality;
    follower.FollowerLevel = 1;
    follower.ItemLevelWeapon = followerTemplate->ItemLevelWeapon;
    follower.ItemLevelArmor = followerTemplate->ItemLevelArmor;
    follower.Xp = 0;
    follower.Durability = followerTemplate->Durability;
    follower.Health = followerTemplate->GetMaxHealth(1);
    follower.HealingTimestamp = GameTime::GetGameTime();    // idx 182406: the add time
    follower.BoardIndex = -1;

    _missionsNeedSave = true;

    addFollowerResult.Follower = follower;
    _owner->SendDirectMessage(addFollowerResult.Write());

    _owner->UpdateCriteria(CriteriaType::RecruitGarrisonFollower, garrFollowerId);
    return true;
}

CovenantGarrison::Mission* CovenantGarrison::OfferMission(uint32 missionId, bool send)
{
    GarrMissionEntry const* missionEntry = sGarrMissionStore.LookupEntry(missionId);
    CovenantMissionTemplate const* missionTemplate = CovenantMissionData::GetMissionTemplate(missionId);
    if (!missionEntry || !missionTemplate || missionEntry->GarrTypeID != int8(GARR_TYPE_ID))
        return nullptr;

    uint64 dbId = GenerateDbId();
    Mission& mission = _missions[dbId];
    mission.PacketInfo.DbID = dbId;
    mission.PacketInfo.MissionRecID = int32(missionId);
    mission.PacketInfo.OfferTime = GameTime::GetGameTime();
    mission.PacketInfo.OfferDuration = Seconds(missionEntry->OfferDuration);
    mission.PacketInfo.StartTime = MISSION_NOT_STARTED;
    mission.PacketInfo.MissionState = MISSION_STATE_OFFERED;
    mission.PacketInfo.MissionScalar = missionTemplate->Scalar;
    _missionsNeedSave = true;

    if (send)
    {
        WorldPackets::Garrison::GarrisonAddMissionResult addMissionResult;
        addMissionResult.GarrTypeID = GARR_TYPE_ID;
        addMissionResult.Result = 0;
        addMissionResult.State = 0;
        addMissionResult.Mission = mission.PacketInfo;
        for (CovenantMissionReward const& reward : missionTemplate->Rewards)
            FillMissionReward(reward, addMissionResult.Mission.Rewards.emplace_back());

        addMissionResult.CanStart = CanStartMission(mission);
        _owner->SendDirectMessage(addMissionResult.Write());
    }

    return &mission;
}

void CovenantGarrison::OfferChain(uint32 prevMissionId, bool send)
{
    std::vector<uint32> const* chain = CovenantMissionData::GetChain(prevMissionId);
    if (!chain)
        return;

    for (uint32 nextMissionId : *chain)
        if (!HasMission(nextMissionId))
            OfferMission(nextMissionId, send);
}

void CovenantGarrison::OfferInitialMissions(bool send)
{
    // the capture offers 2174 at login (idx 11934, OfferTime = the login second); what gates it in retail is not visible
    if (AreMissionsEnabled() && _owner->GetCovenant() > 0)
        OfferChain(0, send);
}

bool CovenantGarrison::RemoveExpiredMissions()
{
    time_t now = GameTime::GetGameTime();
    bool removed = false;
    for (auto itr = _missions.begin(); itr != _missions.end();)
    {
        WorldPackets::Garrison::GarrisonMission const& info = itr->second.PacketInfo;
        if (info.MissionState == MISSION_STATE_OFFERED && Seconds(info.OfferDuration) > Seconds::zero()
            && now >= time_t(info.OfferTime) + Seconds(info.OfferDuration).count())
        {
            itr = _missions.erase(itr);
            removed = true;
        }
        else
            ++itr;
    }

    if (removed)
        _missionsNeedSave = true;

    return removed;
}

void CovenantGarrison::SendStartConditionUpdate() const
{
    WorldPackets::Garrison::GarrisonMissionStartConditionUpdate update;
    for (auto const& [dbId, mission] : _missions)
        if (mission.PacketInfo.MissionState != MISSION_STATE_REWARDED)
            update.Missions.emplace_back(mission.PacketInfo.MissionRecID, CanStartMission(mission));

    if (!update.Missions.empty())
        _owner->SendDirectMessage(update.Write());
}

void CovenantGarrison::SendStartMissionError(int32 error) const
{
    // the capture only has successful starts: an error reuses the same body with empty lists (not observed in retail)
    WorldPackets::Garrison::GarrisonStartMissionResult result;
    result.Result = error;
    result.SessionMissionCount = _sessionMissionCount;
    _owner->SendDirectMessage(result.Write());
}

void CovenantGarrison::StartMission(WorldPackets::Garrison::GarrisonStartMission const& startMission)
{
    if (!AreMissionsEnabled())
        return;

    if (RemoveExpiredMissions())
        SendInfo();

    GarrMissionEntry const* missionEntry = sGarrMissionStore.LookupEntry(startMission.MissionRecID);
    Mission* mission = GetMission(startMission.MissionRecID);
    CovenantMissionTemplate const* missionTemplate = CovenantMissionData::GetMissionTemplate(startMission.MissionRecID);
    if (!missionEntry || !mission || !missionTemplate)
    {
        SendStartMissionError(GARRISON_ERROR_INVALID_MISSION);
        return;
    }

    if (mission->PacketInfo.MissionState != MISSION_STATE_OFFERED)
    {
        SendStartMissionError(mission->PacketInfo.MissionState == MISSION_STATE_IN_PROGRESS ? GARRISON_ERROR_ALREADY_ON_MISSION : GARRISON_ERROR_MISSION_NOT_OFFERED);
        return;
    }

    if (startMission.Followers.empty() || startMission.Followers.size() > missionEntry->MaxFollowers)
    {
        SendStartMissionError(GARRISON_ERROR_MISSION_SIZE_INVALID);
        return;
    }

    if (!ConditionMgr::IsPlayerMeetingCondition(_owner, missionEntry->PlayerConditionID))
    {
        SendStartMissionError(GARRISON_ERROR_MISSION_START_CONDITION_FAILED);
        return;
    }

    if (missionEntry->MissionCost && !_owner->HasCurrency(missionEntry->MissionCostCurrencyTypesID, missionEntry->MissionCost))
    {
        SendStartMissionError(GARRISON_ERROR_NOT_ENOUGH_CURRENCY);
        return;
    }

    // validate every follower before anything changes; the client-sent Health is ignored (the server value is echoed)
    uint32 troopLevel = GetMinHeroLevel();
    std::vector<MissionFollower> participants;
    std::set<uint64> usedHeroes;
    std::set<int32> usedBoards;
    std::vector<WorldPackets::Garrison::GarrisonFollower> troops;
    uint64 firstTroopDbId = _lastDbId;
    for (WorldPackets::Garrison::GarrisonMissionFollowerInfo const& info : startMission.Followers)
    {
        if (info.BoardIndex < 0 || info.BoardIndex >= int32(missionEntry->MaxFollowers) || !usedBoards.insert(info.BoardIndex).second)
        {
            _lastDbId = firstTroopDbId;
            SendStartMissionError(GARRISON_ERROR_INVALID_FOLLOWER);
            return;
        }

        MissionFollower& participant = participants.emplace_back();
        participant.BoardIndex = info.BoardIndex;
        if (info.DatabaseID)
        {
            auto itr = _followers.find(info.DatabaseID);
            if (itr == _followers.end() || !usedHeroes.insert(info.DatabaseID).second)
            {
                _lastDbId = firstTroopDbId;
                SendStartMissionError(GARRISON_ERROR_INVALID_FOLLOWER);
                return;
            }

            if (itr->second.CurrentMissionID)
            {
                _lastDbId = firstTroopDbId;
                SendStartMissionError(GARRISON_ERROR_FOLLOWER_ALREADY_ON_MISSION);
                return;
            }

            participant.DbID = info.DatabaseID;
            participant.GarrFollowerID = itr->second.GarrFollowerID;
            participant.Level = itr->second.FollowerLevel;
            participant.Health = itr->second.Health;
            if (info.Health != participant.Health)
                TC_LOG_DEBUG("entities.player", "CovenantGarrison::StartMission: player {} sent health {} for follower {}, the server has {}",
                    _owner->GetGUID().ToString(), info.Health, info.DatabaseID, participant.Health);
        }
        else
        {
            CovenantFollowerTemplate const* followerTemplate = info.FollowerEntry ? CovenantMissionData::GetFollowerTemplate(*info.FollowerEntry) : nullptr;
            if (!followerTemplate || !followerTemplate->IsTroop)
            {
                _lastDbId = firstTroopDbId;
                SendStartMissionError(GARRISON_ERROR_INVALID_FOLLOWER);
                return;
            }

            participant.DbID = GenerateDbId();
            participant.GarrFollowerID = *info.FollowerEntry;
            participant.IsTroop = true;
            participant.Level = troopLevel;
            participant.Health = followerTemplate->GetMaxHealth(troopLevel);

            WorldPackets::Garrison::GarrisonFollower& troop = troops.emplace_back();
            troop.DbID = participant.DbID;
            troop.GarrFollowerID = participant.GarrFollowerID;
            troop.Quality = followerTemplate->Quality;
            troop.FollowerLevel = troopLevel;
            troop.ItemLevelWeapon = followerTemplate->ItemLevelWeapon;
            troop.ItemLevelArmor = followerTemplate->ItemLevelArmor;
            troop.Durability = followerTemplate->Durability;
            troop.CurrentMissionID = startMission.MissionRecID;
            troop.FollowerStatus = FOLLOWER_STATUS_ON_MISSION;
            troop.Health = participant.Health;
            troop.BoardIndex = int8(participant.BoardIndex);
        }
    }

    if (missionEntry->MissionCost)
        _owner->ModifyCurrency(missionEntry->MissionCostCurrencyTypesID, -int32(missionEntry->MissionCost), CurrencyGainSource::Cheat, CurrencyDestroyReason::Spell);

    time_t now = GameTime::GetGameTime();
    WorldPackets::Garrison::GarrisonMission& info = mission->PacketInfo;
    info.StartTime = now;
    info.TravelDuration = Seconds(missionEntry->TravelDuration);
    info.MissionDuration = Seconds(missionEntry->MissionDuration);
    info.MissionState = MISSION_STATE_IN_PROGRESS;
    info.SuccessChance = 0;
    info.Flags = missionTemplate->StartFlags;
    mission->Followers = std::move(participants);

    for (MissionFollower const& participant : mission->Followers)
    {
        if (participant.IsTroop)
            continue;

        WorldPackets::Garrison::GarrisonFollower& hero = _followers[participant.DbID];
        hero.CurrentMissionID = startMission.MissionRecID;
        hero.BoardIndex = int8(participant.BoardIndex);
    }

    _missionsNeedSave = true;

    WorldPackets::Garrison::GarrisonStartMissionResult result;
    result.Result = 0;
    result.SessionMissionCount = ++_sessionMissionCount;
    result.Mission = info;
    for (MissionFollower const& participant : mission->Followers)
    {
        WorldPackets::Garrison::GarrisonMissionFollowerInfo& followerInfo = result.Followers.emplace_back();
        followerInfo.DatabaseID = participant.DbID;
        followerInfo.BoardIndex = participant.BoardIndex;
        followerInfo.Health = participant.Health;
        if (participant.IsTroop)
            followerInfo.FollowerEntry = int32(participant.GarrFollowerID);
    }
    result.Troops = std::move(troops);
    _owner->SendDirectMessage(result.Write());

    _owner->UpdateCriteria(CriteriaType::StartGarrisonMission, startMission.MissionRecID);
    _owner->UpdateCriteria(CriteriaType::StartAnyGarrisonMissionWithFollowerType, FOLLOWER_TYPE_COVENANT_SANCTUM);
    SendStartConditionUpdate();
}

bool CovenantGarrison::RunAutoCombat(Mission& mission, int32 missionRecId)
{
    GarrMissionEntry const* missionEntry = sGarrMissionStore.LookupEntry(missionRecId);
    if (!missionEntry)
        return false;

    // what the captures pin about the enemy side: AutoMissionScalar 1.0 without a curve (enemy level 1), no environment effect
    if (missionEntry->AutoMissionScalar != 1.0f || missionEntry->AutoMissionScalarCurveID || missionEntry->AutoCombatantEnvCasterID || missionEntry->EnvGarrMechanicID)
    {
        TC_LOG_DEBUG("entities.player", "CovenantGarrison::RunAutoCombat: mission {} is outside what the captures pin (scalar {}, curve {}, env caster {}, env mechanic {})",
            missionRecId, missionEntry->AutoMissionScalar, missionEntry->AutoMissionScalarCurveID, missionEntry->AutoCombatantEnvCasterID, missionEntry->EnvGarrMechanicID);
        return false;
    }

    std::vector<AutoCombat::Combatant> combatants;
    for (MissionFollower const& participant : mission.Followers)
    {
        GarrFollowerEntry const* followerEntry = sGarrFollowerStore.LookupEntry(participant.GarrFollowerID);
        GarrAutoCombatantEntry const* combatant = followerEntry ? sGarrAutoCombatantStore.LookupEntry(followerEntry->AutoCombatantID) : nullptr;
        AutoCombat::Combatant& unit = combatants.emplace_back();
        if (!MakeCombatant(combatant, participant.BoardIndex, participant.Level, participant.Health, unit) || participant.BoardIndex < 0 || participant.BoardIndex > 4 || combatant->PassiveSpellID)
        {
            TC_LOG_DEBUG("entities.player", "CovenantGarrison::RunAutoCombat: follower {} of mission {} has no usable combatant (passive spells are not pinned)", participant.GarrFollowerID, missionRecId);
            return false;
        }
    }

    for (GarrMissionXEncounterEntry const* encounterRow : sGarrMissionXEncounterStore)
    {
        if (encounterRow->GarrMissionID != uint32(missionRecId))
            continue;

        GarrEncounterEntry const* encounter = sGarrEncounterStore.LookupEntry(encounterRow->GarrEncounterID);
        GarrAutoCombatantEntry const* combatant = encounter ? sGarrAutoCombatantStore.LookupEntry(encounter->AutoCombatantID) : nullptr;
        AutoCombat::Combatant& unit = combatants.emplace_back();
        if (!MakeCombatant(combatant, encounterRow->BoardIndex, 1, -1, unit) || encounterRow->BoardIndex < 5 || encounterRow->BoardIndex > 12 || combatant->PassiveSpellID)
        {
            TC_LOG_DEBUG("entities.player", "CovenantGarrison::RunAutoCombat: encounter {} of mission {} has no usable combatant", encounterRow->GarrEncounterID, missionRecId);
            return false;
        }
    }

    std::map<int32, AutoCombat::Spell> const& spells = GetAutoCombatSpells();
    AutoCombat::Result combat = AutoCombat::Run(std::move(combatants), [&spells](int32 spellId) -> AutoCombat::Spell const*
    {
        auto itr = spells.find(spellId);
        return itr != spells.end() ? &itr->second : nullptr;
    });

    if (!combat.IsPinned())
    {
        for (std::string const& reason : combat.Unpinned)
            TC_LOG_DEBUG("entities.player", "CovenantGarrison::RunAutoCombat: mission {} not run: {}", missionRecId, reason);
        return false;
    }

    // a hero at 0 health would mean a follower death, which the captures do not show
    for (MissionFollower const& participant : mission.Followers)
        if (!participant.IsTroop && combat.FinalHealth[participant.BoardIndex] <= 0)
        {
            TC_LOG_DEBUG("entities.player", "CovenantGarrison::RunAutoCombat: mission {} would kill hero {}: not pinned", missionRecId, participant.DbID);
            return false;
        }

    time_t now = GameTime::GetGameTime();
    WorldPackets::Garrison::GarrisonCompleteMissionResult result;
    result.Result = 0;
    result.Mission = mission.PacketInfo;
    result.MissionRecID = missionRecId;
    // the byte before the follower count is 0xE3 / 0x03 / 0xF7 in the captures and not understood: 0x03 = the two low bits all three share (human client test, COMBAT_LOG.md)
    result.UnknownByte = 0x03;
    for (MissionFollower& participant : mission.Followers)
    {
        participant.Health = combat.FinalHealth[participant.BoardIndex];
        if (!participant.IsTroop)
            _followers[participant.DbID].Health = participant.Health;

        WorldPackets::Garrison::GarrisonMissionFollowerHealth& health = result.Followers.emplace_back();
        health.DbID = participant.DbID;
        health.Health = participant.Health;
        health.HealingTimestamp = now;
    }
    std::sort(result.Followers.begin(), result.Followers.end(), [](auto const& a, auto const& b) { return a.DbID < b.DbID; });
    result.Combat = std::move(combat);
    _missionsNeedSave = true;

    WorldPacket const* packet = result.Write();
    _owner->SendDirectMessage(packet);
    TC_LOG_DEBUG("entities.player", "CovenantGarrison::RunAutoCombat: mission {} result sent ({} rounds): {}", missionRecId, result.Combat.Rounds.size(), ByteArrayToHexStr(*packet));
    return true;
}

void CovenantGarrison::CompleteMission(int32 missionRecId)
{
    if (!AreMissionsEnabled())
        return;

    Mission* mission = GetMission(missionRecId);
    if (!mission || mission->PacketInfo.MissionState != MISSION_STATE_IN_PROGRESS)
    {
        TC_LOG_DEBUG("entities.player", "CovenantGarrison::CompleteMission: player {} completes mission {} that is not in progress", _owner->GetGUID().ToString(), missionRecId);
        return;
    }

    WorldPackets::Garrison::GarrisonMission& info = mission->PacketInfo;
    time_t readyTime = time_t(info.StartTime) + Seconds(info.TravelDuration).count() + Seconds(info.MissionDuration).count();
    if (GameTime::GetGameTime() < readyTime)
    {
        TC_LOG_DEBUG("entities.player", "CovenantGarrison::CompleteMission: player {} completes mission {} {} s early", _owner->GetGUID().ToString(), missionRecId, readyTime - GameTime::GetGameTime());
        return;
    }

    info.MissionState = MISSION_STATE_COMPLETE;
    _missionsNeedSave = true;

    _owner->UpdateCriteria(CriteriaType::SucceedGarrisonMission, uint32(missionRecId));
    _owner->UpdateCriteria(CriteriaType::SucceedAnyGarrisonMissionWithFollowerType, FOLLOWER_TYPE_COVENANT_SANCTUM);
    SendStartConditionUpdate();

    // retail answers with SMSG_GARRISON_COMPLETE_MISSION_RESULT here (the auto-combat log, analysis/core_design/m6/COMBAT_LOG.md)
    if (sWorld->getBoolConfig(CONFIG_COVENANT_GARRISON_MISSIONS_AUTO_COMBAT) && RunAutoCombat(*mission, missionRecId))
        return;

    if (sWorld->getBoolConfig(CONFIG_COVENANT_GARRISON_MISSIONS_AUTO_ROLL))
    {
        // the client only asks for the bonus roll after that result: do it now so the followers are freed and the chain goes on
        MissionBonusRoll(missionRecId);
        return;
    }

    TC_LOG_WARN("entities.player", "CovenantGarrison::CompleteMission: player {} mission {}: SMSG_GARRISON_COMPLETE_MISSION_RESULT is not sent (layout not pinned), the client stays on the mission screen",
        _owner->GetGUID().ToString(), missionRecId);
}

void CovenantGarrison::GiveFollowerXp(WorldPackets::Garrison::GarrisonFollower& follower, uint32 xp, time_t completionTime)
{
    WorldPackets::Garrison::GarrisonFollowerChangedXp changedXp;
    changedXp.XPEarned = xp;
    changedXp.XPSource = 0;
    changedXp.FollowerBefore = follower;
    changedXp.FollowerBefore.HealingTimestamp = completionTime;

    uint32 level = follower.FollowerLevel;
    uint32 totalXp = follower.Xp + xp;
    while (uint32 xpToNext = GetXpToNextLevel(level))
    {
        if (totalXp < xpToNext)
            break;

        totalXp -= xpToNext;
        ++level;
    }

    if (level != follower.FollowerLevel)
    {
        // a level up refills the health (every sample of the capture)
        if (CovenantFollowerTemplate const* followerTemplate = CovenantMissionData::GetFollowerTemplate(follower.GarrFollowerID))
            follower.Health = followerTemplate->GetMaxHealth(level);
    }

    follower.FollowerLevel = level;
    follower.Xp = totalXp;
    follower.HealingTimestamp = time_t(0);
    _missionsNeedSave = true;

    changedXp.FollowerAfter = follower;
    _owner->SendDirectMessage(changedXp.Write());
}

void CovenantGarrison::GrantMissionRewards(CovenantMissionTemplate const& missionTemplate)
{
    for (CovenantMissionReward const& reward : missionTemplate.Rewards)
    {
        if (reward.ItemID && reward.ItemQuantity)
        {
            // item bonus lists of the reward are only part of the packet: the item is created plain; a full bag loses the reward (Player::AddItem messages it)
            _owner->AddItem(uint32(reward.ItemID), reward.ItemQuantity);
        }

        if (reward.CurrencyID && reward.CurrencyQuantity)
            _owner->ModifyCurrency(uint32(reward.CurrencyID), int32(reward.CurrencyQuantity), CurrencyGainSource::Cheat);

        // reward.FollowerXP: the capture does not show whether it adds to GarrMission.BaseFollowerXP, so it is not applied
    }
}

void CovenantGarrison::MissionBonusRoll(int32 missionRecId)
{
    if (!AreMissionsEnabled())
        return;

    Mission* mission = GetMission(missionRecId);
    GarrMissionEntry const* missionEntry = sGarrMissionStore.LookupEntry(missionRecId);
    CovenantMissionTemplate const* missionTemplate = CovenantMissionData::GetMissionTemplate(missionRecId);
    if (!mission || !missionEntry || !missionTemplate || mission->PacketInfo.MissionState != MISSION_STATE_COMPLETE)
    {
        TC_LOG_DEBUG("entities.player", "CovenantGarrison::MissionBonusRoll: player {} rolls mission {} that is not complete", _owner->GetGUID().ToString(), missionRecId);
        return;
    }

    WorldPackets::Garrison::GarrisonMission& info = mission->PacketInfo;
    time_t completionTime = time_t(info.StartTime) + Seconds(info.TravelDuration).count() + Seconds(info.MissionDuration).count();
    info.MissionState = MISSION_STATE_REWARDED;

    // 1. SMSG_GARRISON_MISSION_BONUS_ROLL_RESULT: every follower with its health and the time it is healed (no damage yet: health unchanged)
    WorldPackets::Garrison::GarrisonMissionBonusRollResult bonusRoll;
    bonusRoll.Mission = info;
    bonusRoll.MissionRecID = missionRecId;
    bonusRoll.Result = 0;
    std::set<uint64> heroDbIds;
    for (MissionFollower const& participant : mission->Followers)
    {
        WorldPackets::Garrison::GarrisonMissionFollowerHealth& health = bonusRoll.Followers.emplace_back();
        health.DbID = participant.DbID;
        health.HealingTimestamp = completionTime;
        if (participant.IsTroop)
        {
            health.Health = participant.Health;
            continue;
        }

        heroDbIds.insert(participant.DbID);
        WorldPackets::Garrison::GarrisonFollower& hero = _followers[participant.DbID];
        health.Health = hero.Health;
        hero.CurrentMissionID = 0;
        hero.BoardIndex = -1;
        hero.FollowerStatus = 0;
    }
    _owner->SendDirectMessage(bonusRoll.Write());

    // the troops of the mission are not kept
    mission->Followers.clear();
    _missionsNeedSave = true;

    // 2. follower XP (GarrMission.BaseFollowerXP, heroes only): the auto troop level update goes before each one
    for (uint64 heroDbId : heroDbIds)
    {
        WorldPackets::Garrison::GarrisonAutoTroopMinLevelUpdateResult troopLevel;
        troopLevel.GarrSiteID = GARR_SITE_ID;
        troopLevel.MinLevel = int32(GetMaxHeroLevel());
        _owner->SendDirectMessage(troopLevel.Write());

        GiveFollowerXp(_followers[heroDbId], missionEntry->BaseFollowerXP, completionTime);
    }

    // 3. the rewards, then the next offers
    GrantMissionRewards(*missionTemplate);
    OfferChain(uint32(missionRecId), true);
    SendStartConditionUpdate();
}
