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
#include "halls_of_atonement.h"

// Doors from the 2026-11-10 retail capture (Heroic). Room doors close during their encounter (retail closes them 9-10 s after
// the pull); passage doors are closed until the boss dies. Retail opens the outer pair of Halkias exits 2 s after the inner pair.
static constexpr DoorData doorData[] =
{
    { GO_HALKIAS_EXIT_1,            DATA_HALKIAS,                  EncounterDoorBehavior::OpenWhenDone },
    { GO_HALKIAS_EXIT_2,            DATA_HALKIAS,                  EncounterDoorBehavior::OpenWhenDone },
    { GO_HALKIAS_EXIT_3,            DATA_HALKIAS,                  EncounterDoorBehavior::OpenWhenDone },
    { GO_HALKIAS_EXIT_4,            DATA_HALKIAS,                  EncounterDoorBehavior::OpenWhenDone },

    { GO_ECHELON_DOOR,              DATA_ECHELON,                  EncounterDoorBehavior::OpenWhenNotInProgress },
    { GO_CATHEDRAL_DOOR,            DATA_ECHELON,                  EncounterDoorBehavior::OpenWhenDone },
    { GO_ECHELON_EXIT,              DATA_ECHELON,                  EncounterDoorBehavior::OpenWhenDone },

    { GO_ALEEZ_DOOR_1,              DATA_HIGH_ADJUDICATOR_ALEEZ,   EncounterDoorBehavior::OpenWhenNotInProgress },
    { GO_ALEEZ_DOOR_2,              DATA_HIGH_ADJUDICATOR_ALEEZ,   EncounterDoorBehavior::OpenWhenNotInProgress },
    { GO_ALEEZ_EXIT,                DATA_HIGH_ADJUDICATOR_ALEEZ,   EncounterDoorBehavior::OpenWhenDone },

    // Closed until Inquisitor Sigar dies, then a room door for Lord Chamberlain
    { GO_SPIRE_DOOR,                DATA_INQUISITOR_SIGAR,         EncounterDoorBehavior::OpenWhenDone },
    { GO_SPIRE_DOOR,                DATA_LORD_CHAMBERLAIN,         EncounterDoorBehavior::OpenWhenNotInProgress },
};

static constexpr ObjectData creatureData[] =
{
    { BOSS_HALKIAS,                 DATA_HALKIAS                },
    { BOSS_ECHELON,                 DATA_ECHELON                },
    { BOSS_HIGH_ADJUDICATOR_ALEEZ,  DATA_HIGH_ADJUDICATOR_ALEEZ },
    { BOSS_LORD_CHAMBERLAIN,        DATA_LORD_CHAMBERLAIN       },
    { NPC_INQUISITOR_SIGAR,         DATA_INQUISITOR_SIGAR       },
};

// Keyed by DungeonEncounter id: lockout masks follow DungeonEncounter.Bit (Echelon 0, Chamberlain 1, Halkias 2, Aleez 3).
static constexpr DungeonEncounterData encounters[] =
{
    { DATA_HALKIAS,                 {{ 2401 }} },
    { DATA_ECHELON,                 {{ 2380 }} },
    { DATA_HIGH_ADJUDICATOR_ALEEZ,  {{ 2403 }} },
    { DATA_LORD_CHAMBERLAIN,        {{ 2381 }} }
};

class instance_halls_of_atonement : public InstanceMapScript
{
public:
    instance_halls_of_atonement() : InstanceMapScript(HoAScriptName, 2287) { }

    struct instance_halls_of_atonement_InstanceMapScript : public InstanceScript
    {
        instance_halls_of_atonement_InstanceMapScript(InstanceMap* map) : InstanceScript(map)
        {
            SetHeaders(DataHeader);
            SetBossNumber(EncounterCount);
            LoadObjectData(creatureData, {});
            LoadDoorData(doorData);
            LoadDungeonEncounterData(encounters);
            SetEntranceLocation(WORLD_SAFE_LOC_HOA_ENTRANCE);
        }

        // Skeleton: no boss has a script yet, so an encounter is done when its boss dies.
        void OnUnitDeath(Unit* unit) override
        {
            Creature* creature = unit->ToCreature();
            if (!creature)
                return;

            switch (creature->GetEntry())
            {
                case BOSS_HALKIAS:                  SetBossState(DATA_HALKIAS, DONE); break;
                case BOSS_ECHELON:                  SetBossState(DATA_ECHELON, DONE); break;
                case BOSS_HIGH_ADJUDICATOR_ALEEZ:   SetBossState(DATA_HIGH_ADJUDICATOR_ALEEZ, DONE); break;
                case BOSS_LORD_CHAMBERLAIN:         SetBossState(DATA_LORD_CHAMBERLAIN, DONE); break;
                case NPC_INQUISITOR_SIGAR:          SetBossState(DATA_INQUISITOR_SIGAR, DONE); break;
                default:
                    break;
            }
        }
    };

    InstanceScript* GetInstanceScript(InstanceMap* map) const override
    {
        return new instance_halls_of_atonement_InstanceMapScript(map);
    }
};

void AddSC_instance_halls_of_atonement()
{
    new instance_halls_of_atonement();
}
