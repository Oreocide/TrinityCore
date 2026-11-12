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
 * Hakkar the Soulflayer (DungeonEncounter 2395, trinitycore-tracking/sl-dungeons/de-other-side/bosses/hakkar-the-soulflayer.md).
 *
 * Energy bar: +4.3 per second from the pull (Heroic capture: 4 at 1 s, 43 at 10 s, 99 at 23 s, 100 at 24 s). At 100 he casts Blood
 * Barrier (it waits for a cast in progress, as the capture's 26 s does behind the second Corrupted Blood), which damages everyone,
 * forces Blood Barrage and shields him. Blood Barrage rains missiles at the players until the shield is broken, then the energy
 * starts again from 0.
 *
 * Medium / Low (one Heroic pull plus Mythic Warcraft Logs):
 * - Blood Barrier shield: the capture shows no absorb aura (322773), Wowhead says its value is the damage 322759 inflicted. The
 *   shield is that damage (Low); with no damage there is no shield and the barrage runs to the kill, as in the capture.
 * - The barrage ends when the shield is gone. Warcraft Logs' 29 s cycle (25.2, 54.3, 83.4 s) and a Piercing Barb at 37.4 s fit a
 *   shield broken about 2 s after the barrage starts, the Barb and Corrupted Blood timers held during the barrier and resumed 2 s
 *   later (DBM).
 * - Missiles: rate and scatter come from the capture's 1,069 Blood Barrage missiles of one player (a linear ramp of 1.8 + 0.32 per
 *   second of barrage per second; Low for a group).
 * - Corrupted Blood: a random player other than the tank (Low) and the spread to players within 7 yd when it lands (Low).
 *
 * Not scripted:
 * - Sons of Hakkar 165905 (Blood Nova 323544, Zealous 328983, Spilled Essence 323569): the Heroic capture has none (the three Atal'ai
 *   Devoted die at the pull and nothing replaces them); Warcraft Logs shows four per pull with no spawn rule.
 * - The Blood Barrage dummy casts 323067 / 323068 / 323108 / 323485 (hidden originators of the missiles) and the 2.5 s dummy tick of
 *   323064: the missiles are launched from the AI.
 * - Achievement Credit 335978 (a DUMMY with no handler) and the Bwonsamdi conversation 15440 after the kill.
 */

#include "GameObject.h"
#include "InstanceScript.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "SpellAuraEffects.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "SpellScript.h"
#include "de_other_side.h"
#include <cmath>

enum HakkarSpells
{
    SPELL_CORRUPTED_BLOOD           = 323166,   // 2 s cast, two DUMMY effects
    SPELL_CORRUPTED_BLOOD_AURA      = 322746,   // 12 s, 3 s damage ticks; effect 1 base points = the spread radius (7 yd)
    SPELL_PIERCING_BARB             = 322736,   // 1.5 s cast, tank hit
    SPELL_PIERCING_BARB_SECOND      = 322735,   // the second strike (its effect 0 trigger is not chained by the core)
    SPELL_BLOOD_BARRIER             = 322759,   // 2 s cast, damage to all enemies, forces Blood Barrage 2 s later
    SPELL_BLOOD_BARRIER_ABSORB      = 322773,   // the shield (aura, all schools)
    SPELL_BLOOD_BARRIER_ENERGY      = 323482,   // the energy bar (power display 452), applied at the pull
    SPELL_BLOOD_BARRAGE             = 323064,   // 2 s cast, aura with no duration: dome, areatrigger, missiles
    SPELL_BLOOD_BARRAGE_MISSILE     = 323111,   // missile to a location, its landing casts 323118 (core)
    SPELL_BLOOD_BARRAGE_IMPACT      = 323118    // 3 yd damage and -10% damage done for 9 s, stacks
};

enum HakkarEvents
{
    EVENT_CORRUPTED_BLOOD = 1,
    EVENT_PIERCING_BARB,
    EVENT_BLOOD_BARRIER
};

enum Texts
{
    // Group ids as in the live creature_text rows (2 and 4 are unused there)
    SAY_AGGRO                       = 0,    // His power is mine! I will live again!
    SAY_CORRUPTED_BLOOD             = 1,    // Face the wrath of the Soulflayer!
    SAY_PIERCING_BARB               = 3,    // Die, mortal!
    SAY_BLOOD_BARRIER               = 5,    // Your blood sustains me!
    SAY_DEATH                       = 6,    // The power... was mine...
    SAY_CORRUPTED_BLOOD_EMOTE       = 7,    // RaidBossEmote: $n is afflicted with Corrupted Blood!
    SAY_BLOOD_BARRIER_EMOTE         = 8     // RaidBossEmote: %s begins to cast Blood Barrier!
};

