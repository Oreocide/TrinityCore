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

#include "HousingPackets.h"
#include "PacketOperators.h"

namespace WorldPackets::Housing
{
void DeclineNeighborhoodInvites::Read()
{
    _worldPacket >> Bits<1>(Allow);
}

void HousingDecorRequestStorage::Read()
{
    _worldPacket >> BnetAccountGUID;
}

void HousingPhotoSharingCompleteAuthorization::Read()
{
    _worldPacket >> Field_00;
}

void GetPlayerInitiativeInfoRequest::Read()
{
    _worldPacket >> Field_00;
}

WorldPacket const* HousingFixedResponse::Write()
{
    for (uint8 byte : _bytes)
        _worldPacket << byte;

    return &_worldPacket;
}

WorldPacket const* GetPlayerHousesInfoResponse::Write()
{
    _worldPacket << uint32(0); // PlayerHousesCount
    _worldPacket << uint8(Result);

    return &_worldPacket;
}

WorldPacket const* InitiativeServiceStatus::Write()
{
    _worldPacket << Bits<1>(ServiceEnabled);
    _worldPacket.FlushBits();

    return &_worldPacket;
}

void OpenCornerstoneUI::Read()
{
    _worldPacket >> PlotIndex;
    _worldPacket >> CornerstoneGUID;
}

WorldPacket const* OpenCornerstoneUIResponse::Write()
{
    _worldPacket << uint32(PlotIndex);
    _worldPacket << HouseGUID;
    _worldPacket << OwnerGUID;
    _worldPacket << uint64(Field_48);
    _worldPacket << uint8(Field_50);
    _worldPacket << Field_80;
    // 15 flag bits as read by the client: bool, 8-bit string length (incl. NUL), optional u64, bool, optional struct, bool, optional u32, bool
    _worldPacket << Bits<1>(Flag_51);
    _worldPacket << Bits<8>(Name.size() + 1);
    _worldPacket << Bits<1>(0);
    _worldPacket << Bits<1>(Flag_A0);
    _worldPacket << Bits<1>(0);
    _worldPacket << Bits<1>(Flag_100);
    _worldPacket << Bits<1>(0);
    _worldPacket << Bits<1>(Flag_10C);
    _worldPacket.FlushBits();
    _worldPacket.append(Name.data(), Name.size());
    _worldPacket << uint8(0);

    return &_worldPacket;
}
}
