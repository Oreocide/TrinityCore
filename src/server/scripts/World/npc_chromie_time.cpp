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
    // OrderIndex of gossip option 51903, "I'd like to return to the present timeline, Chromie."
    OPTION_RETURN_TO_PRESENT    = 2,
    // OrderIndex of "Tell me more. <Watch cutscene.>" on every timeline FAQ menu below.
    OPTION_WATCH_CUTSCENE       = 1,

    // Aura 394 adds 335595 as a temporary spell. The client casts 335595, whose effect 277 returns to the present.
    SPELL_SELECTED_THE_PRESENT  = 335807
};

struct ChromieCutscene
{
    uint32 MenuId;
    uint32 SpellId;   // effect 45, Play Movie
};

// Sniff 2026-10-09_21-01-44 clicked the Shadowlands option (menu 31384 -> spell 408994, movie 936).
// The other six menus use the same option and the Play Movie spell whose cinematic is that expansion's intro.
ChromieCutscene const ChromieCutscenes[] =
{
    { 31376, 408936 },   // Cataclysm,    movie 23
    { 31383, 408988 },   // Outland,      movie 27
    { 31382, 408987 },   // Northrend,    movie 18
    { 31381, 408989 },   // Pandaria,     movie 115
    { 31386, 408991 },   // Draenor,      movie 195
    { 31385, 408993 },   // Legion,       movie 470
    { 31384, 408994 },   // Shadowlands,  movie 936
};

// Chromie (167032, Alliance and Horde). Choosing a timeline is CMSG_CHROMIE_TIME_SELECT_EXPANSION.
struct npc_chromie_time : public ScriptedAI
{
    using ScriptedAI::ScriptedAI;

    bool OnGossipSelect(Player* player, uint32 menuId, uint32 gossipListId) override
    {
        if (menuId == MENU_CHROMIE_TIME && gossipListId == OPTION_RETURN_TO_PRESENT)
        {
            if (player->CastSpell(player, SPELL_SELECTED_THE_PRESENT, true) != SPELL_CAST_OK)
                player->SelectChromieTime(0);

            player->PlayerTalkClass->SendCloseGossip();
            return true;
        }

        if (gossipListId == OPTION_WATCH_CUTSCENE)
        {
            for (ChromieCutscene const& cutscene : ChromieCutscenes)
            {
                if (cutscene.MenuId != menuId)
                    continue;

                player->CastSpell(player, cutscene.SpellId, true);
                player->PlayerTalkClass->SendCloseGossip();
                return true;
            }
        }

        return false;
    }
};

void AddSC_npc_chromie_time()
{
    RegisterCreatureAI(npc_chromie_time);
}
