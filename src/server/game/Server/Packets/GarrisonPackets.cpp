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

#include "GarrisonPackets.h"
#include "DB2Structure.h"
#include "Errors.h"
#include "PacketOperators.h"

namespace WorldPackets::Garrison
{
WorldPacket const* GarrisonCreateResult::Write()
{
    _worldPacket << uint32(Result);
    _worldPacket << uint32(GarrSiteLevelID);

    return &_worldPacket;
}

WorldPacket const* GarrisonDeleteResult::Write()
{
    _worldPacket << uint32(Result);
    _worldPacket << uint32(GarrSiteID);

    return &_worldPacket;
}

ByteBuffer& operator<<(ByteBuffer& data, GarrisonPlotInfo const& plotInfo)
{
    data << uint32(plotInfo.GarrPlotInstanceID);
    data << plotInfo.PlotPos;
    data << uint8(plotInfo.PlotType);

    return data;
}

ByteBuffer& operator<<(ByteBuffer& data, GarrisonBuildingInfo const& buildingInfo)
{
    data << buildingInfo.TimeBuilt;
    data << uint32(buildingInfo.GarrPlotInstanceID);
    data << uint32(buildingInfo.GarrBuildingID);
    data << uint32(buildingInfo.CurrentGarSpecID);
    data << buildingInfo.TimeSpecCooldown;
    data << Bits<1>(buildingInfo.Active);
    data.FlushBits();

    return data;
}

ByteBuffer& operator<<(ByteBuffer& data, GarrisonFollower const& follower)
{
    data << uint64(follower.DbID);
    data << uint32(follower.GarrFollowerID);
    data << uint32(follower.Quality);
    data << uint32(follower.FollowerLevel);
    data << uint32(follower.ItemLevelWeapon);
    data << uint32(follower.ItemLevelArmor);
    data << uint32(follower.Xp);
    data << uint32(follower.Durability);
    data << uint32(follower.CurrentBuildingID);
    data << uint32(follower.CurrentMissionID);
    data << Size<uint32>(follower.AbilityID);
    data << uint32(follower.ZoneSupportSpellID);
    data << uint32(follower.FollowerStatus);
    data << int32(follower.Health);
    data << int8(follower.BoardIndex);
    data << follower.HealingTimestamp;
    for (GarrAbilityEntry const* ability : follower.AbilityID)
        data << uint32(ability->ID);

    data << SizedString::BitsSize<7>(follower.CustomName);
    data.FlushBits();

    data << SizedString::Data(follower.CustomName);

    return data;
}

ByteBuffer& operator<<(ByteBuffer& data, GarrisonEncounter const& encounter)
{
    data << int32(encounter.GarrEncounterID);
    data << Size<uint32>(encounter.Mechanics);
    data << int32(encounter.GarrAutoCombatantID);
    data << int32(encounter.Health);
    data << int32(encounter.MaxHealth);
    data << int32(encounter.Attack);
    data << int8(encounter.BoardIndex);

    if (!encounter.Mechanics.empty())
        data.append(encounter.Mechanics.data(), encounter.Mechanics.size());

    return data;
}

ByteBuffer& operator<<(ByteBuffer& data, GarrisonMissionReward const& missionRewardItem)
{
    data << int32(missionRewardItem.ItemID);
    data << uint32(missionRewardItem.ItemQuantity);
    data << int32(missionRewardItem.CurrencyID);
    data << uint32(missionRewardItem.CurrencyQuantity);
    data << uint32(missionRewardItem.FollowerXP);
    data << uint32(missionRewardItem.GarrMssnBonusAbilityID);
    data << int32(missionRewardItem.ItemFileDataID);
    data << OptionalInit(missionRewardItem.ItemInstance);
    data.FlushBits();

    if (missionRewardItem.ItemInstance)
        data << *missionRewardItem.ItemInstance;

    return data;
}

ByteBuffer& operator<<(ByteBuffer& data, GarrisonMission const& mission)
{
    data << uint64(mission.DbID);
    data << int32(mission.MissionRecID);
    data << mission.OfferTime;
    data << mission.OfferDuration;
    data << mission.StartTime;
    data << mission.TravelDuration;
    data << mission.MissionDuration;
    data << int32(mission.MissionState);
    data << int32(mission.SuccessChance);
    data << uint32(mission.Flags);
    data << float(mission.MissionScalar);
    data << int32(mission.ContentTuningID);
    data << Size<uint32>(mission.Encounters);
    data << Size<uint32>(mission.Rewards);
    data << Size<uint32>(mission.OvermaxRewards);

    for (GarrisonEncounter const& encounter : mission.Encounters)
        data << encounter;

    for (GarrisonMissionReward const& missionRewardItem : mission.Rewards)
        data << missionRewardItem;

    for (GarrisonMissionReward const& missionRewardItem : mission.OvermaxRewards)
        data << missionRewardItem;

    return data;
}

ByteBuffer& operator<<(ByteBuffer& data, GarrisonMissionBonusAbility const& areaBonus)
{
    data << areaBonus.StartTime;
    data << uint32(areaBonus.GarrMssnBonusAbilityID);

    return data;
}

ByteBuffer& operator<<(ByteBuffer& data, GarrisonTalentSocketData const& talentSocketData)
{
    data << int32(talentSocketData.SoulbindConduitID);
    data << int32(talentSocketData.SoulbindConduitRank);

    return data;
}

ByteBuffer& operator<<(ByteBuffer& data, GarrisonTalent const& talent)
{
    data << int32(talent.GarrTalentID);
    data << int32(talent.Rank);
    data << talent.ResearchStartTime;
    data << int32(talent.Flags);
    data << OptionalInit(talent.Socket);
    data.FlushBits();

    if (talent.Socket)
        data << *talent.Socket;

    return data;
}

ByteBuffer& operator<<(ByteBuffer& data, GarrisonCollectionEntry const& collectionEntry)
{
    data << int32(collectionEntry.EntryID);
    data << int32(collectionEntry.Rank);

    return data;
}

ByteBuffer& operator<<(ByteBuffer& data, GarrisonCollection const& collection)
{
    data << int32(collection.Type);
    data << Size<uint32>(collection.Entries);
    for (GarrisonCollectionEntry const& collectionEntry : collection.Entries)
        data << collectionEntry;

    return data;
}

ByteBuffer& operator<<(ByteBuffer& data, GarrisonEventEntry const& event)
{
    data << int64(event.EventValue);
    data << int32(event.EntryID);

    return data;
}

ByteBuffer& operator<<(ByteBuffer& data, GarrisonEventList const& eventList)
{
    data << int32(eventList.Type);
    data << Size<uint32>(eventList.Events);
    for (GarrisonEventEntry const& event : eventList.Events)
        data << event;

    return data;
}

ByteBuffer& operator<<(ByteBuffer& data, GarrisonSpecGroup const& specGroup)
{
    data << int32(specGroup.ChrSpecializationID);
    data << int32(specGroup.SoulbindID);

    return data;
}

ByteBuffer& operator<<(ByteBuffer& data, GarrisonInfo const& garrison)
{
    ASSERT(garrison.Missions.size() == garrison.MissionRewards.size());
    ASSERT(garrison.Missions.size() == garrison.MissionOvermaxRewards.size());
    ASSERT(garrison.Missions.size() == garrison.CanStartMission.size());

    data << uint8(garrison.GarrTypeID);
    data << int32(garrison.GarrSiteID);
    data << int32(garrison.GarrSiteLevelID);
    data << Size<uint32>(garrison.Buildings);
    data << Size<uint32>(garrison.Plots);
    data << Size<uint32>(garrison.Followers);
    data << Size<uint32>(garrison.AutoTroops);
    data << Size<uint32>(garrison.Missions);
    data << Size<uint32>(garrison.MissionRewards);
    data << Size<uint32>(garrison.MissionOvermaxRewards);
    data << Size<uint32>(garrison.MissionAreaBonuses);
    data << Size<uint32>(garrison.Talents);
    data << Size<uint32>(garrison.Collections);
    data << Size<uint32>(garrison.EventLists);
    data << Size<uint32>(garrison.SpecGroups);
    data << Size<uint32>(garrison.CanStartMission);
    data << Size<uint32>(garrison.ArchivedMissions);
    data << int32(garrison.NumFollowerActivationsRemaining);
    data << uint32(garrison.NumMissionsStartedToday);
    data << int32(garrison.MinAutoTroopLevel);

    for (GarrisonBuildingInfo const* building : garrison.Buildings)
        data << *building;

    for (GarrisonPlotInfo const* plot : garrison.Plots)
        data << *plot;

    for (GarrisonFollower const* follower : garrison.Followers)
        data << *follower;

    for (GarrisonFollower const* follower : garrison.AutoTroops)
        data << *follower;

    for (GarrisonMission const* mission : garrison.Missions)
        data << *mission;

    for (std::vector<GarrisonMissionReward> const& missionReward : garrison.MissionRewards)
    {
        data << Size<uint32>(missionReward);
        for (GarrisonMissionReward const& missionRewardItem : missionReward)
            data << missionRewardItem;
    }

    for (std::vector<GarrisonMissionReward> const& missionReward : garrison.MissionOvermaxRewards)
    {
        data << Size<uint32>(missionReward);
        for (GarrisonMissionReward const& missionRewardItem : missionReward)
            data << missionRewardItem;
    }

    for (GarrisonMissionBonusAbility const* areaBonus : garrison.MissionAreaBonuses)
        data << *areaBonus;

    for (GarrisonTalent const& talent : garrison.Talents)
        data << talent;

    for (GarrisonCollection const& collection : garrison.Collections)
        data << collection;

    for (GarrisonEventList const& eventList : garrison.EventLists)
        data << eventList;

    for (GarrisonSpecGroup const& specGroup : garrison.SpecGroups)
        data << specGroup;

    if (!garrison.ArchivedMissions.empty())
        data.append(garrison.ArchivedMissions.data(), garrison.ArchivedMissions.size());

    for (bool canStartMission : garrison.CanStartMission)
        data << Bits<1>(canStartMission);

    data.FlushBits();

    return data;
}

ByteBuffer& operator<<(ByteBuffer& data, FollowerSoftCapInfo const& followerSoftCapInfo)
{
    data << uint8(followerSoftCapInfo.GarrFollowerTypeID);
    data << uint32(followerSoftCapInfo.Count);
    return data;
}

WorldPacket const* GetGarrisonInfoResult::Write()
{
    _worldPacket << int8(FactionIndex);
    _worldPacket << Size<uint32>(Garrisons);
    _worldPacket << Size<uint32>(FollowerSoftCaps);

    for (GarrisonInfo const& garrison : Garrisons)
        _worldPacket << garrison;

    for (FollowerSoftCapInfo const& followerSoftCapInfo : FollowerSoftCaps)
        _worldPacket << followerSoftCapInfo;

    return &_worldPacket;
}

ByteBuffer& operator<<(ByteBuffer& data, GarrisonRemoteBuildingInfo const& building)
{
    data << uint32(building.GarrPlotInstanceID);
    data << uint32(building.GarrBuildingID);

    return data;
}

ByteBuffer& operator<<(ByteBuffer& data, GarrisonRemoteSiteInfo const& site)
{
    data << uint32(site.GarrSiteLevelID);
    data << Size<uint32>(site.Buildings);
    for (GarrisonRemoteBuildingInfo const& building : site.Buildings)
        data << building;

    return data;
}

WorldPacket const* GarrisonRemoteInfo::Write()
{
    _worldPacket << Size<uint32>(Sites);
    for (GarrisonRemoteSiteInfo const& site : Sites)
        _worldPacket << site;

    return &_worldPacket;
}

void GarrisonPurchaseBuilding::Read()
{
    _worldPacket >> NpcGUID;
    _worldPacket >> PlotInstanceID;
    _worldPacket >> BuildingID;
}

WorldPacket const* GarrisonPlaceBuildingResult::Write()
{
    _worldPacket << uint8(GarrTypeID);
    _worldPacket << uint32(Result);
    _worldPacket << BuildingInfo;
    _worldPacket << Bits<1>(PlayActivationCinematic);
    _worldPacket.FlushBits();

    return &_worldPacket;
}

void GarrisonCancelConstruction::Read()
{
    _worldPacket >> NpcGUID;
    _worldPacket >> PlotInstanceID;
}

WorldPacket const* GarrisonBuildingRemoved::Write()
{
    _worldPacket << uint8(GarrTypeID);
    _worldPacket << uint32(Result);
    _worldPacket << uint32(GarrPlotInstanceID);
    _worldPacket << uint32(GarrBuildingID);

    return &_worldPacket;
}

WorldPacket const* GarrisonLearnBlueprintResult::Write()
{
    _worldPacket << uint8(GarrTypeID);
    _worldPacket << uint32(Result);
    _worldPacket << uint32(BuildingID);

    return &_worldPacket;
}

WorldPacket const* GarrisonUnlearnBlueprintResult::Write()
{
    _worldPacket << uint8(GarrTypeID);
    _worldPacket << uint32(Result);
    _worldPacket << uint32(BuildingID);

    return &_worldPacket;
}

WorldPacket const* GarrisonRequestBlueprintAndSpecializationDataResult::Write()
{
    _worldPacket << uint8(GarrTypeID);
    _worldPacket << uint32(BlueprintsKnown ? BlueprintsKnown->size() : 0);
    _worldPacket << uint32(SpecializationsKnown ? SpecializationsKnown->size() : 0);
    if (BlueprintsKnown)
        for (uint32 blueprint : *BlueprintsKnown)
            _worldPacket << uint32(blueprint);

    if (SpecializationsKnown)
        for (uint32 specialization : *SpecializationsKnown)
            _worldPacket << uint32(specialization);

    return &_worldPacket;
}

ByteBuffer& operator<<(ByteBuffer& data, GarrisonBuildingMapData const& building)
{
    data << uint32(building.GarrBuildingPlotInstID);
    data << building.Pos;

    return data;
}

WorldPacket const* GarrisonMapDataResponse::Write()
{
    _worldPacket << Size<uint32>(Buildings);
    for (GarrisonBuildingMapData& landmark : Buildings)
        _worldPacket << landmark;

    return &_worldPacket;
}

WorldPacket const* GarrisonPlotPlaced::Write()
{
    _worldPacket << uint8(GarrTypeID);
    _worldPacket << *PlotInfo;

    return &_worldPacket;
}

WorldPacket const* GarrisonPlotRemoved::Write()
{
    _worldPacket << uint32(GarrPlotInstanceID);

    return &_worldPacket;
}

WorldPacket const* GarrisonAddFollowerResult::Write()
{
    _worldPacket << uint8(GarrTypeID);
    _worldPacket << uint32(Result);
    _worldPacket << Follower;

    return &_worldPacket;
}

WorldPacket const* GarrisonRemoveFollowerResult::Write()
{
    _worldPacket << uint8(GarrTypeID);
    _worldPacket << uint32(Result);
    _worldPacket << uint64(FollowerDBID);
    _worldPacket << uint32(Destroyed);

    return &_worldPacket;
}

WorldPacket const* GarrisonBuildingActivated::Write()
{
    _worldPacket << uint32(GarrPlotInstanceID);

    return &_worldPacket;
}

void GarrisonCheckUpgradeable::Read()
{
    _worldPacket >> GarrSiteID;
}

WorldPacket const* GarrisonIsUpgradeableResponse::Write()
{
    _worldPacket << uint32(Result);

    return &_worldPacket;
}

void UpgradeGarrison::Read()
{
    _worldPacket >> NpcGUID;
}

WorldPacket const* GarrisonUpgradeResult::Write()
{
    _worldPacket << uint32(Result);
    _worldPacket << uint32(GarrSiteLevelID);

    return &_worldPacket;
}

void ActivateSoulbind::Read()
{
    _worldPacket >> SoulbindID;
}

WorldPacket const* GarrisonAddSpecGroups::Write()
{
    _worldPacket << uint8(GarrTypeID);
    _worldPacket << Size<uint32>(SpecGroups);
    for (GarrisonSpecGroup const& specGroup : SpecGroups)
        _worldPacket << specGroup;

    return &_worldPacket;
}

void GarrisonResearchTalent::Read()
{
    _worldPacket >> NPCTalentGuid;
    _worldPacket >> GarrTalentID;
    _worldPacket >> Flags;
    IsTemporary = _worldPacket.ReadBit();
}

WorldPacket const* GarrisonResearchTalentResult::Write()
{
    _worldPacket << int32(Result);
    _worldPacket << uint8(GarrTypeID);
    _worldPacket << Talent;
    _worldPacket << Bits<1>(DontAlert);
    _worldPacket.FlushBits();

    return &_worldPacket;
}

WorldPacket const* GarrisonTalentCompleted::Write()
{
    _worldPacket << uint8(GarrTypeID);
    _worldPacket << int32(GarrTalentID);
    _worldPacket << int32(GarrTalentRank);
    _worldPacket << int32(Flags);

    return &_worldPacket;
}

void GarrisonLearnTalent::Read()
{
    _worldPacket >> SoulbindID;
    _worldPacket >> GarrTalentID;
}

WorldPacket const* GarrisonSwitchTalentTreeBranch::Write()
{
    _worldPacket << uint8(GarrTypeID);
    _worldPacket << Size<uint32>(Talents);
    for (GarrisonTalent const& talent : Talents)
        _worldPacket << talent;

    return &_worldPacket;
}

void GarrisonSocketTalent::Read()
{
    _worldPacket >> GarrTalentTreeID;
    uint32 count = _worldPacket.read<uint32>();
    if (count > 64)
        throw PacketArrayMaxCapacityException(count, 64);

    Sockets.resize(count);
    for (GarrisonSocketTalentEntry& socket : Sockets)
    {
        _worldPacket >> socket.GarrTalentID;
        _worldPacket >> socket.SoulbindConduitID;
    }
}

WorldPacket const* GarrisonApplyTalentSocketDataChanges::Write()
{
    _worldPacket << uint8(GarrTypeID);
    _worldPacket << Size<uint32>(Changed);
    _worldPacket << Size<uint32>(Cleared);

    for (GarrisonSocketChange const& change : Changed)
    {
        _worldPacket << int32(change.GarrTalentID);
        _worldPacket << OptionalInit(change.Socket);
        _worldPacket.FlushBits();
        if (change.Socket)
            _worldPacket << *change.Socket;
    }

    for (int32 garrTalentId : Cleared)
        _worldPacket << int32(garrTalentId);

    return &_worldPacket;
}

WorldPacket const* GarrisonCollectionUpdateEntry::Write()
{
    _worldPacket << uint8(GarrTypeID);
    _worldPacket << int32(Type);
    _worldPacket << int32(EntryID);
    _worldPacket << int32(Rank);

    return &_worldPacket;
}

WorldPacket const* CovenantRenownSendCatchupState::Write()
{
    _worldPacket.WriteBit(CatchupState);
    _worldPacket.FlushBits();

    return &_worldPacket;
}

WorldPacket const* CovenantCallingsAvailabilityResponse::Write()
{
    _worldPacket.WriteBit(AreCallingsUnlocked);
    _worldPacket.FlushBits();
    _worldPacket << uint32(BountyID.size());
    for (int32 bountyId : BountyID)
        _worldPacket << int32(bountyId);

    return &_worldPacket;
}

void OpenMissionNpc::Read()
{
    _worldPacket >> NpcGUID;
    _worldPacket >> GarrFollowerTypeID;
}

void GarrisonGetClassSpecCategoryInfo::Read()
{
    _worldPacket >> GarrFollowerTypeID;
}

WorldPacket const* GarrisonGetClassSpecCategoryInfoResult::Write()
{
    _worldPacket << uint8(GarrFollowerTypeID);
    _worldPacket << uint32(0);

    return &_worldPacket;
}

void GarrisonStartMission::Read()
{
    _worldPacket >> NpcGUID;
    uint32 count = _worldPacket.read<uint32>();
    _worldPacket >> MissionRecID;
    if (count > 16)
        throw PacketArrayMaxCapacityException(count, 16);

    Followers.resize(count);
    for (GarrisonMissionFollowerInfo& follower : Followers)
    {
        _worldPacket >> follower.DatabaseID;
        _worldPacket >> follower.BoardIndex;
        _worldPacket >> follower.Health;
        bool hasFollowerEntry = _worldPacket.ReadBit();
        _worldPacket.ResetBitPos();
        if (hasFollowerEntry)
            _worldPacket >> follower.FollowerEntry.emplace();
    }
}

static void WriteMissionFollowerInfo(ByteBuffer& data, GarrisonMissionFollowerInfo const& follower)
{
    data << uint64(follower.DatabaseID);
    data << int32(follower.BoardIndex);
    data << int32(follower.Health);
    data << OptionalInit(follower.FollowerEntry);
    data.FlushBits();
    if (follower.FollowerEntry)
        data << int32(*follower.FollowerEntry);
}

WorldPacket const* GarrisonStartMissionResult::Write()
{
    _worldPacket << int32(Result);
    _worldPacket << uint16(SessionMissionCount);
    _worldPacket << Mission;
    _worldPacket << Size<uint32>(Followers);
    _worldPacket << Size<uint32>(Troops);
    for (GarrisonMissionFollowerInfo const& follower : Followers)
        WriteMissionFollowerInfo(_worldPacket, follower);

    for (GarrisonFollower const& troop : Troops)
        _worldPacket << troop;

    return &_worldPacket;
}

void GarrisonCompleteMission::Read()
{
    _worldPacket >> NpcGUID;
    _worldPacket >> MissionRecID;
}

void GarrisonMissionBonusRoll::Read()
{
    _worldPacket >> NpcGUID;
    _worldPacket >> MissionRecID;
}

WorldPacket const* GarrisonMissionBonusRollResult::Write()
{
    _worldPacket << Mission;
    _worldPacket << int32(MissionRecID);
    _worldPacket << int32(Result);
    _worldPacket << Size<uint32>(Followers);
    for (GarrisonMissionFollowerHealth const& follower : Followers)
    {
        _worldPacket << uint64(follower.DbID);
        _worldPacket << int32(follower.Health);
        _worldPacket << follower.HealingTimestamp;
        _worldPacket << int32(0);
    }

    _worldPacket << Bits<1>(false);   // the capture ends with one all-zero bit byte
    _worldPacket.FlushBits();

    return &_worldPacket;
}

namespace
{
    // feeds the shared log serialiser of AutoCombat.h into the packet
    struct CombatLogWriter
    {
        ByteBuffer& Buffer;
        void Put8(uint8 value) { Buffer << value; }
        void Put32(uint32 value) { Buffer << value; }
    };
}

WorldPacket const* GarrisonCompleteMissionResult::Write()
{
    _worldPacket << int32(Result);
    _worldPacket << Mission;
    _worldPacket << int32(MissionRecID);
    _worldPacket << uint8(UnknownByte);
    _worldPacket << Size<uint32>(Followers);
    _worldPacket << uint32(Combat.Rounds.size());
    for (GarrisonMissionFollowerHealth const& follower : Followers)
    {
        _worldPacket << uint64(follower.DbID);
        _worldPacket << int32(follower.Health);
        _worldPacket << follower.HealingTimestamp;
        _worldPacket << int32(0);
    }

    CombatLogWriter writer{ _worldPacket };
    AutoCombat::WriteRounds(Combat, writer);

    _worldPacket << Bits<1>(true);    // 0xC0 in the three captures, all won; which bit is which is not pinned
    _worldPacket << Bits<1>(true);
    _worldPacket.FlushBits();

    return &_worldPacket;
}

WorldPacket const* GarrisonAddMissionResult::Write()
{
    _worldPacket << uint8(GarrTypeID);
    _worldPacket << int32(Result);
    _worldPacket << uint8(State);
    _worldPacket << Mission;
    _worldPacket << Bits<1>(CanStart);
    _worldPacket.FlushBits();

    return &_worldPacket;
}

WorldPacket const* GarrisonMissionStartConditionUpdate::Write()
{
    _worldPacket << Size<uint32>(Missions);
    _worldPacket << Size<uint32>(Missions);
    for (auto const& [missionRecId, canStart] : Missions)
        _worldPacket << int32(missionRecId);

    for (auto const& [missionRecId, canStart] : Missions)
        _worldPacket << Bits<1>(canStart);

    _worldPacket.FlushBits();

    return &_worldPacket;
}

WorldPacket const* GarrisonFollowerChangedXp::Write()
{
    _worldPacket << uint32(XPEarned);
    _worldPacket << uint32(XPSource);
    _worldPacket << FollowerBefore;
    _worldPacket << FollowerAfter;

    return &_worldPacket;
}

WorldPacket const* GarrisonAutoTroopMinLevelUpdateResult::Write()
{
    _worldPacket << int32(GarrSiteID);
    _worldPacket << int32(MinLevel);

    return &_worldPacket;
}
}
