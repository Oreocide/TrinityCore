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

#ifndef TRINITYCORE_GARRISON_PACKETS_H
#define TRINITYCORE_GARRISON_PACKETS_H

#include "Packet.h"
#include "ItemPacketsCommon.h"
#include "ObjectGuid.h"
#include "Optional.h"
#include "Position.h"
#include "PacketUtilities.h"
#include <list>
#include <unordered_set>
#include <vector>

struct GarrAbilityEntry;

namespace WorldPackets
{
    namespace Garrison
    {
        class GarrisonCreateResult final : public ServerPacket
        {
        public:
            explicit GarrisonCreateResult() : ServerPacket(SMSG_GARRISON_CREATE_RESULT, 4 + 4) { }

            WorldPacket const* Write() override;

            uint32 GarrSiteLevelID = 0;
            uint32 Result = 0;
        };

        class GarrisonDeleteResult final : public ServerPacket
        {
        public:
            explicit GarrisonDeleteResult() : ServerPacket(SMSG_GARRISON_DELETE_RESULT, 4 + 4) { }

            WorldPacket const* Write() override;

            uint32 Result = 0;
            uint32 GarrSiteID = 0;
        };

        class GetGarrisonInfo final : public ClientPacket
        {
        public:
            explicit GetGarrisonInfo(WorldPacket&& packet) : ClientPacket(CMSG_GET_GARRISON_INFO, std::move(packet)) { }

            void Read() override { }
        };

        struct GarrisonPlotInfo
        {
            uint32 GarrPlotInstanceID = 0;
            TaggedPosition<Position::XYZO> PlotPos;
            uint8 PlotType = 0;
        };

        struct GarrisonBuildingInfo
        {
            Timestamp<> TimeBuilt;
            uint32 GarrPlotInstanceID = 0;
            uint32 GarrBuildingID = 0;
            uint32 CurrentGarSpecID = 0;
            Timestamp<> TimeSpecCooldown = time_t(2288912640);   // 06/07/1906 18:35:44 - another in the series of magic blizz dates
            bool Active = false;
        };

        struct GarrisonFollower
        {
            uint64 DbID = 0;
            uint32 GarrFollowerID = 0;
            uint32 Quality = 0;
            uint32 FollowerLevel = 0;
            uint32 ItemLevelWeapon = 0;
            uint32 ItemLevelArmor = 0;
            uint32 Xp = 0;
            uint32 Durability = 0;
            uint32 CurrentBuildingID = 0;
            uint32 CurrentMissionID = 0;
            std::list<GarrAbilityEntry const*> AbilityID;
            uint32 ZoneSupportSpellID = 0;
            uint32 FollowerStatus = 0;
            int32 Health = 0;
            Timestamp<> HealingTimestamp;
            int8 BoardIndex = 0;
            std::string CustomName;
        };

        struct GarrisonEncounter
        {
            int32 GarrEncounterID = 0;
            std::vector<int32> Mechanics;
            int32 GarrAutoCombatantID = 0;
            int32 Health = 0;
            int32 MaxHealth = 0;
            int32 Attack = 0;
            int8 BoardIndex = 0;
        };

        struct GarrisonMissionReward
        {
            int32 ItemID = 0;
            uint32 ItemQuantity = 0;
            int32 CurrencyID = 0;
            uint32 CurrencyQuantity = 0;
            uint32 FollowerXP = 0;
            uint32 GarrMssnBonusAbilityID = 0;
            int32 ItemFileDataID = 0;
            Optional<Item::ItemInstance> ItemInstance;
        };

        struct GarrisonMission
        {
            uint64 DbID = 0;
            int32 MissionRecID = 0;
            Timestamp<> OfferTime;
            Duration<Seconds> OfferDuration;
            Timestamp<> StartTime = time_t(2288912640);
            Duration<Seconds> TravelDuration;
            Duration<Seconds> MissionDuration;
            int32 MissionState = 0;
            int32 SuccessChance = 0;
            uint32 Flags = 0;
            float MissionScalar = 1.0f;
            int32 ContentTuningID = 0;
            std::vector<GarrisonEncounter> Encounters;
            std::vector<GarrisonMissionReward> Rewards;
            std::vector<GarrisonMissionReward> OvermaxRewards;
        };

