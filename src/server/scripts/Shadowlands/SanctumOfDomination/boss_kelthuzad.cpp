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
 * Kel'Thuzad (Normal, DungeonEncounter 2422)
 *
 * Timers: DBM (DeadlyBossMods/DBM-Shadowlands, SanctumOfDomination/KelThuzad.lua, copy in
 * trinitycore-tracking/sod-encounters/dbm_ref/) is the primary source; the 2026-10-01 12.1.0.69933 sniff
 * (one solo Normal kill, see sod-encounters/kel_thuzad.md) is used where DBM has nothing (Remnant spells,
 * Deep Freeze, add summons) and confirmed the structure/order. Sniff first-cast offsets were within DBM's
 * ranges, except Frost Blast/Dark Evocation (+2-6s), so DBM values are used as given.
 *
 * Structure:
 *  - Stage One: Kel'Thuzad cannot die. At 0 health he does his cosmetic death, then casts Vengeful Destruction
 *    (23s, wipes the raid unless the Remnant is brought down in time) and March of the Forsaken (adds).
 *    Players walk up to him, get the "Enter the Phylactery" button and fight the Remnant of Kel'Thuzad.
 *  - The Remnant has three lives: at 66% and 33% it yells "I shall return again and again!", Undying Wrath follows
 *    ~11s later, the players are released and Kel'Thuzad returns with a Necrotic Surge stack (Stage One restarts).
 *    Its final death starts Stage Three.
 *
 * NOT implemented (unknown/needs spell scripts): Necrotic Surge/Empowerment effects, Glacial Spike counts,
 * Glacial/Foul Winds, spell behaviour of the dummy-effect spells (see ShadowlandsSoD_GAPS.md).
 */

#include "CreatureAI.h"
#include "CreatureAIImpl.h"
#include "InstanceScript.h"
#include "Map.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "SpellScript.h"
#include "SpellInfo.h"
#include "sanctum_of_domination.h"

enum KelThuzadSpells
{
    // Kel'Thuzad
    SPELL_ICE_SHARD                     = 348953,
    SPELL_SOUL_FRACTURE                 = 348071,
    SPELL_OBLIVIONS_ECHO                = 347291,
    SPELL_GLACIAL_WRATH                 = 346459,
    SPELL_FROST_BLAST                   = 348758,
    SPELL_DARK_EVOCATION                = 348629,
    SPELL_COSMETIC_DEATH                = 351625,
    SPELL_CLEAR_ALL_DEBUFFS             = 34098,
    SPELL_VENGEFUL_DESTRUCTION          = 352293,
    SPELL_MARCH_OF_THE_FORSAKEN         = 352899,
    SPELL_NECROTIC_SURGE                = 352051,
    SPELL_DEEP_FREEZE                   = 354638,
    SPELL_HOWLING_BLIZZARD              = 354198,
    SPELL_ONSLAUGHT_OF_THE_DAMNED       = 352348,

    // Phylactery entry/exit (sniffed player chain: 351908 aura -> button 348785 -> 354539 teleport + 348787 phase 16971;
    // after the Remnant yields: 351923 aura -> button 351924 -> 348794 -> 354591 teleport back). Teleport destinations are
    // spell_target_position rows (see kelthuzad_world.sql).
    SPELL_ENTER_PHYLACTERY_AVAILABLE    = 351908,
    SPELL_INSIDE_PHYLACTERY             = 348787,
    SPELL_EXIT_PHYLACTERY_AVAILABLE     = 351923,
    SPELL_ENTER_PHYLACTERY_TELEPORT     = 354539,
    SPELL_EXIT_PHYLACTERY_TELEPORT      = 354591,

    // Remnant of Kel'Thuzad
    SPELL_FREEZING_BLAST                = 356249,
    SPELL_SHADOW_FISSURE_CAST           = 352000,
    SPELL_SHADOW_FISSURE_FOLLOWUP       = 352002,
    SPELL_UNDYING_WRATH                 = 352355
};

