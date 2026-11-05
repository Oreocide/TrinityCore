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
 * Timings: oct27c sniff (one Normal kill) and Warcraft Logs (20 launch-era kills per set),
 * see trinitycore-tracking/nyalotha-encounters/nyalotha_wcl.md.
 * Not modelled (no source): Gift of the Void pickup and Annihilation on cultists,
 * Terrifying Presence and Heart of Darkness fear (area triggers 18278/18279 have no
 * create properties), Shadow of Vexiona (Mythic), Iron-Willed Enforcer and Sinister
 * Soulcarver waves (not summoned in the Normal sniff). Oreocide/TrinityCore #46-#49.
 */

#include "AreaTrigger.h"
#include "AreaTriggerAI.h"
#include "Containers.h"
#include "Conversation.h"
#include "G3D/Vector3.h"
#include "InstanceScript.h"
#include "MotionMaster.h"
#include "MoveSplineInit.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "SpellAuraEffects.h"
#include "SpellScript.h"
#include "TemporarySummon.h"
#include "nyalotha.h"

enum VexionaSpells
{
    // Stage One: Cult of the Void
    SPELL_TWILIGHT_BREATH                = 307020,
    SPELL_DESPAIR                        = 307359,
    SPELL_ENCROACHING_SHADOWS            = 307314,
    SPELL_ENCROACHING_SHADOWS_TARGETED   = 307280,
    SPELL_ENCROACHING_SHADOWS_DAMAGE     = 307317,
    SPELL_ENCROACHING_SHADOWS_RESIDUE    = 307318,
    SPELL_DARK_GATEWAY                   = 307043,
    SPELL_DARK_GATEWAY_DAMAGE            = 307057,

    // Stage Two: Death From Above
    SPELL_POWER_OF_THE_CHOSEN            = 307075,
    SPELL_TWILIGHT_DECIMATOR             = 315762,
    SPELL_TWILIGHT_DECIMATOR_AURA        = 307206,
    SPELL_TWILIGHT_DECIMATOR_AREATRIGGER = 307212,

    // Stage Three: The Void Unleashed
    SPELL_THE_VOID_UNLEASHED             = 307453,
    SPELL_THE_VOID_UNLEASHED_AREATRIGGER = 307491,
    SPELL_EMPOWERED_VOID_CORRUPTION      = 307828,
    SPELL_HEART_OF_DARKNESS              = 307639,
    SPELL_DESOLATION                     = 310324,

    // Adds
    SPELL_ANNIHILATION                   = 307403,
    SPELL_VOID_BOLT                      = 307177,
    SPELL_GIFT_OF_THE_VOID_MAINTAIN      = 306980
};

enum VexionaEvents
{
    EVENT_ENERGY = 1,
    EVENT_TWILIGHT_BREATH,
    EVENT_DESPAIR,
    EVENT_ENCROACHING_SHADOWS,
    EVENT_DARK_GATEWAY,
    EVENT_POWER_OF_THE_CHOSEN,
    EVENT_EMPOWER_CULTIST,
    EVENT_DECIMATOR_TICK,
    EVENT_VOID_UNLEASHED,
    EVENT_HEART_OF_DARKNESS,
    EVENT_DESOLATION
};

enum VexionaPoints
{
    POINT_HOVER = 1,
    POINT_PLATFORM_DESCEND,
    POINT_PLATFORM_LAND,
    POINT_TAKEOFF,
    POINT_FLIGHT_HIGH,
    POINT_LANE_START,
    POINT_PASS_END,
    POINT_FLIGHT_DESCEND,
    POINT_FLIGHT_LAND
};

enum VexionaMisc
{
    CONVERSATION_VEXIONA_APPROACH        = 13079,
    ANIM_TIER_TRANSITION_LAND            = 57,
    SUMMON_GROUP_VOID_PORTALS            = 1,
    MAX_DECIMATOR_PASSES                 = 3
};

// Pre-encounter (oct27c sniff, 09:33:47 break-off and 09:37:45 landing)
constexpr Position VexionaHoverPosition          = { -535.67f, -227.75f, -142.91f };
constexpr Position VexionaPlatformDescendPosition = { -533.48f, -256.17f, -157.75f };
constexpr Position VexionaPlatformLandPosition   = { -533.48f, -256.17f, -168.41f, 4.71f };

