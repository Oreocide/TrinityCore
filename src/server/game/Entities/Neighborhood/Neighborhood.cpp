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

#include "Neighborhood.h"
#include "Config.h"
#include "DB2Stores.h"
#include "DBCEnums.h"
#include "Log.h"
#include "StringFormat.h"
#include <mutex>
#include <unordered_map>

namespace Housing
{
namespace
{
// Name sent by retail for the public Founder's Point neighborhood (constant in every sniffed packet, meaning unknown)
constexpr std::string_view PublicNeighborhoodName = "66-71-65";
// Retail guid: Housing/Neighborhood (sub type 4), arg1 = NeighborhoodMap.ID, counter 46191 for NeighborhoodMap 1
constexpr uint64 NeighborhoodGuidCounterBase = 46190;
constexpr uint32 NeighborhoodGuidSubType = 4;

std::mutex NeighborhoodsLock;
// Entities are deliberately never freed: BaseEntity aborts if destroyed while in the world or the update list, and they live until shutdown.
std::unordered_map<uint32, Neighborhood*> Neighborhoods;
}

Neighborhood::Neighborhood(ObjectGuid guid, std::string name, uint32 plotCount) : m_name(std::move(name))
{
    _Create(guid);

    m_entityFragments.Add(WowCS::EntityFragment::FNeighborhoodMirrorData_C, false, WowCS::GetRawFragmentData(m_neighborhoodMirrorData));

    auto data = m_values.ModifyValue(&Neighborhood::m_neighborhoodMirrorData);
    SetUpdateFieldValue(data.ModifyValue(&UF::NeighborhoodMirrorData::Name), m_name);

    // one slot per plot, all empty (no plot is owned yet); retail also sends a single empty manager entry
    auto houses = data.ModifyValue(&UF::NeighborhoodMirrorData::Houses);
    for (uint32 i = 0; i < plotCount; ++i)
        AddDynamicUpdateFieldValue(houses);

    AddDynamicUpdateFieldValue(data.ModifyValue(&UF::NeighborhoodMirrorData::Managers));
}

std::string Neighborhood::GetDebugInfo() const
{
    return Trinity::StringFormat("{}\nName: {}", BaseEntity::GetDebugInfo(), m_name);
}

Neighborhood* Neighborhood::GetForMap(uint32 mapId)
{
    if (!sConfigMgr->GetBoolDefault("Housing.Service.Enable"sv, false))
        return nullptr;

    std::lock_guard<std::mutex> guard(NeighborhoodsLock);
    if (auto itr = Neighborhoods.find(mapId); itr != Neighborhoods.end())
        return itr->second;

    MapEntry const* map = sMapStore.LookupEntry(mapId);
    if (!map || map->InstanceType != MAP_HOUSE_NEIGHBORHOOD)
    {
        Neighborhoods.emplace(mapId, nullptr);
        return nullptr;
    }

    NeighborhoodMapEntry const* neighborhoodMap = nullptr;
    for (NeighborhoodMapEntry const* entry : sNeighborhoodMapStore)
        if (uint32(entry->MapID) == mapId)
        {
            neighborhoodMap = entry;
            break;
        }

    Neighborhood* neighborhood = nullptr;
    if (neighborhoodMap)
    {
        uint32 plotCount = 0;
        for (NeighborhoodPlotEntry const* plot : sNeighborhoodPlotStore)
            if (uint32(plot->NeighborhoodMapID) == neighborhoodMap->ID)
                ++plotCount;

        ObjectGuid guid = ObjectGuid::Create<HighGuid::Housing>(NeighborhoodGuidSubType, neighborhoodMap->ID, 0u, NeighborhoodGuidCounterBase + neighborhoodMap->ID);
        neighborhood = new Neighborhood(guid, std::string(PublicNeighborhoodName), plotCount);
        TC_LOG_INFO("server.housing", "Neighborhood entity {} created for map {}: {} plots", guid.ToString(), mapId, plotCount);
    }

    Neighborhoods.emplace(mapId, neighborhood);
    return neighborhood;
}
}
