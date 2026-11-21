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

#include "CovenantRenown.h"
#include "DB2Stores.h"
#include "DB2Structure.h"
#include "LootItemType.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "QuestDef.h"
#include "World.h"

namespace CovenantRenown
{
bool IsRenownEnabled()
{
    return sWorld->getBoolConfig(CONFIG_COVENANT_ENABLE) && sWorld->getBoolConfig(CONFIG_COVENANT_RENOWN_ENABLE);
}

bool IsCallingsEnabled()
{
    return sWorld->getBoolConfig(CONFIG_COVENANT_ENABLE) && sWorld->getBoolConfig(CONFIG_COVENANT_CALLINGS_ENABLE);
}

void GrantLevelRewards(Player* player, uint32 oldLevel, uint32 newLevel)
{
    if (newLevel <= oldLevel || !IsRenownEnabled() || !player->IsInWorld())
        return;

    int32 covenantId = player->GetCovenant();
    if (covenantId <= 0)
        return;

    std::vector<RenownRewardsEntry const*> const* rewards = sDB2Manager.GetRenownRewards(uint32(covenantId));
    if (!rewards)
        return;

    for (RenownRewardsEntry const* reward : *rewards)
    {
        if (reward->Level <= int32(oldLevel) || reward->Level > int32(newLevel))
            continue;

        // Only the quest and the spell of a row are granted: the other reward kinds (items, mounts, appearances, titles, followers) are
        // "will become available" unlocks (adoption, purchase, recruitment) whose server side is not captured.
        // PlayerConditionID is not evaluated: it gates the client display of the row, and row 316 (condition 87733) granted its quest in nov18b idx 175262.
        if (reward->QuestID)
        {
            if (Quest const* quest = sObjectMgr->GetQuestTemplate(uint32(reward->QuestID)))
                player->RewardQuest(quest, LootItemType::Item, 0, nullptr, false);
            else
                TC_LOG_DEBUG("entities.player", "CovenantRenown::GrantLevelRewards: renown row {} (covenant {}, level {}) has quest {} which is not in quest_template",
                    reward->ID, covenantId, reward->Level, reward->QuestID);
        }

        if (reward->SpellID && !player->HasSpell(uint32(reward->SpellID)))
            player->LearnSpell(uint32(reward->SpellID), false);
    }
}

bool GetCatchupState(Player const* /*player*/)
{
    return true;
}

bool AreCallingsUnlocked(Player const* player)
{
    return player->GetCovenant() > 0;
}
}