// Stage Two flight (sniff, pull + 78 s to + 131 s)
constexpr Position VexionaWestHighPosition       = { -663.11f, -288.24f, -82.31f };
constexpr Position VexionaWestLaneStartPosition  = { -633.09f, -288.41f, -123.0f, 0.0f };
constexpr Position VexionaEastLaneStartPositions[] =
{
    { -434.08f, -232.82f, -123.0f, 3.14f }, // north lane
    { -431.28f, -311.96f, -123.0f, 3.14f }  // south lane
};
constexpr Position VexionaFlightDescendPosition  = { -531.93f, -275.20f, -158.76f };
constexpr Position VexionaFlightLandPosition     = { -531.93f, -275.20f, -168.40f };

constexpr Position VexionaPassWestToEast[] =
{
    { -605.59f, -285.96f, -134.77f }, { -583.90f, -283.69f, -140.28f }, { -553.31f, -281.65f, -140.28f }, { -518.82f, -278.53f, -140.28f },
    { -477.43f, -275.69f, -135.64f }, { -454.74f, -275.17f, -123.97f }, { -422.40f, -272.49f, -98.90f },  { -403.75f, -271.15f, -82.31f }
};
constexpr Position VexionaPassEastToWestNorth[] =
{
    { -464.08f, -236.51f, -137.31f }, { -485.76f, -239.07f, -140.28f }, { -516.33f, -241.78f, -140.28f }, { -550.77f, -244.28f, -140.28f },
    { -592.12f, -247.48f, -135.64f }, { -614.78f, -249.91f, -125.03f }, { -651.45f, -258.91f, -97.52f },  { -663.11f, -288.24f, -82.31f }
};
constexpr Position VexionaPassEastToWestSouth[] =
{
    { -460.63f, -314.60f, -134.61f }, { -482.39f, -316.39f, -140.28f }, { -513.02f, -317.99f, -140.28f }, { -547.57f, -320.39f, -140.28f },
    { -589.01f, -322.12f, -135.64f }, { -611.70f, -322.08f, -124.84f }, { -643.97f, -314.45f, -95.98f },  { -663.11f, -288.24f, -82.31f }
};

// Cultist Portal spots of the sniffed kill
constexpr Position VexionaCultistPortalPositions[] =
{
    { -544.09f, -304.86f, -168.32f }, { -538.97f, -244.62f, -168.32f }, { -564.05f, -252.00f, -168.32f },
    { -550.98f, -309.42f, -168.32f }, { -500.05f, -251.25f, -168.24f }, { -539.05f, -314.16f, -168.32f }
};

// Fanatical Cultists and Spellbound Ritualists per portal, in sniffed order
constexpr std::pair<uint8, uint8> VexionaCultistWaves[] = { { 4, 0 }, { 4, 1 }, { 4, 1 }, { 3, 2 }, { 4, 1 }, { 3, 2 } };

constexpr float VexionaPlatformFloorZ = -168.35f;

// 157354 - Vexiona
struct boss_vexiona : public BossAI
{
    boss_vexiona(Creature* creature) : BossAI(creature, DATA_VEXIONA), _brokeOff(false), _landed(false), _flying(false),
        _stageThree(false), _stageThreePending(false), _decimatorPasses(0), _cultistWave(0) { }

    void JustAppeared() override
    {
        BossAI::JustAppeared();

        // The platform pack stays dead, so after a wipe or a restart she waits on the platform
        if (instance->GetData(DATA_VEXIONA_PLATFORM_CLEARED))
            SetGroundedOnPlatform();
    }

    void Reset() override
    {
        _Reset();
        _flying = false;
        _stageThree = false;
        _stageThreePending = false;
        _decimatorPasses = 0;
        _cultistWave = 0;

        me->SetPowerType(POWER_ENERGY);
        me->SetMaxPower(POWER_ENERGY, 100);
        me->SetPower(POWER_ENERGY, 0);
        me->RemoveUnitFlag2(UNIT_FLAG2_REGENERATE_POWER);
    }

