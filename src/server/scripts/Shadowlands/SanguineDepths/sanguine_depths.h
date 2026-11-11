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

#ifndef DEF_SANGUINE_DEPTHS_H_
#define DEF_SANGUINE_DEPTHS_H_

#include "CreatureAIImpl.h"

#define DataHeader "SanguineDepths"
#define SDScriptName "instance_sanguine_depths"

uint32 const EncounterCount = 4;

enum SanguineDepthsDataTypes
{
    // Encounters (DungeonEncounter.OrderIndex order = DungeonEncounter.Bit order)
    DATA_KRYXIS_THE_VORACIOUS       = 0,
    DATA_EXECUTOR_TARVOLD           = 1,
    DATA_GRAND_PROCTOR_BERYLLIA     = 2,
    DATA_GENERAL_KAAL               = 3
};

enum SanguineDepthsCreatureIds
{
    BOSS_KRYXIS_THE_VORACIOUS       = 162100,
    BOSS_EXECUTOR_TARVOLD           = 162103,
    BOSS_GRAND_PROCTOR_BERYLLIA     = 162102,
    BOSS_GENERAL_KAAL               = 162099
};

enum SanguineDepthsGameObjectIds
{
    GO_KRYXIS_EXIT                  = 355842,   // Locked Portcullis
    GO_TARVOLD_EXIT                 = 351902,   // Prison Cell
    GO_BERYLLIA_EXIT_1              = 355818,
    GO_BERYLLIA_EXIT_2              = 355819,
    GO_BERYLLIA_EXIT_3              = 355820,
    GO_KAAL_DOOR                    = 357202
};

enum SanguineDepthsMisc
{
    WORLD_SAFE_LOC_SD_ENTRANCE      = 100090,   // "9.x Dungeon - Sanguine Depths - Entrance", the sniffed arrival and release point
    WORLD_STATE_KAAL_IN_PROGRESS    = 18626
};

template <class AI, class T>
inline AI* GetSanguineDepthsAI(T* obj)
{
    return GetInstanceAI<AI>(obj, SDScriptName);
}

#define RegisterSanguineDepthsCreatureAI(ai_name) RegisterCreatureAIWithFactory(ai_name, GetSanguineDepthsAI)

#endif
