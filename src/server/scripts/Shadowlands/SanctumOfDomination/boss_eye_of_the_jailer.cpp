/*
 * The Eye of the Jailer (DungeonEncounter 2433, boss entry 175725)
 *
 * Sources: DBM (DeadlyBossMods/DBM-Shadowlands, SanctumOfDomination/TheEyeoftheJailer.lua, copy in
 * trinitycore-tracking/sod-encounters/dbm_ref/; mod:SetCreatureID(175725)) for the timers and the stage logic; the Adventure
 * Guide journal text (sod-encounters/the_eye_of_the_jailer.md) for the 66%/33% thresholds; the 2026-10-01 12.1.0.69933 sniff
 * (one 59s Normal 10-player pull seen only through a Deathseeker Eye, entry 176531) for the Deathseeker Eye's spells.
 * The sniffed room eye (entry 180018, destroyed before the pull) is a pre-fight placeholder, not the boss.
 *
 * Structure:
 *  - Stage One: the Eye fights the platform directly (Deathlink on the tank, Fracture Soul, Piercing Lens spam, Annihilating Glare,
 *    Mythic: Hopeless Lethargy).
 *  - At 66% and again at 33% it casts Stygian Darkshield, unleashes two Deathseeker Eyes and fights from a distance (Stage Two:
 *    Desolation Beam, Titanic Death Gaze, Mythic: Scorn and Ire; the Deathseeker Eyes cast Eye Bolt, Spreading Misery and, unless
 *    LFR, Slothful Corruption).  When both Eyes are dead the shield drops and Stage One timers restart.
 *  - After the second pair dies the Eye is enraged: Immediate Extermination (Stage Three), then the Stage One spells with the
 *    Stage Three starting timers.
 *
 * NOT implemented (see ShadowlandsSoD_GAPS.md): the behaviour of every dummy-effect spell here (Deathlink, Fracture Soul,
 * Spreading Misery, Slothful Corruption, Titanic Death Gaze, Desolation Beam chain, Scorn and Ire, Summon Shackler, Grapple!),
 * the periodic-dummy aura of Immediate Extermination, the Stygian Abductor adds (176521), "dead Eye keeps pulsing Suffering", Eyes
 * that are too close casting Suffering, and the Eye physically retreating in Stage Two (no sniffed path).  Eye summon positions
 * are an assumption (symmetric offsets from the boss).
 */

#include "CreatureAI.h"
#include "CreatureAIImpl.h"
#include "InstanceScript.h"
#include "Map.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "SpellInfo.h"
#include "SpellScript.h"
#include "sanctum_of_domination.h"

enum EyeOfTheJailerSpells
{
    // Eye of the Jailer
    SPELL_DEATHLINK                 = 350828,
    SPELL_FRACTURE_SOUL             = 350022,
    SPELL_PIERCING_LENS             = 350803,
    SPELL_ANNIHILATING_GLARE        = 351413,   // cast; the area aura is 350764
    SPELL_HOPELESS_LETHARGY         = 350604,   // Mythic
    SPELL_STYGIAN_DARKSHIELD        = 348805,
    SPELL_DESOLATION_BEAM           = 350847,
    SPELL_TITANIC_DEATH_GAZE        = 349030,
    SPELL_SCORN_AND_IRE             = 355232,   // Mythic
    SPELL_IMMEDIATE_EXTERMINATION   = 348974,

    // Deathseeker Eye
    SPELL_EYE_BOLT                  = 348054,
    SPELL_SPREADING_MISERY          = 350816,
    SPELL_SLOTHFUL_CORRUPTION       = 351835
};

enum EyeOfTheJailerEvents
{
    EVENT_DEATHLINK = 1,
    EVENT_FRACTURE_SOUL,
    EVENT_PIERCING_LENS,
    EVENT_ANNIHILATING_GLARE,
    EVENT_HOPELESS_LETHARGY,
    EVENT_DESOLATION_BEAM,
    EVENT_TITANIC_DEATH_GAZE,
    EVENT_SCORN_AND_IRE,
    EVENT_IMMEDIATE_EXTERMINATION,