    void DoAction(int32 action) override
    {
        switch (action)
        {
            case ACTION_VEXIONA_BREAK_OFF:
                BreakOff(nullptr);
                break;
            case ACTION_VEXIONA_PLATFORM_CLEARED:
                if (_landed)
                    break;
                if (!_brokeOff)
                    BreakOff(nullptr);
                me->GetMotionMaster()->MovePoint(POINT_PLATFORM_DESCEND, VexionaPlatformDescendPosition, false);
                break;
            default:
                break;
        }
    }

    void SetGUID(ObjectGuid const& guid, int32 action) override
    {
        if (action == ACTION_VEXIONA_BREAK_OFF)
            BreakOff(ObjectAccessor::GetPlayer(*me, guid));
    }

    void MovementInform(uint32 type, uint32 id) override
    {
        if (type != POINT_MOTION_TYPE && type != EFFECT_MOTION_TYPE)
            return;

        switch (id)
        {
            case POINT_HOVER:
                me->SetFacingTo(VexionaPlatformLandPosition.GetOrientation());
                break;
            case POINT_PLATFORM_DESCEND:
                me->GetMotionMaster()->MoveLand(POINT_PLATFORM_LAND, VexionaPlatformLandPosition, ANIM_TIER_TRANSITION_LAND);
                break;
            case POINT_PLATFORM_LAND:
                SetGroundedOnPlatform();
                me->HandleEmoteCommand(EMOTE_ONESHOT_BATTLE_ROAR);
                break;
            case POINT_TAKEOFF:
                events.ScheduleEvent(EVENT_POWER_OF_THE_CHOSEN, 1s);
                break;
            case POINT_FLIGHT_HIGH:
                me->GetMotionMaster()->MovePoint(POINT_LANE_START, VexionaWestLaneStartPosition, false);
                break;
            case POINT_LANE_START:
                me->SetFacingTo(_decimatorPasses % 2 ? float(M_PI) : 0.0f);
                DoCastSelf(SPELL_TWILIGHT_DECIMATOR);
                break;
            case POINT_PASS_END:
                events.CancelEvent(EVENT_DECIMATOR_TICK);
                me->RemoveAurasDueToSpell(SPELL_TWILIGHT_DECIMATOR_AURA);
                if (++_decimatorPasses < MAX_DECIMATOR_PASSES)
                {
                    // passes alternate west->east and east->west; from the east she picks the north or south lane
                    if (_decimatorPasses % 2)
                        me->GetMotionMaster()->MovePoint(POINT_LANE_START, Trinity::Containers::SelectRandomContainerElement(VexionaEastLaneStartPositions), false);
                    else
                        me->GetMotionMaster()->MovePoint(POINT_LANE_START, VexionaWestLaneStartPosition, false);
                }
                else
                    me->GetMotionMaster()->MovePoint(POINT_FLIGHT_DESCEND, VexionaFlightDescendPosition, false);
                break;
            case POINT_FLIGHT_DESCEND:
                me->GetMotionMaster()->MoveLand(POINT_FLIGHT_LAND, VexionaFlightLandPosition, ANIM_TIER_TRANSITION_LAND);
                break;
            case POINT_FLIGHT_LAND:
                FinishFlight();
                break;
            default:
                break;
        }
    }

    void OnSpellCast(SpellInfo const* spell) override
    {
        if (spell->Id != SPELL_TWILIGHT_DECIMATOR)
            return;

        DoCastSelf(SPELL_TWILIGHT_DECIMATOR_AURA, true);
        events.ScheduleEvent(EVENT_DECIMATOR_TICK, 0s);

        Position const* path = VexionaPassWestToEast;
        if (_decimatorPasses % 2)
            path = me->GetPositionY() > -270.0f ? VexionaPassEastToWestNorth : VexionaPassEastToWestSouth;
        LaunchPass(path);
    }

    void JustEngagedWith(Unit* who) override
    {
        BossAI::JustEngagedWith(who);
        instance->SendEncounterUnit(ENCOUNTER_FRAME_ENGAGE, me, 1);
        ScheduleStageOne(false);
    }

