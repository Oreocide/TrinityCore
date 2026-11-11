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
 * General Kaal (DungeonEncounter 2363, trinitycore-tracking/sl-dungeons/sanguine-depths/bosses/general-kaal.md).
 *
 * She is created at 70% health and the encounter ends when she reaches 50%, without a death: Rend Souls stuns the party, the encounter
 * is completed, a conversation plays, Draven arrives, the stun is lifted, her chest drops and she flies away. A wipe makes her despawn
 * and come back 29 s later (capture).
 *
 * Loop (Heroic capture, one pull, Medium; Warcraft Logs is Mythic+ only): Wicked Rush 6 s and 21 s, Piercing Blur 11 s and 20 s,
 * Gloom Squall 34.5 s; after a Squall Blur +6 s, Rush +7 s, Blur +15 s, Rush +22.8 s (Medium), Blur +24 s (Low: the Heroic Blur
 * cadence of 9 s continued), the next Squall 38.8 s after the last (Warcraft Logs, High).
 *
 * Not scripted:
 * - Mythic: a second Wicked Gash per Rush, more than two afterimages.
 * - The party buttons Shining Radiance 324086 / 324092 / Radiant Recharge 326790 and the Essence of Z'rali ride they come from (out
 *   of scope): Gloom Squall damages everyone, and nothing here depends on the shelter.
 * - Gloom Squall 322895 / 322898 (a one-sample reposition to (-1287.7, 6501.9) with no destination rule), 323880 (Wicked Rush
 *   target selection: the AI charges the marked player 6 s after the mark), 334877 -> 334876 (the chest is spawned directly).
 * - The telegraph areatriggers 304154 / 304155 of the afterimage (no create properties exist).
 * - The post-fight casts of Draven (318858, 331508, 342624, 342627: effects not verified) and his walk and flight, and the four
 *   Rockbound Sprites that die one second after the end.
 */

#include "Containers.h"
#include "Conversation.h"
#include "G3D/Vector3.h"
#include "GameObjectAI.h"
#include "GameObject.h"
#include "InstanceScript.h"
#include "Map.h"
#include "MotionMaster.h"
#include "MoveSplineInit.h"
#include "ObjectAccessor.h"
#include "PassiveAI.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "SpellInfo.h"
#include "SpellScript.h"
#include "TemporarySummon.h"
#include "sanguine_depths.h"
#include <array>

enum KaalSpells
{
    SPELL_PERIODIC_ENERGY_GAIN      = 322803,   // +3 energy per second on her, from the pull
    SPELL_WICKED_RUSH               = 323845,   // 10 s dummy mark on the target (removed when she charges, 6 s later)
    SPELL_WICKED_RUSH_CHARGE        = 323846,   // charge, its arrival casts Wicked Gash (322796 on Normal and Heroic)
    SPELL_WICKED_GASH               = 322796,
    SPELL_WICKED_GASH_MYTHIC        = 331415,
    SPELL_PIERCING_BLUR             = 323821,   // 1 s cast, its end creates the two afterimages
    SPELL_PIERCING_BLUR_DAMAGE      = 323810,   // afterimage cast, 3 s: jump to the other end and 45 yd rectangle damage
    SPELL_PIERCING_BLUR_JUMP        = 335915,   // the other afterimage's cast: the same jump, no damage
    SPELL_GLOOM_SQUALL              = 322903,   // 4 s cast, raid damage
    SPELL_PRE_PULL_AURA             = 173366,   // creature_template_addon aura, removed at the pull

    // The end
    SPELL_BREAK_PLAYER_TARGETING    = 140562,
    SPELL_REND_SOULS                = 324012,   // E1 triggers the 20 s stun 336749 on every player within 100 yd
    SPELL_REND_SOULS_STUN           = 336749,
    SPELL_REVENDRETH_STUN_REMOVAL   = 304150,   // removes 336749 within 200 yd
    SPELL_KILL_CREDIT_PLAYER_ONLY   = 344992
};

enum KaalEvents
{
    EVENT_WICKED_RUSH = 1,
    EVENT_WICKED_RUSH_CHARGE,
    EVENT_PIERCING_BLUR,
    EVENT_GLOOM_SQUALL
};

enum Texts
{
    // Group ids as in the live creature_text rows
    SAY_AGGRO                       = 0,    // Surrender the prisoner!
    SAY_BLEED                       = 1,    // Bleed for your crimes.
    SAY_WICKED_RUSH                 = 2,    // You lack the strength of stone.
    SAY_GLOOM_SQUALL                = 3     // The depths will take you all!
};