enum KelThuzadEvents
{
    // Stage One
    EVENT_ICE_SHARD = 1,
    EVENT_SOUL_FRACTURE,
    EVENT_OBLIVIONS_ECHO,
    EVENT_GLACIAL_WRATH,
    EVENT_SUMMON_DEVOTED,
    EVENT_FROST_BLAST,
    EVENT_DARK_EVOCATION,
    EVENT_HOWLING_BLIZZARD,

    // Intermission
    EVENT_VENGEFUL_DESTRUCTION,
    EVENT_SUMMON_ADDS,
    EVENT_REVIVE,

    // Stage Three
    EVENT_DEEP_FREEZE,
    EVENT_FROST_BLAST_P3,
    EVENT_ONSLAUGHT_OF_THE_DAMNED,

    // Remnant
    EVENT_FREEZING_BLAST,
    EVENT_SHADOW_FISSURE,
    EVENT_SHADOW_FISSURE_FOLLOWUP,
    EVENT_UNDYING_WRATH
};

enum KelThuzadPhases
{
    PHASE_STAGE_ONE         = 1,
    PHASE_INTERMISSION      = 2,
    PHASE_STAGE_THREE       = 3
};

enum KelThuzadTexts
{
    // Kel'Thuzad
    SAY_AGGRO               = 0,  // "You've meddled with my affairs for the last time!"
    SAY_DEATH               = 1,  // "Wait! Stop! I can offer you unimaginable power!"
    SAY_UNUSED_IMPOSSIBLE   = 2,  // "No! Impossible! Death... answers... to me!" (sniffed, trigger unknown)

    // Remnant of Kel'Thuzad
    SAY_REMNANT_RETURN      = 0   // "I shall return again and again!"
};

enum KelThuzadActions
{
    ACTION_INTERMISSION_STARTED = 1,
    ACTION_REMNANT_YIELDED,
    ACTION_REMNANT_DIED,
    ACTION_RESET_ENCOUNTER
};

// The "Enter the Phylactery" button appeared when the sniffed player was ~5.5 yards from the immune Kel'Thuzad
static constexpr float PhylacteryButtonRange = 6.0f;
static constexpr Milliseconds PhylacteryButtonCheck = 500ms;

// Remnant lives: it yields at these health percentages (journal/guides: 66% and 33%), the third life ends in death
static constexpr int32 RemnantYieldPct[] = { 66, 33 };
static constexpr uint8 RemnantYields = std::size(RemnantYieldPct);

// ---- Stage One timers (DBM, Normal/Heroic/Mythic), measured from combat start (first cycle) or from the revive ----
struct StageOneTimers
{
    Milliseconds SoulFracture, OblivionsEcho, GlacialWrath, FrostBlast, DarkEvocation, HowlingBlizzard;
};
static constexpr StageOneTimers FirstCycle  = { 10s, 14s + 300ms, 24s, 45s,         49s,         89s };
static constexpr StageOneTimers LaterCycles = { 10s, 14s + 100ms, 24s + 600ms, 46s + 500ms, 49s + 500ms, 91s + 900ms };

static constexpr Milliseconds SoulFractureCd      = 32s + 700ms;
static constexpr Milliseconds OblivionsEchoSecond = 61s + 100ms; // DBM: first repeat after the first cast
static constexpr Milliseconds OblivionsEchoCd     = 37s;
static constexpr Milliseconds GlacialWrathCd      = 109s + 900ms;
static constexpr Milliseconds FrostBlastOdd       = 40s;          // after cast #1, #3...
static constexpr Milliseconds FrostBlastEven      = 69s + 100ms;  // after cast #2, #4...
static constexpr Milliseconds DarkEvocationCd     = 86s + 200ms;
static constexpr Milliseconds HowlingBlizzardCd   = 114s + 300ms;
static constexpr Milliseconds IceShardCd          = 6s;           // sniff only (6.1s median)
static constexpr Milliseconds DevotedFirst        = 28s + 700ms;  // sniff only
static constexpr Milliseconds DevotedCd           = 26s;          // sniff only

