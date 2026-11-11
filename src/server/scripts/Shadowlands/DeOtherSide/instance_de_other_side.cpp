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
#include "de_other_side.h"

// Doors from the 2026-11-10 retail capture (Heroic). Hakkar's door closes for his fight (retail closes it 5 s after the pull); the
// Clockwork Hatch opens when the Manastorms are done. Dealer Xy'exa (portal spellclick) and Mueh'zala have no door.
static constexpr DoorData doorData[] =
{
    { GO_HAKKAR_DOOR,               DATA_HAKKAR_THE_SOULFLAYER,     EncounterDoorBehavior::OpenWhenNotInProgress },
    { GO_CLOCKWORK_HATCH,           DATA_THE_MANASTORMS,            EncounterDoorBehavior::OpenWhenDone },
};

static constexpr ObjectData creatureData[] =
{
    { BOSS_HAKKAR_THE_SOULFLAYER,   DATA_HAKKAR_THE_SOULFLAYER  },
    { BOSS_MILLIFICENT_MANASTORM,   DATA_MILLIFICENT_MANASTORM  },
    { BOSS_MILLHOUSE_MANASTORM,     DATA_MILLHOUSE_MANASTORM    },
    { BOSS_DEALER_XYEXA,            DATA_DEALER_XYEXA           },
    { BOSS_MUEHZALA,                DATA_MUEHZALA               },
};

// Keyed by DungeonEncounter id: lockout masks follow DungeonEncounter.Bit (Manastorms 0, Hakkar 1, Mueh'zala 2, Xy'exa 3).
static constexpr DungeonEncounterData encounters[] =
{
    { DATA_HAKKAR_THE_SOULFLAYER,   {{ 2395 }} },
    { DATA_THE_MANASTORMS,          {{ 2394 }} },
    { DATA_DEALER_XYEXA,            {{ 2400 }} },
    { DATA_MUEHZALA,                {{ 2396 }} }
};

class instance_de_other_side : public InstanceMapScript
{
public:
    instance_de_other_side() : InstanceMapScript(DOSScriptName, 2291) { }

    struct instance_de_other_side_InstanceMapScript : public InstanceScript
    {
        instance_de_other_side_InstanceMapScript(InstanceMap* map) : InstanceScript(map)
        {
            SetHeaders(DataHeader);
            SetBossNumber(EncounterCount);
            LoadObjectData(creatureData, {});
            LoadDoorData(doorData);
            LoadDungeonEncounterData(encounters);
            SetEntranceLocation(WORLD_SAFE_LOC_DOS_ENTRANCE);
        }

        bool SetBossState(uint32 type, EncounterState state) override
        {
            if (!InstanceScript::SetBossState(type, state))
                return false;

            int32 inProgress = state == IN_PROGRESS ? 1 : 0;
            switch (type)
            {
                case DATA_HAKKAR_THE_SOULFLAYER:    DoUpdateWorldState(WORLD_STATE_HAKKAR_IN_PROGRESS, inProgress); break;
                case DATA_THE_MANASTORMS:           DoUpdateWorldState(WORLD_STATE_MANASTORMS_IN_PROGRESS, inProgress); break;
                case DATA_DEALER_XYEXA:             DoUpdateWorldState(WORLD_STATE_XYEXA_IN_PROGRESS, inProgress); break;
                case DATA_MUEHZALA:                 DoUpdateWorldState(WORLD_STATE_MUEHZALA_IN_PROGRESS, inProgress); break;
                default:
                    break;
            }
            return true;
        }

        bool IsAlive(uint32 type) const
        {
            Creature* creature = instance->GetCreature(GetObjectGuid(type));
            return creature && creature->IsAlive();
        }

        // Skeleton: no boss has a script yet, so an encounter is done when its boss dies (both Manastorms for theirs). Retail ends the
        // Manastorms and Mueh'zala by script at about 10% health without a death; that needs their boss scripts.
        void OnUnitDeath(Unit* unit) override
        {
            Creature* creature = unit->ToCreature();
            if (!creature)
                return;

            switch (creature->GetEntry())
            {
                case BOSS_HAKKAR_THE_SOULFLAYER:    SetBossState(DATA_HAKKAR_THE_SOULFLAYER, DONE); break;
                case BOSS_DEALER_XYEXA:             SetBossState(DATA_DEALER_XYEXA, DONE); break;
                case BOSS_MUEHZALA:                 SetBossState(DATA_MUEHZALA, DONE); break;
                case BOSS_MILLIFICENT_MANASTORM:
                case BOSS_MILLHOUSE_MANASTORM:
                    if (!IsAlive(DATA_MILLIFICENT_MANASTORM) && !IsAlive(DATA_MILLHOUSE_MANASTORM))
                        SetBossState(DATA_THE_MANASTORMS, DONE);
                    break;
                default:
                    break;
            }
        }
    };

    InstanceScript* GetInstanceScript(InstanceMap* map) const override
    {
        return new instance_de_other_side_InstanceMapScript(map);
    }
};

void AddSC_instance_de_other_side()
{
    new instance_de_other_side();
}
