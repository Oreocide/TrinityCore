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
}
