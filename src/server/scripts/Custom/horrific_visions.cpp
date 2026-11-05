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
 * Horrific Visions (8.3): parts shared by the Stormwind (map 2213) and Orgrimmar (map 2212) visions.
 *
 * Evidence: retail captures of a full maskless Stormwind run (2026-11-03 11:07, import nov03b) and the Orgrimmar
 * runs of 2026-10-27 (oct27a/oct27b). Spec: trinitycore-tracking/analysis/visions/stormwind_spec.md.
 *
 * Covered here: entry from the Chamber of Heart gateways (Vessel and cloak checks, fresh instance at difficulty 152),
 * Open Vision (Vessel consumed, teleport into the city, scenario events 70210 / 68916 credited to the players),
 * sanity (power bar, area-dependent drain, stage auras), Elite Extermination (+200 sanity on elite kills),
 * the Stormwind district quests' server-side reward, Alleria's Fallen Servants buffs per district boss slain,
 * the Stormwind finale (Victory, Evacuation Protocol, final chests, scenario completion credit) and the exit.
 * District objectives, boss kits and mid-run summons are SmartAI (analysis/visions/fix_hv_stormwind_districts.sql).
 *
 * Not covered (no evidence yet, or the core lacks it): group entry, masks, madness effects, sanity orbs,
 * what happens at 0 sanity or on death, Research upgrades, the Orgrimmar districts and finale.
 */

#include "ScriptMgr.h"
#include "Creature.h"
#include "CreatureData.h"
#include "DB2Stores.h"
#include "EventMap.h"
#include "GameEventSender.h"
#include "InstanceScript.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "QuestDef.h"
#include "SpellInfo.h"
#include "SpellScript.h"
#include <cmath>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

namespace HorrificVision
{
    enum Maps : uint32
    {
        MAP_VISION_ORGRIMMAR    = 2212,
        MAP_VISION_STORMWIND    = 2213,
        MAP_CHAMBER_OF_HEART    = 2215
    };

    // MapDifficulty 4541/4542; the `scenarios` rows (2212/2213, 152) start scenarios 1733/1731
    constexpr Difficulty DIFFICULTY_HORRIFIC_VISION = Difficulty(152);

    enum Spells : uint32
    {
        SPELL_ENTER_PROMPT_STORMWIND        = 305200, // cast by the client on accepting the gateway prompt 313505
        SPELL_ENTER_PROMPT_ORGRIMMAR        = 302608, // same for the Orgrimmar gateway prompt 313504
        SPELL_ENTER_STORMWIND               = 294620, // SCRIPT_EFFECT, forced by 305200
        SPELL_ENTER_ORGRIMMAR               = 294651, // SCRIPT_EFFECT, forced by 302608
        SPELL_SANITY_POWER_BAR              = 287742, // ENABLE_ALT_POWER, UnitPowerBar 554
        SPELL_SANITY_LOSE_EVERY_SECOND      = 289995, // periodic trigger of 287769 every 1000 ms
        SPELL_NZOTHS_AWARENESS              = 287769, // ENERGIZE alternate power, amount 0 in DB2
        SPELL_UNSETTLED                     = 290019,
        SPELL_PARANOID                      = 290021,
        SPELL_TERRIFIED                     = 290022,
        SPELL_SANITY_BREAKER                = 296756, // marker aura of elites that restore sanity
        SPELL_ELITE_EXTERMINATION           = 312456, // ENERGIZE alternate power +200
        SPELL_OPEN_VISION                   = 311996, // Image of Wrathion, SEND_EVENT 70210
        SPELL_VISION_TELEPORT_STORMWIND     = 305196, // TELEPORT_UNITS to the DB destination (Stormwind row only)
        SPELL_VICTORY                       = 311273, // triggers Evacuation Protocol 312522 (14 s)
        SPELL_EVACUATION_STORMWIND          = 311036, // TELEPORT_UNITS to the exit chamber (Stormwind row only)
        SPELL_SCENARIO_COMPLETE_CREDIT      = 316230, // KILL_CREDIT 162451, the last step's criterion
        SPELL_EXIT_VISION                   = 291330, // dummy, cast after the exit prompt 291611
        SPELL_MARKER_CORRUPTED_AREA         = 316232,
        SPELL_MARKER_LOST_AREA              = 316233
    };