enum KaalCreatures
{
    NPC_AFTERIMAGE                  = 166085,
    NPC_GENERAL_DRAVEN              = 166219
};

enum KaalMisc
{
    GO_KAALS_ARMAMENTS              = 355346,
    CONVERSATION_KAAL_END           = 14661,    // actor: her spawn (conversation_actors)
    CONVERSATION_DRAVEN_END         = 14668,
    TIER_TRANSITION_TAKEOFF         = 8,
    GUID_RUSH_TARGET                = 1
};

static constexpr float StartHealthPct = 70.0f;
static constexpr float EndHealthPct = 50.0f;

static constexpr Milliseconds WickedRushChargeDelay = 6s;           // sniff: 6.0 s both times
static constexpr Milliseconds GloomSquallFirst = 34500ms;           // sniff 34 s, DBM 34.5 s, Warcraft Logs 35.1 s
static constexpr Milliseconds GloomSquallInterval = 38800ms;
static constexpr Seconds EvadeRespawnDelay = 29s;                   // the capture recreates her 29.0 s after the wipe
static constexpr uint32 RushTargetCount = 1;                        // Low: solo capture, Wowhead says "multiple" players

struct KaalStep
{
    uint32 EventId;
    Milliseconds Offset;
};

// From the pull (Heroic capture)
static constexpr std::array<KaalStep, 4> OpeningLadder =
{ {
    { EVENT_WICKED_RUSH,        6s },
    { EVENT_PIERCING_BLUR,      11s },
    { EVENT_PIERCING_BLUR,      20s },
    { EVENT_WICKED_RUSH,        21s }
} };

// From the start of a Gloom Squall cast
static constexpr std::array<KaalStep, 5> SquallLadder =
{ {
    { EVENT_PIERCING_BLUR,      6s },
    { EVENT_WICKED_RUSH,        7s },
    { EVENT_PIERCING_BLUR,      15s },
    { EVENT_WICKED_RUSH,        22800ms },  // Medium: Mythic+ median
    { EVENT_PIERCING_BLUR,      24s }       // Low
} };

// The afterimages stand at the two ends of one of three parallel lanes (all four captured casts used three distinct lanes: Low)
struct BlurLane
{
    Position DamageEnd;     // casts 323810, faces the other end
    Position JumpEnd;       // casts 335915
};

static BlurLane const BlurLanes[] =
{
    { { -1296.52f, 6538.99f, 4083.17f, 5.3233f }, { -1275.02f, 6508.92f, 4083.17f, 2.1817f } },
    { { -1306.97f, 6531.64f, 4083.17f, 5.3233f }, { -1285.85f, 6501.94f, 4083.17f, 2.1817f } },
    { { -1317.51f, 6522.72f, 4083.17f, 5.3233f }, { -1296.25f, 6494.59f, 4083.17f, 2.1817f } }
};

static constexpr Milliseconds AfterimageJumpDelay = 1s;             // the 3 s cast starts 1 s after they appear
static constexpr Milliseconds AfterimageJumpEndDespawn = 4600ms;    // gone as the jump lands
static constexpr Milliseconds AfterimageDamageEndDespawn = 7s;      // the damage end stays 3 s longer

// The end (offsets from the 50% mark)
static Position const KaalEndPos = { -1289.05f, 6520.75f, 4083.13f, 3.826f };
static Position const DravenPos = { -1304.32f, 6508.51f, 4083.22f, 0.6109f };
static Position const ChestPos = { -1289.05f, 6520.75f, 4083.13f, 4.0014f };
static constexpr float TakeoffHeight = 5.0f;
static constexpr Milliseconds DravenArrives = 7s;
static constexpr Milliseconds DravenConversation = 9s;
static constexpr Milliseconds StunRemoved = 10s;
static constexpr Milliseconds Takeoff = 11s;
static constexpr Milliseconds TakeoffTime = 2s;
static constexpr Milliseconds ChestAndFlight = 14s;
static constexpr Milliseconds Despawn = 19s;
static constexpr Milliseconds DravenDespawn = 32s;                  // after he arrives (+39 s)
static constexpr float FlightSpeed = 10.0f;

static std::array<G3D::Vector3, 4> const FlightPath =
{ {
    { -1278.2343f, 6531.6143f, 4090.3245f },
    { -1262.4843f, 6544.3643f, 4092.0745f },
    { -1238.9843f, 6567.8643f, 4092.0745f },
    { -1148.4185f, 6659.979f, 4092.019f }
} };

