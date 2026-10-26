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
#include "CreatureAI.h"
#include "GossipDef.h"
#include "Player.h"
#include "ScriptedCreature.h"

enum ChromieTimeGossip
{
    MENU_CHROMIE_TIME           = 25426,
    OPTION_RETURN_TO_PRESENT    = 2    // gossip option 51903 "I'd like to return to the present timeline, Chromie."
};

// Chromie: selecting a timeline goes through CMSG_CHROMIE_TIME_SELECT_EXPANSION (WorldSession::HandleChromieTimeSelectExpansion),
// the "return to the present" option is a plain gossip option
struct npc_chromie_time : public ScriptedAI
{
    using ScriptedAI::ScriptedAI;

    bool OnGossipSelect(Player* player, uint32 menuId, uint32 gossipListId) override
    {
        if (menuId == MENU_CHROMIE_TIME && gossipListId == OPTION_RETURN_TO_PRESENT)
        {
            player->PlayerTalkClass->SendCloseGossip();
            player->SelectChromieTime(0);
            return true;
        }
        return false;
    }
};

void AddSC_npc_chromie_time()
{
    RegisterCreatureAI(npc_chromie_time);
}