    enum Items : uint32
    {
        ITEM_VESSEL_OF_HORRIFIC_VISIONS     = 173363,
        ITEM_ASHJRAKAMAS_SHROUD_OF_RESOLVE  = 169223
    };

    enum GameEventIds : uint32
    {
        GAME_EVENT_OPEN_VISION              = 70210, // Orgrimmar step 4172 (criterion 103871)
        GAME_EVENT_STORMWIND_ARRIVED        = 68916  // Stormwind step 4170 (criterion 44998), 2.9 s after the teleport
    };

    enum Creatures : uint32
    {
        NPC_ALLERIA_WINDRUNNER              = 152718,
        NPC_THERUM_DEEPFORGE                = 156577,
        NPC_SLAVEMASTER_ULROK               = 153541,
        NPC_OVERLORD_MATHIAS_SHAW           = 158157,
        NPC_MAGISTER_UMBRIC                 = 158035
    };

    enum GameObjects : uint32
    {
        GO_CORRUPTED_CHEST_FINAL            = 334758, // created at Alleria's death (idx 262062)
        GO_CORRUPTED_CHEST_EVACUATION       = 334763  // created at the end of Evacuation Protocol (idx 264197)
    };

    enum Quests : uint32
    {
        QUEST_DWARVEN_DISTRICT              = 57153,
        QUEST_OLD_TOWN                      = 57216,
        QUEST_TRADE_DISTRICT                = 57271,
        QUEST_MAGE_QUARTER                  = 57282
    };

    enum InstanceData : uint32
    {
        DATA_ALLERIA_ENGAGED                = 1     // set by Alleria's SmartAI on aggro
    };

    enum InstanceEvents : uint32
    {
        EVENT_STORMWIND_ARRIVED             = 1,
        EVENT_EVACUATE                      = 2
    };

    // Stormwind district quests: auto-accept, auto-complete, rewarded by the server the moment they complete
    // (QUEST_GIVER_QUEST_COMPLETE idx 98283, 138796, 195702, 237879 with no client turn-in).
    std::unordered_set<uint32> const StormwindDistrictQuests = { QUEST_DWARVEN_DISTRICT, QUEST_OLD_TOWN, QUEST_TRADE_DISTRICT, QUEST_MAGE_QUARTER };

    // Alleria opens with one Fallen Servants self-cast per district boss slain in this run (spec 5): Therum 312226,
    // Ul'rok 312228, Shaw 312229, Umbric 312230. The run killed all four; the mapping per boss is from the spell names.
    std::unordered_map<uint32, uint32> const FallenServants =
    {
        { NPC_THERUM_DEEPFORGE,      312226 },
        { NPC_SLAVEMASTER_ULROK,     312228 },
        { NPC_OVERLORD_MATHIAS_SHAW, 312229 },
        { NPC_MAGISTER_UMBRIC,       312230 }
    };

    Position const FinalChestPos       = { -8395.87f, 1000.43f, -187.159f, 6.27868f };
    QuaternionData const FinalChestRot = { 0.0f, 0.0f, -0.00225449f, 0.999997f };
    Position const EvacChestPos        = { -8394.39f, 1006.89f, -187.159f, 5.69977f };
    QuaternionData const EvacChestRot  = { 0.0f, 0.0f, -0.287586f, 0.957755f };

    Position const StormwindLobby      = { -8589.83f, 800.1649f, -129.57883f, 5.003547f };    // SMSG_NEW_WORLD, nov03b
    Position const OrgrimmarLobby      = { 1514.4844f, -4412.5f, -215.52884f, 0.35365555f };  // SMSG_NEW_WORLD, oct27a
    Position const OrgrimmarCity       = { 1462.06f, -4420.45f, 25.45f, 0.11466813f };          // teleport after Open Vision, oct27a
    Position const ChamberOfHeartExit  = { -8366.65f, 1760.16f, 314.88f, 3.1180305f };          // SMSG_NEW_WORLD after Exit Vision, nov03b

