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

#include "ScriptMgr.h"
#include "AreaBoundary.h"
#include "Containers.h"
#include "Creature.h"
#include "CreatureAI.h"
#include "firelands.h"
#include "GameObject.h"
#include "InstanceScript.h"
#include "Map.h"

DungeonEncounterData const encounters[] =
{
    { DATA_BETH_TILAC, {{ 1197 }} },
    { DATA_LORD_RHYOLITH, {{ 1204 }} },
    { DATA_SHANNOX, {{ 1205 }} },
    { DATA_ALYSRAZOR, {{ 1206 }} },
    { DATA_BALEROC, {{ 1200 }} },
    { DATA_MAJORDOMO_STAGHELM, {{ 1185 }} },
    { DATA_RAGNAROS, {{ 1203 }} }
};

ObjectData const creatureData[] =
{
    { NPC_LORD_RHYOLITH,                        DATA_LORD_RHYOLITH                      },
    { NPC_RAGNAROS,                             DATA_RAGNAROS                           },
    { NPC_MOVEMENT_CONTROLLER_LORD_RHYOLITH,    DATA_LORD_RHYOLITH_MOVEMENT_CONTROLLER  },
    { NPC_LEFT_FOOT,                            DATA_LEFT_FOOT                          },
    { NPC_RIGHT_FOOT,                           DATA_RIGHT_FOOT                         },
    { NPC_SULFURAS_SMASH_2,                     DATA_SULFURAS_SMASH                     },
    { NPC_SULFURAS_HAND_OF_RAGNAROS,            DATA_SULFURAS_HAND_OF_RAGNAROS          },
    { NPC_ARCHDRUID_HAMUUL_RUNETOTEM,           DATA_ARCHDRUID_HAMUUL_RUNETOTEM         },
    { NPC_DREADFLAME,                           DATA_DREADFLAME_CONTROLLER              },
    { NPC_CENARIUS,                             DATA_CENARIUS                           },
    { 0,                                        0                                       } // END
};

ObjectData const gameObjectData[] =
{
    { GO_RAGNAROS_PLATFORM,     DATA_RAGNAROS_PLATFORM },
    { 0,                        0                      } // END
};

// Only the doors the Rhyolith/Ragnaros scripts rely on; Beth'tilac door, Majordomo firewall and the
// Baleroc firewall keep their existing handling (or none).
DoorData const doorData[] =
{
    { GO_LORD_RHYOLITH_BRIDGE,  DATA_LORD_RHYOLITH, EncounterDoorBehavior::OpenWhenNotInProgress },
    { GO_RAGNAROS_DOOR,         DATA_RAGNAROS,      EncounterDoorBehavior::OpenWhenNotInProgress },
};

enum FirelandsInstanceSpells
{
    SPELL_MAGMA = 108773
};

BossBoundaryData const boundaries =
{
    { DATA_LORD_RHYOLITH, new CircleBoundary(Position(-371.577393f, -318.680725f), 60.f) }
};

class instance_firelands : public InstanceMapScript
{
    public:
        instance_firelands() : InstanceMapScript(FirelandsScriptName, 720) { }

        struct instance_firelands_InstanceScript : public InstanceScript
        {
            instance_firelands_InstanceScript(InstanceMap* map) : InstanceScript(map)
            {
                SetHeaders(DataHeader);
                SetBossNumber(EncounterCount);
                LoadDungeonEncounterData(encounters);
                LoadObjectData(creatureData, gameObjectData);
                LoadDoorData(doorData);
                LoadBossBoundaries(boundaries);
            }

