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
 * Maldraxxus - Theater of Pain arena ("ladder boss" world event, map 2222, area 11465).
 *
 * One controller (Arena Center Stalker) owns the whole sequence; the bosses, Deathshades and challengers only react to it.
 * Everything below is taken from the 2026-10-02 20-50-24 sniff (Xantuth fight at 21:07:51 - 21:08:35, Urtz release at
 * 21:12:39 - 21:13:12) unless a comment says otherwise.
 *
 * Cycle: Idle -> variant (Urtz / Xantuth, alternating) -> fight -> boss dies -> Vole victory yell -> Idle.
 * If the boss evades, the event is cancelled and the boss goes back into its cage.
 *
 *   Urtz variant (t = Vole appears)
 *     0.0s   Vole appears above the arena
 *     4.2s   Urtz leaves his cage and flies to the arena centre (22.1s flight + 1.3s drop)
 *     4.3s   Vole: "Raging earth and magma rise ..."  (group 1)
 *     11.6s  Vole: "Seething with the desire to be free ..." (group 2)
 *     17.6s  Vole flies away (12s)
 *     28.5s  Urtz lands (jump emotes), becomes hostile ~3.6s later; the challengers engage him
 *
 *   Xantuth variant (t = release)
 *     0.0s   Xantuth leaves his cage and takes the arena centre
 *     1.9s   Ping, Deathshades warning (+0.6s), Deathshades (+1.9s later), wave appears ~4.1s after the cast
 *     the spell timings repeat while he is alive (see the EVENT_XANTUTH_* schedule)
 *
 * Not sniffed (so not scripted): Azmogal (162873) and Mistress Dyrax (162880) variants, how Xantuth gets from his cage to the
 * arena, what the challengers do between fights, and the exact length of the idle gap (4m45s between the two sniffed fights).
 */

#include "Containers.h"
#include "Creature.h"
#include "CreatureAI.h"
#include "CreatureAIImpl.h"
#include "EventMap.h"
#include "GridNotifiers.h"
#include "Map.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "ScriptedCreature.h"
#include "ScriptMgr.h"
#include "SpellAuras.h"
#include "TemporarySummon.h"

namespace TheaterOfPainArena
{
    enum Creatures
    {
        NPC_ARENA_CENTER_STALKER    = 172656,
        NPC_GRANDMASTER_VOLE        = 162899,
        NPC_UNBREAKABLE_URTZ        = 162853,
        NPC_XANTUTH_THE_BLIGHTED    = 162872,
        NPC_DEATHSHADE_OF_XANTUTH_A = 172591,
        NPC_DEATHSHADE_OF_XANTUTH_B = 172609,
        NPC_ARENA_CHALLENGER_1      = 161757,
        NPC_ARENA_CHALLENGER_2      = 163030,
        NPC_ARENA_CHALLENGER_3      = 163032
    };

    enum Spells
    {
        SPELL_CONTAINMENT_CELL_URTZ     = 317070,   // Cage visual on Urtz
        SPELL_CONTAINMENT_CELL_XANTUTH  = 317096,   // Cage visual on Xantuth
        SPELL_PING_LADDER_BOSS          = 317184,   // Aura on the boss while he is a ladder opponent
        SPELL_PING_LADDER_BOSS_CAST     = 317187,
        SPELL_DEATHSHADES_WARNING       = 346430,
        SPELL_TAIL_SWEEP                = 336910,
        SPELL_NECROSIS                  = 337982,
        SPELL_DEATHSHADE_VISUAL         = 321276,   // "Summon Skeleton", aura on both Deathshades
        SPELL_DEATHSHADE_VOLLEY         = 337072,
        SPELL_NECROTIC_BREATH           = 337074,
        SPELL_PING_VOLE                 = 345529
    };

    enum Texts
    {
        // Grandmaster Vole
        SAY_VOLE_VICTORY            = 0,
        SAY_VOLE_URTZ_INTRO_1       = 1,
        SAY_VOLE_URTZ_INTRO_2       = 2,

