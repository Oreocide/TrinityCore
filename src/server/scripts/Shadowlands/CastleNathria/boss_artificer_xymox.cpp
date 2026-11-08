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
 * Artificer Xy'mox (DungeonEncounter 2405). Three phases that change on health only (70% and 40%, spell 181089): a cast in
 * progress finishes, then every timer is rebuilt from that moment; the first Hyperlight Spark follows at +6.06 s. Each phase
 * wakes one relic (Crystal of Phantasms, Root of Extinction, Edge of Annihilation). Timers are Warcraft Logs medians (27
 * kills with exact phase events, Normal and Heroic) and agree with DBM and BigWigs where those have a number; phase 3 is
 * checked against the 2026-11-06 capture, a 179 s tail of late phase 3 (trinitycore-tracking/castle-nathria/bosses/
 * artificer-xymox.md, artificer-xymox_followup.json). The repeats after the measured casts (spark 15.85 s, Rift Blast 36.6 s,
 * Glyph 28 s, Stasis Trap 30.4 s) are the capture's steady state; a cast that falls due while he is casting waits for it.
 *
 * - Glyph of Destruction: 2.5 s cast on the victim, the aura lasts 8 s on Normal and 4 s on Heroic (spell data, per
 *   difficulty), ticks every second; at expiry the blast hits every player and 0.5 s later the victim gains Arcane
 *   Vulnerability (stacks, 45 s each). The yells alternate between two lines.
 * - Dimensional Tear: a 2 s cast (342310 at phase start, 328437 from the rotator) marks two players with 328448 (wormhole A)
 *   and 328468 (B) for 8 s; retail never marked a tank (0 of 442). Alone, a player receives B only (capture). When a mark
 *   expires the player summons its wormhole at its feet (328308 / 328312), with the 6 yd burst 328545; both wormholes go
 *   when the next Tear starts. Entering a wormhole casts the cover spell and, when the partner exists, teleports to the
 *   partner; 328324 blocks a bounce for 3.2 s.
 * - Hyperlight Spark: 0.5 s cast on a random player (tank included); it chains to every other player.
 * - Rift Blast: 1 s cast, then a 6 s aura whose four ticks (0, 2, 4, 6 s) each make one random player summon a portal 8-10
 *   yd away, facing that player; the portal casts a 3.75 s beam (rectangle 50 by 5) and goes 1.5 s later.
 * - Phase 3 rotator 342380: every 25.65 s (capture; Warcraft Logs early slots say 26.75 s, DBM 25 / 25.2), alternating Tear
 *   and Annihilate, first slot a Tear at +55.8 s. The first Annihilate at +27.9 s follows Edge of Annihilation (+20.6 s).
 *   Annihilate: the boss casts 340805 and 340807 (1.5 s) at the relic in the same millisecond, then the relic casts 328789
 *   (10 s, knockback). The Worldbreaking areatrigger on the relic adds a gravity pull of 2.75 toward it to every player
 *   inside 25 yd at +0.04, 2, 4, 6 and 8 s; the pulls end with the cast.
 * - Phase 1 Crystal of Phantasms: fixates three non-tanks for 11 s (a Fleeting Spirit chases each); a spirit that reaches its
 *   player casts Possession (charm plus a 9999 absorb that ends the charm when it is spent; the core removes the whole aura
 *   when an absorb is used up). Soul Singe: from the relic, bursts of 13 arcs 0.5 s apart every 17.5 s, the first 6 s after
 *   the Crystal cast starts.
 * - Phase 2 Withering Touch: one random non-tank at +25.0 s, then every 15.3 s.
 * - Defeat: lethal damage leaves him at 1 health. The Stone Husk appears with "Marvelous!", 3 s later Shatter and Stasis
 *   Shield, 0.8 s after that the second line, and 8.8 s after the shield he dies (capture: 166.74 / 167.0 / 169.72 / 170.5 /
 *   178.52 s).
 * No berserk is scripted: the capture shows Berserk (26662) already on him but WCL shows it absent for the first 208 s.
 *
 * Not settled: the Root of Extinction relic (Seeds of Extinction 329770 / 329834 and the Extinction explosion 329107) is not
 * scripted at all: where the four seeds appear, how a player moves one and the explosion radius were never captured (the
 * capture has no phase 2); Stasis Trap is cast on every difficulty but summons nothing (Normal does nothing; the Heroic trap
 * 167691 was never captured); Aura of Dread (340872) has no areatrigger data (create properties 21675 missing, radius never
 * seen); the Fleeting Spirit speed (it spawns 8 yd behind its player, spell data) and its possession range (2.5 yd), the Soul Singe start (6.0 s after the cast start, not the cast end), the fixate count
 * (three) and a tank fallback when nobody else is alive are estimates; the Annihilate pull radius (25 yd, the Worldbreaking
 * sphere) is an estimate (the force areatrigger 26277 radius was never captured); whether the two Tear wormholes swap their
 * marked players the moment they appear; the Tear yell alternation (3 of 4 captured lines alternate); the P2 rotator Tear
 * at +51.0 s (Warcraft Logs, once); the Rift Blast, Glyph and spark repeats outside the capture; the Stone Husk copy aura,
 * the relic Translocate and the Raid Boss Emote for Annihilate; the wormhole lockout auras 342367 / 342368 (not seen);
 * Mythic (80% / 60% thresholds and extra abilities).
 */

#include "AreaTrigger.h"
#include "AreaTriggerAI.h"
#include "Containers.h"
#include "InstanceScript.h"
#include "Map.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "SpellAuraEffects.h"
#include "SpellAuras.h"
#include "SpellScript.h"
#include "TemporarySummon.h"
#include "castle_nathria.h"
#include <algorithm>
#include <span>

