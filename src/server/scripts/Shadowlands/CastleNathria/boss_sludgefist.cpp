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
 * Sludgefist (DungeonEncounter 2399). A 70-71 s rage loop with two anchors, the pull and the end of the Destructive Impact stun:
 * Colossal Roar, Chain Link, two Destructive Stomps, a second Roar, then Hateful Gaze marks a player, Sludgefist charges 6 s
 * later and the Impact stuns him for 12 s, after which every timer restarts from the same offsets. Gruesome Rage at 30% health.
 * Offsets are Warcraft Logs medians (30 Normal kills; cycle 2 matches cycle 1 within 0.3 s) and agree with BigWigs and a retail
 * capture (trinitycore-tracking/castle-nathria/bosses/sludgefist.md, sludgefist_followup.json).
 *
 * The arena, from the 2026-11-06 capture:
 * - Four pillars. A Stomp destroys the pillars its 16 yd dummy effect reaches (it hits the Pillar Stalker at each pillar centre).
 *   The charge runs at 20 yd/s along the line to the marked player and stops at the first standing pillar on that line (the
 *   pillar is a 3.15 yd half-extent square turned 45 degrees), at a closed arena door, or at the wall (navmesh edge); it tramples
 *   the players in front of him on the way. Only a charge stuns him. Pillars not spawned on a difficulty count as down.
 * - A destroyed pillar: its object opens, a rubble pile appears at its centre, two fixed Collapse Stalkers get that pillar's
 *   Structural Collapse, rim rubble falls at fixed points and piles appear there 3.75 s later. Until the first wall hit every
 *   player gets a stack of Crumbling Foundation; a wall hit replaces it with Collapsing Foundation for the rest of the fight.
 * - Falling Rubble missiles land 3 s after launch and leave a Stonequake for the rest of the fight. Launches per trigger and loop
 *   are the capture's; the spots are drawn from the capture's 49 landing points.
 *
 * Not settled (one solo Normal capture): how retail picks the missile counts and spots, whether the aimed Falling Rubble marks one
 * player or every player in a raid (one is used here), which player Giant Fists measures its 5 yd from (the primary target here),
 * the meaning of [DNT] Pillar Counter 335376 (not cast), and the double Impact with an 11 ms stun seen at the third charge.
 * The Heroic Chain Slam is not cast.
 */

#include "AreaTrigger.h"
#include "AreaTriggerAI.h"
#include "Containers.h"
#include "GameObject.h"
#include "InstanceScript.h"
#include "Log.h"
#include "Map.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "PathGenerator.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "SpellAuraEffects.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "SpellScript.h"
#include "TemporarySummon.h"
#include "castle_nathria.h"
#include <span>

enum SludgefistSpells
{
    SPELL_RAGE_GAIN                     = 331542,   // +2 energy per second from each anchor until the Gaze
    SPELL_SEISMIC_SHIFT                 = 342437,
    SPELL_GIANT_FISTS                   = 335297,   // procs on each melee swing
    SPELL_GIANT_FISTS_DAMAGE            = 335298,
    SPELL_COLOSSAL_ROAR                 = 332687,
    SPELL_COLOSSAL_ROAR_DAMAGE          = 332698,
    SPELL_CHAIN_LINK                    = 335300,
    SPELL_CHAIN_THEM                    = 342420,   // 6 s mark on both players of a pair
    SPELL_CHAIN_LINK_AURA               = 335293,   // the link itself, 2 s damage tick
    SPELL_SHATTERING_CHAIN              = 335295,   // the link broke: damage + stun
    SPELL_DESTRUCTIVE_STOMP_WARNING     = 332358,
    SPELL_DESTRUCTIVE_STOMP             = 332318,
    SPELL_HATEFUL_GAZE                  = 339057,
    SPELL_HATEFUL_GAZE_MARK             = 331209,
    SPELL_HEEDLESS_CHARGE               = 331212,
    SPELL_HEEDLESS_CHARGE_HIT           = 339067,
    SPELL_DESTRUCTIVE_IMPACT            = 332969,
    SPELL_DESTRUCTIVE_IMPACT_STUN       = 331314,
    SPELL_GRUESOME_RAGE                 = 341250,

    // arena
    SPELL_PILLAR_DESTRUCTION_FX         = 339072,   // cast by the Pillar Stalker
    SPELL_STRUCTURAL_COLLAPSE_364520    = 332811,
    SPELL_STRUCTURAL_COLLAPSE_364519    = 332814,
    SPELL_STRUCTURAL_COLLAPSE_364521    = 332810,
    SPELL_STRUCTURAL_COLLAPSE_364518    = 332812,
    SPELL_STRUCTURAL_COLLAPSE_WALL      = 332184,   // wall hit: every Collapse Stalker
    SPELL_STRUCTURAL_COLLAPSE_REMAINING = 332817,   // wall hit: the Collapse Stalkers no pillar has collapsed yet
    SPELL_VISUAL_STATE                  = 332816,   // a Collapse Stalker marks itself collapsed
    SPELL_FALLING_RUBBLE_RIM            = 347766,
    SPELL_FALLING_RUBBLE_RIM_VISUAL     = 347767,
    SPELL_CRUMBLING_FOUNDATION          = 332443,
    SPELL_CRUMBLING_FOUNDATION_DAMAGE   = 332444,
    SPELL_COLLAPSING_FOUNDATION         = 332197,
    SPELL_FALLING_RUBBLE_MISSILE        = 332552,   // lands as 332572 (6 yd, 2 s stun)
    SPELL_FALLING_RUBBLE_MISSILE_VISUAL = 332660,
    SPELL_STONEQUAKE_MISSILE            = 341101,   // effect 1 creates the Stonequake (348698)
    SPELL_FALLING_RUBBLE_AIMED          = 341193,
    SPELL_FALLING_RUBBLE_RANDOM         = 341103,
    SPELL_FALLING_RUBBLE_MARK           = 332551,
    SPELL_STONEQUAKE                    = 335361
};

enum SludgefistCreatures
{
    NPC_PILLAR_STALKER                  = 170083,
    NPC_COLLAPSE_STALKER                = 170435,
    NPC_RUBBLE_PILE_PILLAR              = 176283,
    NPC_RUBBLE_PILE_176284              = 176284,
    NPC_RUBBLE_PILE_176362              = 176362,
    NPC_RUBBLE_PILE_176363              = 176363,
    NPC_RUBBLE_PILE_176364              = 176364,
    NPC_RUBBLE_PILE_176365              = 176365,
    NPC_RUBBLE_PILE_176366              = 176366
};