    // Deathseeker Eye
    EVENT_EYE_BOLT,
    EVENT_SPREADING_MISERY,
    EVENT_SLOTHFUL_CORRUPTION
};

enum EyeOfTheJailerPhases
{
    PHASE_STAGE_ONE     = 1,
    PHASE_STAGE_TWO     = 2,
    PHASE_STAGE_THREE   = 3
};

// Journal: "Upon reaching 66%, and 33% health remaining, the Eye of the Jailer unleashes two of its Deathseeker Eyes"
static constexpr int32 AddsPhasePct[] = { 66, 33 };

// DBM timers (Normal/Heroic/Mythic), measured from the stage start
struct StageTimers
{
    Milliseconds Deathlink, FractureSoul, Glare, Lethargy;
};
static constexpr StageTimers Pull       = { 4s + 400ms, 6s + 800ms, 25s + 100ms, 9s + 400ms };   // Lethargy: Mythic only
static constexpr StageTimers AfterAdds  = { 8s + 900ms, 11s,        28s + 600ms, 14s + 100ms };
static constexpr StageTimers StageThree = { 8s + 200ms, 10s + 500ms, 25s,        0s };

static constexpr Milliseconds DeathlinkCd       = 10s + 900ms;
static constexpr Milliseconds FractureSoulCd    = 11s;
static constexpr Milliseconds GlareCd           = 47s + 300ms;
static constexpr Milliseconds LethargyCd        = 47s + 300ms;
static constexpr Milliseconds DesolationBeamCd  = 16s + 200ms;
static constexpr Milliseconds DeathGazeCd       = 32s + 800ms;
static constexpr Milliseconds ScornAndIreCd     = 12s + 100ms;
static constexpr Milliseconds SpreadingMiseryCd = 12s + 100ms;
static constexpr Milliseconds SlothfulCd        = 23s + 800ms;
static constexpr Milliseconds EyeBoltCd         = 4s;          // sniff: median interval 4.0s, first at 5.0s
static constexpr uint8 EyesPerPhase = 2;
static constexpr float EyeSpawnOffset = 8.0f;                  // assumption: no sniffed spawn positions

struct boss_eye_of_the_jailer_sod : public BossAI
{
    boss_eye_of_the_jailer_sod(Creature* creature) : BossAI(creature, DATA_THE_EYE_OF_THE_JAILER), _addsPhasesDone(0), _eyesAlive(0) { }

    void Reset() override
    {
        _Reset();
        _addsPhasesDone = 0;
        _eyesAlive = 0;
        me->SetReactState(REACT_AGGRESSIVE);
        me->RemoveAurasDueToSpell(SPELL_STYGIAN_DARKSHIELD);
    }

    void JustEngagedWith(Unit* who) override
    {
        BossAI::JustEngagedWith(who);
        ScheduleStageOne(Pull);
    }

    bool IsMythic() const { return me->GetMap()->GetDifficultyID() == DIFFICULTY_MYTHIC_RAID; }
    bool IsLFR() const { return me->GetMap()->GetDifficultyID() == DIFFICULTY_LFR_NEW; }

    void DamageTaken(Unit* /*attacker*/, uint32& damage, DamageEffectType /*damageType*/, SpellInfo const* /*spellInfo*/) override
    {
        if (events.IsInPhase(PHASE_STAGE_TWO) || _addsPhasesDone >= std::size(AddsPhasePct))
            return;

        // 66% then 33%: the Eye becomes unattackable behind Stygian Darkshield until both Deathseeker Eyes are dead
        if (me->HealthBelowPctDamaged(AddsPhasePct[_addsPhasesDone], damage))
            StartAddsPhase();
    }

    void JustSummoned(Creature* summon) override
    {
        BossAI::JustSummoned(summon);
        DoZoneInCombat(summon);
    }

