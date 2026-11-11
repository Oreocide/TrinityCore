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
 * Lord Chamberlain (DungeonEncounter 2381). Heroic timers from the 2026-11-10 capture (trinitycore-tracking/sl-dungeons/
 * halls-of-atonement/bosses/lord-chamberlain.md), cross-checked with Warcraft Logs (Mythic, no keystone).
 *
 * Loop: Telekinetic Toss every 12 s (one 10 s gap), Unleashed Suffering every 24 s, Erupting Torment every 24 s. At 70% and at 40%
 * health Chamberlain's Chorus runs about 25 s: Door of Shadows to the arena centre, Telekinetic Onslaught (the statues jump next to
 * him), Telekinetic Repulsion (they are thrown 18 yd out), Ritual of Woe (a 10 s beam at one statue, a shadow tick to every player
 * once a second). The loop then restarts with the pull offsets.
 *
 * The throw chains (323142, 323148, 323150, 329168..72, 323134) are DUMMY or JUMP_CHARGE effects whose jump parameters (misc value
 * 400) are not in jump_charge_params, so the statues are moved by script with the capture's jump profile (36 yd/s, arc height 1).
 * The linked summon 328553 of the Statue Collision aura 328551 is not in the client data: the four static Statue Collision objects
 * follow their statues instead.
 */

#include "AreaTrigger.h"
#include "AreaTriggerAI.h"
#include "Containers.h"
#include "GameObject.h"
#include "InstanceScript.h"
#include "Map.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "SpellScript.h"
#include "TemporarySummon.h"
#include "halls_of_atonement.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <vector>

enum LordChamberlainSpells
{
    SPELL_TELEKINETIC_COLLISION_AT      = 1252635,  // aura on the thrown statue: areatrigger create properties 39432
    SPELL_TELEKINETIC_COLLISION         = 323126,   // damage + knock back to a player the statue flies through
    SPELL_TELEKINETIC_TOSS_LANDING      = 1237634,  // sonic damage to every player within 300 yd of the landing
    SPELL_UNLEASHED_SUFFERING           = 323236,   // 3.5 s cast at the stalker, 40 yd cone
    SPELL_ERUPTING_TORMENT              = 1236973,  // 3.5 s cast
    SPELL_ERUPTING_TORMENT_AURA         = 1236965,  // 3.5 s marker on the boss and each statue
    SPELL_ERUPTING_TORMENT_DAMAGE       = 1236964,  // 10 yd around the boss and around each statue
    SPELL_DOOR_OF_SHADOWS               = 329104,   // 1.5 s cast, teleports to the arena centre
    SPELL_ONSLAUGHT_FRONT               = 329168,   // statue cast bars, 2.5 s
    SPELL_ONSLAUGHT_BACK                = 329170,
    SPELL_ONSLAUGHT_RIGHT               = 329171,
    SPELL_ONSLAUGHT_LEFT                = 329172,
    SPELL_REPULSION_LANDING             = 1237642,  // damage to every player at each statue landing
    SPELL_RITUAL_OF_WOE                 = 323393,   // 3.0 s cast, 10 s channel at one statue (Normal, Heroic)
    SPELL_RITUAL_OF_WOE_TICK            = 323414    // 100 yd around the statue
};

enum LordChamberlainCreatures
{
    NPC_SINSTONE_STATUE                 = 165737,
    NPC_TOSS_STALKER                    = 165770,
    NPC_SUFFERING_STALKER               = 165777
};

enum LordChamberlainGameObjects
{
    GO_STATUE_COLLISION                 = 352524
};

// Group ids as in creature_text for 164218 (group 0 is the Iron Rune Servant intro, not used here)
enum Texts
{
    SAY_AGGRO                           = 1,
    SAY_TOSS_CONTEMPT                   = 2,
    SAY_SUFFERING_MIGHT                 = 3,
    SAY_TOSS_PATIENCE                   = 4,
    SAY_SUFFERING_PITIFUL               = 5,
    SAY_TOSS_OUT_OF_WAY                 = 6,
    EMOTE_RITUAL                        = 7,
    SAY_RITUAL_FIRST                    = 8,
    SAY_RITUAL_SECOND                   = 9,
    SAY_DEATH                           = 10
};

enum LordChamberlainEvents
{
    EVENT_TOSS = 1,
    EVENT_SUFFERING,
    EVENT_TORMENT,
    EVENT_CHORUS
};

Position const ArenaCenter = { -2209.73f, 5700.49f, 4223.37f };