    void DamageTaken(Unit* /*attacker*/, uint32& damage, DamageEffectType /*damageType*/, SpellInfo const* /*spellInfo = nullptr*/) override
    {
        if (_stageThree || _stageThreePending || !me->HealthBelowPctDamaged(40, damage))
            return;

        // The Void Unleashed (307453 s1 = 40%); WCL: cast at 37-38% in all sampled pulls. A flight finishes first.
        _stageThreePending = true;
        if (!_flying)
            StartStageThree();
    }

    void JustSummoned(Creature* summon) override
    {
        summons.Summon(summon);

        switch (summon->GetEntry())
        {
            case NPC_CULTIST_PORTAL:
            case NPC_VOID_PORTAL:
            case NPC_GIFT_OF_THE_VOID:
                break;
            default:
                if (me->IsEngaged())
                    DoZoneInCombat(summon);
                break;
        }
    }

    void JustDied(Unit* /*killer*/) override
    {
        _JustDied();
        instance->SendEncounterUnit(ENCOUNTER_FRAME_DISENGAGE, me);
    }

    void EnterEvadeMode(EvadeReason /*why*/) override
    {
        instance->SendEncounterUnit(ENCOUNTER_FRAME_DISENGAGE, me);

        summons.DespawnAll();
        _EnterEvadeMode();
        _DespawnAtEvade();
    }

    void UpdateAI(uint32 diff) override
    {
        scheduler.Update(diff);

        if (!UpdateVictim())
            return;

        events.Update(diff);

        if (me->HasUnitState(UNIT_STATE_CASTING))
            return;

        while (uint32 eventId = events.ExecuteEvent())
        {
            switch (eventId)
            {
                case EVENT_ENERGY:
                    me->ModifyPower(POWER_ENERGY, 1);
                    if (me->GetPower(POWER_ENERGY) >= 100)
                        StartFlight();
                    else
                        events.Repeat(780ms);
                    break;
                case EVENT_TWILIGHT_BREATH:
                    DoCastVictim(SPELL_TWILIGHT_BREATH);
                    events.Repeat(17s);
                    break;
                case EVENT_DESPAIR:
                    DoCastVictim(SPELL_DESPAIR);
                    events.Repeat(35s);
                    break;
                case EVENT_ENCROACHING_SHADOWS:
                    if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0, [this](Unit* target)
                    {
                        return target->IsPlayer() && target != me->GetVictim() && !target->HasAura(SPELL_ENCROACHING_SHADOWS_TARGETED);
                    }))
                    {
                        DoCast(target, SPELL_ENCROACHING_SHADOWS, true);
                        DoCast(target, SPELL_ENCROACHING_SHADOWS_TARGETED, true);
                    }
                    events.Repeat(_stageThree ? 16s : 15s);
                    break;
                case EVENT_DARK_GATEWAY:
                    OpenDarkGateway();
                    events.Repeat(34s);
                    break;
                case EVENT_POWER_OF_THE_CHOSEN:
                    if (Creature* cultist = me->FindNearestCreature(NPC_FANATICAL_CULTIST, 100.0f))
                    {
                        DoCast(cultist, SPELL_POWER_OF_THE_CHOSEN, true);
                        _empoweredCultist = cultist->GetGUID();
                    }
                    else
                        _empoweredCultist.Clear();
                    events.ScheduleEvent(EVENT_EMPOWER_CULTIST, 1s);
                    me->GetMotionMaster()->MovePoint(POINT_FLIGHT_HIGH, VexionaWestHighPosition, false);
                    break;
                case EVENT_EMPOWER_CULTIST:
                {
                    // the empowered cultist becomes a Void Ascendant (sniff: the Ascendant appears 1 s after Power of the Chosen)
                    Position pos = Trinity::Containers::SelectRandomContainerElement(VexionaCultistPortalPositions);
                    if (Creature* cultist = ObjectAccessor::GetCreature(*me, _empoweredCultist))
                    {
                        pos = cultist->GetPosition();
                        cultist->DespawnOrUnsummon();
                    }
                    me->SummonCreature(NPC_VOID_ASCENDANT, pos, TEMPSUMMON_CORPSE_TIMED_DESPAWN, 10s);
                    break;
                }
                case EVENT_DECIMATOR_TICK:
                {
                    // Twilight Decimator area triggers (16 x 40 yd) are left on the platform floor under her flight line
                    Position pos = me->GetPosition();
                    pos.m_positionZ = VexionaPlatformFloorZ;
                    me->CastSpell(pos, SPELL_TWILIGHT_DECIMATOR_AREATRIGGER, true);
                    events.Repeat(350ms);
                    break;
                }
                case EVENT_VOID_UNLEASHED:
                    DoCastSelf(SPELL_THE_VOID_UNLEASHED_AREATRIGGER, true);
                    DoCastSelf(SPELL_EMPOWERED_VOID_CORRUPTION, true);
                    me->SummonCreatureGroup(SUMMON_GROUP_VOID_PORTALS);
                    break;
                case EVENT_HEART_OF_DARKNESS:
                    DoCastSelf(SPELL_HEART_OF_DARKNESS);
                    events.Repeat(32s);
                    break;
                case EVENT_DESOLATION:
                    if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0, 0.0f, true))
                        DoCast(target, SPELL_DESOLATION);
                    events.Repeat(33s);
                    break;
                default:
                    break;
            }

            if (me->HasUnitState(UNIT_STATE_CASTING))
                return;
        }
    }