        struct GarrisonMissionBonusAbility
        {
            uint32 GarrMssnBonusAbilityID = 0;
            Timestamp<> StartTime;
        };

        struct GarrisonTalentSocketData
        {
            int32 SoulbindConduitID = 0;
            int32 SoulbindConduitRank = 0;
        };

        struct GarrisonTalent
        {
            int32 GarrTalentID = 0;
            int32 Rank = 0;
            Timestamp<> ResearchStartTime;
            int32 Flags = 0;
            Optional<GarrisonTalentSocketData> Socket;
        };

        struct GarrisonCollectionEntry
        {
            int32 EntryID = 0;
            int32 Rank = 0;
        };

        struct GarrisonCollection
        {
            int32 Type = 0;
            std::vector<GarrisonCollectionEntry> Entries;
        };

        struct GarrisonEventEntry
        {
            int32 EntryID = 0;
            int64 EventValue = 0;
        };

        struct GarrisonEventList
        {
            int32 Type = 0;
            std::vector<GarrisonEventEntry> Events;
        };

        struct GarrisonSpecGroup
        {
            int32 ChrSpecializationID = 0;
            int32 SoulbindID = 0;
        };

        struct GarrisonInfo
        {
            uint8 GarrTypeID = 0;
            uint32 GarrSiteID = 0;
            uint32 GarrSiteLevelID = 0;
            uint32 NumFollowerActivationsRemaining = 0;
            uint32 NumMissionsStartedToday = 0;   // might mean something else, but sending 0 here enables follower abilities "Increase success chance of the first mission of the day by %."
            int32 MinAutoTroopLevel = 0;
            std::vector<GarrisonPlotInfo const*> Plots;
            std::vector<GarrisonBuildingInfo const*> Buildings;
            std::vector<GarrisonFollower const*> Followers;
            std::vector<GarrisonFollower const*> AutoTroops;
            std::vector<GarrisonMission const*> Missions;
            std::vector<std::vector<GarrisonMissionReward>> MissionRewards;
            std::vector<std::vector<GarrisonMissionReward>> MissionOvermaxRewards;
            std::vector<GarrisonMissionBonusAbility const*> MissionAreaBonuses;
            std::vector<GarrisonTalent> Talents;
            std::vector<GarrisonCollection> Collections;
            std::vector<GarrisonEventList> EventLists;
            std::vector<GarrisonSpecGroup> SpecGroups;
            std::vector<bool> CanStartMission;
            std::vector<int32> ArchivedMissions;
        };

        struct FollowerSoftCapInfo
        {
            uint8 GarrFollowerTypeID;
            uint32 Count;
        };

        class GetGarrisonInfoResult final : public ServerPacket
        {
        public:
            explicit GetGarrisonInfoResult() : ServerPacket(SMSG_GET_GARRISON_INFO_RESULT) { }

            WorldPacket const* Write() override;

            int8 FactionIndex = 0;
            std::vector<GarrisonInfo> Garrisons;
            std::vector<FollowerSoftCapInfo> FollowerSoftCaps;
        };

        struct GarrisonRemoteBuildingInfo
        {
            GarrisonRemoteBuildingInfo() : GarrPlotInstanceID(0), GarrBuildingID(0) { }
            GarrisonRemoteBuildingInfo(uint32 plotInstanceId, uint32 buildingId) : GarrPlotInstanceID(plotInstanceId), GarrBuildingID(buildingId) { }

            uint32 GarrPlotInstanceID;
            uint32 GarrBuildingID;
        };

        struct GarrisonRemoteSiteInfo
        {
            uint32 GarrSiteLevelID = 0;
            std::vector<GarrisonRemoteBuildingInfo> Buildings;
        };

