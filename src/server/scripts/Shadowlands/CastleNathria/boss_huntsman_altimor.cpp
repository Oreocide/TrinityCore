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
 * Huntsman Altimor (DungeonEncounter 2418) with Margore, Bargast and Hecutis. Four phases that follow the hound deaths, never
 * Altimor's health: Margore, Bargast, Hecutis, then Altimor alone (Broken Bond). Huntsman's Bond 334504 (aura 300, 100%) links
 * Altimor and the active hound in both directions; each hound dies on its own pool (about 0.30 / 0.27 / 0.30 of Altimor's).
 * Offsets are Warcraft Logs Normal medians (30 Normal kills), checked against DBM, BigWigs and the 2026-11-06 retail capture
 * (trinitycore-tracking/castle-nathria/bosses/huntsman-altimor.md and huntsman-altimor_followup.json, which overrides the spec).
 *
 * - Release: Bargast and Hecutis wait in their kennels (immune, passive). After the previous hound dies the yell comes at +2.85 s
 *   (Bargast) / +3.8 s (Hecutis), the kennel gate opens at +2.06 / +3.04 s, the hound runs 22 yd out at +3.25 / +4.17 s, the Bond
 *   and its aggro come at +5.4 / +6.3 s and the gate closes at +5.73 / +6.65 s. The dead hound's corpse goes 11 s after its death.
 *   A wipe returns every hound to its kennel state and respawns the dead ones.
 * - Altimor: Shoot every 2 s (the aura 367 that retail uses to replace his auto attack is a stub in the core, so it is cast here),
 *   Spreadshot (he turns to his victim, 45 degree cone; 12.2 s when idle, 2.3 s after the last Sinseeker bolt, 12 s after the
 *   Vicious Lunge mark, swallowed by a Command or a Shades cast, 3.7 s in phase 4), Sinseeker (marks on three random non-tanks
 *   at +0 / 0.75 / 1.5 s, bolts at 5.5 / 6.5 / 7.5 s in mark order, each along a line he first turns to face, a stacking
 *   DoT per hit; the chain interval is 51 / 61 / 50.5 / 25 s and a cast inside a transition window keeps +51 s).
 * - Margore: Command 335119 at 16.9 s then every 25.6 s, a random non-tank is marked for 6 s, then he jumps to that player and
 *   the landing splits 225% over everyone within 6 yd. Jagged Claws at 10.8 s, then 10.9 s after the last cast, held back while
 *   Altimor casts Sinseeker (to the last bolt + 1 s) and during a Lunge (to the landing + 4 s).
 * - Bargast: Rip Soul on the tank at 9.9 s then every 30.5 s; the Ripped Soul appears 2.8 s after the cast start at the tank at
 *   90% health and runs to Altimor, +20% speed per second, and despawns when healed to full. Command at 16.0 s, Shades 1.5 s
 *   later and every 61 s. The Shade of Bargast roars 1.4 s after spawn and every 6.1 s and becomes crowd control immune 69.6 s
 *   after spawn.
 * - Hecutis: Command at 12.9 s, then 25.2 s and 20.6 s; Petrifying Howl 1.5 s after each Command on three random non-tanks that do
 *   not carry the 40 s protection; 8 s later the petrified player shatters (15% in 10 yd) and leaves a 300 s Stone Shards field.
 *   Crushing Stone stacks with his swings and he sheds one stack a second while he moves.
 *
 * Not settled (one solo Normal capture plus Warcraft Logs): Sinseeker's repeat in phase 3 (50.5 s, DBM Normal 50-51 against
 * BigWigs 60) and phase 4 (25 s, one capture showed none in 24.65 s; low), the phase 4 Spreadshot after Sinseeker (1.1 s, one
 * sample), the Spreadshot Howl reset in DBM (14.3 s, not used), whether a Howl whose targets fall short takes protected players
 * or tanks first, the Devour Soul stacking (30 s in the capture, the data says 60 s), the Destabilize "loss of control" set
 * (stun, confuse, flee here), how Margore's Command reaches him (the companion casts 335120 / 335126 / 335127 are not cast), the
 * live Sinseeker cast bar (5.5 s in the capture, 4.0 s in the data: the bar here is the data's, the marks and bolts keep the
 * captured times), and the 21119 areatrigger of the Bond visual (no create properties exist, so the aura shows no beam).
 * Values that are guesses and not evidence: the Ripped Soul reaches Altimor at 5 yd and despawns 1.4 s after it starts Devour Soul
 * (the data cast is 1.0 s, the capture 0.69 s), the 14 s Lunge safety timeout, the Hecutis Command gap of 20.6 s kept past the
 * third Command (WCL shows three), and which side of Bargast a Shade appears on (random; two samples).
 * Heroic and Mythic extras are left out because nothing captured them: Vicious Wound 334960 on Lunge hits, the Mythic empowered
 * Sinseekers (Vicious Bolt 338615, Pierce Soul 338609, Shatter Shot 338593), Unstable Soul 339639, the heroic and mythic Shade
 * counts (two Shades from 334785 on Mythic) and the Devour Soul strengths past Normal (those come with the spell data).
 */

#include "AreaTrigger.h"
#include "AreaTriggerAI.h"
#include "Conversation.h"
#include "Containers.h"
#include "GameObject.h"
#include "GameTime.h"
#include "InstanceScript.h"
#include "Map.h"
#include "MoveSpline.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "Spell.h"
#include "SpellAuraEffects.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "SpellScript.h"
#include "TemporarySummon.h"
#include "castle_nathria.h"
#include <any>
#include <functional>

enum AltimorSpells
{
    // Altimor
    SPELL_SHOOT                         = 335016,   // retail replaces his auto attack with it (aura 367, a stub in the core)
    SPELL_SPREADSHOT                    = 334404,   // 0.5 s cast, 45 degree cone along his facing
    SPELL_SINSEEKER                     = 335114,
    SPELL_HUNTSMAN_MARK_1               = 335111,
    SPELL_HUNTSMAN_MARK_2               = 335112,
    SPELL_HUNTSMAN_MARK_3               = 335113,
    SPELL_SINSEEKER_BOLT                = 335116,   // effect 1: line 4.5 yd wide along his facing
    SPELL_SINSEEKER_DOT                 = 335304,   // one stack per bolt that hits, 30 s
    SPELL_COMMAND_MARGORE               = 335119,   // 1.5 s cast on Margore
    SPELL_HUNTSMANS_BOND                = 334504,   // aura 300, 100%, both directions
    SPELL_BOND_VISUAL                   = 335118,   // aura on the hound, areatrigger 21119
    SPELL_BROKEN_BOND                   = 345902,   // Hecutis on Altimor at his death: +100% damage taken
    SPELL_DEATH_MARGORE                 = 335722,   // conversation 15620
    SPELL_DEATH_BARGAST                 = 335726,   // conversation 15621
    SPELL_DEATH_HECUTIS                 = 335727,   // conversation 15622

    // Margore
    SPELL_JAGGED_CLAWS                  = 334971,
    SPELL_VICIOUS_LUNGE_VISUAL          = 334953,
    SPELL_VICIOUS_LUNGE_MARK            = 334945,
    SPELL_VICIOUS_LUNGE_JUMP            = 334942,   // effect 1: jump to the marked player, arrival cast 334939
    SPELL_VICIOUS_LUNGE_LANDING         = 334939,