private:
    void BreakOff(Player* player)
    {
        if (_brokeOff || _landed)
            return;

        _brokeOff = true;
        if (player)
            Conversation::CreateConversation(CONVERSATION_VEXIONA_APPROACH, player, *player, ObjectGuid::Empty);

        // leave the patrol loop for good (Clear() keeps the default slot, MoveIdle replaces it)
        me->SetDefaultMovementType(IDLE_MOTION_TYPE);
        me->GetMotionMaster()->Clear();
        me->GetMotionMaster()->MoveIdle();
        me->GetMotionMaster()->MovePoint(POINT_HOVER, VexionaHoverPosition, false);
    }

    void SetGroundedOnPlatform()
    {
        _brokeOff = true;
        _landed = true;

        me->SetDefaultMovementType(IDLE_MOTION_TYPE);
        me->GetMotionMaster()->Clear();
        me->GetMotionMaster()->MoveIdle();
        if (me->GetExactDist(VexionaPlatformLandPosition) > 5.0f)
            me->NearTeleportTo(VexionaPlatformLandPosition);
        me->SetFloating(false);
        me->SetAnimTier(AnimTier::Ground);
        me->SetHomePosition(VexionaPlatformLandPosition);
        me->SetImmuneToPC(false);
    }

    void ScheduleStageOne(bool afterFlight)
    {
        events.ScheduleEvent(EVENT_ENERGY, 780ms);
        // pull: WCL Twilight Breath 5.8 s, Despair 9.6-10.7 s, sniff Encroaching Shadows 15 s, Dark Gateway 33 s;
        // after a landing (sniff, landed at 131.5 s): Dark Gateway +5.7 s, Twilight Breath +8.3 s, Despair +12.7 s
        events.ScheduleEvent(EVENT_TWILIGHT_BREATH, afterFlight ? 8s : 6s);
        events.ScheduleEvent(EVENT_DESPAIR, afterFlight ? 13s : 11s);
        events.ScheduleEvent(EVENT_ENCROACHING_SHADOWS, 15s);
        events.ScheduleEvent(EVENT_DARK_GATEWAY, afterFlight ? 6s : 33s);
    }

    void OpenDarkGateway()
    {
        Position const& pos = Trinity::Containers::SelectRandomContainerElement(VexionaCultistPortalPositions);
        DoCastSelf(SPELL_DARK_GATEWAY, true);
        me->CastSpell(pos, SPELL_DARK_GATEWAY_DAMAGE, true);
        me->SummonCreature(NPC_CULTIST_PORTAL, pos, TEMPSUMMON_TIMED_DESPAWN, 8s);

        // sniff: cultists step out 3 to 5 s after the portal appears
        std::pair<uint8, uint8> const& wave = VexionaCultistWaves[_cultistWave++ % std::size(VexionaCultistWaves)];
        uint8 index = 0;
        for (uint8 i = 0; i < wave.first + wave.second; ++i)
        {
            uint32 entry = i < wave.first ? NPC_FANATICAL_CULTIST : NPC_SPELLBOUND_RITUALIST;
            scheduler.Schedule(Milliseconds(3000 + 500 * index++), [this, pos, entry](TaskContext /*context*/)
            {
                me->SummonCreature(entry, pos, TEMPSUMMON_CORPSE_TIMED_DESPAWN, 10s);
            });
        }
    }

    void StartFlight()
    {
        _flying = true;
        _decimatorPasses = 0;
        events.Reset();

        me->SetReactState(REACT_PASSIVE);
        me->AttackStop();
        me->SetFloating(true);

        Position takeoff = me->GetPosition();
        takeoff.m_positionZ += 15.0f;
        me->GetMotionMaster()->Clear();
        me->GetMotionMaster()->MoveTakeoff(POINT_TAKEOFF, takeoff);
    }

    void LaunchPass(Position const* path)
    {
        me->GetMotionMaster()->LaunchMoveSpline([this, path](Movement::MoveSplineInit& init)
        {
            Movement::PointsArray points;
            points.reserve(9);
            points.emplace_back(me->GetPositionX(), me->GetPositionY(), me->GetPositionZ()); // replaced by the current position
            for (uint8 i = 0; i < 8; ++i)
                points.emplace_back(path[i].GetPositionX(), path[i].GetPositionY(), path[i].GetPositionZ());

            init.MovebyPath(points);
            init.SetFly();
            init.SetSmooth();
            init.SetUncompressed();
            init.SetVelocity(45.7f);
        }, POINT_PASS_END, MOTION_PRIORITY_NORMAL, POINT_MOTION_TYPE);
    }

    void FinishFlight()
    {
        _flying = false;
        me->SetFloating(false);
        me->SetAnimTier(AnimTier::Ground);
        me->SetPower(POWER_ENERGY, 0);
        me->SetReactState(REACT_AGGRESSIVE);

        if (_stageThreePending)
            StartStageThree();
        else
            ScheduleStageOne(true);
    }

    void StartStageThree()
    {
        _stageThree = true;
        _stageThreePending = false;
        events.Reset();

        DoCastSelf(SPELL_THE_VOID_UNLEASHED);
        // sniff: the field and portals at +5 s; Encroaching Shadows +27.4 s, then about every 16 s;
        // WCL Heroic: Heart of Darkness +22.6 s then about 32 s, Desolation +38.3 s then about 33 s
        events.ScheduleEvent(EVENT_VOID_UNLEASHED, 5s);
        events.ScheduleEvent(EVENT_TWILIGHT_BREATH, 12s);
        events.ScheduleEvent(EVENT_HEART_OF_DARKNESS, 23s);
        events.ScheduleEvent(EVENT_ENCROACHING_SHADOWS, 27s);
        // Desolation is in the Heroic WCL kills but not in the Normal sniff
        if (IsHeroicOrHigher())
            events.ScheduleEvent(EVENT_DESOLATION, 38s);
    }

    bool _brokeOff;
    bool _landed;
    bool _flying;
    bool _stageThree;
    bool _stageThreePending;
    uint8 _decimatorPasses;
    uint8 _cultistWave;
    ObjectGuid _empoweredCultist;
};