        // Xantuth the Blighted
        SAY_XANTUTH_DEATHSHADES     = 0
    };

    enum Paths
    {
        PATH_URTZ_RELEASE           = 760004854,
        PATH_DEATHSHADE_A           = 760005325,
        PATH_DEATHSHADE_B           = 760005335
    };

    enum Actions
    {
        ACTION_RELEASE_BOSS         = 1,    // controller -> boss
        ACTION_BOSS_DIED            = 2,    // boss -> controller
        ACTION_BOSS_EVADED          = 3     // boss -> controller
    };

    enum Events
    {
        // Controller
        EVENT_START_NEXT            = 1,
        EVENT_VOLE_APPEAR,
        EVENT_URTZ_RELEASE,
        EVENT_VOLE_INTRO_1,
        EVENT_VOLE_INTRO_2,
        EVENT_VOLE_LEAVE,
        EVENT_URTZ_LAND,
        EVENT_BOSS_ENGAGE,
        EVENT_XANTUTH_RELEASE,
        EVENT_VOLE_VICTORY,
        EVENT_VOLE_DESPAWN,
        EVENT_CHALLENGERS_HOME,

        // Xantuth (relative to JustEngagedWith, sniff 21:07:53.5 = +0)
        EVENT_XANTUTH_PING,
        EVENT_XANTUTH_WARNING,
        EVENT_XANTUTH_DEATHSHADES,
        EVENT_XANTUTH_TAIL_SWEEP,
        EVENT_XANTUTH_NECROSIS,

        // Deathshade
        EVENT_DEATHSHADE_VOLLEY,
        EVENT_DEATHSHADE_BREATH
    };

    Position const VolePerch(2836.41f, -2655.63f, 3308.96f, 1.475f);
    Position const VoleExit(2743.7f, -2679.2f, 3334.5f);
    Position const XantuthArenaSpot(2830.7f, -2543.51f, 3259.67f, 2.97f);

    // The idle gap is only roughly known (4m45s between the two sniffed fights)
    Seconds constexpr FirstEventDelay   = 60s;
    Seconds constexpr EventIdleDelay    = 285s;
    float constexpr ArenaSearchRange    = 160.0f;

    uint32 constexpr ChallengerEntries[] = { NPC_ARENA_CHALLENGER_1, NPC_ARENA_CHALLENGER_2, NPC_ARENA_CHALLENGER_3 };

    Creature* FindArenaController(Creature* me)
    {
        return me->FindNearestCreature(NPC_ARENA_CENTER_STALKER, 250.0f, true);
    }
}

using namespace TheaterOfPainArena;

// 172656 - Arena Center Stalker, the event controller
struct npc_top_arena_center_stalker : public ScriptedAI
{
    npc_top_arena_center_stalker(Creature* creature) : ScriptedAI(creature), _summons(creature), _bossEntry(0), _urtzNext(true), _eventActive(false) { }

    void JustAppeared() override
    {
        _events.ScheduleEvent(EVENT_START_NEXT, FirstEventDelay);
    }

    void DoAction(int32 action) override
    {
        switch (action)
        {
            case ACTION_BOSS_DIED:
                // Sniffed: Vole yells "Extraordinary!" 5.7s after the kill
                _events.CancelEvent(EVENT_BOSS_ENGAGE);
                if (Creature* vole = SummonVole())
                    vole->CastSpell(vole, SPELL_PING_VOLE, TRIGGERED_FULL_MASK);
                _events.ScheduleEvent(EVENT_VOLE_VICTORY, 5s + 700ms);
                break;
            case ACTION_BOSS_EVADED:
                EndEvent(EventIdleDelay);
                break;
            default:
                break;
        }
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);