enum XymoxSpells
{
    // boss
    SPELL_ENCOUNTER_EVENT           = 181089,   // phase change marker, instant
    SPELL_HYPERLIGHT_SPARK          = 325399,
    SPELL_STASIS_TRAP               = 326271,
    SPELL_TEAR_PHASE                = 342310,   // phase start Tear, 2 s cast
    SPELL_TEAR_ROTATOR              = 328437,   // rotator Tear, 2 s cast
    SPELL_TEAR_MARK_A               = 328448,   // 8 s
    SPELL_TEAR_MARK_B               = 328468,
    SPELL_WORMHOLE_A                = 328308,   // summons 168726 and fires the 6 yd burst 328545
    SPELL_WORMHOLE_B                = 328312,   // summons 168730
    SPELL_WORMHOLE_AT_A             = 328309,   // the sphere r6 areatrigger (create properties 20390)
    SPELL_WORMHOLE_AT_B             = 328310,   // create properties 20391
    SPELL_TELEPORT_A_TO_B           = 328313,
    SPELL_TELEPORT_B_TO_A           = 328315,
    SPELL_COVER_A                   = 328422,
    SPELL_COVER_B                   = 328423,
    SPELL_RECOMBOBULATION           = 328324,   // 3.2 s: immune to wormholes
    SPELL_GLYPH_CAST                = 325361,
    SPELL_GLYPH                     = 325236,
    SPELL_GLYPH_BLAST               = 325324,
    SPELL_ARCANE_VULNERABILITY      = 340533,
    SPELL_RIFT_BLAST                = 335013,
    SPELL_PORTAL_BEAM               = 329256,
    SPELL_ROTATOR                   = 342380,
    SPELL_ANNIHILATE_MARK           = 340805,
    SPELL_ANNIHILATE_CAST           = 340807,   // 1.5 s, on the relic
    SPELL_ANNIHILATE                = 328789,   // cast by the relic, 10 s
    SPELL_WORLDBREAKING             = 338044,   // areatrigger 21397 on the relic, 10 s
    SPELL_EDGE_ACTIVATE             = 328880,
    SPELL_CRYSTAL_ACTIVATE          = 327887,
    SPELL_FIXATE                    = 327902,   // 11 s; its LINKED_SUMMON effect summons the spirit
    SPELL_POSSESSION                = 327414,
    SPELL_SOUL_SINGE                = 340842,
    SPELL_WITHERING_TOUCH           = 340860,
    SPELL_CRYSTAL_COSMETIC          = 328237,   // the Crystal goes back at the first phase change
    SPELL_ROOT_COSMETIC             = 329828,   // the Root goes back at the second
    SPELL_SERVICE_IN_STONE          = 340217,   // summons the Stone Husk
    SPELL_SHATTER                   = 340473,
    SPELL_STASIS_SHIELD             = 342874
};

enum XymoxCreatures
{
    NPC_WORMHOLE_A                  = 168726,
    NPC_WORMHOLE_B                  = 168730,
    NPC_RIFT_BLAST_PORTAL           = 169271,
    NPC_FLEETING_SPIRIT             = 168317,
    NPC_STONE_HUSK                  = 173756,
    NPC_CRYSTAL_OF_PHANTASMS        = 168512,
    NPC_ROOT_OF_EXTINCTION          = 169267,
    NPC_EDGE_OF_ANNIHILATION        = 169062
};

enum XymoxEvents
{
    EVENT_SPARK = 1,
    EVENT_STASIS_TRAP,
    EVENT_TEAR_PHASE,
    EVENT_TEAR_ROTATOR_P2,
    EVENT_RIFT_BLAST,
    EVENT_GLYPH,
    EVENT_CRYSTAL,
    EVENT_SOUL_SINGE_BURST,
    EVENT_WITHERING_TOUCH,
    EVENT_EDGE,
    EVENT_ANNIHILATE_FIRST,
    EVENT_ROTATOR,
    EVENT_DEFEAT_SHIELD,
    EVENT_DEFEAT_SECOND_LINE,
    EVENT_DEFEAT_END
};

enum XymoxTexts
{
    SAY_RIFT_BLAST                  = 0,    // "Mind your step!"
    SAY_TEAR                        = 1,    // "Die together!" / "The warp beckons!"
    SAY_GLYPH_1                     = 2,    // "You cannot bargain with death!"
    SAY_GLYPH_2                     = 3,    // "Destruction is guaranteed!"
    SAY_DEFEAT_1                    = 4,    // "Marvelous!"
    SAY_DEFEAT_2                    = 5     // "This exchange has no further value. ..."
};

static constexpr float Phase2HealthPct = 70.0f;
static constexpr float Phase3HealthPct = 40.0f;

// timers in ms from the phase start (Warcraft Logs medians, see the header); the last value of a list repeats at the interval
static constexpr Milliseconds ScheduleHorizon = 300s;

static constexpr Milliseconds SparkP1[] = { 5800ms, 34100ms };
static constexpr Milliseconds SparkP2[] = { 6060ms, 31600ms, 47300ms, 64300ms };
static constexpr Milliseconds SparkP3[] = { 6060ms, 58300ms, 74000ms };
static constexpr Milliseconds SparkRepeat = 15850ms;                // capture steady state (15.87-15.89 s when not delayed)