    // Every +200 restore of the Stormwind run came with the death of one of these (spec 4.4); the Craggle group
    // and Alleria carry no Sanity Breaker marker, so the marker alone is not enough.
    std::unordered_set<uint32> const StormwindEliteExtermination =
    {
        158158, // Forge-Guard Hurrul
        156577, // Therum Deepforge
        152809, // Alx'kov the Infested
        156949, // Armsmaster Terenson
        158157, // Overlord Mathias Shaw
        158284, // Craggle Wobbletop
        152939, // Boundless Corruption
        158371, // Zardeth of the Black Claw
        158035, // Magister Umbric
        158136, // Inquisitor Darkspeak
        153541, // Slavemaster Ul'rok
        152718  // Alleria Windrunner
    };

    // Measured sanity loss per 1 s tick in the Stormwind run (spec 4.2): neutral 1.16, Corrupted Area 1.51, Lost Area 2.28.
    float constexpr DRAIN_NEUTRAL = 1.16f;
    float constexpr DRAIN_CORRUPTED = 1.51f;
    float constexpr DRAIN_LOST = 2.28f;
    float constexpr AREA_MARKER_RANGE = 50.0f;   // a spot belongs to the area of the nearest marked spawn within this range

    // Stage thresholds read from the run: Paranoid below 920 of 1150, Terrified below about 690 (80 % and 60 %).
    uint32 constexpr STAGE_PARANOID_PCT = 80;
    uint32 constexpr STAGE_TERRIFIED_PCT = 60;

    // The player's own dungeon difficulty, restored once the vision instance exists (entry sets 152 for the teleport).
    // Written from the Chamber's map thread and read from the vision's, so it is guarded.
    std::unordered_map<ObjectGuid, Difficulty> PreviousDungeonDifficulty;
    std::mutex PreviousDungeonDifficultyLock;

    void SavePreviousDifficulty(Player* player)
    {
        std::lock_guard<std::mutex> lock(PreviousDungeonDifficultyLock);
        PreviousDungeonDifficulty[player->GetGUID()] = player->GetDungeonDifficultyID();
    }

    // Puts the player's own difficulty back if entry stored one; true when something was restored.
    bool RestorePreviousDifficulty(Player* player)
    {
        std::lock_guard<std::mutex> lock(PreviousDungeonDifficultyLock);
        auto itr = PreviousDungeonDifficulty.find(player->GetGUID());
        if (itr == PreviousDungeonDifficulty.end())
            return false;
        player->SetDungeonDifficultyID(itr->second);
        PreviousDungeonDifficulty.erase(itr);
        return true;
    }

    bool IsVisionMap(uint32 mapId)
    {
        return mapId == MAP_VISION_STORMWIND || mapId == MAP_VISION_ORGRIMMAR;
    }

    bool AddonHasAura(Creature const* creature, uint32 spellId)
    {
        if (CreatureAddon const* addon = creature->GetCreatureAddon())
            for (uint32 aura : addon->auras)
                if (aura == spellId)
                    return true;
        return false;
    }

    enum class Madness : uint8
    {
        Neutral,
        Corrupted,
        Lost
    };

    class instance_horrific_vision_script : public InstanceScript
    {
    public:
        explicit instance_horrific_vision_script(InstanceMap* map) : InstanceScript(map), _visionStarted(false), _finaleStarted(false)
        {
            SetHeaders("HV");
        }

        bool IsStormwind() const { return instance->GetId() == MAP_VISION_STORMWIND; }