static constexpr float JumpSpeed                = 36.0f;    // capture: every statue jump, 36 yd/s
static constexpr float JumpHeight               = 1.0f;
static constexpr Milliseconds MinFlight         = 250ms;
static constexpr Milliseconds LandingGrace      = 60ms;
static constexpr Milliseconds TossLandingDelay  = 3000ms;   // Toss cast to landing damage, 11 casts in the capture
static constexpr float TossBeyondTarget         = 8.0f;     // the statue lands 8 yd past the stalker, along its path
static constexpr float OnslaughtDistance        = 5.0f;
static constexpr float RepulsionDistance        = 18.0f;
static constexpr Milliseconds RepulsionFlight   = 519ms;
static constexpr float RepulsionHeight          = 0.65f;
static constexpr Milliseconds TormentCastTime   = 3500ms;
static constexpr Milliseconds SufferingStalkerLife = 5s;
static constexpr Milliseconds TossStalkerLife   = 4s;
static constexpr float BeamHalfWidth            = 2.0f;     // not in the capture (solo run): estimate
static constexpr uint8 RitualTicks              = 10;

// Statue landings in the capture stay within these bounds; a throw at a player by the wall is pulled inside
static constexpr float ArenaMinX = -2234.0f;
static constexpr float ArenaMaxX = -2185.0f;
static constexpr float ArenaMinY = 5680.0f;
static constexpr float ArenaMaxY = 5722.0f;

static constexpr float ChorusHealthPct[] = { 70.0f, 40.0f };

// Offsets from the Door of Shadows cast
static constexpr Milliseconds OnslaughtOffset   = 4s;
static constexpr Milliseconds OnslaughtStagger[] = { 0ms, 1s, 1s, 2s };
static constexpr Milliseconds OnslaughtCast     = 2500ms;
static constexpr Milliseconds RepulsionOffset   = 10s;
static constexpr Milliseconds RitualOffset      = 11s;
static constexpr Milliseconds RitualTextOffset  = 1s;      // from the Ritual cast start
static constexpr Milliseconds RitualFirstTick   = 5s;      // from the Ritual cast start: 3 s cast, then 2 s
static constexpr Milliseconds ChorusLength      = 25050ms;

static constexpr uint32 OnslaughtSpells[] = { SPELL_ONSLAUGHT_FRONT, SPELL_ONSLAUGHT_BACK, SPELL_ONSLAUGHT_RIGHT, SPELL_ONSLAUGHT_LEFT };
static constexpr float OnslaughtAngles[] = { 0.0f, float(M_PI), -float(M_PI) / 2.0f, float(M_PI) / 2.0f };

static constexpr uint8 TossTexts[] = { SAY_TOSS_CONTEMPT, SAY_TOSS_PATIENCE, SAY_TOSS_OUT_OF_WAY };
static constexpr uint8 SufferingTexts[] = { SAY_SUFFERING_MIGHT, SAY_SUFFERING_PITIFUL };

static Position ClampToArena(Position pos)
{
    pos.m_positionX = std::clamp(pos.m_positionX, ArenaMinX, ArenaMaxX);
    pos.m_positionY = std::clamp(pos.m_positionY, ArenaMinY, ArenaMaxY);
    return pos;
}

static Milliseconds FlightTime(Position const& from, Position const& to)
{
    return std::max(MinFlight, duration_cast<Milliseconds>(FloatSeconds(from.GetExactDist(to) / JumpSpeed)));
}

// 165737 - Sinstone Statue
struct npc_hoa_chamberlain_statue : public ScriptedAI
{
    explicit npc_hoa_chamberlain_statue(Creature* creature) : ScriptedAI(creature) { }

    // statues never fight: the boss moves them
    void AttackStart(Unit* /*who*/) override { }
    void UpdateAI(uint32 /*diff*/) override { }

    bool IsAirborne() const { return _airborne; }

    void StartJump(Position const& dest, Milliseconds flight, float height, bool collisionTrigger)
    {
        _airborne = true;
        _landing = dest;
        _landing.SetOrientation(me->GetAbsoluteAngle(dest));

        if (GameObject* collision = GetCollision())
            collision->EnableCollision(false);

        if (collisionTrigger)
            DoCastSelf(SPELL_TELEKINETIC_COLLISION_AT, true);

        me->GetMotionMaster()->Clear();
        me->GetMotionMaster()->MoveJump(EVENT_JUMP, dest, flight, height, height);
    }