        while (uint32 eventId = _events.ExecuteEvent())
        {
            switch (eventId)
            {
                case EVENT_START_NEXT:
                    StartEvent();
                    break;
                case EVENT_VOLE_APPEAR:
                    SummonVole();
                    break;
                case EVENT_URTZ_RELEASE:
                    ReleaseUrtz();
                    break;
                case EVENT_VOLE_INTRO_1:
                    if (Creature* vole = GetVole())
                        vole->AI()->Talk(SAY_VOLE_URTZ_INTRO_1);
                    break;
                case EVENT_VOLE_INTRO_2:
                    if (Creature* vole = GetVole())
                        vole->AI()->Talk(SAY_VOLE_URTZ_INTRO_2);
                    break;
                case EVENT_VOLE_LEAVE:
                    if (Creature* vole = GetVole())
                    {
                        vole->GetMotionMaster()->MovePoint(0, VoleExit, false);
                        vole->DespawnOrUnsummon(12s + 100ms);
                    }
                    break;
                case EVENT_URTZ_LAND:
                    if (Creature* urtz = GetBoss(NPC_UNBREAKABLE_URTZ))
                    {
                        urtz->SetHover(false);
                        urtz->SetDisableGravity(false);
                        urtz->SetCanFly(false);
                        urtz->HandleEmoteCommand(EMOTE_ONESHOT_JUMPSTART);
                    }
                    break;
                case EVENT_BOSS_ENGAGE:
                    EngageChallengers();
                    break;
                case EVENT_XANTUTH_RELEASE:
                    ReleaseXantuth();
                    break;
                case EVENT_VOLE_VICTORY:
                    if (Creature* vole = GetVole())
                        vole->AI()->Talk(SAY_VOLE_VICTORY);
                    _events.ScheduleEvent(EVENT_VOLE_DESPAWN, 10s);
                    break;
                case EVENT_VOLE_DESPAWN:
                    EndEvent(EventIdleDelay);
                    break;
                case EVENT_CHALLENGERS_HOME:
                    SendChallengersHome();
                    break;
                default:
                    break;
            }
        }
    }

