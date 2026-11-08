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
 * Sun King's Salvation (DungeonEncounter 2402, Journal 2422, map 2296): Kael'thas Sunstrider 165759 is won by healing, not killed.
 * Spec: trinitycore-tracking/castle-nathria/bosses/sun-kings-salvation.md/.json and sun-kings-salvation_followup.json (one solo
 * Normal capture, WCL, DBM, BigWigs, spell data). Times below are seconds after ENCOUNTER_START unless marked S+ (after a Shade
 * appears), T+ (after a Shade retreats) or L+ (after the lethal hit on the second Shade).
 *
 * Health model:
 * - Kael starts at 10% and never enters combat. Health regen is off (a friendly creature out of combat would refill at once).
 *   Gift of the Sun King 456954 (1% of his maximum per second) is cast at the start; player heals, Soul Pedestals and Essence
 *   Overflow add to it.
 * - At 45% and at 90% Kael releases a Shade 165805 with exactly the rest (55% / 10%) and drops to the threshold. While Reflection
 *   of Guilt 323402 sits on the Shade, Kael% = 100 - Shade% (its 500 ms tick) and Heal Monitor 323397 absorbs every heal on Kael.
 *   Heal Monitor and the absorb parts of 323402 and Cracked Reservoir 329539 are infinite pools here (amount -1): with the table
 *   values the first hit would use them up and the core would remove the aura.
 * - The first Shade retreats at 45.00% or less after a hit (Kael 55% or more). The second Shade stays at 1 health on the lethal
 *   hit: the mirror puts Kael at 100% and the encounter is won.
 * - Win: the chest 357752 is summoned 4.0 s later and its personal loot is generated BEFORE SetBossState(DONE), because DONE locks
 *   every player and GenerateDungeonEncounterPersonalLoot skips locked players (the capture sends both in the same 10 ms). The
 *   chest's AI blocks a use by a player who has no loot of his own, which would otherwise rebuild (and empty) the whole loot map.
 *
 * Deviations and estimates, on purpose:
 * - The Shade is summoned at the sniffed point instead of through Release Sin 323381 (its destination is an entry target that no
 *   creature in the data provides). Shade Teleport to Arena 328333 has the same problem; the Shade is moved to the sniffed arena
 *   point (-2260.4, 6384.3) instead.
 * - The encounter starts when any pre-placed add within 100 yd of Kael is engaged; the other pre-placed adds are pulled at once
 *   (in the capture they are all active from the pull). It fails when no living player is within 100 yd of Kael (checked each
 *   second). Retail wipe handling is not evidenced; the pre-placed adds are respawned and Kael goes back to 10%.
 * - Ember Blast's damage is split by the script between the players hit, with the 6 Lingering Embers applications split the same
 *   way. The target of Ember Blast, Crimson Flurry and Blazing Surge is chosen by the AI (the marker spells pick their own random
 *   player); the markers are added as auras on that player.
 * - Blazing Surge pools: 6 missiles 0.15 s apart, starting 0.1 s after the cast, to random points of the 60 degree, 40 yd cone.
 * - Soul Pedestal: flat 5376 heal on Kael per tick (sniff; WCL flat 6000 pre-squish), damage to the clicker 0.212% of his maximum
 *   health times 1, 3, 5 for the three ticks (the only sniffed series, n=3); the aura is removed after the third tick (sniff 10.9 s).
 *   It cannot be clicked while a Shade carries Reflection of Guilt (Journal; not seen in the capture).
 * - Essence Font decay: 2% of its maximum times 1.05^tick every 2 s, rounded up (fits all 19 sniffed drops).
 * - The first Shade's pools go with it when it despawns at T+4.4 s (the core removes a caster's areatriggers); retail keeps
 *   them for their 60 s.
 * - Bleakwing return charge 339308 goes to the Assassin's current victim (in the solo capture victim and marked player are the same).
 * - Occultist first casts: two of them start Scornful Blast at 3.7 s and the others 1.2 s later (split by spawn id; the capture
 *   does not say which pair). Vulgar Brand is cast once at 23.2 / 24.4 s; its repeat is not observed (DBM's timer is commented out).
 *
 * Not settled (left out or estimates): Door of Shadows 328248 (n=0), Occultist behaviour at 0 power, Pestering Fiend waves (not in
 * the 12.x capture), Heroic and Mythic differences beyond the spell data (Cloak of Flames is Mythic only), the group behaviour of the
 * Gift (CN8), the chest loot table (gameobject_loot_template 105689 is empty). No berserk: DBM has none (BigWigs shows 840 s).
 */

#include "AreaTrigger.h"
#include "AreaTriggerAI.h"
#include "Containers.h"
#include "Conversation.h"
#include "GameObject.h"
#include "GameObjectAI.h"
#include "InstanceScript.h"
#include "Loot.h"
#include "LootMgr.h"
#include "Map.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "Random.h"
#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "Spell.h"
#include "SpellAuraEffects.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "SpellScript.h"
#include "TemporarySummon.h"
#include "ThreatManager.h"
#include "castle_nathria.h"
#include <any>
#include <cmath>

enum SunKingSpells
{
    // Kael'thas
    SPELL_GIFT_OF_THE_SUN_KING          = 456954,
    SPELL_HEAL_MONITOR                  = 323397,
    SPELL_CANT_HEAL_SHADER              = 332760,
    SPELL_REFLECTION_OF_GUILT           = 323402,
    SPELL_KAEL_SHADE_BEAM               = 331985,
    SPELL_CHAINS_RIGHT                  = 304158,
    SPELL_CHAINS_LEFT                   = 304159,
    SPELL_CHAINS_RIGHT_CAST             = 335185,
    SPELL_CHAINS_LEFT_CAST              = 335186,

    // Shade of Kael'thas
    SPELL_SHADE_COSMETIC                = 304148,
    SPELL_SCORCHING_ENTRANCE            = 328337,
    SPELL_PHOENIX_REBORN                = 328736,
    SPELL_SHADE_ENERGY                  = 328671,
    SPELL_FIERY_STRIKE                  = 326455,
    SPELL_EMBER_BLAST_MARK              = 325873,
    SPELL_EMBER_BLAST                   = 325877,
    SPELL_LINGERING_EMBERS              = 326430,
    SPELL_EMBER_BLAST_POOL              = 328600,
    SPELL_BLAZING_SURGE                 = 329518,
    SPELL_BLAZING_SURGE_MISSILE         = 329515,
    SPELL_CLEAR_EMBER_BLAST_TARGETING   = 332663,
    SPELL_DRAW_BACK_IN                  = 332681,

    // Reborn Phoenix
    SPELL_SMOLDERING_PLUMAGE            = 328659,
    SPELL_SMOLDERING_PLUMAGE_PULSE      = 341254,
    SPELL_PHOENIX_POOL                  = 328658,
    SPELL_BURNING_FIXATION              = 328654,
    SPELL_EYES_ON_TARGET                = 328479,
    SPELL_PHOENIX_EMBERS                = 328731,
    SPELL_REBORN_PHOENIX_EXIT           = 336658,

    // pools (areatrigger spells 328578 Blazing Surge, 328600 Ember Blast, 328658 Phoenix)
    SPELL_SURGE_POOL                    = 328578,
    SPELL_SMOLDERING_REMNANTS           = 328579,

    // High Torturer Darithos
    SPELL_GREATER_CASTIGATION_MARK      = 332871,
    SPELL_GREATER_CASTIGATION           = 328885,
    SPELL_GREATER_CASTIGATION_CHANNEL   = 328889,
    SPELL_GREATER_CASTIGATION_DAMAGE    = 328890,

    // Vile Occultist
    SPELL_ENERGIZE                      = 328367,
    SPELL_SCORNFUL_BLAST                = 325590,
    SPELL_VULGAR_BRAND                  = 333002,
    SPELL_SUMMON_ESSENCE_FONT           = 329565,

    // Bleakwing Assassin
    SPELL_CRIMSON_FLURRY_MARK           = 341473,
    SPELL_CRIMSON_FLURRY_TELEPORT       = 341474,
    SPELL_RAPID_STRIKES_RETURN_CHARGE   = 339308,

    // Rockbound Vanquisher
    SPELL_ANCHOR_HERE                   = 45313,
    SPELL_CONCUSSIVE_SMASH              = 325506,
    SPELL_VANQUISHING_STRIKE            = 325440,

    // Soul Infuser
    SPELL_INFUSER_TELEPORT_PREP         = 334133,
    SPELL_INFUSER_TELEPORT              = 325950,
    SPELL_SOUL_INFUSION                 = 325665,
    SPELL_INFUSER_BEAM                  = 304147,
    SPELL_SOUL_INFUSION_SHADE           = 339281,
    SPELL_SOUL_INFUSION_KAEL            = 329184,
    SPELL_INFUSERS_ORB                  = 326075,

    // Soul Pedestal
    SPELL_SOUL_PEDESTAL                 = 339232,
    SPELL_PEDESTAL_HEAL                 = 339243,
    SPELL_PEDESTAL_DAMAGE               = 339244,
    SPELL_DRAINED_SOUL                  = 339251,

    // Essence Font
    SPELL_CRACKED_RESERVOIR             = 329539,
    SPELL_ESSENCE_OVERFLOW              = 329561,

    // Fight Controller
    SPELL_FIGHT_CONTROLLER_END          = 328581
};

enum SunKingCreatures
{
    NPC_SHADE_OF_KAELTHAS               = 165805,
    NPC_REBORN_PHOENIX                  = 168962,
    NPC_ESSENCE_FONT                    = 165778,
    NPC_ROCKBOUND_VANQUISHER            = 165764,
    NPC_SOUL_INFUSER                    = 165762,
    NPC_VILE_OCCULTIST                  = 165763,
    NPC_BLEAKWING_ASSASSIN              = 167566,
    NPC_HIGH_TORTURER_DARITHOS          = 168973,
    NPC_THE_ACCUSER_ARENA               = 165760,
    NPC_FIGHT_CONTROLLER                = 165788,
    NPC_SOUL_PEDESTAL                   = 173382,
    NPC_INFUSERS_ORB                    = 170688,
    NPC_PRINCE_RENATHAL_END             = 172652,
    NPC_THE_ACCUSER_END                 = 172651
};

enum SunKingGameObjects
{
    GO_CACHE_OF_THE_SUN_KING            = 357752,
    GO_PEDESTAL_COLLISION               = 357361
};

enum SunKingTexts
{
    // Kael'thas
    SAY_POWER_RETURNS                   = 0,

    // Shade of Kael'thas
    SAY_RAGE_BURNS                      = 0,
    EMOTE_EMBER_BLAST                   = 1,
    SAY_FIRE_CONSUME_YOU                = 2,
    EMOTE_SHADE_EMERGED                 = 3,
    SAY_ENGULFED_IN_FLAME               = 4,
    SAY_FELOMIN_ASHAL                   = 5,
    SAY_NOT_FINISHED                    = 6,

    // The Accuser (arena)
    SAY_ACCUSER_DISPOSE                 = 0,
    SAY_ACCUSER_GUARDS_EN_ROUTE         = 1,

    // Fight Controller
    EMOTE_VANQUISHER_INCOMING           = 0,
    EMOTE_INFUSERS_INCOMING             = 1,

    // High Torturer Darithos
    SAY_DARITHOS_AGGRO                  = 0,
    WHISPER_GREATER_CASTIGATION         = 1
};

enum SunKingData
{
    DATA_SHADE_INDEX                    = 1,
    DATA_INFUSER_INDEX                  = 2,
    DATA_HEALS_BLOCKED                  = 3
};

enum SunKingPoints
{
    POINT_VANQUISHER_LAND               = 1,
    POINT_INFUSER_LEG_1                 = 2,
    POINT_INFUSER_LEG_2                 = 3,
    POINT_INFUSER_HOVER                 = 4
};

enum SunKingStage : uint8
{
    STAGE_IDLE,
    STAGE_ONE,
    STAGE_SHADE_ONE,
    STAGE_INTERLUDE,
    STAGE_SHADE_TWO,
    STAGE_WON
};

enum SunKingEvents
{
    // Kael'thas
    EVENT_WIPE_CHECK                    = 1,
    EVENT_SHADE_BEAM,
    EVENT_VANQUISHER_EMOTE,
    EVENT_SUMMON_VANQUISHER,
    EVENT_INFUSERS_EMOTE,
    EVENT_SUMMON_INFUSERS,
    EVENT_WIN_AURAS,
    EVENT_WIN_CHAINS,
    EVENT_WIN_END,
    EVENT_WIN_CONVERSATION,
    EVENT_DONE_CLEANUP,

    // Shade
    EVENT_SHADE_TELEPORT,
    EVENT_SHADE_ENTRANCE,
    EVENT_SHADE_ACTIVATE,
    EVENT_SHADE_ENERGY,
    EVENT_FIERY_STRIKE,
    EVENT_EMBER_BLAST,

    // Phoenix
    EVENT_BURNING_FIXATION,

    // Vanquisher
    EVENT_ANCHOR_HERE,
    EVENT_VANQUISHER_ENGAGE,
    EVENT_VANQUISHING_STRIKE,
    EVENT_CONCUSSIVE_SMASH,

    // Infuser
    EVENT_INFUSER_TELEPORT,
    EVENT_INFUSER_FLY,

    // Occultist
    EVENT_SCORNFUL_BLAST,
    EVENT_VULGAR_BRAND,

    // Bleakwing
    EVENT_CRIMSON_FLURRY,
    EVENT_CRIMSON_FLURRY_TELEPORT,
    EVENT_CRIMSON_FLURRY_RETURN,

    // Darithos
    EVENT_DARITHOS_AGGRO,
    EVENT_GREATER_CASTIGATION,
    EVENT_GREATER_CASTIGATION_WHISPER
};

static constexpr uint32 DUNGEON_ENCOUNTER_SUN_KING = 2402;
static constexpr uint32 CONVERSATION_SUN_KING_END = 16098;

// health model
static constexpr float StartHealthPct = 10.0f;
static constexpr std::array<float, 2> ShadeThresholds = { 45.0f, 90.0f };
static constexpr float RetreatHealthPct = 45.0f;
static constexpr float EncounterRange = 100.0f;

// Shade (S+)
static constexpr Milliseconds ShadeTeleport = 7500ms;
static constexpr Milliseconds ShadeEntrance = 7800ms;
static constexpr Milliseconds ShadeActivate = 8800ms;
static constexpr Milliseconds ShadeEnergy = 8900ms;
static constexpr Milliseconds FieryStrikeFirst = 14700ms;
static constexpr Milliseconds FieryStrikeRepeat = 9700ms;          // WCL p50 9.7, sniff 8.6-11.0
static constexpr std::array<Milliseconds, 2> EmberBlastFirst = { 19600ms, 19800ms };
static constexpr Milliseconds EmberBlastRepeat = 20800ms;
static constexpr uint32 SurgeEnergy = 100;
static constexpr uint8 SurgePools = 6;
static constexpr Milliseconds SurgePoolFirst = 100ms;
static constexpr Milliseconds SurgePoolGap = 150ms;
static constexpr float SurgeConeHalfAngle = float(M_PI) / 6.0f;
static constexpr float SurgeConeLength = 40.0f;
static constexpr Milliseconds ShadeDespawn = 4400ms;               // T+4.4 (retreat) and L+4.1 (win)
static constexpr Milliseconds ShadeDespawnWin = 4100ms;
static constexpr Milliseconds PhoenixExitDespawn = 240ms;

// interlude (T+)
static constexpr Milliseconds VanquisherEmote = 4160ms;
static constexpr Milliseconds VanquisherSpawn = 4400ms;
static constexpr Milliseconds InfusersEmote = 24160ms;
static constexpr Milliseconds InfusersSpawn = 24660ms;

// win (L+)
static constexpr Milliseconds WinAuras = 500ms;
static constexpr Milliseconds WinChains = 2900ms;
static constexpr Milliseconds WinEnd = 4030ms;
static constexpr Milliseconds WinConversation = 12200ms;

// Phoenix
static constexpr Milliseconds FixateFirst = 1s;
static constexpr Milliseconds FixateRepeat = 11s;

// Vanquisher (from its landing)
static constexpr float VanquisherFlightSpeed = 5.86f;              // 57.4 yd in 9.8 s
static constexpr Milliseconds AnchorAfterLanding = 1300ms;
static constexpr Milliseconds EngageAfterLanding = 3300ms;
static constexpr Milliseconds VanquishingStrikeFirst = 6100ms;
static constexpr Milliseconds VanquishingStrikeRepeat = 6100ms;    // DBM 6.1, WCL 6.1-8.1
static constexpr Milliseconds ConcussiveSmashFirst = 11000ms;
static constexpr Milliseconds ConcussiveSmashRepeat = 19500ms;     // DBM 19.5, WCL 19.4-19.5

// Infusers (from their spawn)
static constexpr Milliseconds InfuserTeleportCast = 4100ms;
static constexpr Milliseconds InfuserFlyDelay = 900ms;
static constexpr float InfuserFlightSpeed = 9.5f;                  // 83-85 yd in 8.7-9.0 s

// Occultist
static constexpr std::array<Milliseconds, 2> ScornfulBlastFirst = { 3700ms, 4900ms };
static constexpr Milliseconds ScornfulBlastRepeat = 2400ms;
static constexpr std::array<Milliseconds, 2> VulgarBrandFirst = { 23200ms, 24400ms };

// Bleakwing
static constexpr std::array<Milliseconds, 2> CrimsonFlurryFirst = { 2400ms, 8600ms };
static constexpr Milliseconds CrimsonFlurrySecond = 12300ms;
static constexpr Milliseconds CrimsonFlurryRepeat = 11000ms;
static constexpr Milliseconds CrimsonFlurryTeleport = 5000ms;
static constexpr Milliseconds CrimsonFlurryReturn = 1500ms;

// Darithos
static constexpr Milliseconds DarithosAggro = 200ms;
static constexpr Milliseconds GreaterCastigationFirst = 6100ms;
static constexpr Milliseconds GreaterCastigationRepeat = 15900ms;

// Soul Pedestal and Essence Font
static constexpr int32 PedestalHeal = 5376;
static constexpr float PedestalDamagePct = 0.212f;
static constexpr uint32 PedestalTicks = 3;
static constexpr float EssenceFontStartPct = 65.0f;
static constexpr float EssenceFontDecayGrowth = 1.05f;
static constexpr float EssenceOverflowPct = 2.5f;

static constexpr uint8 LingeringEmbersApplications = 6;

static Position const ShadeSummonPos = { -2294.0017f, 6384.404f, 4376.8354f, 0.01745329f };
static Position const ShadeArenaPos = { -2260.4f, 6384.3f, 4367.2f, float(M_PI) };
static Position const VanquisherSpawnPos = { -2235.9136f, 6348.141f, 4409.4155f, 2.4657f };
static Position const VanquisherLandPos = { -2236.1f, 6387.2f, 4367.4f };
static Position const CachePos = { -2269.144f, 6384.646f, 4367.202f, 0.0f };
static Position const RenathalEndPos = { -2166.7258f, 6362.3257f, 4381.124f, 0.0f };
static Position const AccuserEndPos = { -2166.1926f, 6407.617f, 4381.124f, 0.0f };

static constexpr std::array<Position, 3> InfuserSpawnPos =
{ {
    { -2162.9f, 6348.7f, 4381.6f, 1.791f },
    { -2162.8f, 6348.3f, 4381.6f, 1.791f },
    { -2162.9f, 6348.5f, 4381.6f, 1.791f }
} };
static Position const InfuserLeg1 = { -2165.1f, 6373.4f, 4379.1f };
static constexpr std::array<Position, 3> InfuserLeg2 =
{ {
    { -2167.5f, 6382.0f, 4380.0f },
    { -2169.9f, 6382.1f, 4380.0f },
    { -2165.9f, 6382.2f, 4380.0f }
} };
static constexpr std::array<Position, 3> InfuserTeleportPos =
{ {
    { -2199.5f, 6353.2f, 4367.8f },
    { -2199.5f, 6353.2f, 4367.8f },
    { -2206.2f, 6440.0f, 4367.9f }
} };
// about 5 yd from the pedestal each one feeds
static constexpr std::array<Position, 3> InfuserHoverPos =
{ {
    { -2280.7f, 6372.1f, 4368.4f },
    { -2291.8f, 6359.9f, 4368.6f },
    { -2276.3f, 6391.4f, 4368.4f }
} };

static Creature* GetKaelthas(WorldObject const* obj)
{
    if (InstanceScript* instance = obj->GetInstanceScript())
        return instance->GetCreature(DATA_SUN_KINGS_SALVATION);
    return nullptr;
}

static bool AreKaelHealsBlocked(WorldObject const* obj)
{
    if (Creature* kael = GetKaelthas(obj))
        if (kael->IsAIEnabled())
            return kael->AI()->GetData(DATA_HEALS_BLOCKED) != 0;
    return false;
}

static bool IsRemnantPool(uint32 spellId)
{
    return spellId == SPELL_SURGE_POOL || spellId == SPELL_EMBER_BLAST_POOL || spellId == SPELL_PHOENIX_POOL;
}

// 165759 - Kael'thas Sunstrider
struct boss_kaelthas_sun_king : public ScriptedAI
{
    boss_kaelthas_sun_king(Creature* creature) : ScriptedAI(creature), _instance(creature->GetInstanceScript()), _summons(creature) { }

    void InitializeAI() override
    {
        me->SetRegenerateHealth(false);
        ScriptedAI::InitializeAI();
    }

    void Reset() override
    {
        me->SetRegenerateHealth(false);
        _events.Reset();
        _mirror = false;
        _shadeGuid.Clear();

        if (_instance && _instance->GetBossState(DATA_SUN_KINGS_SALVATION) == DONE)
        {
            SetDoneState();
            return;
        }

        _stage = STAGE_IDLE;
        me->SetHealth(me->CountPctFromMaxHealth(StartHealthPct));
    }

    uint32 GetData(uint32 type) const override
    {
        if (type == DATA_HEALS_BLOCKED)
            return _mirror ? 1 : 0;
        return 0;
    }

    Creature* GetShade() const
    {
        return ObjectAccessor::GetCreature(*me, _shadeGuid);
    }

    void JustSummoned(Creature* summon) override
    {
        _summons.Summon(summon);
    }

    void SummonedCreatureDespawn(Creature* summon) override
    {
        _summons.Despawn(summon);
    }

    // any pre-placed add of the arena that is engaged starts the encounter
    void StartEncounter(Unit* who)
    {
        if (_stage != STAGE_IDLE || !_instance || !who)
            return;

        EncounterState const state = _instance->GetBossState(DATA_SUN_KINGS_SALVATION);
        if (state == DONE || state == IN_PROGRESS)
            return;

        if (!_instance->SetBossState(DATA_SUN_KINGS_SALVATION, IN_PROGRESS))
            return;

        _stage = STAGE_ONE;
        _said50 = false;
        _said75 = false;
        _instance->SendEncounterUnit(ENCOUNTER_FRAME_ENGAGE, me, 1);
        me->SetHealth(me->CountPctFromMaxHealth(StartHealthPct));
        DoCastSelf(SPELL_GIFT_OF_THE_SUN_KING, true);

        CachePrePlacedAdds();
        for (ObjectGuid::LowType spawnId : _prePlacedSpawnIds)
        {
            auto bounds = me->GetMap()->GetCreatureBySpawnIdStore().equal_range(spawnId);
            for (auto itr = bounds.first; itr != bounds.second; ++itr)
                if (itr->second->IsAlive() && !itr->second->IsEngaged())
                    itr->second->EngageWithTarget(who);
        }

        _events.ScheduleEvent(EVENT_WIPE_CHECK, 1s);
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);
        while (uint32 eventId = _events.ExecuteEvent())
            ExecuteEvent(eventId);

        if (_stage == STAGE_IDLE || _stage == STAGE_WON)
            return;

        // every heal source ends up here: the Gift, players, pedestals, Essence Overflow and the mirror
        float const pct = me->GetHealthPct();
        if (!_said50 && pct >= 50.0f)
        {
            _said50 = true;
            Talk(SAY_POWER_RETURNS);
        }
        if (!_said75 && pct >= 75.0f)
        {
            _said75 = true;
            Talk(SAY_POWER_RETURNS);
        }

        if (_stage == STAGE_ONE && pct >= ShadeThresholds[0])
            ReleaseSin(0);
        else if (_stage == STAGE_INTERLUDE && pct >= ShadeThresholds[1])
            ReleaseSin(1);
    }

    void ReleaseSin(uint8 index)
    {
        _stage = index == 0 ? STAGE_SHADE_ONE : STAGE_SHADE_TWO;
        float const threshold = ShadeThresholds[index];

        DoCastSelf(SPELL_CANT_HEAL_SHADER, true);
        // the tick that crosses the threshold overshoots by up to 1% on retail too; the Shade gets exactly the rest
        me->SetHealth(me->CountPctFromMaxHealth(threshold));

        Creature* shade = me->SummonCreature(NPC_SHADE_OF_KAELTHAS, ShadeSummonPos, TEMPSUMMON_MANUAL_DESPAWN);
        if (!shade)
            return;

        shade->SetHealth(shade->CountPctFromMaxHealth(100.0f - threshold));
        _shadeGuid = shade->GetGUID();
        _mirror = true;
        DoCast(shade, SPELL_REFLECTION_OF_GUILT, true);
        if (shade->IsAIEnabled())
            shade->AI()->SetData(DATA_SHADE_INDEX, index);
        _events.ScheduleEvent(EVENT_SHADE_BEAM, 0s);
    }

    // Reflection of Guilt: Kael% = 100 - Shade%
    void MirrorShade(Unit const* shade)
    {
        if (!_mirror || !shade || shade->GetGUID() != _shadeGuid || !shade->GetMaxHealth())
            return;

        uint64 const missing = shade->GetMaxHealth() - std::min<uint64>(shade->GetHealth(), shade->GetMaxHealth());
        uint64 const health = me->GetMaxHealth() * missing / shade->GetMaxHealth();
        me->SetHealth(std::max<uint64>(health, 1));
    }

    void EndMirror(Creature* shade)
    {
        _mirror = false;
        _events.CancelEvent(EVENT_SHADE_BEAM);
        me->RemoveAurasDueToSpell(SPELL_CANT_HEAL_SHADER);
        if (shade)
            shade->RemoveAurasDueToSpell(SPELL_REFLECTION_OF_GUILT);
    }

    // the first Shade left at 45% or less; Kael keeps the mirrored health (55% or more) and the interlude begins
    void ShadeRetreated(Creature* shade)
    {
        if (_stage != STAGE_SHADE_ONE)
            return;

        MirrorShade(shade);
        EndMirror(shade);
        _shadeGuid.Clear();
        _stage = STAGE_INTERLUDE;

        _events.ScheduleEvent(EVENT_VANQUISHER_EMOTE, VanquisherEmote);
        _events.ScheduleEvent(EVENT_SUMMON_VANQUISHER, VanquisherSpawn);
        _events.ScheduleEvent(EVENT_INFUSERS_EMOTE, InfusersEmote);
        _events.ScheduleEvent(EVENT_SUMMON_INFUSERS, InfusersSpawn);
    }

    // the lethal hit on the second Shade: the mirror gives Kael his full health
    void ShadeDefeated(Creature* shade)
    {
        if (_stage != STAGE_SHADE_TWO)
            return;

        EndMirror(shade);
        _stage = STAGE_WON;
        me->SetFullHealth();

        _events.Reset();
        _events.ScheduleEvent(EVENT_WIN_AURAS, WinAuras);
        _events.ScheduleEvent(EVENT_WIN_CHAINS, WinChains);
        _events.ScheduleEvent(EVENT_WIN_END, WinEnd);
        _events.ScheduleEvent(EVENT_WIN_CONVERSATION, WinConversation);
    }

    void ExecuteEvent(uint32 eventId)
    {
        switch (eventId)
        {
            case EVENT_WIPE_CHECK:
                if (!AnyPlayerInArena())
                {
                    FailEncounter();
                    return;
                }
                _events.Repeat(1s);
                break;
            case EVENT_SHADE_BEAM:
                if (Creature* shade = GetShade(); shade && _mirror)
                {
                    DoCast(shade, SPELL_KAEL_SHADE_BEAM, true);
                    _events.Repeat(2500ms);
                }
                break;
            case EVENT_VANQUISHER_EMOTE:
                if (Creature* controller = me->FindNearestCreature(NPC_FIGHT_CONTROLLER, 100.0f))
                    controller->AI()->Talk(EMOTE_VANQUISHER_INCOMING);
                if (Creature* accuser = me->FindNearestCreature(NPC_THE_ACCUSER_ARENA, 300.0f))
                    accuser->AI()->Talk(SAY_ACCUSER_DISPOSE);
                break;
            case EVENT_SUMMON_VANQUISHER:
                me->SummonCreature(NPC_ROCKBOUND_VANQUISHER, VanquisherSpawnPos, TEMPSUMMON_MANUAL_DESPAWN);
                break;
            case EVENT_INFUSERS_EMOTE:
                if (Creature* controller = me->FindNearestCreature(NPC_FIGHT_CONTROLLER, 100.0f))
                    controller->AI()->Talk(EMOTE_INFUSERS_INCOMING);
                if (Creature* accuser = me->FindNearestCreature(NPC_THE_ACCUSER_ARENA, 300.0f))
                    accuser->AI()->Talk(SAY_ACCUSER_GUARDS_EN_ROUTE);
                break;
            case EVENT_SUMMON_INFUSERS:
                for (uint8 i = 0; i < InfuserSpawnPos.size(); ++i)
                    if (Creature* infuser = me->SummonCreature(NPC_SOUL_INFUSER, InfuserSpawnPos[i], TEMPSUMMON_MANUAL_DESPAWN))
                        if (infuser->IsAIEnabled())
                            infuser->AI()->SetData(DATA_INFUSER_INDEX, i);
                break;
            case EVENT_WIN_AURAS:
                me->RemoveAurasDueToSpell(SPELL_HEAL_MONITOR);
                me->RemoveAurasDueToSpell(SPELL_GIFT_OF_THE_SUN_KING);
                break;
            case EVENT_WIN_CHAINS:
                for (uint32 spellId : { SPELL_CHAINS_RIGHT, SPELL_CHAINS_LEFT, SPELL_CHAINS_RIGHT_CAST, SPELL_CHAINS_LEFT_CAST })
                    me->RemoveAurasDueToSpell(spellId);
                break;
            case EVENT_WIN_END:
                EndEncounter();
                break;
            case EVENT_WIN_CONVERSATION:
                Conversation::CreateConversation(CONVERSATION_SUN_KING_END, me, me->GetPosition(), ObjectGuid::Empty);
                break;
            case EVENT_DONE_CLEANUP:
                for (uint32 spellId : { SPELL_HEAL_MONITOR, SPELL_CHAINS_RIGHT, SPELL_CHAINS_LEFT, SPELL_GIFT_OF_THE_SUN_KING })
                    me->RemoveAurasDueToSpell(spellId);
                DespawnArenaStatics();
                SummonEndScene();
                break;
            default:
                break;
        }
    }

    // the loot is made for everyone present before DONE locks them to the encounter
    void EndEncounter()
    {
        if (GameObject* chest = me->SummonGameObject(GO_CACHE_OF_THE_SUN_KING, CachePos, QuaternionData::fromEulerAnglesZYX(CachePos.GetOrientation(), 0.0f, 0.0f), 7_days, GO_SUMMON_TIMED_DESPAWN))
        {
            GuidUnorderedSet tapList;
            std::vector<Player*> tappers;
            for (MapReference const& ref : me->GetMap()->GetPlayers())
            {
                if (Player* player = ref.GetSource())
                {
                    tapList.insert(player->GetGUID());
                    tappers.push_back(player);
                }
            }

            chest->SetTapList(tapList);
            GameObjectTemplate const* goInfo = chest->GetGOInfo();
            GameObjectTemplateAddon const* addon = chest->GetTemplateAddon();
            if (goInfo->type == GAMEOBJECT_TYPE_CHEST && goInfo->chest.chestPersonalLoot && !tappers.empty())
                chest->m_personalLoot = GenerateDungeonEncounterPersonalLoot(DUNGEON_ENCOUNTER_SUN_KING, goInfo->chest.chestPersonalLoot,
                    LootTemplates_Gameobject, LOOT_CHEST, chest, addon ? addon->Mingold : 0, addon ? addon->Maxgold : 0,
                    chest->GetLootMode(), me->GetMap()->GetMapDifficulty(), tappers);
        }

        if (_instance)
        {
            _instance->SendEncounterUnit(ENCOUNTER_FRAME_DISENGAGE, me);
            _instance->SetBossState(DATA_SUN_KINGS_SALVATION, DONE);
        }

        if (Creature* controller = me->FindNearestCreature(NPC_FIGHT_CONTROLLER, 100.0f))
            controller->CastSpell(controller, SPELL_FIGHT_CONTROLLER_END, true);

        // a despawn calls SummonedCreatureDespawn, which edits the list: work on a copy
        GuidVector const summons(_summons.begin(), _summons.end());
        for (ObjectGuid const& guid : summons)
            if (Creature* summon = ObjectAccessor::GetCreature(*me, guid))
                if (summon->GetEntry() != NPC_SHADE_OF_KAELTHAS)
                    summon->DespawnOrUnsummon();
        DespawnSpellSummons();
        DespawnArenaStatics();
        SummonEndScene();
    }

    void SetDoneState()
    {
        _stage = STAGE_WON;
        me->SetFullHealth();
        // the arena around Kael may not be loaded when he is created
        _events.ScheduleEvent(EVENT_DONE_CLEANUP, 1s);
    }

    void SummonEndScene()
    {
        if (!me->FindNearestCreature(NPC_PRINCE_RENATHAL_END, 200.0f))
            me->SummonCreature(NPC_PRINCE_RENATHAL_END, RenathalEndPos, TEMPSUMMON_MANUAL_DESPAWN);
        if (!me->FindNearestCreature(NPC_THE_ACCUSER_END, 200.0f))
            me->SummonCreature(NPC_THE_ACCUSER_END, AccuserEndPos, TEMPSUMMON_MANUAL_DESPAWN);
    }

    // retail removes the pedestals, their collision objects and the Accuser at the win
    void DespawnArenaStatics()
    {
        for (uint32 entry : { NPC_SOUL_PEDESTAL, NPC_THE_ACCUSER_ARENA })
        {
            std::list<Creature*> creatures;
            me->GetCreatureListWithEntryInGrid(creatures, entry, 300.0f);
            for (Creature* creature : creatures)
                if (!creature->IsSummon())
                    creature->DespawnOrUnsummon(0s, 7_days);
        }

        std::list<GameObject*> objects;
        me->GetGameObjectListWithEntryInGrid(objects, GO_PEDESTAL_COLLISION, 100.0f);
        for (GameObject* object : objects)
            object->DespawnOrUnsummon(0ms, 7_days);
    }

    // creatures summoned by spells of the adds (Phoenixes, Essence Fonts, Infuser's Orbs)
    void DespawnSpellSummons()
    {
        for (uint32 entry : { NPC_REBORN_PHOENIX, NPC_ESSENCE_FONT, NPC_INFUSERS_ORB })
        {
            std::list<Creature*> creatures;
            me->GetCreatureListWithEntryInGrid(creatures, entry, 200.0f);
            for (Creature* creature : creatures)
                if (creature->IsSummon())
                    creature->DespawnOrUnsummon();
        }
    }

    void CachePrePlacedAdds()
    {
        if (!_prePlacedSpawnIds.empty())
            return;

        for (uint32 entry : { NPC_VILE_OCCULTIST, NPC_BLEAKWING_ASSASSIN, NPC_HIGH_TORTURER_DARITHOS })
        {
            std::list<Creature*> creatures;
            me->GetCreatureListWithEntryInGrid(creatures, entry, EncounterRange);
            for (Creature* creature : creatures)
                if (creature->GetSpawnId())
                    _prePlacedSpawnIds.push_back(creature->GetSpawnId());
        }
    }

    void RespawnPrePlacedAdds()
    {
        for (ObjectGuid::LowType spawnId : _prePlacedSpawnIds)
        {
            Creature* found = nullptr;
            auto bounds = me->GetMap()->GetCreatureBySpawnIdStore().equal_range(spawnId);
            for (auto itr = bounds.first; itr != bounds.second; ++itr)
                found = itr->second;

            if (!found)
                me->GetMap()->Respawn(SPAWN_TYPE_CREATURE, spawnId);
            else if (!found->IsAlive())
                found->Respawn(true);
            else if (found->IsEngaged() && found->IsAIEnabled())
                found->AI()->EnterEvadeMode(EvadeReason::Other);
        }
    }

    bool AnyPlayerInArena() const
    {
        for (MapReference const& ref : me->GetMap()->GetPlayers())
            if (Player* player = ref.GetSource())
                if (player->IsAlive() && !player->IsGameMaster() && player->IsWithinDist(me, EncounterRange))
                    return true;
        return false;
    }

    void FailEncounter()
    {
        _stage = STAGE_IDLE;
        _mirror = false;
        _shadeGuid.Clear();
        _events.Reset();
        _said50 = false;
        _said75 = false;

        me->RemoveAurasDueToSpell(SPELL_GIFT_OF_THE_SUN_KING);
        me->RemoveAurasDueToSpell(SPELL_CANT_HEAL_SHADER);
        _summons.DespawnAll();
        DespawnSpellSummons();
        RespawnPrePlacedAdds();
        me->SetHealth(me->CountPctFromMaxHealth(StartHealthPct));

        if (_instance)
        {
            _instance->SendEncounterUnit(ENCOUNTER_FRAME_DISENGAGE, me);
            _instance->SetBossState(DATA_SUN_KINGS_SALVATION, FAIL);
        }
    }

private:
    InstanceScript* _instance;
    SummonList _summons;
    EventMap _events;
    SunKingStage _stage = STAGE_IDLE;
    ObjectGuid _shadeGuid;
    std::vector<ObjectGuid::LowType> _prePlacedSpawnIds;
    bool _mirror = false;
    bool _said50 = false;
    bool _said75 = false;
};

