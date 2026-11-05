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

#include "Player.h"
#include "Bag.h"
#include "Item.h"
#include "ItemTemplate.h"
#include "Log.h"
#include "Spell.h"
#include <algorithm>
#include <array>
#include <map>
#include <vector>

// "Clean Up Bags" (CMSG_SORT_BAGS). The order below follows retail 12.1 sniffs:
// the backpack and bags are one sequence, filled from the first backpack slot to the last slot of the
// last bag, poor quality items take the very last slots and the free slots stay between the two.
namespace
{
    // Retail always puts these first, in this order
    constexpr std::array<uint32, 4> CleanUpFirstItems =
    {
        6948,   // Hearthstone
        140192, // Dalaran Hearthstone
        141605, // Flight Master's Whistle
        110560  // Garrison Hearthstone
    };

    uint32 GetCleanUpClassOrder(uint32 itemClass)
    {
        switch (itemClass)
        {
            case ITEM_CLASS_CONSUMABLE:         return 0;
            case ITEM_CLASS_WEAPON:
            case ITEM_CLASS_ARMOR:              return 1;
            case ITEM_CLASS_GEM:                return 2;
            case ITEM_CLASS_ITEM_ENHANCEMENT:   return 3;
            case ITEM_CLASS_GLYPH:              return 4;
            case ITEM_CLASS_TRADE_GOODS:        return 5;
            case ITEM_CLASS_RECIPE:             return 6;
            case ITEM_CLASS_CONTAINER:          return 7;
            case ITEM_CLASS_QUEST:              return 8;
            case ITEM_CLASS_MISCELLANEOUS:      return 9;
            case ITEM_CLASS_BATTLE_PETS:        return 10;
            case ITEM_CLASS_KEY:                return 11;
            case ITEM_CLASS_HOUSING:            return 12;
            case ITEM_CLASS_PROFESSION:         return 13;
            default:                            return 14;
        }
    }

    // Weapons by hand, then shields and off-hand items, then armor by slot; cosmetic armor last
    uint32 GetCleanUpEquipmentOrder(ItemTemplate const* proto)
    {
        if (proto->GetClass() == ITEM_CLASS_WEAPON)
        {
            switch (proto->GetInventoryType())
            {
                case INVTYPE_2HWEAPON:
                case INVTYPE_RANGED:
                case INVTYPE_RANGEDRIGHT:
                case INVTYPE_THROWN:
                    return 0;
                case INVTYPE_WEAPON:
                case INVTYPE_WEAPONMAINHAND:
                    return 1;
                case INVTYPE_WEAPONOFFHAND:
                    return 2;
                default:
                    return 3;
            }
        }

        uint32 order;
        switch (proto->GetInventoryType())
        {
            case INVTYPE_SHIELD:
            case INVTYPE_HOLDABLE:  order = 4; break;
            case INVTYPE_HEAD:      order = 5; break;
            case INVTYPE_SHOULDERS: order = 6; break;
            case INVTYPE_CHEST:
            case INVTYPE_ROBE:      order = 7; break;
            case INVTYPE_WRISTS:    order = 8; break;
            case INVTYPE_HANDS:     order = 9; break;
            case INVTYPE_WAIST:     order = 10; break;
            case INVTYPE_LEGS:      order = 11; break;
            case INVTYPE_FEET:      order = 12; break;
            case INVTYPE_CLOAK:     order = 13; break;
            case INVTYPE_NECK:      order = 14; break;
            case INVTYPE_FINGER:    order = 15; break;
            case INVTYPE_TRINKET:   order = 16; break;
            case INVTYPE_BODY:      order = 17; break;
            case INVTYPE_TABARD:    order = 18; break;
            case INVTYPE_NON_EQUIP: order = 19; break;
            default:                order = 20; break;
        }

        if (proto->GetSubClass() == ITEM_SUBCLASS_ARMOR_COSMETIC)
            order += 20;

        return order;
    }

    using CleanUpKey = std::array<int64, 10>;

