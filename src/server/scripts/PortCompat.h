/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
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

#ifndef PortCompat_h__
#define PortCompat_h__

// Helpers that stand in for APIs of the BfA-era core our reference scripts (vSLDB/BfaCore/Ashamane) were written against.

#include "Creature.h"
#include "Player.h"
#include "ScriptedCreature.h"
#include "MotionMaster.h"
#include "TemporarySummon.h"
#include "MoveSplineInit.h"
#include "MoveSpline.h"

namespace PortCompat
{
/// escort paths of ported scripts live in waypoint_path with id EscortPathBase + creature entry (imported by script-port/tools/import_escort_paths.py)
inline constexpr uint32 EscortPathBase = 3700000000;

/// The creature of `entry` that `owner` summoned (BfA: Unit::GetSummonedCreatureByEntry)
inline Creature* GetSummonedCreatureByEntry(Unit* owner, uint32 entry, float range = 300.0f)
{
    if (!owner)
        return nullptr;

    Creature* creature = owner->FindNearestCreature(entry, range);
    if (creature && creature->IsSummon() && creature->ToTempSummon()->GetSummonerGUID() == owner->GetGUID())
        return creature;

    return nullptr;
}

/// The unit that summoned `summon` (BfA: AI-side GetSummoner())
inline Unit* GetSummonerOf(Creature* summon)
{
    return summon && summon->IsSummon() ? summon->ToTempSummon()->GetSummonerUnit() : nullptr;
}

/// A smooth spline through fixed points (BfA: MotionMaster::MoveSmoothPath). The end of the move is reported as MovementInform(EFFECT_MOTION_TYPE, pathId)
inline void MoveSmoothPath(Unit* unit, uint32 pathId, Position const* points, size_t count, bool walk = false, bool fly = false)
{
    Movement::PointsArray path;
    path.reserve(count);
    for (size_t i = 0; i < count; ++i)
        path.emplace_back(points[i].GetPositionX(), points[i].GetPositionY(), points[i].GetPositionZ());

    unit->GetMotionMaster()->LaunchMoveSpline([path = std::move(path), walk, fly](Movement::MoveSplineInit& init)
    {
        if (fly)
            init.SetFly();
        init.MovebyPath(path);
        init.SetSmooth();
        init.SetWalk(walk);
    }, pathId, MOTION_PRIORITY_NORMAL, EFFECT_MOTION_TYPE);
}

/// A creature that attacks without moving (old ScriptedAI helper Scripted_NoMovementAI)
struct Scripted_NoMovementAI : public ScriptedAI
{
    explicit Scripted_NoMovementAI(Creature* creature) : ScriptedAI(creature) { }

    void AttackStart(Unit* who) override
    {
        if (who)
            AttackStartNoMove(who);
    }
};

/// Progress of a quest objective (BfA: Player::GetQuestObjectiveCounter)
inline int32 GetQuestObjectiveCounter(Player* player, uint32 questId, uint32 objectiveId)
{
    return player->GetQuestObjectiveData(questId, objectiveId);
}
}

#endif // PortCompat_h__
