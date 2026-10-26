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
#include "GameObject.h"
#include "GameObjectAI.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "PlayerChoice.h"

enum HeroCallBoard
{
    PLAYERCHOICE_HERO_CALL_BOARD_ALLIANCE   = 352, // King's flyer, only a few responses imported so far
    PLAYERCHOICE_HERO_CALL_BOARD_HORDE      = 342  // Warchief's flyer
};

// Hero's Call Board / Warchief's Command Board
struct go_hero_call_board : public GameObjectAI
{
    using GameObjectAI::GameObjectAI;

    bool OnGossipHello(Player* player) override
    {
        player->SendPlayerChoice(me->GetGUID(), player->GetTeam() == HORDE ? PLAYERCHOICE_HERO_CALL_BOARD_HORDE : PLAYERCHOICE_HERO_CALL_BOARD_ALLIANCE);
        return true;
    }
};

// Picking a flyer hands out the zone's Hero's Call / Warchief's Command quest
class playerchoice_hero_call_board : public PlayerChoiceScript
{
public:
    playerchoice_hero_call_board() : PlayerChoiceScript("playerchoice_hero_call_board") { }

    void OnResponse(WorldObject* object, Player* player, PlayerChoice const* /*choice*/, PlayerChoiceResponse const* response, uint16 /*clientIdentifier*/) override
    {
        if (!response->RewardQuestID)
            return;

        Quest const* quest = sObjectMgr->GetQuestTemplate(*response->RewardQuestID);
        if (!quest || !player->CanTakeQuest(quest, true))
            return;

        player->PlayerTalkClass->SendCloseGossip();
        player->AddQuestAndCheckCompletion(quest, object);
    }
};

void AddSC_go_hero_call_board()
{
    RegisterGameObjectAI(go_hero_call_board);
    new playerchoice_hero_call_board();
}
