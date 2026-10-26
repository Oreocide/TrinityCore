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

// Ported from The-Cataclysm-Preservation-Project/TrinityCore
// (src/server/scripts/EasternKingdoms/ThroneOfTheTides/instance_throne_of_the_tides.cpp),
// a fork frozen at Cataclysm client build 4.3.4.15595. Adapted for this
// core's modern API and this tree's existing enum-key convention (BOSS_* as
// boss-index/DungeonEncounter key, matching our pre-existing boss_commander_ulthok.cpp,
// rather than the reference repo's DATA_LADY_NAZJAR/DATA_COMMANDER_ULTHOK/
// DATA_OZUMAT convention for the same values).
//
// API changes from the reference repo's 4.3.4-era code:
//   - Creature::RemoveFlag(UNIT_FIELD_FLAGS, x)/SetFlag(...) -> RemoveUnitFlag(x)/
//     SetUnitFlag(x); Creature::SetFlag(UNIT_NPC_FLAGS, x) -> SetNpcFlag(x).
//     GameObject keeps the plain RemoveFlag(x)/SetFlag(x) (no field-name arg).
//   - SetUInt32Value(UNIT_NPC_EMOTESTATE, x) -> SetEmoteState(x).
//   - MotionMaster::MoveJump(pos, speed, maxHeight) -> MoveJump(pointId, pos,
//     speed, {}, maxHeight) - now takes a leading movement-point id.
//   - EventMap::CalculateTime(int32) -> CalculateTime(Milliseconds(...)) -
//     now takes a strongly-typed duration, not a bare integer.
//   - InstanceScript's old virtual Load(char const*) override and the
//     WriteSaveDataMore/ReadSaveDataMore raw-stream serialization hooks are
//     gone. Post-load logic now goes in AfterDataLoad() (called automatically
//     after saved data is restored), and a single persisted extra value uses
//     PersistentInstanceScriptValue<T> as a member (auto-registers itself in
//     the constructor init-list, reads/writes transparently as T via
//     implicit conversion/assignment - no manual (de)serialization needed).
// Not yet cross-checked against a Warcraft Logs parse per the standing
// per-encounter workflow.

#include "ScriptMgr.h"
#include "throne_of_the_tides.h"
#include "Creature.h"
#include "CreatureAI.h"
#include "EventMap.h"
#include "GameObject.h"
#include "InstanceScript.h"
#include "Vehicle.h"
#include "MoveSpline.h"
#include "MotionMaster.h"
#include "Map.h"
#include "Player.h"
#include "TemporarySummon.h"

static constexpr ObjectData const creatureData[] =
{
    { NPC_LADY_NAZJAR,                      BOSS_LADY_NAZJAR                        },
    { NPC_COMMANDER_ULTHOK,                 BOSS_COMMANDER_ULTHOK                   },
    { NPC_MINDBENDER_GURSHA,                BOSS_MINDBENDER_GURSHA                  },
    { NPC_OZUMAT,                           BOSS_OZUMAT                             },
    { NPC_LADY_NAZJAR_INTRO,                DATA_LADY_NAZJAR_GAUNTLET               },
    { NPC_OZUMAT_VEHICLE_BIG,               DATA_OZUMAT_VEHICLE_BIG                 },
    { NPC_OZUMAT_VEHICLE,                   DATA_OZUMAT_VEHICLE                     },
    { NPC_NEPTULON,                         DATA_NEPTULON                           },
    { NPC_OZUMAT_INTRO,                     DATA_OZUMAT_PASSENGER                   },
    { NPC_THRONE_OF_THE_TIDES_TELEPORTER_1, DATA_THRONE_OF_THE_TIDES_TELEPORTER_1   },
    { NPC_THRONE_OF_THE_TIDES_TELEPORTER_2, DATA_THRONE_OF_THE_TIDES_TELEPORTER_2   },
};