static boss_kaelthas_sun_king* GetKaelthasAI(WorldObject const* obj)
{
    if (Creature* kael = GetKaelthas(obj))
        if (kael->IsAIEnabled())
            return dynamic_cast<boss_kaelthas_sun_king*>(kael->AI());
    return nullptr;
}

// the pre-placed adds of the arena start the encounter
static void StartSunKingEncounter(Creature* add, Unit* who)
{
    if (boss_kaelthas_sun_king* kael = GetKaelthasAI(add))
        if (Creature* kaelthas = GetKaelthas(add); kaelthas && add->IsWithinDist(kaelthas, EncounterRange))
            kael->StartEncounter(who);
}

// 165805 - Shade of Kael'thas
struct npc_sun_king_shade : public ScriptedAI
{
    npc_sun_king_shade(Creature* creature) : ScriptedAI(creature) { }

    void InitializeAI() override
    {
        me->SetRegenerateHealth(false);
        ScriptedAI::InitializeAI();
    }

    void IsSummonedBy(WorldObject* /*summoner*/) override
    {
        me->SetRegenerateHealth(false);
        me->SetImmuneToPC(true);
        me->SetReactState(REACT_PASSIVE);
    }

    void SetData(uint32 type, uint32 value) override
    {
        if (type != DATA_SHADE_INDEX)
            return;

        _index = std::min<uint32>(value, 1);
        Talk(SAY_RAGE_BURNS);
        Talk(EMOTE_SHADE_EMERGED);
        DoCastSelf(SPELL_SHADE_COSMETIC, true);

        _events.ScheduleEvent(EVENT_SHADE_TELEPORT, ShadeTeleport);
        _events.ScheduleEvent(EVENT_SHADE_ENTRANCE, ShadeEntrance);
        _events.ScheduleEvent(EVENT_SHADE_ACTIVATE, ShadeActivate);
        _events.ScheduleEvent(EVENT_SHADE_ENERGY, ShadeEnergy);
        _events.ScheduleEvent(EVENT_FIERY_STRIKE, FieryStrikeFirst);
        _events.ScheduleEvent(EVENT_EMBER_BLAST, EmberBlastFirst[_index]);
    }