        void OnPlayerEnter(Player* player) override
        {
            RestorePreviousDifficulty(player);

            if (_finaleStarted)
                return;

            if (!player->HasAura(SPELL_SANITY_POWER_BAR))
            {
                player->CastSpell(player, SPELL_SANITY_POWER_BAR, true);
                player->SetPower(POWER_ALTERNATE_POWER, player->GetMaxPower(POWER_ALTERNATE_POWER));
            }
            if (!player->HasAura(SPELL_SANITY_LOSE_EVERY_SECOND))
                player->CastSpell(player, SPELL_SANITY_LOSE_EVERY_SECOND, true);
        }

        void OnPlayerLeave(Player* player) override
        {
            RemoveSanity(player);
            _drainCarry.erase(player->GetGUID());
        }

        void OnCreatureCreate(Creature* creature) override
        {
            InstanceScript::OnCreatureCreate(creature);

            if (creature->GetEntry() == NPC_ALLERIA_WINDRUNNER)
                _alleriaGUID = creature->GetGUID();

            if (!creature->GetSpawnId() || !_markedSpawns.insert(creature->GetSpawnId()).second)
                return;

            if (AddonHasAura(creature, SPELL_MARKER_LOST_AREA))
                _areaMarkers.emplace_back(creature->GetPosition(), Madness::Lost);
            else if (AddonHasAura(creature, SPELL_MARKER_CORRUPTED_AREA))
                _areaMarkers.emplace_back(creature->GetPosition(), Madness::Corrupted);
        }

        void OnUnitDeath(Unit* unit) override
        {
            Creature* creature = unit->ToCreature();
            if (!creature)
                return;

            bool restores = AddonHasAura(creature, SPELL_SANITY_BREAKER)
                || (IsStormwind() && StormwindEliteExtermination.count(creature->GetEntry()));
            if (restores && !_finaleStarted)
                instance->DoOnPlayers([](Player* player)
                {
                    if (player->IsAlive() && player->HasAura(SPELL_SANITY_POWER_BAR))
                        player->CastSpell(player, SPELL_ELITE_EXTERMINATION, true);
                });

            if (FallenServants.count(creature->GetEntry()))
                _slainDistrictBosses.insert(creature->GetEntry());

            if (IsStormwind() && creature->GetEntry() == NPC_ALLERIA_WINDRUNNER)
                StartFinale(creature);
        }

        void SetData(uint32 type, uint32 /*value*/) override
        {
            if (type != DATA_ALLERIA_ENGAGED || !IsStormwind())
                return;

            if (Creature* alleria = instance->GetCreature(_alleriaGUID))
                for (uint32 boss : _slainDistrictBosses)
                    alleria->CastSpell(alleria, FallenServants.at(boss), true);
        }

        // Image of Wrathion's Open Vision finished (spell_horrific_vision_open_vision).
        void StartVision(Unit* caster)
        {
            if (_visionStarted)
                return;
            _visionStarted = true;

            bool stormwind = IsStormwind();
            instance->DoOnPlayers([caster, stormwind](Player* player)
            {
                player->DestroyItemCount(ITEM_VESSEL_OF_HORRIFIC_VISIONS, 1, true);
                if (stormwind)
                    player->CastSpell(player, SPELL_VISION_TELEPORT_STORMWIND, true);
                else
                    player->NearTeleportTo(OrgrimmarCity);

                // The core sends 70210 with Wrathion as the source, which credits no player.
                GameEvents::Trigger(GAME_EVENT_OPEN_VISION, player, caster);
            });

            if (stormwind)
                _events.ScheduleEvent(EVENT_STORMWIND_ARRIVED, 3s);
        }

        // One drain tick for this player, as a whole number, keeping the fraction for the next tick.
        uint32 TakeDrain(Player* player)
        {
            float rate = DRAIN_NEUTRAL;
            switch (GetMadness(*player))
            {
                case Madness::Lost:
                    rate = DRAIN_LOST;
                    break;
                case Madness::Corrupted:
                    rate = DRAIN_CORRUPTED;
                    break;
                default:
                    break;
            }

            float& carry = _drainCarry[player->GetGUID()];
            carry += rate;
            float whole = std::floor(carry);
            carry -= whole;
            return uint32(whole);
        }