        class GarrisonRemoteInfo final : public ServerPacket
        {
        public:
            explicit GarrisonRemoteInfo() : ServerPacket(SMSG_GARRISON_REMOTE_INFO) { }

            WorldPacket const* Write() override;

            std::vector<GarrisonRemoteSiteInfo> Sites;
        };

        class GarrisonPurchaseBuilding final : public ClientPacket
        {
        public:
            explicit GarrisonPurchaseBuilding(WorldPacket&& packet) : ClientPacket(CMSG_GARRISON_PURCHASE_BUILDING, std::move(packet)) { }

            void Read() override;

            ObjectGuid NpcGUID;
            uint32 BuildingID = 0;
            uint32 PlotInstanceID = 0;
        };

        class GarrisonPlaceBuildingResult final : public ServerPacket
        {
        public:
            explicit GarrisonPlaceBuildingResult() : ServerPacket(SMSG_GARRISON_PLACE_BUILDING_RESULT) { }

            WorldPacket const* Write() override;

            uint8 GarrTypeID = 0;
            uint32 Result = 0;
            GarrisonBuildingInfo BuildingInfo;
            bool PlayActivationCinematic = false;
        };

        class GarrisonCancelConstruction final : public ClientPacket
        {
        public:
            explicit GarrisonCancelConstruction(WorldPacket&& packet) : ClientPacket(CMSG_GARRISON_CANCEL_CONSTRUCTION, std::move(packet)) { }

            void Read() override;

            ObjectGuid NpcGUID;
            uint32 PlotInstanceID = 0;
        };

        class GarrisonBuildingRemoved final : public ServerPacket
        {
        public:
            explicit GarrisonBuildingRemoved() : ServerPacket(SMSG_GARRISON_BUILDING_REMOVED, 4 + 4 + 4 + 4) { }

            WorldPacket const* Write() override;

            uint8 GarrTypeID = 0;
            uint32 Result = 0;
            uint32 GarrPlotInstanceID = 0;
            uint32 GarrBuildingID = 0;
        };

        class GarrisonLearnBlueprintResult final : public ServerPacket
        {
        public:
            explicit GarrisonLearnBlueprintResult() : ServerPacket(SMSG_GARRISON_LEARN_BLUEPRINT_RESULT, 4 + 4 + 4) { }

            WorldPacket const* Write() override;

            uint8 GarrTypeID = 0;
            uint32 BuildingID = 0;
            uint32 Result = 0;
        };

        class GarrisonUnlearnBlueprintResult final : public ServerPacket
        {
        public:
            explicit GarrisonUnlearnBlueprintResult() : ServerPacket(SMSG_GARRISON_UNLEARN_BLUEPRINT_RESULT, 4 + 4) { }

            WorldPacket const* Write() override;

            uint8 GarrTypeID = 0;
            uint32 Result = 0;
            uint32 BuildingID = 0;
        };

        class GarrisonRequestBlueprintAndSpecializationData final : public ClientPacket
        {
        public:
            explicit GarrisonRequestBlueprintAndSpecializationData(WorldPacket&& packet) : ClientPacket(CMSG_GARRISON_REQUEST_BLUEPRINT_AND_SPECIALIZATION_DATA, std::move(packet)) { }

            void Read() override { }
        };

        class GarrisonRequestBlueprintAndSpecializationDataResult final : public ServerPacket
        {
        public:
            explicit GarrisonRequestBlueprintAndSpecializationDataResult() : ServerPacket(SMSG_GARRISON_REQUEST_BLUEPRINT_AND_SPECIALIZATION_DATA_RESULT, 400) { }

            WorldPacket const* Write() override;

            uint8 GarrTypeID = 0;
            std::unordered_set<uint32> const* BlueprintsKnown = nullptr;
            std::unordered_set<uint32> const* SpecializationsKnown = nullptr;
        };