static constexpr Milliseconds TrapP1[] = { 10700ms, 41100ms };
static constexpr Milliseconds TrapP2[] = { 10900ms, 41300ms };
static constexpr Milliseconds TrapP3[] = { 10900ms, 61900ms };
static constexpr Milliseconds TrapRepeatP12 = 30400ms;
static constexpr Milliseconds TrapRepeatP3 = 33000ms;               // capture gaps 31.8 / 33.0 / 37.9 s

static constexpr Milliseconds TearP1 = 14300ms;
static constexpr Milliseconds TearP23 = 14600ms;
static constexpr Milliseconds TearRotatorP2 = 51000ms;              // Warcraft Logs, n=19, once

static constexpr Milliseconds RiftBlastP1 = 20500ms;
static constexpr Milliseconds RiftBlastP2 = 20600ms;
static constexpr Milliseconds RiftBlastP3 = 46100ms;
static constexpr Milliseconds RiftBlastRepeat = 36600ms;            // capture 36.6-39.1 s; a cast in progress delays it further

static constexpr Milliseconds GlyphP1 = 28700ms;
static constexpr Milliseconds GlyphP2 = 25500ms;
static constexpr Milliseconds GlyphP3 = 51000ms;
static constexpr Milliseconds GlyphRepeat = 28100ms;                // capture gaps 28.1-30.6 s, Warcraft Logs 28.0

static constexpr Milliseconds CrystalActivate = 25100ms;
static constexpr Milliseconds SoulSingeFirst = 6s;
static constexpr Milliseconds SoulSingeBurstRepeat = 17500ms;
static constexpr Milliseconds SoulSingeArcInterval = 500ms;
static constexpr uint8 SoulSingeArcs = 13;
static constexpr uint8 FixateTargets = 3;

static constexpr Milliseconds WitheringTouchFirst = 25s;
static constexpr Milliseconds WitheringTouchRepeat = 15300ms;

static constexpr Milliseconds EdgeActivateP3 = 20600ms;
static constexpr Milliseconds AnnihilateFirstP3 = 27900ms;
static constexpr Milliseconds RotatorFirst = 55800ms;
static constexpr Milliseconds RotatorInterval = 25650ms;

static constexpr uint8 TearTargets = 2;
static constexpr Milliseconds GlyphBlastVulnerabilityDelay = 500ms;
static constexpr Milliseconds PortalDespawnDelay = 3750ms + 1500ms; // the beam, then 0.84-2.23 s in the capture
static constexpr Milliseconds LinkedAuraCheck = 100ms;
static constexpr float PossessionRange = 2.5f;

static constexpr Milliseconds DefeatShieldDelay = 3s;
static constexpr Milliseconds DefeatSecondLineDelay = 780ms;
static constexpr Milliseconds DefeatEndDelay = 8800ms;

// Worldbreaking pull: five forces, magnitude 2.75 each, at the capture's offsets from the cast start
static constexpr Milliseconds WorldbreakingPulls[] = { 40ms, 2000ms, 4000ms, 6000ms, 8000ms };
static constexpr float WorldbreakingMagnitude = 2.75f;
static constexpr float WorldbreakingRadius = 25.0f;
static constexpr uint32 AreaTriggerWorldbreakingForce = 26277;     // the capture's force ids are areatriggers of this entry

// auras that must not outlive the fight on the players
static constexpr uint32 FightPlayerAuras[] =
{
    SPELL_GLYPH, SPELL_TEAR_MARK_A, SPELL_TEAR_MARK_B, SPELL_RECOMBOBULATION, SPELL_FIXATE, SPELL_POSSESSION,
    SPELL_ARCANE_VULNERABILITY, SPELL_WITHERING_TOUCH
};

static void InheritBossFaction(Creature* creature)
{
    Creature* boss = nullptr;
    if (InstanceScript* instance = creature->GetInstanceScript())
        boss = instance->GetCreature(DATA_ARTIFICER_XYMOX);
    if (!boss)
        boss = creature->FindNearestCreature(BOSS_ARTIFICER_XYMOX, 500.0f);
    if (boss)
        creature->SetFaction(boss->GetFaction());
}

// 166644 - Artificer Xy'mox
struct boss_artificer_xymox : public BossAI
{
    boss_artificer_xymox(Creature* creature) : BossAI(creature, DATA_ARTIFICER_XYMOX) { }

    void Reset() override
    {
        _Reset();
        ResetFight();
    }

    void JustEngagedWith(Unit* who) override
    {
        BossAI::JustEngagedWith(who);
        EnterPhase(1);
    }

    void JustSummoned(Creature* summon) override
    {
        // the Stone Husk outlives the fight; the spirits and portals are summoned by the players, not by the boss
        if (summon->GetEntry() != NPC_STONE_HUSK)
            summons.Summon(summon);
    }

    void DamageTaken(Unit* /*attacker*/, uint32& damage, DamageEffectType /*damageType*/, SpellInfo const* /*spellInfo*/) override
    {
        if (_defeated)
        {
            damage = 0;
            return;
        }

        if (damage < me->GetHealth())
            return;

        damage = me->GetHealth() > 1 ? me->GetHealth() - 1 : 0;
        StartDefeat();
    }

    void UpdateAI(uint32 diff) override
    {
        if (_defeated)
        {
            events.Update(diff);
            while (uint32 eventId = events.ExecuteEvent())
                ExecuteEvent(eventId);
            return;
        }

        if (!UpdateVictim())
            return;

        events.Update(diff);

        // the phase changes on health only; a cast in progress finishes
        if (_phase == 1 && me->HealthBelowPct(Phase2HealthPct))
            EnterPhase(2);
        else if (_phase == 2 && me->HealthBelowPct(Phase3HealthPct))
            EnterPhase(3);

        if (me->HasUnitState(UNIT_STATE_CASTING))
            return;

        while (uint32 eventId = events.ExecuteEvent())
        {
            ExecuteEvent(eventId);
            if (me->HasUnitState(UNIT_STATE_CASTING))
                return;
        }
    }