// Intermission offsets from the cosmetic death (sniffed: death 54.5, Vengeful Destruction/March 60.4, adds 64.5)
static constexpr Milliseconds VengefulDestructionDelay = 5s + 900ms;
static constexpr Milliseconds AddsDelay                = 10s;
// Remnant yell -> Undying Wrath / Kel'Thuzad returns (sniffed 11.0s and 10.4s)
static constexpr Milliseconds RemnantReturnDelay       = 10s + 700ms;

// ---- Stage Three (DBM: starts at the Remnant's death) ----
static constexpr Milliseconds FrostBlastP3First   = 7s + 400ms;
static constexpr Milliseconds FrostBlastP3Cd      = 12s + 900ms;
static constexpr Milliseconds OnslaughtFirst      = 45s + 100ms;  // Normal (Mythic 15.3s)
static constexpr Milliseconds DeepFreezeFirst     = 2s + 500ms;   // sniff only
static constexpr Milliseconds DeepFreezeCd        = 5s;           // sniff only
static constexpr Milliseconds HowlingBlizzardP3   = 22s + 500ms;  // sniff only

// Adds are summoned (the sniff static-spawn rows are copies of these positions)
Position const SoulReaverPositions[] =
{
    { 255.7f, -564.7f, 3707.9f, 2.33136f },
    { 262.3f, -553.7f, 3707.9f, 2.73123f }
};

Position const AbominationPositions[] =
{
    { 208.9f, -562.6f, 3707.9f, 0.724594f },
    { 242.3f, -572.0f, 3707.9f, 1.85737f }
};

Position const DevotedPositions[] =
{
    { 206.7f, -546.4f, 3707.9f, 3.63515f },
    { 252.7f, -563.5f, 3707.9f, 0.176781f },
    { 245.7f, -561.7f, 3707.9f, 2.90642f },
    { 220.5f, -551.5f, 3707.9f, 5.56187f },
    { 213.2f, -556.8f, 3707.9f, 0.816581f }
};

// Destinations measured from the SMSG_MOVE_TELEPORT packets of the 2026-10-01 sniff (both spells use TARGET_DEST_NEARBY_ENTRY
// without a usable creature in our data, so the destination is set here)
Position const PhylacteryEntryPos = { 243.35243f, -423.0295f, 3675.0308f, 4.8171086f };
Position const PhylacteryExitPos  = { 233.497f, -542.009f, 3707.75f, 0.0f };

// 354539 - Enter the Phylactery / 354591 - Exit the Phylactery
class spell_kelthuzad_phylactery_teleport : public SpellScript
{
    void SetDest(SpellDestination& dest)
    {
        dest.Relocate(GetSpellInfo()->Id == SPELL_ENTER_PHYLACTERY_TELEPORT ? PhylacteryEntryPos : PhylacteryExitPos);
    }

    void Register() override
    {
        OnDestinationTargetSelect += SpellDestinationTargetSelectFn(spell_kelthuzad_phylactery_teleport::SetDest, EFFECT_0, TARGET_DEST_NEARBY_ENTRY);
    }
};

// 175559 - Kel'Thuzad
struct boss_kelthuzad_sod : public BossAI
{
    boss_kelthuzad_sod(Creature* creature) : BossAI(creature, DATA_KELTHUZAD), _intermission(false), _stageThree(false), _firstCycle(true),
        _buttonTimer(0), _echoCasts(0), _frostBlastCasts(0) { }

    void Reset() override
    {
        _Reset();
        _intermission = false;
        _stageThree = false;
        _firstCycle = true;
        _buttonTimer = 0;
        RemovePhylacteryAuras();
        me->SetReactState(REACT_AGGRESSIVE);
        me->SetImmuneToAll(false);
        me->SetUninteractible(false);

        // Wipe/evade: the Remnant gets all three lives back
        if (Creature* remnant = instance->GetCreature(DATA_KELTHUZAD_REMNANT))
        {
            if (!remnant->IsAlive())
                remnant->Respawn(true);
            if (remnant->IsAIEnabled())
                remnant->AI()->DoAction(ACTION_RESET_ENCOUNTER);
        }
    }