enum SludgefistEvents
{
    EVENT_COLOSSAL_ROAR = 1,
    EVENT_COLOSSAL_ROAR_DAMAGE,
    EVENT_CHAIN_LINK,
    EVENT_CHAIN_LINK_APPLY,
    EVENT_DESTRUCTIVE_STOMP,
    EVENT_HATEFUL_GAZE,
    EVENT_HEEDLESS_CHARGE,
    EVENT_CHARGE_TRAMPLE,
    EVENT_STUN_END,
    EVENT_GIANT_FISTS,
    EVENT_FALLING_RUBBLE_AIMED,
    EVENT_FALLING_RUBBLE_RANDOM
};

enum SludgefistTexts
{
    SAY_AGGRO                           = 0,
    SAY_STOMP                           = 1,    // a random line on each Destructive Stomp cast start
    SAY_HATEFUL_GAZE                    = 2,
    SAY_STUN_END                        = 3,
    SAY_GRUESOME_RAGE                   = 4,
    SAY_DEATH                           = 5
};

// offsets from the anchor (the pull or the end of the stun), Warcraft Logs Normal medians
static constexpr Milliseconds RoarOffsets[] = { 0ms, 36300ms };
static constexpr Milliseconds ChainLinkOffset = 4700ms;
static constexpr Milliseconds StompOffsets[] = { 18400ms, 44000ms };
static constexpr Milliseconds GazeOffset = 51200ms;
// the timed Falling Rubble casts (capture): 13.0 / 34.9 s on the pull, 12.5 / 34.5 s after a stun; the two on the pull do nothing
static constexpr Milliseconds AimedRubbleOffset[] = { 13000ms, 12500ms };
static constexpr Milliseconds RandomRubbleOffset[] = { 34900ms, 34500ms };
static constexpr Milliseconds ChargeAfterGaze = 6s;
static constexpr Milliseconds RoarDamageDelay = 750ms;      // the roar damage lands 0.75 s after the aura
static constexpr Milliseconds ChainThemDuration = 6s;
static constexpr Milliseconds ImpactStun = 12s;
static constexpr Milliseconds GiantFistsDelay = 3200ms;     // capture: aura at 3.245 s
static constexpr Milliseconds TrampleInterval = 100ms;
static constexpr Milliseconds RubblePileDelay = 3750ms;     // piles appear 0.75 s after the rim rubble lands
static constexpr Milliseconds RubbleSecondWave = 190ms;
static constexpr Milliseconds PillarRubbleDelays[] = { 0ms, 95ms, 190ms, 295ms };
static constexpr float ChargeSpeed = 20.0f;                 // capture: all three charges
static constexpr float ChargeRayLength = 200.0f;            // longer than the arena; the charge always ends at a pillar or the wall
static constexpr float ChargeHitHalfWidth = 4.0f;           // 339067 target width 8
static constexpr float PillarHalfExtent = 3.15f;            // object GeoBox
static constexpr float PillarOrientation = 3.927f;
static constexpr float ChainBreakRange = 12.0f;
static constexpr uint8 GruesomeRagePct = 30;
static constexpr size_t PillarCount = 4;

struct RubbleWaves
{
    uint8 First;
    uint8 Second;   // RubbleSecondWave later
};

// missiles per launch, by loop (pull, second, third and later); 2026-11-06 capture
static constexpr RubbleWaves StompRubble[] = { { 2, 0 }, { 2, 1 }, { 2, 1 } };
static constexpr RubbleWaves ImpactRubble[] = { { 2, 0 }, { 2, 1 }, { 4, 2 } };
static constexpr RubbleWaves AimedRubble = { 2, 1 };        // the first lands on the marked player
static constexpr RubbleWaves RandomRubble = { 1, 1 };

// the 49 landing points of the capture
static Position const RubbleSpots[] =
{
    { -1894.07f, 6735.09f, 4319.39f }, { -1877.90f, 6744.07f, 4319.39f }, { -1894.67f, 6735.19f, 4319.39f }, { -1886.13f, 6733.68f, 4319.39f },
    { -1852.33f, 6771.05f, 4319.39f }, { -1867.09f, 6767.41f, 4319.39f }, { -1874.90f, 6764.99f, 4319.39f }, { -1899.37f, 6769.86f, 4320.19f },
    { -1854.98f, 6727.81f, 4319.39f }, { -1864.54f, 6733.97f, 4319.39f }, { -1871.89f, 6769.10f, 4319.39f }, { -1884.53f, 6785.52f, 4319.39f },
    { -1869.68f, 6740.70f, 4319.39f }, { -1892.44f, 6797.43f, 4319.39f }, { -1866.19f, 6768.96f, 4319.39f }, { -1850.67f, 6783.98f, 4319.39f },
    { -1863.52f, 6768.09f, 4319.39f }, { -1892.51f, 6780.93f, 4319.39f }, { -1883.37f, 6773.22f, 4319.39f }, { -1848.66f, 6783.54f, 4319.39f },
    { -1837.81f, 6793.07f, 4319.39f }, { -1852.93f, 6803.51f, 4319.39f }, { -1865.06f, 6735.90f, 4319.39f }, { -1863.72f, 6758.61f, 4319.39f },
    { -1839.48f, 6761.19f, 4319.39f }, { -1863.16f, 6749.51f, 4319.39f }, { -1857.27f, 6795.17f, 4319.39f }, { -1869.34f, 6776.88f, 4319.39f },
    { -1872.09f, 6746.64f, 4319.39f }, { -1836.80f, 6769.97f, 4319.39f }, { -1876.00f, 6766.30f, 4319.39f }, { -1855.42f, 6812.70f, 4319.39f },
    { -1851.33f, 6749.45f, 4319.39f }, { -1837.94f, 6770.57f, 4319.39f }, { -1871.25f, 6727.33f, 4319.39f }, { -1873.53f, 6727.39f, 4319.39f },
    { -1866.04f, 6757.71f, 4319.39f }, { -1870.22f, 6718.02f, 4319.39f }, { -1839.52f, 6770.26f, 4319.39f }, { -1865.82f, 6775.40f, 4319.39f },
    { -1838.81f, 6770.74f, 4319.39f }, { -1866.14f, 6732.59f, 4319.39f }, { -1884.04f, 6756.78f, 4319.39f }, { -1886.34f, 6738.82f, 4319.39f },
    { -1865.62f, 6742.17f, 4319.39f }, { -1880.64f, 6720.59f, 4319.39f }, { -1871.90f, 6718.70f, 4319.39f }, { -1870.74f, 6719.55f, 4319.39f },
    { -1865.81f, 6725.63f, 4319.39f }
};

struct RubblePileSpawn
{
    uint32 Entry;
    Position Pos;
};

// each rubble pile summons its own gameobject through a linked-summon aura (the pile casts it on itself when it appears)
struct RubblePileAura
{
    uint32 Entry;
    uint32 Aura;
    uint32 GameObjectSpell;
};