    // Bargast
    SPELL_BARGAST_COMMAND               = 334792,   // 1.5 s cast on Altimor
    SPELL_RIP_SOUL                      = 334797,
    SPELL_SHADES_OF_BARGAST             = 334757,
    SPELL_RIPPED_SOUL_SPEED             = 335379,   // 1 s periodic, casts 335381 (+20% speed, stacks)
    SPELL_RIPPED_SOUL_HEALTH_CHECK      = 340599,
    SPELL_DEVOUR_SOUL                   = 334884,
    SPELL_DEATHLY_ROAR                  = 334708,
    SPELL_DESTABILIZE                   = 334688,
    SPELL_DESTABILIZE_STACK             = 334695,
    SPELL_SHADE_ENERGY                  = 335302,
    SPELL_UNYIELDING                    = 335303,

    // Hecutis
    SPELL_HECUTIS_COMMAND               = 335129,   // 1.5 s cast on Altimor
    SPELL_PETRIFYING_HOWL               = 334852,
    SPELL_HOWL_SHATTER                  = 334889,   // at the aura's end: 10 yd damage and areatrigger 21100
    SPELL_HOWL_SLOW                     = 334919,
    SPELL_HOWL_PROTECTION               = 335310,   // 40 s, the next Howl skips it
    SPELL_STONE_SHARDS                  = 334893,
    SPELL_CRUSHING_STONE                = 334860,
    SPELL_SHATTERING_STONE              = 334932
};

enum AltimorCreatures
{
    NPC_MARGORE                         = 165067,
    NPC_BARGAST                         = 169457,
    NPC_HECUTIS                         = 169458,
    NPC_SHADE_OF_BARGAST                = 171557,
    NPC_RIPPED_SOUL                     = 171577
};

enum AltimorGameObjects
{
    GO_KENNEL_MARGORE                   = 353140,
    GO_KENNEL_BARGAST                   = 353138,
    GO_KENNEL_HECUTIS                   = 353142
};

enum AltimorConversations
{
    CONVERSATION_COMMAND_MARGORE        = 15571,    // +0.4 s after the cast start
    CONVERSATION_SINSEEKER              = 15591,    // +0.3 s after the cast start
    CONVERSATION_COMMAND_BARGAST        = 15572,
    CONVERSATION_COMMAND_HECUTIS        = 15573
};

enum AltimorEvents
{
    // Altimor
    EVENT_SHOOT = 1,
    EVENT_SPREADSHOT,
    EVENT_SINSEEKER,
    EVENT_COMMAND_MARGORE,

    // hounds
    EVENT_JAGGED_CLAWS,
    EVENT_RIP_SOUL,
    EVENT_BARGAST_COMMAND,
    EVENT_SHADES,
    EVENT_HECUTIS_COMMAND,
    EVENT_PETRIFYING_HOWL,

    // Shade of Bargast
    EVENT_DEATHLY_ROAR
};

enum AltimorActions
{
    ACTION_HOUND_RUN_OUT = 1,
    ACTION_HOUND_ENGAGE,
    ACTION_HOUND_RESET,
    ACTION_MARGORE_COMMAND_STARTED,
    ACTION_MARGORE_LUNGE,
    ACTION_MARGORE_LUNGE_LANDED
};

enum AltimorTexts
{
    // Altimor
    SAY_AGGRO                           = 0,    // The hunt begins!
    SAY_SPREADSHOT                      = 1,    // two lines, one at random on each Spreadshot outside phase 4
    SAY_BARGAST_RELEASED                = 2,
    SAY_HECUTIS_RELEASED                = 3,
    SAY_PHASE_FOUR                      = 4,
    SAY_DEATH                           = 5,

    // Bargast
    SAY_RIP_SOUL                        = 0,    // raid boss emote on the Rip Soul cast start

    // Shade of Bargast
    SAY_UNYIELDING                      = 0
};

enum AltimorMisc
{
    POINT_RUN_OUT                       = 1
};

static constexpr uint32 BossCreatureEntries[] = { BOSS_HUNTSMAN_ALTIMOR, NPC_MARGORE, NPC_BARGAST, NPC_HECUTIS };
static constexpr uint32 HoundEntries[] = { NPC_MARGORE, NPC_BARGAST, NPC_HECUTIS };
static constexpr uint32 KennelGates[] = { GO_KENNEL_MARGORE, GO_KENNEL_BARGAST, GO_KENNEL_HECUTIS };
static constexpr uint32 DeathSpells[] = { SPELL_DEATH_MARGORE, SPELL_DEATH_BARGAST, SPELL_DEATH_HECUTIS };
static constexpr float SearchRange = 500.0f;

// Altimor, from the pull or from the phase start (the Bond), Warcraft Logs Normal medians [capture]
static constexpr Milliseconds ShootInterval = 2s;                                   // capture: 2.0 s between the gaps
static constexpr Milliseconds SpreadshotFirst[] = { 6100ms, 5600ms, 5700ms, 3800ms };
static constexpr Milliseconds SpreadshotIdle = 12200ms;
static constexpr Milliseconds SpreadshotPhaseFour = 3700ms;
static constexpr Milliseconds SpreadshotAfterBolt = 2300ms;
static constexpr Milliseconds SpreadshotAfterBoltPhaseFour = 1100ms;                // one sample, low confidence
static constexpr Milliseconds SpreadshotAfterLunge = 12s;
static constexpr Milliseconds SinseekerFirst[] = { 29s, 32s, 28700ms, 6500ms };
static constexpr Milliseconds SinseekerRepeat[] = { 51s, 61s, 50500ms, 25s };       // phases 3 and 4 low confidence
static constexpr Milliseconds SinseekerTransitionRepeat = 51s;
static constexpr Milliseconds SinseekerMarkStep = 750ms;
static constexpr Milliseconds SinseekerBoltFirst = 5500ms;
static constexpr Milliseconds SinseekerBoltStep = 1s;
static constexpr Milliseconds SinseekerShootDelay = 2s;
static constexpr Milliseconds SinseekerConversationDelay = 300ms;
static constexpr Milliseconds CommandMargoreFirst = 16900ms;
static constexpr Milliseconds CommandMargoreRepeat = 25600ms;
static constexpr Milliseconds CommandCast = 1500ms;
static constexpr Milliseconds CommandConversationDelay = 400ms;
static constexpr uint8 SinseekerMarks = 3;

// release of Bargast and Hecutis, from the previous hound's death (capture)
struct HoundRelease
{
    Milliseconds Yell;
    Milliseconds GateOpen;
    Milliseconds RunOut;
    Milliseconds Bond;
    Milliseconds GateClose;
    Position FirstSpline;
};

static HoundRelease const Releases[] =
{
    { 2850ms, 2060ms, 3250ms, 5400ms, 5730ms, { -1657.26f, 6717.00f, 4279.04f } },    // Bargast
    { 3800ms, 3040ms, 4170ms, 6300ms, 6650ms, { -1657.40f, 6683.60f, 4279.00f } }     // Hecutis
};

// Margore's place on the floor before the pull: the pre-fight RP (conversation 16280, gate 353140) lets him out there
// about 113 s before the pull. That RP is not scripted yet, so he still spawns in his kennel.
static Position const MargoreFloorHome = { -1653.80f, 6696.90f, 4279.04f };

// The three kennels are one navmesh island (x > -1644.5, y 6676 to 6724) with no link to the arena floor. A path from
// inside ends at the kennel mouth, so a hound must leave the kennels on a straight line, as the retail splines do.
static bool IsInKennelStrip(Position const& pos)
{
    return pos.GetPositionX() > -1645.0f && pos.GetPositionY() > 6673.0f && pos.GetPositionY() < 6727.0f;
}

static constexpr Milliseconds CorpseDespawn = 11s;

// Margore
static constexpr Milliseconds JaggedClawsFirst = 10800ms;
static constexpr Milliseconds JaggedClawsGap = 10900ms;
static constexpr Milliseconds JaggedClawsAfterBolts = 1s;
static constexpr Milliseconds JaggedClawsAfterLanding = 4s;
static constexpr Milliseconds LungeJumpDelay = 6s;
static constexpr Milliseconds LungeTimeout = 14s;