        class GarrisonGetMapData final : public ClientPacket
        {
        public:
            explicit GarrisonGetMapData(WorldPacket&& packet) : ClientPacket(CMSG_GARRISON_GET_MAP_DATA, std::move(packet)) { }

            void Read() override { }
        };

        struct GarrisonBuildingMapData
        {
            GarrisonBuildingMapData() : GarrBuildingPlotInstID(0), Pos() { }
            GarrisonBuildingMapData(uint32 buildingPlotInstId, Position const& pos) : GarrBuildingPlotInstID(buildingPlotInstId), Pos(pos) { }

            uint32 GarrBuildingPlotInstID;
            TaggedPosition<Position::XYZ> Pos;
        };

        class GarrisonMapDataResponse final : public ServerPacket
        {
        public:
            explicit GarrisonMapDataResponse() : ServerPacket(SMSG_GARRISON_MAP_DATA_RESPONSE) { }

            WorldPacket const* Write() override;

            std::vector<GarrisonBuildingMapData> Buildings;
        };

        class GarrisonPlotPlaced final : public ServerPacket
        {
        public:
            explicit GarrisonPlotPlaced() : ServerPacket(SMSG_GARRISON_PLOT_PLACED) { }

            WorldPacket const* Write() override;

            uint8 GarrTypeID = 0;
            GarrisonPlotInfo* PlotInfo = nullptr;
        };

        class GarrisonPlotRemoved final : public ServerPacket
        {
        public:
            explicit GarrisonPlotRemoved() : ServerPacket(SMSG_GARRISON_PLOT_REMOVED, 4) { }

            WorldPacket const* Write() override;

            uint32 GarrPlotInstanceID = 0;
        };

        class GarrisonAddFollowerResult final : public ServerPacket
        {
        public:
            explicit GarrisonAddFollowerResult() : ServerPacket(SMSG_GARRISON_ADD_FOLLOWER_RESULT, 8 + 4 + 4 + 4 + 4 + 4 + 4 + 4 + 4 + 4 + 5 * 4 + 4) { }

            WorldPacket const* Write() override;

            uint8 GarrTypeID = 0;
            GarrisonFollower Follower;
            uint32 Result = 0;
        };

        class GarrisonRemoveFollowerResult final : public ServerPacket
        {
        public:
            explicit GarrisonRemoveFollowerResult() : ServerPacket(SMSG_GARRISON_REMOVE_FOLLOWER_RESULT, 1 + 4 + 8 + 4) { }

            WorldPacket const* Write() override;

            uint8 GarrTypeID = 0;
            uint32 Result = 0;
            uint64 FollowerDBID = 0;
            uint32 Destroyed = 0;
        };

        class GarrisonBuildingActivated final : public ServerPacket
        {
        public:
            explicit GarrisonBuildingActivated() : ServerPacket(SMSG_GARRISON_BUILDING_ACTIVATED, 4) { }

            WorldPacket const* Write() override;

            uint32 GarrPlotInstanceID = 0;
        };

        class GarrisonCheckUpgradeable final : public ClientPacket
        {
        public:
            explicit GarrisonCheckUpgradeable(WorldPacket&& packet) : ClientPacket(CMSG_GARRISON_CHECK_UPGRADEABLE, std::move(packet)) { }

            void Read() override;

            int32 GarrSiteID = 0;
        };

        class GarrisonIsUpgradeableResponse final : public ServerPacket
        {
        public:
            explicit GarrisonIsUpgradeableResponse() : ServerPacket(SMSG_GARRISON_IS_UPGRADEABLE_RESPONSE, 4) { }

            WorldPacket const* Write() override;

            uint32 Result = 0;
        };

        class UpgradeGarrison final : public ClientPacket
        {
        public:
            explicit UpgradeGarrison(WorldPacket&& packet) : ClientPacket(CMSG_UPGRADE_GARRISON, std::move(packet)) { }

            void Read() override;

            ObjectGuid NpcGUID;
        };