enum HakkarMisc
{
    NPC_ATALAI_DEVOTED              = 171333,
    NPC_SON_OF_HAKKAR               = 165905,
    GO_BLOOD_BARRIER_DOME           = 349134,

    DATA_BARRIER_DAMAGE             = 1,    // SetData from the Blood Barrier spell script: the damage it dealt
    GUID_CORRUPTED_BLOOD_TARGET     = 1,

    ENCOUNTER_FRAME_PRIORITY        = 1
};

static constexpr Milliseconds CorruptedBloodFirst = 6500ms;     // sniff 7.0 s begin, Warcraft Logs 6.3 s, DBM 8.1 s cast end
static constexpr Milliseconds CorruptedBloodInterval = 17s;     // sniff 17.0 s, DBM 17.1 s
static constexpr Milliseconds PiercingBarbFirst = 10800ms;      // sniff 11.0 s, Warcraft Logs 10.8 s
static constexpr Milliseconds PiercingBarbInterval = 9s;        // sniff 9.0 s, Warcraft Logs 8.5-9.8 s, DBM 8.5 s
static constexpr Milliseconds BarrierResumeDelay = 2s;          // DBM: the paused timers come back 2 s later (Medium)
static constexpr Milliseconds YellDelay = 1s;                   // the aggro yell, sniff 1.0 s after the pull
static constexpr Milliseconds DeathYellDelay = 1s;

static constexpr uint32 EnergyTickInterval = 1000;
static constexpr double EnergyPerTick = 4.3;                    // sniff: 43 at 10 s, 99 at 23 s, 100 at 24 s
static constexpr uint32 EnergyMax = 100;

// Blood Barrage missiles: a linear ramp from the capture (34, 66, 98, 130, 162 per ten seconds from the aura)
static constexpr double MissileRateStart = 1.8;                 // per second at the start of the barrage
static constexpr double MissileRateGrowth = 0.32;               // per second, each second
static constexpr uint32 DomeInterval = 10800;                   // sniff: 30, 42, 53, 63, 74, 84.7, 94.7, 105.5 s
static constexpr Seconds DomeLifetime = 5s;
static constexpr float SpreadRadiusFallback = 7.0f;

static Position const DomePos = { 2740.2f, -1689.5f, 59.2f, 0.0f };

// The scatter of a missile around its player, by the four casts of the capture (distance of the 1,069 landings to the player)
struct MissileScatter
{
    uint32 Weight;
    float MinRadius;
    float MaxRadius;
};

static constexpr MissileScatter MissileScatters[] =
{
    { 36, 0.0f, 4.8f },     // 323067 and the near half of 323485
    { 23, 0.0f, 11.0f },    // 323108
    { 29, 4.2f, 17.0f },    // 323068
    { 12, 12.0f, 19.0f }    // the far half of 323485
};

// 164558 - Hakkar the Soulflayer
struct boss_hakkar_the_soulflayer : public BossAI
{
    boss_hakkar_the_soulflayer(Creature* creature) : BossAI(creature, DATA_HAKKAR_THE_SOULFLAYER) { Initialize(); }

    void Initialize()
    {
        _engaged = false;
        _barrierCycle = false;
        _barrageActive = false;
        _shieldUp = false;
        _energy = 0.0;
        _energyTimer = 0;
        _barrageElapsed = 0;
        _missileCarry = 0.0;
        _domeTimer = 0;
        _heldCorruptedBlood = 0ms;
        _heldPiercingBarb = 0ms;
    }

    void Reset() override
    {
        BossAI::Reset();
        Initialize();
        me->SetPower(POWER_ENERGY, 0);
    }

    void JustEngagedWith(Unit* who) override
    {
        BossAI::JustEngagedWith(who);

        // A sequence break sends him back at once
        if (instance && instance->GetBossState(DATA_HAKKAR_THE_SOULFLAYER) != IN_PROGRESS)
            return;

        _engaged = true;

        if (instance)
            instance->SendEncounterUnit(ENCOUNTER_FRAME_ENGAGE, me, ENCOUNTER_FRAME_PRIORITY);

        // The Atal'ai Devoted of the ritual are consumed at the pull
        std::list<Creature*> devoted;
        me->GetCreatureListWithEntryInGrid(devoted, NPC_ATALAI_DEVOTED, 40.0f);
        for (Creature* creature : devoted)
            if (creature->IsAlive())
                Unit::Kill(me, creature, false);

        DoCastSelf(SPELL_BLOOD_BARRIER_ENERGY, CastSpellExtraArgs(TRIGGERED_FULL_MASK));

        scheduler.Schedule(YellDelay, [this](TaskContext)
        {
            Talk(SAY_AGGRO);
        });

        events.ScheduleEvent(EVENT_CORRUPTED_BLOOD, CorruptedBloodFirst);
        events.ScheduleEvent(EVENT_PIERCING_BARB, PiercingBarbFirst);
    }