static constexpr RubblePileAura RubblePileAuras[] =
{
    { NPC_RUBBLE_PILE_PILLAR, 347296, 347352 },
    { NPC_RUBBLE_PILE_176284, 347587, 347588 },
    { NPC_RUBBLE_PILE_176362, 347297, 347298 },
    { NPC_RUBBLE_PILE_176363, 347583, 347582 },
    { NPC_RUBBLE_PILE_176364, 347586, 347585 },
    { NPC_RUBBLE_PILE_176365, 347590, 347589 },
    { NPC_RUBBLE_PILE_176366, 347592, 347591 }
};

static Position const Pillar364520Rim[] = { { -1898.99f, 6736.78f, 4319.39f }, { -1836.9132f, 6802.116f, 4319.3906f } };
static Position const Pillar364519Rim[] = { { -1878.08f, 6821.71f, 4319.3906f } };
static Position const Pillar364521Rim[] = { { -1845.22f, 6712.69f, 4319.3906f }, { -1830.356f, 6736.335f, 4319.3906f } };
static Position const Pillar364518Rim[] = { { -1872.29f, 6707.3f, 4319.3906f }, { -1916.34f, 6763.58f, 4320.0f } };

static RubblePileSpawn const Pillar364520Piles[] =
{
    { NPC_RUBBLE_PILE_176366, { -1896.816f, 6732.251f, 4319.3535f } },
    { NPC_RUBBLE_PILE_176365, { -1897.908f, 6740.5757f, 4319.3564f } },
    { NPC_RUBBLE_PILE_176284, { -1838.6146f, 6808.4453f, 4319.3423f } },
    { NPC_RUBBLE_PILE_176362, { -1899.8698f, 6736.229f, 4319.3604f } },
    { NPC_RUBBLE_PILE_176362, { -1835.3629f, 6802.9253f, 4319.3511f } }
};
static RubblePileSpawn const Pillar364519Piles[] =
{
    { NPC_RUBBLE_PILE_176363, { -1877.75f, 6823.4673f, 4319.4819f } },
    { NPC_RUBBLE_PILE_176366, { -1883.0348f, 6821.3691f, 4319.3286f } }
};
static RubblePileSpawn const Pillar364521Piles[] =
{
    { NPC_RUBBLE_PILE_176364, { -1821.4723f, 6774.3193f, 4320.0107f } },
    { NPC_RUBBLE_PILE_176365, { -1817.882f, 6776.5547f, 4320.0107f } },
    { NPC_RUBBLE_PILE_176364, { -1829.3038f, 6735.7803f, 4319.3623f } },
    { NPC_RUBBLE_PILE_176363, { -1844.0f, 6712.5581f, 4319.3301f } }
};
static RubblePileSpawn const Pillar364518Piles[] =
{
    { NPC_RUBBLE_PILE_176363, { -1915.8229f, 6763.585f, 4320.0107f } },
    { NPC_RUBBLE_PILE_176364, { -1871.724f, 6706.9185f, 4319.3018f } }
};

struct SludgefistPillar
{
    uint32 GameObjectEntry;
    Position Centre;
    uint32 CollapseSpell;
    std::array<Position, 2> CollapseStalkers;
    std::span<Position const> RimImpacts;
    std::span<RubblePileSpawn const> Piles;
};

static SludgefistPillar const Pillars[PillarCount] =
{
    { 364520, { -1876.6598f, 6757.1899f, 4319.3101f }, SPELL_STRUCTURAL_COLLAPSE_364520,
        { { { -1874.26f, 6739.05f }, { -1847.99f, 6798.80f } } }, Pillar364520Rim, Pillar364520Piles },
    { 364519, { -1876.6598f, 6783.1704f, 4319.3101f }, SPELL_STRUCTURAL_COLLAPSE_364519,
        { { { -1847.64f, 6764.70f }, { -1894.35f, 6762.98f } } }, Pillar364519Rim, Pillar364519Piles },
    { 364521, { -1853.9392f, 6757.1899f, 4319.3101f }, SPELL_STRUCTURAL_COLLAPSE_364521,
        { { { -1843.75f, 6730.86f }, { -1874.24f, 6810.16f } } }, Pillar364521Rim, Pillar364521Piles },
    { 364518, { -1853.9392f, 6783.1704f, 4319.3101f }, SPELL_STRUCTURAL_COLLAPSE_364518,
        { { { -1865.01f, 6722.28f }, { -1875.23f, 6770.97f } } }, Pillar364518Rim, Pillar364518Piles }
};

static SludgefistPillar const* GetPillarByCollapseSpell(uint32 spellId)
{
    for (SludgefistPillar const& pillar : Pillars)
        if (pillar.CollapseSpell == spellId)
            return &pillar;
    return nullptr;
}

struct RayBox
{
    float Min[2];   // local x, y
    float Max[2];
};

static constexpr RayBox PillarBox = { { -PillarHalfExtent, -PillarHalfExtent }, { PillarHalfExtent, PillarHalfExtent } };

// the doors in the arena walls (closed during the fight); the navmesh runs on through their doorways, so the charge is clipped
// against them. Model bounds from GameObjectModels.dtree (display 61497).
static constexpr uint32 ArenaDoors[] = { 349100, 349101, 349102 };
static constexpr RayBox ArenaDoorBox = { { -7.93f, -12.10f }, { 1.42f, 11.95f } };

// distance along a ray (unit direction dx, dy) to where it enters a box turned by orientation, or a negative value when it misses
static float RayBoxEntry(float startX, float startY, float dx, float dy, Position const& centre, float orientation, RayBox const& box, float scale = 1.0f)
{
    float c = std::cos(-orientation);
    float s = std::sin(-orientation);
    float px = startX - centre.GetPositionX();
    float py = startY - centre.GetPositionY();
    float const local[2] = { px * c - py * s, px * s + py * c };
    float const dir[2] = { dx * c - dy * s, dx * s + dy * c };

    float tMin = std::numeric_limits<float>::lowest();
    float tMax = std::numeric_limits<float>::max();
    for (uint8 axis = 0; axis < 2; ++axis)
    {
        float lo = box.Min[axis] * scale;
        float hi = box.Max[axis] * scale;
        if (std::abs(dir[axis]) < 1e-6f)
        {
            if (local[axis] < lo || local[axis] > hi)
                return -1.0f;
            continue;
        }

        float t1 = (lo - local[axis]) / dir[axis];
        float t2 = (hi - local[axis]) / dir[axis];
        if (t1 > t2)
            std::swap(t1, t2);
        tMin = std::max(tMin, t1);
        tMax = std::min(tMax, t2);
        if (tMin > tMax)
            return -1.0f;
    }

    if (tMax < 0.0f)
        return -1.0f;
    return std::max(tMin, 0.0f);
}