    // Kael's watcher ends the encounter; a Shade without targets just waits
    void EnterEvadeMode(EvadeReason /*why*/) override { }

    void JustSummoned(Creature* summon) override
    {
        if (summon->GetEntry() == NPC_REBORN_PHOENIX)
            _phoenixes.push_back(summon->GetGUID());
    }

    void DamageTaken(Unit* /*attacker*/, uint32& damage, DamageEffectType /*damageType*/, SpellInfo const* /*spellInfo*/) override
    {
        if (_done)
        {
            damage = 0;
            return;
        }

        if (_index == 1)
        {
            // the second Shade never dies: the lethal hit leaves it at 1 health and wins the encounter
            if (damage >= me->GetHealth())
            {
                damage = me->GetHealth() - 1;
                Leave(true);
            }
            return;
        }

        uint64 const left = me->GetHealth() > damage ? me->GetHealth() - damage : 0;
        if (left > me->CountPctFromMaxHealth(RetreatHealthPct))
            return;

        if (!left)
            damage = me->GetHealth() - 1;
        Leave(false);
    }

    // the retreat and the win share the Shade's exit
    void Leave(bool win)
    {
        _done = true;
        _events.Reset();
        me->InterruptNonMeleeSpells(true);
        me->AttackStop();
        me->SetReactState(REACT_PASSIVE);
        me->SetImmuneToPC(true);

        Talk(SAY_NOT_FINISHED);
        DoCastSelf(SPELL_CLEAR_EMBER_BLAST_TARGETING, true);
        DoCastSelf(SPELL_DRAW_BACK_IN, true);

        for (ObjectGuid const& guid : _phoenixes)
        {
            if (Creature* phoenix = ObjectAccessor::GetCreature(*me, guid))
            {
                if (!win)
                {
                    phoenix->CastSpell(phoenix, SPELL_REBORN_PHOENIX_EXIT, true);
                    phoenix->DespawnOrUnsummon(PhoenixExitDespawn);
                }
                else
                    phoenix->DespawnOrUnsummon();
            }
        }

        // Kael reads the Shade's health once this hit has landed
        me->m_Events.AddEventAtOffset([this, win]()
        {
            if (boss_kaelthas_sun_king* kael = GetKaelthasAI(me))
            {
                if (win)
                    kael->ShadeDefeated(me);
                else
                    kael->ShadeRetreated(me);
            }
        }, 1ms);

        me->DespawnOrUnsummon(win ? ShadeDespawnWin : ShadeDespawn);
    }

