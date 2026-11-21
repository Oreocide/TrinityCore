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

#include "CovenantGarrison.h"
#include "CharacterDatabase.h"
#include "ConditionMgr.h"
#include "DB2Stores.h"
#include "DB2Structure.h"
#include "DatabaseEnv.h"
#include "Garrison.h"
#include "GarrisonPackets.h"
#include "GameTime.h"
#include "Log.h"
#include "Player.h"
#include <algorithm>

CovenantGarrison::CovenantGarrison(Player* owner) : _owner(owner)
{
}

void CovenantGarrison::LoadFromDB(PreparedQueryResult specResult, PreparedQueryResult talentResult, PreparedQueryResult collectionResult)
{
    if (specResult)
    {
        do
        {
            Field* fields = specResult->Fetch();
            _specGroups[fields[0].GetInt32()] = fields[1].GetInt32();
        }
        while (specResult->NextRow());
    }

    if (talentResult)
    {
        do
        {
            Field* fields = talentResult->Fetch();
            if (fields[0].GetInt32() != GARR_TYPE_ID)
                continue;

            int32 garrTalentId = fields[1].GetInt32();
            if (!sGarrTalentStore.LookupEntry(garrTalentId))
            {
                TC_LOG_ERROR("entities.player", "Player {} has an unknown garrison talent {} in character_garrison_talent, skipped", _owner->GetGUID().ToString(), garrTalentId);
                continue;
            }

            Talent& talent = _talents[garrTalentId];
            talent.Rank = fields[2].GetInt32();
            talent.ResearchStartTime = fields[3].GetInt64();
            talent.Flags = fields[4].GetInt32();
            talent.SocketConduitId = fields[5].GetInt32();
            talent.SocketConduitRank = fields[6].GetInt32();
        }
        while (talentResult->NextRow());
    }

    if (collectionResult)
    {
        do
        {
            Field* fields = collectionResult->Fetch();
            if (fields[0].GetInt32() != GARR_TYPE_ID)
                continue;

            _collections[{ fields[1].GetInt32(), fields[2].GetInt32() }] = fields[3].GetInt32();
        }
        while (collectionResult->NextRow());
    }
}

void CovenantGarrison::DeleteFromDB(uint64 guid, CharacterDatabaseTransaction trans)
{
    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHARACTER_COVENANT_SOULBIND_SPEC);
    stmt->setUInt64(0, guid);
    trans->Append(stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHARACTER_GARRISON_TALENT);
    stmt->setUInt64(0, guid);
    trans->Append(stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHARACTER_GARRISON_COLLECTION);
    stmt->setUInt64(0, guid);
    trans->Append(stmt);
}

void CovenantGarrison::SaveToDB(CharacterDatabaseTransaction trans)
{
    if (!_needsSave)
        return;

    _needsSave = false;

    uint64 guid = _owner->GetGUID().GetCounter();
    DeleteFromDB(guid, trans);

    for (auto const& [specId, soulbindId] : _specGroups)
    {
        CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_CHARACTER_COVENANT_SOULBIND_SPEC);
        stmt->setUInt64(0, guid);
        stmt->setInt32(1, specId);
        stmt->setInt32(2, soulbindId);
        trans->Append(stmt);
    }

    for (auto const& [garrTalentId, talent] : _talents)
    {
        CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_CHARACTER_GARRISON_TALENT);
        stmt->setUInt64(0, guid);
        stmt->setInt32(1, GARR_TYPE_ID);
        stmt->setInt32(2, garrTalentId);
        stmt->setInt32(3, talent.Rank);
        stmt->setInt64(4, talent.ResearchStartTime);
        stmt->setInt32(5, talent.Flags);
        stmt->setInt32(6, talent.SocketConduitId);
        stmt->setInt32(7, talent.SocketConduitRank);
        trans->Append(stmt);
    }

    for (auto const& [key, rank] : _collections)
    {
        CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_CHARACTER_GARRISON_COLLECTION);
        stmt->setUInt64(0, guid);
        stmt->setInt32(1, GARR_TYPE_ID);
        stmt->setInt32(2, key.first);
        stmt->setInt32(3, key.second);
        stmt->setInt32(4, rank);
        trans->Append(stmt);
    }
}

void CovenantGarrison::OnLogin()
{
    auto itr = _specGroups.find(int32(_owner->GetPrimarySpecialization()));
    _owner->SetSoulbind(itr != _specGroups.end() ? itr->second : 0);
}

