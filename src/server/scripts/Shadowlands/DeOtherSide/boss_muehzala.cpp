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
 * Mueh'zala (DungeonEncounter 2396, trinitycore-tracking/sl-dungeons/de-other-side/bosses/muehzala.md).
 *
 * He never dies. The encounter ends on a script when his health reaches 10% (Warcraft Logs: 28 pulls, none above 10.5%; the capture
 * ends at 9.0%): achievement credit, kill credit, Captive Loa, the cache, ENCOUNTER_END success.
 *
 * Loop (Heroic capture, three attempts; Warcraft Logs agrees for the first cycle): stage 1 for 31 s (Cosmic Artifice, Soulcrusher,
 * Master of Death), Shatter Reality (10 s cast, kills everyone within 80 yd of a site unless they took a Deathgate), stage 2
 * (Shattered Visages guard the Primeval Grasp totems; Coalescing 20 s after the Shatter began, 25 s long), then stage 1 again
 * (67 s up to the next Shatter, Medium: one capture).
 *
 * Not scripted:
 * - Stellar Cloud 335003 and Crush 334961: both punish players away from the boss, and neither the platform extent nor the
 *   trigger distance is in any data.
 * - Bwonsamdi's Fury 327891 damage to the boss (a PERIODIC_DUMMY with no handler; the capture's health drops of 5.9% at +1 s
 *   after each Fury are the evidence). Its stun 330101 and the haste 327893 are scripted.
 * - The Shatter ship (171442/171443/171446 and the 18 riders 171444), Darkness 348426 (energy bar 241), the boss riding the
 *   167561 site vehicles (326104/326107), the Visages riding 167561 (327429/327442-4), the helper 171472 and the shackled
 *   Bwonsamdi 168141/168144/169814: no seat data, cosmetic.
 * - The Incantation pull 325352 needs jump_charge_params 433 (not live); it is cast and does nothing until the row exists.
 * - The sites at (2739.95, -2318.11) and (2740.4, -2130.55): the capture's north site has z 17.8 (the boss ship) and no verified
 *   floor or Deathgate height.
 */

#include "AreaTrigger.h"
#include "AreaTriggerAI.h"
#include "Containers.h"
#include "Conversation.h"
#include "GameObject.h"
#include "GameObjectAI.h"
#include "InstanceScript.h"
#include "Map.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "SpellAuraEffects.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "SpellScript.h"
#include "TemporarySummon.h"
#include "de_other_side.h"
#include <array>

enum MuehzalaSpells
{
    // Stage 1
    SPELL_COSMIC_ARTIFICE           = 325725,   // 1.5 s cast, 6 s aura; on expiry Cosmic Collapse 325647 at the player and Unbound Cosmos 325807
    SPELL_COSMIC_COLLAPSE           = 325647,   // creates the 9 s areatrigger 24003 (create properties 20073)
    SPELL_COSMIC_COLLAPSE_DAMAGE    = 325691,   // 10 yd, when that areatrigger ends
    SPELL_UNBOUND_COSMOS            = 325807,
    SPELL_SOULCRUSHER               = 327646,
    SPELL_CRUSHED_SOUL              = 327649,
    SPELL_MASTER_OF_DEATH           = 325258,   // 5 s cast, then a 10 s channel; the three waves land at the cast end and every 5 s
    SPELL_MASTER_OF_DEATH_WAVE_A    = 325279,   // areatrigger 23953, create properties 20008
    SPELL_MASTER_OF_DEATH_WAVE_B    = 325274,   // 20006
    SPELL_MASTER_OF_DEATH_WAVE_C    = 345676,   // 22086
    SPELL_MASTER_OF_DEATH_DAMAGE    = 334913,

    // Shatter
    SPELL_SHATTER_REALITY           = 326171,   // 10 s cast, INSTAKILL within 80 yd of the cast point
    SPELL_SHATTERED                 = 326108,   // his aura from the Shatter to the end of stage 2
    SPELL_SHATTERED_END             = 326211,   // removes 326108 and Coalescing
    SPELL_COALESCING                = 334970,   // 25 s aura
    SPELL_INCANTATION               = 328776,
    SPELL_INCANTATION_JUMP          = 325352,   // jump to him (jump_charge_params 433)
    SPELL_DEATHGATE                 = 324698,   // Bwonsamdi's 4 s cast, then the 10 s areatrigger 23903
    SPELL_DEATHGATE_PLAYER          = 324702,   // the player's cast on entering: transform, 2 s
    SPELL_DEATHGATE_PLAYER_END      = 324690,   // 1 s
    SPELL_PRIMEVAL_GRASP_CLICK      = 335032,   // 1 s cast by the clicker on the totem

    // Stage 2
    SPELL_VISAGE_DEATH              = 327554,
    SPELL_BWONSAMDI_SCALE           = 329919,
    SPELL_BWONSAMDI_VISUAL          = 45313,
    SPELL_BWONSAMDI_FURY            = 327891,
    SPELL_BWONSAMDI_FURY_STUN       = 330101,   // 4 s stun on the boss
    SPELL_BWONSAMDI_FERVOR          = 327893,   // +30% haste and speed, 10 s
    SPELL_FERVOR_FORCE_CAST         = 331547,

    // The end
    SPELL_ACHIEVEMENT_CREDIT        = 335978,
    SPELL_CAPTIVE_LOA               = 334810
};

enum MuehzalaEvents
{
    EVENT_COSMIC_ARTIFICE = 1,
    EVENT_SOULCRUSHER,
    EVENT_MASTER_OF_DEATH,
    EVENT_SHATTER_REALITY
};