    void JustEngagedWith(Unit* who) override
    {
        BossAI::JustEngagedWith(who);
        Talk(SAY_AGGRO);
        ScheduleStageOne();
    }

    void DamageTaken(Unit* /*attacker*/, uint32& damage, DamageEffectType /*damageType*/, SpellInfo const* /*spellInfo*/) override
    {
        // Kel'Thuzad cannot be defeated until the Remnant of Kel'Thuzad is destroyed
        if (_stageThree || _intermission || damage < me->GetHealth())
            return;

        damage = me->GetHealth() - 1;
        StartIntermission();
    }

    void DoAction(int32 action) override
    {
        switch (action)
        {
            case ACTION_REMNANT_YIELDED:
                // Players are released and Kel'Thuzad comes back after the Remnant's "I shall return" yell
                if (me->HasUnitState(UNIT_STATE_CASTING))
                    me->InterruptNonMeleeSpells(false, SPELL_VENGEFUL_DESTRUCTION);
                events.CancelEvent(EVENT_VENGEFUL_DESTRUCTION);
                events.ScheduleEvent(EVENT_REVIVE, RemnantReturnDelay, 0, PHASE_INTERMISSION);
                break;
            case ACTION_REMNANT_DIED:
                StartStageThree();
                break;
            default:
                break;
        }
    }

    void JustSummoned(Creature* summon) override
    {
        summons.Summon(summon);
        DoZoneInCombat(summon);
    }

    void JustDied(Unit* /*killer*/) override
    {
        _JustDied();
        Talk(SAY_DEATH);
    }

    void UpdateAI(uint32 diff) override
    {
        // The intermission has no victim, so events must run without one (and must not wait for the 23s Vengeful Destruction cast)
        if (!_intermission && !UpdateVictim())
            return;

        if (_intermission)
            UpdatePhylacteryButton(diff);

        events.Update(diff);

        if (!_intermission && me->HasUnitState(UNIT_STATE_CASTING))
            return;

        while (uint32 eventId = events.ExecuteEvent())
        {
            ExecuteEvent(eventId);

            if (!_intermission && me->HasUnitState(UNIT_STATE_CASTING))
                return;
        }
    }