    // Ascending key; descending criteria are negated
    CleanUpKey BuildCleanUpKey(Item const* item, Player const* owner)
    {
        ItemTemplate const* proto = item->GetTemplate();
        int64 const entry = item->GetEntry();
        int64 const count = item->GetCount();
        int64 const guid = int64(item->GetGUID().GetCounter());

        auto first = std::find(CleanUpFirstItems.begin(), CleanUpFirstItems.end(), item->GetEntry());
        if (first != CleanUpFirstItems.end())
            return { 0, std::distance(CleanUpFirstItems.begin(), first), -count, -guid };

        int64 const sellPrice = proto->GetSellPrice();
        if (item->GetQuality() == ITEM_QUALITY_POOR)
            return { 2, -sellPrice, -entry, -count, -guid };

        int64 const classOrder = GetCleanUpClassOrder(proto->GetClass());
        int64 const subClass = proto->GetSubClass();
        int64 const itemLevel = item->GetItemLevel(owner);
        int64 const quality = item->GetQuality();

        switch (proto->GetClass())
        {
            case ITEM_CLASS_CONSUMABLE:
                return { 1, classOrder, -subClass, -sellPrice, -itemLevel, -quality, -entry, -count, -guid };
            case ITEM_CLASS_TRADE_GOODS:
                return { 1, classOrder, -subClass, -itemLevel, -quality, -entry, -count, -guid };
            case ITEM_CLASS_RECIPE:
                return { 1, classOrder, -entry, -count, -guid };
            case ITEM_CLASS_WEAPON:
            case ITEM_CLASS_ARMOR:
                return { 1, classOrder, GetCleanUpEquipmentOrder(proto), -itemLevel, -quality, -entry, -count, -guid };
            default:
                return { 1, classOrder, -itemLevel, -quality, -entry, -count, -guid };
        }
    }
}

void Player::SortBags()
{
    if (!IsAlive() || GetTradeData())
        return;

    // the backpack and the four bags are one sequence, the reagent bag is sorted on its own
    std::vector<uint16> inventorySlots;
    std::vector<uint16> reagentSlots;

    if (!IsBackpackAutoSortDisabled())
        for (uint8 slot = INVENTORY_SLOT_ITEM_START; slot < INVENTORY_SLOT_ITEM_START + GetInventorySlotCount(); ++slot)
            inventorySlots.push_back((INVENTORY_SLOT_BAG_0 << 8) | slot);

    for (uint8 bagSlot = INVENTORY_SLOT_BAG_START; bagSlot < REAGENT_BAG_SLOT_END; ++bagSlot)
    {
        Bag* bag = GetBagByPos(bagSlot);
        if (!bag)
            continue;

        uint32 bagIndex = bagSlot - INVENTORY_SLOT_BAG_START;
        if (bagIndex < m_activePlayerData->BagSlotFlags.size() && GetBagSlotFlags(bagIndex).HasFlag(BagSlotFlags::DisableAutoSort))
            continue;

        std::vector<uint16>& slots = bagSlot >= REAGENT_BAG_SLOT_START ? reagentSlots : inventorySlots;
        for (uint32 slot = 0; slot < bag->GetBagSize(); ++slot)
            slots.push_back((bagSlot << 8) | slot);
    }

    SortBagSlots(inventorySlots);
    SortBagSlots(reagentSlots);
}