        void UpdateStage(Player* player) const
        {
            uint32 maxPower = player->GetMaxPower(POWER_ALTERNATE_POWER);
            if (!maxPower)
                return;

            uint32 pct = uint32(player->GetPower(POWER_ALTERNATE_POWER)) * 100 / maxPower;
            uint32 stage = pct >= STAGE_PARANOID_PCT ? SPELL_UNSETTLED : (pct >= STAGE_TERRIFIED_PCT ? SPELL_PARANOID : SPELL_TERRIFIED);
            for (uint32 spellId : { SPELL_UNSETTLED, SPELL_PARANOID, SPELL_TERRIFIED })
                if (spellId != stage)
                    player->RemoveAurasDueToSpell(spellId);
            if (!player->HasAura(stage))
                player->CastSpell(player, stage, true);
        }

        void Update(uint32 diff) override
        {
            _events.Update(diff);

            while (uint32 eventId = _events.ExecuteEvent())
            {
                switch (eventId)
                {
                    case EVENT_STORMWIND_ARRIVED:
                        instance->DoOnPlayers([](Player* player)
                        {
                            GameEvents::Trigger(GAME_EVENT_STORMWIND_ARRIVED, player, nullptr);
                        });
                        break;
                    case EVENT_EVACUATE:
                        instance->DoOnPlayers([](Player* player)
                        {
                            player->CastSpell(player, SPELL_EVACUATION_STORMWIND, true);
                            // Retail never showed what credits 162451 (spec 2); the run ends here, so credit it on arrival.
                            player->CastSpell(player, SPELL_SCENARIO_COMPLETE_CREDIT, true);
                        });
                        break;
                    default:
                        break;
                }
            }
        }

    private:
        static void RemoveSanity(Player* player)
        {
            for (uint32 spellId : { SPELL_SANITY_LOSE_EVERY_SECOND, SPELL_SANITY_POWER_BAR, SPELL_UNSETTLED, SPELL_PARANOID, SPELL_TERRIFIED })
                player->RemoveAurasDueToSpell(spellId);
        }

        // Alleria died: the bar goes away, Victory starts the 14 s Evacuation Protocol, then the exit-chamber teleport.
        // Both final chests stand in the exit chamber, out of sight until the teleport, so they are placed now.
        void StartFinale(Creature* alleria)
        {
            if (_finaleStarted)
                return;
            _finaleStarted = true;

            instance->DoOnPlayers([](Player* player)
            {
                RemoveSanity(player);
                player->CastSpell(player, SPELL_VICTORY, true);
            });
            alleria->SummonGameObject(GO_CORRUPTED_CHEST_FINAL, FinalChestPos, FinalChestRot, 1h, GO_SUMMON_TIMED_DESPAWN);
            alleria->SummonGameObject(GO_CORRUPTED_CHEST_EVACUATION, EvacChestPos, EvacChestRot, 1h, GO_SUMMON_TIMED_DESPAWN);
            _events.ScheduleEvent(EVENT_EVACUATE, 14s);
        }

        Madness GetMadness(Position const& pos) const
        {
            Madness result = Madness::Neutral;
            float best = AREA_MARKER_RANGE * AREA_MARKER_RANGE;
            for (auto const& [markerPos, madness] : _areaMarkers)
            {
                float dist = markerPos.GetExactDist2dSq(pos);
                if (dist <= best)
                {
                    best = dist;
                    result = madness;
                }
            }
            return result;
        }

        EventMap _events;
        bool _visionStarted;
        bool _finaleStarted;
        ObjectGuid _alleriaGUID;
        std::unordered_set<uint32> _slainDistrictBosses;
        std::unordered_set<ObjectGuid::LowType> _markedSpawns;
        std::vector<std::pair<Position, Madness>> _areaMarkers;
        std::unordered_map<ObjectGuid, float> _drainCarry;
    };