        class GarrisonUpgradeResult final : public ServerPacket
        {
        public:
            explicit GarrisonUpgradeResult() : ServerPacket(SMSG_GARRISON_UPGRADE_RESULT, 4 + 4) { }

            WorldPacket const* Write() override;

            uint32 Result = 0;
            uint32 GarrSiteLevelID = 0;
        };
        class ActivateSoulbind final : public ClientPacket
        {
        public:
            explicit ActivateSoulbind(WorldPacket&& packet) : ClientPacket(CMSG_ACTIVATE_SOULBIND, std::move(packet)) { }

            void Read() override;

            int32 SoulbindID = 0;
        };

        class GarrisonAddSpecGroups final : public ServerPacket
        {
        public:
            explicit GarrisonAddSpecGroups() : ServerPacket(SMSG_GARRISON_ADD_SPEC_GROUPS) { }

            WorldPacket const* Write() override;

            uint8 GarrTypeID = 0;
            std::vector<GarrisonSpecGroup> SpecGroups;
        };

        class GarrisonResearchTalent final : public ClientPacket
        {
        public:
            explicit GarrisonResearchTalent(WorldPacket&& packet) : ClientPacket(CMSG_GARRISON_RESEARCH_TALENT, std::move(packet)) { }

            void Read() override;

            ObjectGuid NPCTalentGuid; // empty for the soulbind (Forge of Bonds) trees
            int32 GarrTalentID = 0;
            int32 Flags = 0;
            bool IsTemporary = false;
        };

        class GarrisonResearchTalentResult final : public ServerPacket
        {
        public:
            explicit GarrisonResearchTalentResult() : ServerPacket(SMSG_GARRISON_RESEARCH_TALENT_RESULT) { }

            WorldPacket const* Write() override;

            int32 Result = 0;
            uint8 GarrTypeID = 0;
            GarrisonTalent Talent;
            bool DontAlert = false;
        };

        class GarrisonTalentCompleted final : public ServerPacket
        {
        public:
            explicit GarrisonTalentCompleted() : ServerPacket(SMSG_GARRISON_TALENT_COMPLETED, 1 + 4 + 4 + 4) { }

            WorldPacket const* Write() override;

            uint8 GarrTypeID = 0;
            int32 GarrTalentID = 0;
            int32 GarrTalentRank = 0;
            int32 Flags = 0;
        };

        class GarrisonLearnTalent final : public ClientPacket
        {
        public:
            explicit GarrisonLearnTalent(WorldPacket&& packet) : ClientPacket(CMSG_GARRISON_LEARN_TALENT, std::move(packet)) { }

            void Read() override;

            int32 SoulbindID = 0; // inferred: the capture sent 7 = the active soulbind (Pelagos); not validated
            int32 GarrTalentID = 0;
        };

        class GarrisonSwitchTalentTreeBranch final : public ServerPacket
        {
        public:
            explicit GarrisonSwitchTalentTreeBranch() : ServerPacket(SMSG_GARRISON_SWITCH_TALENT_TREE_BRANCH) { }

            WorldPacket const* Write() override;

            uint8 GarrTypeID = 0;
            std::vector<GarrisonTalent> Talents;
        };

        struct GarrisonSocketTalentEntry
        {
            int32 GarrTalentID = 0;
            int32 SoulbindConduitID = 0; // 0 empties the socket
        };

        class GarrisonSocketTalent final : public ClientPacket
        {
        public:
            explicit GarrisonSocketTalent(WorldPacket&& packet) : ClientPacket(CMSG_GARRISON_SOCKET_TALENT, std::move(packet)) { }

            void Read() override;

            int32 GarrTalentTreeID = 0;
            std::vector<GarrisonSocketTalentEntry> Sockets;
        };

        struct GarrisonSocketChange
        {
            int32 GarrTalentID = 0;
            Optional<GarrisonTalentSocketData> Socket;
        };

        class GarrisonApplyTalentSocketDataChanges final : public ServerPacket
        {
        public:
            explicit GarrisonApplyTalentSocketDataChanges() : ServerPacket(SMSG_GARRISON_APPLY_TALENT_SOCKET_DATA_CHANGES) { }