// retail shows these as the player's own casts; the boss is kept as the original caster so stacks and damage come from him
static void CastOnPlayers(Unit* boss, uint32 spellId)
{
    for (MapReference const& ref : boss->GetMap()->GetPlayers())
        if (Player* player = ref.GetSource())
            if (player->IsAlive() && !player->IsGameMaster())
                player->CastSpell(player, spellId, CastSpellExtraArgs(TRIGGERED_FULL_MASK).SetOriginalCaster(boss->GetGUID()));
}

// 164407 - Sludgefist
struct boss_sludgefist : public BossAI
{
    boss_sludgefist(Creature* creature) : BossAI(creature, DATA_SLUDGEFIST) { }

    void Reset() override
    {
        _Reset();
        ResetArena();
    }

    void JustEngagedWith(Unit* who) override
    {
        BossAI::JustEngagedWith(who);
        Talk(SAY_AGGRO);
        DoCastSelf(SPELL_SEISMIC_SHIFT, true);
        events.ScheduleEvent(EVENT_GIANT_FISTS, GiantFistsDelay);
        // a pillar whose object is not spawned on this difficulty is treated as already down
        for (size_t i = 0; i < PillarCount; ++i)
            _pillarDown[i] = !GetPillarObject(Pillars[i]);
        StartCycle();
    }

    void StartCycle()
    {
        ++_loop;
        DoCastSelf(SPELL_RAGE_GAIN, true);
        for (Milliseconds offset : RoarOffsets)
            events.ScheduleEvent(EVENT_COLOSSAL_ROAR, offset);
        events.ScheduleEvent(EVENT_CHAIN_LINK, ChainLinkOffset);
        for (Milliseconds offset : StompOffsets)
            events.ScheduleEvent(EVENT_DESTRUCTIVE_STOMP, offset);
        events.ScheduleEvent(EVENT_FALLING_RUBBLE_AIMED, AimedRubbleOffset[_loop > 1 ? 1 : 0]);
        events.ScheduleEvent(EVENT_FALLING_RUBBLE_RANDOM, RandomRubbleOffset[_loop > 1 ? 1 : 0]);
        events.ScheduleEvent(EVENT_HATEFUL_GAZE, GazeOffset);
    }

    void DamageTaken(Unit* /*attacker*/, uint32& damage, DamageEffectType /*damageType*/, SpellInfo const* /*spellInfo*/) override
    {
        if (!_gruesomeRage && me->HealthBelowPctDamaged(GruesomeRagePct, damage))
        {
            _gruesomeRage = true;
            Talk(SAY_GRUESOME_RAGE);
            DoCastSelf(SPELL_GRUESOME_RAGE, true);
        }
    }

    // the timed casts of the pull launch nothing (capture, BigWigs, Warcraft Logs)
    bool IsTimedRubbleActive() const { return _loop > 1; }

    RubbleWaves const& LoopWaves(RubbleWaves const (&waves)[3]) const
    {
        return waves[std::clamp<uint8>(_loop, 1, 3) - 1];
    }

    void LaunchRubbleMissile(Position const& spot)
    {
        me->CastSpell(spot, SPELL_FALLING_RUBBLE_MISSILE_VISUAL, true);
        me->CastSpell(spot, SPELL_STONEQUAKE_MISSILE, true);
        me->CastSpell(spot, SPELL_FALLING_RUBBLE_MISSILE, true);
    }

    void LaunchDelayedRubble(Position const& spot, Milliseconds delay)
    {
        if (delay <= 0ms)
        {
            LaunchRubbleMissile(spot);
            return;
        }

        me->m_Events.AddEventAtOffset([this, spot, serial = _serial]()
        {
            if (serial == _serial)
                LaunchRubbleMissile(spot);
        }, delay);
    }

    static std::vector<Position> PickRubbleSpots(size_t count)
    {
        std::vector<Position> spots(std::begin(RubbleSpots), std::end(RubbleSpots));
        Trinity::Containers::RandomShuffle(spots);
        spots.resize(std::min(count, spots.size()));
        return spots;
    }

    void LaunchRubble(RubbleWaves const& waves, Position const* aimed = nullptr)
    {
        std::vector<Position> spots = PickRubbleSpots(waves.First + waves.Second);
        if (aimed && !spots.empty())
            spots.front() = *aimed;

        for (size_t i = 0; i < spots.size(); ++i)
            LaunchDelayedRubble(spots[i], i < waves.First ? 0ms : RubbleSecondWave);
    }

    void OnStompLanded()
    {
        LaunchRubble(LoopWaves(StompRubble));
    }

    void OnTimedRubble(Unit* markedTarget)
    {
        if (!IsTimedRubbleActive())
            return;

        if (markedTarget)
        {
            Position aimed = markedTarget->GetPosition();
            LaunchRubble(AimedRubble, &aimed);
        }
        else
            LaunchRubble(RandomRubble);
    }

    GameObject* GetPillarObject(SludgefistPillar const& pillar) const
    {
        return me->FindNearestGameObject(pillar.GameObjectEntry, 300.0f);
    }

    Creature* FindCreatureAt(uint32 entry, Position const& pos, float range) const
    {
        std::list<Creature*> creatures;
        me->GetCreatureListWithEntryInGrid(creatures, entry, 300.0f);
        for (Creature* creature : creatures)
            if (creature->IsAlive() && creature->GetExactDist2d(pos) <= range)
                return creature;
        return nullptr;
    }

    void SummonRubblePile(uint32 entry, Position const& pos)
    {
        // a static spawn already standing there (older imports) keeps the spot
        if (FindCreatureAt(entry, pos, 0.5f))
            return;

        me->SummonCreature(entry, pos, TEMPSUMMON_MANUAL_DESPAWN);
    }

    void JustSummoned(Creature* summon) override
    {
        for (RubblePileAura const& pile : RubblePileAuras)
        {
            if (pile.Entry != summon->GetEntry())
                continue;

            // rubble stays after the kill (capture), so it is kept out of the boss summons and removed only on a wipe
            _rubble.push_back(summon->GetGUID());
            if (!summon->HasAura(pile.Aura))
                summon->CastSpell(summon, pile.Aura, true);
            else if (!summon->GetGameObject(pile.GameObjectSpell))
                summon->CastSpell(summon, pile.GameObjectSpell, true);
            return;
        }

        BossAI::JustSummoned(summon);
    }

    // a Stomp hit the Pillar Stalker at this position
    void OnPillarStruck(Position const& stalkerPos)
    {
        for (size_t i = 0; i < PillarCount; ++i)
            if (Pillars[i].Centre.GetExactDist2d(stalkerPos) <= 1.0f)
                DestroyPillar(i, false);
    }