    void OnSpellCast(SpellInfo const* spellInfo) override
    {
        if (spellInfo->Id != SPELL_BLAZING_SURGE)
            return;

        for (uint8 i = 0; i < SurgePools; ++i)
        {
            me->m_Events.AddEventAtOffset([this]()
            {
                Position const dest = me->GetNearPosition(frand(5.0f, SurgeConeLength), frand(-SurgeConeHalfAngle, SurgeConeHalfAngle));
                me->CastSpell(dest, SPELL_BLAZING_SURGE_MISSILE, true);
            }, SurgePoolFirst + SurgePoolGap * i);
        }
    }

    void BlazingSurge()
    {
        me->SetPower(POWER_ENERGY, 0);
        Unit* target = SelectTarget(SelectTargetMethod::Random, 0, 300.0f, true);
        if (!target)
            return;

        Talk(SAY_ENGULFED_IN_FLAME);
        DoCast(target, SPELL_BLAZING_SURGE);
    }

    void ExecuteEvent(uint32 eventId)
    {
        switch (eventId)
        {
            case EVENT_SHADE_TELEPORT:
                me->NearTeleportTo(ShadeArenaPos);
                break;
            case EVENT_SHADE_ENTRANCE:
                DoCastSelf(SPELL_SCORCHING_ENTRANCE, true);
                DoCastSelf(SPELL_PHOENIX_REBORN, true);
                break;
            case EVENT_SHADE_ACTIVATE:
                _active = true;
                me->SetImmuneToPC(false);
                me->SetReactState(REACT_AGGRESSIVE);
                DoZoneInCombat();
                break;
            case EVENT_SHADE_ENERGY:
                me->SetPowerType(POWER_ENERGY);
                me->SetMaxPower(POWER_ENERGY, SurgeEnergy);
                me->SetPower(POWER_ENERGY, 0);
                DoCastSelf(SPELL_SHADE_ENERGY, true);
                break;
            case EVENT_FIERY_STRIKE:
                if (!me->GetVictim())
                {
                    _events.Repeat(1s);
                    break;
                }
                DoCastVictim(SPELL_FIERY_STRIKE);
                _events.Repeat(FieryStrikeRepeat);
                break;
            case EVENT_EMBER_BLAST:
                if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0, 0.0f, true))
                {
                    // 'Fire consume you!' 7 of 8 times in the capture, 'Felomin ashal!' once
                    Talk(roll_chance(12.5f) ? SAY_FELOMIN_ASHAL : SAY_FIRE_CONSUME_YOU);
                    Talk(EMOTE_EMBER_BLAST, target);
                    me->AddAura(SPELL_EMBER_BLAST_MARK, target);
                    DoCast(target, SPELL_EMBER_BLAST);
                }
                _events.Repeat(EmberBlastRepeat);
                break;
            default:
                break;
        }
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);
        if (_done)
            return;

        if (_active && !UpdateVictim())
            return;

        if (me->HasUnitState(UNIT_STATE_CASTING))
            return;

        if (_active && me->GetPowerType() == POWER_ENERGY && me->GetPower(POWER_ENERGY) >= int32(SurgeEnergy))
        {
            BlazingSurge();
            return;
        }

        while (uint32 eventId = _events.ExecuteEvent())
        {
            ExecuteEvent(eventId);
            if (me->HasUnitState(UNIT_STATE_CASTING))
                return;
        }
    }

