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

#ifndef TRINITYCORE_NEIGHBORHOOD_H
#define TRINITYCORE_NEIGHBORHOOD_H

#include "BaseEntity.h"

namespace Housing
{
// Client-side mirror of a neighborhood (entity fragment FNeighborhoodMirrorData_C). Retail sends it as an object of type 18 with the
// player's create block when entering a neighborhood map; the cornerstone window reads the neighborhood name and owner from it.
// Static data only: never added to the world or the update queue, shared by every player on the map.
class TC_GAME_API Neighborhood final : public BaseEntity
{
public:
    Neighborhood(ObjectGuid guid, std::string name, uint32 plotCount);

    // nullptr unless Housing.Service.Enable is set and mapId is a neighborhood map
    static Neighborhood* GetForMap(uint32 mapId);

    std::string GetNameForLocaleIdx(LocaleConstant /*locale*/) const override { return m_name; }
    std::string GetDebugInfo() const override;

    UF::UpdateField<UF::NeighborhoodMirrorData, int32(WowCS::EntityFragment::FNeighborhoodMirrorData_C), 0> m_neighborhoodMirrorData;

protected:
    UF::UpdateFieldFlag GetUpdateFieldFlagsFor(Player const* /*target*/) const override { return UF::UpdateFieldFlag::None; }
    bool AddToObjectUpdate() override { return false; }
    void RemoveFromObjectUpdate() override { }

private:
    std::string m_name;
};
}

#endif // TRINITYCORE_NEIGHBORHOOD_H