    instance_horrific_vision_script* GetVisionInstance(WorldObject const* object)
    {
        if (!object || !IsVisionMap(object->GetMapId()))
            return nullptr;
        return dynamic_cast<instance_horrific_vision_script*>(object->GetInstanceScript());
    }
}

using namespace HorrificVision;

class instance_horrific_vision : public InstanceMapScript
{
public:
    instance_horrific_vision(char const* scriptName, uint32 mapId) : InstanceMapScript(scriptName, mapId) { }

    InstanceScript* GetInstanceScript(InstanceMap* map) const override
    {
        return new instance_horrific_vision_script(map);
    }
};

// 305200 - Enter the Vision of N'Zoth (Stormwind), 302608 - (Orgrimmar)
class spell_horrific_vision_enter_prompt : public SpellScript
{
    SpellCastResult CheckRequirements()
    {
        Player* player = GetCaster()->ToPlayer();
        if (!player)
            return SPELL_FAILED_BAD_TARGETS;

        // Group runs need the LFG party path, which is not built (instances are entered solo).
        if (player->GetGroup())
            return SPELL_FAILED_CANT_DO_THAT_RIGHT_NOW;

        if (!player->HasItemOrGemWithIdEquipped(ITEM_ASHJRAKAMAS_SHROUD_OF_RESOLVE, 1))
        {
            SetCustomCastResultMessage(SPELL_CUSTOM_ERROR_REQUIRES_ASHJRAKAMAS_SHROUD_OF_RESOLVE);
            return SPELL_FAILED_CUSTOM_ERROR;
        }

        if (!player->HasItemCount(ITEM_VESSEL_OF_HORRIFIC_VISIONS, 1))
        {
            SetCustomCastResultMessage(SPELL_CUSTOM_ERROR_REQUIRES_VESSEL_OF_HORRIFIC_VISIONS);
            return SPELL_FAILED_CUSTOM_ERROR;
        }

        return SPELL_CAST_OK;
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(spell_horrific_vision_enter_prompt::CheckRequirements);
    }
};

// 294620 - Enter the Vision (Stormwind), 294651 - (Orgrimmar): SCRIPT_EFFECT that retail answers with an LFG join of
// dungeon 1993 / 1995. Here: a fresh instance at difficulty 152 for the solo player and a teleport to the lobby.
class spell_horrific_vision_enter : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return sMapStore.LookupEntry(MAP_VISION_STORMWIND) && sMapStore.LookupEntry(MAP_VISION_ORGRIMMAR);
    }

    void HandleScript(SpellEffIndex /*effIndex*/)
    {
        Player* player = GetHitPlayer();
        if (!player)
            return;

        bool stormwind = GetSpellInfo()->Id == SPELL_ENTER_STORMWIND;
        uint32 mapId = stormwind ? MAP_VISION_STORMWIND : MAP_VISION_ORGRIMMAR;
        Position const& lobby = stormwind ? StormwindLobby : OrgrimmarLobby;

        // District quests are per run (made repeatable in the DB): a run left unfinished must not block the next one.
        if (stormwind)
            for (uint32 questId : StormwindDistrictQuests)
                if (player->GetQuestStatus(questId) != QUEST_STATUS_NONE && player->GetQuestStatus(questId) != QUEST_STATUS_REWARDED)
                    player->RemoveActiveQuest(questId);

        SavePreviousDifficulty(player);
        player->SetDungeonDifficultyID(DIFFICULTY_HORRIFIC_VISION);
        player->SetRecentInstance(mapId, 0);    // every run is a new vision, never the last one
        // A refused transfer (instance limit, access checks) returns false here: give the difficulty back at once.
        if (!player->TeleportTo(mapId, lobby.GetPositionX(), lobby.GetPositionY(), lobby.GetPositionZ(), lobby.GetOrientation()))
            RestorePreviousDifficulty(player);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_horrific_vision_enter::HandleScript, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT);
    }
};