private:
    EventMap _events;
    GuidVector _phoenixes;
    uint32 _index = 0;
    bool _active = false;
    bool _done = false;
};

// 168962 - Reborn Phoenix
struct npc_sun_king_reborn_phoenix : public ScriptedAI
{
    npc_sun_king_reborn_phoenix(Creature* creature) : ScriptedAI(creature) { }

    void InitializeAI() override
    {
        me->SetRegenerateHealth(false);
        ScriptedAI::InitializeAI();
    }

    void IsSummonedBy(WorldObject* /*summoner*/) override
    {
        me->SetRegenerateHealth(false);
        DoZoneInCombat();
        _events.ScheduleEvent(EVENT_BURNING_FIXATION, FixateFirst);
    }

    void EnterEvadeMode(EvadeReason /*why*/) override { }

    // at 0 health the Phoenix stays at 1, untargetable, and rises again at full health (5% a second)
    void DamageTaken(Unit* /*attacker*/, uint32& damage, DamageEffectType /*damageType*/, SpellInfo const* /*spellInfo*/) override
    {
        if (_reborning)
        {
            damage = 0;
            return;
        }

        if (damage < me->GetHealth())
            return;

        damage = me->GetHealth() - 1;
        _reborning = true;
        _events.Reset();
        me->InterruptNonMeleeSpells(true);
        me->RemoveAurasDueToSpell(SPELL_SMOLDERING_PLUMAGE);
        ClearFixate();
        me->AttackStop();
        me->SetReactState(REACT_PASSIVE);
        me->SetImmuneToPC(true);
        DoCastSelf(SPELL_PHOENIX_EMBERS, true);
    }

    void ClearFixate()
    {
        me->GetThreatManager().ClearFixate();
        for (MapReference const& ref : me->GetMap()->GetPlayers())
            if (Player* player = ref.GetSource())
                player->RemoveAurasDueToSpell(SPELL_EYES_ON_TARGET, me->GetGUID());
    }

