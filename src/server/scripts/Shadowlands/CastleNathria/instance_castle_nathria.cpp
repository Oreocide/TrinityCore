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

#include "Creature.h"
#include "InstanceScript.h"
#include "Map.h"
#include "ScriptMgr.h"
#include "castle_nathria.h"

// Doors from the 2026-11-06 retail capture (Normal). Boss-bound doors close during their encounter; doors that were closed on
// first sight and opened only after the previous boss died (story doors) open when that boss is done. Retail opens the story
// doors after Prince Renathal / General Draven role-play (39 s to 4.9 min after the kill); here they open at the kill.
static constexpr DoorData doorData[] =
{
    { GO_XYMOX_DOOR,                DATA_ARTIFICER_XYMOX,          EncounterDoorBehavior::OpenWhenNotInProgress },
    { GO_SUN_KING_PASSAGE,          DATA_ARTIFICER_XYMOX,          EncounterDoorBehavior::OpenWhenDone },

    { GO_SUN_KING_DOOR_1,           DATA_SUN_KINGS_SALVATION,      EncounterDoorBehavior::OpenWhenNotInProgress },
    { GO_SUN_KING_DOOR_2,           DATA_SUN_KINGS_SALVATION,      EncounterDoorBehavior::OpenWhenNotInProgress },

    // Retail also keeps these closed until Sun King's Salvation is done. That encounter is won by healing Kael'thas to full,
    // which needs a boss script, so the OpenWhenDone(DATA_SUN_KINGS_SALVATION) rows wait for it.
    { GO_COUNCIL_DOOR_1,            DATA_COUNCIL_OF_BLOOD,         EncounterDoorBehavior::OpenWhenNotInProgress },
    { GO_COUNCIL_DOOR_2,            DATA_COUNCIL_OF_BLOOD,         EncounterDoorBehavior::OpenWhenNotInProgress },
    { GO_COUNCIL_DOOR_3,            DATA_COUNCIL_OF_BLOOD,         EncounterDoorBehavior::OpenWhenNotInProgress },

    { GO_ALTIMOR_DOOR,              DATA_COUNCIL_OF_BLOOD,         EncounterDoorBehavior::OpenWhenDone },
    { GO_ALTIMOR_DOOR,              DATA_HUNTSMAN_ALTIMOR,         EncounterDoorBehavior::OpenWhenNotInProgress },
    { GO_ALTIMOR_EXIT,              DATA_HUNTSMAN_ALTIMOR,         EncounterDoorBehavior::OpenWhenDone },
    { GO_ALTIMOR_TUNNEL,            DATA_HUNTSMAN_ALTIMOR,         EncounterDoorBehavior::OpenWhenDone },

    { GO_DESTROYER_DOOR,            DATA_HUNGERING_DESTROYER,      EncounterDoorBehavior::OpenWhenNotInProgress },
    { GO_DESTROYER_EXIT,            DATA_HUNGERING_DESTROYER,      EncounterDoorBehavior::OpenWhenDone },

    { GO_LADY_DOOR_1,               DATA_LADY_INERVA_DARKVEIN,     EncounterDoorBehavior::OpenWhenNotInProgress },
    { GO_LADY_DOOR_2,               DATA_LADY_INERVA_DARKVEIN,     EncounterDoorBehavior::OpenWhenNotInProgress },
    { GO_LADY_EXIT,                 DATA_LADY_INERVA_DARKVEIN,     EncounterDoorBehavior::OpenWhenDone },
    { GO_HALL_BRICK_DOOR,           DATA_LADY_INERVA_DARKVEIN,     EncounterDoorBehavior::OpenWhenDone },

    { GO_SLUDGEFIST_DOOR_1,         DATA_SLUDGEFIST,               EncounterDoorBehavior::OpenWhenNotInProgress },
    { GO_SLUDGEFIST_DOOR_2,         DATA_SLUDGEFIST,               EncounterDoorBehavior::OpenWhenNotInProgress },
    { GO_SLUDGEFIST_DOOR_3,         DATA_SLUDGEFIST,               EncounterDoorBehavior::OpenWhenNotInProgress },
    { GO_SLUDGEFIST_DOOR_4,         DATA_SLUDGEFIST,               EncounterDoorBehavior::OpenWhenNotInProgress },
    { GO_SLUDGEFIST_DOOR_5,         DATA_SLUDGEFIST,               EncounterDoorBehavior::OpenWhenNotInProgress },
    { GO_SLUDGEFIST_EXIT,           DATA_SLUDGEFIST,               EncounterDoorBehavior::OpenWhenDone },

    { GO_GENERALS_DOOR,             DATA_STONE_LEGION_GENERALS,    EncounterDoorBehavior::OpenWhenNotInProgress },

    { GO_DENATHRIUS_DOOR_1,         DATA_STONE_LEGION_GENERALS,    EncounterDoorBehavior::OpenWhenDone },
    { GO_DENATHRIUS_DOOR_1,         DATA_SIRE_DENATHRIUS,          EncounterDoorBehavior::OpenWhenNotInProgress },
    { GO_DENATHRIUS_DOOR_2,         DATA_STONE_LEGION_GENERALS,    EncounterDoorBehavior::OpenWhenDone },
    { GO_DENATHRIUS_DOOR_2,         DATA_SIRE_DENATHRIUS,          EncounterDoorBehavior::OpenWhenNotInProgress },
};