    void JustDied(Unit* killer) override
    {
        BossAI::JustDied(killer);
        Cleanup();

        me->m_Events.AddEventAtOffset([this]()
        {
            Talk(SAY_DEATH);
        }, DeathYellDelay);
    }

    void EnterEvadeMode(EvadeReason why) override
    {
        Cleanup();
        BossAI::EnterEvadeMode(why);
    }

    // The players' debuffs and the encounter frame
    void Cleanup()
    {
        _engaged = false;
        _barrageActive = false;
        if (instance)
        {
            instance->SendEncounterUnit(ENCOUNTER_FRAME_DISENGAGE, me);
            instance->DoRemoveAurasDueToSpellOnPlayers(SPELL_CORRUPTED_BLOOD_AURA);
            instance->DoRemoveAurasDueToSpellOnPlayers(SPELL_BLOOD_BARRAGE_IMPACT);
        }
    }

    void SetData(uint32 id, uint32 value) override
    {
        if (id != DATA_BARRIER_DAMAGE || !value)
            return;

        // The shield is the damage Blood Barrier dealt
        _shieldUp = true;
        me->CastSpell(me, SPELL_BLOOD_BARRIER_ABSORB, CastSpellExtraArgs(TRIGGERED_FULL_MASK).AddSpellBP0(value));
    }

    void SetGUID(ObjectGuid const& guid, int32 id) override
    {
        if (id != GUID_CORRUPTED_BLOOD_TARGET)
            return;

        if (Player* player = ObjectAccessor::GetPlayer(*me, guid))
            Talk(SAY_CORRUPTED_BLOOD_EMOTE, player);
    }

    void OnSpellCast(SpellInfo const* spell) override
    {
        if (spell->Id != SPELL_BLOOD_BARRAGE)
            return;

        // The capture's energy drops to 0 as the aura lands
        _barrageActive = true;
        _barrageElapsed = 0;
        _missileCarry = 0.0;
        _domeTimer = DomeInterval;
        _energy = 0.0;
        me->SetPower(POWER_ENERGY, 0);

        // The linked summon of the aura makes the first dome with the spell's own duration
        me->m_Events.AddEventAtOffset([this]()
        {
            if (GameObject* dome = me->FindNearestGameObject(GO_BLOOD_BARRIER_DOME, 10.0f))
                dome->SetRespawnTime(DomeLifetime.count());
        }, 1ms);
    }

    // A Blood Barrier or forced Blood Barrage that never completes (interrupted, stunned) would leave the cycle held for the rest of
    // the fight: start the energy again and give the held abilities back
    void OnSpellFailed(SpellInfo const* spell) override
    {
        if (_barrageActive || !_barrierCycle)
            return;

        if (spell->Id == SPELL_BLOOD_BARRIER || spell->Id == SPELL_BLOOD_BARRAGE)
        {
            me->RemoveAurasDueToSpell(SPELL_BLOOD_BARRIER_ABSORB);
            ResumeAfterBarrier();
        }
    }

    void ExecuteEvent(uint32 eventId) override
    {
        switch (eventId)
        {
            case EVENT_CORRUPTED_BLOOD:
            {
                Unit* target = SelectTarget(SelectTargetMethod::Random, 0, 100.0f, true, false, -SPELL_CORRUPTED_BLOOD_AURA);
                if (!target)
                    target = SelectTarget(SelectTargetMethod::Random, 0, 100.0f, true, true, -SPELL_CORRUPTED_BLOOD_AURA);
                if (target)
                {
                    Talk(SAY_CORRUPTED_BLOOD);
                    DoCast(target, SPELL_CORRUPTED_BLOOD);
                }
                events.Repeat(CorruptedBloodInterval);
                break;
            }
            case EVENT_PIERCING_BARB:
                Talk(SAY_PIERCING_BARB);
                DoCastVictim(SPELL_PIERCING_BARB);
                events.Repeat(PiercingBarbInterval);
                break;
            case EVENT_BLOOD_BARRIER:
                // DBM pauses the Barb and Corrupted Blood timers for the barrier
                _heldCorruptedBlood = HoldEvent(EVENT_CORRUPTED_BLOOD, CorruptedBloodInterval);
                _heldPiercingBarb = HoldEvent(EVENT_PIERCING_BARB, PiercingBarbInterval);
                Talk(SAY_BLOOD_BARRIER);
                Talk(SAY_BLOOD_BARRIER_EMOTE);
                DoCastSelf(SPELL_BLOOD_BARRIER);
                break;
            default:
                break;
        }
    }