    void UpdateAI(uint32 diff) override
    {
        if (_reborning)
        {
            if (me->IsFullHealth())
            {
                _reborning = false;
                me->RemoveAurasDueToSpell(SPELL_PHOENIX_EMBERS);
                DoCastSelf(SPELL_SMOLDERING_PLUMAGE, true);
                me->SetImmuneToPC(false);
                me->SetReactState(REACT_AGGRESSIVE);
                DoZoneInCombat();
                _events.ScheduleEvent(EVENT_BURNING_FIXATION, FixateFirst);
            }
            return;
        }

        if (!UpdateVictim())
            return;

        _events.Update(diff);
        if (me->HasUnitState(UNIT_STATE_CASTING))
            return;

        while (uint32 eventId = _events.ExecuteEvent())
        {
            if (eventId == EVENT_BURNING_FIXATION)
            {
                DoCastSelf(SPELL_BURNING_FIXATION, true);
                _events.Repeat(FixateRepeat);
            }
        }
    }

private:
    EventMap _events;
    bool _reborning = false;
};

// 165764 - Rockbound Vanquisher (summoned in the interlude: flies in, anchors, engages)
struct npc_sun_king_rockbound_vanquisher : public ScriptedAI
{
    npc_sun_king_rockbound_vanquisher(Creature* creature) : ScriptedAI(creature) { }

    void IsSummonedBy(WorldObject* /*summoner*/) override
    {
        _summoned = true;
        me->SetImmuneToPC(true);
        me->SetReactState(REACT_PASSIVE);
        me->SetDisableGravity(true);
        me->GetMotionMaster()->MovePoint(POINT_VANQUISHER_LAND, VanquisherLandPos, false, {}, VanquisherFlightSpeed);
    }

    void EnterEvadeMode(EvadeReason why) override
    {
        if (!_summoned)
            ScriptedAI::EnterEvadeMode(why);
    }

    void MovementInform(uint32 type, uint32 id) override
    {
        if (type != POINT_MOTION_TYPE || id != POINT_VANQUISHER_LAND)
            return;

        me->SetDisableGravity(false);
        _events.ScheduleEvent(EVENT_ANCHOR_HERE, AnchorAfterLanding);
        _events.ScheduleEvent(EVENT_VANQUISHER_ENGAGE, EngageAfterLanding);
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        _events.ScheduleEvent(EVENT_VANQUISHING_STRIKE, VanquishingStrikeFirst);
        _events.ScheduleEvent(EVENT_CONCUSSIVE_SMASH, ConcussiveSmashFirst);
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);

        if (!me->IsEngaged())
        {
            while (uint32 eventId = _events.ExecuteEvent())
            {
                if (eventId == EVENT_ANCHOR_HERE)
                    DoCastSelf(SPELL_ANCHOR_HERE, true);
                else if (eventId == EVENT_VANQUISHER_ENGAGE)
                {
                    me->SetImmuneToPC(false);
                    me->SetReactState(REACT_AGGRESSIVE);
                    DoZoneInCombat();
                    return;
                }
            }
            return;
        }

        if (!UpdateVictim())
            return;

        if (me->HasUnitState(UNIT_STATE_CASTING))
            return;

        while (uint32 eventId = _events.ExecuteEvent())
        {
            switch (eventId)
            {
                case EVENT_VANQUISHING_STRIKE:
                    DoCastVictim(SPELL_VANQUISHING_STRIKE);
                    _events.Repeat(VanquishingStrikeRepeat);
                    break;
                case EVENT_CONCUSSIVE_SMASH:
                    DoCastSelf(SPELL_CONCUSSIVE_SMASH);
                    _events.Repeat(ConcussiveSmashRepeat);
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
    bool _summoned = false;
};

// 165762 - Soul Infuser (summoned in the interlude: flies in, teleports to the arena and feeds one Soul Pedestal)
struct npc_sun_king_soul_infuser : public ScriptedAI
{
    npc_sun_king_soul_infuser(Creature* creature) : ScriptedAI(creature) { }

    void IsSummonedBy(WorldObject* /*summoner*/) override
    {
        _summoned = true;
        me->SetReactState(REACT_PASSIVE);
        me->SetDisableGravity(true);
    }

    void SetData(uint32 type, uint32 value) override
    {
        if (type != DATA_INFUSER_INDEX)
            return;

        _index = std::min<uint32>(value, InfuserHoverPos.size() - 1);
        me->GetMotionMaster()->MovePoint(POINT_INFUSER_LEG_1, InfuserLeg1, false);
        _events.ScheduleEvent(EVENT_INFUSER_TELEPORT, InfuserTeleportCast);
    }

    void EnterEvadeMode(EvadeReason why) override
    {
        if (!_summoned)
            ScriptedAI::EnterEvadeMode(why);
    }

    void MovementInform(uint32 type, uint32 id) override
    {
        if (type != POINT_MOTION_TYPE)
            return;

        if (id == POINT_INFUSER_LEG_1)
            me->GetMotionMaster()->MovePoint(POINT_INFUSER_LEG_2, InfuserLeg2[_index], false);
        else if (id == POINT_INFUSER_HOVER)
        {
            if (Creature* pedestal = me->FindNearestCreature(NPC_SOUL_PEDESTAL, 15.0f))
            {
                _pedestalGuid = pedestal->GetGUID();
                DoCast(pedestal, SPELL_SOUL_INFUSION, true);
                DoCastSelf(SPELL_INFUSER_BEAM, true);
            }
        }
    }

    void OnSpellCast(SpellInfo const* spellInfo) override
    {
        if (spellInfo->Id != SPELL_INFUSER_TELEPORT)
            return;

        me->NearTeleportTo(InfuserTeleportPos[_index]);
        _events.ScheduleEvent(EVENT_INFUSER_FLY, InfuserFlyDelay);
    }

    void JustDied(Unit* /*killer*/) override
    {
        DoCastSelf(SPELL_INFUSERS_ORB, true);
        if (Creature* pedestal = ObjectAccessor::GetCreature(*me, _pedestalGuid))
            pedestal->RemoveAurasDueToSpell(SPELL_SOUL_INFUSION, me->GetGUID());
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);
        if (me->HasUnitState(UNIT_STATE_CASTING))
            return;

        while (uint32 eventId = _events.ExecuteEvent())
        {
            switch (eventId)
            {
                case EVENT_INFUSER_TELEPORT:
                    me->GetMotionMaster()->Clear();
                    DoCastSelf(SPELL_INFUSER_TELEPORT_PREP, true);
                    me->CastSpell(InfuserTeleportPos[_index], SPELL_INFUSER_TELEPORT, false);
                    return;
                case EVENT_INFUSER_FLY:
                    me->GetMotionMaster()->MovePoint(POINT_INFUSER_HOVER, InfuserHoverPos[_index], false, {}, InfuserFlightSpeed);
                    break;
                default:
                    break;
            }
        }
    }

private:
    EventMap _events;
    ObjectGuid _pedestalGuid;
    uint32 _index = 0;
    bool _summoned = false;
};

// 170688 - Infuser's Orb
struct npc_sun_king_infusers_orb : public ScriptedAI
{
    npc_sun_king_infusers_orb(Creature* creature) : ScriptedAI(creature) { }

    void OnSpellClick(Unit* /*clicker*/, bool spellClickHandled) override
    {
        if (spellClickHandled)
            me->DespawnOrUnsummon();
    }

    void UpdateAI(uint32 /*diff*/) override { }
};

// 165778 - Essence Font (left by a Vile Occultist's death; healed to full it gives Kael 2.5% of his health)
struct npc_sun_king_essence_font : public ScriptedAI
{
    npc_sun_king_essence_font(Creature* creature) : ScriptedAI(creature) { }

    void InitializeAI() override
    {
        me->SetRegenerateHealth(false);
        ScriptedAI::InitializeAI();
    }

    void JustAppeared() override
    {
        me->SetRegenerateHealth(false);
        me->SetHealth(me->CountPctFromMaxHealth(EssenceFontStartPct));
    }

    void OnSpellCast(SpellInfo const* spellInfo) override
    {
        if (spellInfo->Id == SPELL_ESSENCE_OVERFLOW)
            me->DespawnOrUnsummon(500ms);
    }

    void UpdateAI(uint32 /*diff*/) override
    {
        if (_overflow || !me->IsFullHealth())
            return;

        if (Creature* kael = GetKaelthas(me))
        {
            _overflow = true;
            me->RemoveAurasDueToSpell(SPELL_CRACKED_RESERVOIR);
            DoCast(kael, SPELL_ESSENCE_OVERFLOW);
        }
    }

private:
    bool _overflow = false;
};

// 165763 - Vile Occultist
struct npc_sun_king_vile_occultist : public ScriptedAI
{
    npc_sun_king_vile_occultist(Creature* creature) : ScriptedAI(creature) { }

    void Reset() override
    {
        _events.Reset();
    }

    void JustEngagedWith(Unit* who) override
    {
        StartSunKingEncounter(me, who);
        DoCastSelf(SPELL_ENERGIZE, true);

        uint8 const pair = me->GetSpawnId() % 2;
        _events.ScheduleEvent(EVENT_SCORNFUL_BLAST, ScornfulBlastFirst[pair]);
        _events.ScheduleEvent(EVENT_VULGAR_BRAND, VulgarBrandFirst[pair]);
    }