static constexpr ObjectData const gameobjectData[] =
{
    { GO_DOODAD_ABYSSAL_MAW_DOOR_1,             DATA_ABYSSAL_MAW_DOOR_1                 },
    { GO_DOODAD_ABYSSAL_MAW_DOOR_2,             DATA_ABYSSAL_MAW_DOOR_2                 },
    { GO_DOODAD_ABYSSAL_MAW_DOOR_4,             DATA_ABYSSAL_MAW_DOOR_4                 },
    { GO_THRONE_OF_THE_TIDES_DEFENSE_SYSTEM,    DATA_THRONE_OF_THE_TIDES_DEFENSE_SYSTEM },
    { GO_DOODAD_INVISIBLE_DOOR_1,               DATA_INVISIBLE_DOOR_1                   },
    { GO_DOODAD_INVISIBLE_DOOR_2,               DATA_INVISIBLE_DOOR_2                   },
    { GO_DOODAD_ABYSSAL_LEVIATHAN_TENTACLE_1,   DATA_LEVIATHAN_TENTACLE_1               },
    { GO_DOODAD_ABYSSAL_LEVIATHAN_TENTACLE_2,   DATA_LEVIATHAN_TENTACLE_2               },
    { GO_DOODAD_ABYSSAL_CORAL_CHUNK_CEILING,    DATA_ABYSSAL_CORAL_CHUNK                },
};

static constexpr DoorData const doorData[] =
{
    { GO_DOODAD_ABYSSAL_MAW_DOOR_1,     BOSS_COMMANDER_ULTHOK,  EncounterDoorBehavior::OpenWhenNotInProgress },
    { GO_DOODAD_ABYSSAL_MAW_DOOR_2,     BOSS_COMMANDER_ULTHOK,  EncounterDoorBehavior::OpenWhenNotInProgress },
    { GO_DOODAD_ABYSSAL_MAW_DOOR_4,     BOSS_OZUMAT,            EncounterDoorBehavior::OpenWhenNotInProgress },
};

enum TotTInstanceEvents
{
    EVENT_FALLING_ROCKS = 1,
    EVENT_RESPAWN_COMMANDER_ULTHOK,
    EVENT_BREAK_CORAL,
    EVENT_MOVE_COMMANDER_ULTHOK_TO_HOME_POS,
    EVENT_CHECK_DEAD_PLAYERS,
    EVENT_SUMMON_GHURSHA_FLOOR_ADDS
};

enum TotTInstancePoints
{
    POINT_JUMP_THROUGH_WINDOW = 1
};

Position const fallingRocksDummyPos         = { -144.283f,  983.316f,  230.4773f };
Position const shockDefenseDummyPos         = { -188.321f,  805.252f,  384.9963f };
Position const ulthokIntroDummyPos          = { 51.1059f,   802.1563f, 846.4826f };
Position const ladyNazjarGauntletPos        = { -20.19097f, 802.0608f, 807.5371f, 3.141593f };
Position const commanderUlthokIntroPos      = { 51.9911f,   802.6801f, 888.0374f, 6.198466f };
Position const commanderUlthokRespawnPos    = { 51.5573f,   802.717f,  805.7309f, 6.230825f };

// Invader lands from its window jump: drop any suspended MovePoint first (a
// stacked, deactivated Point generator would otherwise reset and fly the
// creature back toward its staging point once the jump ends), then apply the
// landing state once the jump's spline has actually finished.
static void JumpThroughWindow(Creature* creature, Position const jumpPos)
{
    creature->CastSpell(creature, SPELL_WATER_JUMP_THROUGH_WINDOW_VISUAL, true);
    creature->GetMotionMaster()->Clear();
    creature->GetMotionMaster()->MoveJump(POINT_JUMP_THROUGH_WINDOW, jumpPos, 16.0f, {}, 10.0f);
    creature->SetHomePosition(jumpPos);

    ExecuteOnMovementEnd(creature, [creature, facingAngle = jumpPos.GetOrientation()]()
    {
        creature->RemoveAurasDueToSpell(SPELL_WATER_JUMP_THROUGH_WINDOW_VISUAL);
        creature->SetDisableGravity(false);
        creature->SetMovementAnimKitId(0);
        creature->SetFacingTo(facingAngle);
        creature->RemoveUnitFlag(UNIT_FLAG_IMMUNE_TO_PC);
        creature->SetEmoteState(EMOTE_STATE_READY2HL);
        creature->SetReactState(REACT_AGGRESSIVE);
    });
}