    void ExecuteEvent(uint32 eventId) override
    {
        switch (eventId)
        {
            case EVENT_ICE_SHARD:
                DoCastVictim(SPELL_ICE_SHARD);
                events.Repeat(IceShardCd);
                break;
            case EVENT_SOUL_FRACTURE:
                DoCastVictim(SPELL_SOUL_FRACTURE);
                events.Repeat(SoulFractureCd);
                break;
            case EVENT_OBLIVIONS_ECHO:
                ++_echoCasts;
                DoCastAOE(SPELL_OBLIVIONS_ECHO);
                events.Repeat(_echoCasts == 1 ? OblivionsEchoSecond : OblivionsEchoCd);
                break;
            case EVENT_GLACIAL_WRATH:
                DoCastAOE(SPELL_GLACIAL_WRATH);
                events.Repeat(GlacialWrathCd);
                break;
            case EVENT_SUMMON_DEVOTED:
                me->SummonCreature(NPC_FROSTBOUND_DEVOTED, DevotedPositions[urand(0, std::size(DevotedPositions) - 1)], TEMPSUMMON_MANUAL_DESPAWN);
                events.Repeat(DevotedCd);
                break;
            case EVENT_FROST_BLAST:
                ++_frostBlastCasts;
                DoCastAOE(SPELL_FROST_BLAST);
                events.Repeat(_frostBlastCasts % 2 == 0 ? FrostBlastEven : FrostBlastOdd);
                break;
            case EVENT_DARK_EVOCATION:
                DoCastSelf(SPELL_DARK_EVOCATION);
                events.Repeat(DarkEvocationCd);
                break;
            case EVENT_HOWLING_BLIZZARD:
                DoCastSelf(SPELL_HOWLING_BLIZZARD);
                events.Repeat(HowlingBlizzardCd);
                break;
            case EVENT_VENGEFUL_DESTRUCTION:
                // 23s cast: the raid has to bring the Remnant down in time. March of the Forsaken is cast at the same moment (sniffed).
                DoCastSelf(SPELL_MARCH_OF_THE_FORSAKEN, true);
                DoCastSelf(SPELL_VENGEFUL_DESTRUCTION);
                break;
            case EVENT_SUMMON_ADDS:
                for (Position const& pos : SoulReaverPositions)
                    me->SummonCreature(NPC_SOUL_REAVER, pos, TEMPSUMMON_MANUAL_DESPAWN);
                for (Position const& pos : AbominationPositions)
                    me->SummonCreature(NPC_UNSTOPPABLE_ABOMINATION, pos, TEMPSUMMON_MANUAL_DESPAWN);
                break;
            case EVENT_REVIVE:
                Revive();
                break;
            case EVENT_DEEP_FREEZE:
                DoCastAOE(SPELL_DEEP_FREEZE);
                events.Repeat(DeepFreezeCd);
                break;
            case EVENT_FROST_BLAST_P3:
                DoCastAOE(SPELL_FROST_BLAST);
                events.Repeat(FrostBlastP3Cd);
                break;
            case EVENT_ONSLAUGHT_OF_THE_DAMNED:
                DoCastAOE(SPELL_ONSLAUGHT_OF_THE_DAMNED);
                break;
            default:
                break;
        }
    }

private:
    // Players walking up to the immune Kel'Thuzad get the "Enter the Phylactery" action button (aura overriding their bar)
    void UpdatePhylacteryButton(uint32 diff)
    {
        if (_buttonTimer > diff)
        {
            _buttonTimer -= diff;
            return;
        }

        _buttonTimer = uint32(PhylacteryButtonCheck.count());

        for (MapReference const& ref : me->GetMap()->GetPlayers())
        {
            Player* player = ref.GetSource();
            if (!player || !player->IsAlive() || player->IsGameMaster())
                continue;

            if (player->HasAura(SPELL_INSIDE_PHYLACTERY) || player->HasAura(SPELL_ENTER_PHYLACTERY_AVAILABLE))
                continue;

            if (player->IsWithinDist3d(me, PhylacteryButtonRange))
                player->CastSpell(player, SPELL_ENTER_PHYLACTERY_AVAILABLE, TRIGGERED_FULL_MASK);
        }
    }

    void RemovePhylacteryAuras()
    {
        instance->DoRemoveAurasDueToSpellOnPlayers(SPELL_ENTER_PHYLACTERY_AVAILABLE);
        instance->DoRemoveAurasDueToSpellOnPlayers(SPELL_EXIT_PHYLACTERY_AVAILABLE);
    }

    void ScheduleStageOne()
    {
        StageOneTimers const& t = _firstCycle ? FirstCycle : LaterCycles;
        _echoCasts = 0;
        _frostBlastCasts = 0;

        events.SetPhase(PHASE_STAGE_ONE);
        events.ScheduleEvent(EVENT_ICE_SHARD, 1s, 0, PHASE_STAGE_ONE);
        events.ScheduleEvent(EVENT_SOUL_FRACTURE, t.SoulFracture, 0, PHASE_STAGE_ONE);
        events.ScheduleEvent(EVENT_OBLIVIONS_ECHO, t.OblivionsEcho, 0, PHASE_STAGE_ONE);
        events.ScheduleEvent(EVENT_GLACIAL_WRATH, t.GlacialWrath, 0, PHASE_STAGE_ONE);
        events.ScheduleEvent(EVENT_SUMMON_DEVOTED, DevotedFirst, 0, PHASE_STAGE_ONE);
        events.ScheduleEvent(EVENT_FROST_BLAST, t.FrostBlast, 0, PHASE_STAGE_ONE);
        events.ScheduleEvent(EVENT_DARK_EVOCATION, t.DarkEvocation, 0, PHASE_STAGE_ONE);
        events.ScheduleEvent(EVENT_HOWLING_BLIZZARD, t.HowlingBlizzard, 0, PHASE_STAGE_ONE);
    }

