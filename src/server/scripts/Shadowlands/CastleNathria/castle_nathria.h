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

#ifndef DEF_CASTLE_NATHRIA_H_
#define DEF_CASTLE_NATHRIA_H_

#include "CreatureAIImpl.h"

#define DataHeader "CastleNathria"
#define CNScriptName "instance_castle_nathria"

uint32 const EncounterCount = 10;

enum CastleNathriaDataTypes
{
    // Encounters (DB2 DungeonEncounter.OrderIndex order; lockout bits follow DungeonEncounter.Bit, see encounters[])
    DATA_SHRIEKWING                 = 0,
    DATA_HUNTSMAN_ALTIMOR           = 1,
    DATA_SUN_KINGS_SALVATION        = 2,
    DATA_HUNGERING_DESTROYER        = 3,
    DATA_ARTIFICER_XYMOX            = 4,
    DATA_LADY_INERVA_DARKVEIN       = 5,
    DATA_COUNCIL_OF_BLOOD           = 6,
    DATA_SLUDGEFIST                 = 7,
    DATA_STONE_LEGION_GENERALS      = 8,
    DATA_SIRE_DENATHRIUS            = 9,

    // Encounter members
    DATA_BARONESS_FRIEDA,
    DATA_LORD_STAVROS,
    DATA_CASTELLAN_NIKLAUS,
    DATA_GENERAL_GRASHAAL,
    DATA_GENERAL_KAAL
};

enum CastleNathriaCreatureIds
{
    BOSS_SHRIEKWING                 = 164406,
    BOSS_HUNTSMAN_ALTIMOR           = 165066,
    BOSS_KAELTHAS_SUNSTRIDER        = 165759,
    BOSS_HUNGERING_DESTROYER        = 164261,
    BOSS_ARTIFICER_XYMOX            = 166644,
    BOSS_LADY_INERVA_DARKVEIN       = 165521,
    BOSS_BARONESS_FRIEDA            = 166969,
    BOSS_LORD_STAVROS               = 166970,
    BOSS_CASTELLAN_NIKLAUS          = 166971,
    BOSS_SLUDGEFIST                 = 164407,
    BOSS_GENERAL_GRASHAAL           = 168113,
    BOSS_GENERAL_KAAL               = 168112,
    BOSS_SIRE_DENATHRIUS            = 167406
};

enum CastleNathriaGameObjectIds
{
    // Artificer Xy'mox
    GO_XYMOX_DOOR                   = 352901,
    GO_SUN_KING_PASSAGE             = 352894,
    // Sun King's Salvation
    GO_SUN_KING_DOOR_1              = 352902,
    GO_SUN_KING_DOOR_2              = 352903,
    // The Council of Blood
    GO_COUNCIL_DOOR_1               = 349538,
    GO_COUNCIL_DOOR_2               = 349863,
    GO_COUNCIL_DOOR_3               = 353985,
    // Huntsman Altimor
    GO_ALTIMOR_DOOR                 = 353146,
    GO_ALTIMOR_EXIT                 = 353145,
    GO_ALTIMOR_TUNNEL               = 352930,
    // Hungering Destroyer
    GO_DESTROYER_DOOR               = 352036,
    GO_DESTROYER_EXIT               = 352031,
    // Lady Inerva Darkvein
    GO_LADY_DOOR_1                  = 352032,
    GO_LADY_DOOR_2                  = 352038,
    GO_LADY_EXIT                    = 352033,
    GO_HALL_BRICK_DOOR              = 352916,
    // Sludgefist
    GO_SLUDGEFIST_DOOR_1            = 349100,
    GO_SLUDGEFIST_DOOR_2            = 349101,
    GO_SLUDGEFIST_DOOR_3            = 349102,
    GO_SLUDGEFIST_DOOR_4            = 352899,
    GO_SLUDGEFIST_DOOR_5            = 352900,
    GO_SLUDGEFIST_EXIT              = 348932,
    // Stone Legion Generals
    GO_GENERALS_DOOR                = 353182,
    // Sire Denathrius
    GO_DENATHRIUS_DOOR_1            = 352823,
    GO_DENATHRIUS_DOOR_2            = 352824
};

enum CastleNathriaMisc
{
    WORLD_SAFE_LOC_CN_ENTRANCE      = 9400401   // release / spirit-heal point in the entrance hall (sniffed SMSG_DEATH_RELEASE_LOC)
};

template <class AI, class T>
inline AI* GetCastleNathriaAI(T* obj)
{
    return GetInstanceAI<AI>(obj, CNScriptName);
}

#define RegisterCastleNathriaCreatureAI(ai_name) RegisterCreatureAIWithFactory(ai_name, GetCastleNathriaAI)

#endif
