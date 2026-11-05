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

#ifndef TRINITYCORE_HOUSING_PACKETS_H
#define TRINITYCORE_HOUSING_PACKETS_H

#include "ObjectGuid.h"
#include "Packet.h"

namespace WorldPackets::Housing
{
    class DeclineNeighborhoodInvites final : public ClientPacket
    {
    public:
        explicit DeclineNeighborhoodInvites(WorldPacket&& packet) : ClientPacket(CMSG_DECLINE_NEIGHBORHOOD_INVITES, std::move(packet)) { }

        void Read() override;

        bool Allow = false;
    };

    class GetPlayerHousesInfo final : public ClientPacket
    {
    public:
        explicit GetPlayerHousesInfo(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_SVCS_GET_PLAYER_HOUSES_INFO, std::move(packet)) { }

        void Read() override { }
    };

    // Houses are not implemented: only the zero-house reply (retail bytes 00 00 00 00 00) is sent.
    class GetPlayerHousesInfoResponse final : public ServerPacket
    {
    public:
        GetPlayerHousesInfoResponse() : ServerPacket(SMSG_HOUSING_SVCS_GET_PLAYER_HOUSES_INFO_RESPONSE, 5) { }

        WorldPacket const* Write() override;

        uint8 Result = 0; // 0 = Success
    };

    class StartTutorial final : public ClientPacket
    {
    public:
        explicit StartTutorial(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_SVCS_START_TUTORIAL, std::move(packet)) { }

        void Read() override { }
    };

    // zero-state login requests, answered with the retail bytes of a character without houses
    class HousingDecorRequestStorage final : public ClientPacket
    {
    public:
        explicit HousingDecorRequestStorage(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_DECOR_REQUEST_STORAGE, std::move(packet)) { }

        void Read() override;

        ObjectGuid BnetAccountGUID;
    };

    class HousingBlueprintRequestCollection final : public ClientPacket
    {
    public:
        explicit HousingBlueprintRequestCollection(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_BLUEPRINT_REQUEST_COLLECTION, std::move(packet)) { }

        void Read() override { }
    };

    class HousingPhotoSharingCompleteAuthorization final : public ClientPacket
    {
    public:
        explicit HousingPhotoSharingCompleteAuthorization(WorldPacket&& packet) : ClientPacket(CMSG_HOUSING_PHOTO_SHARING_COMPLETE_AUTHORIZATION, std::move(packet)) { }

        void Read() override;

        uint8 Field_00 = 0; // unknown, 1 byte, seen 0
    };

    class GetDecorRefundList final : public ClientPacket
    {
    public:
        explicit GetDecorRefundList(WorldPacket&& packet) : ClientPacket(CMSG_GET_DECOR_REFUND_LIST, std::move(packet)) { }

        void Read() override { }
    };

    class GetAllLicensedDecorQuantities final : public ClientPacket
    {
    public:
        explicit GetAllLicensedDecorQuantities(WorldPacket&& packet) : ClientPacket(CMSG_GET_ALL_LICENSED_DECOR_QUANTITIES, std::move(packet)) { }

        void Read() override { }
    };

    class GetPlayerInitiativeInfoRequest final : public ClientPacket
    {
    public:
        explicit GetPlayerInitiativeInfoRequest(WorldPacket&& packet) : ClientPacket(CMSG_GET_PLAYER_INITIATIVE_INFO_REQUEST, std::move(packet)) { }

        void Read() override;

        uint16 Field_00 = 0; // unknown, 2 bytes, seen 00 00
    };

    // fixed-layout reply for opcodes whose layout is only known for the empty state
    class HousingFixedResponse final : public ServerPacket
    {
    public:
        HousingFixedResponse(OpcodeServer opcode, std::initializer_list<uint8> bytes) : ServerPacket(opcode, bytes.size()), _bytes(bytes) { }

        WorldPacket const* Write() override;

    private:
        std::vector<uint8> _bytes;
    };

    class NeighborhoodInitiativeServiceStatusCheck final : public ClientPacket
    {
    public:
        explicit NeighborhoodInitiativeServiceStatusCheck(WorldPacket&& packet) : ClientPacket(CMSG_NEIGHBORHOOD_INITIATIVE_SERVICE_STATUS_CHECK, std::move(packet)) { }

        void Read() override { }
    };

    class InitiativeServiceStatus final : public ServerPacket
    {
    public:
        InitiativeServiceStatus() : ServerPacket(SMSG_INITIATIVE_SERVICE_STATUS, 1) { }

        WorldPacket const* Write() override;

        bool ServiceEnabled = true;
    };
}

#endif // TRINITYCORE_HOUSING_PACKETS_H