// Bargast
static constexpr Milliseconds RipSoulFirst = 9900ms;
static constexpr Milliseconds RipSoulRepeat = 30500ms;
static constexpr Milliseconds RipSoulSpawnDelay = 2800ms;
static constexpr Milliseconds BargastCommandFirst = 16s;
static constexpr Milliseconds BargastCommandRepeat = 61s;
static constexpr Milliseconds ShadesAfterCommand = 1500ms;
static constexpr Milliseconds ShadeSpawnDelay = 4600ms;     // after the Shades cast start (2.0 s cast)
static constexpr float ShadeDistance = 15.0f;
static constexpr uint8 RippedSoulHealthPct = 90;
static constexpr float RippedSoulArriveRange = 5.0f;
static constexpr Milliseconds RippedSoulDevourDespawn = 1400ms;
static constexpr Milliseconds DeathlyRoarFirst = 1400ms;
static constexpr Milliseconds DeathlyRoarRepeat = 6100ms;
static constexpr Milliseconds UnyieldingDelay = 69600ms;
static constexpr Milliseconds UnyieldingWarning = 5900ms;

// Hecutis
static constexpr Milliseconds HecutisCommandFirst = 12900ms;
static constexpr Milliseconds HecutisCommandGaps[] = { 25200ms, 20600ms };
static constexpr uint8 HowlTargets = 3;

// a lookup that does not depend on which hound asks: every living hound, boss or add of this fight near the reference
static GuidSet CollectTanks(Creature const* reference)
{
    GuidSet tanks;
    for (uint32 entry : BossCreatureEntries)
    {
        std::list<Creature*> creatures;
        reference->GetCreatureListWithEntryInGrid(creatures, entry, SearchRange);
        for (Creature* creature : creatures)
            if (creature->IsAlive() && creature->GetVictim())
                tanks.insert(creature->GetVictim()->GetGUID());
    }
    return tanks;
}

// Random non-tank players first (retail never marked a tank in Warcraft Logs, with a fallback to the tank when nobody else
// exists: the solo capture marked the only player three times). Players that carry skipAura come after the others. With
// allowRepeat the list is filled up to count by repeating the picks.
static std::vector<Unit*> PickRaidTargets(Creature const* caster, uint32 count, uint32 skipAura, bool allowRepeat)
{
    GuidSet tanks = CollectTanks(caster);
    std::vector<Unit*> preferred;
    std::vector<Unit*> skipped;
    std::vector<Unit*> tankUnits;
    for (MapReference const& ref : caster->GetMap()->GetPlayers())
    {
        Player* player = ref.GetSource();
        if (!player || !player->IsAlive() || player->IsGameMaster())
            continue;

        if (tanks.contains(player->GetGUID()))
            tankUnits.push_back(player);
        else if (skipAura && player->HasAura(skipAura))
            skipped.push_back(player);
        else
            preferred.push_back(player);
    }

    Trinity::Containers::RandomShuffle(preferred);
    Trinity::Containers::RandomShuffle(skipped);
    Trinity::Containers::RandomShuffle(tankUnits);

    std::vector<Unit*> result;
    for (std::vector<Unit*>* group : { &preferred, &skipped, &tankUnits })
        for (Unit* unit : *group)
            if (result.size() < count)
                result.push_back(unit);

    if (allowRepeat && !result.empty())
    {
        size_t unique = result.size();
        for (size_t i = 0; result.size() < count; ++i)
            result.push_back(result[i % unique]);
    }
    return result;
}

static CastSpellExtraArgs PreselectedTargets(std::vector<Unit*> const& units)
{
    std::vector<ObjectGuid> guids;
    for (Unit const* unit : units)
        guids.push_back(unit->GetGUID());
    return CastSpellExtraArgs(TRIGGERED_FULL_MASK).SetCustomArg(std::move(guids));
}

static void FaceUnit(Creature* creature, Unit* target)
{
    // the cone and the line follow the caster's orientation at cast time, the facing spline only follows a moment later
    creature->SetOrientation(creature->GetAbsoluteAngle(target));
    creature->SetFacingToObject(target);
}

// 165066 - Huntsman Altimor
struct boss_huntsman_altimor : public BossAI
{
    boss_huntsman_altimor(Creature* creature) : BossAI(creature, DATA_HUNTSMAN_ALTIMOR) { }

    void Reset() override
    {
        _Reset();
        ResetFight();
        me->SetCanMelee(false);
        SetCombatMovement(false);
    }

    void JustEngagedWith(Unit* who) override
    {
        BossAI::JustEngagedWith(who);
        Talk(SAY_AGGRO);
        CacheHounds();

        // Margore is already out and linked at the pull
        if (Creature* margore = GetHound(0, true))
        {
            if (!margore->IsEngaged())
                margore->EngageWithTarget(who);
            DoCast(margore, SPELL_HUNTSMANS_BOND, true);
            margore->CastSpell(me, SPELL_HUNTSMANS_BOND, true);
        }

        _phase = 1;
        events.ScheduleEvent(EVENT_SHOOT, 0ms);
        events.ScheduleEvent(EVENT_SPREADSHOT, SpreadshotFirst[0]);
        events.ScheduleEvent(EVENT_SINSEEKER, SinseekerFirst[0]);
        events.ScheduleEvent(EVENT_COMMAND_MARGORE, CommandMargoreFirst);
    }

    void JustSummoned(Creature* summon) override
    {
        summons.Summon(summon);
        // the Ripped Soul is friendly and must not be pulled into combat
        if (summon->GetEntry() == NPC_SHADE_OF_BARGAST)
            DoZoneInCombat(summon);
    }

    Creature* SummonAdd(uint32 entry, Position const& pos)
    {
        return me->SummonCreature(entry, pos, TEMPSUMMON_MANUAL_DESPAWN);
    }

    void JustDied(Unit* killer) override
    {
        BossAI::JustDied(killer);
        Talk(SAY_DEATH);
        RemoveFightAuras();
    }

    void EnterEvadeMode(EvadeReason why) override
    {
        ResetFight();
        BossAI::EnterEvadeMode(why);
        ResetHounds();
    }

    // hounds -----------------------------------------------------------------------------------------------------------------

    Creature* GetHound(uint8 index, bool aliveOnly) const
    {
        std::list<Creature*> creatures;
        me->GetCreatureListWithEntryInGrid(creatures, HoundEntries[index], SearchRange);
        Creature* corpse = nullptr;
        for (Creature* creature : creatures)
        {
            if (creature->IsAlive())
                return creature;
            if (!aliveOnly)
                corpse = creature;
        }
        return corpse;
    }

    // the spawn ids are kept so that a hound whose corpse is gone can be respawned after a wipe
    void CacheHounds()
    {
        for (uint8 i = 0; i < 3; ++i)
            if (Creature* hound = GetHound(i, false))
                if (hound->GetSpawnId())
                    _houndSpawnIds[i] = hound->GetSpawnId();
    }

    void ResetHounds()
    {
        for (uint8 i = 0; i < 3; ++i)
        {
            Creature* hound = GetHound(i, false);
            if (hound && hound->IsAlive())
                hound->AI()->DoAction(ACTION_HOUND_RESET);
            else if (_houndSpawnIds[i])
                me->GetMap()->Respawn(SPAWN_TYPE_CREATURE, _houndSpawnIds[i]);
        }
    }

    void SetKennelGate(uint8 index, bool open)
    {
        if (GameObject* gate = me->FindNearestGameObject(KennelGates[index], SearchRange))
            gate->SetGoState(open ? GO_STATE_ACTIVE : GO_STATE_READY);
    }