int32 CovenantGarrison::GetActiveSoulbind() const
{
    return _owner->GetSoulbind();
}

void CovenantGarrison::FillTalentPacket(int32 garrTalentId, Talent const& talent, WorldPackets::Garrison::GarrisonTalent& packetTalent) const
{
    packetTalent.GarrTalentID = garrTalentId;
    packetTalent.Rank = talent.Rank;
    packetTalent.ResearchStartTime = time_t(talent.ResearchStartTime);
    packetTalent.Flags = talent.Flags;
    if (talent.SocketConduitId)
    {
        packetTalent.Socket.emplace();
        packetTalent.Socket->SoulbindConduitID = talent.SocketConduitId;
        packetTalent.Socket->SoulbindConduitRank = talent.SocketConduitRank;
    }
}

void CovenantGarrison::BuildInfoPacket(WorldPackets::Garrison::GarrisonInfo& info) const
{
    info.GarrTypeID = GARR_TYPE_ID;
    info.GarrSiteID = GARR_SITE_ID;
    info.GarrSiteLevelID = GARR_SITE_LEVEL_ID;
    info.NumFollowerActivationsRemaining = 1;   // 1 in every type-111 entry of the captures

    info.Talents.reserve(_talents.size());
    for (auto const& [garrTalentId, talent] : _talents)
        FillTalentPacket(garrTalentId, talent, info.Talents.emplace_back());

    for (auto const& [key, rank] : _collections)
    {
        if (info.Collections.empty() || info.Collections.back().Type != key.first)
            info.Collections.emplace_back().Type = key.first;

        WorldPackets::Garrison::GarrisonCollectionEntry& entry = info.Collections.back().Entries.emplace_back();
        entry.EntryID = key.second;
        entry.Rank = rank;
    }

    for (auto const& [specId, soulbindId] : _specGroups)
    {
        WorldPackets::Garrison::GarrisonSpecGroup& specGroup = info.SpecGroups.emplace_back();
        specGroup.ChrSpecializationID = specId;
        specGroup.SoulbindID = soulbindId;
    }
}

void CovenantGarrison::AppendToInfoResult(WorldPackets::Garrison::GetGarrisonInfoResult& result) const
{
    BuildInfoPacket(result.Garrisons.emplace_back());

    // follower type -> soft cap, as sent next to the type-111 entry in every capture
    result.FollowerSoftCaps = { { 4, 6 }, { 1, 20 }, { 2, 6 }, { 22, 30 }, { 123, 100 } };
}

void CovenantGarrison::SendInfo() const
{
    WorldPackets::Garrison::GetGarrisonInfoResult garrisonInfo;
    garrisonInfo.FactionIndex = Garrison::GetFaction(_owner->GetTeam());

    if (Garrison* garrison = _owner->GetGarrison())
        garrison->BuildInfoPacket(garrisonInfo.Garrisons.emplace_back());

    AppendToInfoResult(garrisonInfo);
    _owner->SendDirectMessage(garrisonInfo.Write());
}

SoulbindEntry const* CovenantGarrison::GetOwnedSoulbind(GarrTalentEntry const* garrTalent) const
{
    SoulbindEntry const* soulbind = sDB2Manager.GetSoulbindByGarrTalentTree(garrTalent->GarrTalentTreeID);
    if (!soulbind || soulbind->CovenantID != _owner->GetCovenant())
        return nullptr;

    return soulbind;
}

bool CovenantGarrison::IsTreeActive(int32 garrTalentTreeId) const
{
    SoulbindEntry const* soulbind = sDB2Manager.GetSoulbindByGarrTalentTree(garrTalentTreeId);
    return soulbind && int32(soulbind->ID) == GetActiveSoulbind();
}

uint32 CovenantGarrison::GetPerkSpell(int32 garrTalentId, Talent const& talent) const
{
    if (talent.Rank <= 0 || !(talent.Flags & COVENANT_TALENT_FLAG_RESEARCHED))
        return 0;

    // the player's rank n is GarrTalentRank.Rank n - 1 (1316 rank 1 -> rank 0 -> spell 328266)
    GarrTalentRankEntry const* rank = sDB2Manager.GetGarrTalentRank(garrTalentId, talent.Rank - 1);
    if (!rank || rank->PerkSpellID <= 0)
        return 0;

    if (rank->PerkPlayerConditionID && !ConditionMgr::IsPlayerMeetingCondition(_owner, rank->PerkPlayerConditionID))
        return 0;

    return rank->PerkSpellID;
}