    // schedules a list of offsets, then repeats the last one at the interval up to the horizon (a phase change resets all)
    void ScheduleSeries(uint32 eventId, std::span<Milliseconds const> offsets, Milliseconds repeat)
    {
        for (Milliseconds offset : offsets)
            events.ScheduleEvent(eventId, offset);

        if (repeat > 0ms)
            for (Milliseconds offset = offsets.back() + repeat; offset < ScheduleHorizon; offset += repeat)
                events.ScheduleEvent(eventId, offset);
    }

    void ScheduleSeries(uint32 eventId, Milliseconds first, Milliseconds repeat)
    {
        ScheduleSeries(eventId, std::span<Milliseconds const>(&first, 1), repeat);
    }

    void EnterPhase(uint8 phase)
    {
        _phase = phase;
        events.Reset();

        if (phase > 1)
        {
            DoCastSelf(SPELL_ENCOUNTER_EVENT, true);
            // the relic that was active goes back to its display case in the same millisecond
            StopSoulSinge();
            if (Creature* relic = GetRelic(phase == 2 ? NPC_CRYSTAL_OF_PHANTASMS : NPC_ROOT_OF_EXTINCTION))
                relic->CastSpell(relic, phase == 2 ? SPELL_CRYSTAL_COSMETIC : SPELL_ROOT_COSMETIC, true);
        }

        switch (phase)
        {
            case 1:
                ScheduleSeries(EVENT_SPARK, SparkP1, SparkRepeat);
                ScheduleSeries(EVENT_STASIS_TRAP, TrapP1, TrapRepeatP12);
                events.ScheduleEvent(EVENT_TEAR_PHASE, TearP1);
                ScheduleSeries(EVENT_RIFT_BLAST, RiftBlastP1, RiftBlastRepeat);
                ScheduleSeries(EVENT_GLYPH, GlyphP1, GlyphRepeat);
                events.ScheduleEvent(EVENT_CRYSTAL, CrystalActivate);
                break;
            case 2:
                ScheduleSeries(EVENT_SPARK, SparkP2, SparkRepeat);
                ScheduleSeries(EVENT_STASIS_TRAP, TrapP2, TrapRepeatP12);
                events.ScheduleEvent(EVENT_TEAR_PHASE, TearP23);
                events.ScheduleEvent(EVENT_TEAR_ROTATOR_P2, TearRotatorP2);
                ScheduleSeries(EVENT_RIFT_BLAST, RiftBlastP2, RiftBlastRepeat);
                ScheduleSeries(EVENT_GLYPH, GlyphP2, GlyphRepeat);
                ScheduleSeries(EVENT_WITHERING_TOUCH, WitheringTouchFirst, WitheringTouchRepeat);
                break;
            case 3:
                _rotatorTear = true;
                ScheduleSeries(EVENT_SPARK, SparkP3, SparkRepeat);
                ScheduleSeries(EVENT_STASIS_TRAP, TrapP3, TrapRepeatP3);
                events.ScheduleEvent(EVENT_TEAR_PHASE, TearP23);
                ScheduleSeries(EVENT_RIFT_BLAST, RiftBlastP3, RiftBlastRepeat);
                ScheduleSeries(EVENT_GLYPH, GlyphP3, GlyphRepeat);
                events.ScheduleEvent(EVENT_EDGE, EdgeActivateP3);
                events.ScheduleEvent(EVENT_ANNIHILATE_FIRST, AnnihilateFirstP3);
                ScheduleSeries(EVENT_ROTATOR, RotatorFirst, RotatorInterval);
                break;
            default:
                break;
        }
    }