    // Anchor Here (45313) and the Statue Collision re-summon of the capture: the statue keeps its landing as home, its collision follows
    void Land()
    {
        _airborne = false;
        me->RemoveAurasDueToSpell(SPELL_TELEKINETIC_COLLISION_AT);
        me->SetHomePosition(_landing);
        PlaceCollision(_landing);
    }

    void ResetToSpawn()
    {
        _airborne = false;
        me->RemoveAurasDueToSpell(SPELL_TELEKINETIC_COLLISION_AT);
        me->GetMotionMaster()->Clear();
        me->StopMoving();

        Position spawn = me->GetRespawnPosition();
        if (GameObject* collision = GetCollision())
            collision->EnableCollision(false);
        me->NearTeleportTo(spawn);
        me->SetHomePosition(spawn);
        PlaceCollision(spawn);
    }

private:
    GameObject* GetCollision()
    {
        if (GameObject* collision = ObjectAccessor::GetGameObject(*me, _collision))
            return collision;

        GameObject* collision = me->FindNearestGameObject(GO_STATUE_COLLISION, 4.0f);
        if (collision)
            _collision = collision->GetGUID();
        return collision;
    }

    void PlaceCollision(Position const& pos)
    {
        GameObject* collision = GetCollision();
        if (!collision)
            return;

        me->GetMap()->GameObjectRelocation(collision, pos.GetPositionX(), pos.GetPositionY(), collision->GetPositionZ(), pos.GetOrientation());
        collision->EnableCollision(true);
    }

    ObjectGuid _collision;
    Position _landing;
    bool _airborne = false;
};

// 164218 - Lord Chamberlain
struct boss_lord_chamberlain : public BossAI
{
    boss_lord_chamberlain(Creature* creature) : BossAI(creature, DATA_LORD_CHAMBERLAIN) { }

    void Reset() override
    {
        _Reset();
        me->SetControlled(false, UNIT_STATE_ROOT);
        _chorusCount = 0;
        _chorusActive = false;
        _beamBlockers.clear();
        _statues.clear();
        ResetStatues();
    }

    void JustEngagedWith(Unit* who) override
    {
        BossAI::JustEngagedWith(who);
        Talk(SAY_AGGRO);
        CacheStatues();
        ScheduleLoop(9s, 15s, 24s);
    }

    void JustDied(Unit* /*killer*/) override
    {
        _JustDied();
        me->SetControlled(false, UNIT_STATE_ROOT);
        Talk(SAY_DEATH);
    }

    void DamageTaken(Unit* /*attacker*/, uint32& damage, DamageEffectType /*damageType*/, SpellInfo const* /*spellInfo*/) override
    {
        if (_chorusActive || _chorusCount >= std::size(ChorusHealthPct))
            return;

        if (me->HealthBelowPctDamaged(ChorusHealthPct[_chorusCount], damage))
            QueueChorus();
    }

    void UpdateAI(uint32 diff) override
    {
        scheduler.Update(diff);
        BossAI::UpdateAI(diff);
    }

    void ExecuteEvent(uint32 eventId) override
    {
        switch (eventId)
        {
            case EVENT_TOSS:
                DoToss();
                events.Repeat(_tossCount++ == 1 ? 10s : 12s);
                break;
            case EVENT_SUFFERING:
                DoSuffering();
                events.Repeat(_sufferingCount++ == 0 ? 22s : 24s);
                break;
            case EVENT_TORMENT:
                DoTorment();
                events.Repeat(_tormentCount++ == 0 ? 25s : 24s);
                break;
            case EVENT_CHORUS:
                StartChorus();
                break;
            default:
                break;
        }
    }

    std::vector<ObjectGuid> const& GetBeamBlockers() const { return _beamBlockers; }

private:
    void ScheduleLoop(Milliseconds toss, Milliseconds suffering, Milliseconds torment)
    {
        _tossCount = 0;
        _sufferingCount = 0;
        _tormentCount = 0;
        events.ScheduleEvent(EVENT_TOSS, toss);
        events.ScheduleEvent(EVENT_SUFFERING, suffering);
        events.ScheduleEvent(EVENT_TORMENT, torment);
    }

    void CacheStatues()
    {
        _statues.clear();
        std::list<Creature*> statues;
        me->GetCreatureListWithEntryInGrid(statues, NPC_SINSTONE_STATUE, 100.0f);
        for (Creature* statue : statues)
            _statues.push_back(statue->GetGUID());
    }

