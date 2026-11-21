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

#ifndef TRINITYCORE_COVENANT_GARRISON_H
#define TRINITYCORE_COVENANT_GARRISON_H

#include "DatabaseEnvFwd.h"
#include "Define.h"
#include <map>
#include <vector>

class Player;
struct GarrTalentEntry;
struct SoulbindEntry;

namespace WorldPackets::Garrison
{
    class GetGarrisonInfoResult;
    struct GarrisonInfo;
    struct GarrisonTalent;
}

enum CovenantGarrisonTalentFlags : int32
{
    COVENANT_TALENT_FLAG_RESEARCHED     = 0x1,  // GarrisonTalentFlag::Researched; for soulbind nodes also "the node is selected" (a node that was switched away from keeps its rank with this bit cleared)
    COVENANT_TALENT_FLAG_RESPEC         = 0x2,
    COVENANT_TALENT_FLAG_RESEARCHING    = 0x4,
    COVENANT_TALENT_FLAG_ONGOING        = 0x8,
    COVENANT_TALENT_FLAG_DONT_RESET     = 0x10
};

enum CovenantGarrisonCollectionType : int32
{
    COVENANT_COLLECTION_CONDUITS        = 1
};

/*
 * The Shadowlands covenant garrison (GarrType 111): what retail keeps in the one type-111 entry of SMSG_GET_GARRISON_INFO_RESULT.
 * It is separate from the WoD Garrison class (plots, buildings, blueprints). This part holds the soulbind data:
 *  - spec groups: ChrSpecialization -> active Soulbind
 *  - talents: the nodes of the soulbind trees (GarrTalent rows of a tree that a Soulbind row points to) with their conduit sockets
 *  - collection type 1: the conduits the player owns, with their rank
 * Only created when Covenant.Enable and CovenantGarrison.Enable are both set.
 */
class TC_GAME_API CovenantGarrison
{
public:
    static constexpr uint8 GARR_TYPE_ID = 111;
    static constexpr uint32 GARR_SITE_ID = 296;         // as sent by retail in every type-111 entry
    static constexpr uint32 GARR_SITE_LEVEL_ID = 837;   // idem

    struct Talent
    {
        int32 Rank = 0;
        int64 ResearchStartTime = 0;
        int32 Flags = 0;
        int32 SocketConduitId = 0;      // 0 = empty socket
        int32 SocketConduitRank = 0;    // SoulbindConduitRank.RankIndex
    };

    explicit CovenantGarrison(Player* owner);

    void LoadFromDB(PreparedQueryResult specResult, PreparedQueryResult talentResult, PreparedQueryResult collectionResult);
    void SaveToDB(CharacterDatabaseTransaction trans);
    static void DeleteFromDB(uint64 guid, CharacterDatabaseTransaction trans);

    // sets PlayerData.SoulbindID from the spec group of the current specialization
    void OnLogin();

    void BuildInfoPacket(WorldPackets::Garrison::GarrisonInfo& info) const;
    // appends the type-111 entry and the follower soft caps retail sends with it
    void AppendToInfoResult(WorldPackets::Garrison::GetGarrisonInfoResult& result) const;
    // unsolicited SMSG_GET_GARRISON_INFO_RESULT (retail pushes one when the covenant is joined)
    void SendInfo() const;

    // CMSG_ACTIVATE_SOULBIND
    bool ActivateSoulbind(int32 soulbindId);
    // CMSG_GARRISON_RESEARCH_TALENT with an empty npc guid (soulbind trees only)
    bool ResearchTalent(int32 garrTalentId);
    // CMSG_GARRISON_LEARN_TALENT: picks another branch of a soulbind column
    bool LearnTalent(int32 garrTalentId);
    // CMSG_GARRISON_SOCKET_TALENT
    void SocketTalents(int32 garrTalentTreeId, std::vector<std::pair<int32, int32>> const& sockets);

    // SPELL_EFFECT_LEARN_SOULBIND_CONDUIT: adds the conduit to the collection at rank 1
    bool AddConduit(int32 conduitId);
    uint32 CountConduitsAtRank(int32 rank) const;
    bool HasConduitAtRank(int32 conduitId, int32 rank) const;

    int32 GetActiveSoulbind() const;

private:
    // the soulbind that owns the tree of this talent, only if it belongs to the player's covenant
    SoulbindEntry const* GetOwnedSoulbind(GarrTalentEntry const* garrTalent) const;
    // spell a researched node teaches: GarrTalentRank.PerkSpellID of its rank, and the spell of the conduit in its socket
    uint32 GetPerkSpell(int32 garrTalentId, Talent const& talent) const;
    static uint32 GetConduitSpell(Talent const& talent);
    bool IsTreeActive(int32 garrTalentTreeId) const;
    void LearnTalentSpells(int32 garrTalentId, Talent const& talent);
    void UnlearnTalentSpells(int32 garrTalentId, Talent const& talent);
    void LearnTreeSpells(int32 garrTalentTreeId);
    void UnlearnTreeSpells(int32 garrTalentTreeId);
    void FillTalentPacket(int32 garrTalentId, Talent const& talent, WorldPackets::Garrison::GarrisonTalent& packetTalent) const;
    // clears the node: unlearns its spells, empties its socket, clears the selected bit
    void DeactivateTalent(int32 garrTalentId, Talent& talent);

    Player* _owner;
    std::map<int32 /*ChrSpecializationID*/, int32 /*SoulbindID*/> _specGroups;
    std::map<int32 /*GarrTalentID*/, Talent> _talents;
    std::map<std::pair<int32 /*type*/, int32 /*entry*/>, int32 /*rank*/> _collections;
    bool _needsSave = false;
};

#endif // TRINITYCORE_COVENANT_GARRISON_H