    Milliseconds HoldEvent(uint32 eventId, Milliseconds fallback)
    {
        Milliseconds remaining = events.GetTimeUntilEvent(eventId);
        events.CancelEvent(eventId);
        return remaining == Milliseconds::max() ? fallback : std::max(remaining, 0ms);
    }

    void UpdateAI(uint32 diff) override
    {
        scheduler.Update(diff);

        if (_engaged)
        {
            UpdateEnergy(diff);
            UpdateBarrage(diff);
        }

        BossAI::UpdateAI(diff);
    }

    void UpdateEnergy(uint32 diff)
    {
        if (_barrierCycle)
            return;

        _energyTimer += diff;
        while (_energyTimer >= EnergyTickInterval)
        {
            _energyTimer -= EnergyTickInterval;
            _energy = std::min<double>(_energy + EnergyPerTick, EnergyMax);
            me->SetPower(POWER_ENERGY, uint32(std::lround(_energy)));
        }

        if (me->GetPower(POWER_ENERGY) >= int32(EnergyMax))
        {
            _barrierCycle = true;
            events.ScheduleEvent(EVENT_BLOOD_BARRIER, 0ms);
        }
    }

    void UpdateBarrage(uint32 diff)
    {
        if (!_barrageActive)
            return;

        // The shield is broken: the barrage ends. Checked one update after the aura lands.
        if (_shieldUp && !me->HasAura(SPELL_BLOOD_BARRIER_ABSORB))
        {
            EndBarrage();
            return;
        }

        _barrageElapsed += diff;
        double seconds = double(diff) / 1000.0;
        _missileCarry += (MissileRateStart + MissileRateGrowth * (double(_barrageElapsed) / 1000.0)) * seconds;
        while (_missileCarry >= 1.0)
        {
            _missileCarry -= 1.0;
            LaunchMissile();
        }

        if (_domeTimer <= diff)
        {
            _domeTimer = DomeInterval;
            me->SummonGameObject(GO_BLOOD_BARRIER_DOME, DomePos, QuaternionData::fromEulerAnglesZYX(DomePos.GetOrientation(), 0.0f, 0.0f), DomeLifetime, GO_SUMMON_TIMED_DESPAWN);
        }
        else
            _domeTimer -= diff;
    }

    void LaunchMissile()
    {
        Unit* target = SelectTarget(SelectTargetMethod::Random, 0, 0.0f, true);
        if (!target)
            return;

        uint32 roll = urand(1, 100);
        MissileScatter const* scatter = &MissileScatters[0];
        for (MissileScatter const& candidate : MissileScatters)
        {
            scatter = &candidate;
            if (roll <= candidate.Weight)
                break;
            roll -= candidate.Weight;
        }

        float radius = frand(scatter->MinRadius, scatter->MaxRadius);
        float angle = frand(0.0f, 2.0f * float(M_PI));
        Position dest = target->GetPosition();
        dest.m_positionX += radius * std::cos(angle);
        dest.m_positionY += radius * std::sin(angle);
        me->CastSpell(dest, SPELL_BLOOD_BARRAGE_MISSILE, CastSpellExtraArgs(TRIGGERED_FULL_MASK));
    }

    void EndBarrage()
    {
        _barrageActive = false;
        me->RemoveAurasDueToSpell(SPELL_BLOOD_BARRAGE);
        me->RemoveAreaTrigger(SPELL_BLOOD_BARRAGE);
        ResumeAfterBarrier();
    }

    void ResumeAfterBarrier()
    {
        _barrierCycle = false;
        _shieldUp = false;
        _energy = 0.0;
        _energyTimer = 0;
        me->SetPower(POWER_ENERGY, 0);

        events.ScheduleEvent(EVENT_CORRUPTED_BLOOD, _heldCorruptedBlood + BarrierResumeDelay);
        events.ScheduleEvent(EVENT_PIERCING_BARB, _heldPiercingBarb + BarrierResumeDelay);
    }

private:
    bool _engaged;
    bool _barrierCycle;         // from the energy reaching 100 to the end of the barrage
    bool _barrageActive;
    bool _shieldUp;
    double _energy;
    uint32 _energyTimer;
    uint32 _barrageElapsed;
    double _missileCarry;
    uint32 _domeTimer;
    Milliseconds _heldCorruptedBlood;
    Milliseconds _heldPiercingBarb;
};