    void SummonedCreatureDies(Creature* summon, Unit* /*killer*/) override
    {
        if (summon->GetEntry() != NPC_DEATHSEEKER_EYE || !events.IsInPhase(PHASE_STAGE_TWO))
            return;

        if (_eyesAlive > 0)
            --_eyesAlive;
        if (_eyesAlive == 0)
            EndAddsPhase();
    }

    void JustDied(Unit* /*killer*/) override
    {
        _JustDied();
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim())
            return;

        events.Update(diff);

        if (me->HasUnitState(UNIT_STATE_CASTING))
            return;

        while (uint32 eventId = events.ExecuteEvent())
        {
            ExecuteEvent(eventId);

            if (me->HasUnitState(UNIT_STATE_CASTING))
                return;
        }
    }

    void ExecuteEvent(uint32 eventId) override
    {
        switch (eventId)
        {
            case EVENT_DEATHLINK:
                DoCastVictim(SPELL_DEATHLINK);
                events.Repeat(DeathlinkCd);
                break;
            case EVENT_FRACTURE_SOUL:
                DoCastSelf(SPELL_FRACTURE_SOUL);
                events.Repeat(FractureSoulCd);
                break;
            case EVENT_PIERCING_LENS:
                // DBM: "boss effectively spams this every 1-4 seconds"
                DoCastVictim(SPELL_PIERCING_LENS);
                events.Repeat(1s, 4s);
                break;
            case EVENT_ANNIHILATING_GLARE:
                DoCastSelf(SPELL_ANNIHILATING_GLARE);
                events.Repeat(GlareCd);
                break;
            case EVENT_HOPELESS_LETHARGY:
                DoCastSelf(SPELL_HOPELESS_LETHARGY);
                events.Repeat(LethargyCd);
                break;
            case EVENT_DESOLATION_BEAM:
                if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0, 0.0f, true))
                    DoCast(target, SPELL_DESOLATION_BEAM);
                events.Repeat(DesolationBeamCd);
                break;
            case EVENT_TITANIC_DEATH_GAZE:
                DoCastSelf(SPELL_TITANIC_DEATH_GAZE);
                events.Repeat(DeathGazeCd);
                break;
            case EVENT_SCORN_AND_IRE:
                DoCastSelf(SPELL_SCORN_AND_IRE);
                events.Repeat(ScornAndIreCd);
                break;
            case EVENT_IMMEDIATE_EXTERMINATION:
                DoCastSelf(SPELL_IMMEDIATE_EXTERMINATION);   // 10s cast
                break;
            default:
                break;
        }
    }