    void Later(Milliseconds delay, std::function<void()> fn)
    {
        me->m_Events.AddEventAtOffset([this, serial = _serial, fn = std::move(fn)]()
        {
            if (serial == _serial)
                fn();
        }, delay);
    }

    // a hound died: the Bond goes with it and the next one is released
    void OnHoundDied(uint8 index)
    {
        me->RemoveAurasDueToSpell(SPELL_HUNTSMANS_BOND);
        _sinseekerCarried = false;

        if (index == 0)
            events.CancelEvent(EVENT_COMMAND_MARGORE);

        if (index >= 2)
        {
            // Hecutis cast Broken Bond on Altimor at his death; Altimor is alone from here
            _phase = 4;
            _inTransition = false;
            Talk(SAY_PHASE_FOUR);
            events.RescheduleEvent(EVENT_SPREADSHOT, SpreadshotFirst[3]);
            events.RescheduleEvent(EVENT_SINSEEKER, SinseekerFirst[3]);
            return;
        }

        _inTransition = true;
        uint8 next = index + 1;
        HoundRelease const& release = Releases[next - 1];
        Later(release.Yell, [this, next]() { Talk(next == 1 ? SAY_BARGAST_RELEASED : SAY_HECUTIS_RELEASED); });
        Later(release.GateOpen, [this, next]() { SetKennelGate(next, true); });
        Later(release.RunOut, [this, next]()
        {
            if (Creature* hound = GetHound(next, true))
                hound->AI()->DoAction(ACTION_HOUND_RUN_OUT);
        });
        Later(release.Bond, [this, next]() { StartHoundPhase(next); });
        Later(release.GateClose, [this, next]() { SetKennelGate(next, false); });
    }

    void StartHoundPhase(uint8 index)
    {
        _phase = index + 1;
        _inTransition = false;

        if (Creature* hound = GetHound(index, true))
        {
            DoCast(hound, SPELL_HUNTSMANS_BOND, true);
            hound->AI()->DoAction(ACTION_HOUND_ENGAGE);
        }

        // the Bond sets Sinseeker again unless it was cast inside the transition window (its chain is kept then)
        events.RescheduleEvent(EVENT_SPREADSHOT, SpreadshotFirst[_phase - 1]);
        if (!_sinseekerCarried)
            events.RescheduleEvent(EVENT_SINSEEKER, SinseekerFirst[_phase - 1]);
        _sinseekerCarried = false;
    }

    // hound hooks ------------------------------------------------------------------------------------------------------------

    // Command or Shades: the Spreadshot that was pending is lost, not queued
    void SwallowSpreadshot() { events.CancelEvent(EVENT_SPREADSHOT); }

    // the Lunge mark resets the Spreadshot to about 12 s
    void OnLungeMarked() { events.RescheduleEvent(EVENT_SPREADSHOT, SpreadshotAfterLunge); }

    void StartConversation(uint32 conversationId)
    {
        Conversation::CreateConversation(conversationId, me, me->GetPosition(), ObjectGuid::Empty);
    }

    uint32 GetSinseekerEnd() const { return _sinseekerEnd; }

    bool IsSinseekerBusy(Milliseconds extra = 0ms) const
    {
        return GameTime::GetGameTimeMS() < _sinseekerEnd + uint32(extra.count());
    }

    // abilities --------------------------------------------------------------------------------------------------------------

    void ExecuteEvent(uint32 eventId) override
    {
        // nothing is cast inside a Sinseeker, not even the next one (its marks and bolts would overwrite the pending ones)
        if (IsSinseekerBusy())
        {
            uint32 remaining = _sinseekerEnd - GameTime::GetGameTimeMS();
            if (eventId != EVENT_SPREADSHOT)
                events.RescheduleEvent(eventId, Milliseconds(remaining) + SinseekerShootDelay);
            return;
        }

        switch (eventId)
        {
            case EVENT_SHOOT:
                if (me->GetVictim())
                    DoCastVictim(SPELL_SHOOT);
                events.Repeat(ShootInterval);
                break;
            case EVENT_SPREADSHOT:
                if (Unit* victim = me->GetVictim())
                {
                    FaceUnit(me, victim);
                    DoCast(victim, SPELL_SPREADSHOT);
                    if (_phase < 4)
                        Talk(SAY_SPREADSHOT);
                }
                events.Repeat(_phase >= 4 ? SpreadshotPhaseFour : SpreadshotIdle);
                break;
            case EVENT_SINSEEKER:
                StartSinseeker();
                break;
            case EVENT_COMMAND_MARGORE:
                CommandMargore();
                break;
            default:
                break;
        }
    }

    void CommandMargore()
    {
        Creature* margore = GetHound(0, true);
        if (!margore)
            return;

        SwallowSpreadshot();
        margore->AI()->DoAction(ACTION_MARGORE_COMMAND_STARTED);
        DoCast(margore, SPELL_COMMAND_MARGORE);
        Later(CommandConversationDelay, [this]() { StartConversation(CONVERSATION_COMMAND_MARGORE); });
        Later(CommandCast, [this]()
        {
            if (Creature* hound = GetHound(0, true))
                hound->AI()->DoAction(ACTION_MARGORE_LUNGE);
        });
        events.Repeat(CommandMargoreRepeat);
    }

    void StartSinseeker()
    {
        std::vector<Unit*> targets = PickRaidTargets(me, SinseekerMarks, 0, true);
        if (targets.empty())
        {
            events.Repeat(SinseekerRepeat[_phase - 1]);
            return;
        }

        SwallowSpreadshot();
        events.CancelEvent(EVENT_SHOOT);

        // inside a transition window the chain goes on 51 s after this cast and the Bond does not reset it
        if (_inTransition)
        {
            _sinseekerCarried = true;
            events.Repeat(SinseekerTransitionRepeat);
        }
        else
            events.Repeat(SinseekerRepeat[_phase - 1]);

        Milliseconds lastBolt = SinseekerBoltFirst + SinseekerBoltStep * (SinseekerMarks - 1);
        _sinseekerEnd = GameTime::GetGameTimeMS() + uint32(lastBolt.count());

        DoCastSelf(SPELL_SINSEEKER);
        Later(SinseekerConversationDelay, [this]() { StartConversation(CONVERSATION_SINSEEKER); });

        static constexpr uint32 markSpells[SinseekerMarks] = { SPELL_HUNTSMAN_MARK_1, SPELL_HUNTSMAN_MARK_2, SPELL_HUNTSMAN_MARK_3 };
        for (uint8 i = 0; i < SinseekerMarks; ++i)
        {
            Unit* target = targets[i];
            _marks[i] = target->GetGUID();
            // the mark spells select an area; the script narrows each to the player picked here
            Later(SinseekerMarkStep * i, [this, i, target = target->GetGUID()]()
            {
                if (Unit* unit = ObjectAccessor::GetUnit(*me, target))
                    me->CastSpell(me, markSpells[i], PreselectedTargets({ unit }));
            });
            Later(SinseekerBoltFirst + SinseekerBoltStep * i, [this, i]() { FireSinseekerBolt(i); });
        }

        Later(lastBolt, [this]()
        {
            RestoreAfterSinseeker();
        });
    }

    void FireSinseekerBolt(uint8 index)
    {
        static constexpr uint32 markSpells[SinseekerMarks] = { SPELL_HUNTSMAN_MARK_1, SPELL_HUNTSMAN_MARK_2, SPELL_HUNTSMAN_MARK_3 };
        Unit* target = ObjectAccessor::GetUnit(*me, _marks[index]);
        if (!target)
            return;

        // the mark ends at its bolt
        target->RemoveAurasDueToSpell(markSpells[index], me->GetGUID());
        if (!target->IsAlive())
            return;

        FaceUnit(me, target);
        me->CastSpell(target, SPELL_SINSEEKER_BOLT, TRIGGERED_FULL_MASK);
    }

