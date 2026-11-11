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
#include "sanguine_depths.h"

// Doors from the 2026-11-10 retail capture (Heroic). Only General Kaal's door closes for the fight (retail closes it 10 s after the
// pull); the other bosses' exits are closed until the boss is done.
static constexpr DoorData doorData[] =
{
    { GO_KRYXIS_EXIT,               DATA_KRYXIS_THE_VORACIOUS,      EncounterDoorBehavior::OpenWhenDone },
    { GO_TARVOLD_EXIT,              DATA_EXECUTOR_TARVOLD,          EncounterDoorBehavior::OpenWhenDone },
    { GO_BERYLLIA_EXIT_1,           DATA_GRAND_PROCTOR_BERYLLIA,    EncounterDoorBehavior::OpenWhenDone },
    { GO_BERYLLIA_EXIT_2,           DATA_GRAND_PROCTOR_BERYLLIA,    EncounterDoorBehavior::OpenWhenDone },
    { GO_BERYLLIA_EXIT_3,           DATA_GRAND_PROCTOR_BERYLLIA,    EncounterDoorBehavior::OpenWhenDone },
    { GO_KAAL_DOOR,                 DATA_GENERAL_KAAL,              EncounterDoorBehavior::OpenWhenNotInProgress },
};

static constexpr ObjectData creatureData[] =
{
    { BOSS_KRYXIS_THE_VORACIOUS,    DATA_KRYXIS_THE_VORACIOUS   },
    { BOSS_EXECUTOR_TARVOLD,        DATA_EXECUTOR_TARVOLD       },
    { BOSS_GRAND_PROCTOR_BERYLLIA,  DATA_GRAND_PROCTOR_BERYLLIA },
    { BOSS_GENERAL_KAAL,            DATA_GENERAL_KAAL           },
};

static constexpr DungeonEncounterData encounters[] =
{
    { DATA_KRYXIS_THE_VORACIOUS,    {{ 2360 }} },
    { DATA_EXECUTOR_TARVOLD,        {{ 2361 }} },
    { DATA_GRAND_PROCTOR_BERYLLIA,  {{ 2362 }} },
    { DATA_GENERAL_KAAL,            {{ 2363 }} }
};

class instance_sanguine_depths : public InstanceMapScript
{
public:
    instance_sanguine_depths() : InstanceMapScript(SDScriptName, 2284) { }

    struct instance_sanguine_depths_InstanceMapScript : public InstanceScript
    {
        instance_sanguine_depths_InstanceMapScript(InstanceMap* map) : InstanceScript(map)
        {
            SetHeaders(DataHeader);
            SetBossNumber(EncounterCount);
            LoadObjectData(creatureData, {});
            LoadDoorData(doorData);
            LoadDungeonEncounterData(encounters);
            SetEntranceLocation(WORLD_SAFE_LOC_SD_ENTRANCE);
        }

        bool SetBossState(uint32 type, EncounterState state) override
        {
            if (!InstanceScript::SetBossState(type, state))
                return false;

            if (type == DATA_GENERAL_KAAL)
                DoUpdateWorldState(WORLD_STATE_KAAL_IN_PROGRESS, state == IN_PROGRESS ? 1 : 0);

            return true;
        }

        // General Kaal never dies: her script sets DONE at 50% health.
        void OnUnitDeath(Unit* unit) override
        {
            Creature* creature = unit->ToCreature();
            if (!creature)
                return;

            switch (creature->GetEntry())
            {
                case BOSS_KRYXIS_THE_VORACIOUS:     SetBossState(DATA_KRYXIS_THE_VORACIOUS, DONE); break;
                case BOSS_EXECUTOR_TARVOLD:         SetBossState(DATA_EXECUTOR_TARVOLD, DONE); break;
                case BOSS_GRAND_PROCTOR_BERYLLIA:   SetBossState(DATA_GRAND_PROCTOR_BERYLLIA, DONE); break;
                default:
                    break;
            }
        }
    };

    InstanceScript* GetInstanceScript(InstanceMap* map) const override
    {
        return new instance_sanguine_depths_InstanceMapScript(map);
    }
};

void AddSC_instance_sanguine_depths()
{
    new instance_sanguine_depths();
}
