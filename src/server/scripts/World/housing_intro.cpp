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

namespace Scripts::World::HousingIntro
{
enum HousingIntro
{
    MAP_FOUNDERS_POINT              = 2735,
    CREATURE_LYSSABEL_GUARDIAN      = 249850,

    QUEST_MY_FIRST_HOME             = 91863,

    OBJECTIVE_ASK_STEWARD_TO_JOIN   = 463609,
    OBJECTIVE_ACQUIRE_A_HOUSE       = 463610,

    SPELL_LYSSABEL_GUARDIAN_AURA    = 1250473  // LINKED_SUMMON of the steward guardian (creature 249850), summoned only when the aura is applied
};

static bool IsObjectiveDone(Player const* player, uint32 objectiveId)
{
    QuestObjective const* objective = sObjectMgr->GetQuestObjective(objectiveId);
    return objective && player->GetQuestObjectiveData(*objective) >= objective->Amount;
}

static bool HasGuardian(Player* player)
{
    std::list<Creature*> guardians;
    player->GetCreatureListWithEntryInGrid(guardians, CREATURE_LYSSABEL_GUARDIAN, 200.0f);
    for (Creature const* guardian : guardians)
        if (guardian->GetOwnerGUID() == player->GetGUID() || (guardian->IsSummon() && guardian->ToTempSummon()->GetSummonerGUID() == player->GetGUID()))
            return true;
    return false;
}

// The guardian exists only on Founder's Point while the player is between "Ask the steward to join you" and acquiring a house.
// Summons are not saved with the character and a linked-summon aura only summons when it is applied, so the aura is re-applied
// whenever the player logs in or re-enters the map in that quest state, and removed (despawning the guardian) when leaving it.
static void SyncGuardian(Player* player)
{
    QuestStatus status = player->GetQuestStatus(QUEST_MY_FIRST_HOME);
    bool wanted = player->GetMapId() == MAP_FOUNDERS_POINT
        && (status == QUEST_STATUS_INCOMPLETE || status == QUEST_STATUS_COMPLETE)
        && IsObjectiveDone(player, OBJECTIVE_ASK_STEWARD_TO_JOIN)
        && !IsObjectiveDone(player, OBJECTIVE_ACQUIRE_A_HOUSE);

    if (!wanted)
    {
        if (player->HasAura(SPELL_LYSSABEL_GUARDIAN_AURA))
            player->RemoveAurasDueToSpell(SPELL_LYSSABEL_GUARDIAN_AURA);
        return;
    }

    if (player->HasAura(SPELL_LYSSABEL_GUARDIAN_AURA) && HasGuardian(player))
        return;

    player->RemoveAurasDueToSpell(SPELL_LYSSABEL_GUARDIAN_AURA);
    player->CastSpell(player, SPELL_LYSSABEL_GUARDIAN_AURA, true);
}

class player_housing_intro_guardian : public PlayerScript
{
public:
    player_housing_intro_guardian() : PlayerScript("player_housing_intro_guardian") { }

    void OnLogin(Player* player, bool /*firstLogin*/) override { SyncGuardian(player); }
    void OnMapChanged(Player* player) override { SyncGuardian(player); }
    void OnUpdateZone(Player* player, uint32 /*newZone*/, uint32 /*newArea*/) override { SyncGuardian(player); }
};
}

void AddSC_housing_intro()
{
    using namespace Scripts::World::HousingIntro;
    new player_housing_intro_guardian();
}