enum Texts
{
    // Mueh'zala. Group ids as in the live creature_text rows; 2 and 6 are the two raid boss emotes (proposed rows)
    SAY_AGGRO                       = 0,    // Dis be da end of Bwonsamdi, and da end of you!
    SAY_COSMIC_ARTIFICE             = 1,    // Darkness be mine ta command!
    SAY_MASTER_OF_DEATH_EMOTE       = 2,
    SAY_WAVE_C                      = 3,    // Ya life be mine!
    SAY_WAVE_A                      = 4,    // Da shadows take ya!
    SAY_WAVE_B                      = 5,    // So small, so weak!
    SAY_SHATTER_EMOTE               = 6,
    SAY_WIPE                        = 7,    // To da Maw wit' ya souls! No one be defyin' Mueh'zala!
    SAY_INCANTATION                 = 8     // Dere be no escapin' death!
};

enum MuehzalaCreatures
{
    NPC_BWONSAMDI_DEATHGATE         = 166659,
    NPC_PRIMEVAL_GRASP              = 171685,
    NPC_SHATTERED_VISAGE            = 168326,
    NPC_BWONSAMDI_FURY              = 169506,
    NPC_KILL_CREDIT_DUNGEON_BOSSES  = 169002
};

enum MuehzalaMisc
{
    GO_ARENA_BLOCKER                = 364333,
    GO_MUEHZALA_CACHE               = 355295,
    CONVERSATION_SHATTER            = 16503,    // Bwonsamdi's helper and Mueh'zala, 2 s after the Shatter began
    CONVERSATION_END                = 15442,    // actor 0: his spawn (conversation_actors)
    WORLD_STATE_MUEHZALA_COMPLETE   = 18817,

    GUID_VISAGE_DIED                = 1,
    GUID_TOTEM_FREED                = 2,

    ACTION_TOTEM_ENABLE             = 1,
    ACTION_TOTEM_DISABLE,
    ACTION_TOTEM_FREED,
    ACTION_TOTEM_RESET,
    ACTION_GIANT_FURY               = 100,

    TOTEM_COUNT                     = 4
};

static constexpr float EndHealthPct = 10.0f;
static constexpr Seconds EvadeRespawnDelay = 30s;                  // not in the capture: default

static constexpr Milliseconds CosmicArtificeFirst = 2600ms;        // sniff 2 / 2 / 3 s, Warcraft Logs 2.6 s
static constexpr Milliseconds ShatterCastTime = 10s;
static constexpr Milliseconds ConversationDelay = 2s;              // sniff: 34.0 s with the Shatter at 32.0 s
static constexpr Milliseconds CoalescingDelay = 20s;               // from the Shatter start, High (every source)
static constexpr Milliseconds CoalescingLength = 25s;
static constexpr Milliseconds StageTwoEnd = CoalescingDelay + CoalescingLength;
static constexpr Milliseconds FuryDelay = 1s;                      // after the stage ends
static constexpr Milliseconds FuryInterval = 4s;                   // one stun each: the next giant waits for it to end
static constexpr Milliseconds FuryGiantLife = 4s;
static constexpr Milliseconds EndConversationDelay = 1s;
static constexpr float DeathgateForward = 16.0f;
static constexpr float DeathgateSide = 22.7f;

struct MuehzalaStep
{
    uint32 EventId;
    Milliseconds Offset;
};

// From the pull (Heroic capture, three attempts agree; Warcraft Logs Heroic/Normal n=3 for the times that differ by a second)
static constexpr std::array<MuehzalaStep, 6> OpeningLadder =
{ {
    { EVENT_COSMIC_ARTIFICE,    CosmicArtificeFirst },
    { EVENT_SOULCRUSHER,        6200ms },
    { EVENT_MASTER_OF_DEATH,    9500ms },
    { EVENT_SOULCRUSHER,        26500ms },
    { EVENT_COSMIC_ARTIFICE,    28500ms },
    { EVENT_SHATTER_REALITY,    31000ms }
} };

// From the end of Coalescing (kill: 77 s -> second Shatter 144.1 s; Medium, one sample)
static constexpr std::array<MuehzalaStep, 10> StageOneLadder =
{ {
    { EVENT_COSMIC_ARTIFICE,    10s },
    { EVENT_SOULCRUSHER,        14s },
    { EVENT_MASTER_OF_DEATH,    17s },
    { EVENT_SOULCRUSHER,        34s },
    { EVENT_COSMIC_ARTIFICE,    37s },
    { EVENT_SOULCRUSHER,        45s },
    { EVENT_COSMIC_ARTIFICE,    47s },
    { EVENT_MASTER_OF_DEATH,    50s },
    { EVENT_SOULCRUSHER,        66s },
    { EVENT_SHATTER_REALITY,    67s }
} };

// The vehicles 167561 at the Shatter sites: the Master of Death waves start at their position and facing, the Shatter is centred on them.
// The capture's north site (2739.95, -2318.11, boss ship at z 17.8) is left out: its floor height and Deathgate z were not verified.
static constexpr std::array<Position, 2> Sites =
{ {
    { 2858.1875f, -2223.7517f, -18.357666f, 0.0f },         // east
    { 2625.125f,  -2223.2883f, -18.357666f, 3.18529f }      // west
} };

struct ArenaCorner
{
    Position Totem;     // Primeval Grasp 171685 and its collision blocker
    Position Visage;    // Shattered Visage spawn (capture)
    Position Giant;     // Bwonsamdi 169506 (capture; the south-west one is derived: Low)
};

