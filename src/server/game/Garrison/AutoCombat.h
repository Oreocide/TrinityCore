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

#ifndef TRINITYCORE_AUTO_COMBAT_H
#define TRINITYCORE_AUTO_COMBAT_H

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

/*
 * Adventure auto-combat of the covenant command table (M6). A pure model: plain structs in, rounds out, no server types, so it can be
 * compiled and run on its own (analysis/core_design/m6/tools/gate_cpp.cpp does that against the captured packets).
 *
 * Every rule is fitted to the three tutorial missions captured in nov18c (2174, 2295, 2175; analysis/core_design/m6/COMBAT_LOG.md). The three captures are both the
 * fit set and the test set. Anything they do not show is NOT guessed: the run records it in Result::Unpinned and the caller must not use such a result.
 * Fitted constants and the event that pins each one are listed in COMBAT_LOG.md. There is no random number: all three captures reproduce without one.
 */
namespace AutoCombat
{
    // GarrAutoMissionEventType (client constants, build 70961)
    enum EventType : uint32_t
    {
        EVENT_MELEE_DAMAGE       = 0,
        EVENT_RANGE_DAMAGE       = 1,
        EVENT_SPELL_MELEE_DAMAGE = 2,
        EVENT_SPELL_RANGE_DAMAGE = 3,
        EVENT_APPLY_AURA         = 7,
        EVENT_REMOVE_AURA        = 8,
        EVENT_DIED               = 9
    };

    struct SpellEffect
    {
        uint8_t EffectIndex = 0;
        int32_t Effect = 0;         // GarrAutoSpellEffect.Effect
        float Points = 0.0f;
        int32_t TargetType = 0;
    };

    struct Spell
    {
        int32_t Id = 0;
        int32_t Cooldown = 0;
        int32_t Duration = 0;
        uint32_t SchoolMask = 0;
        std::vector<SpellEffect> Effects;   // sorted by EffectIndex
    };

    struct Combatant
    {
        int8_t Board = 0;           // GarrAutoBoardIndex: allies 0-4, enemies 5-12
        uint8_t Role = 0;           // GarrAutoCombatantRole
        int32_t Attack = 0;
        int32_t Health = 0;
        int32_t MaxHealth = 0;
        int32_t AttackSpell = 0;
        std::vector<int32_t> Abilities;     // AbilitySpellID, AbilitySpellID2 (0 skipped by the caller)
    };

    struct TargetResult
    {
        uint32_t Board = 0;
        uint32_t OldHealth = 0;
        uint32_t NewHealth = 0;
        uint32_t MaxHealth = 0;
        bool HasPoints = false;
        uint32_t Points = 0;
    };

    struct Event
    {
        uint32_t Type = 0;
        uint32_t SpellID = 0;
        uint32_t SchoolMask = 0;
        uint8_t EffectIndex = 0;
        uint32_t CasterBoard = 0;
        uint32_t AuraType = 0;      // client name "auraType"; 1 on the events of the combatant's own attack spell, 0 on everything else captured
        std::vector<TargetResult> Targets;
    };

    struct Result
    {
        std::vector<std::vector<Event>> Rounds;
        bool Winner = false;
        int32_t FinalHealth[13] = { };      // by board index
        std::vector<std::string> Unpinned;  // a non-empty list means: do not use this result

        bool IsPinned() const { return Unpinned.empty(); }
    };

    using SpellLookup = std::function<Spell const*(int32_t spellId)>;

    // runs one fight; deterministic
    Result Run(std::vector<Combatant> combatants, SpellLookup const& spells);

    // The log section of SMSG_GARRISON_COMPLETE_MISSION_RESULT: per round u32 eventCount, per event
    // u32 type, u32 spellID, u32 schoolMask, u8 effectIndex, u32 casterBoardIndex, u32 auraType, u32 targetCount,
    // per target u32 board, u32 oldHealth, u32 newHealth, u32 maxHealth, u8 hasPointsBit (0x80 / 0), [u32 points]; the final 0xC0 byte is written by the packet.
    template<typename Writer>
    void WriteRounds(Result const& result, Writer& w)
    {
        for (std::vector<Event> const& round : result.Rounds)
        {
            w.Put32(uint32_t(round.size()));
            for (Event const& event : round)
            {
                w.Put32(event.Type);
                w.Put32(event.SpellID);
                w.Put32(event.SchoolMask);
                w.Put8(event.EffectIndex);
                w.Put32(event.CasterBoard);
                w.Put32(event.AuraType);
                w.Put32(uint32_t(event.Targets.size()));
                for (TargetResult const& target : event.Targets)
                {
                    w.Put32(target.Board);
                    w.Put32(target.OldHealth);
                    w.Put32(target.NewHealth);
                    w.Put32(target.MaxHealth);
                    w.Put8(target.HasPoints ? 0x80 : 0x00);
                    if (target.HasPoints)
                        w.Put32(target.Points);
                }
            }
        }
    }
}

#endif // TRINITYCORE_AUTO_COMBAT_H
