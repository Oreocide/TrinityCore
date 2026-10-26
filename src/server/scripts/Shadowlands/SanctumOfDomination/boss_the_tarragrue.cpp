/*
 * The Tarragrue (DungeonEncounter 2423, boss entry 175611)
 *
 * Part 1 - passivity: the creature is hostile (faction 16) and, without an AI, attacks every NPC whose faction carries the player-group
 * bit that stands in its aggro range (Knights of the Ebon Blade, Bound Soul Remnants, FX/stalker NPCs...).  It only fights players and
 * their pets/guardians.  BossAI also registers the encounter (engage/kill) with the instance script.
 *
 * Part 2 - post-fight sequence (sniffs 2026-10-01 Normal and 2026-10-05 Heroic, seconds after the encounter end):
 *     1.2  Jailer conversation 16648 ("A hollow victory. You will yield to the inevitable.")
 *     4.2  Bolvar, Thrall, Jaina, Ebon Blade knights + acolyte appear              (Normal 4.7)   -> spawn group 1290
 *    11.4  Bolvar/Jaina/Thrall conversation 16994 (42.7s; Thrall's line starts 26.9s into it)  (Normal 11.9)
 *    25.2  Jaina pulls the Tarragrue's essence: the Heart of the Tarragrue appears at the corpse, ~8yd above the ground
 *    28.0  the Heart flies (3.75 yd/s, 16.4s on Heroic from a corpse 62yd away, 9.7s on Normal) to (180.46,-833.61,187.23)
 *          and when it arrives the Portal (180714) fades in (Heroic 44.7, Normal 38.5)  -> spawn group 1292
 * The Jailer line 17207 ("These powers are mine to claim!") is NOT part of the post-fight: on Heroic it started 32s before the kill, with
 * the Tarragrue's Ten of Towers / The Jailer's Gaze final phase (on Normal the fight ended within a second of it).
 * The Portal is an NPC (npc_sod_tarragrue_portal): walking onto it casts [DNT] Teleport (353607, fade to black 2.5s, then the
 * destination of spell_target_position 353607).
 *
 * Part 3 - combat, timers from the 2026-10-05 Heroic sniff (two pulls, 10 players, cycle 94.4s) and DBM (TheTarragrue.lua in dbm_ref/):
 *  - Stage One: Predator's Howl, Grasp of Death, Overpower (+Crushed Armor), Chains of Eternity, Fury of the Ages, Hungering Mist (a 94.4s
 *    cycle: the Mist dissolve at 24.2s, its waves from 29.2s; every timer is re-set from the Mist), Remnant of Forgotten Torments
 *    (Heroic/Mythic only, three per cycle, three different elemental versions).
 *  - Stage Two at 10% health: the Jailer ("These powers are mine to claim!", conversation 17207) removes the players' Anima Powers
 *    (Ten of Towers) and The Jailer's Gaze frenzies the Tarragrue; all stage-one timers stop.
 * Core Support Gate (effects from the client data): plain damage/stun/aura effects work; DUMMY effects need spell scripts and do
 * nothing yet (Grasp of Death 347668, Chains of Eternity 350280, Hungering Mist 347755/354080, Remnant 352368, The Jailer's Gaze
 * 347369/348313), as do the newer effect types (Chains 347556 pull 124, Mist 352987 area trigger 179, Jailer's Gaze knockback 98/353636,
 * the elemental Remnant missiles 352573/352576/352579 effect 32, Ten of Towers effect 203).  The script casts them at the sniffed times
 * and invents nothing.  Not done: Normal/Mythic-specific first-cast offsets (Heroic values are used for every difficulty, DBM lists Normal
 * Howl 3.1s/Grasp 8.1s/Overpower 10.5s/Chains 13s), the 420s berserk (spell unknown), energy-based Mist (timers are used), Annihilating
 * Smash (the follow-up of Chains, +11.6s), the Remnant adds and the Infused Goliaths.
 * Jaina's visible casts (358846 Ping Heart, 353540 Empower Portal) are dummy effects and are not cast.
 */

#include "AreaTrigger.h"
#include "Conversation.h"
#include "CreatureAIImpl.h"
#include "InstanceScript.h"
#include "Map.h"
#include "MotionMaster.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "Unit.h"
#include "sanctum_of_domination.h"