    void JustDied(Unit* /*killer*/) override
    {
        DoCastSelf(SPELL_SUMMON_ESSENCE_FONT, true);
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
                case EVENT_SCORNFUL_BLAST:
                    DoCastVictim(SPELL_SCORNFUL_BLAST);
                    _events.Repeat(ScornfulBlastRepeat);
                    break;
                case EVENT_VULGAR_BRAND:
                    DoCastSelf(SPELL_VULGAR_BRAND);
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

// 167566 - Bleakwing Assassin
struct npc_sun_king_bleakwing_assassin : public ScriptedAI
{
    npc_sun_king_bleakwing_assassin(Creature* creature) : ScriptedAI(creature) { }

    void Reset() override
    {
        _events.Reset();
        _flurryTarget.Clear();
        _flurries = 0;
    }

    void JustEngagedWith(Unit* who) override
    {
        StartSunKingEncounter(me, who);
        _events.ScheduleEvent(EVENT_CRIMSON_FLURRY, CrimsonFlurryFirst[me->GetSpawnId() % 2]);
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
                case EVENT_CRIMSON_FLURRY:
                    if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0, 300.0f, true))
                    {
                        _flurryTarget = target->GetGUID();
                        me->AddAura(SPELL_CRIMSON_FLURRY_MARK, target);
                        _events.ScheduleEvent(EVENT_CRIMSON_FLURRY_TELEPORT, CrimsonFlurryTeleport);
                    }
                    _events.Repeat(_flurries++ == 0 ? CrimsonFlurrySecond : CrimsonFlurryRepeat);
                    break;
                case EVENT_CRIMSON_FLURRY_TELEPORT:
                    if (Unit* target = ObjectAccessor::GetUnit(*me, _flurryTarget); target && target->IsAlive())
                    {
                        DoCast(target, SPELL_CRIMSON_FLURRY_TELEPORT, true);
                        _events.ScheduleEvent(EVENT_CRIMSON_FLURRY_RETURN, CrimsonFlurryReturn);
                    }
                    break;
                case EVENT_CRIMSON_FLURRY_RETURN:
                    DoCastVictim(SPELL_RAPID_STRIKES_RETURN_CHARGE, true);
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
    ObjectGuid _flurryTarget;
    uint32 _flurries = 0;
};

// 168973 - High Torturer Darithos
struct npc_sun_king_high_torturer_darithos : public ScriptedAI
{
    npc_sun_king_high_torturer_darithos(Creature* creature) : ScriptedAI(creature) { }

    void Reset() override
    {
        _events.Reset();
    }

    void JustEngagedWith(Unit* who) override
    {
        StartSunKingEncounter(me, who);
        _events.ScheduleEvent(EVENT_DARITHOS_AGGRO, DarithosAggro);
        _events.ScheduleEvent(EVENT_GREATER_CASTIGATION, GreaterCastigationFirst);
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
                case EVENT_DARITHOS_AGGRO:
                    Talk(SAY_DARITHOS_AGGRO);
                    break;
                case EVENT_GREATER_CASTIGATION:
                    // up to 4 players are marked; the 2 s cast then channels on each of them
                    DoCastSelf(SPELL_GREATER_CASTIGATION_MARK, true);
                    _events.ScheduleEvent(EVENT_GREATER_CASTIGATION_WHISPER, 100ms);
                    DoCastSelf(SPELL_GREATER_CASTIGATION);
                    _events.Repeat(GreaterCastigationRepeat);
                    break;
                case EVENT_GREATER_CASTIGATION_WHISPER:
                    for (MapReference const& ref : me->GetMap()->GetPlayers())
                        if (Player* player = ref.GetSource())
                            if (player->HasAura(SPELL_GREATER_CASTIGATION_MARK, me->GetGUID()))
                                Talk(WHISPER_GREATER_CASTIGATION, player);
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

// 323397 - Heal Monitor: absorbs every heal on Kael'thas while a Shade carries Reflection of Guilt, nothing otherwise
class spell_sun_king_heal_monitor : public AuraScript
{
    void CalculateAmount(AuraEffect const* /*aurEff*/, SpellEffectValue& amount, bool& /*canBeRecalculated*/) const
    {
        // an infinite pool: the core neither reduces nor removes it, the script sets each absorb
        amount = -1;
    }

    void Absorb(AuraEffect* /*aurEff*/, HealInfo const& healInfo, uint32& absorbAmount) const
    {
        absorbAmount = AreKaelHealsBlocked(GetTarget()) ? healInfo.GetHeal() : 0;
    }

    void Register() override
    {
        DoEffectCalcAmount += AuraEffectCalcAmountFn(spell_sun_king_heal_monitor::CalculateAmount, EFFECT_0, SPELL_AURA_SCHOOL_HEAL_ABSORB);
        OnEffectAbsorbHeal += AuraEffectAbsorbHealFn(spell_sun_king_heal_monitor::Absorb, EFFECT_0);
    }
};

// 323402 - Reflection of Guilt (on the Shade, cast by Kael'thas): Kael% = 100 - Shade% every 500 ms
class spell_sun_king_reflection_of_guilt : public AuraScript
{
    void HandlePeriodic(AuraEffect const* /*aurEff*/) const
    {
        if (Creature* caster = Object::ToCreature(GetCaster()))
            if (boss_kaelthas_sun_king* kael = caster->IsAIEnabled() ? dynamic_cast<boss_kaelthas_sun_king*>(caster->AI()) : nullptr)
                kael->MirrorShade(GetTarget());
    }

    void CalculateAmount(AuraEffect const* /*aurEff*/, SpellEffectValue& amount, bool& /*canBeRecalculated*/) const
    {
        // the 1 point absorb would be used up by the first hit and take the mirror with it
        amount = -1;
    }

    void Absorb(AuraEffect* /*aurEff*/, DamageInfo& /*dmgInfo*/, uint32& absorbAmount) const
    {
        absorbAmount = 0;
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_sun_king_reflection_of_guilt::HandlePeriodic, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY);
        DoEffectCalcAmount += AuraEffectCalcAmountFn(spell_sun_king_reflection_of_guilt::CalculateAmount, EFFECT_1, SPELL_AURA_SCHOOL_ABSORB);
        OnEffectAbsorb += AuraEffectAbsorbFn(spell_sun_king_reflection_of_guilt::Absorb, EFFECT_1);
    }
};

// 325877 - Ember Blast: the damage and the 6 Lingering Embers applications are split between the players hit; a pool is left
class spell_sun_king_ember_blast : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_LINGERING_EMBERS, SPELL_EMBER_BLAST_POOL });
    }

    void CountTargets(std::list<WorldObject*>& targets)
    {
        _targets = std::max<uint32>(uint32(targets.size()), 1);
    }

    void HandleDamage(SpellEffIndex /*effIndex*/)
    {
        SetHitDamage(GetHitDamage() / int32(_targets));

        uint32 stacks = LingeringEmbersApplications / _targets;
        if (_hits < LingeringEmbersApplications % _targets)
            ++stacks;
        ++_hits;

        Unit* caster = GetCaster();
        for (uint32 i = 0; i < stacks; ++i)
            caster->CastSpell(GetHitUnit(), SPELL_LINGERING_EMBERS, true);
    }

    void HandlePool(SpellEffIndex /*effIndex*/)
    {
        if (WorldLocation const* dest = GetHitDest())
            GetCaster()->CastSpell(*dest, SPELL_EMBER_BLAST_POOL, true);
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_sun_king_ember_blast::CountTargets, EFFECT_0, TARGET_UNIT_DEST_AREA_ENEMY);
        OnEffectHitTarget += SpellEffectFn(spell_sun_king_ember_blast::HandleDamage, EFFECT_0, SPELL_EFFECT_SCHOOL_DAMAGE);
        OnEffectHit += SpellEffectFn(spell_sun_king_ember_blast::HandlePool, EFFECT_0, SPELL_EFFECT_SCHOOL_DAMAGE);
    }

    uint32 _targets = 1;
    uint32 _hits = 0;
};

// 328654 - Burning Fixation: one random player gets Eyes on Target, the Phoenix's fixate
class spell_sun_king_burning_fixation : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_EYES_ON_TARGET });
    }

    void FilterTargets(std::list<WorldObject*>& targets)
    {
        targets.remove_if([](WorldObject const* target) { return !target->IsPlayer(); });
        if (targets.size() > 1)
            Trinity::Containers::RandomResize(targets, 1);
    }

    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        GetCaster()->CastSpell(GetHitUnit(), SPELL_EYES_ON_TARGET, true);
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_sun_king_burning_fixation::FilterTargets, EFFECT_0, TARGET_UNIT_SRC_AREA_ENEMY);
        OnEffectHitTarget += SpellEffectFn(spell_sun_king_burning_fixation::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 328659 - Smoldering Plumage: every 2.5 s the Phoenix burns the players around it and leaves a pool (a PERIODIC_DUMMY casts nothing by itself)
class spell_sun_king_smoldering_plumage : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_SMOLDERING_PLUMAGE_PULSE, SPELL_PHOENIX_POOL });
    }

    void HandlePeriodic(AuraEffect const* /*aurEff*/) const
    {
        Unit* phoenix = GetTarget();
        phoenix->CastSpell(phoenix, SPELL_SMOLDERING_PLUMAGE_PULSE, true);
        phoenix->CastSpell(phoenix, SPELL_PHOENIX_POOL, true);
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_sun_king_smoldering_plumage::HandlePeriodic, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY);
    }
};

// 328885 - Greater Castigation: every marked player gets the channel
class spell_sun_king_greater_castigation : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_GREATER_CASTIGATION_MARK, SPELL_GREATER_CASTIGATION_CHANNEL });
    }

    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        Unit* caster = GetCaster();
        Unit* target = GetHitUnit();
        if (target->HasAura(SPELL_GREATER_CASTIGATION_MARK, caster->GetGUID()))
            caster->CastSpell(target, SPELL_GREATER_CASTIGATION_CHANNEL, true);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_sun_king_greater_castigation::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 328889 - Greater Castigation: each 500 ms tick hits the players around the marked one
class spell_sun_king_greater_castigation_channel : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_GREATER_CASTIGATION_DAMAGE });
    }

    void HandlePeriodic(AuraEffect const* /*aurEff*/) const
    {
        if (Unit* caster = GetCaster())
            caster->CastSpell(GetTarget()->GetPosition(), SPELL_GREATER_CASTIGATION_DAMAGE, true);
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_sun_king_greater_castigation_channel::HandlePeriodic, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY);
    }
};