class DelayedPathEvent : public BasicEvent
{
public:
    DelayedPathEvent(Creature* owner) : _owner(owner) { }

    bool Execute(uint64 /*time*/, uint32 /*diff*/) override
    {
        _owner->GetMotionMaster()->MovePath(_owner->GetEntry() * 10 + 1, true);
        return true;
    }

private:
    Creature* _owner;
};

class EmoteStateEvent : public BasicEvent
{
public:
    EmoteStateEvent(Creature* owner) : _owner(owner) { }

    bool Execute(uint64 /*time*/, uint32 /*diff*/) override
    {
        if (_owner->GetEntry() == NPC_GILGOBLIN_HUNTER)
            _owner->SetEmoteState(EMOTE_STATE_HOLD_THROWN);
        else
            _owner->SetEmoteState(EMOTE_STATE_READY2HL);
        return true;
    }

private:
    Creature* _owner;
};

class GeyserActivationEvent : public BasicEvent
{
public:
    GeyserActivationEvent(Creature* owner) : _owner(owner) { }

    bool Execute(uint64 /*time*/, uint32 /*diff*/) override
    {
        _owner->RemoveAurasDueToSpell(SPELL_GEYSER_DUMMY);
        _owner->CastSpell(_owner, SPELL_GEYSER_DUMMY_KNOCKBACK);
        return true;
    }

private:
    Creature* _owner;
};

enum TotTInstanceSpells
{
    SPELL_TELEPORTER_ACTIVE_VISUAL = 95296
};

enum TotTSpawnGroups
{
    SPAWN_GROUP_ID_OZUMAT_WING_EVENT = 455
};

class instance_throne_of_the_tides : public InstanceMapScript
{
public:
    instance_throne_of_the_tides() : InstanceMapScript(TotTScriptName, 643) { }

    struct instance_throne_of_the_tides_InstanceMapScript : public InstanceScript
    {
        instance_throne_of_the_tides_InstanceMapScript(InstanceMap* map) : InstanceScript(map), _eventIndex(*this, "EventIndex", EVENT_INDEX_NONE), _killedTaintedSentryCount(0)
        {
            SetHeaders(DataHeader);
            SetBossNumber(EncounterCount);
            LoadObjectData(creatureData, gameobjectData);
            LoadDoorData(doorData);
        }

        void Create() override
        {
            instance->SpawnGroupSpawn(SPAWN_GROUP_ID_OZUMAT_WING_EVENT, true);
        }

        void AfterDataLoad() override
        {
            if (GetBossState(BOSS_OZUMAT) != DONE)
                instance->SpawnGroupSpawn(SPAWN_GROUP_ID_OZUMAT_WING_EVENT, true);

            if (_eventIndex == EVENT_INDEX_ULTHOK_ARRIVED && GetBossState(BOSS_COMMANDER_ULTHOK) != DONE)
                instance->SummonCreature(NPC_COMMANDER_ULTHOK, commanderUlthokRespawnPos);
        }