uint32 CovenantGarrison::GetConduitSpell(Talent const& talent)
{
    if (!talent.SocketConduitId)
        return 0;

    SoulbindConduitRankEntry const* rank = sDB2Manager.GetSoulbindConduitRank(talent.SocketConduitId, talent.SocketConduitRank);
    return rank ? rank->SpellID : 0;
}

void CovenantGarrison::LearnTalentSpells(int32 garrTalentId, Talent const& talent)
{
    for (uint32 spellId : { GetPerkSpell(garrTalentId, talent), GetConduitSpell(talent) })
        if (spellId && !_owner->HasSpell(spellId))
            _owner->LearnSpell(spellId, false);
}

void CovenantGarrison::UnlearnTalentSpells(int32 garrTalentId, Talent const& talent)
{
    for (uint32 spellId : { GetPerkSpell(garrTalentId, talent), GetConduitSpell(talent) })
        if (spellId && _owner->HasSpell(spellId))
            _owner->RemoveSpell(spellId, false, false);
}

void CovenantGarrison::LearnTreeSpells(int32 garrTalentTreeId)
{
    for (auto const& [garrTalentId, talent] : _talents)
        if (GarrTalentEntry const* entry = sGarrTalentStore.LookupEntry(garrTalentId))
            if (int32(entry->GarrTalentTreeID) == garrTalentTreeId)
                LearnTalentSpells(garrTalentId, talent);
}

void CovenantGarrison::UnlearnTreeSpells(int32 garrTalentTreeId)
{
    for (auto const& [garrTalentId, talent] : _talents)
        if (GarrTalentEntry const* entry = sGarrTalentStore.LookupEntry(garrTalentId))
            if (int32(entry->GarrTalentTreeID) == garrTalentTreeId)
                UnlearnTalentSpells(garrTalentId, talent);
}

void CovenantGarrison::DeactivateTalent(int32 garrTalentId, Talent& talent)
{
    UnlearnTalentSpells(garrTalentId, talent);
    talent.SocketConduitId = 0;
    talent.SocketConduitRank = 0;
    talent.Flags &= ~COVENANT_TALENT_FLAG_RESEARCHED;
}

bool CovenantGarrison::ActivateSoulbind(int32 soulbindId)
{
    SoulbindEntry const* soulbind = sSoulbindStore.LookupEntry(soulbindId);
    if (!soulbind || !soulbind->CovenantID || soulbind->CovenantID != _owner->GetCovenant())
        return false;

    if (soulbind->PlayerConditionID && !ConditionMgr::IsPlayerMeetingCondition(_owner, soulbind->PlayerConditionID))
        return false;

    int32 oldSoulbindId = GetActiveSoulbind();
    if (oldSoulbindId == soulbindId)
        return true;

    if (SoulbindEntry const* oldSoulbind = sSoulbindStore.LookupEntry(oldSoulbindId))
        UnlearnTreeSpells(oldSoulbind->GarrTalentTreeID);

    // retail wrote the soulbind to every specialization of the class on the first activation (nov18b: all three demon hunter specs)
    WorldPackets::Garrison::GarrisonAddSpecGroups addSpecGroups;
    addSpecGroups.GarrTypeID = GARR_TYPE_ID;
    for (uint32 i = 0; i < MAX_SPECIALIZATIONS; ++i)
    {
        ChrSpecializationEntry const* spec = sDB2Manager.GetChrSpecializationByIndex(_owner->GetClass(), i);
        if (!spec)
            continue;

        _specGroups[spec->ID] = soulbindId;
        WorldPackets::Garrison::GarrisonSpecGroup& specGroup = addSpecGroups.SpecGroups.emplace_back();
        specGroup.ChrSpecializationID = spec->ID;
        specGroup.SoulbindID = soulbindId;
    }

    // retail order: learned spells, spec groups, then the SoulbindID update
    LearnTreeSpells(soulbind->GarrTalentTreeID);
    _owner->SendDirectMessage(addSpecGroups.Write());
    _owner->SetSoulbind(soulbindId);
    _needsSave = true;
    return true;
}

