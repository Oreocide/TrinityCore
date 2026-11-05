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

#ifndef DEF_NYALOTHA_H_
#define DEF_NYALOTHA_H_

#include "CreatureAIImpl.h"

#define NyalothaScriptName "instance_nyalotha"
#define DataHeader "Nyalotha"

uint32 const EncounterCount = 12;

enum NyalothaDataTypes
{
    // Encounters
    DATA_WRATHION               = 0,
    DATA_MAUT                   = 1,
    DATA_PROPHET_SKITRA         = 2,
    DATA_DARK_INQUISITOR_XANESH = 3,
    DATA_VEXIONA                = 4,
    DATA_THE_HIVEMIND           = 5,
    DATA_RA_DEN                 = 6,
    DATA_SHADHAR                = 7,
    DATA_DRESTAGATH             = 8,
    DATA_ILGYNOTH               = 9,
    DATA_CARAPACE_OF_NZOTH      = 10,
    DATA_NZOTH                  = 11,

    // Additional Data
    DATA_VEXIONA_PLATFORM_CLEARED
};

enum NyalothaCreatureIds
{
    // Bosses
    BOSS_VEXIONA                = 157354,

    // Vexiona
    NPC_FANATICAL_CULTIST       = 157447,
    NPC_CULTIST_PORTAL          = 157448,
    NPC_SPELLBOUND_RITUALIST    = 157450,
    NPC_VOID_ASCENDANT          = 157467,
    NPC_VOID_ASCENDANT_PLATFORM = 162719,
    NPC_GIFT_OF_THE_VOID        = 157422,
    NPC_VOID_PORTAL             = 157651
};

enum NyalothaActions
{
    ACTION_VEXIONA_BREAK_OFF        = 1,
    ACTION_VEXIONA_PLATFORM_CLEARED = 2
};

template <class AI, class T>
inline AI* GetNyalothaAI(T* obj)
{
    return GetInstanceAI<AI>(obj, NyalothaScriptName);
}

#define RegisterNyalothaCreatureAI(ai_name) RegisterCreatureAIWithFactory(ai_name, GetNyalothaAI)

#endif
