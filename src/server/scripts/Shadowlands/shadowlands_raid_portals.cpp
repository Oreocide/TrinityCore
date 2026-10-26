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

/*
 * Walk-in raid portals of Shadowlands (Sanctum of Domination in the Maw, Sepulcher of the First Ones in Zereth Mortis).
 *
 * The portal gameobjects ("Instance Portal", generic type 5) are decoration; retail teleports players that walk into them.
 * The sniffs do not contain the retail trigger, so these are custom server-side area triggers (spheres, radius 4) spawned at the
 * sniffed portal gameobject positions (see sod-encounters notes). Destinations are the SMSG_NEW_WORLD positions of the 2026-10-01 sniffs.
 * The normal instance entry rules apply because the teleport goes through Player::TeleportTo.
 */

#include "AreaTrigger.h"
#include "AreaTriggerAI.h"
#include "Player.h"
#include "ScriptMgr.h"

namespace ShadowlandsRaidPortals
{
    // Sanctum of Domination (2450) <-> the Maw (2222)
    WorldLocation const SanctumOfDominationEntrance(2450, 73.1716f, -670.362f, 178.404f, 5.509969f);
    WorldLocation const SanctumOfDominationExit(2222, 4839.47f, 5785.76f, 4860.74f, 2.62323f);

    // Sepulcher of the First Ones (2481) <-> Zereth Mortis (2374)
    WorldLocation const SepulcherEntrance(2481, -3830.19f, -1666.82f, 139.729f, 4.7084618f);
    WorldLocation const SepulcherExit(2374, -3828.4653f, -1498.8055f, 118.09645f, 1.5968975f);

    void TeleportPlayer(Unit* unit, WorldLocation const& destination)
    {
        Player* player = unit->ToPlayer();
        if (!player || player->IsGameMaster() || !player->IsAlive())
            return;

        // Do not yank players out of a taxi/teleport already in progress
        if (player->IsBeingTeleported())
            return;

        player->TeleportTo(destination);
    }
}

struct at_shadowlands_raid_portal_base : AreaTriggerAI
{
    at_shadowlands_raid_portal_base(AreaTrigger* areatrigger, WorldLocation const& destination) : AreaTriggerAI(areatrigger), _destination(destination) { }

    void OnUnitEnter(Unit* unit) override
    {
        ShadowlandsRaidPortals::TeleportPlayer(unit, _destination);
    }

private:
    WorldLocation const& _destination;
};

// Maw -> Sanctum of Domination
struct at_sod_portal_entrance : public at_shadowlands_raid_portal_base
{
    at_sod_portal_entrance(AreaTrigger* areatrigger) : at_shadowlands_raid_portal_base(areatrigger, ShadowlandsRaidPortals::SanctumOfDominationEntrance) { }
};

// Sanctum of Domination -> Maw
struct at_sod_portal_exit : public at_shadowlands_raid_portal_base
{
    at_sod_portal_exit(AreaTrigger* areatrigger) : at_shadowlands_raid_portal_base(areatrigger, ShadowlandsRaidPortals::SanctumOfDominationExit) { }
};

// Zereth Mortis -> Sepulcher of the First Ones
struct at_sepulcher_portal_entrance : public at_shadowlands_raid_portal_base
{
    at_sepulcher_portal_entrance(AreaTrigger* areatrigger) : at_shadowlands_raid_portal_base(areatrigger, ShadowlandsRaidPortals::SepulcherEntrance) { }
};

// Sepulcher of the First Ones -> Zereth Mortis
struct at_sepulcher_portal_exit : public at_shadowlands_raid_portal_base
{
    at_sepulcher_portal_exit(AreaTrigger* areatrigger) : at_shadowlands_raid_portal_base(areatrigger, ShadowlandsRaidPortals::SepulcherExit) { }
};

void AddSC_shadowlands_raid_portals()
{
    RegisterAreaTriggerAI(at_sod_portal_entrance);
    RegisterAreaTriggerAI(at_sod_portal_exit);
    RegisterAreaTriggerAI(at_sepulcher_portal_entrance);
    RegisterAreaTriggerAI(at_sepulcher_portal_exit);
}