bool CovenantGarrison::ResearchTalent(int32 garrTalentId)
{
    GarrTalentEntry const* entry = sGarrTalentStore.LookupEntry(garrTalentId);
    if (!entry || !GetOwnedSoulbind(entry))
        return false;

    // a node that has a rank (selected or switched away from) is picked through CMSG_GARRISON_LEARN_TALENT
    auto itr = _talents.find(garrTalentId);
    if (itr != _talents.end() && itr->second.Rank > 0)
        return false;

    if (entry->PlayerConditionID && !ConditionMgr::IsPlayerMeetingCondition(_owner, entry->PlayerConditionID))
        return false;

    if (entry->PrerequisiteTalentID)
    {
        auto prerequisite = _talents.find(entry->PrerequisiteTalentID);
        if (prerequisite == _talents.end() || !(prerequisite->second.Flags & COVENANT_TALENT_FLAG_RESEARCHED))
            return false;
    }

    // soulbind nodes cost nothing in the data; a priced or timed node belongs to the sanctum trees, which are not handled here
    GarrTalentRankEntry const* rank = sDB2Manager.GetGarrTalentRank(garrTalentId, 0);
    if (rank && (rank->ResearchCost || rank->ResearchGoldCost || rank->ResearchDurationSecs))
        return false;

    Talent& talent = _talents[garrTalentId];
    talent.Rank = 1;
    talent.ResearchStartTime = GameTime::GetGameTime();
    talent.Flags = COVENANT_TALENT_FLAG_RESEARCHED;
    _needsSave = true;

    WorldPackets::Garrison::GarrisonResearchTalentResult result;
    result.Result = 0;
    result.GarrTypeID = GARR_TYPE_ID;
    FillTalentPacket(garrTalentId, talent, result.Talent);
    _owner->SendDirectMessage(result.Write());

    // the spell of a node is only learned for the active soulbind (1316 was researched before the activation and learned at it)
    if (IsTreeActive(entry->GarrTalentTreeID))
        LearnTalentSpells(garrTalentId, talent);

    WorldPackets::Garrison::GarrisonTalentCompleted completed;
    completed.GarrTypeID = GARR_TYPE_ID;
    completed.GarrTalentID = garrTalentId;
    completed.GarrTalentRank = talent.Rank;
    completed.Flags = talent.Flags;
    _owner->SendDirectMessage(completed.Write());
    return true;
}

namespace
{
// the nodes of a soulbind column: the first node and the nodes that name the previous one as their prerequisite, in tier order
std::vector<GarrTalentEntry const*> GetColumn(GarrTalentEntry const* first)
{
    std::vector<GarrTalentEntry const*> column = { first };
    std::vector<GarrTalentEntry const*> const* treeTalents = sDB2Manager.GetGarrTalentsByTree(first->GarrTalentTreeID);
    if (!treeTalents)
        return column;

    for (;;)
    {
        GarrTalentEntry const* next = nullptr;
        for (GarrTalentEntry const* candidate : *treeTalents)
            if (candidate->PrerequisiteTalentID == int32(column.back()->ID))
            {
                next = candidate;
                break;
            }

        if (!next || column.size() > 16)
            break;

        column.push_back(next);
    }

    return column;
}
}