        bool SetBossState(uint32 type, EncounterState state) override
        {
            if (!InstanceScript::SetBossState(type, state))
                return false;

            switch (type)
            {
                case BOSS_LADY_NAZJAR:
                    if (state == DONE)
                    {
                        if (GameObject* defenseSystem = GetGameObject(DATA_THRONE_OF_THE_TIDES_DEFENSE_SYSTEM))
                            defenseSystem->RemoveFlag(GO_FLAG_NOT_SELECTABLE);

                        if (Creature* teleporter1 = GetCreature(DATA_THRONE_OF_THE_TIDES_TELEPORTER_1))
                        {
                            teleporter1->SetNpcFlag(UNIT_NPC_FLAG_SPELLCLICK);
                            teleporter1->CastSpell(teleporter1, SPELL_TELEPORTER_ACTIVE_VISUAL);
                        }

                        if (Creature* teleporter2 = GetCreature(DATA_THRONE_OF_THE_TIDES_TELEPORTER_2))
                        {
                            teleporter2->SetNpcFlag(UNIT_NPC_FLAG_SPELLCLICK);
                            teleporter2->CastSpell(teleporter2, SPELL_TELEPORTER_ACTIVE_VISUAL);
                        }
                    }
                    break;
                case BOSS_COMMANDER_ULTHOK:
                    if (state == FAIL)
                        events.ScheduleEvent(EVENT_RESPAWN_COMMANDER_ULTHOK, 30s);
                    break;
                default:
                    break;
            }

            return true;
        }

        void OnCreatureCreate(Creature* creature) override
        {
            InstanceScript::OnCreatureCreate(creature);

            switch (creature->GetEntry())
            {
                case NPC_OZUMAT:
                case NPC_OZUMAT_VEHICLE:
                case NPC_OZUMAT_VEHICLE_BIG:
                case NPC_LADY_NAZJAR_INTRO:
                case NPC_OZUMAT_ADD_SPAWNER:
                    creature->setActive(true);
                    creature->SetFarVisible(true);
                    break;
                case NPC_WORLD_TRIGGER_INFINITE_AOI:
                    creature->setActive(true);
                    creature->SetFarVisible(true);

                    if (creature->GetExactDist2d(fallingRocksDummyPos) < 1.0f)
                    {
                        _fallingRocksDummyGUID = creature->GetGUID();
                        if (_eventIndex < EVENT_INDEX_DEFENSE_SYSTEM_ACTIVATED)
                            events.ScheduleEvent(EVENT_FALLING_ROCKS, 1min + 15s);
                        break;
                    }
                    else if (creature->GetExactDist2d(shockDefenseDummyPos) < 1.0f)
                    {
                        _shockDefenseDummyGUID = creature->GetGUID();
                        break;
                    }
                    else if (creature->GetExactDist2d(ulthokIntroDummyPos) < 1.0f)
                    {
                        _ulthokIntroDummyGUID = creature->GetGUID();
                        break;
                    }

                    _infiniteAOIDummyGUIDs.push_back(creature->GetGUID());
                    break;
                case NPC_GEYSER_DUMMY:
                    _geyserGUIDs.push_back(creature->GetGUID());
                    break;
                case NPC_DEEP_MURLOC_INVADER:
                case NPC_VICIOUS_MINDLASHER:
                case NPC_UNYIELDING_BEHEMOTH:
                case NPC_UNYIELDING_BEHEMOTH_LEAP_VEHICLE:
                case NPC_BLIGHT_BEAST:
                case NPC_BLIGHT_OF_OZUMAT:
                case NPC_BLIGHT_OF_OZUMAT_2:
                    if (Creature* neptulon = GetCreature(DATA_NEPTULON))
                        neptulon->AI()->JustSummoned(creature);
                    break;
                case NPC_THRONE_OF_THE_TIDES_TELEPORTER_1:
                case NPC_THRONE_OF_THE_TIDES_TELEPORTER_2:
                    if (GetBossState(BOSS_LADY_NAZJAR) == DONE)
                    {
                        creature->SetNpcFlag(UNIT_NPC_FLAG_SPELLCLICK);
                        creature->CastSpell(creature, SPELL_TELEPORTER_ACTIVE_VISUAL);
                    }
                    break;
                case NPC_UNSTABLE_CORRUPTION:
                {
                    // Unstable Corruption respawns every 30 seconds but their
                    // initial spawn timer has been randomized so we need to
                    // stagger the first wave with randomized timers so they
                    // don't all respawn at the same time.
                    ObjectGuid::LowType spawnId = creature->GetSpawnId();
                    if (!spawnId)
                        break;

                    if (_spawnedUnstableCorruptions.find(spawnId) == _spawnedUnstableCorruptions.end())
                    {
                        creature->m_Events.AddEventAtOffset([creature]() { creature->CastSpell(creature, SPELL_QUIET_SUICIDE); }, Seconds(urand(1, 60)));
                        _spawnedUnstableCorruptions.insert(spawnId);
                    }
                    else
                        creature->m_Events.AddEventAtOffset([creature]() { creature->CastSpell(creature, SPELL_QUIET_SUICIDE); }, 60s);
                    break;
                }
                default:
                    break;
            }
        }