    void DestroyPillar(size_t index, bool byCharge)
    {
        if (_pillarDown[index])
            return;

        _pillarDown[index] = true;
        SludgefistPillar const& pillar = Pillars[index];

        if (Creature* stalker = FindCreatureAt(NPC_PILLAR_STALKER, pillar.Centre, 1.0f))
            stalker->CastSpell(stalker, SPELL_PILLAR_DESTRUCTION_FX, true);

        if (GameObject* go = GetPillarObject(pillar))
        {
            // capture: Flags 49 with State 0, then Flags 48 one second later
            go->SetFlag(GO_FLAG_IN_USE);
            go->SetGoState(GO_STATE_ACTIVE);
            go->m_Events.AddEventAtOffset([go]() { go->RemoveFlag(GO_FLAG_IN_USE); }, 1s);
        }

        me->CastSpell(me, pillar.CollapseSpell, true);
        for (Position const& rim : pillar.RimImpacts)
        {
            me->CastSpell(rim, SPELL_FALLING_RUBBLE_RIM, true);
            me->CastSpell(rim, SPELL_FALLING_RUBBLE_RIM_VISUAL, true);
        }

        SummonRubblePile(NPC_RUBBLE_PILE_PILLAR, pillar.Centre);
        me->m_Events.AddEventAtOffset([this, index, serial = _serial]()
        {
            if (serial != _serial)
                return;
            for (RubblePileSpawn const& pile : Pillars[index].Piles)
                SummonRubblePile(pile.Entry, pile.Pos);
        }, RubblePileDelay);

        // Crumbling Foundation stacks once per pillar until a wall hit replaces it
        if (!_wallHit)
            CastOnPlayers(me, SPELL_CRUMBLING_FOUNDATION);

        // a Stomp that breaks a pillar adds four missiles; the charge's own rubble comes with the Impact
        if (!byCharge)
        {
            std::vector<Position> spots = PickRubbleSpots(std::size(PillarRubbleDelays));
            for (size_t i = 0; i < spots.size(); ++i)
                LaunchDelayedRubble(spots[i], PillarRubbleDelays[i]);
        }
    }

    void WallStruck()
    {
        me->CastSpell(me, SPELL_STRUCTURAL_COLLAPSE_REMAINING, true);
        me->CastSpell(me, SPELL_STRUCTURAL_COLLAPSE_WALL, true);
        if (_wallHit)
            return;

        _wallHit = true;
        instance->DoRemoveAurasDueToSpellOnPlayers(SPELL_CRUMBLING_FOUNDATION);
        CastOnPlayers(me, SPELL_COLLAPSING_FOUNDATION);
    }

    void DestructiveImpact()
    {
        DoCastSelf(SPELL_DESTRUCTIVE_IMPACT, true);
        DoCastSelf(SPELL_DESTRUCTIVE_IMPACT_STUN, true);
        // capture: during the stun he cannot turn, drops his target and does not swing
        me->SetUnitFlag2(UNIT_FLAG2_CANNOT_TURN);
        me->SetUnitFlag3(UNIT_FLAG3_UNK30);
        me->SetReactState(REACT_PASSIVE);
        me->AttackStop();
        LaunchRubble(LoopWaves(ImpactRubble));
        events.ScheduleEvent(EVENT_STUN_END, ImpactStun);
    }

    void EndStun()
    {
        me->RemoveUnitFlag2(UNIT_FLAG2_CANNOT_TURN);
        me->RemoveUnitFlag3(UNIT_FLAG3_UNK30);
        me->SetReactState(REACT_AGGRESSIVE);
    }

    Position ChargeWallPoint(float angle) const
    {
        Position start = me->GetPosition();
        Position end(start.GetPositionX() + std::cos(angle) * ChargeRayLength, start.GetPositionY() + std::sin(angle) * ChargeRayLength, start.GetPositionZ());

        // navmesh only: rubble collision objects must not stop the charge, standing pillars are tested separately
        PathGenerator path(me);
        path.SetUseRaycast(true);
        path.CalculatePath(start.GetPositionX(), start.GetPositionY(), start.GetPositionZ(), end.GetPositionX(), end.GetPositionY(), end.GetPositionZ(), false);
        if (!(path.GetPathType() & (PATHFIND_NOPATH | PATHFIND_NOT_USING_PATH | PATHFIND_FARFROMPOLY_START)) && !path.GetPath().empty())
        {
            G3D::Vector3 const& hit = path.GetPath().back();
            return Position(hit.x, hit.y, hit.z);
        }

        return me->GetFirstCollisionPosition(ChargeRayLength, me->GetRelativeAngle(end));
    }

    void StartCharge()
    {
        Unit* target = ObjectAccessor::GetUnit(*me, _gazeTarget);
        if (!target || !target->IsAlive())
            target = me->GetVictim();
        if (!target)
            return;

        float angle = me->GetAbsoluteAngle(target);
        _chargeDirX = std::cos(angle);
        _chargeDirY = std::sin(angle);
        Position dest = ChargeWallPoint(angle);
        float wallDist = me->GetExactDist2d(dest);
        float stopDist = wallDist;
        bool clipped = false;

        for (uint32 doorEntry : ArenaDoors)
        {
            GameObject* door = me->FindNearestGameObject(doorEntry, 300.0f);
            if (!door || door->GetGoState() != GO_STATE_READY)
                continue;

            float entry = RayBoxEntry(me->GetPositionX(), me->GetPositionY(), _chargeDirX, _chargeDirY, door->GetPosition(), door->GetOrientation(), ArenaDoorBox, door->GetObjectScale());
            if (entry >= 0.0f && entry < stopDist)
            {
                stopDist = entry;
                clipped = true;
            }
        }

        _chargePillar.reset();
        for (size_t i = 0; i < PillarCount; ++i)
        {
            if (_pillarDown[i])
                continue;

            float entry = RayBoxEntry(me->GetPositionX(), me->GetPositionY(), _chargeDirX, _chargeDirY, Pillars[i].Centre, PillarOrientation, PillarBox);
            if (entry >= 0.0f && entry < stopDist)
            {
                stopDist = entry;
                _chargePillar = i;
            }
        }

        if (_chargePillar || clipped)
        {
            dest.Relocate(me->GetPositionX() + _chargeDirX * stopDist, me->GetPositionY() + _chargeDirY * stopDist, me->GetPositionZ());
            float groundZ = me->GetMapHeight(dest.GetPositionX(), dest.GetPositionY(), dest.GetPositionZ());
            if (groundZ > INVALID_HEIGHT)
                dest.m_positionZ = groundZ;
        }

        TC_LOG_DEBUG("scripts", "Sludgefist charge: start {} angle {:.3f} wall point {:.2f} yd, stop {:.2f} yd at {} ({})", me->GetPosition().ToString(),
            angle, wallDist, stopDist, _chargePillar ? Pillars[*_chargePillar].GameObjectEntry : 0, _chargePillar ? "pillar" : clipped ? "door" : "wall");

        _trampled.clear();
        _charging = true;
        me->CastSpell(dest, SPELL_HEEDLESS_CHARGE, true);
        events.ScheduleEvent(EVENT_CHARGE_TRAMPLE, 0ms);
    }