enum TarragrueConversations
{
    CONVERSATION_JAILER_HOLLOW      = 16648,
    CONVERSATION_ALLIES_TALK        = 16994
};

enum TarragrueHeart
{
    NPC_HEART_OF_THE_TARRAGRUE      = 178574,
    POINT_HEART_AT_PORTAL           = 1
};

// The Heart flew to the same spot on both difficulties (sniffed spline end point)
Position const HeartPortalPos = { 180.4566f, -833.6094f, 187.23116f };
static constexpr float HeartSpawnHeight = 8.2f;      // above the corpse (sniffed 177.8-178.1 over a 169.8 ground)
static constexpr float HeartSpeed = 3.75f;           // yd/s: 61.5yd in 16.4s (Heroic), 35.9yd in 9.7s (Normal)

enum TarragrueAreaTriggers
{
    // custom areatrigger_create_properties (IsCustom=1): AT template 27849, spell visual 358479 [DNT] Portal Visual, box 8x4x15 (sniff 10-05)
    AREATRIGGER_PORTAL_VISUAL       = 9100001
};

enum TarragrueSpells
{
    SPELL_DNT_TELEPORT              = 353607,   // [DNT] Teleport: root + TELEPORT_WITH_SPELL_VISUAL_KIT_LOADING_SCREEN, destination in spell_target_position

    SPELL_PREDATORS_HOWL            = 347283,
    SPELL_GRASP_OF_DEATH            = 347668,
    SPELL_OVERPOWER                 = 346985,   // damage + stun, triggers Crushed Armor 346986
    SPELL_CHAINS_OF_ETERNITY        = 350280,
    SPELL_FURY_OF_THE_AGES          = 347490,
    SPELL_HUNGERING_MIST_DISSOLVE   = 347755,
    SPELL_HUNGERING_MIST            = 347679,
    SPELL_HUNGERING_MIST_WAVE       = 352987,
    SPELL_HUNGERING_MIST_WAVE_2     = 354080,
    SPELL_REMNANT_OF_TORMENTS       = 352368,
    SPELL_REMNANT_PHYSICAL          = 352573,
    SPELL_REMNANT_SHADOW            = 352576,
    SPELL_REMNANT_FIRE              = 352579,
    SPELL_TEN_OF_TOWERS             = 352787,
    SPELL_JAILERS_GAZE_FRENZY       = 347369,
    SPELL_JAILERS_GAZE_1            = 352346,
    SPELL_JAILERS_GAZE_2            = 348314,
    SPELL_JAILERS_GAZE_3            = 348313,
    SPELL_JAILERS_GAZE_4            = 353636,
    SPELL_JAILERS_GAZE_5            = 347939
};

enum TarragrueEvents
{
    EVENT_HOWL = 1,
    EVENT_GRASP,
    EVENT_OVERPOWER,
    EVENT_CHAINS,
    EVENT_FURY,
    EVENT_MIST_DISSOLVE,
    EVENT_MIST,
    EVENT_MIST_WAVE,
    EVENT_REMNANT,

    EVENT_GAZE_1,
    EVENT_GAZE_2,
    EVENT_GAZE_3
};

enum TarragruePhases
{
    PHASE_STAGE_ONE = 1,
    PHASE_STAGE_TWO = 2
};

enum TarragrueConversationsCombat
{
    CONVERSATION_JAILER_POWERS      = 17207     // "These powers are mine to claim!" with Ten of Towers
};

static constexpr int32 StageTwoHealthPct = 10;   // journal: "At 10% health remaining, the Jailer removes Anima Powers"
static constexpr Milliseconds MistCycle = 94s + 400ms;

static constexpr Milliseconds JailerHollowAt    = 1s + 300ms;
static constexpr Milliseconds AlliesAppearAt    = 4s + 400ms;
static constexpr Milliseconds AlliesTalkAt      = 11s + 600ms;
static constexpr Milliseconds HeartAppearsAt    = 25s + 500ms;
static constexpr Milliseconds HeartFliesAt      = 28s + 400ms;

static void PlayConversation(Creature* source, uint32 conversationId)
{
    for (MapReference const& ref : source->GetMap()->GetPlayers())
        if (Player* player = ref.GetSource())
            Conversation::CreateConversation(conversationId, player, player->GetPosition(), player->GetGUID(), nullptr);
}