    void RestoreAfterSinseeker()
    {
        events.RescheduleEvent(EVENT_SHOOT, SinseekerShootDelay);
        events.RescheduleEvent(EVENT_SPREADSHOT, _phase >= 4 ? SpreadshotAfterBoltPhaseFour : SpreadshotAfterBolt);
    }

    // -------------------------------------------------------------------------------------------------------------------------

    void RemoveFightAuras()
    {
        for (uint32 spellId : { SPELL_HUNTSMAN_MARK_1, SPELL_HUNTSMAN_MARK_2, SPELL_HUNTSMAN_MARK_3, SPELL_SINSEEKER_DOT,
            SPELL_VICIOUS_LUNGE_MARK, SPELL_JAGGED_CLAWS, SPELL_PETRIFYING_HOWL, SPELL_HOWL_PROTECTION, SPELL_HOWL_SLOW,
            SPELL_STONE_SHARDS })
            instance->DoRemoveAurasDueToSpellOnPlayers(spellId);

        for (MapReference const& ref : me->GetMap()->GetPlayers())
            if (Player* player = ref.GetSource())
                player->RemoveAreaTrigger(SPELL_HOWL_SHATTER);
    }

    void ResetFight()
    {
        ++_serial;
        _phase = 1;
        _inTransition = false;
        _sinseekerCarried = false;
        _sinseekerEnd = 0;
        for (ObjectGuid& guid : _marks)
            guid.Clear();

        me->RemoveAurasDueToSpell(SPELL_HUNTSMANS_BOND);
        me->RemoveAurasDueToSpell(SPELL_BROKEN_BOND);
        me->RemoveAurasDueToSpell(SPELL_DEVOUR_SOUL);
        for (uint8 i = 1; i < 3; ++i)
            SetKennelGate(i, false);

        if (instance)
            RemoveFightAuras();
    }

private:
    uint32 _serial = 0;
    uint8 _phase = 1;
    bool _inTransition = false;
    bool _sinseekerCarried = false;
    uint32 _sinseekerEnd = 0;
    ObjectGuid _marks[SinseekerMarks];
    ObjectGuid::LowType _houndSpawnIds[3] = { };
};

static boss_huntsman_altimor* GetAltimorAI(Creature* from)
{
    if (Creature* altimor = from->FindNearestCreature(BOSS_HUNTSMAN_ALTIMOR, SearchRange))
        return CAST_AI(boss_huntsman_altimor, altimor->AI());
    return nullptr;
}

// the three hounds: Margore is out and linked at the pull; Bargast and Hecutis wait in their kennels
struct npc_altimor_hound_base : public ScriptedAI
{
    npc_altimor_hound_base(Creature* creature, uint8 index) : ScriptedAI(creature), _index(index) { }

    void Reset() override
    {
        _events.Reset();
        ++_serial;
        if (_index != 0)
            EnterKennel();
    }

    // flags 832 in the capture: immune to players and creatures, no target, no combat
    void EnterKennel()
    {
        me->SetImmuneToAll(true);
        me->SetReactState(REACT_PASSIVE);
        // retail shows the Bond visual from the release on
        me->RemoveAurasDueToSpell(SPELL_BOND_VISUAL);
    }

    void JustEngagedWith(Unit* who) override
    {
        if (Creature* altimor = me->FindNearestCreature(BOSS_HUNTSMAN_ALTIMOR, SearchRange))
            if (!altimor->IsEngaged())
                altimor->EngageWithTarget(who);

        ScheduleAbilities();
    }

    virtual void ScheduleAbilities() { }
    virtual void ExecuteEvent(uint32 /*eventId*/) { }

    void DoAction(int32 action) override
    {
        switch (action)
        {
            case ACTION_HOUND_RUN_OUT:
                // 22 yd out of the kennel at the hound's run speed (14 yd/s in the capture), then straight on at the Bond
                me->GetMotionMaster()->MovePoint(POINT_RUN_OUT, Releases[_index - 1].FirstSpline, false);
                break;
            case ACTION_HOUND_ENGAGE:
                Engage();
                break;
            case ACTION_HOUND_RESET:
                ResetHound();
                break;
            default:
                OnAction(action);
                break;
        }
    }

    virtual void OnAction(int32 /*action*/) { }

    Position const& ReleasePoint() const
    {
        return _index == 0 ? MargoreFloorHome : Releases[_index - 1].FirstSpline;
    }

    // A chase from the kennel strip to a target on the floor stops at the strip's edge and is not recalculated until the
    // target moves. A hound on the strip with a target off it (a late run-out at the Bond, Margore pulled in his kennel)
    // runs straight out to its release point first and starts the chase from there.
    bool MustLeaveKennel(Unit const* target) const
    {
        return target && IsInKennelStrip(me->GetPosition()) && !IsInKennelStrip(target->GetPosition());
    }

    void LeaveKennel()
    {
        if (me->GetMotionMaster()->GetCurrentMovementGeneratorType() != POINT_MOTION_TYPE)
            me->GetMotionMaster()->MovePoint(POINT_RUN_OUT, ReleasePoint(), false);
    }

    void AttackStart(Unit* who) override
    {
        if (!MustLeaveKennel(who))
        {
            ScriptedAI::AttackStart(who);
            return;
        }

        me->Attack(who, true);
        if (me->GetVictim() == who)
            LeaveKennel();
    }

    void MovementInform(uint32 type, uint32 id) override
    {
        if (type != POINT_MOTION_TYPE || id != POINT_RUN_OUT)
            return;

        // before the Bond the hound waits at its release point; with a target it chases from here
        if (Unit* victim = me->GetVictim())
            me->GetMotionMaster()->MoveChase(victim);
    }

    void Engage()
    {
        me->SetImmuneToAll(false);
        me->SetReactState(REACT_AGGRESSIVE);
        DoCastSelf(SPELL_BOND_VISUAL, true);

        Creature* altimor = me->FindNearestCreature(BOSS_HUNTSMAN_ALTIMOR, SearchRange);
        if (!altimor)
            return;

        DoCast(altimor, SPELL_HUNTSMANS_BOND, true);
        if (Unit* victim = altimor->GetVictim())
        {
            me->GetMotionMaster()->Clear();
            me->EngageWithTarget(victim);
            AttackStart(victim);
        }
        DoZoneInCombat();
    }

    // a wipe: Bargast and Hecutis go back to their kennels at once, Margore walks home like any evading creature
    void ResetHound()
    {
        _events.Reset();
        ++_serial;
        if (_index == 0)
        {
            if (me->IsEngaged())
                EnterEvadeMode(EvadeReason::Other);
            return;
        }

        me->InterruptNonMeleeSpells(true);
        me->CombatStop(true);
        // without this the AI stays engaged: UpdateVictim then evades the passive hound every tick, and each evade
        // teleports it home again and clears its next run-out
        if (IsEngaged())
            EngagementOver();
        me->GetMotionMaster()->Clear();
        me->RemoveAurasDueToSpell(SPELL_HUNTSMANS_BOND);
        me->RemoveAurasDueToSpell(SPELL_CRUSHING_STONE);
        me->SetFullHealth();
        // an engage during the run-out moves the home position to where the hound stood, so use the spawn point
        Position const kennel = me->GetRespawnPosition();
        me->SetHomePosition(kennel);
        me->NearTeleportTo(kennel);
        EnterKennel();
    }