// 325665 - Soul Infusion (on a Soul Pedestal, cast by a Soul Infuser): the pedestal heals the Shade 1%, or hurts Kael 1% without one
class spell_sun_king_soul_infusion : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_SOUL_INFUSION_SHADE, SPELL_SOUL_INFUSION_KAEL });
    }

    void HandlePeriodic(AuraEffect const* /*aurEff*/) const
    {
        Unit* pedestal = GetTarget();
        boss_kaelthas_sun_king* kaelAI = GetKaelthasAI(pedestal);
        Creature* kael = GetKaelthas(pedestal);
        if (!kaelAI || !kael)
            return;

        if (kaelAI->GetData(DATA_HEALS_BLOCKED))
        {
            if (Creature* shade = kaelAI->GetShade())
                pedestal->CastSpell(shade, SPELL_SOUL_INFUSION_SHADE, true);
        }
        else
            pedestal->CastSpell(kael, SPELL_SOUL_INFUSION_KAEL, true);
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_sun_king_soul_infusion::HandlePeriodic, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY);
    }
};

// 339232 - Soul Pedestal (cast by the clicker on the pedestal)
class spell_sun_king_soul_pedestal : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DRAINED_SOUL });
    }

    SpellCastResult CheckCast()
    {
        // 'Not usable while the Shade is present' (Journal)
        if (AreKaelHealsBlocked(GetCaster()))
            return SPELL_FAILED_NOT_READY;
        return SPELL_CAST_OK;
    }

    void HandleAfterCast()
    {
        GetCaster()->CastSpell(GetCaster(), SPELL_DRAINED_SOUL, true);
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(spell_sun_king_soul_pedestal::CheckCast);
        AfterCast += SpellCastFn(spell_sun_king_soul_pedestal::HandleAfterCast);
    }
};

// 339232 - Soul Pedestal: three 3 s ticks, each healing Kael'thas and draining the clicker
class spell_sun_king_soul_pedestal_aura : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_PEDESTAL_HEAL, SPELL_PEDESTAL_DAMAGE, SPELL_DRAINED_SOUL });
    }

    void HandlePeriodic(AuraEffect const* aurEff)
    {
        Unit* pedestal = GetTarget();
        if (Creature* kael = GetKaelthas(pedestal))
            pedestal->CastSpell(kael, SPELL_PEDESTAL_HEAL, true);

        if (Unit* clicker = GetCaster())
        {
            pedestal->CastSpell(clicker, SPELL_PEDESTAL_DAMAGE, CastSpellExtraArgs(TRIGGERED_FULL_MASK).SetCustomArg(uint32(aurEff->GetTickNumber())));
            clicker->CastSpell(clicker, SPELL_DRAINED_SOUL, true);
        }

        if (aurEff->GetTickNumber() >= PedestalTicks)
            Remove();
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_sun_king_soul_pedestal_aura::HandlePeriodic, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY);
    }
};

// 339243 - Soul Pedestal heal: flat (the table value has no scaling that matches the capture)
class spell_sun_king_pedestal_heal : public SpellScript
{
    void CalculateHealing(SpellEffectInfo const& /*spellEffectInfo*/, Unit* /*victim*/, int32& healing, int32& /*flatMod*/, float& /*pctMod*/) const
    {
        healing = PedestalHeal;
    }

    void Register() override
    {
        CalcHealing += SpellCalcHealingFn(spell_sun_king_pedestal_heal::CalculateHealing);
    }
};

// 339244 - Soul Pedestal damage to the clicker: 0.212% of his maximum health times 1, 3, 5
class spell_sun_king_pedestal_damage : public SpellScript
{
    void CalculateDamage(SpellEffectInfo const& /*spellEffectInfo*/, Unit* victim, int32& damage, int32& /*flatMod*/, float& /*pctMod*/) const
    {
        uint32 tick = 1;
        if (uint32 const* arg = std::any_cast<uint32>(&GetSpell()->m_customArg))
            tick = std::max<uint32>(*arg, 1);

        damage = int32(CalculatePct(float(victim->GetMaxHealth()), PedestalDamagePct * float(2 * tick - 1)));
    }

    void Register() override
    {
        CalcDamage += SpellCalcDamageFn(spell_sun_king_pedestal_damage::CalculateDamage);
    }
};

// 329561 - Essence Overflow: 2.5% of Kael's maximum health (the table value is a flat 2.5)
class spell_sun_king_essence_overflow : public SpellScript
{
    void CalculateHealing(SpellEffectInfo const& /*spellEffectInfo*/, Unit* victim, int32& healing, int32& /*flatMod*/, float& /*pctMod*/) const
    {
        healing = int32(victim->CountPctFromMaxHealth(EssenceOverflowPct));
    }

    void Register() override
    {
        CalcHealing += SpellCalcHealingFn(spell_sun_king_essence_overflow::CalculateHealing);
    }
};

// 329539 - Cracked Reservoir: the Essence Font loses 2% of its maximum times 1.05^tick every 2 s
class spell_sun_king_cracked_reservoir : public AuraScript
{
    void HandlePeriodic(AuraEffect const* aurEff) const
    {
        Unit* font = GetTarget();
        uint64 const loss = uint64(std::ceil(float(font->CountPctFromMaxHealth(aurEff->GetAmount())) * std::pow(EssenceFontDecayGrowth, float(aurEff->GetTickNumber()))));
        if (loss >= font->GetHealth())
        {
            if (Creature* creature = font->ToCreature())
                creature->DespawnOrUnsummon();
            return;
        }

        font->SetHealth(font->GetHealth() - loss);
    }

    void CalculateAmount(AuraEffect const* /*aurEff*/, SpellEffectValue& amount, bool& /*canBeRecalculated*/) const
    {
        // the 1 point heal absorb would be used up by the first heal and take the decay with it
        amount = -1;
    }

    void Absorb(AuraEffect* /*aurEff*/, HealInfo const& /*healInfo*/, uint32& absorbAmount) const
    {
        absorbAmount = 0;
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_sun_king_cracked_reservoir::HandlePeriodic, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY);
        DoEffectCalcAmount += AuraEffectCalcAmountFn(spell_sun_king_cracked_reservoir::CalculateAmount, EFFECT_1, SPELL_AURA_SCHOOL_HEAL_ABSORB);
        OnEffectAbsorbHeal += AuraEffectAbsorbHealFn(spell_sun_king_cracked_reservoir::Absorb, EFFECT_1);
    }
};

// Smoldering Remnants pools (create properties 20427 / 20428 / 20432, Mythic 22419-22421): standing in one burns
struct at_sun_king_smoldering_remnants : AreaTriggerAI
{
    using AreaTriggerAI::AreaTriggerAI;

    void OnUnitEnter(Unit* unit) override
    {
        if (!unit->IsPlayer())
            return;

        unit->CastSpell(unit, SPELL_SMOLDERING_REMNANTS, CastSpellExtraArgs(TRIGGERED_FULL_MASK).SetOriginalCaster(at->GetCasterGuid()));
    }

    void OnUnitExit(Unit* unit, AreaTriggerExitReason /*reason*/) override
    {
        if (!unit->IsPlayer())
            return;

        for (AreaTrigger const* other : unit->GetInsideAreaTriggers())
            if (other != at && IsRemnantPool(other->GetSpellId()))
                return;

        unit->RemoveAurasDueToSpell(SPELL_SMOLDERING_REMNANTS);
    }
};

// 357752 - Cache of the Sun King: the loot is made at the win; a player without his own loot may not open it (the core would
// regenerate the loot map for him and empty everyone's)
struct go_sun_king_cache : public GameObjectAI
{
    using GameObjectAI::GameObjectAI;

    bool OnGossipHello(Player* player) override
    {
        return !me->m_personalLoot.contains(player->GetGUID());
    }
};

void AddSC_boss_sun_kings_salvation()
{
    RegisterCastleNathriaCreatureAI(boss_kaelthas_sun_king);
    RegisterCastleNathriaCreatureAI(npc_sun_king_shade);
    RegisterCastleNathriaCreatureAI(npc_sun_king_reborn_phoenix);
    RegisterCastleNathriaCreatureAI(npc_sun_king_rockbound_vanquisher);
    RegisterCastleNathriaCreatureAI(npc_sun_king_soul_infuser);
    RegisterCastleNathriaCreatureAI(npc_sun_king_infusers_orb);
    RegisterCastleNathriaCreatureAI(npc_sun_king_essence_font);
    RegisterCastleNathriaCreatureAI(npc_sun_king_vile_occultist);
    RegisterCastleNathriaCreatureAI(npc_sun_king_bleakwing_assassin);
    RegisterCastleNathriaCreatureAI(npc_sun_king_high_torturer_darithos);

    RegisterSpellScript(spell_sun_king_heal_monitor);
    RegisterSpellScript(spell_sun_king_reflection_of_guilt);
    RegisterSpellScript(spell_sun_king_ember_blast);
    RegisterSpellScript(spell_sun_king_burning_fixation);
    RegisterSpellScript(spell_sun_king_smoldering_plumage);
    RegisterSpellScript(spell_sun_king_greater_castigation);
    RegisterSpellScript(spell_sun_king_greater_castigation_channel);
    RegisterSpellScript(spell_sun_king_soul_infusion);
    RegisterSpellAndAuraScriptPair(spell_sun_king_soul_pedestal, spell_sun_king_soul_pedestal_aura);
    RegisterSpellScript(spell_sun_king_pedestal_heal);
    RegisterSpellScript(spell_sun_king_pedestal_damage);
    RegisterSpellScript(spell_sun_king_essence_overflow);
    RegisterSpellScript(spell_sun_king_cracked_reservoir);

    RegisterAreaTriggerAI(at_sun_king_smoldering_remnants);
    RegisterGameObjectAI(go_sun_king_cache);
}