struct boss_the_tarragrue_sod : public BossAI
{
    boss_the_tarragrue_sod(Creature* creature) : BossAI(creature, DATA_THE_TARRAGRUE) { }

    bool CanAIAttack(Unit const* target) const override
    {
        return target->IsControlledByPlayer();
    }

    void Reset() override
    {
        _Reset();
        _stageTwo = false;
        _mistCount = 0;
        _remnantCount = 0;
        Map* map = me->GetMap();
        for (uint32 group : { SPAWNGROUP_TARRAGRUE_ALLIES, SPAWNGROUP_TARRAGRUE_PORTAL })
            map->SpawnGroupDespawn(group);
    }

    void JustEngagedWith(Unit* who) override
    {
        BossAI::JustEngagedWith(who);
        events.SetPhase(PHASE_STAGE_ONE);
        // first casts of the Heroic pull (seconds after the encounter start): medians of 30 Heroic Warcraft Logs kills
        events.ScheduleEvent(EVENT_HOWL, 5s + 800ms, 0, PHASE_STAGE_ONE);
        events.ScheduleEvent(EVENT_GRASP, 8s + 200ms, 0, PHASE_STAGE_ONE);
        events.ScheduleEvent(EVENT_OVERPOWER, 10s + 800ms, 0, PHASE_STAGE_ONE);
        events.ScheduleEvent(EVENT_CHAINS, 13s + 300ms, 0, PHASE_STAGE_ONE);
        events.ScheduleEvent(EVENT_MIST_DISSOLVE, 24s + 300ms, 0, PHASE_STAGE_ONE);
    }

    void DamageTaken(Unit* /*attacker*/, uint32& damage, DamageEffectType /*damageType*/, SpellInfo const* /*spellInfo*/) override
    {
        if (_stageTwo || !me->HealthBelowPctDamaged(StageTwoHealthPct, damage))
            return;

        _stageTwo = true;
        events.Reset();
        events.SetPhase(PHASE_STAGE_TWO);
        DoCastSelf(SPELL_TEN_OF_TOWERS, true);
        PlayConversation(me, CONVERSATION_JAILER_POWERS);
        // sniffed Heroic: Ten of Towers 162.5s, Gaze 163.7s (352346/348314), 169.3s (348313), 170.7s (353636/347939)
        events.ScheduleEvent(EVENT_GAZE_1, 1s + 200ms, 0, PHASE_STAGE_TWO);
        events.ScheduleEvent(EVENT_GAZE_2, 6s + 800ms, 0, PHASE_STAGE_TWO);
        events.ScheduleEvent(EVENT_GAZE_3, 8s + 200ms, 0, PHASE_STAGE_TWO);
    }