    // only players in his path: the front half of a 4 yd circle around him, checked every 0.1 s (2 yd of travel)
    void Trample()
    {
        for (MapReference const& ref : me->GetMap()->GetPlayers())
        {
            Player* player = ref.GetSource();
            if (!player || !player->IsAlive() || player->IsGameMaster() || me->GetExactDist2d(player) > ChargeHitHalfWidth)
                continue;

            float along = (player->GetPositionX() - me->GetPositionX()) * _chargeDirX + (player->GetPositionY() - me->GetPositionY()) * _chargeDirY;
            if (along >= 0.0f && _trampled.insert(player->GetGUID()).second)
                DoCast(player, SPELL_HEEDLESS_CHARGE_HIT, true);
        }
    }

    void MovementInform(uint32 /*type*/, uint32 id) override
    {
        if (id != EVENT_CHARGE || !_charging)
            return;

        _charging = false;
        Trample();
        events.CancelEvent(EVENT_CHARGE_TRAMPLE);
        me->RemoveAurasDueToSpell(SPELL_HEEDLESS_CHARGE);

        if (_chargePillar)
            DestroyPillar(*_chargePillar, true);
        else
            WallStruck();
        DestructiveImpact();
    }

    uint32 ChainLinkPairs(uint32 players) const
    {
        // Warcraft Logs pairs per cast by raid size; a solo or duo pull gets none
        uint32 pairs;
        if (me->GetMap()->IsHeroicOrHigher())
            pairs = players <= 11 ? 3 : players <= 17 ? 4 : players <= 19 ? 5 : players <= 21 ? 6 : 7;
        else
            pairs = (players + 1) / 5;
        return std::min(pairs, players / 2);
    }

    void CastChainLink()
    {
        DoCastSelf(SPELL_CHAIN_LINK, true);
        std::vector<Player*> players;
        for (MapReference const& ref : me->GetMap()->GetPlayers())
            if (Player* player = ref.GetSource())
                if (player->IsAlive() && !player->IsGameMaster())
                    players.push_back(player);

        uint32 pairs = ChainLinkPairs(players.size());
        if (!pairs)
            return;

        Trinity::Containers::RandomShuffle(players);
        _pairs.clear();
        for (uint32 i = 0; i < pairs; ++i)
        {
            Player* first = players[i * 2];
            Player* second = players[i * 2 + 1];
            DoCast(first, SPELL_CHAIN_THEM, true);
            DoCast(second, SPELL_CHAIN_THEM, true);
            _pairs.emplace_back(first->GetGUID(), second->GetGUID());
        }
        events.ScheduleEvent(EVENT_CHAIN_LINK_APPLY, ChainThemDuration);
    }

    void ApplyChainLinks()
    {
        for (auto const& [firstGuid, secondGuid] : _pairs)
        {
            Player* first = ObjectAccessor::GetPlayer(*me, firstGuid);
            Player* second = ObjectAccessor::GetPlayer(*me, secondGuid);
            if (!first || !second || !first->IsAlive() || !second->IsAlive())
                continue;
            // each player carries the link with its partner as the caster, so the aura knows whom to measure from
            second->CastSpell(first, SPELL_CHAIN_LINK_AURA, CastSpellExtraArgs(TRIGGERED_FULL_MASK).SetOriginalCaster(second->GetGUID()));
            first->CastSpell(second, SPELL_CHAIN_LINK_AURA, CastSpellExtraArgs(TRIGGERED_FULL_MASK).SetOriginalCaster(first->GetGUID()));
        }
        _pairs.clear();
    }

    void ExecuteEvent(uint32 eventId) override
    {
        switch (eventId)
        {
            case EVENT_GIANT_FISTS:
                DoCastSelf(SPELL_GIANT_FISTS, true);
                break;
            case EVENT_COLOSSAL_ROAR:
                DoCastSelf(SPELL_COLOSSAL_ROAR, true);
                events.ScheduleEvent(EVENT_COLOSSAL_ROAR_DAMAGE, RoarDamageDelay);
                break;
            case EVENT_COLOSSAL_ROAR_DAMAGE:
                DoCastAOE(SPELL_COLOSSAL_ROAR_DAMAGE, true);
                break;
            case EVENT_CHAIN_LINK:
                CastChainLink();
                break;
            case EVENT_CHAIN_LINK_APPLY:
                ApplyChainLinks();
                break;
            case EVENT_DESTRUCTIVE_STOMP:
                DoCastSelf(SPELL_DESTRUCTIVE_STOMP_WARNING, true);
                DoCastSelf(SPELL_DESTRUCTIVE_STOMP);
                Talk(SAY_STOMP);
                break;
            case EVENT_FALLING_RUBBLE_AIMED:
                DoCastAOE(SPELL_FALLING_RUBBLE_AIMED, true);
                break;
            case EVENT_FALLING_RUBBLE_RANDOM:
                DoCastAOE(SPELL_FALLING_RUBBLE_RANDOM, true);
                break;
            case EVENT_HATEFUL_GAZE:
                me->RemoveAurasDueToSpell(SPELL_RAGE_GAIN);
                _gazeTarget.Clear();
                if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0, 0.0f, true))
                {
                    _gazeTarget = target->GetGUID();
                    DoCast(target, SPELL_HATEFUL_GAZE_MARK, true);
                }
                DoCastSelf(SPELL_HATEFUL_GAZE, true);
                Talk(SAY_HATEFUL_GAZE);
                events.ScheduleEvent(EVENT_HEEDLESS_CHARGE, ChargeAfterGaze);
                break;
            case EVENT_HEEDLESS_CHARGE:
                StartCharge();
                break;
            case EVENT_CHARGE_TRAMPLE:
                if (!_charging)
                    break;
                Trample();
                events.Repeat(TrampleInterval);
                break;
            case EVENT_STUN_END:
                EndStun();
                Talk(SAY_STUN_END);
                StartCycle();
                break;
            default:
                break;
        }
    }

    void RemovePlayerAuras()
    {
        for (uint32 spellId : { SPELL_CRUMBLING_FOUNDATION, SPELL_COLLAPSING_FOUNDATION, SPELL_CHAIN_LINK_AURA, SPELL_CHAIN_THEM,
            SPELL_HATEFUL_GAZE_MARK, SPELL_FALLING_RUBBLE_MARK, SPELL_STONEQUAKE })
            instance->DoRemoveAurasDueToSpellOnPlayers(spellId);
    }

    // a wipe: the pillars stand again, the rubble, the Stonequakes and the stalker visuals go
    void ResetArena()
    {
        ++_serial;
        _loop = 0;
        _gruesomeRage = false;
        _wallHit = false;
        _charging = false;
        _chargePillar.reset();
        _gazeTarget.Clear();
        _pairs.clear();
        _trampled.clear();
        EndStun();

        for (size_t i = 0; i < PillarCount; ++i)
        {
            _pillarDown[i] = false;
            if (GameObject* go = GetPillarObject(Pillars[i]))
            {
                go->RemoveFlag(GO_FLAG_IN_USE);
                go->SetGoState(GO_STATE_READY);
            }
        }

        for (ObjectGuid const& guid : _rubble)
            if (Creature* rubble = ObjectAccessor::GetCreature(*me, guid))
                rubble->DespawnOrUnsummon();
        _rubble.clear();

        me->RemoveAllAreaTriggers();

        std::list<Creature*> stalkers;
        me->GetCreatureListWithEntryInGrid(stalkers, NPC_COLLAPSE_STALKER, 300.0f);
        for (Creature* stalker : stalkers)
            for (uint32 spellId : { SPELL_STRUCTURAL_COLLAPSE_364520, SPELL_STRUCTURAL_COLLAPSE_364519, SPELL_STRUCTURAL_COLLAPSE_364521,
                SPELL_STRUCTURAL_COLLAPSE_364518, SPELL_STRUCTURAL_COLLAPSE_WALL, SPELL_STRUCTURAL_COLLAPSE_REMAINING, SPELL_VISUAL_STATE })
                stalker->RemoveAurasDueToSpell(spellId);

        if (instance)
            RemovePlayerAuras();
    }

    void JustDied(Unit* killer) override
    {
        BossAI::JustDied(killer);
        Talk(SAY_DEATH);
        // the destroyed pillars, the rubble and the stalker visuals stay (capture); the Stonequakes end with the fight
        me->RemoveAllAreaTriggers();
        RemovePlayerAuras();
    }

    void EnterEvadeMode(EvadeReason why) override
    {
        ResetArena();
        BossAI::EnterEvadeMode(why);
    }