// 323166 - Corrupted Blood: both effects are DUMMY with no core behaviour; the aura goes on the target
class spell_dos_hakkar_corrupted_blood : public SpellScript
{
    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        Unit* caster = GetCaster();
        Unit* target = GetHitUnit();
        if (!target)
            return;

        caster->CastSpell(target, SPELL_CORRUPTED_BLOOD_AURA, CastSpellExtraArgs(TRIGGERED_FULL_MASK));

        if (Creature* creature = caster->ToCreature())
            if (creature->IsAIEnabled())
                creature->AI()->SetGUID(target->GetGUID(), GUID_CORRUPTED_BLOOD_TARGET);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_dos_hakkar_corrupted_blood::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 322746 - Corrupted Blood: it spreads to the players within 7 yd when it lands
class spell_dos_hakkar_corrupted_blood_aura : public AuraScript
{
    void HandleApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        Unit* caster = GetCaster();
        Unit* target = GetTarget();
        if (!caster || !target)
            return;

        float radius = SpreadRadiusFallback;
        if (GetSpellInfo()->GetEffects().size() > EFFECT_1)
            if (float value = float(GetSpellInfo()->GetEffect(EFFECT_1).CalcValue(caster)); value > 0.0f)
                radius = value;

        std::list<Player*> players;
        target->GetPlayerListInGrid(players, radius);
        for (Player* player : players)
            if (player != target && !player->HasAura(SPELL_CORRUPTED_BLOOD_AURA))
                caster->CastSpell(player, SPELL_CORRUPTED_BLOOD_AURA, CastSpellExtraArgs(TRIGGERED_FULL_MASK));
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(spell_dos_hakkar_corrupted_blood_aura::HandleApply, EFFECT_0, SPELL_AURA_PERIODIC_DAMAGE, AURA_EFFECT_HANDLE_REAL);
    }
};

// 322736 - Piercing Barb: the second strike 322735 follows the first on the same target
class spell_dos_hakkar_piercing_barb : public SpellScript
{
    void HandleSecondStrike(SpellEffIndex /*effIndex*/)
    {
        if (Unit* target = GetHitUnit())
            GetCaster()->CastSpell(target, SPELL_PIERCING_BARB_SECOND, CastSpellExtraArgs(TRIGGERED_FULL_MASK));
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_dos_hakkar_piercing_barb::HandleSecondStrike, EFFECT_0, SPELL_EFFECT_SCHOOL_DAMAGE);
    }
};

// 322759 - Blood Barrier: the shield is the damage dealt to players
class spell_dos_hakkar_blood_barrier : public SpellScript
{
    void CountDamage(SpellEffIndex /*effIndex*/)
    {
        Unit* target = GetHitUnit();
        if (!target || GetHitDamage() <= 0)
            return;

        if (target->IsPlayer() || target->GetEntry() == NPC_SON_OF_HAKKAR)
            _damage += uint32(GetHitDamage());
    }

    void SendDamage()
    {
        Creature* caster = GetCaster()->ToCreature();
        if (caster && caster->IsAIEnabled())
            caster->AI()->SetData(DATA_BARRIER_DAMAGE, _damage);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_dos_hakkar_blood_barrier::CountDamage, EFFECT_0, SPELL_EFFECT_SCHOOL_DAMAGE);
        OnEffectHitTarget += SpellEffectFn(spell_dos_hakkar_blood_barrier::CountDamage, EFFECT_1, SPELL_EFFECT_SCHOOL_DAMAGE);
        AfterCast += SpellCastFn(spell_dos_hakkar_blood_barrier::SendDamage);
    }

private:
    uint32 _damage = 0;
};

void AddSC_boss_hakkar_the_soulflayer()
{
    RegisterDeOtherSideCreatureAI(boss_hakkar_the_soulflayer);
    RegisterSpellScript(spell_dos_hakkar_corrupted_blood);
    RegisterSpellScript(spell_dos_hakkar_corrupted_blood_aura);
    RegisterSpellScript(spell_dos_hakkar_piercing_barb);
    RegisterSpellScript(spell_dos_hakkar_blood_barrier);
}