// 162099 - General Kaal
struct boss_general_kaal_sd : public BossAI
{
    boss_general_kaal_sd(Creature* creature) : BossAI(creature, DATA_GENERAL_KAAL), _ending(false) { }

    void Reset() override
    {
        BossAI::Reset();
        _ending = false;
        _rushTargets.clear();
        PrepareHealth();
    }

    // Created at 70% and never healed: the template has RegenHealth set
    void PrepareHealth()
    {
        me->SetRegenerateHealth(false);
        me->SetHealth(me->CountPctFromMaxHealth(StartHealthPct));
        me->SetPower(POWER_ENERGY, 0);
    }

    void JustAppeared() override
    {
        BossAI::JustAppeared();
        PrepareHealth();

        // After an instance reload with her encounter done she must not be there again
        if (instance && instance->GetBossState(DATA_GENERAL_KAAL) == DONE)
            scheduler.Schedule(1ms, [this](TaskContext)
            {
                me->DespawnOrUnsummon(0ms, 24h);
            });
    }

    void JustEngagedWith(Unit* who) override
    {
        if (_ending)
            return;

        BossAI::JustEngagedWith(who);

        if (instance)
            instance->SendEncounterUnit(ENCOUNTER_FRAME_ENGAGE, me, 1);

        if (me->GetHealth() > me->CountPctFromMaxHealth(StartHealthPct))
            me->SetHealth(me->CountPctFromMaxHealth(StartHealthPct));

        Talk(SAY_AGGRO);
        me->RemoveAurasDueToSpell(SPELL_PRE_PULL_AURA);
        DoCastSelf(SPELL_PERIODIC_ENERGY_GAIN);

        for (KaalStep const& step : OpeningLadder)
            events.ScheduleEvent(step.EventId, step.Offset);
        events.ScheduleEvent(EVENT_GLOOM_SQUALL, GloomSquallFirst);
    }

    void SetGUID(ObjectGuid const& guid, int32 id) override
    {
        if (id == GUID_RUSH_TARGET)
            _rushTargets.push_back(guid);
    }

    // The afterimages and Draven are never zoned in
    void JustSummoned(Creature* summon) override
    {
        summons.Summon(summon);
    }

    void OnSpellCast(SpellInfo const* spell) override
    {
        switch (spell->Id)
        {
            case SPELL_PIERCING_BLUR:
                SummonAfterimages();
                break;
            case SPELL_GLOOM_SQUALL:
                me->SetPower(POWER_ENERGY, 0);  // the capture's energy restarts at the end of the cast
                break;
            default:
                break;
        }
    }

    void SummonAfterimages()
    {
        BlurLane const& lane = BlurLanes[urand(0, std::size(BlurLanes) - 1)];
        TempSummon* damageEnd = me->SummonCreature(NPC_AFTERIMAGE, lane.DamageEnd, TEMPSUMMON_TIMED_DESPAWN, AfterimageDamageEndDespawn);
        TempSummon* jumpEnd = me->SummonCreature(NPC_AFTERIMAGE, lane.JumpEnd, TEMPSUMMON_TIMED_DESPAWN, AfterimageJumpEndDespawn);
        if (!damageEnd || !jumpEnd)
            return;

        // Not on the BossAI scheduler: the jump must not wait for a cast she starts in the meantime
        me->m_Events.AddEventAtOffset([this, damageGuid = damageEnd->GetGUID(), jumpGuid = jumpEnd->GetGUID(), &lane]()
        {
            if (Creature* image = ObjectAccessor::GetCreature(*me, damageGuid))
                image->CastSpell(lane.JumpEnd, SPELL_PIERCING_BLUR_DAMAGE);
            if (Creature* image = ObjectAccessor::GetCreature(*me, jumpGuid))
                image->CastSpell(lane.DamageEnd, SPELL_PIERCING_BLUR_JUMP);
        }, AfterimageJumpDelay);
    }

