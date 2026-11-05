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
#include "CreatureAI.h"
#include "InstanceScript.h"
#include "Map.h"
#include "ScriptMgr.h"
#include "nyalotha.h"

static constexpr ObjectData creatureData[] =
{
    { BOSS_VEXIONA, DATA_VEXIONA }
};

static constexpr DungeonEncounterData const encounters[] =
{
    { DATA_WRATHION,               {{ 2329 }} },
    { DATA_MAUT,                   {{ 2327 }} },
    { DATA_PROPHET_SKITRA,         {{ 2334 }} },
    { DATA_DARK_INQUISITOR_XANESH, {{ 2328 }} },
    { DATA_VEXIONA,                {{ 2336 }} },
    { DATA_THE_HIVEMIND,           {{ 2333 }} },
    { DATA_RA_DEN,                 {{ 2331 }} },
    { DATA_SHADHAR,                {{ 2335 }} },
    { DATA_DRESTAGATH,             {{ 2343 }} },
    { DATA_ILGYNOTH,               {{ 2345 }} },
    { DATA_CARAPACE_OF_NZOTH,      {{ 2337 }} },
    { DATA_NZOTH,                  {{ 2344 }} }
};

// Spawns on Vexiona's platform carry this StringId; she lands once all of them are dead
static constexpr std::string_view VexionaPlatformStringId = "vexiona_platform";

class instance_nyalotha : public InstanceMapScript
{
public:
    instance_nyalotha() : InstanceMapScript(NyalothaScriptName, 2217) { }

    struct instance_nyalotha_InstanceMapScript : public InstanceScript
    {
        instance_nyalotha_InstanceMapScript(InstanceMap* map) : InstanceScript(map),
            _vexionaPlatformCleared(*this, "VexionaPlatformCleared", 0)
        {
            SetHeaders(DataHeader);
            SetBossNumber(EncounterCount);
            LoadObjectData(creatureData, {});
            LoadDungeonEncounterData(encounters);
        }

        void OnCreatureCreate(Creature* creature) override
        {
            InstanceScript::OnCreatureCreate(creature);

            if (creature->IsAlive() && creature->HasStringId(VexionaPlatformStringId))
                _vexionaPlatformAlive.insert(creature->GetGUID());
        }

        void OnUnitDeath(Unit* unit) override
        {
            Creature* creature = unit->ToCreature();
            if (!creature || !creature->HasStringId(VexionaPlatformStringId))
                return;

            _vexionaPlatformAlive.erase(creature->GetGUID());
            if (!_vexionaPlatformAlive.empty() || _vexionaPlatformCleared)
                return;

            _vexionaPlatformCleared = 1;
            if (Creature* vexiona = GetCreature(DATA_VEXIONA))
                vexiona->AI()->DoAction(ACTION_VEXIONA_PLATFORM_CLEARED);
        }

        uint32 GetData(uint32 type) const override
        {
            switch (type)
            {
                case DATA_VEXIONA_PLATFORM_CLEARED:
                    return _vexionaPlatformCleared;
                default:
                    return 0;
            }
        }

    private:
        GuidUnorderedSet _vexionaPlatformAlive;
        PersistentInstanceScriptValue<uint8> _vexionaPlatformCleared;
    };

    InstanceScript* GetInstanceScript(InstanceMap* map) const override
    {
        return new instance_nyalotha_InstanceMapScript(map);
    }
};

void AddSC_instance_nyalotha()
{
    new instance_nyalotha();
}