// 157467, 162719 - Void Ascendant
struct npc_vexiona_void_ascendant : public ScriptedAI
{
    npc_vexiona_void_ascendant(Creature* creature) : ScriptedAI(creature) { }

    void JustEngagedWith(Unit* /*who*/) override
    {
        // WCL: 13.2 s after the pull for the Ascendant that stays from the platform, +16 s for one made in Stage Two, then about 15 s
        _events.ScheduleEvent(EVENT_ENERGY, 13s, 16s);
    }

    void JustDied(Unit* /*killer*/) override
    {
        // Void Ascendants drop Gift of the Void (Journal)
        if (TempSummon* summon = me->ToTempSummon())
            if (Unit* summoner = summon->GetSummonerUnit())
            {
                summoner->SummonCreature(NPC_GIFT_OF_THE_VOID, me->GetPosition(), TEMPSUMMON_MANUAL_DESPAWN);
                return;
            }

        if (InstanceScript* instance = me->GetInstanceScript())
            if (Creature* vexiona = instance->GetCreature(DATA_VEXIONA))
                if (vexiona->IsEngaged())
                    vexiona->SummonCreature(NPC_GIFT_OF_THE_VOID, me->GetPosition(), TEMPSUMMON_MANUAL_DESPAWN);
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim())
            return;

