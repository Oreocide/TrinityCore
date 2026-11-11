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
#include "mists_of_tirna_scithe.h"

// Doors from the 2026-11-10 retail capture (Heroic). No boss closes a door for its fight; each Mist or door below opens when the boss
// before it is done (retail: 4.5 s after Ingra, 18 s after Mistcaller, at Tred'ova's end). The Mistveil Tangle maze Mists are not
// listed: they open on correct maze choices, which needs the maze script; until then they keep their spawn state.
static constexpr DoorData doorData[] =
{
    { GO_INGRA_MIST,                DATA_INGRA_MALOCH,  EncounterDoorBehavior::OpenWhenDone },
    { GO_MISTCALLER_EXIT_1,         DATA_MISTCALLER,    EncounterDoorBehavior::OpenWhenDone },
    { GO_MISTCALLER_EXIT_2,         DATA_MISTCALLER,    EncounterDoorBehavior::OpenWhenDone },
    { GO_MISTCALLER_EXIT_3,         DATA_MISTCALLER,    EncounterDoorBehavior::OpenWhenDone },
    { GO_MISTCALLER_EXIT_4,         DATA_MISTCALLER,    EncounterDoorBehavior::OpenWhenDone },
    { GO_TREDOVA_DOOR,              DATA_TREDOVA,       EncounterDoorBehavior::OpenWhenDone },
};

static constexpr ObjectData creatureData[] =
{
    { BOSS_INGRA_MALOCH,            DATA_INGRA_MALOCH   },
    { BOSS_MISTCALLER,              DATA_MISTCALLER     },
    { BOSS_TREDOVA,                 DATA_TREDOVA        },
};

// Keyed by DungeonEncounter id: lockout masks follow DungeonEncounter.Bit (Mistcaller 0, Tred'ova 1, Ingra Maloch 2).
static constexpr DungeonEncounterData encounters[] =
{
    { DATA_INGRA_MALOCH,            {{ 2397 }} },
    { DATA_MISTCALLER,              {{ 2392 }} },
    { DATA_TREDOVA,                 {{ 2393 }} }
};

class instance_mists_of_tirna_scithe : public InstanceMapScript
{
public:
    instance_mists_of_tirna_scithe() : InstanceMapScript(MOTSScriptName, 2290) { }

    struct instance_mists_of_tirna_scithe_InstanceMapScript : public InstanceScript
    {
        instance_mists_of_tirna_scithe_InstanceMapScript(InstanceMap* map) : InstanceScript(map)
        {
            SetHeaders(DataHeader);
            SetBossNumber(EncounterCount);
            LoadObjectData(creatureData, {});
            LoadDoorData(doorData);
            LoadDungeonEncounterData(encounters);
            SetEntranceLocation(WORLD_SAFE_LOC_MOTS_ENTRANCE);
        }

        bool SetBossState(uint32 type, EncounterState state) override
        {
            if (!InstanceScript::SetBossState(type, state))
                return false;

            int32 inProgress = state == IN_PROGRESS ? 1 : 0;
            switch (type)
            {
                case DATA_INGRA_MALOCH:     DoUpdateWorldState(WORLD_STATE_INGRA_IN_PROGRESS, inProgress); break;
                case DATA_MISTCALLER:       DoUpdateWorldState(WORLD_STATE_MISTCALLER_IN_PROGRESS, inProgress); break;
                case DATA_TREDOVA:          DoUpdateWorldState(WORLD_STATE_TREDOVA_IN_PROGRESS, inProgress); break;
                default:
                    break;
            }
            return true;
        }

        // Skeleton: no boss has a script yet, so an encounter is done when its boss dies (Ingra Maloch for hers; Droman survives).
        void OnUnitDeath(Unit* unit) override
        {
            Creature* creature = unit->ToCreature();
            if (!creature)
                return;

            switch (creature->GetEntry())
            {
                case BOSS_INGRA_MALOCH:     SetBossState(DATA_INGRA_MALOCH, DONE); break;
                case BOSS_MISTCALLER:       SetBossState(DATA_MISTCALLER, DONE); break;
                case BOSS_TREDOVA:          SetBossState(DATA_TREDOVA, DONE); break;
                default:
                    break;
            }
        }
    };

    InstanceScript* GetInstanceScript(InstanceMap* map) const override
    {
        return new instance_mists_of_tirna_scithe_InstanceMapScript(map);
    }
};

void AddSC_instance_mists_of_tirna_scithe()
{
    new instance_mists_of_tirna_scithe();
}