    void EnterEvadeMode(EvadeReason why) override
    {
        if (_index != 0)
        {
            ResetHound();
            return;
        }

        ScriptedAI::EnterEvadeMode(why);
        // the fight is over for everyone when one hound gives up
        if (Creature* altimor = me->FindNearestCreature(BOSS_HUNTSMAN_ALTIMOR, SearchRange))
            if (altimor->IsEngaged() && altimor->IsAIEnabled())
                altimor->AI()->EnterEvadeMode(EvadeReason::Other);
    }

    void JustDied(Unit* /*killer*/) override
    {
        _events.Reset();
        ++_serial;
        DoCastSelf(DeathSpells[_index], true);
        me->DespawnOrUnsummon(CorpseDespawn);

        if (_index == 2)
            if (Creature* altimor = me->FindNearestCreature(BOSS_HUNTSMAN_ALTIMOR, SearchRange))
                DoCast(altimor, SPELL_BROKEN_BOND, true);

        if (boss_huntsman_altimor* altimor = GetAltimorAI(me))
            altimor->OnHoundDied(_index);
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim())
            return;

        // the target walked off the strip while the hound chased it inside
        if (me->GetMotionMaster()->GetCurrentMovementGeneratorType() == CHASE_MOTION_TYPE && MustLeaveKennel(me->GetVictim()))
            LeaveKennel();

        _events.Update(diff);
        if (me->HasUnitState(UNIT_STATE_CASTING))
            return;

        while (uint32 eventId = _events.ExecuteEvent())
        {
            ExecuteEvent(eventId);
            if (me->HasUnitState(UNIT_STATE_CASTING))
                return;
        }
    }

    void Later(Milliseconds delay, std::function<void()> fn)
    {
        me->m_Events.AddEventAtOffset([this, serial = _serial, fn = std::move(fn)]()
        {
            if (serial == _serial && me->IsAlive())
                fn();
        }, delay);
    }

protected:
    EventMap _events;
    uint8 _index;
    uint32 _serial = 0;
};

// 165067 - Margore
struct npc_altimor_margore : public npc_altimor_hound_base
{
    npc_altimor_margore(Creature* creature) : npc_altimor_hound_base(creature, 0) { }

    void Reset() override
    {
        npc_altimor_hound_base::Reset();
        _lungeUntil = 0;
        _lungeTarget.Clear();
    }

    void ScheduleAbilities() override
    {
        _lungeUntil = 0;
        _events.ScheduleEvent(EVENT_JAGGED_CLAWS, JaggedClawsFirst);
    }

    void OnAction(int32 action) override
    {
        switch (action)
        {
            case ACTION_MARGORE_COMMAND_STARTED:
                // the Lunge holds Jagged Claws back from the Command to the landing
                _lungeUntil = GameTime::GetGameTimeMS() + uint32(LungeTimeout.count());
                break;
            case ACTION_MARGORE_LUNGE:
                StartLunge();
                break;
            case ACTION_MARGORE_LUNGE_LANDED:
                _lungeUntil = GameTime::GetGameTimeMS() + uint32(JaggedClawsAfterLanding.count());
                break;
            default:
                break;
        }
    }

    // at the end of the Command cast: a random non-tank is marked for 6 s, then Margore jumps to that player
    void StartLunge()
    {
        std::vector<Unit*> targets = PickRaidTargets(me, 1, 0, false);
        if (targets.empty())
            return;

        Unit* target = targets.front();
        _lungeTarget = target->GetGUID();
        DoCastSelf(SPELL_VICIOUS_LUNGE_VISUAL, true);
        me->CastSpell(me, SPELL_VICIOUS_LUNGE_MARK, PreselectedTargets({ target }));
        if (boss_huntsman_altimor* altimor = GetAltimorAI(me))
            altimor->OnLungeMarked();

        Later(LungeJumpDelay, [this]()
        {
            Unit* target = ObjectAccessor::GetUnit(*me, _lungeTarget);
            if (target && target->IsAlive())
            {
                me->CastSpell(target, SPELL_VICIOUS_LUNGE_JUMP, TRIGGERED_IGNORE_CAST_IN_PROGRESS);
                return;
            }

            // the marked player is gone: the Lunge fizzles
            me->RemoveAurasDueToSpell(SPELL_VICIOUS_LUNGE_VISUAL);
            _lungeUntil = GameTime::GetGameTimeMS() + uint32(JaggedClawsAfterLanding.count());
        });
    }

    void ExecuteEvent(uint32 eventId) override
    {
        if (eventId != EVENT_JAGGED_CLAWS)
            return;

        // held back while Altimor casts Sinseeker (to the last bolt + 1 s) and during a Lunge (to the landing + 4 s)
        uint32 now = GameTime::GetGameTimeMS();
        uint32 hold = now;
        if (_lungeUntil > hold)
            hold = _lungeUntil;
        if (boss_huntsman_altimor* altimor = GetAltimorAI(me))
            if (altimor->IsSinseekerBusy(JaggedClawsAfterBolts))
            {
                uint32 busy = altimor->GetSinseekerEnd() + uint32(JaggedClawsAfterBolts.count());
                if (busy > hold)
                    hold = busy;
            }

        if (hold > now)
        {
            _events.ScheduleEvent(EVENT_JAGGED_CLAWS, Milliseconds(hold - now + 50));
            return;
        }

        DoCastVictim(SPELL_JAGGED_CLAWS);
        _events.ScheduleEvent(EVENT_JAGGED_CLAWS, JaggedClawsGap);
    }

private:
    uint32 _lungeUntil = 0;
    ObjectGuid _lungeTarget;
};

// 169457 - Bargast
struct npc_altimor_bargast : public npc_altimor_hound_base
{
    npc_altimor_bargast(Creature* creature) : npc_altimor_hound_base(creature, 1) { }

    void ScheduleAbilities() override
    {
        _events.ScheduleEvent(EVENT_RIP_SOUL, RipSoulFirst);
        _events.ScheduleEvent(EVENT_BARGAST_COMMAND, BargastCommandFirst);
    }

    void ExecuteEvent(uint32 eventId) override
    {
        switch (eventId)
        {
            case EVENT_RIP_SOUL:
                if (Unit* victim = me->GetVictim())
                {
                    Talk(SAY_RIP_SOUL);
                    DoCast(victim, SPELL_RIP_SOUL);
                    Later(RipSoulSpawnDelay, [this]() { SpawnRippedSoul(); });
                }
                _events.Repeat(RipSoulRepeat);
                break;
            case EVENT_BARGAST_COMMAND:
            {
                Creature* altimor = me->FindNearestCreature(BOSS_HUNTSMAN_ALTIMOR, SearchRange);
                if (!altimor)
                    break;

                if (boss_huntsman_altimor* ai = CAST_AI(boss_huntsman_altimor, altimor->AI()))
                {
                    ai->SwallowSpreadshot();
                    ai->Later(CommandConversationDelay, [ai]() { ai->StartConversation(CONVERSATION_COMMAND_BARGAST); });
                }
                DoCast(altimor, SPELL_BARGAST_COMMAND);
                _events.ScheduleEvent(EVENT_SHADES, ShadesAfterCommand);
                _events.Repeat(BargastCommandRepeat);
                break;
            }
            case EVENT_SHADES:
                // one Shade per cast, 15 yd to his right or left; the spell itself summons nothing (see spell_altimor_shades)
                if (boss_huntsman_altimor* ai = GetAltimorAI(me))
                    ai->SwallowSpreadshot();
                DoCastSelf(SPELL_SHADES_OF_BARGAST);
                Later(ShadeSpawnDelay, [this]() { SpawnShade(); });
                break;
            default:
                break;
        }
    }