private:
    void ScheduleStageOne(StageTimers const& t)
    {
        events.SetPhase(PHASE_STAGE_ONE);
        events.ScheduleEvent(EVENT_DEATHLINK, t.Deathlink, 0, PHASE_STAGE_ONE);
        events.ScheduleEvent(EVENT_FRACTURE_SOUL, t.FractureSoul, 0, PHASE_STAGE_ONE);
        events.ScheduleEvent(EVENT_PIERCING_LENS, 1s, 2s, 0, PHASE_STAGE_ONE);
        events.ScheduleEvent(EVENT_ANNIHILATING_GLARE, t.Glare, 0, PHASE_STAGE_ONE);
        if (IsMythic())
            events.ScheduleEvent(EVENT_HOPELESS_LETHARGY, t.Lethargy, 0, PHASE_STAGE_ONE);
    }

    void StartAddsPhase()
    {
        ++_addsPhasesDone;
        events.Reset();
        events.SetPhase(PHASE_STAGE_TWO);
        DoCastSelf(SPELL_STYGIAN_DARKSHIELD, true);

        for (uint8 i = 0; i < EyesPerPhase; ++i)
        {
            Position pos = me->GetNearPosition(EyeSpawnOffset, i == 0 ? float(M_PI_2) : -float(M_PI_2));
            if (me->SummonCreature(NPC_DEATHSEEKER_EYE, pos, TEMPSUMMON_MANUAL_DESPAWN))
                ++_eyesAlive;
        }

        // DBM: first casts, Mythic is delayed
        if (IsMythic())
        {
            events.ScheduleEvent(EVENT_SCORN_AND_IRE, 14s, 0, PHASE_STAGE_TWO);
            events.ScheduleEvent(EVENT_DESOLATION_BEAM, 21s + 100ms, 0, PHASE_STAGE_TWO);
            events.ScheduleEvent(EVENT_TITANIC_DEATH_GAZE, 28s + 200ms, 0, PHASE_STAGE_TWO);
        }
        else
        {
            events.ScheduleEvent(EVENT_DESOLATION_BEAM, 8s + 400ms, 0, PHASE_STAGE_TWO);
            events.ScheduleEvent(EVENT_TITANIC_DEATH_GAZE, 18s, 0, PHASE_STAGE_TWO);
        }
    }

    void EndAddsPhase()
    {
        me->RemoveAurasDueToSpell(SPELL_STYGIAN_DARKSHIELD);
        events.Reset();

        if (_addsPhasesDone < std::size(AddsPhasePct))
        {
            ScheduleStageOne(AfterAdds);
            return;
        }

        // Both pairs are dead: the Eye is enraged
        events.SetPhase(PHASE_STAGE_THREE);
        events.ScheduleEvent(EVENT_IMMEDIATE_EXTERMINATION, 1ms, 0, PHASE_STAGE_THREE);
        events.ScheduleEvent(EVENT_DEATHLINK, StageThree.Deathlink, 0, PHASE_STAGE_THREE);
        events.ScheduleEvent(EVENT_FRACTURE_SOUL, StageThree.FractureSoul, 0, PHASE_STAGE_THREE);
        events.ScheduleEvent(EVENT_DESOLATION_BEAM, 13s, 0, PHASE_STAGE_THREE);
        events.ScheduleEvent(EVENT_ANNIHILATING_GLARE, StageThree.Glare, 0, PHASE_STAGE_THREE);
        events.ScheduleEvent(EVENT_PIERCING_LENS, 1s, 2s, 0, PHASE_STAGE_THREE);
    }

    uint8 _addsPhasesDone;
    uint8 _eyesAlive;
};

// 176531 - Deathseeker Eye
struct npc_deathseeker_eye_sod : public ScriptedAI
{
    npc_deathseeker_eye_sod(Creature* creature) : ScriptedAI(creature) { }

    void Reset() override
    {
        _events.Reset();
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        _events.ScheduleEvent(EVENT_EYE_BOLT, 5s);
        if (!(me->GetMap()->GetDifficultyID() == DIFFICULTY_LFR_NEW))
        {
            _events.ScheduleEvent(EVENT_SPREADING_MISERY, 14s + 900ms);
            _events.ScheduleEvent(EVENT_SLOTHFUL_CORRUPTION, 24s);
        }
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
                case EVENT_EYE_BOLT:
                    if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0, 0.0f, true))
                        DoCast(target, SPELL_EYE_BOLT);
                    _events.Repeat(EyeBoltCd);
                    break;
                case EVENT_SPREADING_MISERY:
                    DoCastVictim(SPELL_SPREADING_MISERY);
                    _events.Repeat(SpreadingMiseryCd);
                    break;
                case EVENT_SLOTHFUL_CORRUPTION:
                    DoCastSelf(SPELL_SLOTHFUL_CORRUPTION);
                    _events.Repeat(SlothfulCd);
                    break;
                default:
                    break;
            }

            if (me->HasUnitState(UNIT_STATE_CASTING))
                return;
        }
    }

private:
    EventMap _events;
};

void AddSC_boss_eye_of_the_jailer_sod()
{
    RegisterSanctumOfDominationCreatureAI(boss_eye_of_the_jailer_sod);
    RegisterSanctumOfDominationCreatureAI(npc_deathseeker_eye_sod);
}
