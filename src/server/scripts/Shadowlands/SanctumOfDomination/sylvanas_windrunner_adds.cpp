/*
 * Sylvanas Windrunner - Mawsworn / Mawforged adds (map 2450)
 *
 * Ability lists come from the Wowhead NPC pages (per-difficulty modes), the repeat gaps from Warcraft Logs (median of the 8 best pulls per difficulty,
 * see trinitycore-tracking/sod-encounters/sylvanas_encounter_model.md "Adds").  The logs give the gap between casts and the first cast counted from the
 * pull of the boss, not from the moment the add engages, so the first cast is set to a fraction of the gap instead of the logged absolute time.
 * Spell targets follow the DB2 implicit targets (self / enemy target / any unit => random player).
 *
 * Only the spells that appear in the logs are cast.  Difficulty tables are indexed Normal, Heroic, Mythic (LFR uses the Normal column).
 * Cast intervals of 0 mean "not cast on that difficulty".
 * Hymn of the End (358215, Heroic, all adds, 277 s) is the boss-phase transition and is left to the boss script.
 */

#include "ScriptMgr.h"
#include "InstanceScript.h"
#include "ScriptedCreature.h"
#include "SpellInfo.h"
#include "sanctum_of_domination.h"

namespace SylvanasAdds
{
    enum Entries
    {
        NPC_MAWSWORN_VANGUARD   = 177154,
        NPC_MAWSWORN_HOPEBREAKER = 177787,
        NPC_MAWFORGED_SOULJUDGE = 177889,
        NPC_MAWFORGED_SUMMONER  = 177891,
        NPC_MAWFORGED_GOLIATH   = 177892,
        NPC_MAWFORGED_COLOSSUS  = 177893,
        NPC_DECREPIT_ORB        = 178008,
        NPC_TERROR_ORB          = 179963,
        NPC_THE_JAILER_ADD      = 178079
    };

    enum Spells
    {
        // Mawsworn Vanguard
        SPELL_ACCURSED_MIGHT        = 350865,
        SPELL_UNSTOPPABLE_FORCE     = 351075,
        // Mawsworn Hopebreaker
        SPELL_DESTABILIZE           = 351091,
        SPELL_ENFLAME               = 351109,
        // Mawforged Souljudge
        SPELL_CRUSHING_DREAD        = 351117,
        SPELL_LASHING_STRIKE        = 351178,
        SPELL_AURA_OF_CONTEMPT      = 355826,
        // Mawforged Summoner
        SPELL_SUMMON_DECREPIT_ORBS  = 351353,
        SPELL_CURSE_OF_LETHARGY     = 351939,
        SPELL_DARK_COMMUNION        = 356021,
        // Mawforged Goliath
        SPELL_AURA_OF_VIOLENCE      = 355841,
        // Mawforged Colossus
        SPELL_EXPULSION             = 351562,
        SPELL_FILTH                 = 351589,
        // The Jailer (Mythic)
        SPELL_MERCILESS             = 358588,
        // Orbs
        SPELL_DECREPIT_ORB          = 351317,
        SPELL_DETONATION            = 351323,
        SPELL_TERROR_ORB            = 356023
    };

    enum Targeting : uint8
    {
        TARGET_SELF,
        TARGET_VICTIM,
        TARGET_RANDOM_PLAYER
    };

    struct AddAbility
    {
        uint32 SpellId;
        Targeting Target;
        uint32 GapMs[3];    // Normal, Heroic, Mythic (0 = not cast)
    };

    uint8 DifficultyColumn(Difficulty difficulty)
    {
        switch (difficulty)
        {
            case DIFFICULTY_HEROIC_RAID: return 1;
            case DIFFICULTY_MYTHIC_RAID: return 2;
            default:                     return 0;
        }
    }

    // Plain data-driven combat AI: every ability is an independent repeating timer.
    struct npc_sylvanas_add_base : public ScriptedAI
    {
        npc_sylvanas_add_base(Creature* creature, AddAbility const* abilities, size_t count) : ScriptedAI(creature), _abilities(abilities), _count(count) { }

        void JustEngagedWith(Unit* who) override
        {
            ScriptedAI::JustEngagedWith(who);
            uint8 column = DifficultyColumn(me->GetMap()->GetDifficultyID());
            for (size_t i = 0; i < _count; ++i)
            {
                uint32 gap = _abilities[i].GapMs[column];
                if (!gap)
                    continue;
                // first cast: a quarter of the gap (min 3s) with a little spread so a pack does not cast in the same tick
                uint32 first = std::max<uint32>(3000, gap / 4) + urand(0, 1500);
                events.ScheduleEvent(uint32(i + 1), Milliseconds(first));
            }
        }

