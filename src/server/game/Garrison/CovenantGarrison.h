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
#include "GarrisonPackets.h"
#include <map>
#include <vector>

class Player;
struct GarrTalentEntry;
struct SoulbindEntry;

struct CovenantFollowerTemplate;
struct CovenantMissionTemplate;
struct CovenantMissionReward;
struct GarrMissionEntry;

namespace WorldPackets::Garrison
{
    class GetGarrisonInfoResult;
    class GarrisonStartMission;
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
 *  - followers (heroes, type 123) and missions of the command table (CovenantGarrison.Missions.Enable): the mission lifecycle
 *    Offered (0) -> InProgress (1) -> Complete (2) -> Rewarded (6), persisted in character_covenant_follower / _mission / _mission_follower.
 *    There is no auto-combat (M6): a mission has a fixed result, followers take no damage.
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

    enum MissionState : int32
    {
        MISSION_STATE_OFFERED       = 0,
        MISSION_STATE_IN_PROGRESS   = 1,
        MISSION_STATE_COMPLETE      = 2,    // finished, rewards not yet rolled
        MISSION_STATE_REWARDED      = 6     // state of SMSG_GARRISON_MISSION_BONUS_ROLL_RESULT; the row stays as the record that it was done
    };

    static constexpr uint8 FOLLOWER_TYPE_COVENANT_SANCTUM = 123;
    static constexpr uint32 FOLLOWER_STATUS_ON_MISSION = 0x20;      // FollowerStatus of the auto troops of a started mission in every capture
    static constexpr uint64 DBID_BASE = 1000000000ULL;              // type-111 DbIDs start above the WoD ones

    struct MissionFollower
    {
        uint64 DbID = 0;
        uint32 GarrFollowerID = 0;
        bool IsTroop = false;
        uint32 Level = 1;
        int32 BoardIndex = 0;
        int32 Health = 0;
    };

    struct Mission
    {
        WorldPackets::Garrison::GarrisonMission PacketInfo;     // rewards stay empty here, they come from the template
        std::vector<MissionFollower> Followers;
    };

    explicit CovenantGarrison(Player* owner);

    void LoadFromDB(PreparedQueryResult specResult, PreparedQueryResult talentResult, PreparedQueryResult collectionResult,
        PreparedQueryResult followerResult, PreparedQueryResult missionResult, PreparedQueryResult missionFollowerResult);
    void SaveToDB(CharacterDatabaseTransaction trans);
    static void DeleteFromDB(uint64 guid, CharacterDatabaseTransaction trans);

    // sets PlayerData.SoulbindID from the spec group of the current specialization
    void OnLogin();

    void BuildInfoPacket(WorldPackets::Garrison::GarrisonInfo& info) const;
    // appends the type-111 entry and the follower soft caps retail sends with it
    void AppendToInfoResult(WorldPackets::Garrison::GetGarrisonInfoResult& result) const;
    // unsolicited SMSG_GET_GARRISON_INFO_RESULT (retail pushes one when the covenant is joined)
    void SendInfo();

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

    // CovenantGarrison.Missions.Enable (and the two keys above)
    static bool AreMissionsEnabled();

    // SPELL_EFFECT_ADD_GARRISON_FOLLOWER for a type-111 follower: adds the hero (capture-backed templates only)
    bool AddFollower(uint32 garrFollowerId);
    WorldPackets::Garrison::GarrisonFollower const* GetFollower(uint64 dbId) const;

    // CMSG_GARRISON_START_MISSION / CMSG_GARRISON_COMPLETE_MISSION / CMSG_GARRISON_MISSION_BONUS_ROLL (the caller checked the npc)
    void StartMission(WorldPackets::Garrison::GarrisonStartMission const& startMission);
    void CompleteMission(int32 missionRecId);
    void MissionBonusRoll(int32 missionRecId);

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

    // missions and followers
    static void DeleteSoulbindRows(uint64 guid, CharacterDatabaseTransaction trans);
    static void DeleteMissionRows(uint64 guid, CharacterDatabaseTransaction trans);
    uint64 GenerateDbId() { return ++_lastDbId; }
    Mission* GetMission(int32 missionRecId);
    Mission const* GetMission(int32 missionRecId) const;
    bool HasMission(uint32 missionId) const;
    bool CanStartMission(Mission const& mission) const;
    uint32 GetMinHeroLevel() const;
    uint32 GetMaxHeroLevel() const;
    static uint32 GetXpToNextLevel(uint32 level);
    void FillMissionReward(CovenantMissionReward const& reward, WorldPackets::Garrison::GarrisonMissionReward& packetReward) const;
    // creates the mission as offered; send = also SMSG_GARRISON_ADD_MISSION_RESULT
    Mission* OfferMission(uint32 missionId, bool send);
    // offers every mission the chain names after prevMissionId (0 = the first ones) that was never offered before
    void OfferChain(uint32 prevMissionId, bool send);
    // drops offered missions that ran out; true if any
    bool RemoveExpiredMissions();
    void SendStartConditionUpdate() const;
    void GiveFollowerXp(WorldPackets::Garrison::GarrisonFollower& follower, uint32 xp, time_t completionTime);
    void GrantMissionRewards(CovenantMissionTemplate const& missionTemplate);
    void SendStartMissionError(int32 error) const;
    // the first mission(s) of the chain (covenant_mission_chain PrevMissionID 0), once the player is in a covenant
    void OfferInitialMissions(bool send);

    Player* _owner;
    std::map<int32 /*ChrSpecializationID*/, int32 /*SoulbindID*/> _specGroups;
    std::map<int32 /*GarrTalentID*/, Talent> _talents;
    std::map<std::pair<int32 /*type*/, int32 /*entry*/>, int32 /*rank*/> _collections;
    bool _needsSave = false;

    std::map<uint64 /*DbID*/, WorldPackets::Garrison::GarrisonFollower> _followers;
    std::map<uint64 /*DbID*/, Mission> _missions;
    uint64 _lastDbId = DBID_BASE;
    uint16 _sessionMissionCount = 0;
    bool _missionsNeedSave = false;
};

#endif // TRINITYCORE_COVENANT_GARRISON_H
