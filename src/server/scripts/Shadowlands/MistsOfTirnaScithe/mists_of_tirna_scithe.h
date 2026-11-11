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

#ifndef DEF_MISTS_OF_TIRNA_SCITHE_H_
#define DEF_MISTS_OF_TIRNA_SCITHE_H_

#include "CreatureAIImpl.h"

#define DataHeader "MistsOfTirnaScithe"
#define MOTSScriptName "instance_mists_of_tirna_scithe"

uint32 const EncounterCount = 3;

enum MistsOfTirnaScitheDataTypes
{
    // Encounters (DungeonEncounter.OrderIndex order; lockout bits follow DungeonEncounter.Bit, see encounters[])
    DATA_INGRA_MALOCH               = 0,
    DATA_MISTCALLER                 = 1,
    DATA_TREDOVA                    = 2
};

enum MistsOfTirnaScitheCreatureIds
{
    BOSS_INGRA_MALOCH               = 164567,
    BOSS_MISTCALLER                 = 164501,
    BOSS_TREDOVA                    = 164517
};

enum MistsOfTirnaScitheGameObjectIds
{
    GO_INGRA_MIST                   = 355383,   // opens after Ingra Maloch
    GO_MISTCALLER_EXIT_1            = 353622,   // the Mistcaller arena's exit Mists
    GO_MISTCALLER_EXIT_2            = 353636,
    GO_MISTCALLER_EXIT_3            = 353637,
    GO_MISTCALLER_EXIT_4            = 353638,
    GO_TREDOVA_DOOR                 = 353514
};

enum MistsOfTirnaScitheMisc
{
    WORLD_SAFE_LOC_MOTS_ENTRANCE        = 100084,   // "9.x Dungeon - Mists of Tirna Scithe - Entrance", the sniffed arrival point

    WORLD_STATE_INGRA_IN_PROGRESS       = 18818,
    WORLD_STATE_MISTCALLER_IN_PROGRESS  = 18806,
    WORLD_STATE_TREDOVA_IN_PROGRESS     = 18809
};

template <class AI, class T>
inline AI* GetMistsOfTirnaScitheAI(T* obj)
{
    return GetInstanceAI<AI>(obj, MOTSScriptName);
}

#define RegisterMistsOfTirnaScitheCreatureAI(ai_name) RegisterCreatureAIWithFactory(ai_name, GetMistsOfTirnaScitheAI)

#endif