static constexpr ObjectData creatureData[] =
{
    { BOSS_SHRIEKWING,              DATA_SHRIEKWING             },
    { BOSS_HUNTSMAN_ALTIMOR,        DATA_HUNTSMAN_ALTIMOR       },
    { BOSS_KAELTHAS_SUNSTRIDER,     DATA_SUN_KINGS_SALVATION    },
    { BOSS_HUNGERING_DESTROYER,     DATA_HUNGERING_DESTROYER    },
    { BOSS_ARTIFICER_XYMOX,         DATA_ARTIFICER_XYMOX        },
    { BOSS_LADY_INERVA_DARKVEIN,    DATA_LADY_INERVA_DARKVEIN   },
    { BOSS_BARONESS_FRIEDA,         DATA_BARONESS_FRIEDA        },
    { BOSS_LORD_STAVROS,            DATA_LORD_STAVROS           },
    { BOSS_CASTELLAN_NIKLAUS,       DATA_CASTELLAN_NIKLAUS      },
    { BOSS_SLUDGEFIST,              DATA_SLUDGEFIST             },
    { BOSS_GENERAL_GRASHAAL,        DATA_GENERAL_GRASHAAL       },
    { BOSS_GENERAL_KAAL,            DATA_GENERAL_KAAL           },
    { BOSS_SIRE_DENATHRIUS,         DATA_SIRE_DENATHRIUS        },
};

// Keyed by DungeonEncounter id: lockout masks follow DungeonEncounter.Bit, not the order above.
static constexpr DungeonEncounterData encounters[] =
{
    { DATA_SHRIEKWING,              {{ 2398 }} },
    { DATA_HUNTSMAN_ALTIMOR,        {{ 2418 }} },
    { DATA_SUN_KINGS_SALVATION,     {{ 2402 }} },
    { DATA_HUNGERING_DESTROYER,     {{ 2383 }} },
    { DATA_ARTIFICER_XYMOX,         {{ 2405 }} },
    { DATA_LADY_INERVA_DARKVEIN,    {{ 2406 }} },
    { DATA_COUNCIL_OF_BLOOD,        {{ 2412 }} },
    { DATA_SLUDGEFIST,              {{ 2399 }} },
    { DATA_STONE_LEGION_GENERALS,   {{ 2417 }} },
    { DATA_SIRE_DENATHRIUS,         {{ 2407 }} }
};

class instance_castle_nathria : public InstanceMapScript
{
public:
    instance_castle_nathria() : InstanceMapScript(CNScriptName, 2296) { }

    struct instance_castle_nathria_InstanceMapScript : public InstanceScript
    {
        instance_castle_nathria_InstanceMapScript(InstanceMap* map) : InstanceScript(map)
        {
            SetHeaders(DataHeader);
            SetBossNumber(EncounterCount);
            LoadObjectData(creatureData, {});
            LoadDoorData(doorData);
            LoadDungeonEncounterData(encounters);
            SetEntranceLocation(WORLD_SAFE_LOC_CN_ENTRANCE);
        }

        bool IsAlive(uint32 type) const
        {
            Creature* creature = instance->GetCreature(GetObjectGuid(type));
            return creature && creature->IsAlive();
        }

        // Skeleton: no boss has a script yet, so an encounter is done when its boss dies (all members for the Council and the
        // Generals). Retail ends both of those with Permanent Feign Death and heals Kael'thas to full to win Sun King's
        // Salvation; neither is modelled here.
        void OnUnitDeath(Unit* unit) override
        {
            Creature* creature = unit->ToCreature();
            if (!creature)
                return;

            switch (creature->GetEntry())
            {
                case BOSS_SHRIEKWING:           SetBossState(DATA_SHRIEKWING, DONE); break;
                case BOSS_HUNTSMAN_ALTIMOR:     SetBossState(DATA_HUNTSMAN_ALTIMOR, DONE); break;
                case BOSS_HUNGERING_DESTROYER:  SetBossState(DATA_HUNGERING_DESTROYER, DONE); break;
                case BOSS_ARTIFICER_XYMOX:      SetBossState(DATA_ARTIFICER_XYMOX, DONE); break;
                case BOSS_LADY_INERVA_DARKVEIN: SetBossState(DATA_LADY_INERVA_DARKVEIN, DONE); break;
                case BOSS_SLUDGEFIST:           SetBossState(DATA_SLUDGEFIST, DONE); break;
                case BOSS_SIRE_DENATHRIUS:      SetBossState(DATA_SIRE_DENATHRIUS, DONE); break;
                case BOSS_BARONESS_FRIEDA:
                case BOSS_LORD_STAVROS:
                case BOSS_CASTELLAN_NIKLAUS:
                    if (!IsAlive(DATA_BARONESS_FRIEDA) && !IsAlive(DATA_LORD_STAVROS) && !IsAlive(DATA_CASTELLAN_NIKLAUS))
                        SetBossState(DATA_COUNCIL_OF_BLOOD, DONE);
                    break;
                case BOSS_GENERAL_GRASHAAL:
                case BOSS_GENERAL_KAAL:
                    if (!IsAlive(DATA_GENERAL_GRASHAAL) && !IsAlive(DATA_GENERAL_KAAL))
                        SetBossState(DATA_STONE_LEGION_GENERALS, DONE);
                    break;
                default:
                    break;
            }
        }
    };

    InstanceScript* GetInstanceScript(InstanceMap* map) const override
    {
        return new instance_castle_nathria_InstanceMapScript(map);
    }
};

void AddSC_instance_castle_nathria()
{
    new instance_castle_nathria();
}