    void ExecuteEvent(uint32 eventId) override
    {
        switch (eventId)
        {
            // Stage One: each ability repeats three times per Mist cycle, offsets measured from the Mist (347679) cast
            case EVENT_HOWL:
                DoCastSelf(SPELL_PREDATORS_HOWL);
                break;
            case EVENT_GRASP:
                DoCastVictim(SPELL_GRASP_OF_DEATH);
                if (_mistCount == 0 && _graspCasts++ == 0)
                    events.ScheduleEvent(EVENT_GRASP, 13s + 600ms, 0, PHASE_STAGE_ONE);   // second cast of the pull, 21.8s (WCL median)
                break;
            case EVENT_OVERPOWER:
                DoCastVictim(SPELL_OVERPOWER);
                break;
            case EVENT_CHAINS:
                DoCastSelf(SPELL_CHAINS_OF_ETERNITY);
                break;
            case EVENT_FURY:
                DoCastSelf(SPELL_FURY_OF_THE_AGES);
                break;
            case EVENT_MIST_DISSOLVE:
                // 347755 (instant) and the 5s cast of 347679 start together (WCL: 347679 begincast at 24.3s, sniff lists its finish at 29.0s)
                DoCastSelf(SPELL_HUNGERING_MIST_DISSOLVE, true);
                DoCastSelf(SPELL_HUNGERING_MIST);
                events.ScheduleEvent(EVENT_MIST, 5s, 0, PHASE_STAGE_ONE);
                break;
            case EVENT_MIST:
                StartMistCycle();   // the Mist cast has just finished
                break;
            case EVENT_MIST_WAVE:
                // three waves, 5s apart: 352987 (instant) at +0/+5/+10, each followed by a 4.8s cast of 354080 (WCL begincasts at +0.1/+5.1/+10.1)
                DoCastSelf(SPELL_HUNGERING_MIST_WAVE, true);
                DoCastSelf(SPELL_HUNGERING_MIST_WAVE_2);
                if (++_mistWave < 3)
                    events.ScheduleEvent(EVENT_MIST_WAVE, 5s, 0, PHASE_STAGE_ONE);
                break;
            case EVENT_REMNANT:
            {
                // Heroic/Mythic: three per cycle, each a different elemental version (physical, shadow, fire)
                static constexpr uint32 RemnantSpells[] = { SPELL_REMNANT_PHYSICAL, SPELL_REMNANT_SHADOW, SPELL_REMNANT_FIRE };
                DoCastSelf(SPELL_REMNANT_OF_TORMENTS);
                DoCastSelf(RemnantSpells[_remnantCount % 3], true);
                ++_remnantCount;
                break;
            }
            // Stage Two
            case EVENT_GAZE_1:
                DoCastSelf(SPELL_JAILERS_GAZE_FRENZY, true);
                DoCastSelf(SPELL_JAILERS_GAZE_1, true);
                DoCastSelf(SPELL_JAILERS_GAZE_2, true);
                break;
            case EVENT_GAZE_2:
                DoCastSelf(SPELL_JAILERS_GAZE_3, true);
                break;
            case EVENT_GAZE_3:
                DoCastSelf(SPELL_JAILERS_GAZE_4, true);
                DoCastSelf(SPELL_JAILERS_GAZE_5, true);
                break;
            default:
                break;
        }
    }

    void JustDied(Unit* /*killer*/) override
    {
        _JustDied();

        Creature* boss = me;
        Map* map = me->GetMap();
        Position corpse = me->GetPosition();
        boss->m_Events.AddEventAtOffset([boss]() { PlayConversation(boss, CONVERSATION_JAILER_HOLLOW); }, JailerHollowAt);
        boss->m_Events.AddEventAtOffset([map]() { map->SpawnGroupSpawn(SPAWNGROUP_TARRAGRUE_ALLIES); }, AlliesAppearAt);
        boss->m_Events.AddEventAtOffset([boss]() { PlayConversation(boss, CONVERSATION_ALLIES_TALK); }, AlliesTalkAt);
        // Jaina pulls the Tarragrue's essence: the Heart appears over the corpse (the group-1291 rows stay unspawned)
        boss->m_Events.AddEventAtOffset([boss, corpse]()
        {
            Position pos = corpse;
            pos.m_positionZ += HeartSpawnHeight;
            boss->SummonCreature(NPC_HEART_OF_THE_TARRAGRUE, pos, TEMPSUMMON_MANUAL_DESPAWN);
        }, HeartAppearsAt);
        boss->m_Events.AddEventAtOffset([boss]()
        {
            if (Creature* heart = boss->FindNearestCreature(NPC_HEART_OF_THE_TARRAGRUE, 200.0f))
                heart->GetMotionMaster()->MovePoint(POINT_HEART_AT_PORTAL, HeartPortalPos, false, {}, HeartSpeed);
        }, HeartFliesAt);
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

private:
    // Hungering Mist cast: every other stage-one timer restarts from here (offsets measured in the Heroic sniff, cycle 94.4s)
    void StartMistCycle()
    {
        ++_mistCount;
        events.CancelEvent(EVENT_HOWL); events.CancelEvent(EVENT_GRASP); events.CancelEvent(EVENT_OVERPOWER);
        events.CancelEvent(EVENT_CHAINS); events.CancelEvent(EVENT_FURY); events.CancelEvent(EVENT_REMNANT);
        _mistWave = 0;
        events.ScheduleEvent(EVENT_MIST_WAVE, 0ms, 0, PHASE_STAGE_ONE);

        for (Milliseconds t : { 18s + 200ms, 44s + 200ms, 69s + 600ms })
            events.ScheduleEvent(EVENT_HOWL, t, 0, PHASE_STAGE_ONE);
        for (Milliseconds t : { 22s + 400ms, 49s + 900ms, 76s + 900ms })
            events.ScheduleEvent(EVENT_OVERPOWER, t, 0, PHASE_STAGE_ONE);
        for (Milliseconds t : { 23s + 900ms, 52s + 800ms, 79s + 0ms })
            events.ScheduleEvent(EVENT_GRASP, t, 0, PHASE_STAGE_ONE);
        for (Milliseconds t : { 27s + 500ms, 55s + 200ms, 83s + 300ms })
            events.ScheduleEvent(EVENT_CHAINS, t, 0, PHASE_STAGE_ONE);
        for (Milliseconds t : { 28s + 800ms, 65s + 200ms })
            events.ScheduleEvent(EVENT_FURY, t, 0, PHASE_STAGE_ONE);

        Difficulty difficulty = me->GetMap()->GetDifficultyID();
        if (difficulty == DIFFICULTY_HEROIC_RAID || difficulty == DIFFICULTY_MYTHIC_RAID)
            for (Milliseconds t : { 19s + 200ms, 50s + 900ms, 81s + 700ms })
                events.ScheduleEvent(EVENT_REMNANT, t, 0, PHASE_STAGE_ONE);

        // next Mist: the dissolve is 5s before the cast
        events.ScheduleEvent(EVENT_MIST_DISSOLVE, MistCycle - 5s, 0, PHASE_STAGE_ONE);
    }

    bool _stageTwo = false;
    uint8 _mistCount = 0;
    uint8 _mistWave = 0;
    uint8 _remnantCount = 0;
    uint8 _graspCasts = 0;
};

// 178574 - Heart of the Tarragrue: when it reaches the portal spot the Portal fades in
struct npc_sod_tarragrue_heart : public ScriptedAI
{
    npc_sod_tarragrue_heart(Creature* creature) : ScriptedAI(creature) { }