    // 2.8 s after the cast start, at the tank, at 90% health (capture)
    void SpawnRippedSoul()
    {
        Unit* victim = me->GetVictim();
        boss_huntsman_altimor* altimor = GetAltimorAI(me);
        if (victim && altimor)
            altimor->SummonAdd(NPC_RIPPED_SOUL, victim->GetPosition());
    }

    void SpawnShade()
    {
        boss_huntsman_altimor* altimor = GetAltimorAI(me);
        if (!altimor)
            return;

        float side = urand(0, 1) ? float(M_PI_2) : -float(M_PI_2);
        Position pos = me->GetNearPosition(ShadeDistance, side);
        altimor->SummonAdd(NPC_SHADE_OF_BARGAST, pos);
    }
};

// 169458 - Hecutis
struct npc_altimor_hecutis : public npc_altimor_hound_base
{
    npc_altimor_hecutis(Creature* creature) : npc_altimor_hound_base(creature, 2) { }

    void ScheduleAbilities() override
    {
        _commandCount = 0;
        _events.ScheduleEvent(EVENT_HECUTIS_COMMAND, HecutisCommandFirst);
    }

    void ExecuteEvent(uint32 eventId) override
    {
        switch (eventId)
        {
            case EVENT_HECUTIS_COMMAND:
            {
                Creature* altimor = me->FindNearestCreature(BOSS_HUNTSMAN_ALTIMOR, SearchRange);
                if (!altimor)
                    break;

                if (boss_huntsman_altimor* ai = CAST_AI(boss_huntsman_altimor, altimor->AI()))
                {
                    ai->SwallowSpreadshot();
                    ai->Later(CommandConversationDelay, [ai]() { ai->StartConversation(CONVERSATION_COMMAND_HECUTIS); });
                }
                DoCast(altimor, SPELL_HECUTIS_COMMAND);
                _events.ScheduleEvent(EVENT_PETRIFYING_HOWL, CommandCast);
                // 25.2 s to the second Command, 20.6 s from then on
                _events.Repeat(HecutisCommandGaps[std::min<size_t>(_commandCount++, std::size(HecutisCommandGaps) - 1)]);
                break;
            }
            case EVENT_PETRIFYING_HOWL:
            {
                // three random non-tanks that do not carry the 40 s protection; the script narrows the area to them
                std::vector<Unit*> targets = PickRaidTargets(me, HowlTargets, SPELL_HOWL_PROTECTION, false);
                if (!targets.empty())
                    DoCastSelf(SPELL_PETRIFYING_HOWL, PreselectedTargetsNonTriggered(targets));
                break;
            }
            default:
                break;
        }
    }

private:
    static CastSpellExtraArgs PreselectedTargetsNonTriggered(std::vector<Unit*> const& units)
    {
        CastSpellExtraArgs args = PreselectedTargets(units);
        args.TriggerFlags = TRIGGERED_NONE;
        return args;
    }

    uint8 _commandCount = 0;
};

// 171577 - Ripped Soul
struct npc_altimor_ripped_soul : public ScriptedAI
{
    npc_altimor_ripped_soul(Creature* creature) : ScriptedAI(creature) { }

    void IsSummonedBy(WorldObject* /*summoner*/) override
    {
        me->SetReactState(REACT_PASSIVE);
        me->SetHealth(me->CountPctFromMaxHealth(RippedSoulHealthPct));
        DoCastSelf(SPELL_RIPPED_SOUL_SPEED, true);
        DoCastSelf(SPELL_RIPPED_SOUL_HEALTH_CHECK, true);
        _checkTimer = 250ms;
    }

    void UpdateAI(uint32 diff) override
    {
        if (_arrived)
            return;

        if (_checkTimer > Milliseconds(diff))
        {
            _checkTimer -= Milliseconds(diff);
            return;
        }
        _checkTimer = 250ms;

        Creature* altimor = me->FindNearestCreature(BOSS_HUNTSMAN_ALTIMOR, SearchRange);
        if (!altimor)
            return;

        if (me->GetExactDist2d(altimor) > RippedSoulArriveRange)
        {
            // it runs for Altimor and speeds up by itself (335381 every second)
            if (me->movespline->Finalized())
                me->GetMotionMaster()->MovePoint(1, altimor->GetPosition());
            return;
        }

        _arrived = true;
        me->StopMoving();
        DoCast(altimor, SPELL_DEVOUR_SOUL);
        me->DespawnOrUnsummon(RippedSoulDevourDespawn);
    }

private:
    Milliseconds _checkTimer = 250ms;
    bool _arrived = false;
};

// 171557 - Shade of Bargast
struct npc_altimor_shade_of_bargast : public ScriptedAI
{
    npc_altimor_shade_of_bargast(Creature* creature) : ScriptedAI(creature) { }

    void IsSummonedBy(WorldObject* /*summoner*/) override
    {
        DoCastSelf(SPELL_DESTABILIZE, true);
        DoCastSelf(SPELL_SHADE_ENERGY, true);
        _events.ScheduleEvent(EVENT_DEATHLY_ROAR, DeathlyRoarFirst);

        // Unyielding (immune to crowd control and forced movement) once its energy is full: 69.6 s after the spawn
        me->m_Events.AddEventAtOffset([this]()
        {
            if (me->IsAlive())
                Talk(SAY_UNYIELDING);
        }, UnyieldingDelay - UnyieldingWarning);
        me->m_Events.AddEventAtOffset([this]()
        {
            if (me->IsAlive())
                DoCastSelf(SPELL_UNYIELDING, true);
        }, UnyieldingDelay);
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim())
            return;

        _events.Update(diff);
        if (me->HasUnitState(UNIT_STATE_CASTING))
            return;

        while (uint32 eventId = _events.ExecuteEvent())
        {
            if (eventId == EVENT_DEATHLY_ROAR)
            {
                DoCastAOE(SPELL_DEATHLY_ROAR);
                _events.Repeat(DeathlyRoarRepeat);
            }
            if (me->HasUnitState(UNIT_STATE_CASTING))
                return;
        }
    }

private:
    EventMap _events;
};

// 335111, 335112, 335113, 334945, 334852 - the script picked the targets (customArg) and narrows the area targets to them:
// the core's random choice would not skip the tanks and the protected players, and the two effects of a spell would not agree
class spell_altimor_preselected_targets : public SpellScript
{
    void FilterTargets(std::list<WorldObject*>& targets)
    {
        auto const* guids = std::any_cast<std::vector<ObjectGuid>>(&GetSpell()->m_customArg);
        if (!guids)
            return;

        targets.clear();
        for (ObjectGuid const& guid : *guids)
            if (Unit* unit = ObjectAccessor::GetUnit(*GetCaster(), guid))
                targets.push_back(unit);
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_altimor_preselected_targets::FilterTargets, EFFECT_ALL, TARGET_UNIT_SRC_AREA_ENEMY);
    }
};

// 335116 - Sinseeker bolt: effect 0 is a dummy on target 151 which the core does not know; the DoT goes on everyone the line of
// effect 1 hits (the marked player and any players in the line)
class spell_altimor_sinseeker_bolt : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_SINSEEKER_DOT });
    }

    void HandleHit(SpellEffIndex /*effIndex*/)
    {
        if (Unit* target = GetHitUnit())
            GetCaster()->CastSpell(target, SPELL_SINSEEKER_DOT, TRIGGERED_FULL_MASK);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_altimor_sinseeker_bolt::HandleHit, EFFECT_1, SPELL_EFFECT_SCHOOL_DAMAGE);
    }
};

// 334939 - Vicious Lunge landing: "divided evenly by the players within 6 yards" is text only, so the damage is split here
class spell_altimor_vicious_lunge_landing : public SpellScript
{
    void CountTargets(std::list<WorldObject*>& targets)
    {
        _targets = uint32(targets.size());
    }