void Player::SortBagSlots(std::vector<uint16> const& slots)
{
    if (slots.empty())
        return;

    auto isCastItem = [this](Item const* item)
    {
        for (uint32 i = CURRENT_FIRST_NON_MELEE_SPELL; i < CURRENT_MAX_SPELL; ++i)
            if (Spell const* spell = GetCurrentSpell(CurrentSpellTypes(i)))
                if (spell->m_CastItem == item)
                    return true;
        return false;
    };

    // items that keep their slot
    auto isPinned = [&](Item const* item)
    {
        return item->HasItemFlag(ITEM_FIELD_FLAG_CHILD) || item->m_lootGenerated || item->IsInTrade() || isCastItem(item);
    };

    // 1. merge partial stacks of the same item, moving the smaller stacks into the bigger ones
    std::map<uint32, std::vector<Item*>> partialStacks;
    for (uint16 pos : slots)
    {
        Item* item = GetItemByPos(pos);
        if (!item || isPinned(item) || item->IsRefundable() || item->IsBOPTradeable())
            continue;

        uint32 maxStack = item->GetTemplate()->GetMaxStackSize();
        if (maxStack > 1 && item->GetCount() < maxStack)
            partialStacks[item->GetEntry()].push_back(item);
    }

    for (auto& [entry, stacks] : partialStacks)
    {
        if (stacks.size() < 2)
            continue;

        std::ranges::sort(stacks, [](Item const* a, Item const* b)
        {
            if (a->GetCount() != b->GetCount())
                return a->GetCount() > b->GetCount();
            return a->GetGUID().GetCounter() > b->GetGUID().GetCounter();
        });

        std::vector<uint16> positions;
        for (Item const* item : stacks)
            positions.push_back(item->GetPos());

        std::size_t target = 0;
        std::size_t source = positions.size() - 1;
        while (target < source)
        {
            Item* targetItem = GetItemByPos(positions[target]);
            Item* sourceItem = GetItemByPos(positions[source]);
            if (!targetItem || targetItem->GetEntry() != entry || targetItem->GetCount() >= targetItem->GetTemplate()->GetMaxStackSize())
            {
                ++target;
                continue;
            }

            if (!sourceItem || sourceItem->GetEntry() != entry)
            {
                --source;
                continue;
            }

            uint32 targetCount = targetItem->GetCount();
            SwapItem(positions[source], positions[target]);

            // stop if the merge was refused, nothing changed
            Item const* targetAfter = GetItemByPos(positions[target]);
            if (!targetAfter || targetAfter->GetCount() == targetCount)
                break;
        }
    }

    // 2. collect the items to sort and the free slots for them
    std::vector<std::pair<CleanUpKey, Item*>> items;
    std::vector<uint16> targetSlots;
    for (uint16 pos : slots)
    {
        Item* item = GetItemByPos(pos);
        if (item && isPinned(item))
            continue;

        targetSlots.push_back(pos);
        if (item)
            items.emplace_back(BuildCleanUpKey(item, this), item);
    }

    if (items.empty())
        return;

    std::ranges::sort(items, {}, &std::pair<CleanUpKey, Item*>::first);

    // 3. plan the new layout before moving anything
    std::vector<Item*> layout(targetSlots.size(), nullptr);
    std::vector<bool> placed(items.size(), false);

    auto bagOf = [this](uint16 pos) -> Bag const* { return (pos >> 8) == INVENTORY_SLOT_BAG_0 ? nullptr : GetBagByPos(pos >> 8); };
    auto isGeneralSlot = [&](uint16 pos)
    {
        Bag const* bag = bagOf(pos);
        return !bag || (bag->GetTemplate()->GetClass() == ITEM_CLASS_CONTAINER && bag->GetTemplate()->GetSubClass() == ITEM_SUBCLASS_CONTAINER);
    };

    // profession and reagent bags take the first items in order that fit them
    for (std::size_t i = 0; i < targetSlots.size(); ++i)
    {
        if (isGeneralSlot(targetSlots[i]))
            continue;

        ItemTemplate const* bagProto = bagOf(targetSlots[i])->GetTemplate();
        for (std::size_t j = 0; j < items.size(); ++j)
        {
            if (!placed[j] && ItemCanGoIntoBag(items[j].second->GetTemplate(), bagProto))
            {
                layout[i] = items[j].second;
                placed[j] = true;
                break;
            }
        }
    }

    std::vector<std::size_t> generalSlots;
    for (std::size_t i = 0; i < targetSlots.size(); ++i)
        if (isGeneralSlot(targetSlots[i]))
            generalSlots.push_back(i);

    std::vector<std::size_t> regularItems;
    std::vector<std::size_t> junkItems;
    for (std::size_t j = 0; j < items.size(); ++j)
        if (!placed[j])
            (items[j].first[0] == 2 ? junkItems : regularItems).push_back(j);

    if (regularItems.size() + junkItems.size() > generalSlots.size())
    {
        TC_LOG_ERROR("entities.player.items", "Player::SortBagSlots: {} has {} items for {} slots, bags left unsorted",
            GetGUID().ToString(), regularItems.size() + junkItems.size(), generalSlots.size());
        return;
    }

    // regular items from the front, poor quality items in the last slots
    for (std::size_t k = 0; k < regularItems.size(); ++k)
        layout[generalSlots[k]] = items[regularItems[k]].second;

    std::size_t junkStart = generalSlots.size() - junkItems.size();
    for (std::size_t k = 0; k < junkItems.size(); ++k)
        layout[generalSlots[junkStart + k]] = items[junkItems[k]].second;

    // 4. move the items; slot and container changes reach the client in the next object update
    std::vector<std::pair<Item*, uint16>> moves;
    for (std::size_t i = 0; i < targetSlots.size(); ++i)
        if (layout[i] && layout[i]->GetPos() != targetSlots[i])
            moves.emplace_back(layout[i], targetSlots[i]);

    if (moves.empty())
        return;

    for (auto const& [item, pos] : moves)
        RemoveItem(item->GetBagSlot(), item->GetSlot(), false);

    for (auto const& [item, pos] : moves)
    {
        ItemPosCountVec dest;
        dest.emplace_back(pos, item->GetCount());
        StoreItem(dest, item, false);

        // RemoveItem drops it from the tradeable list
        if (item->IsBOPTradeable())
            AddTradeableItem(item);
    }
}