        void Reset() override
        {
            events.Reset();
        }

        void UpdateAI(uint32 diff) override
        {
            if (!UpdateVictim())
                return;

            events.Update(diff);

            if (me->HasUnitState(UNIT_STATE_CASTING))
                return;

            if (uint32 eventId = events.ExecuteEvent())
            {
                AddAbility const& ability = _abilities[eventId - 1];
                uint8 column = DifficultyColumn(me->GetMap()->GetDifficultyID());
                Unit* target = nullptr;
                switch (ability.Target)
                {
                    case TARGET_SELF:          target = me; break;
                    case TARGET_VICTIM:        target = me->GetVictim(); break;
                    case TARGET_RANDOM_PLAYER: target = SelectTarget(SelectTargetMethod::Random, 0, 100.0f, true); break;
                }
                if (target)
                    DoCast(target, ability.SpellId);
                events.Repeat(Milliseconds(ability.GapMs[column]));
                return;
            }

        }

    private:
        EventMap events;
        AddAbility const* _abilities;
        size_t _count;
    };

    // gaps in ms, from the WCL medians (Normal / Heroic / Mythic)
    constexpr AddAbility VanguardAbilities[] =
    {
        { SPELL_ACCURSED_MIGHT,    TARGET_VICTIM,        { 3700, 5000, 5600 } },
        { SPELL_UNSTOPPABLE_FORCE, TARGET_RANDOM_PLAYER, { 0,    4900, 5700 } }
    };
    constexpr AddAbility HopebreakerAbilities[] =
    {
        { SPELL_DESTABILIZE, TARGET_RANDOM_PLAYER, { 8500, 9800, 7000 } },
        { SPELL_ENFLAME,     TARGET_SELF,          { 0,    12200, 41200 } }
    };
    constexpr AddAbility SouljudgeAbilities[] =
    {
        { SPELL_CRUSHING_DREAD,   TARGET_SELF,   { 0,   57200, 97600 } },
        { SPELL_LASHING_STRIKE,   TARGET_VICTIM, { 0,   7800,  7300 } },
        { SPELL_AURA_OF_CONTEMPT, TARGET_SELF,   { 0,   0,     98500 } }
    };
    constexpr AddAbility SummonerAbilities[] =
    {
        { SPELL_SUMMON_DECREPIT_ORBS, TARGET_SELF, { 0,    29500, 0 } },
        { SPELL_CURSE_OF_LETHARGY,    TARGET_SELF, { 0,    7300,  7300 } },
        { SPELL_DARK_COMMUNION,       TARGET_SELF, { 0,    0,     98300 } }
    };
    constexpr AddAbility GoliathAbilities[] =
    {
        { SPELL_AURA_OF_VIOLENCE, TARGET_SELF, { 0, 0, 30000 } }
    };
    constexpr AddAbility ColossusAbilities[] =
    {
        { SPELL_EXPULSION, TARGET_SELF,          { 0, 0, 15800 } },
        { SPELL_FILTH,     TARGET_RANDOM_PLAYER, { 0, 0, 13200 } }
    };
    constexpr AddAbility JailerAddAbilities[] =
    {
        { SPELL_MERCILESS, TARGET_SELF, { 0, 0, 20900 } }
    };

    template <typename T, size_t N>
    constexpr size_t Count(T const (&)[N]) { return N; }
}

struct npc_sylvanas_mawsworn_vanguard : public SylvanasAdds::npc_sylvanas_add_base
{
    npc_sylvanas_mawsworn_vanguard(Creature* c) : npc_sylvanas_add_base(c, SylvanasAdds::VanguardAbilities, SylvanasAdds::Count(SylvanasAdds::VanguardAbilities)) { }
};

struct npc_sylvanas_mawsworn_hopebreaker : public SylvanasAdds::npc_sylvanas_add_base
{
    npc_sylvanas_mawsworn_hopebreaker(Creature* c) : npc_sylvanas_add_base(c, SylvanasAdds::HopebreakerAbilities, SylvanasAdds::Count(SylvanasAdds::HopebreakerAbilities)) { }
};

struct npc_sylvanas_mawforged_souljudge : public SylvanasAdds::npc_sylvanas_add_base
{
    npc_sylvanas_mawforged_souljudge(Creature* c) : npc_sylvanas_add_base(c, SylvanasAdds::SouljudgeAbilities, SylvanasAdds::Count(SylvanasAdds::SouljudgeAbilities)) { }
};