            WorldPacket const* Write() override;

            uint8 GarrTypeID = 0;
            std::vector<GarrisonSocketChange> Changed;
            std::vector<int32> Cleared;
        };

        class GarrisonCollectionUpdateEntry final : public ServerPacket
        {
        public:
            explicit GarrisonCollectionUpdateEntry() : ServerPacket(SMSG_GARRISON_COLLECTION_UPDATE_ENTRY, 1 + 4 + 4 + 4) { }

            WorldPacket const* Write() override;

            uint8 GarrTypeID = 0;
            int32 Type = 0;
            int32 EntryID = 0;
            int32 Rank = 0;
        };

        class CovenantRenownRequestCatchupState final : public ClientPacket
        {
        public:
            explicit CovenantRenownRequestCatchupState(WorldPacket&& packet) : ClientPacket(CMSG_COVENANT_RENOWN_REQUEST_CATCHUP_STATE, std::move(packet)) { }

            void Read() override { }
        };

        class CovenantRenownSendCatchupState final : public ServerPacket
        {
        public:
            explicit CovenantRenownSendCatchupState() : ServerPacket(SMSG_COVENANT_RENOWN_SEND_CATCHUP_STATE, 1) { }

            WorldPacket const* Write() override;

            bool CatchupState = false;
        };

        class RequestCovenantCallings final : public ClientPacket
        {
        public:
            explicit RequestCovenantCallings(WorldPacket&& packet) : ClientPacket(CMSG_REQUEST_COVENANT_CALLINGS, std::move(packet)) { }

            void Read() override { }
        };

        class CovenantCallingsAvailabilityResponse final : public ServerPacket
        {
        public:
            explicit CovenantCallingsAvailabilityResponse() : ServerPacket(SMSG_COVENANT_CALLINGS_AVAILABILITY_RESPONSE, 1 + 4) { }

            WorldPacket const* Write() override;

            bool AreCallingsUnlocked = false;
            std::vector<int32> BountyID;
        };

        // 12.1.5 layouts below: nov18c raw capture (analysis/core_design/m5/PACKETS_nov18c_raw.md)
        class OpenMissionNpc final : public ClientPacket
        {
        public:
            explicit OpenMissionNpc(WorldPacket&& packet) : ClientPacket(CMSG_OPEN_MISSION_NPC, std::move(packet)) { }

            void Read() override;

            ObjectGuid NpcGUID;
            uint8 GarrFollowerTypeID = 0;
        };

        class GarrisonGetClassSpecCategoryInfo final : public ClientPacket
        {
        public:
            explicit GarrisonGetClassSpecCategoryInfo(WorldPacket&& packet) : ClientPacket(CMSG_GARRISON_GET_CLASS_SPEC_CATEGORY_INFO, std::move(packet)) { }

            void Read() override;

            uint8 GarrFollowerTypeID = 0;
        };

        class GarrisonGetClassSpecCategoryInfoResult final : public ServerPacket
        {
        public:
            explicit GarrisonGetClassSpecCategoryInfoResult() : ServerPacket(SMSG_GARRISON_GET_CLASS_SPEC_CATEGORY_INFO_RESULT, 1 + 4) { }

            WorldPacket const* Write() override;

            uint8 GarrFollowerTypeID = 0;
            // the capture always sent an empty list (5 bytes)
        };

        struct GarrisonMissionFollowerInfo
        {
            uint64 DatabaseID = 0;
            int32 BoardIndex = 0;
            int32 Health = 0;
            Optional<int32> FollowerEntry;
        };

        class GarrisonStartMission final : public ClientPacket
        {
        public:
            explicit GarrisonStartMission(WorldPacket&& packet) : ClientPacket(CMSG_GARRISON_START_MISSION, std::move(packet)) { }

            void Read() override;