static constexpr std::array<ArenaCorner, TOTEM_COUNT> Corners =
{ {
    { { 2637.74f, -2128.98f, -2.2753f }, { 2615.4f, -2098.5f, -2.8f },  { 2632.9f, -2121.1f, -2.8f } },
    { { 2636.89f, -2315.72f, -1.6265f }, { 2611.9f, -2351.7f, -2.8f },  { 2631.4f, -2323.6f, -2.5f } },
    { { 2843.33f, -2130.08f, -1.6418f }, { 2865.3f, -2098.5f, -2.8f },  { 2851.8f, -2122.9f, -2.3f } },
    { { 2842.96f, -2317.02f, -2.5873f }, { 2865.3f, -2351.7f, -2.8f },  { 2848.6f, -2327.1f, -2.7f } }
} };

// The capture's gate use landed the player 6.7 yd from the totem towards the Visage's corner (30% of the way)
static constexpr float LandingFraction = 0.3f;

static Position const ChestPos = { 2899.0642f, -2216.0886f, -18.498238f, 3.3269281f };

// 166608 - Mueh'zala
struct boss_muehzala : public BossAI
{
    boss_muehzala(Creature* creature) : BossAI(creature, DATA_MUEHZALA), _ending(false), _shattered(false), _siteIndex(0), _clicks(0) { }

    void Reset() override
    {
        BossAI::Reset();
        _ending = false;
        _shattered = false;
        _clicks = 0;
        _totemFreed.fill(false);
        _visageTotem.clear();
        _giants.clear();
        me->m_Events.KillAllEvents(false);
        SetCombatMovement(false);
        // The template carries the pre-fight flags (848: immune to players and NPCs); the pull's own state has neither (559168)
        me->RemoveUnitFlag(UNIT_FLAG_IMMUNE_TO_PC | UNIT_FLAG_IMMUNE_TO_NPC | UNIT_FLAG_UNINTERACTIBLE);
        me->SetReactState(REACT_AGGRESSIVE);
        DespawnBlockers();
        ResetTotems();
    }

    void JustAppeared() override
    {
        BossAI::JustAppeared();

        // After an instance reload with the encounter done he is the captive again, not a pull
        if (instance && instance->GetBossState(DATA_MUEHZALA) == DONE)
        {
            EnterCaptiveState();
            return;
        }

        // The spawn row carries a two-node path that is not a real route: he stands where the fight is
        me->SetDefaultMovementType(IDLE_MOTION_TYPE);
        me->GetMotionMaster()->Initialize();
        _siteIndex = urand(0, Sites.size() - 1);
        MoveToSite();
    }

    // He stands on the vehicle at the active site: the waves start there and the Shatter is centred on it
    void MoveToSite()
    {
        Position const& site = GetSite();
        me->SetHomePosition(site);
        me->NearTeleportTo(site);
    }

    void JustEngagedWith(Unit* who) override
    {
        if (_ending)
            return;

        BossAI::JustEngagedWith(who);

        if (instance)
            instance->SendEncounterUnit(ENCOUNTER_FRAME_ENGAGE, me, 1);

        Talk(SAY_AGGRO);
        ResetTotems();
        SpawnBlockers();
        ScheduleLadder(OpeningLadder.data(), OpeningLadder.size());
    }

    void JustSummoned(Creature* summon) override
    {
        summons.Summon(summon);

        if (summon->GetEntry() == NPC_BWONSAMDI_FURY && _clicks > 0 && _clicks <= 3)
            if (CreatureAI* ai = summon->AI())
                ai->DoAction(_clicks - 1);
    }

    Position const& GetSite() const { return Sites[_siteIndex]; }

    // Where a Deathgate sends the player: a corner whose totem is still shackled, if any
    Position GetDeathgateLanding() const
    {
        std::vector<uint8> corners;
        for (uint8 i = 0; i < TOTEM_COUNT; ++i)
            if (!_totemFreed[i])
                corners.push_back(i);
        if (corners.empty())
            for (uint8 i = 0; i < TOTEM_COUNT; ++i)
                corners.push_back(i);

        ArenaCorner const& corner = Corners[Trinity::Containers::SelectRandomContainerElement(corners)];
        return Position(corner.Totem.GetPositionX() + (corner.Visage.GetPositionX() - corner.Totem.GetPositionX()) * LandingFraction,
            corner.Totem.GetPositionY() + (corner.Visage.GetPositionY() - corner.Totem.GetPositionY()) * LandingFraction,
            corner.Visage.GetPositionZ(), corner.Totem.GetAbsoluteAngle(corner.Visage));
    }

    void SetGUID(ObjectGuid const& guid, int32 id) override
    {
        switch (id)
        {
            case GUID_VISAGE_DIED:
            {
                auto itr = _visageTotem.find(guid);
                if (itr == _visageTotem.end())
                    break;

                if (Creature* totem = ObjectAccessor::GetCreature(*me, _totems[itr->second]))
                    totem->AI()->DoAction(ACTION_TOTEM_ENABLE);
                break;
            }
            case GUID_TOTEM_FREED:
                for (uint8 i = 0; i < TOTEM_COUNT; ++i)
                    if (_totems[i] == guid && !_totemFreed[i])
                        FreeTotem(i);
                break;
            default:
                break;
        }
    }

    void OnSpellCast(SpellInfo const* spell) override
    {
        switch (spell->Id)
        {
            case SPELL_SOULCRUSHER:
                if (Unit* victim = me->GetVictim())
                    if (DoCast(victim, SPELL_CRUSHED_SOUL) != SPELL_CAST_OK)
                        DoCast(victim, SPELL_CRUSHED_SOUL, CastSpellExtraArgs(TRIGGERED_FULL_MASK));
                break;
            case SPELL_SHATTER_REALITY:
                OnShatterComplete();
                break;
            default:
                break;
        }
    }