    void StartIntermission()
    {
        _intermission = true;

        events.Reset();
        events.SetPhase(PHASE_INTERMISSION);
        me->AttackStop();
        me->SetReactState(REACT_PASSIVE);
        me->SetImmuneToAll(true);

        DoCastSelf(SPELL_COSMETIC_DEATH, true);
        DoCastSelf(SPELL_CLEAR_ALL_DEBUFFS, true);

        events.ScheduleEvent(EVENT_VENGEFUL_DESTRUCTION, VengefulDestructionDelay, 0, PHASE_INTERMISSION);
        events.ScheduleEvent(EVENT_SUMMON_ADDS, AddsDelay, 0, PHASE_INTERMISSION);

        if (Creature* remnant = instance->GetCreature(DATA_KELTHUZAD_REMNANT))
            if (remnant->IsAIEnabled())
                remnant->AI()->DoAction(ACTION_INTERMISSION_STARTED);
    }

    void Revive()
    {
        RemovePhylacteryAuras();
        summons.DespawnAll();
        me->SetFullHealth();
        me->SetImmuneToAll(false);
        me->SetReactState(REACT_AGGRESSIVE);
        _intermission = false;
        _firstCycle = false;

        // Each return stacks Necrotic Surge on Kel'Thuzad
        DoCastSelf(SPELL_NECROTIC_SURGE, true);
        ScheduleStageOne();
    }

    void StartStageThree()
    {
        RemovePhylacteryAuras();
        summons.DespawnAll();
        events.Reset();
        _stageThree = true;
        _intermission = false;

        me->SetFullHealth();
        me->SetImmuneToAll(false);
        me->SetReactState(REACT_AGGRESSIVE);

        events.SetPhase(PHASE_STAGE_THREE);
        events.ScheduleEvent(EVENT_DEEP_FREEZE, DeepFreezeFirst, 0, PHASE_STAGE_THREE);
        events.ScheduleEvent(EVENT_FROST_BLAST_P3, FrostBlastP3First, 0, PHASE_STAGE_THREE);
        events.ScheduleEvent(EVENT_ONSLAUGHT_OF_THE_DAMNED, OnslaughtFirst, 0, PHASE_STAGE_THREE);
        events.ScheduleEvent(EVENT_HOWLING_BLIZZARD, HowlingBlizzardP3, 0, PHASE_STAGE_THREE);
    }

    bool _intermission;
    bool _stageThree;
    bool _firstCycle;
    uint32 _buttonTimer;
    uint8 _echoCasts;
    uint8 _frostBlastCasts;
};

// 176929 - Remnant of Kel'Thuzad
struct npc_kelthuzad_remnant_sod : public ScriptedAI
{
    npc_kelthuzad_remnant_sod(Creature* creature) : ScriptedAI(creature), _instance(creature->GetInstanceScript()), _yields(0), _yielding(false), _started(false) { }

    void JustAppeared() override
    {
        // Only attackable once Kel'Thuzad's phylactery has opened
        me->SetImmuneToAll(true);
        me->SetUninteractible(true);
    }

    void DoAction(int32 action) override
    {
        switch (action)
        {
            case ACTION_INTERMISSION_STARTED:
                _yielding = false;
                _started = false;
                me->SetImmuneToAll(false);
                me->SetUninteractible(false);
                break;
            case ACTION_RESET_ENCOUNTER:
                _yields = 0;
                _yielding = false;
                _started = false;
                _events.Reset();
                me->SetFullHealth();
                me->SetImmuneToAll(true);
                me->SetUninteractible(true);
                break;
            default:
                break;
        }
    }

    void DamageTaken(Unit* /*attacker*/, uint32& damage, DamageEffectType /*damageType*/, SpellInfo const* /*spellInfo*/) override
    {
        if (_yielding || _yields >= RemnantYields)
            return;

        // Stop at the next threshold instead of dying; the third life is the real death
        uint64 const thresholdHealth = me->CountPctFromMaxHealth(RemnantYieldPct[_yields]);
        uint64 const health = me->GetHealth();

        if (health > thresholdHealth && damage < health - thresholdHealth)
            return;

        damage = health > thresholdHealth ? uint32(health - thresholdHealth) : 0;
        Yield();
    }