    void ExecuteEvent(uint32 eventId) override
    {
        switch (eventId)
        {
            case EVENT_SPARK:
                if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0, 0.0f, true))
                    DoCast(target, SPELL_HYPERLIGHT_SPARK);
                break;
            case EVENT_STASIS_TRAP:
                // a bare cast on Normal; the Heroic trap was never captured and is not scripted
                DoCastSelf(SPELL_STASIS_TRAP);
                break;
            case EVENT_TEAR_PHASE:
                DespawnWormholes();
                DoCastSelf(SPELL_TEAR_PHASE);
                break;
            case EVENT_TEAR_ROTATOR_P2:
                DespawnWormholes();
                Talk(SAY_TEAR);
                DoCastSelf(SPELL_TEAR_ROTATOR);
                break;
            case EVENT_RIFT_BLAST:
                Talk(SAY_RIFT_BLAST);
                DoCastSelf(SPELL_RIFT_BLAST);
                break;
            case EVENT_GLYPH:
                // the two lines alternate (capture, 6 of 6)
                Talk(_glyphSecondLine ? SAY_GLYPH_2 : SAY_GLYPH_1);
                _glyphSecondLine = !_glyphSecondLine;
                DoCastVictim(SPELL_GLYPH_CAST);
                break;
            case EVENT_CRYSTAL:
                if (Creature* crystal = GetRelic(NPC_CRYSTAL_OF_PHANTASMS))
                    DoCast(crystal, SPELL_CRYSTAL_ACTIVATE);
                events.ScheduleEvent(EVENT_SOUL_SINGE_BURST, SoulSingeFirst);
                break;
            case EVENT_SOUL_SINGE_BURST:
                // the arcs belong to the relic, so they run on its own event queue and do not wait for the boss's casts
                if (Creature* crystal = GetRelic(NPC_CRYSTAL_OF_PHANTASMS))
                    for (uint8 i = 0; i < SoulSingeArcs; ++i)
                        crystal->m_Events.AddEventAtOffset([crystal]() { crystal->CastSpell(crystal, SPELL_SOUL_SINGE, true); }, SoulSingeArcInterval * i);
                events.ScheduleEvent(EVENT_SOUL_SINGE_BURST, SoulSingeBurstRepeat);
                break;
            case EVENT_WITHERING_TOUCH:
            {
                std::vector<Player*> targets = PickPlayers(1);
                if (!targets.empty())
                    DoCast(targets.front(), SPELL_WITHERING_TOUCH, true);
                break;
            }
            case EVENT_EDGE:
                if (Creature* edge = GetRelic(NPC_EDGE_OF_ANNIHILATION))
                    DoCast(edge, SPELL_EDGE_ACTIVATE);
                break;
            case EVENT_ANNIHILATE_FIRST:
                StartAnnihilate();
                break;
            case EVENT_ROTATOR:
                DoCastSelf(SPELL_ROTATOR, true);
                if (_rotatorTear)
                {
                    DespawnWormholes();
                    Talk(SAY_TEAR);
                    DoCastSelf(SPELL_TEAR_ROTATOR);
                }
                else
                    StartAnnihilate();
                _rotatorTear = !_rotatorTear;
                break;
            case EVENT_DEFEAT_SHIELD:
                DoCastSelf(SPELL_SHATTER, true);
                DoCastSelf(SPELL_STASIS_SHIELD, true);
                events.ScheduleEvent(EVENT_DEFEAT_SECOND_LINE, DefeatSecondLineDelay);
                events.ScheduleEvent(EVENT_DEFEAT_END, DefeatEndDelay);
                break;
            case EVENT_DEFEAT_SECOND_LINE:
                Talk(SAY_DEFEAT_2);
                break;
            case EVENT_DEFEAT_END:
                me->RemoveAurasDueToSpell(SPELL_STASIS_SHIELD);
                me->KillSelf();
                break;
            default:
                break;
        }
    }

    // the cast of the Tear (342310 or 328437) just finished: mark two players, not tanks while others exist; alone, a player
    // receives mark B only (capture, 3 casts)
    void HandleTearCast()
    {
        std::vector<Player*> targets = PickPlayers(TearTargets);
        if (targets.size() == 1)
            me->CastSpell(targets.front(), SPELL_TEAR_MARK_B, true);
        else
            for (size_t i = 0; i < targets.size() && i < TearTargets; ++i)
                me->CastSpell(targets[i], i == 0 ? SPELL_TEAR_MARK_A : SPELL_TEAR_MARK_B, true);
    }

    // the Crystal cast finished: three non-tanks are fixated, each fixate summons its spirit
    void HandleCrystalCast()
    {
        for (Player* player : PickPlayers(FixateTargets))
            me->CastSpell(player, SPELL_FIXATE, true);
    }

    void StartAnnihilate()
    {
        Creature* edge = GetRelic(NPC_EDGE_OF_ANNIHILATION);
        if (!edge)
            return;

        // both start in the same millisecond; the relic starts its 10 s cast when 340807 completes (spell script)
        DoCast(edge, SPELL_ANNIHILATE_CAST);
        DoCast(edge, SPELL_ANNIHILATE_MARK, true);
    }

    void StopSoulSinge()
    {
        if (Creature* crystal = GetRelic(NPC_CRYSTAL_OF_PHANTASMS))
            crystal->m_Events.KillAllEvents(false);
    }

    void DespawnWormholes()
    {
        summons.DespawnEntry(NPC_WORMHOLE_A);
        summons.DespawnEntry(NPC_WORMHOLE_B);
    }

    Creature* GetRelic(uint32 entry) const
    {
        return me->FindNearestCreature(entry, 500.0f);
    }

    bool IsTank(Player const* player) const
    {
        if (me->GetVictim() == player)
            return true;

        ChrSpecializationEntry const* spec = player->GetPrimarySpecializationEntry();
        return spec && spec->GetRole() == ChrSpecializationRole::Tank;
    }

    // random living non-tank players; tanks only fill the list when fewer than count non-tanks exist
    // (retail marked a tank 0 of 442 times; the solo capture marked its only player)
    std::vector<Player*> PickPlayers(size_t count) const
    {
        std::vector<Player*> others;
        std::vector<Player*> tanks;
        for (MapReference const& ref : me->GetMap()->GetPlayers())
        {
            Player* player = ref.GetSource();
            if (!player || !player->IsAlive() || player->IsGameMaster())
                continue;

            if (IsTank(player))
                tanks.push_back(player);
            else
                others.push_back(player);
        }

        Trinity::Containers::RandomShuffle(others);
        Trinity::Containers::RandomShuffle(tanks);

        std::vector<Player*> result;
        for (std::vector<Player*>* group : { &others, &tanks })
            for (Player* player : *group)
                if (result.size() < count)
                    result.push_back(player);
        return result;
    }

    void StartDefeat()
    {
        _defeated = true;
        events.Reset();
        StopSoulSinge();
        me->InterruptNonMeleeSpells(false);
        summons.DespawnAll();
        RemoveFightAuras();
        Talk(SAY_DEFEAT_1);
        DoCastSelf(SPELL_SERVICE_IN_STONE, true);
        events.ScheduleEvent(EVENT_DEFEAT_SHIELD, DefeatShieldDelay);
    }

    void RemoveFightAuras()
    {
        for (MapReference const& ref : me->GetMap()->GetPlayers())
            if (Player* player = ref.GetSource())
                for (uint32 spellId : FightPlayerAuras)
                    player->RemoveAurasDueToSpell(spellId);
    }

    void ResetFight()
    {
        _phase = 0;
        _defeated = false;
        _glyphSecondLine = false;
        _rotatorTear = true;
        RemoveFightAuras();
        StopSoulSinge();

        if (Creature* edge = GetRelic(NPC_EDGE_OF_ANNIHILATION))
        {
            edge->InterruptNonMeleeSpells(false);
            edge->RemoveAurasDueToSpell(SPELL_WORLDBREAKING);
        }
    }

    void JustDied(Unit* killer) override
    {
        BossAI::JustDied(killer);
        RemoveFightAuras();
    }

    void EnterEvadeMode(EvadeReason why) override
    {
        ResetFight();
        BossAI::EnterEvadeMode(why);
    }