    void ExecuteEvent(uint32 eventId) override
    {
        switch (eventId)
        {
            case EVENT_COSMIC_ARTIFICE:
                Talk(SAY_COSMIC_ARTIFICE);
                DoCastSelf(SPELL_COSMIC_ARTIFICE);
                break;
            case EVENT_SOULCRUSHER:
                DoCastVictim(SPELL_SOULCRUSHER);
                break;
            case EVENT_MASTER_OF_DEATH:
                StartMasterOfDeath();
                break;
            case EVENT_SHATTER_REALITY:
                StartShatter();
                break;
            default:
                break;
        }
    }

    // The 10 s channel and the Shatter cast keep BossAI's events (and its scheduler) waiting: what happens during them is on m_Events
    void StartMasterOfDeath()
    {
        Talk(SAY_MASTER_OF_DEATH_EMOTE);
        me->CastSpell(GetSite(), SPELL_MASTER_OF_DEATH, CastSpellExtraArgs(TRIGGERED_NONE));

        // Three shapes in a different order in each of the three captured casts (Low)
        std::array<uint32, 3> waves = { SPELL_MASTER_OF_DEATH_WAVE_A, SPELL_MASTER_OF_DEATH_WAVE_B, SPELL_MASTER_OF_DEATH_WAVE_C };
        Trinity::Containers::RandomShuffle(waves);

        for (uint32 i = 0; i < waves.size(); ++i)
        {
            uint32 spellId = waves[i];

            // The line comes four seconds before its wave
            me->m_Events.AddEventAtOffset([this, spellId]()
            {
                if (_ending || _shattered)
                    return;

                Talk(WaveText(spellId));
            }, 1s + 5s * i);

            me->m_Events.AddEventAtOffset([this, spellId]()
            {
                if (_ending || _shattered)
                    return;

                me->CastSpell(me, spellId, CastSpellExtraArgs(TRIGGERED_FULL_MASK));
            }, 5s * (i + 1));
        }
    }

    static uint32 WaveText(uint32 spellId)
    {
        switch (spellId)
        {
            case SPELL_MASTER_OF_DEATH_WAVE_A:  return SAY_WAVE_A;
            case SPELL_MASTER_OF_DEATH_WAVE_B:  return SAY_WAVE_B;
            default:                            return SAY_WAVE_C;
        }
    }

    void StartShatter()
    {
        events.Reset();
        Talk(SAY_SHATTER_EMOTE);
        if (me->CastSpell(GetSite(), SPELL_SHATTER_REALITY, CastSpellExtraArgs(TRIGGERED_NONE)) != SPELL_CAST_OK)
            me->CastSpell(GetSite(), SPELL_SHATTER_REALITY, CastSpellExtraArgs(TRIGGERED_IGNORE_CAST_IN_PROGRESS));

        Position const& site = GetSite();
        float forward = site.GetOrientation();
        float left = forward + float(M_PI_2);
        for (float side : { DeathgateSide, -DeathgateSide })
        {
            Position gate(site.GetPositionX() + std::cos(forward) * DeathgateForward + std::cos(left) * side,
                site.GetPositionY() + std::sin(forward) * DeathgateForward + std::sin(left) * side, site.GetPositionZ(), forward);
            me->SummonCreature(NPC_BWONSAMDI_DEATHGATE, gate, TEMPSUMMON_TIMED_DESPAWN, 16s);
        }

        me->m_Events.AddEventAtOffset([this]()
        {
            if (_ending)
                return;

            Conversation::CreateConversation(CONVERSATION_SHATTER, me, me->GetPosition(), ObjectGuid::Empty);
        }, ConversationDelay);
    }

    // The INSTAKILL has landed: stage 2
    void OnShatterComplete()
    {
        if (_ending)
            return;

        _shattered = true;
        me->SetUnitFlag(UNIT_FLAG_UNINTERACTIBLE);
        me->SetReactState(REACT_PASSIVE);
        me->AttackStop();
        DoCastSelf(SPELL_SHATTERED, CastSpellExtraArgs(TRIGGERED_FULL_MASK));
        SummonVisages();

        // The capture's wipe line comes with the INSTAKILL; the kills land on this update
        me->m_Events.AddEventAtOffset([this]()
        {
            if (_ending)
                return;

            bool anyAlive = false;
            me->GetMap()->DoOnPlayers([&anyAlive](Player* player)
            {
                if (player->IsAlive())
                    anyAlive = true;
            });
            if (!anyAlive)
                Talk(SAY_WIPE);
        }, 500ms);

        me->m_Events.AddEventAtOffset([this]()
        {
            if (_ending)
                return;

            CastOrForce(SPELL_COALESCING);

            // The capture swaps the vehicle he rides here (326104/326107): the next Master of Death and Shatter are at the other site
            _siteIndex = (_siteIndex + 1) % Sites.size();
            MoveToSite();
        }, CoalescingDelay - ShatterCastTime);

        me->m_Events.AddEventAtOffset([this]()
        {
            EndShatterStage();
        }, StageTwoEnd - ShatterCastTime);
    }

    void SummonVisages()
    {
        for (uint8 i = 0; i < TOTEM_COUNT; ++i)
        {
            if (_totemFreed[i])
                continue;

            ArenaCorner const& corner = Corners[i];
            Position pos = corner.Visage;
            pos.SetOrientation(pos.GetAbsoluteAngle(corner.Totem));
            if (TempSummon* visage = me->SummonCreature(NPC_SHATTERED_VISAGE, pos, TEMPSUMMON_MANUAL_DESPAWN))
                _visageTotem[visage->GetGUID()] = i;
        }
    }

    void FreeTotem(uint8 index)
    {
        _totemFreed[index] = true;
        ++_clicks;

        if (GameObject* blocker = ObjectAccessor::GetGameObject(*me, _blockers[index]))
            blocker->DespawnOrUnsummon();

        if (Creature* totem = ObjectAccessor::GetCreature(*me, _totems[index]))
            totem->AI()->DoAction(ACTION_TOTEM_FREED);

        if (TempSummon* giant = me->SummonCreature(NPC_BWONSAMDI_FURY, Corners[index].Giant, TEMPSUMMON_MANUAL_DESPAWN))
            _giants.push_back(giant->GetGUID());
    }

