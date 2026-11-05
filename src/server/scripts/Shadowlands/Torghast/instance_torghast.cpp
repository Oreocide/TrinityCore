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
#include "EventMap.h"
#include "InstanceScript.h"
#include "Map.h"
#include "Player.h"
#include "ScriptedCreature.h"
#include "ScriptedGossip.h"
#include "SpellScript.h"

/*
 * Map 2162 is one fixed library of floors, and all wings and layers share it. A run walks a list of floors: only the
 * floor it is on exists, spawned as a manual spawn group when the floor starts, so nothing here may be a plain spawn.
 * Which scenario runs comes from the dungeon the door teleports with (LFGDungeons.ScenarioID).
 */

#include "torghast_floors.h"

namespace Torghast
{
static constexpr uint32 MAP_TORGHAST = 2162;
static constexpr uint32 MAP_TORGHAST_HUB = 2453;
static constexpr uint32 LFG_TWISTING_CORRIDORS_LAYER_1 = 2106;

static constexpr uint32 SPAWN_GROUP_START_SET = 216200;
static constexpr uint32 SPAWN_GROUP_FLOOR = 216200;      // + floor
static constexpr uint32 SPAWN_GROUP_FLOOR_EXIT = 216220; // + floor, boss floors only

static constexpr float NEXT_FLOOR_RANGE = 15.0f;
static constexpr Position HubExitPosition = { 1595.43f, 2154.84f, 385.152f, 2.75608f };

enum Spells
{
    SPELL_NEXT_FLOOR            = 323473,
    SPELL_EXIT_TORGHAST         = 344497
};

enum Data
{
    DATA_NEXT_FLOOR             = 1
};

enum Events
{
    EVENT_NEXT_FLOOR            = 1,
    EVENT_RETURN_TO_FLOOR
};

enum WorldStates
{
    WORLD_STATE_FLOOR_INDEX     = 19812,
    WORLD_STATE_FLOOR_NUMBER    = 19143,
    WORLD_STATE_FLOOR_COUNT     = 19149,
    WORLD_STATE_LAYER           = 19811
};

struct WorldStateValue
{
    int32 Id;
    int32 Value;
};

// Set once per run, values as sniffed for this dungeon
static constexpr WorldStateValue CorridorsLayer1States[] =
{
    { 18445, 30 }, { 18581, 4 }, { 18582, 4 }, { 18583, 1 }, { 18749, 1 }, { 19152, 1 }, { 20234, 1 },
    { WORLD_STATE_FLOOR_COUNT, 18 }, { WORLD_STATE_LAYER, 1 }
};

struct BossFloor
{
    uint32 Floor;
    uint32 Boss;
};

// These floors have no way forward until the boss dies. Floor 18 ends the run, it leaves through the exit NPC
static constexpr BossFloor BossFloors[] =
{
    { 6, 159755 },  // The Grand Malleare
    { 12, 152995 }  // Warden of Souls
};

}

using namespace Torghast;

class instance_torghast : public InstanceMapScript
{
public:
    instance_torghast() : InstanceMapScript("instance_torghast", MAP_TORGHAST) { }

    struct instance_torghast_InstanceMapScript : public InstanceScript
    {
        instance_torghast_InstanceMapScript(InstanceMap* map) : InstanceScript(map), _floor(0), _changingFloor(false)
        {
            SetHeaders("TOR");
        }

        void OnPlayerEnter(Player* /*player*/) override
        {
            // Back into a run that has moved on: the door always sends the player to floor 1
            if (_floor)
            {
                if (_floor > 1)
                    _events.ScheduleEvent(EVENT_RETURN_TO_FLOOR, 2s);
                return;
            }

            if (instance->GetLfgDungeonsId() != LFG_TWISTING_CORRIDORS_LAYER_1)
                return;

            for (WorldStateValue const& state : CorridorsLayer1States)
                DoUpdateWorldState(state.Id, state.Value);

            instance->SpawnGroupSpawn(SPAWN_GROUP_START_SET);
            StartFloor(1);
        }

        void OnUnitDeath(Unit* unit) override
        {
            for (BossFloor const& boss : BossFloors)
                if (boss.Floor == _floor && unit->GetEntry() == boss.Boss)
                    instance->SpawnGroupSpawn(SPAWN_GROUP_FLOOR_EXIT + _floor);
        }

        uint32 GetData(uint32 type) const override
        {
            return type == DATA_NEXT_FLOOR && (_changingFloor || !_floor || _floor >= std::size(CorridorsLayer1Floors));
        }