    void ResetStatues()
    {
        std::list<Creature*> statues;
        me->GetCreatureListWithEntryInGrid(statues, NPC_SINSTONE_STATUE, 100.0f);
        for (Creature* statue : statues)
            if (npc_hoa_chamberlain_statue* ai = dynamic_cast<npc_hoa_chamberlain_statue*>(statue->AI()))
                ai->ResetToSpawn();
    }

    npc_hoa_chamberlain_statue* GetStatueAI(ObjectGuid guid) const
    {
        Creature* statue = ObjectAccessor::GetCreature(*me, guid);
        return statue ? dynamic_cast<npc_hoa_chamberlain_statue*>(statue->AI()) : nullptr;
    }

    // Moves a statue after <delay>: jumps to dest with the capture's jump profile, lands, then runs onLand
    void ThrowStatue(ObjectGuid statue, Position const& dest, Milliseconds delay, Milliseconds flight, float height, bool collisionTrigger, std::function<void()> onLand = {})
    {
        scheduler.Schedule(delay, [this, statue, dest, flight, height, collisionTrigger, onLand = std::move(onLand)](TaskContext context)
        {
            npc_hoa_chamberlain_statue* ai = GetStatueAI(statue);
            if (!ai)
                return;

            ai->StartJump(dest, flight, height, collisionTrigger);
            context.Schedule(flight + LandingGrace, [this, statue, onLand](TaskContext /*landed*/)
            {
                if (npc_hoa_chamberlain_statue* ai = GetStatueAI(statue))
                    ai->Land();

                if (onLand)
                    onLand();
            });
        });
    }

    void DoToss()
    {
        std::vector<ObjectGuid> idle;
        for (ObjectGuid guid : _statues)
            if (npc_hoa_chamberlain_statue* ai = GetStatueAI(guid))
                if (!ai->IsAirborne())
                    idle.push_back(guid);

        Unit* target = SelectTarget(SelectTargetMethod::Random, 0, 100.0f, true);
        if (idle.empty() || !target)
            return;

        ObjectGuid statueGuid = Trinity::Containers::SelectRandomContainerElement(idle);
        Creature* statue = ObjectAccessor::GetCreature(*me, statueGuid);
        if (!statue)
            return;

        Talk(Trinity::Containers::SelectRandomContainerElement(TossTexts));

        Position stalker = target->GetPosition();
        me->SummonCreature(NPC_TOSS_STALKER, stalker, TEMPSUMMON_TIMED_DESPAWN, TossStalkerLife);

        float angle = statue->GetAbsoluteAngle(stalker);
        Position dest(stalker.GetPositionX() + TossBeyondTarget * std::cos(angle), stalker.GetPositionY() + TossBeyondTarget * std::sin(angle), statue->GetPositionZ());
        dest = ClampToArena(dest);

        Milliseconds flight = FlightTime(statue->GetPosition(), dest);
        ThrowStatue(statueGuid, dest, TossLandingDelay - flight, flight, JumpHeight, true, [this, dest]()
        {
            me->CastSpell(dest, SPELL_TELEKINETIC_TOSS_LANDING, true);
        });
    }

    void DoSuffering()
    {
        Unit* target = SelectTarget(SelectTargetMethod::Random, 0, 40.0f, true);
        if (!target)
            target = SelectTarget(SelectTargetMethod::Random, 0, 100.0f, true);
        if (!target)
            return;

        Talk(Trinity::Containers::SelectRandomContainerElement(SufferingTexts));
        if (Creature* stalker = me->SummonCreature(NPC_SUFFERING_STALKER, target->GetPosition(), TEMPSUMMON_TIMED_DESPAWN, SufferingStalkerLife))
            DoCast(stalker, SPELL_UNLEASHED_SUFFERING);
    }

    void DoTorment()
    {
        DoCastSelf(SPELL_ERUPTING_TORMENT);
        DoCastSelf(SPELL_ERUPTING_TORMENT_AURA, true);
        for (ObjectGuid guid : _statues)
            if (Creature* statue = ObjectAccessor::GetCreature(*me, guid))
                me->CastSpell(statue, SPELL_ERUPTING_TORMENT_AURA, true);

        scheduler.Schedule(TormentCastTime, [this](TaskContext /*context*/)
        {
            me->CastSpell(me->GetPosition(), SPELL_ERUPTING_TORMENT_DAMAGE, true);
            for (ObjectGuid guid : _statues)
                if (Creature* statue = ObjectAccessor::GetCreature(*me, guid))
                    me->CastSpell(statue->GetPosition(), SPELL_ERUPTING_TORMENT_DAMAGE, true);
        });
    }