    void SplitDamage(SpellEffIndex /*effIndex*/)
    {
        if (_targets > 1)
            SetHitDamage(GetHitDamage() / _targets);
    }

    void HandleLanded()
    {
        if (Creature* margore = GetCaster()->ToCreature())
            if (margore->IsAIEnabled())
                margore->AI()->DoAction(ACTION_MARGORE_LUNGE_LANDED);
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_altimor_vicious_lunge_landing::CountTargets, EFFECT_0, TARGET_UNIT_SRC_AREA_ENEMY);
        OnEffectHitTarget += SpellEffectFn(spell_altimor_vicious_lunge_landing::SplitDamage, EFFECT_0, SPELL_EFFECT_SCHOOL_DAMAGE);
        AfterCast += SpellCastFn(spell_altimor_vicious_lunge_landing::HandleLanded);
    }

private:
    uint32 _targets = 0;
};

// 334757 - Shades of Bargast: the Shade appears 4.6 s after the cast start, so the creature script summons it and the effects
// (a dummy on Normal, a summon chain on the other difficulties) do nothing
class spell_altimor_shades : public SpellScript
{
    void PreventSummon(SpellEffIndex effIndex)
    {
        PreventHitDefaultEffect(effIndex);
    }

    void Register() override
    {
        OnEffectLaunch += SpellEffectFn(spell_altimor_shades::PreventSummon, EFFECT_0, SPELL_EFFECT_ANY);
        OnEffectLaunch += SpellEffectFn(spell_altimor_shades::PreventSummon, EFFECT_1, SPELL_EFFECT_ANY);
    }
};

// 334688 - Destabilize: every 2 s a stack of 334695 (+100% damage taken) while the Shade is under a loss of control effect
class spell_altimor_destabilize : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DESTABILIZE_STACK });
    }

    void HandlePeriodic(AuraEffect const* /*aurEff*/)
    {
        Unit* shade = GetTarget();
        if (shade->HasUnitState(UNIT_STATE_STUNNED | UNIT_STATE_CONFUSED | UNIT_STATE_FLEEING))
            shade->CastSpell(shade, SPELL_DESTABILIZE_STACK, TRIGGERED_FULL_MASK);
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_altimor_destabilize::HandlePeriodic, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY);
    }
};

// 340599 - Ripped Soul health check: a soul that is healed to full is gone
class spell_altimor_ripped_soul_health_check : public AuraScript
{
    void HandlePeriodic(AuraEffect const* /*aurEff*/)
    {
        Unit* soul = GetTarget();
        if (soul->IsFullHealth())
            if (Creature* creature = soul->ToCreature())
                creature->DespawnOrUnsummon();
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_altimor_ripped_soul_health_check::HandlePeriodic, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY);
    }
};

// 334852 - Petrifying Howl: the player carries the 40 s protection from the first instant, gets the slow after the first second
// and shatters when the aura runs out (15% in 10 yd and the Stone Shards field)
class spell_altimor_petrifying_howl : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_HOWL_PROTECTION, SPELL_HOWL_SLOW, SPELL_HOWL_SHATTER });
    }

    void HandleApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        if (Unit* caster = GetCaster())
            caster->CastSpell(GetTarget(), SPELL_HOWL_PROTECTION, TRIGGERED_FULL_MASK);
    }

    void HandlePeriodic(AuraEffect const* /*aurEff*/)
    {
        Unit* target = GetTarget();
        if (!target->HasAura(SPELL_HOWL_SLOW))
            target->CastSpell(target, SPELL_HOWL_SLOW, CastSpellExtraArgs(TRIGGERED_FULL_MASK).SetOriginalCaster(GetCasterGUID()));
    }

    void HandleRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        if (GetTargetApplication()->GetRemoveMode() != AURA_REMOVE_BY_EXPIRE)
            return;

        // cast by the petrified player himself, as in the capture: the field belongs to the caster and must outlive Hecutis
        Unit* target = GetTarget();
        target->CastSpell(target, SPELL_HOWL_SHATTER, TRIGGERED_FULL_MASK);
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(spell_altimor_petrifying_howl::HandleApply, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY, AURA_EFFECT_HANDLE_REAL);
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_altimor_petrifying_howl::HandlePeriodic, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY);
        AfterEffectRemove += AuraEffectRemoveFn(spell_altimor_petrifying_howl::HandleRemove, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }
};

// 334858 - Hecutis: while he moves he loses one Crushing Stone stack a second and Shattering Stone hits the raid
class spell_altimor_hecutis_shed : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_CRUSHING_STONE, SPELL_SHATTERING_STONE });
    }

    void HandlePeriodic(AuraEffect const* /*aurEff*/)
    {
        Unit* hecutis = GetTarget();
        if (!hecutis->isMoving() || !hecutis->HasAura(SPELL_CRUSHING_STONE))
            return;

        hecutis->RemoveAuraFromStack(SPELL_CRUSHING_STONE);
        hecutis->CastSpell(hecutis, SPELL_SHATTERING_STONE, TRIGGERED_FULL_MASK);
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_altimor_hecutis_shed::HandlePeriodic, EFFECT_1, SPELL_AURA_PERIODIC_DUMMY);
    }
};

// 334889 - Stone Shards field (areatrigger create properties 21100, sphere 10 yd, 300 s): a player inside gets Stone Shards,
// which ends with the exit
struct at_altimor_petrifying_howl : AreaTriggerAI
{
    using AreaTriggerAI::AreaTriggerAI;

    void OnUnitEnter(Unit* unit) override
    {
        if (!unit->IsPlayer())
            return;

        unit->CastSpell(unit, SPELL_STONE_SHARDS, CastSpellExtraArgs(TRIGGERED_FULL_MASK).SetOriginalCaster(at->GetCasterGuid()));
    }

    void OnUnitExit(Unit* unit, AreaTriggerExitReason /*reason*/) override
    {
        if (!unit->IsPlayer())
            return;

        // another Stone Shards field the player still stands in keeps the aura; every petrified player owns his own field
        for (MapReference const& ref : unit->GetMap()->GetPlayers())
            if (Player const* owner = ref.GetSource())
                for (AreaTrigger const* other : owner->GetAreaTriggers(at->GetSpellId()))
                    if (other != at && other->GetInsideUnits().contains(unit->GetGUID()))
                        return;

        unit->RemoveAurasDueToSpell(SPELL_STONE_SHARDS);
    }
};

void AddSC_boss_huntsman_altimor()
{
    RegisterCastleNathriaCreatureAI(boss_huntsman_altimor);
    RegisterCastleNathriaCreatureAI(npc_altimor_margore);
    RegisterCastleNathriaCreatureAI(npc_altimor_bargast);
    RegisterCastleNathriaCreatureAI(npc_altimor_hecutis);
    RegisterCastleNathriaCreatureAI(npc_altimor_ripped_soul);
    RegisterCastleNathriaCreatureAI(npc_altimor_shade_of_bargast);
    RegisterSpellScript(spell_altimor_preselected_targets);
    RegisterSpellScript(spell_altimor_sinseeker_bolt);
    RegisterSpellScript(spell_altimor_vicious_lunge_landing);
    RegisterSpellScript(spell_altimor_shades);
    RegisterSpellScript(spell_altimor_destabilize);
    RegisterSpellScript(spell_altimor_ripped_soul_health_check);
    RegisterSpellScript(spell_altimor_petrifying_howl);
    RegisterSpellScript(spell_altimor_hecutis_shed);
    RegisterAreaTriggerAI(at_altimor_petrifying_howl);
}