private:
    uint32 _serial = 0;
    uint8 _loop = 0;
    bool _gruesomeRage = false;
    bool _wallHit = false;
    bool _charging = false;
    float _chargeDirX = 1.0f;
    float _chargeDirY = 0.0f;
    std::array<bool, PillarCount> _pillarDown = { };
    Optional<size_t> _chargePillar;
    ObjectGuid _gazeTarget;
    GuidSet _trampled;
    GuidVector _rubble;
    std::vector<std::pair<ObjectGuid, ObjectGuid>> _pairs;
};

// 335293 - Chain Link: when the two players are more than 12 yards apart the chain shatters on both
class spell_sludgefist_chain_link : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_SHATTERING_CHAIN });
    }

    void HandlePeriodic(AuraEffect const* /*aurEff*/)
    {
        Unit* owner = GetTarget();
        Unit* partner = GetCaster();
        if (!partner || !partner->IsAlive() || owner->GetDistance(partner) <= ChainBreakRange)
            return;

        owner->CastSpell(owner, SPELL_SHATTERING_CHAIN, true);
        partner->CastSpell(partner, SPELL_SHATTERING_CHAIN, true);
        partner->RemoveAurasDueToSpell(SPELL_CHAIN_LINK_AURA, owner->GetGUID());
        Remove();
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_sludgefist_chain_link::HandlePeriodic, EFFECT_0, SPELL_AURA_PERIODIC_TRIGGER_SPELL);
    }
};

// 332318 - Destructive Stomp: effect 2 reaches the Pillar Stalkers within 16 yards, and every Stomp drops rubble
class spell_sludgefist_destructive_stomp : public SpellScript
{
    void HandlePillar(SpellEffIndex /*effIndex*/)
    {
        Creature* stalker = GetHitCreature();
        if (!stalker || stalker->GetEntry() != NPC_PILLAR_STALKER)
            return;

        if (boss_sludgefist* ai = CAST_AI(boss_sludgefist, GetCaster()->GetAI()))
            ai->OnPillarStruck(stalker->GetPosition());
    }

    void HandleRubble(SpellEffIndex /*effIndex*/)
    {
        if (boss_sludgefist* ai = CAST_AI(boss_sludgefist, GetCaster()->GetAI()))
            ai->OnStompLanded();
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_sludgefist_destructive_stomp::HandlePillar, EFFECT_2, SPELL_EFFECT_DUMMY);
        OnEffectHit += SpellEffectFn(spell_sludgefist_destructive_stomp::HandleRubble, EFFECT_0, SPELL_EFFECT_SCHOOL_DAMAGE);
    }
};

// 331212 - Heedless Charge: retail charges at 20 yards per second in a straight line to the point the boss picked
class spell_sludgefist_heedless_charge : public SpellScript
{
    void HandleCharge(SpellEffIndex effIndex)
    {
        PreventHitDefaultEffect(effIndex);
        if (WorldLocation const* dest = GetExplTargetDest())
            GetCaster()->GetMotionMaster()->MoveCharge(dest->GetPositionX(), dest->GetPositionY(), dest->GetPositionZ(), ChargeSpeed, EVENT_CHARGE);
    }

    void Register() override
    {
        OnEffectLaunch += SpellEffectFn(spell_sludgefist_heedless_charge::HandleCharge, EFFECT_0, SPELL_EFFECT_CHARGE_DEST);
    }
};

// 332810, 332811, 332812, 332814 - Structural Collapse: each pillar collapses its own two Collapse Stalkers
class spell_sludgefist_structural_collapse : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_VISUAL_STATE });
    }

    void FilterTargets(std::list<WorldObject*>& targets)
    {
        SludgefistPillar const* pillar = GetPillarByCollapseSpell(GetSpellInfo()->Id);
        if (!pillar)
        {
            targets.clear();
            return;
        }

        targets.remove_if([pillar](WorldObject const* target)
        {
            return std::none_of(pillar->CollapseStalkers.begin(), pillar->CollapseStalkers.end(), [target](Position const& pos)
            {
                return target->GetExactDist2d(pos) <= 1.0f;
            });
        });
    }

    void HandleHit(SpellEffIndex /*effIndex*/)
    {
        Unit* stalker = GetHitUnit();
        stalker->CastSpell(stalker, SPELL_VISUAL_STATE, true);
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_sludgefist_structural_collapse::FilterTargets, EFFECT_0, TARGET_UNIT_SRC_AREA_ENTRY);
        OnEffectHitTarget += SpellEffectFn(spell_sludgefist_structural_collapse::HandleHit, EFFECT_0, SPELL_EFFECT_APPLY_AURA);
    }
};

// 332817 - Structural Collapse: a wall hit collapses the stalkers that no pillar has collapsed yet
class spell_sludgefist_structural_collapse_remaining : public SpellScript
{
    void FilterTargets(std::list<WorldObject*>& targets)
    {
        targets.remove_if([](WorldObject const* target)
        {
            Unit const* unit = target->ToUnit();
            return !unit || unit->HasAura(SPELL_VISUAL_STATE);
        });
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_sludgefist_structural_collapse_remaining::FilterTargets, EFFECT_0, TARGET_UNIT_SRC_AREA_ENTRY);
    }
};

