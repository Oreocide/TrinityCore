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

#include "WorldSession.h"
#include "CovenantGarrison.h"
#include "CovenantRenown.h"
#include "DB2Structure.h"
#include "Garrison.h"
#include "GarrisonPackets.h"
#include "Log.h"
#include "Player.h"
#include "World.h"

void WorldSession::HandleGetGarrisonInfo(WorldPackets::Garrison::GetGarrisonInfo& /*getGarrisonInfo*/)
{
    WorldPackets::Garrison::GetGarrisonInfoResult garrisonInfo;
    garrisonInfo.FactionIndex = Garrison::GetFaction(_player->GetTeam());

    if (Garrison* garrison = _player->GetGarrison())
        garrison->BuildInfoPacket(garrisonInfo.Garrisons.emplace_back());

    // the type-111 entry retail appends after the WoD one (CovenantGarrison.Enable)
    if (CovenantGarrison* covenantGarrison = _player->GetCovenantGarrison())
        covenantGarrison->AppendToInfoResult(garrisonInfo);

    SendPacket(garrisonInfo.Write());
}

void WorldSession::HandleGarrisonPurchaseBuilding(WorldPackets::Garrison::GarrisonPurchaseBuilding& garrisonPurchaseBuilding)
{
    if (!_player->GetNPCIfCanInteractWith(garrisonPurchaseBuilding.NpcGUID, UNIT_NPC_FLAG_NONE, UNIT_NPC_FLAG_2_GARRISON_ARCHITECT))
        return;

    if (Garrison* garrison = _player->GetGarrison())
        garrison->PlaceBuilding(garrisonPurchaseBuilding.PlotInstanceID, garrisonPurchaseBuilding.BuildingID);
}

void WorldSession::HandleGarrisonCancelConstruction(WorldPackets::Garrison::GarrisonCancelConstruction& garrisonCancelConstruction)
{
    if (!_player->GetNPCIfCanInteractWith(garrisonCancelConstruction.NpcGUID, UNIT_NPC_FLAG_NONE, UNIT_NPC_FLAG_2_GARRISON_ARCHITECT))
        return;

    if (Garrison* garrison = _player->GetGarrison())
        garrison->CancelBuildingConstruction(garrisonCancelConstruction.PlotInstanceID);
}

void WorldSession::HandleGarrisonRequestBlueprintAndSpecializationData(WorldPackets::Garrison::GarrisonRequestBlueprintAndSpecializationData& /*garrisonRequestBlueprintAndSpecializationData*/)
{
    if (Garrison* garrison = _player->GetGarrison())
        garrison->SendBlueprintAndSpecializationData();
}

void WorldSession::HandleGarrisonGetMapData(WorldPackets::Garrison::GarrisonGetMapData& /*garrisonGetMapData*/)
{
    if (Garrison* garrison = _player->GetGarrison())
        garrison->SendMapData(_player);
}

void WorldSession::HandleGarrisonCheckUpgradeable(WorldPackets::Garrison::GarrisonCheckUpgradeable& garrisonCheckUpgradeable)
{
    WorldPackets::Garrison::GarrisonIsUpgradeableResponse isUpgradeableResponse;
    isUpgradeableResponse.Result = GARRISON_ERROR_NO_GARRISON;
    if (Garrison* garrison = _player->GetGarrison())
    {
        if (garrison->GetSiteLevel()->GarrSiteID == uint32(garrisonCheckUpgradeable.GarrSiteID))
            isUpgradeableResponse.Result = garrison->CanUpgrade(true);
        else
            isUpgradeableResponse.Result = GARRISON_ERROR_INVALID_SITE_ID;
    }

    SendPacket(isUpgradeableResponse.Write());
}

void WorldSession::HandleUpgradeGarrison(WorldPackets::Garrison::UpgradeGarrison& upgradeGarrison)
{
    if (!_player->GetNPCIfCanInteractWith(upgradeGarrison.NpcGUID, UNIT_NPC_FLAG_NONE, UNIT_NPC_FLAG_2_GARRISON_ARCHITECT))
        return;

    if (Garrison* garrison = _player->GetGarrison())
        garrison->Upgrade(true);
}

void WorldSession::HandleActivateSoulbind(WorldPackets::Garrison::ActivateSoulbind& activateSoulbind)
{
    CovenantGarrison* covenantGarrison = _player->GetCovenantGarrison();
    if (!covenantGarrison)
        return;

    // SMSG_ACTIVATE_SOULBIND_FAILED was never captured, its layout is unknown: a refused activation is only logged
    if (!covenantGarrison->ActivateSoulbind(activateSoulbind.SoulbindID))
        TC_LOG_DEBUG("network", "WorldSession::HandleActivateSoulbind: {} cannot activate soulbind {}", _player->GetGUID().ToString(), activateSoulbind.SoulbindID);
}

void WorldSession::HandleCovenantRenownRequestCatchupState(WorldPackets::Garrison::CovenantRenownRequestCatchupState& /*covenantRenownRequestCatchupState*/)
{
    if (!CovenantRenown::IsRenownEnabled())
        return;

    WorldPackets::Garrison::CovenantRenownSendCatchupState catchupState;
    catchupState.CatchupState = CovenantRenown::GetCatchupState(_player);
    SendPacket(catchupState.Write());
}