    // The event waits for any cast in progress (BossAI::UpdateAI), as the capture's 2 s lag behind the health crossing does
    void QueueChorus()
    {
        _chorusActive = true;
        events.ScheduleEvent(EVENT_CHORUS, 0ms);
    }

    void StartChorus()
    {
        ++_chorusCount;
        events.Reset();
        me->SetControlled(true, UNIT_STATE_ROOT);
        DoCastSelf(SPELL_DOOR_OF_SHADOWS);

        scheduler.Schedule(OnslaughtOffset, [this](TaskContext /*context*/) { DoOnslaught(); });
        scheduler.Schedule(RepulsionOffset, [this](TaskContext /*context*/) { DoRepulsion(); });
        scheduler.Schedule(RitualOffset, [this](TaskContext /*context*/) { DoRitual(); });
        scheduler.Schedule(ChorusLength, [this](TaskContext /*context*/) { EndChorus(); });
    }

    void EndChorus()
    {
        _chorusActive = false;
        _beamBlockers.clear();
        me->SetControlled(false, UNIT_STATE_ROOT);

        if (_chorusCount < std::size(ChorusHealthPct) && me->HealthBelowPct(int32(ChorusHealthPct[_chorusCount])))
        {
            QueueChorus();
            return;
        }

        // the loop restarts with the pull offsets: Toss 9 s, Suffering 16 s, Torment 25 s after the last tick (one capture, Medium)
        ScheduleLoop(9s, 16s, 25s);
    }

    // The statues jump next to the boss: front, back, right, left, started 1 s apart as in the capture
    void DoOnslaught()
    {
        float facing = me->GetOrientation();
        for (size_t i = 0; i < _statues.size() && i < std::size(OnslaughtSpells); ++i)
        {
            ObjectGuid guid = _statues[i];
            Creature* statue = ObjectAccessor::GetCreature(*me, guid);
            if (!statue)
                continue;

            float angle = facing + OnslaughtAngles[i];
            Position dest(ArenaCenter.GetPositionX() + OnslaughtDistance * std::cos(angle), ArenaCenter.GetPositionY() + OnslaughtDistance * std::sin(angle), ArenaCenter.GetPositionZ());

            scheduler.Schedule(OnslaughtStagger[i], [this, guid, spellId = OnslaughtSpells[i]](TaskContext /*context*/)
            {
                if (Creature* statue = ObjectAccessor::GetCreature(*me, guid))
                    statue->CastSpell(me, spellId, false);
            });

            ThrowStatue(guid, dest, OnslaughtStagger[i] + OnslaughtCast, FlightTime(statue->GetPosition(), dest), JumpHeight, true);
        }
    }

    // Telekinetic Repulsion (323134 -> 323135 knock back): every statue is thrown 18 yd from the centre in 519 ms
    void DoRepulsion()
    {
        for (ObjectGuid guid : _statues)
        {
            Creature* statue = ObjectAccessor::GetCreature(*me, guid);
            if (!statue)
                continue;

            float angle = ArenaCenter.GetAbsoluteAngle(statue->GetPosition());
            Position dest = ClampToArena(Position(ArenaCenter.GetPositionX() + RepulsionDistance * std::cos(angle), ArenaCenter.GetPositionY() + RepulsionDistance * std::sin(angle), statue->GetPositionZ()));
            ThrowStatue(guid, dest, 0ms, RepulsionFlight, RepulsionHeight, false, [this, dest]()
            {
                me->CastSpell(dest, SPELL_REPULSION_LANDING, true);
            });
        }
    }

    // Ritual of Woe: one statue is the beam target (random: Low). Retail's 335345 periodic dummy ticks every second and 323414 follows
    // 1 s later: ten ticks, the first 5 s after the cast starts. A statue hit means a shadow tick for every player.
    void DoRitual()
    {
        if (_statues.empty())
            return;

        ObjectGuid beamStatue = Trinity::Containers::SelectRandomContainerElement(_statues);
        if (Creature* statue = ObjectAccessor::GetCreature(*me, beamStatue))
            DoCast(statue, SPELL_RITUAL_OF_WOE);

        scheduler.Schedule(RitualTextOffset, [this](TaskContext /*context*/)
        {
            Talk(EMOTE_RITUAL);
            Talk(_chorusCount == 1 ? SAY_RITUAL_FIRST : SAY_RITUAL_SECOND);
        });

        for (uint8 i = 0; i < RitualTicks; ++i)
            scheduler.Schedule(RitualFirstTick + std::chrono::seconds(i), [this, beamStatue](TaskContext /*context*/) { RitualTick(beamStatue); });
    }