private:
    uint8 _phase = 0;
    bool _defeated = false;
    bool _glyphSecondLine = false;
    bool _rotatorTear = true;
};

// 168726, 168730 - Wormhole A / B: the entry's areatrigger aura. Wormhole B carries it in its template addon; wormhole A has
// no addon row in the world database (never sniffed), so the script casts what is missing.
struct npc_xymox_wormhole : public ScriptedAI
{
    npc_xymox_wormhole(Creature* creature) : ScriptedAI(creature) { }

    void JustAppeared() override
    {
        me->SetReactState(REACT_PASSIVE);
        me->SetUnitFlag(UNIT_FLAG_NON_ATTACKABLE);

        // after the template addon auras are in place
        me->m_Events.AddEventAtOffset([creature = me]()
        {
            uint32 spellId = creature->GetEntry() == NPC_WORMHOLE_A ? SPELL_WORMHOLE_AT_A : SPELL_WORMHOLE_AT_B;
            if (!creature->HasAura(spellId))
                creature->CastSpell(creature, spellId, true);
        }, LinkedAuraCheck);
    }
};

// 169271 - Rift Blast Portal: summoned by the force-cast player 8-10 yd away, faces that player and fires its beam
struct npc_xymox_rift_blast_portal : public ScriptedAI
{
    npc_xymox_rift_blast_portal(Creature* creature) : ScriptedAI(creature) { }

    void JustAppeared() override
    {
        me->SetReactState(REACT_PASSIVE);
        InheritBossFaction(me);

        // retail: orientation exactly toward the player (error 0.0 degrees, 20 of 20)
        if (TempSummon* summon = me->ToTempSummon())
        {
            if (WorldObject* summoner = summon->GetSummoner())
            {
                float angle = me->GetAbsoluteAngle(summoner);
                me->SetOrientation(angle);
                me->SetFacingTo(angle);
            }
        }

        DoCastSelf(SPELL_PORTAL_BEAM);
        me->m_Events.AddEventAtOffset([creature = me]() { creature->DespawnOrUnsummon(); }, PortalDespawnDelay);
    }
};

// 168317 - Fleeting Spirit: summoned by the fixated player itself (the LINKED_SUMMON effect of 327902 makes the player cast the
// summon), chases its summoner and possesses it on arrival; it dissipates when the fixate ends first, and leaves when the
// possession ends
struct npc_xymox_fleeting_spirit : public ScriptedAI
{
    npc_xymox_fleeting_spirit(Creature* creature) : ScriptedAI(creature) { }

    void JustAppeared() override
    {
        me->SetReactState(REACT_PASSIVE);
        InheritBossFaction(me);

        if (TempSummon* summon = me->ToTempSummon())
        {
            if (Unit* summoner = summon->GetSummonerUnit())
            {
                _target = summoner->GetGUID();
                me->GetMotionMaster()->MoveChase(summoner);
            }
        }
    }

    void UpdateAI(uint32 diff) override
    {
        if (_target.IsEmpty())
            return;

        _sinceCheck += Milliseconds(diff);
        if (_sinceCheck < LinkedAuraCheck)
            return;
        _sinceCheck = 0ms;

        Player* player = ObjectAccessor::GetPlayer(*me, _target);
        if (!player || !player->IsAlive())
        {
            me->DespawnOrUnsummon();
            return;
        }

        if (_possessing)
        {
            if (!player->HasAura(SPELL_POSSESSION, me->GetGUID()))
            {
                player->RemoveAurasDueToSpell(SPELL_FIXATE);
                me->DespawnOrUnsummon();
            }
            return;
        }

        if (!player->HasAura(SPELL_FIXATE))
        {
            me->DespawnOrUnsummon();
            return;
        }

        if (me->GetDistance(player) <= PossessionRange)
        {
            me->GetMotionMaster()->Clear();
            me->StopMoving();
            me->CastSpell(player, SPELL_POSSESSION, true);
            _possessing = true;
        }
    }

private:
    ObjectGuid _target;
    Milliseconds _sinceCheck = 0ms;
    bool _possessing = false;
};