        void OnGameObjectCreate(GameObject* go) override
        {
            InstanceScript::OnGameObjectCreate(go);

            switch (go->GetEntry())
            {
                case GO_DOODAD_ABYSSAL_MAW_DOOR_1:
                    if (_eventIndex >= EVENT_INDEX_LADY_NAZJAR_GAUNTLET)
                        go->SetGoState(GO_STATE_ACTIVE);
                    break;
                case GO_DOODAD_ABYSSAL_MAW_DOOR_4:
                    // The door to Neptulon's room opens up once all four
                    // Tainted Sentries have been defeated. Until then the
                    // door remains closed.
                    if (GetBossState(BOSS_OZUMAT) != DONE)
                        go->SetGoState(GO_STATE_READY);
                    break;
                case GO_THRONE_OF_THE_TIDES_DEFENSE_SYSTEM:
                    if (_eventIndex < EVENT_INDEX_DEFENSE_SYSTEM_ACTIVATED && GetBossState(BOSS_LADY_NAZJAR) == DONE)
                        go->RemoveFlag(GO_FLAG_NOT_SELECTABLE);
                    break;
                case GO_DOODAD_INVISIBLE_DOOR_1:
                case GO_DOODAD_INVISIBLE_DOOR_2:
                    if (_eventIndex >= EVENT_INDEX_DEFENSE_SYSTEM_ACTIVATED)
                        go->SetGoState(GO_STATE_ACTIVE);
                    break;
                case GO_DOODAD_ABYSSAL_LEVIATHAN_TENTACLE_1:
                case GO_DOODAD_ABYSSAL_LEVIATHAN_TENTACLE_2:
                    go->SetFarVisible(true);
                    if (_eventIndex >= EVENT_INDEX_DEFENSE_SYSTEM_ACTIVATED)
                        go->DespawnOrUnsummon();
                    break;
                case GO_DOODAD_ABYSSAL_CORAL_CHUNK_CEILING:
                    if (_eventIndex == EVENT_INDEX_ULTHOK_ARRIVED)
                        go->DespawnOrUnsummon();
                    break;
                default:
                    break;
            }
        }

        uint32 GetData(uint32 type) const override
        {
            switch (type)
            {
                case DATA_CURRENT_EVENT_PROGRESS:
                    return _eventIndex;
            }
            return 0;
        }