            void OnCreatureCreate(Creature* creature) override
            {
                InstanceScript::OnCreatureCreate(creature);

                switch (creature->GetEntry())
                {
                    case NPC_SMOULDERING_HATCHLING:
                        // Cannot directly start attacking here as the creature is not yet on map
                        creature->m_Events.AddEventAtOffset(new DelayedAttackStartEvent(creature), 500ms);
                        break;
                    case NPC_BALEROC:
                        BalerocGUID = creature->GetGUID();
                        break;
                    case NPC_SULFURAS_SMASH_1:
                    case NPC_LAVA_WAVE:
                    case NPC_SULFURAS_HAND_OF_RAGNAROS:
                    case NPC_MOLTEN_ELEMENTAL:
                    case NPC_BLAZING_HEAT:
                    case NPC_ENTRAPPING_ROOTS:
                    case NPC_CLOUDBURST:
                    case NPC_BREADTH_OF_FROST:
                        if (Creature* ragnaros = GetCreature(DATA_RAGNAROS))
                            if (CreatureAI* ai = ragnaros->AI())
                                ai->JustSummoned(creature);
                        break;
                    case NPC_SPLITTING_BLOW:
                        _splittingBlowGUIDs.push_back(creature->GetGUID());
                        break;
                    case NPC_MAGMA:
                        _magmaGUIDs.push_back(creature->GetGUID());
                        break;
                    default:
                        break;
                }
            }

            void OnGameObjectCreate(GameObject* go) override
            {
                InstanceScript::OnGameObjectCreate(go);

                switch (go->GetEntry())
                {
                    case GO_BALEROC_FIREWALL:
                        BalerocDoorGUID = go->GetGUID();
                        if (GetBossState(DATA_SHANNOX) == DONE || GetBossState(DATA_BALEROC) == DONE)
                            go->SetGoState(GO_STATE_ACTIVE);
                        break;
                    default:
                        break;
                }
            }

            bool SetBossState(uint32 type, EncounterState state) override
            {
                if (!InstanceScript::SetBossState(type, state))
                    return false;

                if ((type == DATA_SHANNOX && state == DONE) || (type == DATA_BALEROC && state != IN_PROGRESS))
                {
                    if (GameObject* door = instance->GetGameObject(BalerocDoorGUID))
                        door->SetGoState(GO_STATE_ACTIVE);
                }
                else if (type == DATA_BALEROC && state == IN_PROGRESS)
                    if (GameObject* door = instance->GetGameObject(BalerocDoorGUID))
                        door->SetGoState(GO_STATE_READY);

                if (type == DATA_RAGNAROS && state == FAIL)
                {
                    _splittingBlowGUIDs.clear();
                    _magmaGUIDs.clear();
                }

                return true;
            }

            uint32 GetData(uint32 type) const override
            {
                switch (type)
                {
                    case DATA_FIRST_RAGNAROS_SPAWN:
                        return uint32(_firstRagnarosSpawn);
                    default:
                        break;
                }
                return InstanceScript::GetData(type);
            }

            void SetData(uint32 type, uint32 value) override
            {
                switch (type)
                {
                    case DATA_FIRST_RAGNAROS_SPAWN:
                        _firstRagnarosSpawn = value != 0;
                        break;
                    case DATA_MAGMA_KNOCKBACK:
                        for (ObjectGuid const& guid : _magmaGUIDs)
                        {
                            if (Creature* magma = instance->GetCreature(guid))
                            {
                                magma->CastSpell(magma, SPELL_MAGMA);
                                magma->DespawnOrUnsummon(100ms);
                            }
                        }
                        break;
                    default:
                        InstanceScript::SetData(type, value);
                        break;
                }
            }

            ObjectGuid GetGuidData(uint32 type) const override
            {
                switch (type)
                {
                    case DATA_BALEROC:
                        return BalerocGUID;
                    case DATA_RANDOM_SPLITTING_BLOW_GUID:
                        if (_splittingBlowGUIDs.empty())
                            return ObjectGuid::Empty;
                        return Trinity::Containers::SelectRandomContainerElement(_splittingBlowGUIDs);
                    default:
                        break;
                }
                return InstanceScript::GetGuidData(type);
            }

            protected:
                ObjectGuid BalerocDoorGUID;
                ObjectGuid BalerocGUID;

            private:
                bool _firstRagnarosSpawn = true;
                GuidVector _splittingBlowGUIDs;
                GuidVector _magmaGUIDs;
        };

        InstanceScript* GetInstanceScript(InstanceMap* map) const override
        {
            return new instance_firelands_InstanceScript(map);
        }
};

void AddSC_instance_firelands()
{
    new instance_firelands();
}