            ObjectGuid NpcGUID;
            int32 MissionRecID = 0;
            std::vector<GarrisonMissionFollowerInfo> Followers;
        };

        class GarrisonStartMissionResult final : public ServerPacket
        {
        public:
            explicit GarrisonStartMissionResult() : ServerPacket(SMSG_GARRISON_START_MISSION_RESULT, 4 + 2 + 100 + 4 + 4) { }

            WorldPacket const* Write() override;

            int32 Result = 0;
            uint16 SessionMissionCount = 0;
            GarrisonMission Mission;
            std::vector<GarrisonMissionFollowerInfo> Followers;
            std::vector<GarrisonFollower> Troops;   // the auto troops made for this start, as full follower records
        };

        class GarrisonCompleteMission final : public ClientPacket
        {
        public:
            explicit GarrisonCompleteMission(WorldPacket&& packet) : ClientPacket(CMSG_GARRISON_COMPLETE_MISSION, std::move(packet)) { }

            void Read() override;

            ObjectGuid NpcGUID;
            int32 MissionRecID = 0;
        };

        class GarrisonMissionBonusRoll final : public ClientPacket
        {
        public:
            explicit GarrisonMissionBonusRoll(WorldPacket&& packet) : ClientPacket(CMSG_GARRISON_MISSION_BONUS_ROLL, std::move(packet)) { }

            void Read() override;

            ObjectGuid NpcGUID;
            int32 MissionRecID = 0;
        };

        struct GarrisonMissionFollowerHealth
        {
            uint64 DbID = 0;
            int32 Health = 0;
            Timestamp<> HealingTimestamp;
        };

        class GarrisonMissionBonusRollResult final : public ServerPacket
        {
        public:
            explicit GarrisonMissionBonusRollResult() : ServerPacket(SMSG_GARRISON_MISSION_BONUS_ROLL_RESULT, 100 + 4 + 4 + 4 + 5 * 24 + 1) { }

            WorldPacket const* Write() override;

            GarrisonMission Mission;
            int32 MissionRecID = 0;
            int32 Result = 0;
            std::vector<GarrisonMissionFollowerHealth> Followers;
        };

        class GarrisonAddMissionResult final : public ServerPacket
        {
        public:
            explicit GarrisonAddMissionResult() : ServerPacket(SMSG_GARRISON_ADD_MISSION_RESULT, 1 + 4 + 1 + 140) { }

            WorldPacket const* Write() override;

            uint8 GarrTypeID = 0;
            int32 Result = 0;
            uint8 State = 0;
            GarrisonMission Mission;
            bool CanStart = false;
        };

        class GarrisonMissionStartConditionUpdate final : public ServerPacket
        {
        public:
            explicit GarrisonMissionStartConditionUpdate() : ServerPacket(SMSG_GARRISON_MISSION_START_CONDITION_UPDATE, 4 + 4 + 4 + 1) { }

            WorldPacket const* Write() override;

            std::vector<std::pair<int32 /*MissionRecID*/, bool /*CanStart*/>> Missions;
        };

        class GarrisonFollowerChangedXp final : public ServerPacket
        {
        public:
            explicit GarrisonFollowerChangedXp() : ServerPacket(SMSG_GARRISON_FOLLOWER_CHANGED_XP, 4 + 4 + 2 * 70) { }

            WorldPacket const* Write() override;

            uint32 XPEarned = 0;
            uint32 XPSource = 0;
            GarrisonFollower FollowerBefore;
            GarrisonFollower FollowerAfter;
        };

        class GarrisonAutoTroopMinLevelUpdateResult final : public ServerPacket
        {
        public:
            explicit GarrisonAutoTroopMinLevelUpdateResult() : ServerPacket(SMSG_GARRISON_AUTO_TROOP_MIN_LEVEL_UPDATE_RESULT, 4 + 4) { }

            WorldPacket const* Write() override;

            int32 GarrSiteID = 0;
            int32 MinLevel = 0;
        };
    }
}

#endif // TRINITYCORE_GARRISON_PACKETS_H