        void SetData(uint32 type, uint32 /*value*/) override
        {
            if (type != DATA_NEXT_FLOOR || GetData(DATA_NEXT_FLOOR))
                return;

            _changingFloor = true;
            _events.ScheduleEvent(EVENT_NEXT_FLOOR, 1s);
        }

        void Update(uint32 diff) override
        {
            _events.Update(diff);

            while (uint32 eventId = _events.ExecuteEvent())
            {
                switch (eventId)
                {
                    case EVENT_NEXT_FLOOR:
                    {
                        StartFloor(_floor + 1);
                        Position const& arrival = CorridorsLayer1Floors[_floor - 1];
                        instance->DoOnPlayers([&arrival](Player* player) { player->NearTeleportTo(arrival); });
                        _changingFloor = false;
                        break;
                    }
                    case EVENT_RETURN_TO_FLOOR:
                    {
                        Position const& arrival = CorridorsLayer1Floors[_floor - 1];
                        instance->DoOnPlayers([&arrival](Player* player) { player->NearTeleportTo(arrival); });
                        break;
                    }
                    default:
                        break;
                }
            }
        }

    private:
        void StartFloor(uint32 floor)
        {
            if (_floor)
            {
                instance->SpawnGroupDespawn(SPAWN_GROUP_FLOOR + _floor, true);

                for (BossFloor const& boss : BossFloors)
                    if (boss.Floor == _floor)
                        instance->SpawnGroupDespawn(SPAWN_GROUP_FLOOR_EXIT + _floor, true);
            }

            _floor = floor;
            instance->SpawnGroupSpawn(SPAWN_GROUP_FLOOR + _floor);

            DoUpdateWorldState(WORLD_STATE_FLOOR_INDEX, _floor);
            DoUpdateWorldState(WORLD_STATE_FLOOR_NUMBER, _floor + 1);
        }

        EventMap _events;
        uint32 _floor;
        bool _changingFloor;
    };

    InstanceScript* GetInstanceScript(InstanceMap* map) const override
    {
        return new instance_torghast_InstanceMapScript(map);
    }
};

// 156480 - Next Floor
struct npc_torghast_next_floor : public ScriptedAI
{
    npc_torghast_next_floor(Creature* creature) : ScriptedAI(creature), _checkTimer(500) { }

    void UpdateAI(uint32 diff) override
    {
        if (_checkTimer > diff)
        {
            _checkTimer -= diff;
            return;
        }

        _checkTimer = 500;

        InstanceScript* instance = me->GetInstanceScript();
        if (!instance || instance->GetData(DATA_NEXT_FLOOR))
            return;

        Player* player = me->SelectNearestPlayer(NEXT_FLOOR_RANGE);
        if (!player || !player->IsAlive())
            return;

        player->CastSpell(player, SPELL_NEXT_FLOOR, true);
        instance->SetData(DATA_NEXT_FLOOR, 1);
    }

private:
    uint32 _checkTimer;
};

// 175074 - Exit Torghast
struct npc_torghast_exit : public ScriptedAI
{
    using ScriptedAI::ScriptedAI;

    bool OnGossipHello(Player* player) override
    {
        player->CastSpell(player, SPELL_EXIT_TORGHAST, true);
        return true;
    }
};

// 175029 - Twisting Corridors
struct npc_torghast_wing_door : public ScriptedAI
{
    using ScriptedAI::ScriptedAI;

    bool OnGossipSelect(Player* player, uint32 /*menuId*/, uint32 /*gossipListId*/) override
    {
        CloseGossipMenuFor(player);
        player->TeleportTo({ .Location = WorldLocation(MAP_TORGHAST, CorridorsLayer1Floors[0]), .LfgDungeonsId = LFG_TWISTING_CORRIDORS_LAYER_1 });
        return true;
    }
};

// 344497 - Exit Torghast
class spell_torghast_exit_torghast : public SpellScript
{
    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        if (Player* player = GetHitUnit()->ToPlayer())
            player->TeleportTo(WorldLocation(MAP_TORGHAST_HUB, HubExitPosition));
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_torghast_exit_torghast::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

void AddSC_instance_torghast()
{
    new instance_torghast();
    RegisterCreatureAI(npc_torghast_next_floor);
    RegisterCreatureAI(npc_torghast_exit);
    RegisterCreatureAI(npc_torghast_wing_door);
    RegisterSpellScript(spell_torghast_exit_torghast);
}
