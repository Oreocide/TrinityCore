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

#ifndef DEF_HALLS_OF_ATONEMENT_H_
#define DEF_HALLS_OF_ATONEMENT_H_

#include "CreatureAIImpl.h"

#define DataHeader "HallsOfAtonement"
#define HoAScriptName "instance_halls_of_atonement"

uint32 const EncounterCount = 5;

enum HallsOfAtonementDataTypes
{
    // Encounters (DungeonEncounter.OrderIndex order; lockout bits follow DungeonEncounter.Bit, see encounters[])
    DATA_HALKIAS                    = 0,
    DATA_ECHELON                    = 1,
    DATA_HIGH_ADJUDICATOR_ALEEZ     = 2,
    DATA_LORD_CHAMBERLAIN           = 3,

    // Not an encounter: his death opens the Spire Door to Lord Chamberlain
    DATA_INQUISITOR_SIGAR           = 4
};

enum HallsOfAtonementCreatureIds
{
    BOSS_HALKIAS                    = 165408,
    BOSS_ECHELON                    = 164185,
    BOSS_HIGH_ADJUDICATOR_ALEEZ     = 165410,
    BOSS_LORD_CHAMBERLAIN           = 164218,

    NPC_INQUISITOR_SIGAR            = 167876
};

enum HallsOfAtonementGameObjectIds
{
    // Halkias arena exits
    GO_HALKIAS_EXIT_1               = 349933,
    GO_HALKIAS_EXIT_2               = 349935,
    GO_HALKIAS_EXIT_3               = 349931,
    GO_HALKIAS_EXIT_4               = 349936,
    // Echelon
    GO_ECHELON_DOOR                 = 353178,
    GO_CATHEDRAL_DOOR               = 353179,
    GO_ECHELON_EXIT                 = 355591,
    // High Adjudicator Aleez
    GO_ALEEZ_DOOR_1                 = 355592,
    GO_ALEEZ_DOOR_2                 = 355593,
    GO_ALEEZ_EXIT                   = 349295,
    // Lord Chamberlain
    GO_SPIRE_DOOR                   = 353180
};

enum HallsOfAtonementMisc
{
    WORLD_SAFE_LOC_HOA_ENTRANCE     = 100088    // "9.x Dungeon - Halls of Atonement - Entrance", the sniffed arrival point
};

template <class AI, class T>
inline AI* GetHallsOfAtonementAI(T* obj)
{
    return GetInstanceAI<AI>(obj, HoAScriptName);
}

#define RegisterHallsOfAtonementCreatureAI(ai_name) RegisterCreatureAIWithFactory(ai_name, GetHallsOfAtonementAI)

#endif
