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

#include "ScriptMgr.h"
#include "Creature.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "QuestDef.h"
#include "TemporarySummon.h"
#include <array>
#include <list>

namespace Scripts::DragonIsles::ZaralekCavern
{
namespace Maps
{
    static constexpr uint32 ZaralekCavern = 2454;
}

// Quest followers that retail summons through a linked-summon aura (SPELL_AURA_LINKED_SUMMON) on the player. Summons are not
// saved with the character and the aura only summons when it is applied, so it is applied again on login and on every map or
// zone change while the quest state matches, and removed (which despawns the follower) as soon as it no longer does.
struct QuestFollower
{
    uint32 QuestId;
    uint32 JoinObjectiveId;     // the follower joins once this objective is done
    bool StaysUntilTurnIn;      // true: stays while the quest is complete too; false: leaves when every objective is done
    uint32 LinkedSummonAura;
    uint32 FollowerEntry;
};

static constexpr std::array<QuestFollower, 2> QuestFollowers =
{{
    // 72922: Scalecommander Emberthal 204016 joins at "Speak to Emberthal" and stays until the turn-in (aura 407448 -> 407450)
    { 72922, 433704, true,  407448, 204016 },
    // 72974: Brynnsahce 202477 joins at "Speak with Brynnsahce" and leaves once every objective is done (aura 403050 -> 403053)
    { 72974, 435404, false, 403050, 202477 },
}};

static bool IsObjectiveDone(Player const* player, uint32 objectiveId)
{
    QuestObjective const* objective = sObjectMgr->GetQuestObjective(objectiveId);
    return objective && player->GetQuestObjectiveData(*objective) >= objective->Amount;
}

static bool HasFollower(Player* player, uint32 entry)
{
    std::list<Creature*> followers;
    player->GetCreatureListWithEntryInGrid(followers, entry, 200.0f);
    for (Creature const* follower : followers)
        if (follower->IsSummon() && follower->ToTempSummon()->GetSummonerGUID() == player->GetGUID())
            return true;
    return false;
}

static bool IsWanted(Player const* player, QuestFollower const& follower)
{
    if (player->GetMapId() != Maps::ZaralekCavern)
        return false;

    QuestStatus status = player->GetQuestStatus(follower.QuestId);
    if (status != QUEST_STATUS_INCOMPLETE && !(follower.StaysUntilTurnIn && status == QUEST_STATUS_COMPLETE))
        return false;

    return IsObjectiveDone(player, follower.JoinObjectiveId);
}

static void SyncFollower(Player* player, QuestFollower const& follower)
{
    if (!IsWanted(player, follower))
    {
        if (player->HasAura(follower.LinkedSummonAura))
            player->RemoveAurasDueToSpell(follower.LinkedSummonAura);
        return;
    }

    if (player->HasAura(follower.LinkedSummonAura) && HasFollower(player, follower.FollowerEntry))
        return;

    player->RemoveAurasDueToSpell(follower.LinkedSummonAura);
    player->CastSpell(player, follower.LinkedSummonAura, true);
}

static void SyncFollowers(Player* player)
{
    for (QuestFollower const& follower : QuestFollowers)
        SyncFollower(player, follower);
}

class player_zaralek_cavern_quest_followers : public PlayerScript
{
public:
    player_zaralek_cavern_quest_followers() : PlayerScript("player_zaralek_cavern_quest_followers") { }

    void OnLogin(Player* player, bool /*firstLogin*/) override { SyncFollowers(player); }
    void OnMapChanged(Player* player) override { SyncFollowers(player); }
    void OnUpdateZone(Player* player, uint32 /*newZone*/, uint32 /*newArea*/) override { SyncFollowers(player); }

    void OnQuestStatusChange(Player* player, uint32 questId) override
    {
        for (QuestFollower const& follower : QuestFollowers)
            if (follower.QuestId == questId)
                SyncFollower(player, follower);
    }
};
}

void AddSC_zone_zaralek_cavern()
{
    using namespace Scripts::DragonIsles::ZaralekCavern;
    new player_zaralek_cavern_quest_followers();
}