private:
    void StartEvent()
    {
        if (_eventActive)
            return;

        // Nobody around to watch: try again later instead of burning a boss
        if (!HasPlayersNearby())
        {
            _events.ScheduleEvent(EVENT_START_NEXT, 30s);
            return;
        }

        _eventActive = true;
        SendChallengersToArena();

        _bossEntry = _urtzNext ? NPC_UNBREAKABLE_URTZ : NPC_XANTUTH_THE_BLIGHTED;

        if (_urtzNext)
        {
            _events.ScheduleEvent(EVENT_VOLE_APPEAR, 0s);
            _events.ScheduleEvent(EVENT_URTZ_RELEASE, 4s + 200ms);
            _events.ScheduleEvent(EVENT_VOLE_INTRO_1, 4s + 300ms);
            _events.ScheduleEvent(EVENT_VOLE_INTRO_2, 11s + 600ms);
            _events.ScheduleEvent(EVENT_VOLE_LEAVE, 17s + 600ms);
            // 22.13s flight + 1.3s drop starting at 4.2s, hostile ~3.6s after the landing
            _events.ScheduleEvent(EVENT_URTZ_LAND, 27s + 600ms);
            _events.ScheduleEvent(EVENT_BOSS_ENGAGE, 32s);
        }
        else
        {
            _events.ScheduleEvent(EVENT_XANTUTH_RELEASE, 0s);
            _events.ScheduleEvent(EVENT_BOSS_ENGAGE, 1s + 900ms);
        }

        _urtzNext = !_urtzNext;
    }

    void EndEvent(Milliseconds nextDelay)
    {
        _events.Reset();
        _summons.DespawnAll();
        _eventActive = false;

        // Whoever survived walks back to where they came from
        _events.ScheduleEvent(EVENT_CHALLENGERS_HOME, 2s);
        _events.ScheduleEvent(EVENT_START_NEXT, nextDelay);
    }

    bool HasPlayersNearby() const
    {
        std::list<Player*> players;
        me->GetPlayerListInGrid(players, ArenaSearchRange);
        return !players.empty();
    }

    Creature* SummonVole()
    {
        if (Creature* existing = GetVole())
            return existing;

        TempSummon* vole = me->SummonCreature(NPC_GRANDMASTER_VOLE, VolePerch, TEMPSUMMON_MANUAL_DESPAWN);
        if (!vole)
            return nullptr;

        vole->SetDisableGravity(true);
        vole->SetCanFly(true);
        _summons.Summon(vole);
        _voleGuid = vole->GetGUID();
        return vole;
    }

    Creature* GetVole() const
    {
        return ObjectAccessor::GetCreature(*me, _voleGuid);
    }

    Creature* GetBoss(uint32 entry) const
    {
        return me->FindNearestCreature(entry, 250.0f, true);
    }

    void ReleaseUrtz()
    {
        Creature* urtz = GetBoss(NPC_UNBREAKABLE_URTZ);
        if (!urtz)
        {
            EndEvent(EventIdleDelay);
            return;
        }

        urtz->AI()->DoAction(ACTION_RELEASE_BOSS);
        urtz->SetDisableGravity(true);
        urtz->SetCanFly(true);
        urtz->SetHover(true);
        // 22.13s sniffed flight over the first 7 nodes, plus the 1.3s drop of the last node
        urtz->GetMotionMaster()->MovePath(PATH_URTZ_RELEASE, false, 23s + 430ms, {}, MovementWalkRunSpeedSelectionMode::Default, {}, {}, {}, {}, false);
    }

    void ReleaseXantuth()
    {
        Creature* xantuth = GetBoss(NPC_XANTUTH_THE_BLIGHTED);
        if (!xantuth)
        {
            EndEvent(EventIdleDelay);
            return;
        }

        xantuth->AI()->DoAction(ACTION_RELEASE_BOSS);
        // Unsniffed: how he gets from the cage to the arena
        xantuth->NearTeleportTo(XantuthArenaSpot);
    }

    void EngageChallengers()
    {
        Creature* boss = GetBoss(_bossEntry);
        if (!boss)
            return;

        for (Creature* challenger : GetChallengers())
        {
            challenger->EngageWithTarget(boss);
            boss->EngageWithTarget(challenger);
        }
    }

    std::vector<Creature*> GetChallengers() const
    {
        std::vector<Creature*> result;
        for (uint32 entry : ChallengerEntries)
        {
            std::list<Creature*> list;
            me->GetCreatureListWithEntryInGrid(list, entry, ArenaSearchRange);
            for (Creature* creature : list)
                if (creature->IsAlive())
                    result.push_back(creature);
        }
        return result;
    }

    // Challengers with a sniffed path walk it once when an event starts (the path leads into the arena)
    void SendChallengersToArena()
    {
        for (Creature* challenger : GetChallengers())
            if (uint32 pathId = challenger->GetWaypointPathId())
                challenger->GetMotionMaster()->MovePath(pathId, false);
    }

    void SendChallengersHome()
    {
        for (Creature* challenger : GetChallengers())
            if (!challenger->IsInCombat() && challenger->GetWaypointPathId())
                challenger->GetMotionMaster()->MoveTargetedHome();
    }

    EventMap _events;
    SummonList _summons;
    ObjectGuid _voleGuid;
    uint32 _bossEntry;
    bool _urtzNext;
    bool _eventActive;
};

// Common behaviour of the ladder bosses: caged until released, report death/evade to the controller, respawn into the cage
struct npc_top_arena_ladder_boss_base : public ScriptedAI
{
    npc_top_arena_ladder_boss_base(Creature* creature, uint32 cageSpell) : ScriptedAI(creature), _cageSpell(cageSpell) { }

    void JustAppeared() override
    {
        me->SetImmuneToAll(true);
        me->SetUninteractible(true);
        DoCastSelf(_cageSpell, TRIGGERED_FULL_MASK);
        me->RemoveAurasDueToSpell(SPELL_PING_LADDER_BOSS);
    }

    void DoAction(int32 action) override
    {
        if (action != ACTION_RELEASE_BOSS)
            return;

        me->RemoveAurasDueToSpell(_cageSpell);
        me->SetUninteractible(false);
        me->SetImmuneToAll(false);
    }

    void JustDied(Unit* /*killer*/) override
    {
        if (Creature* controller = FindArenaController(me))
            controller->AI()->DoAction(ACTION_BOSS_DIED);

        // Back in the cage for the next cycle
        me->DespawnOrUnsummon(8s, 20s);
    }