        _events.Update(diff);

        if (me->HasUnitState(UNIT_STATE_CASTING))
            return;

        if (_events.ExecuteEvent() == EVENT_ENERGY)
        {
            DoCastSelf(SPELL_ANNIHILATION);
            _events.Repeat(15s);
        }
    }

private:
    EventMap _events;
};

// 157450 - Spellbound Ritualist
struct npc_vexiona_spellbound_ritualist : public ScriptedAI
{
    npc_vexiona_spellbound_ritualist(Creature* creature) : ScriptedAI(creature) { }

    void JustEngagedWith(Unit* /*who*/) override
    {
        _events.ScheduleEvent(EVENT_ENERGY, 1s, 2s);
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim())
            return;

        _events.Update(diff);

        if (me->HasUnitState(UNIT_STATE_CASTING))
            return;

        // WCL: Void Bolt about every 2 to 5 s per Ritualist
        if (_events.ExecuteEvent() == EVENT_ENERGY)
        {
            DoCastVictim(SPELL_VOID_BOLT);
            _events.Repeat(2500ms, 3500ms);
        }
    }

private:
    EventMap _events;
};

// 157422 - Gift of the Void
struct npc_vexiona_gift_of_the_void : public ScriptedAI
{
    npc_vexiona_gift_of_the_void(Creature* creature) : ScriptedAI(creature) { }

    void JustAppeared() override
    {
        DoCastSelf(SPELL_GIFT_OF_THE_VOID_MAINTAIN, true);
    }

    void UpdateAI(uint32 /*diff*/) override { }
};

// 9400203 - custom server-side areatrigger on the approach to Vexiona's platform
struct at_vexiona_approach : AreaTriggerAI
{
    at_vexiona_approach(AreaTrigger* areatrigger) : AreaTriggerAI(areatrigger) { }

    void OnUnitEnter(Unit* unit) override
    {
        Player* player = unit->ToPlayer();
        if (!player || player->IsGameMaster())
            return;

        InstanceScript* instance = at->GetInstanceScript();
        if (!instance)
            return;

        if (Creature* vexiona = instance->GetCreature(DATA_VEXIONA))
            if (vexiona->IsAlive() && vexiona->IsAIEnabled())
                vexiona->AI()->SetGUID(player->GetGUID(), ACTION_VEXIONA_BREAK_OFF);
    }
};

// 307314 - Encroaching Shadows
class spell_vexiona_encroaching_shadows : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_ENCROACHING_SHADOWS_DAMAGE, SPELL_ENCROACHING_SHADOWS_RESIDUE });
    }

    // the triggered 307316 is a dummy; the sniff shows its damage 307317 on the target and Shadowy Residue left where it stood
    void HandlePeriodic(AuraEffect const* /*aurEff*/)
    {
        PreventDefaultAction();

        Unit* caster = GetCaster();
        if (!caster)
            return;

        Unit* target = GetTarget();
        caster->CastSpell(target, SPELL_ENCROACHING_SHADOWS_DAMAGE, true);
        caster->CastSpell(target->GetPosition(), SPELL_ENCROACHING_SHADOWS_RESIDUE, true);
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_vexiona_encroaching_shadows::HandlePeriodic, EFFECT_0, SPELL_AURA_PERIODIC_TRIGGER_SPELL);
    }
};

void AddSC_boss_vexiona()
{
    RegisterNyalothaCreatureAI(boss_vexiona);
    RegisterNyalothaCreatureAI(npc_vexiona_void_ascendant);
    RegisterNyalothaCreatureAI(npc_vexiona_spellbound_ritualist);
    RegisterNyalothaCreatureAI(npc_vexiona_gift_of_the_void);
    RegisterAreaTriggerAI(at_vexiona_approach);
    RegisterSpellScript(spell_vexiona_encroaching_shadows);
}
