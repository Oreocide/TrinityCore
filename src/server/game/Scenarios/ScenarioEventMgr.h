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

#ifndef ScenarioEventMgr_h__
#define ScenarioEventMgr_h__

#include "Define.h"
#include <unordered_map>

class Player;
class WorldObject;

enum class ScenarioEventTriggerType : uint8
{
    CreatureDeath   = 1,    // arg = creature entry; fired for the player credited with the kill
    GameObjectUse   = 2,    // arg = gameobject entry; fired for the player using it (client interaction)
    AreaTrigger     = 3,    // arg = AreaTrigger.db2 ID; fired when a player enters it
    AreaEnter       = 4,    // arg = AreaTable ID; fired when a player enters the area/subzone
    SpellCast       = 5,    // arg = spell id; fired when a player's cast finishes

    Max
};

// Data driven emitters for scenario game events (see patterns/SCENARIO_FOUNDATION.md, S2): table `scenario_event_trigger`
// maps "something happened" -> game event id, delivered through GameEvents::Trigger with the player as source, which is
// what AnyoneTriggerGameEventScenario / PlayerTriggerGameEvent criteria of scenario steps listen to.
class TC_GAME_API ScenarioEventMgr
{
public:
    static ScenarioEventMgr* Instance();

    void LoadFromDB();
    void Trigger(ScenarioEventTriggerType type, uint32 arg, Player* player, WorldObject* other = nullptr) const;

    bool HasTriggers(ScenarioEventTriggerType type) const { return _hasTriggers[size_t(type)]; }

private:
    struct Row
    {
        uint32 MapId;
        uint32 ScenarioId;
        uint32 EventId;
    };

    static uint64 MakeKey(ScenarioEventTriggerType type, uint32 arg) { return (uint64(type) << 32) | arg; }

    std::unordered_multimap<uint64, Row> _triggers;
    bool _hasTriggers[size_t(ScenarioEventTriggerType::Max)] = { };
};

#define sScenarioEventMgr ScenarioEventMgr::Instance()

#endif // ScenarioEventMgr_h__
