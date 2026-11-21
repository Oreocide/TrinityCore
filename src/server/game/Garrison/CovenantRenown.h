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

#ifndef TRINITYCORE_COVENANT_RENOWN_H
#define TRINITYCORE_COVENANT_RENOWN_H

#include "Define.h"

class Player;

/*
 * Covenant renown. The level is the quantity of the Renown currency (1822, nov18b idx 46794 / 175199, nov18c login idx 393); the rows of RenownRewards.db2
 * for the player's covenant and each level crossed are granted in ascending row id order (reward quest, reward spell).
 * Everything here is off unless Covenant.Enable and Covenant.Renown.Enable (callings: Covenant.Callings.Enable) are set.
 */
namespace CovenantRenown
{
    constexpr uint32 CURRENCY_RENOWN = 1822;

    bool IsRenownEnabled();
    bool IsCallingsEnabled();

    // called by Player::ModifyCurrency after a gain of the Renown currency
    void GrantLevelRewards(Player* player, uint32 oldLevel, uint32 newLevel);

    // SMSG_COVENANT_RENOWN_SEND_CATCHUP_STATE: true in both captures (nov18b idx 17154 renown 0, nov18c idx 194340 renown 4)
    bool GetCatchupState(Player const* player);

    // SMSG_COVENANT_CALLINGS_AVAILABILITY_RESPONSE: AreCallingsUnlocked was true in all 116 replies of a Kyrian character
    bool AreCallingsUnlocked(Player const* player);
}

#endif // TRINITYCORE_COVENANT_RENOWN_H