// 325361 - Glyph of Destruction: the spell data triggers the aura from the caster; retail puts it on the victim
class spell_xymox_glyph_of_destruction_cast : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_GLYPH });
    }

    void PreventDefault(SpellEffIndex effIndex)
    {
        PreventHitDefaultEffect(effIndex);
    }

    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        GetCaster()->CastSpell(GetHitUnit(), SPELL_GLYPH, true);
    }

    void Register() override
    {
        OnEffectLaunch += SpellEffectFn(spell_xymox_glyph_of_destruction_cast::PreventDefault, EFFECT_0, SPELL_EFFECT_TRIGGER_SPELL);
        OnEffectLaunchTarget += SpellEffectFn(spell_xymox_glyph_of_destruction_cast::PreventDefault, EFFECT_0, SPELL_EFFECT_TRIGGER_SPELL);
        OnEffectHitTarget += SpellEffectFn(spell_xymox_glyph_of_destruction_cast::HandleDummy, EFFECT_1, SPELL_EFFECT_DUMMY);
    }
};

// 325236 - Glyph of Destruction: when the aura runs out the blast hits every player, 0.5 s later the victim gains Arcane Vulnerability
class spell_xymox_glyph_of_destruction : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_GLYPH_BLAST, SPELL_ARCANE_VULNERABILITY });
    }

    void HandleRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        if (GetTargetApplication()->GetRemoveMode() != AURA_REMOVE_BY_EXPIRE)
            return;

        Unit* boss = GetCaster();
        if (!boss)
            return;

        Unit* victim = GetTarget();
        boss->CastSpell(victim->GetPosition(), SPELL_GLYPH_BLAST, true);

        boss->m_Events.AddEventAtOffset([boss, victimGuid = victim->GetGUID()]()
        {
            if (Unit* target = ObjectAccessor::GetUnit(*boss, victimGuid))
                boss->CastSpell(target, SPELL_ARCANE_VULNERABILITY, true);
        }, GlyphBlastVulnerabilityDelay);
    }

    void Register() override
    {
        OnEffectRemove += AuraEffectRemoveFn(spell_xymox_glyph_of_destruction::HandleRemove, EFFECT_0, SPELL_AURA_PERIODIC_TRIGGER_SPELL, AURA_EFFECT_HANDLE_REAL);
    }
};

// 325399 - Hyperlight Spark: the chain reaches every other player (retail: distinct players hit per cast / raid size = 1.0),
// whatever the distance; the order follows the nearest neighbour from the primary target
class spell_xymox_hyperlight_spark : public SpellScript
{
    void FilterChain(std::list<WorldObject*>& targets)
    {
        Unit* caster = GetCaster();
        Unit* current = GetExplTargetUnit();
        if (!current)
            return;

        std::vector<Player*> remaining;
        for (MapReference const& ref : caster->GetMap()->GetPlayers())
            if (Player* player = ref.GetSource())
                if (player != current && player->IsAlive() && !player->IsGameMaster())
                    remaining.push_back(player);

        targets.clear();
        while (!remaining.empty())
        {
            auto nearest = std::ranges::min_element(remaining, [current](Player* a, Player* b)
            {
                return current->GetExactDist2dSq(a) < current->GetExactDist2dSq(b);
            });
            current = *nearest;
            targets.push_back(current);
            remaining.erase(nearest);
        }
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_xymox_hyperlight_spark::FilterChain, EFFECT_0, TARGET_UNIT_TARGET_ANY);
    }
};

// 342310 - Dimensional Tear (phase start), 328437 - Dimensional Tear (rotator): the data marks every player in reach or
// nobody; the script marks two when the cast completes
class spell_xymox_dimensional_tear : public SpellScript
{
    void PreventDefault(SpellEffIndex effIndex)
    {
        PreventHitDefaultEffect(effIndex);
    }

    void HandleAfterCast()
    {
        if (boss_artificer_xymox* ai = CAST_AI(boss_artificer_xymox, GetCaster()->GetAI()))
            ai->HandleTearCast();
    }

    void Register() override
    {
        if (m_scriptSpellId == SPELL_TEAR_PHASE)
        {
            OnEffectLaunch += SpellEffectFn(spell_xymox_dimensional_tear::PreventDefault, EFFECT_0, SPELL_EFFECT_TRIGGER_SPELL);
            OnEffectLaunchTarget += SpellEffectFn(spell_xymox_dimensional_tear::PreventDefault, EFFECT_0, SPELL_EFFECT_TRIGGER_SPELL);
            OnEffectLaunch += SpellEffectFn(spell_xymox_dimensional_tear::PreventDefault, EFFECT_1, SPELL_EFFECT_TRIGGER_SPELL);
            OnEffectLaunchTarget += SpellEffectFn(spell_xymox_dimensional_tear::PreventDefault, EFFECT_1, SPELL_EFFECT_TRIGGER_SPELL);
        }
        AfterCast += SpellCastFn(spell_xymox_dimensional_tear::HandleAfterCast);
    }
};

// 328448, 328468 - Dimensional Tear marks: when the mark runs out the player summons its wormhole where it stands
class spell_xymox_tear_mark : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_WORMHOLE_A, SPELL_WORMHOLE_B });
    }

    void HandleRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        if (GetTargetApplication()->GetRemoveMode() != AURA_REMOVE_BY_EXPIRE)
            return;

        if (!GetCaster())
            return;

        // cast by the player (capture): the burst 328545 hits its allies, the boss stays the summoner and damage source
        Unit* target = GetTarget();
        target->CastSpell(target->GetPosition(), GetId() == SPELL_TEAR_MARK_A ? SPELL_WORMHOLE_A : SPELL_WORMHOLE_B,
            CastSpellExtraArgs(TRIGGERED_FULL_MASK).SetOriginalCaster(GetCasterGUID()));
    }

    void Register() override
    {
        OnEffectRemove += AuraEffectRemoveFn(spell_xymox_tear_mark::HandleRemove, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }
};