    void MovementInform(uint32 type, uint32 id) override
    {
        if (type == POINT_MOTION_TYPE && id == POINT_HEART_AT_PORTAL)
            me->GetMap()->SpawnGroupSpawn(SPAWNGROUP_TARRAGRUE_PORTAL);
    }

    void UpdateAI(uint32 /*diff*/) override { }
};

// 180714 - Portal: players who walk into it are teleported to the next area of the raid
struct npc_sod_tarragrue_portal : public ScriptedAI
{
    npc_sod_tarragrue_portal(Creature* creature) : ScriptedAI(creature), _checkTimer(0) { }

    // The sniffed teleport happened with the player standing on the NPC (0.05 yd); 2 yards is an assumption
    static constexpr float TriggerRange = 2.0f;
    static constexpr uint32 PlayerCooldown = 8000;   // the teleport takes 2.5s of fade + the loading screen; never retrigger meanwhile
    static constexpr int32 VisualDuration = 2500;    // sniffed lifetime of the Portal Visual area trigger

    void UpdateAI(uint32 diff) override
    {
        for (auto itr = _cooldowns.begin(); itr != _cooldowns.end();)
        {
            if (itr->second <= diff)
                itr = _cooldowns.erase(itr);
            else
            {
                itr->second -= diff;
                ++itr;
            }
        }

        if (_checkTimer > diff)
        {
            _checkTimer -= diff;
            return;
        }
        _checkTimer = 250;

        for (MapReference const& ref : me->GetMap()->GetPlayers())
        {
            Player* player = ref.GetSource();
            if (!player || !player->IsAlive() || player->IsGameMaster() || _cooldowns.contains(player->GetGUID()))
                continue;
            if (me->GetDistance(player) > TriggerRange)
                continue;

            _cooldowns[player->GetGUID()] = PlayerCooldown;
            // the white light of the open portal is an area trigger created when a player enters (sniff: created with the cast, gone 2.5s later)
            AreaTrigger::CreateAreaTrigger({ AREATRIGGER_PORTAL_VISUAL, true }, me->GetPosition(), VisualDuration, me, nullptr);
            player->CastSpell(player, SPELL_DNT_TELEPORT, true);
        }
    }

private:
    uint32 _checkTimer;
    std::unordered_map<ObjectGuid, uint32> _cooldowns;
};

void AddSC_boss_the_tarragrue_sod()
{
    RegisterSanctumOfDominationCreatureAI(boss_the_tarragrue_sod);
    RegisterSanctumOfDominationCreatureAI(npc_sod_tarragrue_heart);
    RegisterSanctumOfDominationCreatureAI(npc_sod_tarragrue_portal);
}