// 332443 - Crumbling Foundation: every second, 332444 times the stack count (Warcraft Logs: 228 / 455 / 683 at 1 / 2 / 3 stacks)
class spell_sludgefist_crumbling_foundation : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_CRUMBLING_FOUNDATION_DAMAGE });
    }

    void HandlePeriodic(AuraEffect const* aurEff)
    {
        if (Unit* caster = GetCaster())
            caster->CastSpell(GetTarget(), SPELL_CRUMBLING_FOUNDATION_DAMAGE, CastSpellExtraArgs(aurEff));
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_sludgefist_crumbling_foundation::HandlePeriodic, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY);
    }
};

// 332444 - Crumbling Foundation
class spell_sludgefist_crumbling_foundation_damage : public SpellScript
{
    void CalculateDamage(SpellEffectInfo const& /*spellEffectInfo*/, Unit* victim, int32& damage, int32& /*flatMod*/, float& /*pctMod*/)
    {
        if (Aura const* crumbling = victim->GetAura(SPELL_CRUMBLING_FOUNDATION))
            damage *= crumbling->GetStackAmount();
    }

    void Register() override
    {
        CalcDamage += SpellCalcDamageFn(spell_sludgefist_crumbling_foundation_damage::CalculateDamage);
    }
};

// 341193 - Falling Rubble: marks one player and drops the first missile on it (the cast of the pull does nothing)
class spell_sludgefist_falling_rubble_aimed : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_FALLING_RUBBLE_MARK });
    }

    void FilterTargets(std::list<WorldObject*>& targets)
    {
        targets.remove_if([](WorldObject const* target) { return !target->IsPlayer(); });
        Trinity::Containers::RandomResize(targets, 1);
    }

    void HandleHit(SpellEffIndex /*effIndex*/)
    {
        boss_sludgefist* ai = CAST_AI(boss_sludgefist, GetCaster()->GetAI());
        if (!ai || !ai->IsTimedRubbleActive())
            return;

        Unit* target = GetHitUnit();
        target->CastSpell(target, SPELL_FALLING_RUBBLE_MARK, CastSpellExtraArgs(TRIGGERED_FULL_MASK).SetOriginalCaster(GetCaster()->GetGUID()));
        ai->OnTimedRubble(target);
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_sludgefist_falling_rubble_aimed::FilterTargets, EFFECT_0, TARGET_UNIT_SRC_AREA_ENEMY);
        OnEffectHitTarget += SpellEffectFn(spell_sludgefist_falling_rubble_aimed::HandleHit, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 341103 - Falling Rubble: two missiles at random spots (the cast of the pull does nothing)
class spell_sludgefist_falling_rubble_random : public SpellScript
{
    void HandleLaunch(SpellEffIndex /*effIndex*/)
    {
        if (boss_sludgefist* ai = CAST_AI(boss_sludgefist, GetCaster()->GetAI()))
            ai->OnTimedRubble(nullptr);
    }

    void Register() override
    {
        OnEffectHit += SpellEffectFn(spell_sludgefist_falling_rubble_random::HandleLaunch, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 341101 - Stonequake: retail never fired effect 0 (335371); effect 1 leaves the Stonequake
class spell_sludgefist_stonequake_missile : public SpellScript
{
    void PreventUnusedEffect(SpellEffIndex effIndex)
    {
        PreventHitDefaultEffect(effIndex);
    }

    void Register() override
    {
        OnEffectHit += SpellEffectFn(spell_sludgefist_stonequake_missile::PreventUnusedEffect, EFFECT_0, SPELL_EFFECT_TRIGGER_MISSILE);
    }
};

// 335297 - Giant Fists: each swing also strikes the closest other player within 5 yards of the target, or the target again
class spell_sludgefist_giant_fists : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_GIANT_FISTS_DAMAGE });
    }

    void HandleProc(AuraEffect const* aurEff, ProcEventInfo& eventInfo)
    {
        Unit* boss = GetTarget();
        Unit* victim = eventInfo.GetActionTarget();
        if (!victim)
            return;

        Unit* next = nullptr;
        float nextDist = float(aurEff->GetAmount());
        for (MapReference const& ref : boss->GetMap()->GetPlayers())
        {
            Player* player = ref.GetSource();
            if (!player || player == victim || !player->IsAlive() || player->IsGameMaster() || !boss->IsValidAttackTarget(player))
                continue;

            float dist = victim->GetDistance(player);
            if (dist <= nextDist)
            {
                nextDist = dist;
                next = player;
            }
        }

        boss->CastSpell(next ? next : victim, SPELL_GIANT_FISTS_DAMAGE, CastSpellExtraArgs(aurEff));
    }

    void Register() override
    {
        OnEffectProc += AuraEffectProcFn(spell_sludgefist_giant_fists::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// 348698 - Stonequake (areatrigger create properties 22410): a player inside slows and takes damage until it has left every Stonequake
struct at_sludgefist_stonequake : AreaTriggerAI
{
    using AreaTriggerAI::AreaTriggerAI;

    void OnUnitEnter(Unit* unit) override
    {
        if (!unit->IsPlayer())
            return;

        unit->CastSpell(unit, SPELL_STONEQUAKE, CastSpellExtraArgs(TRIGGERED_FULL_MASK).SetOriginalCaster(at->GetCasterGuid()));
    }

    void OnUnitExit(Unit* unit, AreaTriggerExitReason /*reason*/) override
    {
        if (!unit->IsPlayer())
            return;

        if (Unit* caster = at->GetCaster())
            for (AreaTrigger const* other : caster->GetAreaTriggers(at->GetSpellId()))
                if (other != at && other->GetInsideUnits().contains(unit->GetGUID()))
                    return;

        unit->RemoveAurasDueToSpell(SPELL_STONEQUAKE);
    }
};

void AddSC_boss_sludgefist()
{
    RegisterCastleNathriaCreatureAI(boss_sludgefist);
    RegisterSpellScript(spell_sludgefist_chain_link);
    RegisterSpellScript(spell_sludgefist_destructive_stomp);
    RegisterSpellScript(spell_sludgefist_heedless_charge);
    RegisterSpellScript(spell_sludgefist_structural_collapse);
    RegisterSpellScript(spell_sludgefist_structural_collapse_remaining);
    RegisterSpellScript(spell_sludgefist_crumbling_foundation);
    RegisterSpellScript(spell_sludgefist_crumbling_foundation_damage);
    RegisterSpellScript(spell_sludgefist_falling_rubble_aimed);
    RegisterSpellScript(spell_sludgefist_falling_rubble_random);
    RegisterSpellScript(spell_sludgefist_stonequake_missile);
    RegisterSpellScript(spell_sludgefist_giant_fists);
    RegisterAreaTriggerAI(at_sludgefist_stonequake);
}
