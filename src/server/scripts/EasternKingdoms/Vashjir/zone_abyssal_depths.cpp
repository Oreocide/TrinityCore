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
#include "Creature.h"
#include "MotionMaster.h"
#include "Player.h"
#include "ScriptedCreature.h"

namespace Scripts::EasternKingdoms::Vashjir::AbyssalDepths
{
namespace Spells
{
    static constexpr uint32 SummonVashjirWhirlpoolVehicleTrigger = 82905;
    static constexpr uint32 RideVehicleHardcoded                 = 46598;
}

namespace Creatures
{
    static constexpr uint32 WorldTrigger = 22515;
}

namespace Paths
{
    static constexpr uint32 PathUnderWhirlpool = 4437300;
    static constexpr uint32 PathUnderWhirlpoolFinalNode = 4;
}

// 6200 - Abyssal Depths Whirlpool Entrance
class at_abyssal_depths_whirlpool_entrance : public AreaTriggerScript
{
public:
    at_abyssal_depths_whirlpool_entrance() : AreaTriggerScript("at_abyssal_depths_whirlpool_entrance") { }

    bool OnTrigger(Player* player, AreaTriggerEntry const* /*areaTrigger*/) override
    {
        // Prevent re-summoning the whirlpool vehicle while the player is
        // already riding one - the trigger volume can be re-entered mid
        // sequence, which previously restarted the whole ride/path chain
        // from scratch.
        if (player->GetVehicle())
            return true;

        // cast from nearby worldtrigger
        if (Creature* worldTrigger = player->FindNearestCreature(Creatures::WorldTrigger, 100.0f))
            worldTrigger->CastSpell(player, Spells::SummonVashjirWhirlpoolVehicleTrigger);

        return true;
    }
};

// 44373 - Whirlpool Vehicle
struct npc_abyssal_depths_whirlpool_vehicle : public ScriptedAI
{
    explicit npc_abyssal_depths_whirlpool_vehicle(Creature* creature) : ScriptedAI(creature) { }

    void JustAppeared() override
    {
        if (Unit* owner = me->GetOwner())
        {
            owner->CastSpell(me, Spells::RideVehicleHardcoded);

            _scheduler.Schedule(3s + 500ms, [this](TaskContext const& /*task*/)
            {
                me->SetAIAnimKitId(822);

                // Previously used an instant same-map CastSpell teleport
                // (TeleportUnderWhirlpool, 82903) here before starting the
                // path. That relocates the vehicle server-side but never
                // notifies the seated player's client (Unit::NearTeleportTo
                // only sends a teleport packet for the teleporting unit
                // itself, not its passengers - see Vehicle::RelocatePassengers
                // / TransportBase::UpdatePassengerPosition, which silently
                // rewrite the passenger's server position with no client
                // packet). The client then had to reconcile a 250+ unit gap
                // on its own, which looked like running/clipping through
                // walls back toward the whirlpool. Using MovePath for the
                // ENTIRE distance (spawn to destination) instead avoids any
                // teleport - ordinary spline movement is properly synced to
                // clients tick-by-tick.
                me->GetMotionMaster()->MovePath(Paths::PathUnderWhirlpool, false);
            });
        }
    }

    // Despawning on cast (the previous approach) happened before MovePath
    // below had a chance to actually run, leaving the passenger's own
    // motion generator in a bad state - still trying to follow a vehicle
    // that no longer existed - causing an uncontrollable "clip through
    // walls back to the whirlpool" bug. Defer the despawn until the
    // vehicle has actually finished its path instead.
    void MovementInform(uint32 type, uint32 id) override
    {
        if (type == WAYPOINT_MOTION_TYPE && id == Paths::PathUnderWhirlpoolFinalNode)
            me->DespawnOrUnsummon();
    }

    void UpdateAI(uint32 diff) override
    {
        _scheduler.Update(diff);
    }

private:
    TaskScheduler _scheduler;
};
}

void AddSC_abyssal_depths()
{
    using namespace Scripts::EasternKingdoms::Vashjir::AbyssalDepths;

    new at_abyssal_depths_whirlpool_entrance();

    RegisterCreatureAI(npc_abyssal_depths_whirlpool_vehicle);
}