        void SetData(uint32 type, uint32 data) override
        {
            switch (type)
            {
                case DATA_CURRENT_EVENT_PROGRESS:
                    _eventIndex = data;

                    switch (data)
                    {
                        case EVENT_INDEX_FIRST_INVADERS:
                            for (uint8 i = 0; i < 4; i++)
                            {
                                if (Creature* invader = instance->SummonCreature(i < 2 ? NPC_NAZJAR_SPIRITMENDER_1 : NPC_NAZJAR_INVADER_1, TotTFirstInvaderPositions[i]))
                                {
                                    invader->SetUnitFlag(UNIT_FLAG_IMMUNE_TO_PC);
                                    invader->SetDisableGravity(true);
                                    invader->SetMovementAnimKitId(3);
                                    invader->SetReactState(REACT_PASSIVE);
                                    invader->GetMotionMaster()->MovePoint(0, TotTFirstInvaderInitialMovePositions[i], false);
                                    ExecuteOnMovementEnd(invader, [invader, jumpPos = TotTFirstInvaderJumpPositions[i]]() { JumpThroughWindow(invader, jumpPos); });
                                }
                            }
                            break;
                        case EVENT_INDEX_LADY_NAZJAR_GAUNTLET:
                            instance->SummonCreature(NPC_LADY_NAZJAR_INTRO, ladyNazjarGauntletPos);
                            for (uint8 i = 0; i < 6; i++)
                            {
                                if (Creature* invader = instance->SummonCreature(i < 3 ? NPC_NAZJAR_SPIRITMENDER_2 : NPC_NAZJAR_INVADER_2, TotTLadyNazjarInvaderPositions[i]))
                                {
                                    invader->SetUnitFlag(UNIT_FLAG_IMMUNE_TO_PC);
                                    invader->SetDisableGravity(true);
                                    invader->SetMovementAnimKitId(3);
                                    invader->SetReactState(REACT_PASSIVE);
                                    invader->GetMotionMaster()->MovePoint(0, TotTLadyNazjarInvaderInitialMovePositions[i], false);
                                    ExecuteOnMovementEnd(invader, [invader, jumpPos = TotTLadyNazjarInvaderJumpPositions[i]]() { JumpThroughWindow(invader, jumpPos); });
                                }
                            }
                            break;
                        case EVENT_INDEX_DEFENSE_SYSTEM_ACTIVATED:
                            events.CancelEvent(EVENT_FALLING_ROCKS);
                            if (Creature* shockDefenseDummy = instance->GetCreature(_shockDefenseDummyGUID))
                            {
                                shockDefenseDummy->CastSpell(shockDefenseDummy, SPELL_CAMERA, true);
                                shockDefenseDummy->CastSpell(shockDefenseDummy, SPELL_SHOCK_DEFENSE_OZUMAT, true);
                            }

                            if (GameObject* defenseSystem = GetGameObject(DATA_THRONE_OF_THE_TIDES_DEFENSE_SYSTEM))
                                defenseSystem->SetFlag(GO_FLAG_NOT_SELECTABLE);

                            for (ObjectGuid guid : _infiniteAOIDummyGUIDs)
                                if (Creature* trigger = instance->GetCreature(guid))
                                    trigger->CastSpell(trigger, SPELL_SHOCK_DEFENSE, true);

                            if (Creature* ozumatVehicle = GetCreature(DATA_OZUMAT_VEHICLE_BIG))
                                if (ozumatVehicle->IsAIEnabled())
                                    ozumatVehicle->AI()->DoAction(ACTION_DEFENSE_SYSTEM_ACTIVATED);

                            if (GameObject* door1 = GetGameObject(DATA_INVISIBLE_DOOR_1))
                                door1->SetGoState(GO_STATE_ACTIVE);

                            if (GameObject* door2 = GetGameObject(DATA_INVISIBLE_DOOR_2))
                                door2->SetGoState(GO_STATE_ACTIVE);

                            if (GameObject* abyssalMawDoor = GetGameObject(DATA_ABYSSAL_MAW_DOOR_2))
                                abyssalMawDoor->SetGoState(GO_STATE_ACTIVE);

                            if (GameObject* tentacle1 = GetGameObject(DATA_LEVIATHAN_TENTACLE_1))
                                tentacle1->DespawnOrUnsummon(20s);

                            if (GameObject* tentacle2 = GetGameObject(DATA_LEVIATHAN_TENTACLE_2))
                                tentacle2->DespawnOrUnsummon(15s);

                            events.ScheduleEvent(EVENT_SUMMON_GHURSHA_FLOOR_ADDS, 17s);

                            break;
                        case EVENT_INDEX_ULTHOK_ARRIVED:
                            if (Creature* ulthok = instance->SummonCreature(NPC_COMMANDER_ULTHOK, commanderUlthokIntroPos))
                            {
                                ulthok->SetUnitFlag(UNIT_FLAG_IMMUNE_TO_PC);
                                ulthok->PlayDirectSound(SOUND_ULTHOK_INTRO);
                                ulthok->CastSpell(ulthok, SPELL_ULTHOK_INTRO_VISUAL_STATE);
                                ulthok->CastSpell(ulthok, SPELL_ULTHOK_INTRO_JUMP);
                            }

                            if (Creature* introDummy = instance->GetCreature(_ulthokIntroDummyGUID))
                                introDummy->CastSpell(introDummy, SPELL_ULTHOK_INTRO_VISUAL_CEILING_IMPACT);

                            events.ScheduleEvent(EVENT_BREAK_CORAL, 2s);
                            break;
                        case EVENT_INDEX_NEPTULON_INTRO_DONE:
                            if (Creature* neptulon = GetCreature(DATA_NEPTULON))
                                neptulon->AI()->DoAction(ACTION_CORAL_GARDEN_ENTERED);
                            break;
                        default:
                            break;
                    }
                    break;
                case DATA_LADY_NAZJAR_GEYSERS:
                    for (ObjectGuid guid : _geyserGUIDs)
                    {
                        if (Creature* geyser = instance->GetCreature(guid))
                        {
                            if (data == IN_PROGRESS)
                            {
                                geyser->CastSpell(geyser, SPELL_GEYSER_DUMMY);
                                geyser->m_Events.AddEventAtOffset(new GeyserActivationEvent(geyser), 6s);
                            }
                            else
                            {
                                geyser->RemoveAllAuras();
                                geyser->m_Events.KillAllEvents(true);
                            }
                        }
                    }
                    break;
                default:
                    break;
            }
        }