bool CovenantGarrison::LearnTalent(int32 garrTalentId)
{
    GarrTalentEntry const* entry = sGarrTalentStore.LookupEntry(garrTalentId);
    if (!entry || !GetOwnedSoulbind(entry) || entry->PrerequisiteTalentID)
        return false;

    std::vector<GarrTalentEntry const*> newColumn = GetColumn(entry);
    if (newColumn.size() < 2)
        return false;   // not a branch: nothing follows this node

    auto isActive = [this](int32 id)
    {
        auto itr = _talents.find(id);
        return itr != _talents.end() && (itr->second.Flags & COVENANT_TALENT_FLAG_RESEARCHED);
    };

    if (isActive(garrTalentId))
        return false;

    // the selected column of the same tier (1765 when 1306 is picked)
    GarrTalentEntry const* activeFirst = nullptr;
    if (std::vector<GarrTalentEntry const*> const* treeTalents = sDB2Manager.GetGarrTalentsByTree(entry->GarrTalentTreeID))
        for (GarrTalentEntry const* candidate : *treeTalents)
            if (candidate != entry && candidate->Tier == entry->Tier && !candidate->PrerequisiteTalentID && isActive(candidate->ID) && GetColumn(candidate).size() > 1)
            {
                activeFirst = candidate;
                break;
            }

    if (!activeFirst)
        return false;   // no column to switch from: the client researches the node instead

    auto newItr = _talents.find(garrTalentId);
    bool alreadyHasRank = newItr != _talents.end() && newItr->second.Rank > 0;
    if (!alreadyHasRank && entry->PlayerConditionID && !ConditionMgr::IsPlayerMeetingCondition(_owner, entry->PlayerConditionID))
        return false;

    GarrTalentRankEntry const* rank = sDB2Manager.GetGarrTalentRank(garrTalentId, 0);
    if (!alreadyHasRank && rank && (rank->ResearchCost || rank->ResearchGoldCost || rank->ResearchDurationSecs))
        return false;

    std::vector<GarrTalentEntry const*> oldColumn = GetColumn(activeFirst);

    // retail answered with: LEARNED (new node spells), UNLEARNED (old ones), then the branch packet that lists old node, new node, old follower, new follower
    // (nov18c idx 192871-192877: 1765 -> 1306 and its follower 1317 -> 1309). A follower node of the new column is selected when the old column's node of the same depth was.
    std::vector<std::pair<int32, Talent>> oldStates;
    std::vector<int32> newlySelected;
    WorldPackets::Garrison::GarrisonSwitchTalentTreeBranch branch;
    branch.GarrTypeID = GARR_TYPE_ID;

    for (std::size_t i = 0; i < std::max(oldColumn.size(), newColumn.size()); ++i)
    {
        bool oldWasActive = false;
        if (i < oldColumn.size() && isActive(oldColumn[i]->ID))
        {
            oldWasActive = true;
            Talent& oldTalent = _talents[oldColumn[i]->ID];
            oldStates.emplace_back(oldColumn[i]->ID, oldTalent);
            oldTalent.SocketConduitId = 0;      // a socketed conduit goes back to the collection
            oldTalent.SocketConduitRank = 0;
            oldTalent.Flags &= ~COVENANT_TALENT_FLAG_RESEARCHED;
            FillTalentPacket(oldColumn[i]->ID, oldTalent, branch.Talents.emplace_back());
        }

        if (i < newColumn.size() && (i == 0 || oldWasActive))
        {
            Talent& newTalent = _talents[newColumn[i]->ID];
            if (newTalent.Rank <= 0)
            {
                newTalent.Rank = 1;
                newTalent.ResearchStartTime = GameTime::GetGameTime();
            }

            newTalent.Flags |= COVENANT_TALENT_FLAG_RESEARCHED;
            newlySelected.push_back(newColumn[i]->ID);
            FillTalentPacket(newColumn[i]->ID, newTalent, branch.Talents.emplace_back());
        }
    }

    _needsSave = true;

    if (IsTreeActive(entry->GarrTalentTreeID))
    {
        for (int32 id : newlySelected)
            LearnTalentSpells(id, _talents[id]);

        for (auto const& [id, oldTalent] : oldStates)
            UnlearnTalentSpells(id, oldTalent);
    }

    _owner->SendDirectMessage(branch.Write());
    return true;
}