struct npc_sylvanas_mawforged_summoner : public SylvanasAdds::npc_sylvanas_add_base
{
    npc_sylvanas_mawforged_summoner(Creature* c) : npc_sylvanas_add_base(c, SylvanasAdds::SummonerAbilities, SylvanasAdds::Count(SylvanasAdds::SummonerAbilities)) { }
};

struct npc_sylvanas_mawforged_goliath : public SylvanasAdds::npc_sylvanas_add_base
{
    npc_sylvanas_mawforged_goliath(Creature* c) : npc_sylvanas_add_base(c, SylvanasAdds::GoliathAbilities, SylvanasAdds::Count(SylvanasAdds::GoliathAbilities)) { }
};

struct npc_sylvanas_mawforged_colossus : public SylvanasAdds::npc_sylvanas_add_base
{
    npc_sylvanas_mawforged_colossus(Creature* c) : npc_sylvanas_add_base(c, SylvanasAdds::ColossusAbilities, SylvanasAdds::Count(SylvanasAdds::ColossusAbilities)) { }
};

struct npc_sylvanas_jailer_add : public SylvanasAdds::npc_sylvanas_add_base
{
    npc_sylvanas_jailer_add(Creature* c) : npc_sylvanas_add_base(c, SylvanasAdds::JailerAddAbilities, SylvanasAdds::Count(SylvanasAdds::JailerAddAbilities)) { }
};

// Decrepit Orb / Terror Orb: slow floating bombs summoned by the Summoner.  They never melee; Decrepit Orb detonates when a player gets close
// (Detonation 351323), the Terror Orb applies Terror Orb (356023) on contact.
struct npc_sylvanas_decrepit_orb : public ScriptedAI
{
    npc_sylvanas_decrepit_orb(Creature* creature) : ScriptedAI(creature), _detonated(false) { }

    void JustAppeared() override
    {
        me->SetReactState(REACT_PASSIVE);
        DoCastSelf(SylvanasAdds::SPELL_DECREPIT_ORB);
        _checkTimer = 500;
    }

    void UpdateAI(uint32 diff) override
    {
        if (_detonated)
            return;
        if (_checkTimer > diff)
        {
            _checkTimer -= diff;
            return;
        }
        _checkTimer = 500;
        if (me->SelectNearestPlayer(4.0f))
        {
            _detonated = true;
            DoCastSelf(SylvanasAdds::SPELL_DETONATION, true);
            me->DespawnOrUnsummon(1s);
        }
    }

private:
    uint32 _checkTimer = 500;
    bool _detonated;
};

struct npc_sylvanas_terror_orb : public ScriptedAI
{
    npc_sylvanas_terror_orb(Creature* creature) : ScriptedAI(creature), _triggered(false) { }

    void JustAppeared() override
    {
        me->SetReactState(REACT_PASSIVE);
        _checkTimer = 500;
    }

    void UpdateAI(uint32 diff) override
    {
        if (_triggered)
            return;
        if (_checkTimer > diff)
        {
            _checkTimer -= diff;
            return;
        }
        _checkTimer = 500;
        if (Player* player = me->SelectNearestPlayer(4.0f))
        {
            _triggered = true;
            DoCast(player, SylvanasAdds::SPELL_TERROR_ORB, true);
            me->DespawnOrUnsummon(1s);
        }
    }

private:
    uint32 _checkTimer = 500;
    bool _triggered;
};

void AddSC_sylvanas_windrunner_adds()
{
    RegisterSanctumOfDominationCreatureAI(npc_sylvanas_mawsworn_vanguard);
    RegisterSanctumOfDominationCreatureAI(npc_sylvanas_mawsworn_hopebreaker);
    RegisterSanctumOfDominationCreatureAI(npc_sylvanas_mawforged_souljudge);
    RegisterSanctumOfDominationCreatureAI(npc_sylvanas_mawforged_summoner);
    RegisterSanctumOfDominationCreatureAI(npc_sylvanas_mawforged_goliath);
    RegisterSanctumOfDominationCreatureAI(npc_sylvanas_mawforged_colossus);
    RegisterSanctumOfDominationCreatureAI(npc_sylvanas_jailer_add);
    RegisterSanctumOfDominationCreatureAI(npc_sylvanas_decrepit_orb);
    RegisterSanctumOfDominationCreatureAI(npc_sylvanas_terror_orb);
}
