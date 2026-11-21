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
#include "DB2Structure.h"
#include "Garrison.h"
#include "GarrisonPackets.h"
#include "Log.h"
#include "Player.h"

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
