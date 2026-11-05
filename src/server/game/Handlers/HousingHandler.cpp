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
#include "Config.h"
#include "DB2Stores.h"
#include "HousingPackets.h"
#include "Log.h"
#include "Player.h"
#include "SpellMgr.h"

enum HousingTutorialSpells : uint32
{
    SPELL_TELEPORT_TO_FOUNDERS_POINT    = 1258476, // Alliance, 10s cast, TELEPORT_UNITS to DEST_DB + force cast "Set Housing Return Location"
    SPELL_TELEPORT_TO_RAZORWIND_SHORES  = 1258484  // Horde, destination not sniffed yet
};

void WorldSession::HandleDeclineNeighborhoodInvites(WorldPackets::Housing::DeclineNeighborhoodInvites const& declineNeighborhoodInvites)
{
    if (declineNeighborhoodInvites.Allow)
        GetPlayer()->SetPlayerFlagEx(PLAYER_FLAGS_EX_AUTO_DECLINE_NEIGHBORHOOD);
    else
        GetPlayer()->RemovePlayerFlagEx(PLAYER_FLAGS_EX_AUTO_DECLINE_NEIGHBORHOOD);
}

// Housing is not implemented. With Housing.Service.Enable the client treats the service as available and
// asks for state on login; answer with the retail "no houses" state so the dashboard shows its empty frame.
void WorldSession::HandleGetPlayerHousesInfo(WorldPackets::Housing::GetPlayerHousesInfo const& /*getPlayerHousesInfo*/)
{
    if (!sConfigMgr->GetBoolDefault("Housing.Service.Enable"sv, false))
        return;

    WorldPackets::Housing::GetPlayerHousesInfoResponse response;
    SendPacket(response.Write());
}

void WorldSession::HandleNeighborhoodInitiativeServiceStatusCheck(WorldPackets::Housing::NeighborhoodInitiativeServiceStatusCheck const& /*statusCheck*/)
{
    if (!sConfigMgr->GetBoolDefault("Housing.Service.Enable"sv, false))
        return;

    WorldPackets::Housing::InitiativeServiceStatus status;
    SendPacket(status.Write());
}

namespace
{
// The client sends the plot index (0..54 within a neighborhood), not a NeighborhoodPlot row id.
NeighborhoodPlotEntry const* FindNeighborhoodPlot(uint32 mapId, uint32 plotIndex)
{
    NeighborhoodMapEntry const* neighborhood = nullptr;
    for (NeighborhoodMapEntry const* entry : sNeighborhoodMapStore)
    {
        if (uint32(entry->MapID) == mapId)
        {
            neighborhood = entry;
            break;
        }
    }

    if (!neighborhood)
        return nullptr;

    for (NeighborhoodPlotEntry const* plot : sNeighborhoodPlotStore)
        if (uint32(plot->NeighborhoodMapID) == neighborhood->ID && uint32(plot->PlotIndex) == plotIndex)
            return plot;

    return nullptr;
}
}

// Plot sign (GO type 48, UILinkType 4) click: GameObject::Use sends interaction 70, then the client asks for the plot window.
// Only the unowned-plot reply (retail sample, 30 bytes) is implemented; the price shown by the client comes from NeighborhoodPlot.Cost.
void WorldSession::HandleNeighborhoodOpenCornerstoneUI(WorldPackets::Housing::OpenCornerstoneUI const& openCornerstoneUI)
{
    if (!sConfigMgr->GetBoolDefault("Housing.Service.Enable"sv, false))
        return;

    NeighborhoodPlotEntry const* plot = FindNeighborhoodPlot(_player->GetMapId(), openCornerstoneUI.PlotIndex);
    TC_LOG_INFO("server.housing", "OPEN_CORNERSTONE_UI from {}: plot index {} cornerstone {} map {} -> plot row {}, cost {}", GetPlayerInfo(),
        openCornerstoneUI.PlotIndex, openCornerstoneUI.CornerstoneGUID.ToString(), _player->GetMapId(), plot ? plot->ID : 0, plot ? plot->Cost : 0);
    if (!plot)
        return;

    WorldPackets::Housing::OpenCornerstoneUIResponse response;
    response.PlotIndex = openCornerstoneUI.PlotIndex;
    response.Name = "66-71-65"; // TEMP: constant in all 5 retail samples, meaning unknown
    WorldPacket const* data = response.Write();
    TC_LOG_INFO("server.housing", "OPEN_CORNERSTONE_UI_RESPONSE {} bytes: {}", data->size(), ByteArrayToHexStr(std::span<uint8 const>(data->data(), data->size())));
    SendPacket(data);
}