    void SpawnBlockers()
    {
        std::list<Creature*> totems;
        me->GetCreatureListWithEntryInGrid(totems, NPC_PRIMEVAL_GRASP, 400.0f);
        _totems.fill(ObjectGuid::Empty);
        for (Creature* totem : totems)
            for (uint8 i = 0; i < TOTEM_COUNT; ++i)
                if (totem->GetExactDist2d(Corners[i].Totem) < 3.0f)
                    _totems[i] = totem->GetGUID();

        for (uint8 i = 0; i < TOTEM_COUNT; ++i)
            if (GameObject* blocker = me->SummonGameObject(GO_ARENA_BLOCKER, Corners[i].Totem, QuaternionData::fromEulerAnglesZYX(Corners[i].Totem.GetOrientation(), 0.0f, 0.0f), 1h, GO_SUMMON_TIMED_DESPAWN))
                _blockers[i] = blocker->GetGUID();
    }

    void DespawnBlockers()
    {
        for (ObjectGuid& guid : _blockers)
        {
            if (GameObject* blocker = ObjectAccessor::GetGameObject(*me, guid))
                blocker->DespawnOrUnsummon();
            guid = ObjectGuid::Empty;
        }
    }

    void ResetTotems()
    {
        for (ObjectGuid const& guid : _totems)
            if (Creature* totem = ObjectAccessor::GetCreature(*me, guid))
                totem->AI()->DoAction(ACTION_TOTEM_RESET);
    }

    void ScheduleLadder(MuehzalaStep const* steps, size_t count)
    {
        for (size_t i = 0; i < count; ++i)
            events.ScheduleEvent(steps[i].EventId, steps[i].Offset);
    }

    // Stage 1 resumes: what the capture shows at the Coalescing end (326211 removes his auras, Incantation, Bwonsamdi's Fury)
    void EndShatterStage()
    {
        if (_ending || !_shattered)
            return;

        _shattered = false;
        DoCastSelf(SPELL_SHATTERED_END, CastSpellExtraArgs(TRIGGERED_FULL_MASK));
        me->RemoveAurasDueToSpell(SPELL_SHATTERED);
        me->RemoveUnitFlag(UNIT_FLAG_UNINTERACTIBLE);
        me->SetReactState(REACT_AGGRESSIVE);

        for (auto const& [guid, index] : _visageTotem)
            if (Creature* visage = ObjectAccessor::GetCreature(*me, guid))
                visage->DespawnOrUnsummon(1s);
        _visageTotem.clear();

        for (uint8 i = 0; i < TOTEM_COUNT; ++i)
            if (!_totemFreed[i])
                if (Creature* totem = ObjectAccessor::GetCreature(*me, _totems[i]))
                    totem->AI()->DoAction(ACTION_TOTEM_DISABLE);

        Talk(SAY_INCANTATION);
        DoCastSelf(SPELL_INCANTATION);
        PullPlayers();

        Milliseconds delay = FuryDelay;
        for (ObjectGuid const& guid : _giants)
        {
            me->m_Events.AddEventAtOffset([this, guid]()
            {
                if (_ending)
                    return;

                if (Creature* giant = ObjectAccessor::GetCreature(*me, guid))
                    giant->AI()->DoAction(ACTION_GIANT_FURY);
            }, delay);
            delay += FuryInterval;
        }
        _giants.clear();

        ScheduleLadder(StageOneLadder.data(), StageOneLadder.size());
    }

    // "Drawing all players to him": every player jumps to where he stands
    void PullPlayers()
    {
        Position dest = me->GetPosition();
        me->GetMap()->DoOnPlayers([&dest](Player* player)
        {
            if (player->IsAlive())
                player->CastSpell(dest, SPELL_INCANTATION_JUMP, CastSpellExtraArgs(TRIGGERED_FULL_MASK));
        });
    }

    // His health may not drop below one: the encounter ends at 10% without a death. The hit that crosses the line lands (the
    // capture ends at 9.0%); the end itself runs on the next update, outside the damage code.
    void DamageTaken(Unit* /*attacker*/, uint32& damage, DamageEffectType /*damageType*/, SpellInfo const* /*spellInfo*/) override
    {
        if (_ending)
        {
            damage = 0;
            return;
        }

        if (damage >= me->GetHealth())
            damage = me->GetHealth() - 1;

        if (me->GetHealth() - damage > me->CountPctFromMaxHealth(EndHealthPct))
            return;

        _ending = true;
        me->m_Events.AddEventAtOffset([this]()
        {
            EndEncounter();
        }, 1ms);
    }