    void JustDied(Unit* /*killer*/) override
    {
        // Final death: Stage Three; players inside are released as well
        ReleasePlayers();

        if (Creature* kelthuzad = _instance->GetCreature(DATA_KELTHUZAD))
            if (kelthuzad->IsAIEnabled())
                kelthuzad->AI()->DoAction(ACTION_REMNANT_DIED);
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);

        if (!UpdateVictim())
        {
            // Undying Wrath still has to fire while the Remnant is immune and out of combat
            while (uint32 eventId = _events.ExecuteEvent())
                if (eventId == EVENT_UNDYING_WRATH)
                    DoCastSelf(SPELL_UNDYING_WRATH);
            return;
        }

        if (!_started && !_yielding)
        {
            // Sniffed from the first player entering the phylactery: Freezing Blast +2.2s then 7.3s, Shadow Fissure +7.4s then 5.0s (follow-up +3.0s)
            _started = true;
            _events.ScheduleEvent(EVENT_FREEZING_BLAST, 2s + 200ms);
            _events.ScheduleEvent(EVENT_SHADOW_FISSURE, 7s + 400ms);
        }

        if (me->HasUnitState(UNIT_STATE_CASTING))
            return;

        while (uint32 eventId = _events.ExecuteEvent())
        {
            switch (eventId)
            {
                case EVENT_FREEZING_BLAST:
                    DoCastAOE(SPELL_FREEZING_BLAST);
                    _events.Repeat(7s + 300ms);
                    break;
                case EVENT_SHADOW_FISSURE:
                    DoCastAOE(SPELL_SHADOW_FISSURE_CAST);
                    _events.ScheduleEvent(EVENT_SHADOW_FISSURE_FOLLOWUP, 3s);
                    _events.Repeat(5s);
                    break;
                case EVENT_SHADOW_FISSURE_FOLLOWUP:
                    DoCastAOE(SPELL_SHADOW_FISSURE_FOLLOWUP);
                    break;
                case EVENT_UNDYING_WRATH:
                    DoCastSelf(SPELL_UNDYING_WRATH);
                    break;
                default:
                    break;
            }

            if (me->HasUnitState(UNIT_STATE_CASTING))
                return;
        }
    }

private:
    void Yield()
    {
        ++_yields;
        _yielding = true;

        // Sniffed: yell, then Undying Wrath ~11s later; Kel'Thuzad returns and the players are released
        Talk(SAY_REMNANT_RETURN);
        me->SetImmuneToAll(true);
        me->AttackStop();
        _events.CancelEvent(EVENT_FREEZING_BLAST);
        _events.CancelEvent(EVENT_SHADOW_FISSURE);
        _events.CancelEvent(EVENT_SHADOW_FISSURE_FOLLOWUP);
        _events.ScheduleEvent(EVENT_UNDYING_WRATH, RemnantReturnDelay);

        ReleasePlayers();

        if (Creature* kelthuzad = _instance->GetCreature(DATA_KELTHUZAD))
            if (kelthuzad->IsAIEnabled())
                kelthuzad->AI()->DoAction(ACTION_REMNANT_YIELDED);
    }

    // Sniffed: aura 351923 arrives on players inside the phylactery, giving them the "Exit the Phylactery" button
    void ReleasePlayers()
    {
        for (MapReference const& ref : me->GetMap()->GetPlayers())
            if (Player* player = ref.GetSource())
                if (player->HasAura(SPELL_INSIDE_PHYLACTERY))
                    player->CastSpell(player, SPELL_EXIT_PHYLACTERY_AVAILABLE, TRIGGERED_FULL_MASK);
    }

    InstanceScript* _instance;
    EventMap _events;
    uint8 _yields;
    bool _yielding;
    bool _started;
};

void AddSC_boss_kelthuzad_sod()
{
    RegisterSanctumOfDominationCreatureAI(boss_kelthuzad_sod);
    RegisterSanctumOfDominationCreatureAI(npc_kelthuzad_remnant_sod);
    RegisterSpellScript(spell_kelthuzad_phylactery_teleport);
}