void CovenantGarrison::SocketTalents(int32 garrTalentTreeId, std::vector<std::pair<int32, int32>> const& sockets)
{
    WorldPackets::Garrison::GarrisonApplyTalentSocketDataChanges changes;
    changes.GarrTypeID = GARR_TYPE_ID;

    std::vector<std::pair<int32, Talent>> unlearn;
    std::vector<int32> learn;

    for (auto const& [garrTalentId, conduitId] : sockets)
    {
        GarrTalentEntry const* entry = sGarrTalentStore.LookupEntry(garrTalentId);
        if (!entry || int32(entry->GarrTalentTreeID) != garrTalentTreeId || !GetOwnedSoulbind(entry))
            continue;

        GarrTalentSocketPropertiesEntry const* socketProperties = sGarrTalentSocketPropertiesStore.LookupEntry(entry->GarrTalentSocketPropertiesID);
        if (!socketProperties)
            continue;

        auto talentItr = _talents.find(garrTalentId);
        if (talentItr == _talents.end() || !(talentItr->second.Flags & COVENANT_TALENT_FLAG_RESEARCHED))
            continue;

        Talent& talent = talentItr->second;
        if (!conduitId)
        {
            if (!talent.SocketConduitId)
                continue;

            unlearn.emplace_back(garrTalentId, talent);
            talent.SocketConduitId = 0;
            talent.SocketConduitRank = 0;
            changes.Cleared.push_back(garrTalentId);
            continue;
        }

        auto collectionItr = _collections.find({ int32(COVENANT_COLLECTION_CONDUITS), conduitId });
        SoulbindConduitEntry const* conduit = sSoulbindConduitStore.LookupEntry(conduitId);
        if (collectionItr == _collections.end() || !conduit)
            continue;

        // conduit type 0/1/2 (finesse/potency/endurance) = GarrTalentSocketProperties subtype (nov18b/c: 130 -> 1312, 138 -> 1306, 132 -> 1765)
        if (int32(conduit->ConduitType) != socketProperties->GarrTalentSocketSubtype)
            continue;

        if (conduit->CovenantID && conduit->CovenantID != _owner->GetCovenant())
            continue;

        if (talent.SocketConduitId == conduitId)
            continue;

        bool usedElsewhere = false;
        for (auto const& [otherId, other] : _talents)
            if (otherId != garrTalentId && other.SocketConduitId == conduitId)
            {
                usedElsewhere = true;
                break;
            }

        if (usedElsewhere)
            continue;

        // collection rank 1 is rank index 0 (nov18b: collection rank 1, socket rank 0, SoulbindConduitRank.RankIndex 0 = spell 338793)
        int32 rankIndex = std::max(collectionItr->second - 1, 0);
        if (!sDB2Manager.GetSoulbindConduitRank(conduitId, rankIndex))
            continue;

        if (talent.SocketConduitId)
            unlearn.emplace_back(garrTalentId, talent);

        talent.SocketConduitId = conduitId;
        talent.SocketConduitRank = rankIndex;
        learn.push_back(garrTalentId);

        WorldPackets::Garrison::GarrisonSocketChange& change = changes.Changed.emplace_back();
        change.GarrTalentID = garrTalentId;
        change.Socket.emplace();
        change.Socket->SoulbindConduitID = conduitId;
        change.Socket->SoulbindConduitRank = rankIndex;
    }

    if (changes.Changed.empty() && changes.Cleared.empty())
        return;

    _needsSave = true;
    _owner->SendDirectMessage(changes.Write());

    // retail: the reply first, then the learned or unlearned conduit spell
    // only the conduit spells change here, the perk spell of the node stays
    if (IsTreeActive(garrTalentTreeId))
    {
        for (auto const& [garrTalentId, oldTalent] : unlearn)
            if (uint32 spellId = GetConduitSpell(oldTalent))
                if (_owner->HasSpell(spellId))
                    _owner->RemoveSpell(spellId, false, false);

        for (int32 garrTalentId : learn)
            if (uint32 spellId = GetConduitSpell(_talents[garrTalentId]))
                if (!_owner->HasSpell(spellId))
                    _owner->LearnSpell(spellId, false);
    }
}

bool CovenantGarrison::AddConduit(int32 conduitId)
{
    if (!sSoulbindConduitStore.LookupEntry(conduitId))
        return false;

    auto [itr, inserted] = _collections.try_emplace({ int32(COVENANT_COLLECTION_CONDUITS), conduitId }, 1);
    if (!inserted)
        return false;

    _needsSave = true;

    WorldPackets::Garrison::GarrisonCollectionUpdateEntry update;
    update.GarrTypeID = GARR_TYPE_ID;
    update.Type = COVENANT_COLLECTION_CONDUITS;
    update.EntryID = conduitId;
    update.Rank = itr->second;
    _owner->SendDirectMessage(update.Write());
    return true;
}

uint32 CovenantGarrison::CountConduitsAtRank(int32 rank) const
{
    uint32 count = 0;
    for (auto const& [key, conduitRank] : _collections)
        if (key.first == COVENANT_COLLECTION_CONDUITS && conduitRank >= rank)
            ++count;

    return count;
}

bool CovenantGarrison::HasConduitAtRank(int32 conduitId, int32 rank) const
{
    auto itr = _collections.find({ int32(COVENANT_COLLECTION_CONDUITS), conduitId });
    return itr != _collections.end() && itr->second >= rank;
}