    // A player standing in the beam takes the tick alone (Journal text; no capture of it, Low), otherwise the statue pulses
    void RitualTick(ObjectGuid beamStatue)
    {
        Creature* statue = ObjectAccessor::GetCreature(*me, beamStatue);
        if (!statue)
            return;

        _beamBlockers.clear();
        Position const from = me->GetPosition();
        Position const to = statue->GetPosition();
        float const dx = to.GetPositionX() - from.GetPositionX();
        float const dy = to.GetPositionY() - from.GetPositionY();
        float const lengthSq = dx * dx + dy * dy;
        for (MapReference const& ref : me->GetMap()->GetPlayers())
        {
            Player* player = ref.GetSource();
            if (!player || !player->IsAlive() || player->IsGameMaster() || lengthSq < 0.01f)
                continue;

            float const t = std::clamp(((player->GetPositionX() - from.GetPositionX()) * dx + (player->GetPositionY() - from.GetPositionY()) * dy) / lengthSq, 0.0f, 1.0f);
            float const px = from.GetPositionX() + t * dx - player->GetPositionX();
            float const py = from.GetPositionY() + t * dy - player->GetPositionY();
            if (px * px + py * py <= BeamHalfWidth * BeamHalfWidth)
                _beamBlockers.push_back(player->GetGUID());
        }

        me->CastSpell(to, SPELL_RITUAL_OF_WOE_TICK, true);
    }

    std::vector<ObjectGuid> _statues;
    std::vector<ObjectGuid> _beamBlockers;
    uint8 _chorusCount = 0;
    bool _chorusActive = false;
    uint8 _tossCount = 0;
    uint8 _sufferingCount = 0;
    uint8 _tormentCount = 0;
};

// 329104 - Door of Shadows: the teleport target is the arena centre of the capture (no Onslaught stalker stands there live)
class spell_hoa_chamberlain_door_of_shadows : public SpellScript
{
    void HandleTeleport(SpellEffIndex effIndex)
    {
        PreventHitDefaultEffect(effIndex);
        if (Unit* target = GetHitUnit())
            target->NearTeleportTo(ArenaCenter.GetPositionX(), ArenaCenter.GetPositionY(), ArenaCenter.GetPositionZ(), target->GetOrientation(), true);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_hoa_chamberlain_door_of_shadows::HandleTeleport, EFFECT_0, SPELL_EFFECT_TELEPORT_UNITS);
    }
};

// 323414 - Ritual of Woe: with a player in the beam the tick hits that player only
class spell_hoa_chamberlain_ritual_of_woe_tick : public SpellScript
{
    void FilterTargets(std::list<WorldObject*>& targets)
    {
        Unit* caster = GetCaster();
        if (!caster)
            return;

        boss_lord_chamberlain* ai = dynamic_cast<boss_lord_chamberlain*>(caster->GetAI());
        if (!ai)
            return;

        std::vector<ObjectGuid> const& blockers = ai->GetBeamBlockers();
        if (blockers.empty())
            return;

        targets.remove_if([&blockers](WorldObject const* target)
        {
            return std::ranges::find(blockers, target->GetGUID()) == blockers.end();
        });
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_hoa_chamberlain_ritual_of_woe_tick::FilterTargets, EFFECT_0, TARGET_UNIT_DEST_AREA_ENEMY);
    }
};

// 1252635 - Telekinetic Collision (areatrigger create properties 39432, AT 40921): a thrown statue damages and knocks back the players it flies through
struct at_hoa_chamberlain_collision : AreaTriggerAI
{
    using AreaTriggerAI::AreaTriggerAI;

    void OnUnitEnter(Unit* unit) override
    {
        if (!unit->IsPlayer())
            return;

        if (Unit* caster = at->GetCaster())
            caster->CastSpell(unit, SPELL_TELEKINETIC_COLLISION, true);
    }
};

void AddSC_boss_lord_chamberlain()
{
    RegisterHallsOfAtonementCreatureAI(boss_lord_chamberlain);
    RegisterCreatureAI(npc_hoa_chamberlain_statue);
    RegisterSpellScript(spell_hoa_chamberlain_door_of_shadows);
    RegisterSpellScript(spell_hoa_chamberlain_ritual_of_woe_tick);
    RegisterAreaTriggerAI(at_hoa_chamberlain_collision);
}