// 327887 - Crystal of Phantasms: the fixates come when the cast completes
class spell_xymox_crystal_activate : public SpellScript
{
    void HandleAfterCast()
    {
        if (boss_artificer_xymox* ai = CAST_AI(boss_artificer_xymox, GetCaster()->GetAI()))
            ai->HandleCrystalCast();
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_xymox_crystal_activate::HandleAfterCast);
    }
};

// 340807 - Annihilate (boss, 1.5 s): when it completes the relic starts its own 10 s cast
class spell_xymox_annihilate_cast : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_ANNIHILATE });
    }

    // the explicit target, not the hit target: the relic is passive scenery and need not be hit by the dummy
    void HandleAfterCast()
    {
        if (Unit* relic = GetExplTargetUnit())
            relic->CastSpell(relic, SPELL_ANNIHILATE, CastSpellExtraArgs());
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_xymox_annihilate_cast::HandleAfterCast);
    }
};

// 328789 - Annihilate (relic, 10 s): the Worldbreaking areatrigger lasts for the cast, the pulls end with it
class spell_xymox_annihilate : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_WORLDBREAKING });
    }

    // BeforeCast runs when the 10 s cast completes; OnPrecast runs when it starts, which is when the pull must begin
    void OnPrecast() override
    {
        GetCaster()->CastSpell(GetCaster(), SPELL_WORLDBREAKING, TRIGGERED_FULL_MASK);
    }

    void HandleAfterCast()
    {
        GetCaster()->RemoveAurasDueToSpell(SPELL_WORLDBREAKING);
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_xymox_annihilate::HandleAfterCast);
    }
};

// Wormhole areatrigger (create properties 20390 A, 20391 B; sphere r6, attached to the wormhole creature): a player that
// enters casts the cover spell and, when the partner exists, travels to it; Recombobulation blocks a bounce for 3.2 s
struct at_xymox_wormhole : AreaTriggerAI
{
    using AreaTriggerAI::AreaTriggerAI;

    void OnUnitEnter(Unit* unit) override
    {
        if (!unit->IsPlayer() || !unit->IsAlive() || unit->HasAura(SPELL_RECOMBOBULATION))
            return;

        Unit* wormhole = at->GetCaster();
        if (!wormhole || wormhole->GetTypeId() != TYPEID_UNIT)
            return;

        bool isA = wormhole->GetEntry() == NPC_WORMHOLE_A;
        unit->CastSpell(unit, isA ? SPELL_COVER_A : SPELL_COVER_B, TRIGGERED_FULL_MASK);

        if (!wormhole->ToCreature()->FindNearestCreature(isA ? NPC_WORMHOLE_B : NPC_WORMHOLE_A, 1000.0f))
            return;

        unit->CastSpell(unit, isA ? SPELL_TELEPORT_A_TO_B : SPELL_TELEPORT_B_TO_A, TRIGGERED_FULL_MASK);
    }
};

// Worldbreaking areatrigger (create properties 21397, sphere r25 on the relic, 10 s): five stacking gravity pulls toward the
// relic on every player inside 25 yd; all end with the areatrigger
struct at_xymox_worldbreaking : AreaTriggerAI
{
    using AreaTriggerAI::AreaTriggerAI;

    void OnUpdate(uint32 diff) override
    {
        _elapsed += Milliseconds(diff);
        while (_pulses < std::size(WorldbreakingPulls) && _elapsed >= WorldbreakingPulls[_pulses])
        {
            Pull();
            ++_pulses;
        }
    }

    void Pull()
    {
        ObjectGuid force = ObjectGuid::Create<HighGuid::AreaTrigger>(at->GetMapId(), AreaTriggerWorldbreakingForce, at->GetMap()->GenerateLowGuid<HighGuid::AreaTrigger>());
        _forces.push_back(force);
        for (MapReference const& ref : at->GetMap()->GetPlayers())
            if (Player* player = ref.GetSource())
                if (player->IsAlive() && !player->IsGameMaster() && player->IsWithinDist2d(at, WorldbreakingRadius))
                    player->ApplyMovementForce(force, at->GetPosition(), WorldbreakingMagnitude, MovementForceType::Gravity);
    }

    void OnRemove() override
    {
        for (MapReference const& ref : at->GetMap()->GetPlayers())
            if (Player* player = ref.GetSource())
                for (ObjectGuid const& force : _forces)
                    player->RemoveMovementForce(force);
        _forces.clear();
    }

private:
    Milliseconds _elapsed = 0ms;
    size_t _pulses = 0;
    GuidVector _forces;
};

void AddSC_boss_artificer_xymox()
{
    RegisterCastleNathriaCreatureAI(boss_artificer_xymox);
    RegisterCreatureAI(npc_xymox_wormhole);
    RegisterCreatureAI(npc_xymox_rift_blast_portal);
    RegisterCreatureAI(npc_xymox_fleeting_spirit);
    RegisterSpellScript(spell_xymox_glyph_of_destruction_cast);
    RegisterSpellScript(spell_xymox_glyph_of_destruction);
    RegisterSpellScript(spell_xymox_hyperlight_spark);
    RegisterSpellScript(spell_xymox_dimensional_tear);
    RegisterSpellScript(spell_xymox_tear_mark);
    RegisterSpellScript(spell_xymox_crystal_activate);
    RegisterSpellScript(spell_xymox_annihilate_cast);
    RegisterSpellScript(spell_xymox_annihilate);
    RegisterAreaTriggerAI(at_xymox_wormhole);
    RegisterAreaTriggerAI(at_xymox_worldbreaking);
}