    void ExecuteEvent(uint32 eventId) override
    {
        switch (eventId)
        {
            case EVENT_WICKED_RUSH:
                _rushTargets.clear();
                Talk(urand(0, 3) ? SAY_WICKED_RUSH : SAY_BLEED);    // Low: the lines' odds (3 to 1 in four samples)
                DoCastSelf(SPELL_WICKED_RUSH);
                events.ScheduleEvent(EVENT_WICKED_RUSH_CHARGE, WickedRushChargeDelay);
                break;
            case EVENT_WICKED_RUSH_CHARGE:
            {
                GuidVector targets = std::move(_rushTargets);
                _rushTargets.clear();
                for (ObjectGuid const& guid : targets)
                {
                    Player* target = ObjectAccessor::GetPlayer(*me, guid);
                    if (!target)
                        continue;

                    target->RemoveAurasDueToSpell(SPELL_WICKED_RUSH, me->GetGUID());
                    if (!target->IsAlive())
                        continue;

                    if (DoCast(target, SPELL_WICKED_RUSH_CHARGE) != SPELL_CAST_OK)
                        DoCast(target, SPELL_WICKED_RUSH_CHARGE, CastSpellExtraArgs(TRIGGERED_FULL_MASK));
                }
                break;
            }
            case EVENT_PIERCING_BLUR:
                DoCastSelf(SPELL_PIERCING_BLUR);
                break;
            case EVENT_GLOOM_SQUALL:
                Talk(SAY_GLOOM_SQUALL);
                DoCastSelf(SPELL_GLOOM_SQUALL);
                for (KaalStep const& step : SquallLadder)
                    events.ScheduleEvent(step.EventId, step.Offset);
                events.Repeat(GloomSquallInterval);
                break;
            default:
                break;
        }
    }

    // What Clear Player Debuffs 334817 does at the end, and the cleanup of a wipe
    void ClearPlayerDebuffs()
    {
        if (!instance)
            return;

        for (uint32 spellId : { SPELL_WICKED_RUSH, SPELL_WICKED_GASH, SPELL_WICKED_GASH_MYTHIC })
            instance->DoRemoveAurasDueToSpellOnPlayers(spellId);
    }

    // Her health may not drop below one: the encounter ends at 50% without a death. The hit that crosses the line lands (the capture
    // ends at 49.1%); the end itself runs on the next update, outside the damage code.
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

        // Not on the BossAI scheduler: its validator would hold the end until a cast in progress finishes
        _ending = true;
        me->m_Events.AddEventAtOffset([this]()
        {
            EndEncounter();
        }, 1ms);
    }

    // Capture order at the end (all in one second): Break Player Targetting, the energy aura off, Clear Player Debuffs, Rend Souls and
    // its stun, her return to her spawn point, ENCOUNTER_END success, the afterimages gone, conversation 14661
    void EndEncounter()
    {
        if (instance)
            instance->SendEncounterUnit(ENCOUNTER_FRAME_DISENGAGE, me);

        events.Reset();
        _rushTargets.clear();
        me->InterruptNonMeleeSpells(false);
        me->AttackStop();
        me->SetReactState(REACT_PASSIVE);
        me->GetMotionMaster()->Clear();

        DoCastSelf(SPELL_BREAK_PLAYER_TARGETING);
        me->RemoveAurasDueToSpell(SPELL_PERIODIC_ENERGY_GAIN);
        ClearPlayerDebuffs();
        // Rend Souls is a self channel with no duration: the outro tasks must run while she channels it, so the BossAI validator
        // (no task while casting) goes, and nothing below may interrupt it (CombatStop(false))
        scheduler.ClearValidator();
        me->CastSpell(me, SPELL_REND_SOULS, CastSpellExtraArgs(TRIGGERED_FULL_MASK));

        me->SetImmuneToAll(true);
        me->SetUnitFlag2(UNIT_FLAG2_CANNOT_TURN);
        me->NearTeleportTo(KaalEndPos);

        // DONE: ENCOUNTER_END, boss kill credit, the complete world state and her door. She is out of combat for good afterwards.
        if (instance)
            instance->SetBossState(DATA_GENERAL_KAAL, DONE);
        me->CombatStop(false);
        EngagementOver();

        summons.DespawnAll();
        Conversation::CreateConversation(CONVERSATION_KAAL_END, me, me->GetPosition(), ObjectGuid::Empty);

        scheduler.Schedule(DravenArrives, [this](TaskContext)
        {
            me->SummonCreature(NPC_GENERAL_DRAVEN, DravenPos, TEMPSUMMON_TIMED_DESPAWN, DravenDespawn);
        });

        scheduler.Schedule(DravenConversation, [this](TaskContext)
        {
            if (Creature* draven = me->FindNearestCreature(NPC_GENERAL_DRAVEN, 100.0f))
                Conversation::CreateConversation(CONVERSATION_DRAVEN_END, draven, draven->GetPosition(), ObjectGuid::Empty);
        });

        scheduler.Schedule(StunRemoved, [this](TaskContext)
        {
            DoCastSelf(SPELL_REVENDRETH_STUN_REMOVAL, CastSpellExtraArgs(TRIGGERED_FULL_MASK));
            me->InterruptNonMeleeSpells(true, SPELL_REND_SOULS);
            me->RemoveAurasDueToSpell(SPELL_REND_SOULS);
            me->RemoveUnitFlag2(UNIT_FLAG2_CANNOT_TURN);
        });

        scheduler.Schedule(Takeoff, [this](TaskContext)
        {
            me->SetFloating(true);
            Position hover = me->GetPosition();
            hover.m_positionZ += TakeoffHeight;
            me->GetMotionMaster()->MoveTakeoff(0, hover, TIER_TRANSITION_TAKEOFF, TakeoffHeight / std::chrono::duration_cast<std::chrono::duration<float>>(TakeoffTime).count());
        });

        scheduler.Schedule(ChestAndFlight, [this](TaskContext)
        {
            DoCastSelf(SPELL_KILL_CREDIT_PLAYER_ONLY, CastSpellExtraArgs(TRIGGERED_FULL_MASK));
            SummonChest();
            me->GetMotionMaster()->LaunchMoveSpline([](Movement::MoveSplineInit& init)
            {
                init.MovebyPath(FlightPath);
                init.SetFly();
                init.SetSmooth();
                init.SetVelocity(FlightSpeed);
            });
        });

        scheduler.Schedule(Despawn, [this](TaskContext)
        {
            me->DespawnOrUnsummon(0ms, 24h);
        });
    }

    // The chest's personal loot is made by the core at its first use, for everyone in the tap list
    void SummonChest()
    {
        GameObject* chest = me->SummonGameObject(GO_KAALS_ARMAMENTS, ChestPos, QuaternionData::fromEulerAnglesZYX(ChestPos.GetOrientation(), 0.0f, 0.0f), 1h, GO_SUMMON_TIMED_DESPAWN);
        if (!chest || !instance)
            return;

        GuidUnorderedSet tapList;
        me->GetMap()->DoOnPlayers([&tapList](Player* player)
        {
            tapList.insert(player->GetGUID());
        });
        chest->SetTapList(std::move(tapList));
    }

    void UpdateAI(uint32 diff) override
    {
        scheduler.Update(diff);

        // The end runs on the scheduler: no victim checks, nothing to evade from
        if (_ending)
            return;

        BossAI::UpdateAI(diff);
    }

    void EnterEvadeMode(EvadeReason /*why*/) override
    {
        if (_ending)
            return;

        if (instance)
            instance->SendEncounterUnit(ENCOUNTER_FRAME_DISENGAGE, me);

        ClearPlayerDebuffs();
        summons.DespawnAll();
        _DespawnAtEvade(EvadeRespawnDelay);
    }