    void EnterEvadeMode(EvadeReason /*why*/) override
    {
        if (Creature* controller = FindArenaController(me))
            controller->AI()->DoAction(ACTION_BOSS_EVADED);

        me->DespawnOrUnsummon(0s, 5s);
    }

protected:
    uint32 const _cageSpell;
};

// 162853 - Unbreakable Urtz
// Only the pings are sniffed for him (the fight itself was not recorded), so he is a plain melee boss.
struct boss_top_unbreakable_urtz : public npc_top_arena_ladder_boss_base
{
    boss_top_unbreakable_urtz(Creature* creature) : npc_top_arena_ladder_boss_base(creature, SPELL_CONTAINMENT_CELL_URTZ) { }

    void JustEngagedWith(Unit* /*who*/) override
    {
        // Sniffed on landing: ping aura + ping cast
        DoCastSelf(SPELL_PING_LADDER_BOSS, TRIGGERED_FULL_MASK);
        DoCastSelf(SPELL_PING_LADDER_BOSS_CAST, TRIGGERED_FULL_MASK);
    }

    void UpdateAI(uint32 /*diff*/) override
    {
        if (!UpdateVictim())
            return;

    }
};

// 162872 - Xantuth the Blighted
struct boss_top_xantuth_the_blighted : public npc_top_arena_ladder_boss_base
{
    boss_top_xantuth_the_blighted(Creature* creature) : npc_top_arena_ladder_boss_base(creature, SPELL_CONTAINMENT_CELL_XANTUTH), _waves(0) { }

    void JustAppeared() override
    {
        npc_top_arena_ladder_boss_base::JustAppeared();
        _waves = 0;
        _events.Reset();
    }

    void DoAction(int32 action) override
    {
        npc_top_arena_ladder_boss_base::DoAction(action);

        if (action == ACTION_RELEASE_BOSS)
            DoCastSelf(SPELL_PING_LADDER_BOSS, TRIGGERED_FULL_MASK);
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        // Offsets from the first ping (21:07:53.5); the second round of each spell is 20-22s later
        _events.ScheduleEvent(EVENT_XANTUTH_PING, 0s);
        _events.ScheduleEvent(EVENT_XANTUTH_WARNING, 600ms);
        _events.ScheduleEvent(EVENT_XANTUTH_TAIL_SWEEP, 8s + 900ms);
        _events.ScheduleEvent(EVENT_XANTUTH_NECROSIS, 18s + 900ms);
    }

    void EnterEvadeMode(EvadeReason why) override
    {
        _events.Reset();
        npc_top_arena_ladder_boss_base::EnterEvadeMode(why);
    }

    void JustDied(Unit* killer) override
    {
        _events.Reset();
        npc_top_arena_ladder_boss_base::JustDied(killer);
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
            switch (eventId)
            {
                case EVENT_XANTUTH_PING:
                    DoCastSelf(SPELL_PING_LADDER_BOSS_CAST);
                    _events.Repeat(20s);
                    break;
                case EVENT_XANTUTH_WARNING:
                    DoCastSelf(SPELL_DEATHSHADES_WARNING);
                    // Second and later warnings also announce the wave in chat
                    if (_waves > 0)
                        Talk(SAY_XANTUTH_DEATHSHADES);
                    _events.ScheduleEvent(EVENT_XANTUTH_DEATHSHADES, 1s + 900ms);
                    _events.Repeat(22s);
                    break;
                case EVENT_XANTUTH_DEATHSHADES:
                    // The Deathshades appear ~4.1s after the cast. The wave is summoned by hand rather than through
                    // 337131 so that it cannot double up if the spell carries its own summon effect.
                    ++_waves;
                    me->m_Events.AddEventAtOffset([this]() { SummonDeathshades(); }, 4s + 100ms);
                    break;
                case EVENT_XANTUTH_TAIL_SWEEP:
                    DoCastAOE(SPELL_TAIL_SWEEP);
                    _events.Repeat(22s + 250ms);
                    break;
                case EVENT_XANTUTH_NECROSIS:
                    DoCastSelf(SPELL_NECROSIS);
                    _events.Repeat(22s);
                    break;
                default:
                    break;
            }

            if (me->HasUnitState(UNIT_STATE_CASTING))
                return;
        }

    }