    // Capture order in one tick: achievement credit, the cache, kill credit, Captive Loa, ENCOUNTER_END; conversation 15442 a second later
    void EndEncounter()
    {
        if (instance)
            instance->SendEncounterUnit(ENCOUNTER_FRAME_DISENGAGE, me);

        events.Reset();
        _giants.clear();
        _visageTotem.clear();
        me->m_Events.KillAllEvents(false);
        me->InterruptNonMeleeSpells(false);
        me->AttackStop();
        me->RemoveUnitFlag(UNIT_FLAG_UNINTERACTIBLE);
        me->RemoveAurasDueToSpell(SPELL_SHATTERED);
        me->RemoveAurasDueToSpell(SPELL_COALESCING);
        me->RemoveAurasDueToSpell(SPELL_BWONSAMDI_FURY_STUN);
        me->SetReactState(REACT_PASSIVE);

        summons.DespawnAll();
        DespawnBlockers();
        for (ObjectGuid const& guid : _totems)
            if (Creature* totem = ObjectAccessor::GetCreature(*me, guid))
                totem->AI()->DoAction(ACTION_TOTEM_FREED);

        CastOrForce(SPELL_ACHIEVEMENT_CREDIT);
        SummonCache();
        // 345900 (KILL_CREDIT 169002 on SRC_AREA_ENTRY) has no condition rows to find its targets: the credit is given directly
        me->GetMap()->DoOnPlayers([](Player* player)
        {
            player->KilledMonsterCredit(NPC_KILL_CREDIT_DUNGEON_BOSSES);
        });
        CastOrForce(SPELL_CAPTIVE_LOA);

        me->SetImmuneToAll(true);
        me->CombatStop(false);
        EngagementOver();
        me->setActive(false);

        // DONE: ENCOUNTER_END, boss kill credit and his door. He stays, shackled.
        if (instance)
        {
            instance->SetBossState(DATA_MUEHZALA, DONE);
            instance->DoUpdateWorldState(WORLD_STATE_MUEHZALA_COMPLETE, 1);
        }

        me->m_Events.AddEventAtOffset([this]()
        {
            Conversation::CreateConversation(CONVERSATION_END, me, me->GetPosition(), ObjectGuid::Empty);
        }, EndConversationDelay);
    }

    void CastOrForce(uint32 spellId)
    {
        if (DoCastSelf(spellId) != SPELL_CAST_OK)
            DoCastSelf(spellId, CastSpellExtraArgs(TRIGGERED_FULL_MASK));
    }

    // 334807 (SUMMON_OBJECT_WILD) has no usable destination here: the cache is summoned at the captured position. Its personal
    // loot is made by the core at the first use for everyone in the tap list (the chest's loot id 102658 has no rows live).
    void SummonCache()
    {
        GameObject* chest = me->SummonGameObject(GO_MUEHZALA_CACHE, ChestPos, QuaternionData(0.0f, 0.0f, -0.9957094f, 0.09253509f), 1h, GO_SUMMON_TIMED_DESPAWN);
        if (!chest)
            return;

        GuidUnorderedSet tapList;
        me->GetMap()->DoOnPlayers([&tapList](Player* player)
        {
            tapList.insert(player->GetGUID());
        });
        chest->SetTapList(std::move(tapList));
    }

    void EnterCaptiveState()
    {
        me->SetReactState(REACT_PASSIVE);
        me->SetImmuneToAll(true);
        me->SetHealth(me->CountPctFromMaxHealth(9.0f));
        me->AddAura(SPELL_CAPTIVE_LOA, me);
    }

    void UpdateAI(uint32 diff) override
    {
        scheduler.Update(diff);

        if (_ending)
            return;

        BossAI::UpdateAI(diff);
    }

    void EnterEvadeMode(EvadeReason /*why*/) override
    {
        if (_ending)
            return;

        if (instance)
        {
            instance->SendEncounterUnit(ENCOUNTER_FRAME_DISENGAGE, me);
            instance->DoRemoveAurasDueToSpellOnPlayers(SPELL_BWONSAMDI_FERVOR);
        }

        me->m_Events.KillAllEvents(false);
        summons.DespawnAll();
        DespawnBlockers();
        ResetTotems();
        _DespawnAtEvade(EvadeRespawnDelay);
    }

private:
    bool _ending;
    bool _shattered;
    uint8 _siteIndex;
    uint8 _clicks;
    std::array<bool, TOTEM_COUNT> _totemFreed = { };
    std::array<ObjectGuid, TOTEM_COUNT> _totems = { };
    std::array<ObjectGuid, TOTEM_COUNT> _blockers = { };
    std::unordered_map<ObjectGuid, uint8> _visageTotem;
    GuidVector _giants;
};

// 171685 - Primeval Grasp: clickable only from its Visage's death until the click. The spellclick row puts the flag on the template.
struct npc_dos_primeval_grasp : public ScriptedAI
{
    npc_dos_primeval_grasp(Creature* creature) : ScriptedAI(creature) { }

    void Reset() override
    {
        scheduler.CancelAll();
        me->SetVisible(true);
        me->RemoveNpcFlag(UNIT_NPC_FLAG_SPELLCLICK);
    }

    void DoAction(int32 action) override
    {
        switch (action)
        {
            case ACTION_TOTEM_ENABLE:
                me->SetNpcFlag(UNIT_NPC_FLAG_SPELLCLICK);
                break;
            case ACTION_TOTEM_DISABLE:
                me->RemoveNpcFlag(UNIT_NPC_FLAG_SPELLCLICK);
                break;
            case ACTION_TOTEM_FREED:
                me->RemoveNpcFlag(UNIT_NPC_FLAG_SPELLCLICK);
                // The freed totem is gone 4 s after the click; it is hidden, not despawned, so the next pull finds it
                scheduler.Schedule(4s, [this](TaskContext)
                {
                    me->SetVisible(false);
                });
                break;
            case ACTION_TOTEM_RESET:
                Reset();
                break;
            default:
                break;
        }
    }

    void UpdateAI(uint32 diff) override
    {
        scheduler.Update(diff);
    }

private:
    TaskScheduler scheduler;
};

// 168326 - Shattered Visage: Shattered Dominion comes from the addon aura; killing it makes its totem clickable
struct npc_dos_shattered_visage : public ScriptedAI
{
    npc_dos_shattered_visage(Creature* creature) : ScriptedAI(creature) { }

    void JustAppeared() override
    {
        me->RemoveUnitFlag(UNIT_FLAG_UNINTERACTIBLE);
        me->SetReactState(REACT_PASSIVE);
    }