private:
    bool _ending;
    GuidVector _rushTargets;
};

// 166085 - the afterimages only cast what the boss tells them to
struct npc_sd_kaal_afterimage : public NullCreatureAI
{
    using NullCreatureAI::NullCreatureAI;
};

// 355346 - Kaal's Armaments: the core makes the personal loot at the first use for the tap list. A player left out of it (an empty
// roll) would make the core rebuild and replace everyone's loot, so only a use before the first or by a player with loot goes through.
struct go_sd_kaal_armaments : public GameObjectAI
{
    using GameObjectAI::GameObjectAI;

    bool OnGossipHello(Player* player) override
    {
        return !me->m_personalLoot.empty() && !me->m_personalLoot.contains(player->GetGUID());
    }
};

// 323845 - Wicked Rush: the mark goes on one player (the AI charges the marked ones)
class spell_sd_kaal_wicked_rush : public SpellScript
{
    void FilterTargets(std::list<WorldObject*>& targets)
    {
        targets.remove_if([](WorldObject const* target)
        {
            Unit const* unit = target->ToUnit();
            return !unit || !unit->IsPlayer() || !unit->IsAlive();
        });
        Trinity::Containers::RandomResize(targets, RushTargetCount);

        Creature* caster = GetCaster()->ToCreature();
        if (!caster || !caster->IsAIEnabled())
            return;

        for (WorldObject const* target : targets)
            caster->AI()->SetGUID(target->GetGUID(), GUID_RUSH_TARGET);
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_sd_kaal_wicked_rush::FilterTargets, EFFECT_0, TARGET_UNIT_DEST_AREA_ENEMY);
    }
};

void AddSC_boss_general_kaal()
{
    RegisterSanguineDepthsCreatureAI(boss_general_kaal_sd);
    RegisterCreatureAI(npc_sd_kaal_afterimage);
    RegisterGameObjectAI(go_sd_kaal_armaments);
    RegisterSpellScript(spell_sd_kaal_wicked_rush);
}
