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

#ifndef DEF_DE_OTHER_SIDE_H_
#define DEF_DE_OTHER_SIDE_H_

#include "CreatureAIImpl.h"

#define DataHeader "DeOtherSide"
#define DOSScriptName "instance_de_other_side"

uint32 const EncounterCount = 4;

enum DeOtherSideDataTypes
{
    // Encounters (DungeonEncounter.OrderIndex order; lockout bits follow DungeonEncounter.Bit, see encounters[])
    DATA_HAKKAR_THE_SOULFLAYER      = 0,
    DATA_THE_MANASTORMS             = 1,
    DATA_DEALER_XYEXA               = 2,
    DATA_MUEHZALA                   = 3,

    // Encounter members
    DATA_MILLIFICENT_MANASTORM,
    DATA_MILLHOUSE_MANASTORM
};

enum DeOtherSideCreatureIds
{
    BOSS_HAKKAR_THE_SOULFLAYER      = 164558,
    BOSS_MILLIFICENT_MANASTORM      = 164555,
    BOSS_MILLHOUSE_MANASTORM        = 164556,
    BOSS_DEALER_XYEXA               = 164450,
    BOSS_MUEHZALA                   = 166608
};

enum DeOtherSideGameObjectIds
{
    GO_HAKKAR_DOOR                  = 352074,
    GO_CLOCKWORK_HATCH              = 352818    // opens when the Manastorms are done
};

enum DeOtherSideMisc
{
    WORLD_SAFE_LOC_DOS_ENTRANCE     = 100086,   // "9.x Dungeon - De Other Side - Entrance", the sniffed arrival and release point

    WORLD_STATE_HAKKAR_IN_PROGRESS      = 18814,
    WORLD_STATE_MANASTORMS_IN_PROGRESS  = 18813,
    WORLD_STATE_XYEXA_IN_PROGRESS       = 18825,
    WORLD_STATE_MUEHZALA_IN_PROGRESS    = 18816
};

template <class AI, class T>
inline AI* GetDeOtherSideAI(T* obj)
{
    return GetInstanceAI<AI>(obj, DOSScriptName);
}

#define RegisterDeOtherSideCreatureAI(ai_name) RegisterCreatureAIWithFactory(ai_name, GetDeOtherSideAI)

#endif