// Dashboard "Start Tutorial": retail answers with a server-side cast of the neighborhood teleport (sniff: no CMSG_CAST_SPELL).
void WorldSession::HandleHousingSvcsStartTutorial(WorldPackets::Housing::StartTutorial const& /*startTutorial*/)
{
    if (!sConfigMgr->GetBoolDefault("Housing.Service.Enable"sv, false))
        return;

    uint32 spellId = _player->GetTeam() == ALLIANCE ? SPELL_TELEPORT_TO_FOUNDERS_POINT : SPELL_TELEPORT_TO_RAZORWIND_SHORES;
    if (!sSpellMgr->GetSpellTargetPosition(spellId, EFFECT_0))
    {
        TC_LOG_WARN("network", "WorldSession::HandleHousingSvcsStartTutorial: no spell_target_position for spell {}, {} cannot start the housing tutorial",
            spellId, GetPlayerInfo());
        return;
    }

    _player->CastSpell(_player, spellId, false);
}

namespace
{
bool HousingServiceEnabled()
{
    return sConfigMgr->GetBoolDefault("Housing.Service.Enable"sv, false);
}
}

// The following answer the zero-state login requests with the bytes retail sent to a character without houses
// (sniff 12.1.5.70529/70575), so the client does not keep housing requests pending.
void WorldSession::HandleHousingDecorRequestStorage(WorldPackets::Housing::HousingDecorRequestStorage const& /*request*/)
{
    if (!HousingServiceEnabled())
        return;

    SendPacket(WorldPackets::Housing::HousingFixedResponse(SMSG_HOUSING_DECOR_REQUEST_STORAGE_RESPONSE, { 0x00, 0x00, 0x00, 0x80 }).Write());
}

void WorldSession::HandleHousingBlueprintRequestCollection(WorldPackets::Housing::HousingBlueprintRequestCollection const& /*request*/)
{
    if (!HousingServiceEnabled())
        return;

    SendPacket(WorldPackets::Housing::HousingFixedResponse(SMSG_HOUSING_BLUEPRINT_GET_RESPONSE, { 0x00, 0x00, 0x00, 0x00, 0x00 }).Write());
}

void WorldSession::HandleHousingPhotoSharingCompleteAuthorization(WorldPackets::Housing::HousingPhotoSharingCompleteAuthorization const& /*request*/)
{
    if (!HousingServiceEnabled())
        return;

    SendPacket(WorldPackets::Housing::HousingFixedResponse(SMSG_HOUSING_PHOTO_SHARING_AUTHORIZATION_RESULT, { 0x01, 0x00 }).Write());
}

void WorldSession::HandleGetDecorRefundList(WorldPackets::Housing::GetDecorRefundList const& /*request*/)
{
    if (!HousingServiceEnabled())
        return;

    SendPacket(WorldPackets::Housing::HousingFixedResponse(SMSG_GET_DECOR_REFUND_LIST_RESPONSE, { 0x00, 0x00, 0x00, 0x00 }).Write());
}

void WorldSession::HandleGetAllLicensedDecorQuantities(WorldPackets::Housing::GetAllLicensedDecorQuantities const& /*request*/)
{
    if (!HousingServiceEnabled())
        return;

    SendPacket(WorldPackets::Housing::HousingFixedResponse(SMSG_GET_ALL_LICENSED_DECOR_QUANTITIES_RESPONSE, { 0x00, 0x00, 0x00, 0x00 }).Write());
}

void WorldSession::HandleGetPlayerInitiativeInfoRequest(WorldPackets::Housing::GetPlayerInitiativeInfoRequest const& /*request*/)
{
    if (!HousingServiceEnabled())
        return;

    SendPacket(WorldPackets::Housing::HousingFixedResponse(SMSG_GET_PLAYER_INITIATIVE_INFO_RESULT, { 0x00, 0x00, 0x00 }).Write());
}