    void JustDied(Unit* /*killer*/) override
    {
        DoCastSelf(SPELL_VISAGE_DEATH, CastSpellExtraArgs(TRIGGERED_FULL_MASK));
        me->DespawnOrUnsummon(1s);

        if (InstanceScript* instance = me->GetInstanceScript())
            if (Creature* boss = instance->GetCreature(DATA_MUEHZALA))
                boss->AI()->SetGUID(me->GetGUID(), GUID_VISAGE_DIED);
    }

    void UpdateAI(uint32 /*diff*/) override { }
};

// 169506 - Bwonsamdi, freed from a totem: yells when it appears (group = click order), hits Mueh'zala when Coalescing ends
struct npc_dos_bwonsamdi_fury : public ScriptedAI
{
    npc_dos_bwonsamdi_fury(Creature* creature) : ScriptedAI(creature) { }

    void JustAppeared() override
    {
        me->SetReactState(REACT_PASSIVE);
        DoCastSelf(SPELL_BWONSAMDI_SCALE);
        scheduler.Schedule(2s, [this](TaskContext)
        {
            DoCastSelf(SPELL_BWONSAMDI_VISUAL);
        });
    }

    void DoAction(int32 action) override
    {
        if (action == ACTION_GIANT_FURY)
        {
            Fury();
            return;
        }

        Talk(action);
    }

    // Capture: the stun lands as the cast starts, the cast takes 1 s (Fervor goes out with it), the giant is gone 4 s after the start.
    // Not scripted: the 5.9% health drop on the boss that follows (see the file header).
    void Fury()
    {
        InstanceScript* instance = me->GetInstanceScript();
        Creature* boss = instance ? instance->GetCreature(DATA_MUEHZALA) : nullptr;
        if (!boss || !boss->IsAlive())
            return;

        me->AddAura(SPELL_BWONSAMDI_FURY_STUN, boss);
        DoCastSelf(SPELL_BWONSAMDI_FURY);
        scheduler.Schedule(1s, [this](TaskContext)
        {
            if (InstanceScript* inst = me->GetInstanceScript())
                inst->DoCastSpellOnPlayers(SPELL_BWONSAMDI_FERVOR);
        });
        scheduler.Schedule(FuryGiantLife, [this](TaskContext)
        {
            me->DespawnOrUnsummon();
        });
    }

    void UpdateAI(uint32 diff) override
    {
        scheduler.Update(diff);
    }

private:
    TaskScheduler scheduler;
};

// 166659 - Bwonsamdi at a Deathgate: casts the gate on himself as he appears
struct npc_dos_deathgate_bwonsamdi : public ScriptedAI
{
    npc_dos_deathgate_bwonsamdi(Creature* creature) : ScriptedAI(creature) { }

    void JustAppeared() override
    {
        me->SetReactState(REACT_PASSIVE);
        DoCastSelf(SPELL_DEATHGATE);
    }

    void UpdateAI(uint32 /*diff*/) override { }
};

// 355295 - Mueh'zala's cache: the core makes the personal loot at the first use for the tap list. A player left out of it (an empty
// roll) would make the core rebuild and replace everyone's loot, so only a use before the first or by a player with loot goes through.
struct go_dos_muehzala_cache : public GameObjectAI
{
    using GameObjectAI::GameObjectAI;

    bool OnGossipHello(Player* player) override
    {
        return !me->m_personalLoot.empty() && !me->m_personalLoot.contains(player->GetGUID());
    }
};

// 325725 - Cosmic Artifice: when the 6 s aura runs out, Cosmic Collapse's areatrigger lands where the player stands and Unbound Cosmos
// hits the group. A dispel ends both (Journal: dispel it to reduce the damage over time and the group damage; Low: how much).
class spell_dos_cosmic_artifice : public AuraScript
{
    void HandleRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        if (GetTargetApplication()->GetRemoveMode() != AURA_REMOVE_BY_EXPIRE)
            return;

        Unit* caster = GetCaster();
        Unit* target = GetTarget();
        if (!caster || !caster->IsAlive() || !target->IsAlive())
            return;

        Position pos = target->GetPosition();
        caster->CastSpell(pos, SPELL_COSMIC_COLLAPSE, CastSpellExtraArgs(TRIGGERED_FULL_MASK));
        caster->CastSpell(pos, SPELL_UNBOUND_COSMOS, CastSpellExtraArgs(TRIGGERED_FULL_MASK));
    }

    void Register() override
    {
        AfterEffectRemove += AuraEffectRemoveFn(spell_dos_cosmic_artifice::HandleRemove, EFFECT_0, SPELL_AURA_PERIODIC_DAMAGE, AURA_EFFECT_HANDLE_REAL);
    }
};

// 325279, 325274, 345676 - Master of Death waves: the area starts at the Shatter site (their effect is TARGET_DEST_CASTER)
class spell_dos_master_of_death_wave : public SpellScript
{
    void SetDest(SpellDestination& dest)
    {
        Creature* caster = GetCaster()->ToCreature();
        if (!caster || !caster->IsAIEnabled())
            return;

        if (boss_muehzala* ai = CAST_AI(boss_muehzala, caster->AI()))
            dest.Relocate(ai->GetSite());
    }

    void Register() override
    {
        OnDestinationTargetSelect += SpellDestinationTargetSelectFn(spell_dos_master_of_death_wave::SetDest, EFFECT_0, TARGET_DEST_CASTER);
    }
};