// 311996 - Open Vision (Image of Wrathion, 10 s cast)
class spell_horrific_vision_open_vision : public SpellScript
{
    void HandleAfterCast()
    {
        if (instance_horrific_vision_script* vision = GetVisionInstance(GetCaster()))
            vision->StartVision(GetCaster());
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_horrific_vision_open_vision::HandleAfterCast);
    }
};

// 287769 - N'Zoth's Awareness (sanity tick from 289995)
class spell_horrific_vision_nzoths_awareness : public SpellScript
{
    void HandleEnergize(SpellEffIndex /*effIndex*/)
    {
        Player* player = GetHitPlayer();
        instance_horrific_vision_script* vision = GetVisionInstance(player);
        if (!player || !vision)
        {
            PreventHitDefaultEffect(EFFECT_0);
            return;
        }

        SetEffectValue(-SpellEffectValue(vision->TakeDrain(player)));
    }

    void HandleAfterHit()
    {
        if (Player* player = GetHitPlayer())
            if (instance_horrific_vision_script* vision = GetVisionInstance(player))
                vision->UpdateStage(player);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_horrific_vision_nzoths_awareness::HandleEnergize, EFFECT_0, SPELL_EFFECT_ENERGIZE);
        AfterHit += SpellHitFn(spell_horrific_vision_nzoths_awareness::HandleAfterHit);
    }
};

// 291330 - Exit Vision (dummy after the exit prompt 291611): back to the Chamber of Heart
class spell_horrific_vision_exit : public SpellScript
{
    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        Player* player = GetHitPlayer();
        if (!player || !IsVisionMap(player->GetMapId()))
            return;

        player->SetRecentInstance(player->GetMapId(), 0);
        player->TeleportTo(MAP_CHAMBER_OF_HEART, ChamberOfHeartExit.GetPositionX(), ChamberOfHeartExit.GetPositionY(),
            ChamberOfHeartExit.GetPositionZ(), ChamberOfHeartExit.GetOrientation());
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_horrific_vision_exit::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// Rewards the Stormwind district quests as soon as they complete, as retail does (no turn-in NPC exists), and gives
// back the player's own dungeon difficulty if an entry ever stored one without reaching the vision.
class player_horrific_vision_district_quests : public PlayerScript
{
public:
    player_horrific_vision_district_quests() : PlayerScript("player_horrific_vision_district_quests") { }

    void OnLogin(Player* player, bool /*firstLogin*/) override
    {
        if (!IsVisionMap(player->GetMapId()))
            RestorePreviousDifficulty(player);
    }

    void OnMapChanged(Player* player) override
    {
        if (!IsVisionMap(player->GetMapId()))
            RestorePreviousDifficulty(player);
    }

    void OnQuestStatusChange(Player* player, uint32 questId) override
    {
        if (!StormwindDistrictQuests.count(questId) || player->GetMapId() != MAP_VISION_STORMWIND
            || player->GetQuestStatus(questId) != QUEST_STATUS_COMPLETE)
            return;

        // Not from inside the status change itself: reward on the player's next update.
        player->m_Events.AddEventAtOffset([player, questId]()
        {
            Quest const* quest = sObjectMgr->GetQuestTemplate(questId);
            if (quest && player->GetQuestStatus(questId) == QUEST_STATUS_COMPLETE && player->CanRewardQuest(quest, false))
                player->RewardQuest(quest, LootItemType::Item, 0, player, false);
        }, 1ms);
    }
};

void AddSC_horrific_visions()
{
    new player_horrific_vision_district_quests();
    new instance_horrific_vision("instance_horrific_vision_orgrimmar", MAP_VISION_ORGRIMMAR);
    new instance_horrific_vision("instance_horrific_vision_stormwind", MAP_VISION_STORMWIND);
    RegisterSpellScript(spell_horrific_vision_enter_prompt);
    RegisterSpellScript(spell_horrific_vision_enter);
    RegisterSpellScript(spell_horrific_vision_open_vision);
    RegisterSpellScript(spell_horrific_vision_nzoths_awareness);
    RegisterSpellScript(spell_horrific_vision_exit);
}