private:
    void SummonDeathshades()
    {
        if (!me->IsAlive())
            return;

        // Both Deathshades are destroyed ~12s after they appear
        for (uint32 entry : { NPC_DEATHSHADE_OF_XANTUTH_A, NPC_DEATHSHADE_OF_XANTUTH_B })
        {
            WorldLocation const& start = entry == NPC_DEATHSHADE_OF_XANTUTH_A ? DeathshadeStartA : DeathshadeStartB;
            me->SummonCreature(entry, start, TEMPSUMMON_TIMED_DESPAWN, 13s);
        }
    }

    // First node of paths 760005325 / 760005335
    static inline WorldLocation const DeathshadeStartA = WorldLocation(2222, 2877.6f, -2681.3f, 3313.6f, 1.82f);
    static inline WorldLocation const DeathshadeStartB = WorldLocation(2222, 2762.2f, -2751.6f, 3358.3f, 1.29f);

    EventMap _events;
    uint8 _waves;
};

// 172591, 172609 - Deathshade of Xantuth: flies its path across the arena and shoots at players on the way
struct npc_top_xantuth_deathshade : public ScriptedAI
{
    npc_top_xantuth_deathshade(Creature* creature) : ScriptedAI(creature) { }

    void JustAppeared() override
    {
        me->SetDisableGravity(true);
        me->SetCanFly(true);
        me->SetHover(true);
        DoCastSelf(SPELL_DEATHSHADE_VISUAL, TRIGGERED_FULL_MASK);

        uint32 const pathId = me->GetEntry() == NPC_DEATHSHADE_OF_XANTUTH_A ? PATH_DEATHSHADE_A : PATH_DEATHSHADE_B;
        me->GetMotionMaster()->MovePath(pathId, false, {}, {}, MovementWalkRunSpeedSelectionMode::Default, {}, {}, {}, {}, false);

        // Necrotic Breath / Volley were sniffed in the first 2s; later timings are estimates
        _events.ScheduleEvent(EVENT_DEATHSHADE_BREATH, 1s);
        _events.ScheduleEvent(EVENT_DEATHSHADE_VOLLEY, 2s);
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);

        while (uint32 eventId = _events.ExecuteEvent())
        {
            Unit* target = RandomPlayer();
            switch (eventId)
            {
                case EVENT_DEATHSHADE_BREATH:
                    if (target)
                        DoCast(target, SPELL_NECROTIC_BREATH);
                    break;
                case EVENT_DEATHSHADE_VOLLEY:
                    if (target)
                        DoCast(target, SPELL_DEATHSHADE_VOLLEY);
                    _events.Repeat(3s);
                    break;
                default:
                    break;
            }
        }
    }

private:
    Unit* RandomPlayer() const
    {
        std::list<Player*> players;
        me->GetPlayerListInGrid(players, 80.0f);
        if (players.empty())
            return nullptr;

        return Trinity::Containers::SelectRandomContainerElement(players);
    }

    EventMap _events;
};

// Arena Challengers: plain melee fighters once the controller engages them with the boss
struct npc_top_arena_challenger : public ScriptedAI
{
    npc_top_arena_challenger(Creature* creature) : ScriptedAI(creature) { }

    void UpdateAI(uint32 /*diff*/) override
    {
        if (!UpdateVictim())
            return;

    }
};

void AddSC_theater_of_pain_arena()
{
    RegisterCreatureAI(npc_top_arena_center_stalker);
    RegisterCreatureAI(boss_top_unbreakable_urtz);
    RegisterCreatureAI(boss_top_xantuth_the_blighted);
    RegisterCreatureAI(npc_top_xantuth_deathshade);
    RegisterCreatureAI(npc_top_arena_challenger);
}