        void OnUnitDeath(Unit* who) override
        {
            if (who->IsPlayer() && GetBossState(BOSS_OZUMAT) == IN_PROGRESS)
                events.ScheduleEvent(EVENT_CHECK_DEAD_PLAYERS, 100ms);

            if (!who->IsCreature())
                return;

            switch (who->GetEntry())
            {
                case NPC_NAZJAR_INVADER_2:
                case NPC_NAZJAR_SPIRITMENDER_2:
                    if (Creature* nazjar = GetCreature(DATA_LADY_NAZJAR_GAUNTLET))
                        if (nazjar->IsAIEnabled())
                            nazjar->AI()->SummonedCreatureDies(who->ToCreature(), nullptr);
                    break;
                case NPC_TAINTED_SENTRY:
                    ++_killedTaintedSentryCount;
                    if (_killedTaintedSentryCount >= 4)
                    {
                        instance->SetSpawnGroupInactive(SPAWN_GROUP_ID_OZUMAT_WING_EVENT);
                        if (GameObject* neptulonDoor = GetGameObject(DATA_ABYSSAL_MAW_DOOR_4))
                            neptulonDoor->SetGoState(GO_STATE_ACTIVE);
                    }
                    break;
                case NPC_UNSTABLE_CORRUPTION:
                    if (who->ToCreature()->GetSpawnId())
                        who->ToCreature()->DespawnOrUnsummon(3s);
                    break;
                default:
                    break;
            }
        }