void WorldSession::HandleRequestCovenantCallings(WorldPackets::Garrison::RequestCovenantCallings& /*requestCovenantCallings*/)
{
    if (!CovenantRenown::IsCallingsEnabled())
        return;

    // retail lists the BountyIDs of the callings on offer that the player does not carry (nov18b idx 186014: Bounty 185 = quest 60391, right after the
    // quest was dropped); which callings are on offer each day is server data that no capture shows, so no calling is listed
    WorldPackets::Garrison::CovenantCallingsAvailabilityResponse callings;
    callings.AreCallingsUnlocked = CovenantRenown::AreCallingsUnlocked(_player);
    SendPacket(callings.Write());
}

void WorldSession::HandleGarrisonResearchTalent(WorldPackets::Garrison::GarrisonResearchTalent& garrisonResearchTalent)
{
    CovenantGarrison* covenantGarrison = _player->GetCovenantGarrison();
    if (!covenantGarrison)
        return;

    // the soulbind trees are researched without an npc (empty guid, Forge of Bonds); npc research is the sanctum, not handled
    if (!garrisonResearchTalent.NPCTalentGuid.IsEmpty())
        return;

    if (!covenantGarrison->ResearchTalent(garrisonResearchTalent.GarrTalentID))
        TC_LOG_DEBUG("network", "WorldSession::HandleGarrisonResearchTalent: {} cannot research garrison talent {}", _player->GetGUID().ToString(), garrisonResearchTalent.GarrTalentID);
}

void WorldSession::HandleGarrisonLearnTalent(WorldPackets::Garrison::GarrisonLearnTalent& garrisonLearnTalent)
{
    CovenantGarrison* covenantGarrison = _player->GetCovenantGarrison();
    if (!covenantGarrison)
        return;

    if (!covenantGarrison->LearnTalent(garrisonLearnTalent.GarrTalentID))
        TC_LOG_DEBUG("network", "WorldSession::HandleGarrisonLearnTalent: {} cannot learn garrison talent {}", _player->GetGUID().ToString(), garrisonLearnTalent.GarrTalentID);
}

void WorldSession::HandleGarrisonSocketTalent(WorldPackets::Garrison::GarrisonSocketTalent& garrisonSocketTalent)
{
    CovenantGarrison* covenantGarrison = _player->GetCovenantGarrison();
    if (!covenantGarrison)
        return;

    std::vector<std::pair<int32, int32>> sockets;
    sockets.reserve(garrisonSocketTalent.Sockets.size());
    for (WorldPackets::Garrison::GarrisonSocketTalentEntry const& socket : garrisonSocketTalent.Sockets)
        sockets.emplace_back(socket.GarrTalentID, socket.SoulbindConduitID);

    covenantGarrison->SocketTalents(garrisonSocketTalent.GarrTalentTreeID, sockets);
}

void WorldSession::HandleOpenMissionNpc(WorldPackets::Garrison::OpenMissionNpc& openMissionNpc)
{
    // retail sends no reply to the open itself (nov18c idx 182532-182545); the interaction was announced by the gossip option
    // (SMSG_GOSSIP_OPTION_NPC_INTERACTION, GossipNpcOption 31295 on the Command Table)
    if (!CovenantGarrison::AreMissionsEnabled())
        return;

    TC_LOG_DEBUG("network", "WorldSession::HandleOpenMissionNpc: {} opens {} (follower type {})", _player->GetGUID().ToString(), openMissionNpc.NpcGUID.ToString(), openMissionNpc.GarrFollowerTypeID);
}

void WorldSession::HandleGarrisonGetClassSpecCategoryInfo(WorldPackets::Garrison::GarrisonGetClassSpecCategoryInfo& getClassSpecCategoryInfo)
{
    if (!CovenantGarrison::AreMissionsEnabled() || getClassSpecCategoryInfo.GarrFollowerTypeID != CovenantGarrison::FOLLOWER_TYPE_COVENANT_SANCTUM)
        return;

    WorldPackets::Garrison::GarrisonGetClassSpecCategoryInfoResult result;
    result.GarrFollowerTypeID = getClassSpecCategoryInfo.GarrFollowerTypeID;
    SendPacket(result.Write());
}

void WorldSession::HandleGarrisonStartMission(WorldPackets::Garrison::GarrisonStartMission& startMission)
{
    CovenantGarrison* covenantGarrison = _player->GetCovenantGarrison();
    if (!covenantGarrison || !CovenantGarrison::AreMissionsEnabled())
        return;

    if (!_player->GetNPCIfCanInteractWith(startMission.NpcGUID, UNIT_NPC_FLAG_NONE, UNIT_NPC_FLAG_2_NONE))
        return;

    covenantGarrison->StartMission(startMission);
}

void WorldSession::HandleGarrisonCompleteMission(WorldPackets::Garrison::GarrisonCompleteMission& completeMission)
{
    CovenantGarrison* covenantGarrison = _player->GetCovenantGarrison();
    if (!covenantGarrison || !CovenantGarrison::AreMissionsEnabled())
        return;

    if (!_player->GetNPCIfCanInteractWith(completeMission.NpcGUID, UNIT_NPC_FLAG_NONE, UNIT_NPC_FLAG_2_NONE))
        return;

    covenantGarrison->CompleteMission(completeMission.MissionRecID);
}

void WorldSession::HandleGarrisonMissionBonusRoll(WorldPackets::Garrison::GarrisonMissionBonusRoll& missionBonusRoll)
{
    CovenantGarrison* covenantGarrison = _player->GetCovenantGarrison();
    if (!covenantGarrison || !CovenantGarrison::AreMissionsEnabled())
        return;

    if (!_player->GetNPCIfCanInteractWith(missionBonusRoll.NpcGUID, UNIT_NPC_FLAG_NONE, UNIT_NPC_FLAG_2_NONE))
        return;

    covenantGarrison->MissionBonusRoll(missionBonusRoll.MissionRecID);
}