// 335032 - the click on a Primeval Grasp totem (1 s cast): the boss frees the totem
class spell_dos_primeval_grasp_click : public SpellScript
{
    void HandleHit(SpellEffIndex /*effIndex*/)
    {
        Creature* totem = GetHitCreature();
        if (!totem)
            return;

        if (InstanceScript* instance = totem->GetInstanceScript())
            if (Creature* boss = instance->GetCreature(DATA_MUEHZALA))
                boss->AI()->SetGUID(totem->GetGUID(), GUID_TOTEM_FREED);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_dos_primeval_grasp_click::HandleHit, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 331547 - Bwonsamdi's Fury force-casts Fervor on SRC_AREA_ENTRY targets with no conditions: only players get it
class spell_dos_bwonsamdi_fervor_targets : public SpellScript
{
    void FilterTargets(std::list<WorldObject*>& targets)
    {
        targets.remove_if([](WorldObject const* target)
        {
            return !target->IsPlayer();
        });
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_dos_bwonsamdi_fervor_targets::FilterTargets, EFFECT_0, TARGET_UNIT_SRC_AREA_ENTRY);
    }
};

// 23953 (create properties 20006, 20008, 22086) - Master of Death wave, 1 s: damages whoever stands in it. The retail polygon has
// height 0 (the capture's Height and HeightTarget), which the core's polygon search reads as a slab of zero thickness that no player is
// inside, so the test is made here on the ground plane: the shapes are the ones of the create properties (x forward, y to the left).
struct at_dos_master_of_death : AreaTriggerAI
{
    using AreaTriggerAI::AreaTriggerAI;

    void OnCreate(Spell const* /*creatingSpell*/) override
    {
        Check();
    }

    void OnUpdate(uint32 /*diff*/) override
    {
        Check();
    }

    bool InShape(Position const& pos) const
    {
        float angle = at->GetOrientation();
        float dx = pos.GetPositionX() - at->GetPositionX();
        float dy = pos.GetPositionY() - at->GetPositionY();
        float x = dx * std::cos(angle) + dy * std::sin(angle);
        float y = -dx * std::sin(angle) + dy * std::cos(angle);

        switch (at->GetSpellId())
        {
            case SPELL_MASTER_OF_DEATH_WAVE_A:  return x >= 0.0f && x <= 100.0f && y >= 0.0f && y <= 50.0f;
            case SPELL_MASTER_OF_DEATH_WAVE_B:  return x >= 0.0f && x <= 100.0f && y >= -50.0f && y <= 0.0f;
            case SPELL_MASTER_OF_DEATH_WAVE_C:  return x >= 0.0f && x <= 30.0f && y >= -50.0f && y <= 50.0f;
            default:                            return false;
        }
    }

    void Check()
    {
        Unit* caster = at->GetCaster();
        if (!caster)
            return;

        at->GetMap()->DoOnPlayers([this, caster](Player* player)
        {
            if (!player->IsAlive() || _hit.contains(player->GetGUID()) || !InShape(player->GetPosition()))
                return;

            _hit.insert(player->GetGUID());
            caster->CastSpell(player, SPELL_MASTER_OF_DEATH_DAMAGE, CastSpellExtraArgs(TRIGGERED_FULL_MASK));
        });
    }

private:
    GuidUnorderedSet _hit;
};

// 24003 (create properties 20073) - Cosmic Collapse: detonates when it ends, 9 s after Cosmic Artifice ran out
struct at_dos_cosmic_collapse : AreaTriggerAI
{
    using AreaTriggerAI::AreaTriggerAI;

    void OnRemove() override
    {
        if (Unit* caster = at->GetCaster())
            if (caster->IsAlive())
                caster->CastSpell(at->GetPosition(), SPELL_COSMIC_COLLAPSE_DAMAGE, CastSpellExtraArgs(TRIGGERED_FULL_MASK));
    }
};

// 23903 (create properties 19954) - Deathgate: the player becomes a spirit for 2 s and appears at an arena corner, out of the Shatter
struct at_dos_deathgate : AreaTriggerAI
{
    using AreaTriggerAI::AreaTriggerAI;

    void OnUnitEnter(Unit* unit) override
    {
        Player* player = unit->ToPlayer();
        if (!player || !player->IsAlive())
            return;

        InstanceScript* instance = player->GetInstanceScript();
        Creature* boss = instance ? instance->GetCreature(DATA_MUEHZALA) : nullptr;
        if (!boss || !boss->IsAIEnabled())
            return;

        boss_muehzala* ai = CAST_AI(boss_muehzala, boss->AI());
        if (!ai)
            return;

        Position landing = ai->GetDeathgateLanding();
        player->CastSpell(landing, SPELL_DEATHGATE_PLAYER, CastSpellExtraArgs(TRIGGERED_FULL_MASK));
        player->NearTeleportTo(landing);
        player->m_Events.AddEventAtOffset([player]()
        {
            player->CastSpell(player, SPELL_DEATHGATE_PLAYER_END, CastSpellExtraArgs(TRIGGERED_FULL_MASK));
        }, 2s);
    }
};

void AddSC_boss_muehzala()
{
    RegisterDeOtherSideCreatureAI(boss_muehzala);
    RegisterDeOtherSideCreatureAI(npc_dos_primeval_grasp);
    RegisterDeOtherSideCreatureAI(npc_dos_shattered_visage);
    RegisterDeOtherSideCreatureAI(npc_dos_bwonsamdi_fury);
    RegisterDeOtherSideCreatureAI(npc_dos_deathgate_bwonsamdi);
    RegisterGameObjectAI(go_dos_muehzala_cache);
    RegisterSpellScript(spell_dos_cosmic_artifice);
    RegisterSpellScript(spell_dos_master_of_death_wave);
    RegisterSpellScript(spell_dos_primeval_grasp_click);
    RegisterSpellScript(spell_dos_bwonsamdi_fervor_targets);
    RegisterAreaTriggerAI(at_dos_master_of_death);
    RegisterAreaTriggerAI(at_dos_cosmic_collapse);
    RegisterAreaTriggerAI(at_dos_deathgate);
}