        void Update(uint32 diff) override
        {
            events.Update(diff);

            while (uint32 eventId = events.ExecuteEvent())
            {
                switch (eventId)
                {
                    case EVENT_FALLING_ROCKS:
                        if (Creature* fallingRocksDummy = instance->GetCreature(_fallingRocksDummyGUID))
                            fallingRocksDummy->CastSpell(fallingRocksDummy, SPELL_SUMMON_FALLING_ROCKS, true);
                        events.Repeat(1min + 25s, 2min);
                        break;
                    case EVENT_RESPAWN_COMMANDER_ULTHOK:
                        instance->SummonCreature(NPC_COMMANDER_ULTHOK, commanderUlthokRespawnPos);
                        break;
                    case EVENT_BREAK_CORAL:
                        if (GameObject* coral = GetGameObject(DATA_ABYSSAL_CORAL_CHUNK))
                        {
                            coral->SendCustomAnim(0);
                            coral->DespawnOrUnsummon(800ms);
                        }
                        events.ScheduleEvent(EVENT_MOVE_COMMANDER_ULTHOK_TO_HOME_POS, 1ms);
                        break;
                    case EVENT_MOVE_COMMANDER_ULTHOK_TO_HOME_POS:
                        if (Creature* ulthok = GetCreature(BOSS_COMMANDER_ULTHOK))
                        {
                            ulthok->SetHomePosition(commanderUlthokRespawnPos);
                            ulthok->GetMotionMaster()->MoveTargetedHome();
                        }
                        break;
                    case EVENT_CHECK_DEAD_PLAYERS:
                    {
                        uint8 instancePlayersCount = instance->GetPlayersCountExceptGMs();
                        uint8 deadPlayersCount = 0;
                        Map::PlayerList const& players = instance->GetPlayers();

                        for (auto const& i : players)
                        {
                            if (Player* player = i.GetSource())
                            {
                                if (player->IsGameMaster())
                                    continue;

                                if (player->isDead())
                                    deadPlayersCount++;

                                if (deadPlayersCount == instancePlayersCount)
                                    if (Creature* neptulon = GetCreature(DATA_NEPTULON))
                                        neptulon->AI()->EnterEvadeMode(EvadeReason::Other);
                            }
                        }
                        break;
                    }
                    case EVENT_SUMMON_GHURSHA_FLOOR_ADDS:
                        for (uint8 i = 0; i < 6; i++)
                        {
                            if (Creature* hunter = instance->SummonCreature(NPC_GILGOBLIN_HUNTER, TotTGilgoblinHunterPositions[i]))
                            {
                                hunter->GetMotionMaster()->MovePath(hunter->GetEntry() * 10 + i, false);
                                hunter->m_Events.AddEventAtOffset(new EmoteStateEvent(hunter), 5s);
                            }
                        }

                        for (uint8 i = 0; i < 2; i++)
                        {
                            if (Creature* aquamage = instance->SummonCreature(NPC_GILGOBLIN_AQUAMAGE, TotTGilgoblinAquamagePositions[i]))
                            {
                                aquamage->GetMotionMaster()->MovePath(aquamage->GetEntry() * 10 + i, false);
                                aquamage->m_Events.AddEventAtOffset(new EmoteStateEvent(aquamage), 5s);
                            }
                        }

                        if (Creature* watcher = instance->SummonCreature(NPC_FACELESS_WATCHER, TotTFacelessWatcherPosition))
                        {
                            watcher->GetMotionMaster()->MovePath(watcher->GetEntry() * 10, false);
                            watcher->m_Events.AddEventAtOffset(new DelayedPathEvent(watcher), 16s);
                        }
                        break;
                    default:
                        break;
                }
            }
        }

    private:
        EventMap events;
        PersistentInstanceScriptValue<uint8> _eventIndex;
        uint8 _killedTaintedSentryCount;
        ObjectGuid _fallingRocksDummyGUID;
        ObjectGuid _shockDefenseDummyGUID;
        ObjectGuid _ulthokIntroDummyGUID;
        GuidVector _geyserGUIDs;
        GuidVector _infiniteAOIDummyGUIDs;
        std::unordered_set<ObjectGuid::LowType> _spawnedUnstableCorruptions;
    };

    InstanceScript* GetInstanceScript(InstanceMap* map) const override
    {
        return new instance_throne_of_the_tides_InstanceMapScript(map);
    }
};

void AddSC_instance_throne_of_the_tides()
{
    new instance_throne_of_the_tides();
}
