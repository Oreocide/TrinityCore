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
 * Stone Legion Generals (DungeonEncounter 2417, map 2296): General Kaal 168112 and General Grashaal 168113. Two separate health pools;
 * the grounded general stones himself at 50% (329636 Kaal, 329808 Grashaal, -95% damage taken) and Prince Renathal ends the stone
 * with Shattering Blast when his Power (0 - 1000) is full. Spec: trinitycore-tracking/castle-nathria/bosses/stone-legion-generals.md
 * and stone-legion-generals_followup.json (a retail capture of one solo Normal kill, 15 Warcraft Logs kills, Journal, DBM, BigWigs).
 *
 * Phases (each general has his own EventMap; the timers are the Warcraft Logs Normal medians, the same on every difficulty):
 * - P1: Kaal on the ground (Serrated Swipe, Wicked Blade, Heart Rend). Grashaal flies to H (-1901.89, -9184.78, 4689.21) at the pull,
 *   untargetable; his Sinsoul Bulwark (343135) makes him immune and rooted 3.26 s later and ticks Stone Spike every 4 s. He still casts
 *   Crystalize (Kaal casts a dummy twin in the same instant).
 * - I1: Kaal is stoned at 50%. The Goliath is summoned, General Draven's Aerial Offensive kills one Commando every 6.17 s (each drops an
 *   Anima Orb; a replacement flies in about 2 s later), a player carrying Anima Infusion in Renathal's Anima Hunger field gives +150
 *   Power, a Massive Anima Orb (Goliath death) gives +400. At 1000 Renathal casts Shattering Blast (5 s) on the stoned general:
 *   at cast start the other general loses Bulwark and lands at L (-1902.22, -9240.46, 4671.03); at the end the stone is replaced by
 *   Shattered, the alive Commandos die, Anima Infusion is removed, the shockwave hits and Power is 0.
 * - P2: Kaal takes off 3.16 s after the Blast (Bulwark after 4.88 s: Ricocheting Shuriken every 4 s, Wicked Blade). Grashaal is on
 *   the ground: Stone Fist, Reverberating Eruption with its Stalker echoes, Seismic Upheaval, Crystalize. I2 repeats I1 with
 *   Grashaal's stone; P3 has both on the ground after the second Blast.
 * - End: at lethal damage a Shattered general stays at 1 health in Permanent Feign Death 343288; the other swears the Soldier's Oath
 *   (336212). When the second one feigns, 0.25 s later the feign auras are removed, Kaal is killed with Unit::Kill by a player of his
 *   tap list (his loot), then Grashaal with KillSelf (not lootable on retail).
 *
 * Deviation from the capture, on purpose: the capture sends ENCOUNTER_END in the millisecond of the second feign. Here
 * SetBossState(DONE) comes from Kaal's JustDied, 0.25 s later, because DONE writes the encounter into every player's lock at once
 * and Unit::Kill (GenerateDungeonEncounterPersonalLoot) skips players already locked to it: DONE first would leave Kaal without loot.
 * The instance door 353182 follows the DoorData row (OpenWhenNotInProgress): it closes at the engage, 9.8 s before retail, and opens
 * when the encounter is done or reset. castle_nathria's OnUnitDeath still sets DONE when both generals are dead and is harmless here.
 *
 * Core: nothing is added to the core. ENERGIZE_PCT with a negative percent (Blast effect 4) wraps to a full bar, so the script empties
 * the bar itself. Entry-target effects (332963, 332989, 336137, 339693, 344718, 344721, 344740 and Kaal's twins) need the condition
 * rows of boss_stone_legion_generals.sql: an *_AREA_ENTRY target has no faction filter.
 *
 * Not settled (each is left out or a marked estimate):
 * - Enrage: DBM starts a Berserk timer at 720 s (Mythic) and 840 s (other difficulties), BigWigs only on Mythic; no spell id is named
 *   by either addon, the Journal has no section for it and no log reaches the time. Not implemented.
 * - Pulverizing Meteor is always cast 5 s after a Crystalize (93 of 98 in Warcraft Logs) although the solo capture never shows it.
 * - Seismic Upheaval: who runs the ground missiles (4 Normal, 3 LFR, 10 Heroic and Mythic marked players are used, from the MaxTargets
 *   of 334338) and its interval (low confidence, three casts).
 * - Heart Rend cluster size (spell data 2, Mythic 4; Normal showed 1 in 9 of 17 casts), the dispel path of Heart Hemorrhage (expiry is
 *   proven, a dispel is assumed from the spell text), whether Stalkers are capped at 3 (all are kept until the fight ends).
 * - The Commandos' Cluster Bombardment target pick with several players (a random player), the first cast 13.5 s after they appear.
 * - Voice lines: which Swipes and Wicked Blades are voiced and which line is used (random with the capture's rates); the Kaal Oath and
 *   Grashaal "I tire of your insolence!" pairing with the feigns (n = 1).
 * - Heroic: the extra Goliath at the pull and in P2 (Summon Reinforcements 342255). Mythic: Call Shadow Forces 342256, Skirmishers
 *   173280, Volatile Anima 342655 / Stone Shell 340037 (absent from every capture); Mythic Eruption/Heart Rend targets use the
 *   spell data only.
 * - Soultaint Effigy 343898 (Goliath at 25% Normal, 15% Heroic, 30% Mythic): its areatrigger create properties 21931 is not in the
 *   database and its shape was never captured, so Soultaint 343895 goes on every player each second directly (8 stacks, Mythic 4).
 * - A living Goliath is not despawned by the Blast (not captured). Draven's 45313 "Anchor Here" and the Renathal / general
 *   conversation spells (336081, 336089, 332757, 333228) are not cast.
 * - Chains of Suppression 173413 (6 in the capture, 4 gone 88.8 s before the pull) is not scripted.
 */

#include "AreaTrigger.h"
#include "AreaTriggerAI.h"
#include "Containers.h"
#include "GameTime.h"
#include "InstanceScript.h"
#include "Map.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "Spell.h"
#include "SpellAuraEffects.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "SpellScript.h"
#include "TemporarySummon.h"
#include "castle_nathria.h"
#include <any>
#include <functional>

enum StoneLegionSpells
{
    // stone and Blast
    SPELL_HARDENED_STONE_FORM_KAAL      = 329636,   // -95% damage taken, applied by script at 50% (the DB2 cast time is not used: START and GO share a millisecond)
    SPELL_HARDENED_STONE_FORM_GRASHAAL  = 329808,
    SPELL_SHATTERED                     = 339480,   // permanent; disables a second stone
    SPELL_SHATTERING_BLAST              = 332683,
    SPELL_SHATTERING_SHARDS             = 332965,   // bp of the DUMMY effect of 332963
    SPELL_SUICIDE                       = 45254,

    // Sinsoul Bulwark: immunity + root + a 4 s PERIODIC_DUMMY without a trigger id on Kaal's copy
    SPELL_SINSOUL_BULWARK_GRASHAAL      = 343135,
    SPELL_SINSOUL_BULWARK_KAAL          = 343126,
    SPELL_STONE_SPIKE                   = 343060,
    SPELL_RICOCHETING_SHURIKEN          = 343086,

    // Kaal
    SPELL_SERRATED_SWIPE                = 334929,
    SPELL_WICKED_BLADE_NORMAL           = 344230,
    SPELL_WICKED_BLADE                  = 333387,   // Heroic and Mythic
    SPELL_WICKED_MARK                   = 333377,
    SPELL_WICKED_SLICE                  = 344226,
    SPELL_WICKED_LACERATION             = 333913,
    SPELL_SERRATED_TEAR                 = 343881,
    SPELL_HEART_REND                    = 334765,
    SPELL_HEART_HEMORRHAGE              = 334771,
    SPELL_STONE_BREAKERS_COMBO_KAAL     = 337739,   // Kaal's dummy twins: they only mirror the casts of Grashaal
    SPELL_STONE_FIST_KAAL               = 342599,
    SPELL_REVERBERATING_ERUPTION_KAAL   = 344710,
    SPELL_SEISMIC_UPHEAVAL_KAAL         = 342569,

    // Grashaal
    SPELL_CRYSTALIZE                    = 339690,
    SPELL_CRYSTALIZE_PAIR               = 342531,
    SPELL_CRYSTALLINE_BURST             = 339693,
    SPELL_CRYSTALLINE_BURST_CLEANSE     = 350243,
    SPELL_PULVERIZING_METEOR            = 342544,
    SPELL_STONE_FIST                    = 342425,
    SPELL_REVERBERATING_ERUPTION        = 344496,
    SPELL_REVERBERATING_VULNERABILITY   = 344655,
    SPELL_ERUPTION_ARM                  = 344682,
    SPELL_UNSTABLE_GROUND               = 344503,
    SPELL_ECHO_CAST                     = 344718,   // TRIGGER_MISSILE to the position of an older Stalker
    SPELL_ECHOING_ANNIHILATION          = 344721,
    SPELL_ECHOING_BLAST                 = 344740,
    SPELL_SEISMIC_UPHEAVAL_MARK         = 334338,
    SPELL_SEISMIC_UPHEAVAL              = 334498,
    SPELL_SEISMIC_UPHEAVAL_MISSILE      = 337595,

    // death
    SPELL_SOLDIERS_OATH                 = 336212,
    SPELL_PERMANENT_FEIGN_DEATH         = 343288,

    // Renathal, Draven and the adds
    SPELL_ANIMA_HUNGER                  = 332497,
    SPELL_CONSUME_ANIMA                 = 332498,
    SPELL_FEAST_ON_ANIMA                = 339886,
    SPELL_ANIMA_INFUSION                = 332406,
    SPELL_ANIMA_INFECTION               = 339885,
    SPELL_ANIMA_ORB_CAST                = 332390,
    SPELL_ANIMA_ORB                     = 332393,
    SPELL_MASSIVE_ANIMA_ORB_CAST        = 339883,
    SPELL_MASSIVE_ANIMA_ORB             = 339884,
    SPELL_DEBILITATING_INJURY           = 331982,
    SPELL_AERIAL_OFFENSIVE              = 332231,
    SPELL_AERIAL_OFFENSIVE_STUN         = 332238,
    SPELL_CLUSTER_BOMBARDMENT           = 332150,
    SPELL_RAVENOUS_HUNGER               = 342741,
    SPELL_RAVENOUS_FEAST                = 342732,
    SPELL_SOULTAINT                     = 343895
};

enum StoneLegionCreatures
{
    NPC_RENATHAL                        = 169835,
    NPC_DRAVEN                          = 170404,
    NPC_COMMANDO                        = 169601,
    NPC_GOLIATH                         = 172858,
    NPC_ERUPTION_STALKER                = 175102
};

enum StoneLegionTexts
{
    // Kaal
    SAY_KAAL_WICKED_BLADE_A             = 1,    // "You cannot run!"
    SAY_KAAL_SWIPE_A                    = 2,    // "Tear you asunder!"
    SAY_KAAL_SWIPE_B                    = 3,    // "Bleed!"
    SAY_KAAL_STONE                      = 4,
    SAY_KAAL_WICKED_BLADE_B             = 5,    // "My blade will find you!"
    SAY_KAAL_SHATTERED                  = 6,
    SAY_KAAL_TAKEOFF                    = 7,
    SAY_KAAL_GRASHAAL_STONE             = 8,
    SAY_KAAL_BLAST_TWO                  = 9,
    SAY_KAAL_OATH                       = 10,
    SAY_KAAL_DEATH                      = 11,

    // Grashaal
    SAY_GRASHAAL_KAAL_STONE             = 1,
    SAY_GRASHAAL_BLAST_ONE              = 2,
    SAY_GRASHAAL_STONE                  = 4,
    SAY_GRASHAAL_SHATTERED              = 5,
    SAY_GRASHAAL_ERUPTION               = 6,
    SAY_GRASHAAL_DEATH                  = 7,
    SAY_GRASHAAL_FINAL                  = 8,

    // Renathal: group 0 after Kaal's stone, group 2 after Grashaal's
    SAY_RENATHAL_ANIMA_ONE              = 0,
    SAY_RENATHAL_ANIMA_TWO              = 2,

    SAY_DRAVEN_SKIES                    = 0
};

enum StoneLegionEvents
{
    // both generals
    EVENT_BULWARK = 1,
    EVENT_LANDED,

    // Kaal
    EVENT_SERRATED_SWIPE,
    EVENT_SWIPE_TALK,
    EVENT_WICKED_BLADE,
    EVENT_WICKED_SLICE,
    EVENT_HEART_REND,

    // Grashaal
    EVENT_CRYSTALIZE,
    EVENT_METEOR,
    EVENT_STONE_FIST,
    EVENT_REVERBERATING_ERUPTION,
    EVENT_SEISMIC_UPHEAVAL,

    EVENT_COUNT
};

enum StoneLegionData
{
    DATA_INTERMISSION_START             = 1,    // value: 1 after Kaal's stone, 2 after Grashaal's
    DATA_BLAST_GO                       = 2,
    DATA_FIGHT_RESET                    = 3,
    DATA_FIGHT_END                      = 4,

    ACTION_SHATTERED                    = 1     // a Commando hit by Shattering Shards
};

enum StoneLegionPoints
{
    POINT_HOVER                         = 1,
    POINT_LAND                          = 2,
    POINT_DRAVEN_NODE                   = 3,
    POINT_DRAVEN_HOME                   = 4
};

// Normal medians from Warcraft Logs (45 kills), checked against one solo capture; the first value counts from the phase anchor
// (the pull, or the Blast GO), the second from the first cast, then every Repeat.
struct TimerSequence
{
    Milliseconds First;
    Milliseconds Second;
    Milliseconds Repeat;
};

static constexpr TimerSequence SwipeP1 = { 8300ms, 24300ms, 21900ms };
static constexpr TimerSequence SwipeP3 = { 8900ms, 21900ms, 21900ms };
static constexpr TimerSequence BladeP1 = { 18400ms, 30400ms, 30400ms };
static constexpr TimerSequence BladeP2 = { 25700ms, 32000ms, 32000ms };
static constexpr TimerSequence BladeP3 = { 19800ms, 30400ms, 30400ms };
static constexpr TimerSequence HeartRendP1 = { 30300ms, 42500ms, 42500ms };
static constexpr TimerSequence HeartRendP3 = { 30800ms, 42500ms, 42500ms };
static constexpr TimerSequence CrystalizeP1 = { 24600ms, 51000ms, 51000ms };
static constexpr TimerSequence CrystalizeP2 = { 14500ms, 51100ms, 51100ms };
static constexpr TimerSequence CrystalizeP3 = { 7700ms, 50900ms, 50900ms };
static constexpr TimerSequence StoneFistP2 = { 22500ms, 19000ms, 19000ms };
static constexpr TimerSequence StoneFistP3 = { 16100ms, 19500ms, 19500ms };
static constexpr TimerSequence EruptionP2 = { 33400ms, 31900ms, 31900ms };
static constexpr TimerSequence EruptionP3 = { 25700ms, 43700ms, 43700ms };
static constexpr TimerSequence SeismicP2 = { 45500ms, 41500ms, 41500ms };      // low confidence: few casts
static constexpr TimerSequence SeismicP3 = { 39000ms, 41500ms, 41500ms };

// One scheduler: Kaal casts a dummy twin at the instant Grashaal casts the real spell, and Deadly Boss Mods models a shared
// spell-queue lock (category cooldowns of the twins: Crystalize, Seismic Upheaval and the Eruption 8 s, Wicked Blade 6 s,
// Stone Fist 3 s). An ability that falls inside the lock of an earlier cast is pushed behind it.
static constexpr uint32 LockCrystalize = 8000;
static constexpr uint32 LockSeismicUpheaval = 8000;
static constexpr uint32 LockEruption = 8000;
static constexpr uint32 LockWickedBlade = 6000;
static constexpr uint32 LockStoneFist = 3000;

// flight (capture, Normal): H = the hover point, L = the landing point, both nodes of the live waypoint paths
static Position const HoverPoint = { -1901.89f, -9184.78f, 4689.21f };
static Position const LandPoint = { -1902.22f, -9240.46f, 4671.03f };
static constexpr int32 TierTransitionUp = 581;
static constexpr int32 TierTransitionDown = 582;
static constexpr Milliseconds GrashaalTakeoffTime = 2455ms;     // Fly | CatmullRom spline, AnimTier 3
static constexpr Milliseconds KaalTakeoffTime = 4776ms;
static constexpr Milliseconds LandingTime = 2441ms;
static constexpr Milliseconds KaalTakeoffAfterBlast = 3160ms;
static constexpr Milliseconds GrashaalBulwarkAfterTakeoff = 3260ms;
static constexpr Milliseconds KaalBulwarkAfterTakeoff = 4880ms;
static constexpr Milliseconds GrashaalLandedAfterLanding = 3280ms;     // UNIT_FLAG2_UNTARGETABLE_BY_CLIENT cleared 190.11 - 186.83
static constexpr Milliseconds KaalLandedAfterLanding = 3660ms;         // 302.84 - 299.18
static constexpr Milliseconds SwipeTalkDelay = 1700ms;                 // 0.16 - 0.2 s after the 1.5 s cast
static constexpr uint32 SwipeTalkChance = 46;                          // 6 of 13 swipes
static constexpr uint32 WickedBladeTalkChance = 45;                    // 5 of 11 casts
static constexpr uint32 EruptionTalkChance = 25;                       // 1 of 4 casts
static constexpr Milliseconds WickedBladeCast = 4s;
static constexpr uint32 WickedSliceDelayMin = 240;                     // ms after the cast ends, 0.24 - 0.74 s
static constexpr uint32 WickedSliceDelayMax = 740;
static constexpr Milliseconds WickedMarkRemove = 100ms;
static constexpr Milliseconds CrystalizeDuration = 5s;
static constexpr float StonePct = 50.0f;
static constexpr Milliseconds FinaleDelay = 250ms;

// intermission, from the stone application (capture: I1 = Kaal's, I2 = Grashaal's)
static constexpr Milliseconds CommandoSpawnDelay[2] = { 806ms, 1220ms };
static constexpr Milliseconds DravenInjuryRemoved[2] = { 4900ms, 5200ms };
static constexpr Milliseconds DravenTalk[2] = { 5070ms, 5300ms };
static constexpr Milliseconds DravenFirstNode[2] = { 6100ms, 6500ms };
static constexpr Milliseconds DravenAerialOffensive[2] = { 9740ms, 10170ms };
static constexpr Milliseconds DravenFirstKill[2] = { 11650ms, 12170ms };
static constexpr Milliseconds DravenKillInterval = 6170ms;             // median, n = 25 gaps
static constexpr Milliseconds DravenMoveBeforeKill = 950ms;
static constexpr Milliseconds DravenNodeTime = 6s;
static constexpr Milliseconds DravenReturnAfterBlast = 1s;
static constexpr Milliseconds DravenInjuryAfterBlast[2] = { 4380ms, 3110ms };
static constexpr Milliseconds CommandoRefillMin = 1100ms;
static constexpr Milliseconds CommandoRefillMax = 2500ms;
static constexpr uint32 CommandoCount = 6;
static constexpr Milliseconds CommandoClusterFirst = 13500ms;          // median of the first cast after a Commando appears
static constexpr Milliseconds CommandoClusterRepeat = 8s;
static constexpr Milliseconds CommandoSuicideMin = 700ms;
static constexpr Milliseconds CommandoSuicideMax = 1800ms;
static constexpr Milliseconds CommandoCorpseTime = 10s;
static constexpr Milliseconds RenathalAnimaHunger[2] = { 12180ms, 12600ms };
static constexpr Milliseconds RenathalTalkAfterHunger = 220ms;
static constexpr uint32 RenathalMaxPower = 1000;
static constexpr uint32 BlastDelayMin = 40;                            // ms after Power reaches 1000 (sniff: 0.04 and 0.19 s)
static constexpr uint32 BlastDelayMax = 190;
static constexpr Milliseconds GoliathHungerFirst = 20720ms;
static constexpr Milliseconds GoliathHungerRepeat = 11s;
static constexpr Milliseconds GoliathFeastFirst = 28700ms;             // 29.27 and 28.06 s in the capture
static constexpr Milliseconds GoliathFeastRepeat = 18200ms;            // BigWigs; Mythic 24.3 s
static constexpr Milliseconds GoliathFeastRepeatMythic = 24300ms;
static constexpr uint8 EffigyPctNormal = 25;
static constexpr uint8 EffigyPctHeroic = 15;
static constexpr uint8 EffigyPctMythic = 30;
static constexpr uint8 SoultaintStacks = 8;
static constexpr uint8 SoultaintStacksMythic = 4;
static Position const GoliathSpawn = { -1905.89f, -9097.72f, 4681.34f, 4.53843f };

// the 16 points where the capture first saw a Commando, z 4674 - 4679 (they fly in)
static Position const CommandoPoints[] =
{
    { -1930.9f, -9119.0f, 4676.1f, 4.75f }, { -1920.7f, -9111.2f, 4676.3f, 4.75f }, { -1896.6f, -9108.5f, 4676.7f, 4.75f },
    { -1887.0f, -9118.1f, 4675.9f, 4.75f }, { -1910.5f, -9118.2f, 4676.8f, 4.75f }, { -1876.9f, -9107.6f, 4675.7f, 4.75f },
    { -1910.7f, -9125.3f, 4678.8f, 4.75f }, { -1876.4f, -9101.2f, 4674.4f, 4.75f }, { -1921.1f, -9104.8f, 4674.7f, 4.75f },
    { -1887.2f, -9111.6f, 4674.6f, 4.75f }, { -1929.9f, -9126.6f, 4677.7f, 4.75f }, { -1886.9f, -9125.5f, 4677.5f, 4.75f },
    { -1877.5f, -9115.1f, 4677.1f, 4.75f }, { -1931.7f, -9112.4f, 4674.6f, 4.75f }, { -1910.3f, -9112.0f, 4675.0f, 4.75f },
    { -1897.8f, -9102.3f, 4675.0f, 4.75f }
};

// General Draven's flight in the two intermissions: the 20 + 9 points of the capture (the live path 1451480118 holds the same 31
// nodes, nodes 21 and 31 being his spawn point); one point per Aerial Offensive kill. A longer intermission walks the list again.
static Position const DravenNodesOne[] =
{
    { -1903.5f, -9232.6f, 4699.2f }, { -1914.9f, -9261.8f, 4697.4f }, { -1931.0f, -9222.8f, 4700.8f }, { -1932.9f, -9236.3f, 4701.9f },
    { -1920.2f, -9262.4f, 4695.6f }, { -1888.0f, -9271.8f, 4695.8f }, { -1925.3f, -9257.5f, 4694.0f }, { -1910.7f, -9267.9f, 4695.6f },
    { -1898.8f, -9271.7f, 4695.8f }, { -1899.7f, -9267.2f, 4694.0f }, { -1897.7f, -9273.9f, 4699.6f }, { -1927.3f, -9265.5f, 4700.2f },
    { -1898.4f, -9267.1f, 4694.0f }, { -1912.1f, -9267.2f, 4695.6f }, { -1897.2f, -9272.0f, 4695.9f }, { -1906.5f, -9267.5f, 4694.0f },
    { -1887.1f, -9272.1f, 4701.0f }, { -1878.0f, -9263.0f, 4694.6f }, { -1866.9f, -9239.2f, 4692.2f }, { -1882.4f, -9203.2f, 4703.4f }
};
static Position const DravenNodesTwo[] =
{
    { -1903.5f, -9232.6f, 4699.2f }, { -1909.5f, -9267.1f, 4698.1f }, { -1919.7f, -9206.7f, 4703.5f }, { -1914.1f, -9214.2f, 4703.3f },
    { -1928.3f, -9252.7f, 4697.0f }, { -1896.8f, -9271.1f, 4704.3f }, { -1888.1f, -9272.5f, 4700.9f }, { -1891.9f, -9274.6f, 4702.2f },
    { -1884.9f, -9262.0f, 4703.3f }
};
static Position const DravenHome = { -1892.8f, -9269.02f, 4671.12f, 2.55655f };

// the auras of the fight that a wipe or the kill removes from the players
static constexpr uint32 PlayerAuras[] =
{
    SPELL_WICKED_MARK, SPELL_WICKED_LACERATION, SPELL_SERRATED_TEAR, SPELL_HEART_REND, SPELL_HEART_HEMORRHAGE, SPELL_CRYSTALIZE,
    SPELL_CRYSTALIZE_PAIR, SPELL_CRYSTALLINE_BURST, SPELL_CRYSTALLINE_BURST_CLEANSE, SPELL_STONE_FIST, SPELL_REVERBERATING_VULNERABILITY,
    SPELL_SEISMIC_UPHEAVAL_MARK, SPELL_SEISMIC_UPHEAVAL, SPELL_ANIMA_INFUSION, SPELL_ANIMA_INFECTION, SPELL_SOULTAINT,
    343063,     // Stone Spike slow
    342735      // Ravenous Feast stun
};

class StoneLegionGeneralAI;

// Players the script may pick for a mark: living, no game masters; the tanks (the victim of a general or a Tank specialization)
// only fill the list when fewer non-tanks exist (retail never marked a tank in Warcraft Logs; the solo capture marked its only
// player).
static std::vector<Player*> PickPlayers(Creature* reference, size_t count)
{
    GuidSet tanks;
    if (InstanceScript* instance = reference->GetInstanceScript())
        for (uint32 data : { DATA_GENERAL_KAAL, DATA_GENERAL_GRASHAAL })
            if (Creature* general = instance->GetCreature(data))
                if (Unit* victim = general->GetVictim())
                    tanks.insert(victim->GetGUID());

    std::vector<Player*> others;
    std::vector<Player*> tankPlayers;
    for (MapReference const& ref : reference->GetMap()->GetPlayers())
    {
        Player* player = ref.GetSource();
        if (!player || !player->IsAlive() || player->IsGameMaster())
            continue;

        ChrSpecializationEntry const* spec = player->GetPrimarySpecializationEntry();
        if (tanks.contains(player->GetGUID()) || (spec && spec->GetRole() == ChrSpecializationRole::Tank))
            tankPlayers.push_back(player);
        else
            others.push_back(player);
    }

    Trinity::Containers::RandomShuffle(others);
    Trinity::Containers::RandomShuffle(tankPlayers);

    std::vector<Player*> result;
    for (std::vector<Player*>* group : { &others, &tankPlayers })
        for (Player* player : *group)
            if (result.size() < count)
                result.push_back(player);
    return result;
}

// the script picked the targets (customArg) and spell_stone_legion_preselected_targets narrows the area targets to them
static CastSpellExtraArgs PreselectedTargets(std::vector<Player*> const& players, TriggerCastFlags flags)
{
    std::vector<ObjectGuid> guids;
    for (Player const* player : players)
        guids.push_back(player->GetGUID());
    return CastSpellExtraArgs(flags).SetCustomArg(std::move(guids));
}

// triggered flags that keep the cast time (a boss cast without range, line of sight and cooldown checks)
static constexpr TriggerCastFlags TimedCast = TriggerCastFlags(TRIGGERED_IGNORE_GCD | TRIGGERED_IGNORE_SPELL_AND_CATEGORY_CD | TRIGGERED_IGNORE_POWER_COST);

// ---------------------------------------------------------------------------------------------------------------------------------
// The two generals. Both are BossAI of DATA_STONE_LEGION_GENERALS: Kaal's personal loot exists only for a BossAI creature
// (InstanceScript::GetBossDungeonEncounter). The shared lock lives in Kaal's AI.
// ---------------------------------------------------------------------------------------------------------------------------------
class StoneLegionGeneralAI : public BossAI
{
public:
    StoneLegionGeneralAI(Creature* creature, bool isKaal) : BossAI(creature, DATA_STONE_LEGION_GENERALS), _isKaal(isKaal) { }

    bool IsKaal() const { return _isKaal; }
    Creature* GetGeneral() const { return me; }
    bool IsStoned() const { return _stoned; }
    bool IsFeigned() const { return _feigned; }

    void Reset() override
    {
        _Reset();
        ResetFight();
    }

    void JustEngagedWith(Unit* who) override
    {
        BossAI::JustEngagedWith(who);
        if (!instance || instance->GetBossState(DATA_STONE_LEGION_GENERALS) != IN_PROGRESS)
            return;

        // both generals fight from the first pull
        StoneLegionGeneralAI* peer = GetPeer();
        if (peer && peer->me->IsAlive() && !peer->me->IsEngaged())
            CreatureAI::DoZoneInCombat(peer->me);
        DoZoneInCombat();

        StartPull();
    }

    void JustSummoned(Creature* summon) override
    {
        summons.Summon(summon);
        if (summon->GetEntry() == NPC_GOLIATH && me->IsEngaged())
            CreatureAI::DoZoneInCombat(summon);
    }

    void UpdateAI(uint32 diff) override
    {
        // a feigned general stays out of the fight (SetImmuneToAll ended his combat); the survivor keeps the encounter running
        if (_feigned)
            return;

        BossAI::UpdateAI(diff);
    }

    // The grounded general stones himself at 50%; before that, and while stoned, nothing kills him. Only a Shattered general
    // feigns at lethal damage (retail floors him at 1 health, see FeignDeath).
    void DamageTaken(Unit* /*attacker*/, uint32& damage, DamageEffectType /*damageType*/, SpellInfo const* /*spellInfo*/) override
    {
        if (_feigned)
        {
            damage = 0;
            return;
        }

        uint32 const health = me->GetHealth();
        if (!_stoned && !_shattered && !_hovering)
        {
            uint64 const threshold = me->CountPctFromMaxHealth(StonePct);
            if (int64(health) - int64(damage) <= int64(threshold))
            {
                // the hit leaves him on the 50% line and the stone goes on at once
                damage = health > threshold ? uint32(health - threshold) : 0;
                ApplyStone();
            }
            return;
        }

        if (damage >= health)
        {
            damage = health ? health - 1 : 0;
            if (_shattered)
                FeignDeath();
        }
    }

    void JustDied(Unit* killer) override
    {
        BossAI::JustDied(killer);
        RemovePlayerAuras();
    }

    // A wipe resets both generals together; a feigned general ignores it while his partner still fights.
    void EnterEvadeMode(EvadeReason why) override
    {
        if (_evading)
            return;

        StoneLegionGeneralAI* peer = GetPeer();
        if (_feigned && peer && peer->me->IsAlive() && peer->me->IsEngaged() && !peer->_feigned)
            return;

        _evading = true;
        ResetFight();
        BossAI::EnterEvadeMode(why);
        if (peer && peer->me->IsAlive())
            peer->EnterEvadeMode(why);
        _evading = false;
    }

    void MovementInform(uint32 type, uint32 id) override
    {
        if (type != EFFECT_MOTION_TYPE)
            return;

        // the CatmullRom takeoff and landing splines end here; a hovering general stands still
        if (id == POINT_HOVER)
            me->SetFacingTo(me->GetHomePosition().GetOrientation());
    }

    // The Bulwark of the hovering general ticks every 4 s: Stone Spike (Grashaal) or Ricocheting Shuriken (Kaal) at one non-tank.
    // A tick that falls inside another cast, or inside the Crystalize channel, is dropped, not delayed.
    void OnBulwarkTick()
    {
        if (_feigned || !_hovering || me->HasUnitState(UNIT_STATE_CASTING) || GameTime::GetGameTimeMS() < _busyUntil)
            return;

        std::vector<Player*> targets = PickPlayers(me, 1);
        if (targets.empty())
            return;

        me->CastSpell(targets.front(), _isKaal ? SPELL_RICOCHETING_SHURIKEN : SPELL_STONE_SPIKE, CastSpellExtraArgs(TimedCast));
    }

    // Shattering Blast starts on this (stoned) general: the other one is released and lands
    void OnBlastStart()
    {
        StoneLegionGeneralAI* peer = GetPeer();
        if (!peer)
            return;

        peer->Release();
        peer->Talk(_isKaal ? SAY_GRASHAAL_BLAST_ONE : SAY_KAAL_BLAST_TWO);
    }

    // Shattering Blast goes off on this general: the stone is removed and Shattered keeps a second one off
    void OnBlastGo()
    {
        if (!_stoned)
            return;

        _stoned = false;
        _shattered = true;
        me->RemoveAurasDueToSpell(_isKaal ? SPELL_HARDENED_STONE_FORM_KAAL : SPELL_HARDENED_STONE_FORM_GRASHAAL);
        DoCastSelf(SPELL_SHATTERED, true);
        Later(200ms, [this]() { Talk(_isKaal ? SAY_KAAL_SHATTERED : SAY_GRASHAAL_SHATTERED); });

        uint8 const phase = _isKaal ? 2 : 3;
        SchedulePhase(phase);
        if (StoneLegionGeneralAI* peer = GetPeer())
            peer->SchedulePhase(phase);

        if (_isKaal)
            Later(KaalTakeoffAfterBlast, [this]() { TakeOff(KaalTakeoffTime); });

        for (uint32 entry : { NPC_RENATHAL, NPC_DRAVEN })
            if (Creature* npc = me->FindNearestCreature(entry, 500.0f))
                if (npc->IsAIEnabled())
                    npc->AI()->SetData(DATA_BLAST_GO, _isKaal ? 1 : 2);
    }

    virtual void SchedulePhase(uint8 phase) = 0;
    virtual void ExecuteAbility(uint32 eventId) = 0;

    void ExecuteEvent(uint32 eventId) override
    {
        switch (eventId)
        {
            case EVENT_BULWARK:
                DoCastSelf(_isKaal ? SPELL_SINSOUL_BULWARK_KAAL : SPELL_SINSOUL_BULWARK_GRASHAAL, true);
                break;
            case EVENT_LANDED:
                Landed();
                break;
            default:
                ExecuteAbility(eventId);
                break;
        }
    }

protected:
    StoneLegionGeneralAI* GetPeer() const
    {
        if (!instance)
            return nullptr;

        Creature* other = instance->GetCreature(_isKaal ? DATA_GENERAL_GRASHAAL : DATA_GENERAL_KAAL);
        if (!other || !other->IsAIEnabled())
            return nullptr;

        return dynamic_cast<StoneLegionGeneralAI*>(other->AI());
    }

    StoneLegionGeneralAI* GetKaalAI()
    {
        if (_isKaal)
            return this;
        return GetPeer();
    }

    // the cast lock of the shared scheduler lives in Kaal's AI
    uint32& SharedLock()
    {
        StoneLegionGeneralAI* kaal = GetKaalAI();
        return kaal ? kaal->_lockUntil : _lockUntil;
    }

    // true when the ability may go now; otherwise the event is pushed behind the lock and the caller stops
    bool CheckLock(uint32 lockTime)
    {
        uint32 const now = GameTime::GetGameTimeMS();
        uint32& lockUntil = SharedLock();
        if (lockUntil > now)
        {
            events.Repeat(Milliseconds(lockUntil - now));
            return false;
        }

        lockUntil = now + lockTime;
        return true;
    }

    void Later(Milliseconds delay, std::function<void()> fn)
    {
        me->m_Events.AddEventAtOffset([this, serial = _serial, fn = std::move(fn)]()
        {
            if (serial == _serial)
                fn();
        }, delay);
    }

    void Sequence(uint32 eventId, TimerSequence const& sequence)
    {
        _sequence[eventId] = sequence;
        _casts[eventId] = 0;
        events.RescheduleEvent(eventId, sequence.First);
    }

    // the next cast of a timed ability: Second after the first cast, then Repeat
    void Reschedule(uint32 eventId)
    {
        TimerSequence const& sequence = _sequence[eventId];
        events.Repeat(_casts[eventId]++ == 0 ? sequence.Second : sequence.Repeat);
    }

    void StartPull()
    {
        _casts.fill(0);
        SchedulePhase(1);
    }

    void ResetFight()
    {
        ++_serial;
        _stoned = false;
        _shattered = false;
        _feigned = false;
        _busyUntil = 0;
        _lockUntil = 0;
        _casts.fill(0);
        events.Reset();

        if (!me->IsAlive())
            return;

        for (uint32 spellId : { SPELL_HARDENED_STONE_FORM_KAAL, SPELL_HARDENED_STONE_FORM_GRASHAAL, SPELL_SHATTERED, SPELL_SINSOUL_BULWARK_KAAL,
            SPELL_SOLDIERS_OATH, SPELL_PERMANENT_FEIGN_DEATH })
            me->RemoveAurasDueToSpell(spellId);

        // Grashaal's live template and spawn carry the Bulwark: it is removed before the takeoff, the script casts it again
        // 3.26 s after the pull (retail), so the script works with or without the addon row.
        if (_hovering)
        {
            me->GetMotionMaster()->Clear();
            me->SetFloating(false);
            me->SetAnimTier(AnimTier::Ground);
            me->NearTeleportTo(me->GetHomePosition());
        }
        _hovering = false;
        me->RemoveUnitFlag2(UNIT_FLAG2_UNTARGETABLE_BY_CLIENT);
        me->SetCanMelee(true);
        me->SetReactState(REACT_AGGRESSIVE);

        if (!me->IsInCombat())
            me->SetFullHealth();

        if (instance)
        {
            RemovePlayerAuras();
            for (uint32 entry : { NPC_RENATHAL, NPC_DRAVEN })
                if (Creature* npc = me->FindNearestCreature(entry, 500.0f))
                    if (npc->IsAIEnabled())
                        npc->AI()->SetData(DATA_FIGHT_RESET, 0);
        }
    }

    void RemovePlayerAuras()
    {
        if (!instance)
            return;

        for (uint32 spellId : PlayerAuras)
            instance->DoRemoveAurasDueToSpellOnPlayers(spellId);
    }

    // takeoff: the general stops fighting on the ground and flies to H; Bulwark follows on a fixed delay
    void TakeOff(Milliseconds travelTime)
    {
        if (_feigned || !me->IsAlive())
            return;

        _hovering = true;
        me->AttackStop();
        me->SetReactState(REACT_PASSIVE);
        me->SetCanMelee(false);
        me->GetMotionMaster()->Clear();
        me->SetUnitFlag2(UNIT_FLAG2_UNTARGETABLE_BY_CLIENT);
        me->SetFloating(true);
        float const velocity = me->GetExactDist(HoverPoint) / std::chrono::duration_cast<std::chrono::duration<float>>(travelTime).count();
        me->GetMotionMaster()->MoveTakeoff(POINT_HOVER, HoverPoint, TierTransitionUp, velocity);
        if (_isKaal)
            Talk(SAY_KAAL_TAKEOFF);
        events.RescheduleEvent(EVENT_BULWARK, _isKaal ? KaalBulwarkAfterTakeoff : GrashaalBulwarkAfterTakeoff);
    }

    // released by the Blast cast start: Bulwark off, landing spline, targetable again a moment after the spline ends
    void Release()
    {
        if (_feigned || !_hovering)
            return;

        events.CancelEvent(EVENT_BULWARK);
        me->RemoveAurasDueToSpell(_isKaal ? SPELL_SINSOUL_BULWARK_KAAL : SPELL_SINSOUL_BULWARK_GRASHAAL);
        float const velocity = me->GetExactDist(LandPoint) / std::chrono::duration_cast<std::chrono::duration<float>>(LandingTime).count();
        me->GetMotionMaster()->Clear();
        me->GetMotionMaster()->MoveLand(POINT_LAND, LandPoint, TierTransitionDown, velocity);
        events.RescheduleEvent(EVENT_LANDED, _isKaal ? KaalLandedAfterLanding : GrashaalLandedAfterLanding);
    }

    void Landed()
    {
        _hovering = false;
        me->SetFloating(false);
        me->RemoveUnitFlag2(UNIT_FLAG2_UNTARGETABLE_BY_CLIENT);
        me->SetCanMelee(true);
        me->SetReactState(REACT_AGGRESSIVE);
        if (Unit* victim = me->SelectVictim())
            AttackStart(victim);
    }

    // 50%: the stone goes on, the Goliath is summoned and Draven and Renathal begin their intermission
    void ApplyStone()
    {
        _stoned = true;
        DoCastSelf(_isKaal ? SPELL_HARDENED_STONE_FORM_KAAL : SPELL_HARDENED_STONE_FORM_GRASHAAL, true);
        Talk(_isKaal ? SAY_KAAL_STONE : SAY_GRASHAAL_STONE);
        if (StoneLegionGeneralAI* peer = GetPeer())
            peer->Talk(_isKaal ? SAY_GRASHAAL_KAAL_STONE : SAY_KAAL_GRASHAAL_STONE);

        // Seismic Upheaval is not cast during Grashaal's own stone
        if (!_isKaal)
            events.CancelEvent(EVENT_SEISMIC_UPHEAVAL);

        me->SummonCreature(NPC_GOLIATH, GoliathSpawn, TEMPSUMMON_MANUAL_DESPAWN);
        for (uint32 entry : { NPC_RENATHAL, NPC_DRAVEN })
            if (Creature* npc = me->FindNearestCreature(entry, 500.0f))
                if (npc->IsAIEnabled())
                    npc->AI()->SetData(DATA_INTERMISSION_START, _isKaal ? 1 : 2);
    }

    // Lethal damage: the general stays at 1 health in Permanent Feign Death. The first one's partner swears the Soldier's Oath;
    // when the second one feigns the encounter ends.
    void FeignDeath()
    {
        _feigned = true;
        events.Reset();
        me->InterruptNonMeleeSpells(false);
        DoCastSelf(SPELL_PERMANENT_FEIGN_DEATH, true);
        Talk(_isKaal ? SAY_KAAL_DEATH : SAY_GRASHAAL_DEATH);

        StoneLegionGeneralAI* peer = GetPeer();
        if (peer && peer->me->IsAlive() && !peer->_feigned)
        {
            peer->me->CastSpell(peer->me, SPELL_SOLDIERS_OATH, true);
            if (peer->_isKaal)
                peer->Talk(SAY_KAAL_OATH);
            return;
        }

        // capture: Grashaal's last line comes with Kaal's feign
        if (peer && _isKaal)
            peer->Talk(SAY_GRASHAAL_FINAL);

        for (uint32 entry : { NPC_RENATHAL, NPC_DRAVEN })
            if (Creature* npc = me->FindNearestCreature(entry, 500.0f))
                if (npc->IsAIEnabled())
                    npc->AI()->SetData(DATA_FIGHT_END, 0);

        // The kill must come before the lock: SetBossState(DONE) writes the encounter into every player's lock at once and
        // Unit::Kill skips locked tappers when it rolls Kaal's personal loot. Kaal's JustDied then sets DONE (Grashaal's
        // KillSelf finds it done already).
        Later(FinaleDelay, [this]() { Finale(); });
    }

    void Finale()
    {
        Creature* kaal = instance ? instance->GetCreature(DATA_GENERAL_KAAL) : nullptr;
        Creature* grashaal = instance ? instance->GetCreature(DATA_GENERAL_GRASHAAL) : nullptr;
        for (Creature* general : { kaal, grashaal })
            if (general)
                general->RemoveAurasDueToSpell(SPELL_PERMANENT_FEIGN_DEATH);

        if (kaal && kaal->IsAlive())
        {
            // Unit::Kill gives the loot to the tap list; KillSelf and a null attacker would not reach a player
            Player* tapper = nullptr;
            for (ObjectGuid const& guid : kaal->GetTapList())
                if (Player* player = ObjectAccessor::GetPlayer(*kaal, guid))
                {
                    tapper = player;
                    if (player->IsAlive())
                        break;
                }

            if (tapper)
                Unit::Kill(tapper, kaal);
            else
                kaal->KillSelf();
        }

        // Grashaal is not lootable on retail
        if (grashaal && grashaal->IsAlive())
            grashaal->KillSelf();
    }

    bool _isKaal;
    bool _stoned = false;
    bool _shattered = false;
    bool _hovering = false;
    bool _feigned = false;
    bool _evading = false;
    uint32 _serial = 0;
    uint32 _busyUntil = 0;
    uint32 _lockUntil = 0;
    std::array<TimerSequence, EVENT_COUNT> _sequence = { };
    std::array<uint8, EVENT_COUNT> _casts = { };
};

// 168112 - General Kaal
struct boss_general_kaal : public StoneLegionGeneralAI
{
    boss_general_kaal(Creature* creature) : StoneLegionGeneralAI(creature, true) { }

    void SchedulePhase(uint8 phase) override
    {
        events.CancelEvent(EVENT_SERRATED_SWIPE);
        events.CancelEvent(EVENT_WICKED_BLADE);
        events.CancelEvent(EVENT_HEART_REND);

        switch (phase)
        {
            case 1:
                Sequence(EVENT_SERRATED_SWIPE, SwipeP1);
                Sequence(EVENT_WICKED_BLADE, BladeP1);
                Sequence(EVENT_HEART_REND, HeartRendP1);
                break;
            case 2:
                // airborne: no Swipe, no Heart Rend
                Sequence(EVENT_WICKED_BLADE, BladeP2);
                break;
            default:
                Sequence(EVENT_SERRATED_SWIPE, SwipeP3);
                Sequence(EVENT_WICKED_BLADE, BladeP3);
                Sequence(EVENT_HEART_REND, HeartRendP3);
                break;
        }
    }

    void ExecuteAbility(uint32 eventId) override
    {
        switch (eventId)
        {
            case EVENT_SERRATED_SWIPE:
            {
                Unit* victim = me->GetVictim();
                if (!victim || _hovering)
                {
                    events.Repeat(1s);
                    break;
                }
                me->CastSpell(victim, SPELL_SERRATED_SWIPE, CastSpellExtraArgs(TimedCast));
                if ((urand(1, 100) <= SwipeTalkChance))
                    events.RescheduleEvent(EVENT_SWIPE_TALK, SwipeTalkDelay);
                Reschedule(eventId);
                break;
            }
            case EVENT_SWIPE_TALK:
                Talk(urand(0, 1) ? SAY_KAAL_SWIPE_B : SAY_KAAL_SWIPE_A);
                break;
            case EVENT_WICKED_BLADE:
            {
                if (!CheckLock(LockWickedBlade))
                    break;

                // two non-tanks are marked at cast start (333377, 10 s); the cast lasts 4 s, Wicked Slice follows 0.24 - 0.74 s later
                std::vector<Player*> targets = PickPlayers(me, 2);
                if (targets.empty())
                {
                    Reschedule(eventId);
                    break;
                }

                _wickedTargets.clear();
                for (Player* player : targets)
                {
                    me->CastSpell(player, SPELL_WICKED_MARK, true);
                    _wickedTargets.push_back(player->GetGUID());
                }

                if ((urand(1, 100) <= WickedBladeTalkChance))
                    Talk(urand(0, 1) ? SAY_KAAL_WICKED_BLADE_B : SAY_KAAL_WICKED_BLADE_A);

                me->CastSpell(targets.front(), me->GetMap()->IsHeroicOrHigher() ? SPELL_WICKED_BLADE : SPELL_WICKED_BLADE_NORMAL, CastSpellExtraArgs(TimedCast));
                events.RescheduleEvent(EVENT_WICKED_SLICE, WickedBladeCast + Milliseconds(urand(WickedSliceDelayMin, WickedSliceDelayMax)));
                Reschedule(eventId);
                break;
            }
            case EVENT_WICKED_SLICE:
            {
                for (ObjectGuid const& guid : _wickedTargets)
                {
                    Player* player = ObjectAccessor::GetPlayer(*me, guid);
                    if (!player || !player->IsAlive())
                        continue;

                    me->CastSpell(player, SPELL_WICKED_SLICE, true);
                    me->CastSpell(player, SPELL_WICKED_LACERATION, true);
                    Later(WickedMarkRemove, [this, guid]()
                    {
                        if (Player* marked = ObjectAccessor::GetPlayer(*me, guid))
                            marked->RemoveAurasDueToSpell(SPELL_WICKED_MARK, me->GetGUID());
                    });
                }
                _wickedTargets.clear();
                break;
            }
            case EVENT_HEART_REND:
            {
                // 2 non-tanks (4 on Mythic), 36 s; the aura script gives Heart Hemorrhage when it ends
                std::vector<Player*> targets = PickPlayers(me, me->GetMap()->IsMythic() ? 4 : 2);
                if (targets.empty())
                {
                    Reschedule(eventId);
                    break;
                }
                me->CastSpell(me, SPELL_HEART_REND, PreselectedTargets(targets, TimedCast));
                Reschedule(eventId);
                break;
            }
            default:
                break;
        }
    }

private:
    GuidVector _wickedTargets;
};

// 168113 - General Grashaal
struct boss_general_grashaal : public StoneLegionGeneralAI
{
    boss_general_grashaal(Creature* creature) : StoneLegionGeneralAI(creature, false) { }

    void JustEngagedWith(Unit* who) override
    {
        StoneLegionGeneralAI::JustEngagedWith(who);
        if (!instance || instance->GetBossState(DATA_STONE_LEGION_GENERALS) != IN_PROGRESS)
            return;

        // t = 0: Grashaal flies to H. The addon Bulwark (immune, rooted) would hold him on the ground: off before the takeoff.
        me->RemoveAurasDueToSpell(SPELL_SINSOUL_BULWARK_GRASHAAL);
        TakeOff(GrashaalTakeoffTime);
    }

    void SchedulePhase(uint8 phase) override
    {
        events.CancelEvent(EVENT_CRYSTALIZE);
        events.CancelEvent(EVENT_STONE_FIST);
        events.CancelEvent(EVENT_REVERBERATING_ERUPTION);
        events.CancelEvent(EVENT_SEISMIC_UPHEAVAL);

        switch (phase)
        {
            case 1:
                Sequence(EVENT_CRYSTALIZE, CrystalizeP1);
                break;
            case 2:
                Sequence(EVENT_CRYSTALIZE, CrystalizeP2);
                Sequence(EVENT_STONE_FIST, StoneFistP2);
                Sequence(EVENT_REVERBERATING_ERUPTION, EruptionP2);
                Sequence(EVENT_SEISMIC_UPHEAVAL, SeismicP2);
                break;
            default:
                Sequence(EVENT_CRYSTALIZE, CrystalizeP3);
                Sequence(EVENT_STONE_FIST, StoneFistP3);
                Sequence(EVENT_REVERBERATING_ERUPTION, EruptionP3);
                Sequence(EVENT_SEISMIC_UPHEAVAL, SeismicP3);
                break;
        }
    }

    Creature* GetKaal() const
    {
        return instance ? instance->GetCreature(DATA_GENERAL_KAAL) : nullptr;
    }

    // Kaal casts a dummy twin in the same millisecond
    void CastTwin(uint32 spellId)
    {
        if (Creature* kaal = GetKaal())
            if (kaal->IsAlive())
                kaal->CastSpell(kaal, spellId, true);
    }

    void ExecuteAbility(uint32 eventId) override
    {
        switch (eventId)
        {
            case EVENT_CRYSTALIZE:
            {
                if (!CheckLock(LockCrystalize))
                    break;

                // one non-tank: Crystalize + its pair aura, 5 s; at the end the target casts Crystalline Burst on itself
                // (spell_stone_legion_crystalize). Grashaal casts nothing else meanwhile.
                std::vector<Player*> targets = PickPlayers(me, 1);
                if (targets.empty())
                {
                    Reschedule(eventId);
                    break;
                }

                Player* target = targets.front();
                me->CastSpell(target, SPELL_CRYSTALIZE, true);
                me->CastSpell(target, SPELL_CRYSTALIZE_PAIR, true);
                CastTwin(SPELL_STONE_BREAKERS_COMBO_KAAL);
                _busyUntil = GameTime::GetGameTimeMS() + uint32(CrystalizeDuration.count());
                _crystalTarget = target->GetGUID();
                // Pulverizing Meteor: begincast +5.0 s after the apply, 2 s cast, lands +8.0 s (Warcraft Logs, 93 casts)
                events.RescheduleEvent(EVENT_METEOR, CrystalizeDuration);
                Reschedule(eventId);
                break;
            }
            case EVENT_METEOR:
                if (Player* target = ObjectAccessor::GetPlayer(*me, _crystalTarget))
                    me->CastSpell(target, SPELL_PULVERIZING_METEOR, CastSpellExtraArgs(TimedCast));
                _crystalTarget.Clear();
                break;
            case EVENT_STONE_FIST:
            {
                Unit* victim = me->GetVictim();
                if (!victim || _hovering)
                {
                    events.Repeat(1s);
                    break;
                }
                if (!CheckLock(LockStoneFist))
                    break;

                CastTwin(SPELL_STONE_FIST_KAAL);
                me->CastSpell(victim, SPELL_STONE_FIST, CastSpellExtraArgs(TimedCast));
                Reschedule(eventId);
                break;
            }
            case EVENT_REVERBERATING_ERUPTION:
            {
                if (!CheckLock(LockEruption))
                    break;

                std::vector<Player*> targets = PickPlayers(me, 1);
                if (targets.empty())
                {
                    Reschedule(eventId);
                    break;
                }

                // cast start: the older Stalkers are armed (344682, 10 s); the missile lands 1.5 s after the 5 s cast
                std::list<Creature*> stalkers;
                me->GetCreatureListWithEntryInGrid(stalkers, NPC_ERUPTION_STALKER, 300.0f);
                for (Creature* stalker : stalkers)
                    if (stalker->IsAlive())
                        stalker->CastSpell(stalker, SPELL_ERUPTION_ARM, true);

                CastTwin(SPELL_REVERBERATING_ERUPTION_KAAL);
                if ((urand(1, 100) <= EruptionTalkChance))
                    Talk(SAY_GRASHAAL_ERUPTION);
                me->CastSpell(targets.front(), SPELL_REVERBERATING_ERUPTION, CastSpellExtraArgs(TimedCast));
                Reschedule(eventId);
                break;
            }
            case EVENT_SEISMIC_UPHEAVAL:
            {
                if (!CheckLock(LockSeismicUpheaval))
                    break;

                // marked players run the ground missiles: 4 Normal, 3 LFR, 10 Heroic and Mythic (334338 MaxTargets)
                size_t const count = me->GetMap()->IsLFR() ? 3 : (me->GetMap()->IsHeroicOrHigher() ? 10 : 4);
                std::vector<Player*> targets = PickPlayers(me, count);
                if (targets.empty())
                {
                    Reschedule(eventId);
                    break;
                }

                CastTwin(SPELL_SEISMIC_UPHEAVAL_KAAL);
                me->CastSpell(me, SPELL_SEISMIC_UPHEAVAL_MARK, PreselectedTargets(targets, TRIGGERED_FULL_MASK));
                me->CastSpell(me, SPELL_SEISMIC_UPHEAVAL, PreselectedTargets(targets, TimedCast));
                Reschedule(eventId);
                break;
            }
            default:
                break;
        }
    }

private:
    ObjectGuid _crystalTarget;
};

// ---------------------------------------------------------------------------------------------------------------------------------
// Prince Renathal: holds the Power bar (0 - 1000) and casts Shattering Blast when it is full.
// ---------------------------------------------------------------------------------------------------------------------------------
enum RenathalEvents
{
    EVENT_RENATHAL_ANIMA_HUNGER = 1,
    EVENT_RENATHAL_TALK,
    EVENT_RENATHAL_BLAST
};

// 169835 - Prince Renathal
struct npc_stone_legion_renathal : public ScriptedAI
{
    using ScriptedAI::ScriptedAI;

    void Reset() override
    {
        _events.Reset();
        _active = false;
        _blastQueued = false;
        _intermission = 0;
        me->RemoveAurasDueToSpell(SPELL_ANIMA_HUNGER);
        ResetPower();
    }

    void ResetPower()
    {
        // ENERGIZE_PCT scales off the maximum, so the bar is 1000 wide (Consume Anima +15% = 150, Feast on Anima +40% = 400)
        me->SetPowerType(POWER_MANA);
        me->SetMaxPower(POWER_MANA, RenathalMaxPower);
        me->SetPower(POWER_MANA, 0);
    }

    void SetData(uint32 id, uint32 value) override
    {
        switch (id)
        {
            case DATA_INTERMISSION_START:
                _events.Reset();
                _active = true;
                _blastQueued = false;
                _intermission = uint8(value);
                ResetPower();
                _events.ScheduleEvent(EVENT_RENATHAL_ANIMA_HUNGER, RenathalAnimaHunger[_intermission == 2 ? 1 : 0]);
                break;
            case DATA_BLAST_GO:
                _active = false;
                _events.Reset();
                me->RemoveAurasDueToSpell(SPELL_ANIMA_HUNGER);
                me->SetPower(POWER_MANA, 0);
                break;
            case DATA_FIGHT_RESET:
                Reset();
                break;
            case DATA_FIGHT_END:
                _active = false;
                _events.Reset();
                me->RemoveAurasDueToSpell(SPELL_ANIMA_HUNGER);
                // retail removes the fight versions at the kill; the post-kill rows 172652 and 172653 take over
                me->DespawnOrUnsummon(0ms, 24h);
                break;
            default:
                break;
        }
    }

    StoneLegionGeneralAI* GetStonedGeneral() const
    {
        InstanceScript* instance = me->GetInstanceScript();
        if (!instance)
            return nullptr;

        for (uint32 data : { DATA_GENERAL_KAAL, DATA_GENERAL_GRASHAAL })
            if (Creature* general = instance->GetCreature(data))
                if (general->IsAIEnabled())
                    if (StoneLegionGeneralAI* ai = dynamic_cast<StoneLegionGeneralAI*>(general->AI()))
                        if (ai->IsStoned())
                            return ai;
        return nullptr;
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);
        while (uint32 eventId = _events.ExecuteEvent())
        {
            switch (eventId)
            {
                case EVENT_RENATHAL_ANIMA_HUNGER:
                    DoCastSelf(SPELL_ANIMA_HUNGER, true);
                    _events.ScheduleEvent(EVENT_RENATHAL_TALK, RenathalTalkAfterHunger);
                    break;
                case EVENT_RENATHAL_TALK:
                    Talk(_intermission == 2 ? SAY_RENATHAL_ANIMA_TWO : SAY_RENATHAL_ANIMA_ONE);
                    break;
                case EVENT_RENATHAL_BLAST:
                {
                    // 5.0 s cast on the stoned general; at cast start the Anima Hunger ends and the other general lands
                    StoneLegionGeneralAI* stoned = GetStonedGeneral();
                    if (!stoned)
                    {
                        _blastQueued = false;
                        break;
                    }
                    me->CastSpell(stoned->GetGeneral(), SPELL_SHATTERING_BLAST, CastSpellExtraArgs(TimedCast));
                    me->RemoveAurasDueToSpell(SPELL_ANIMA_HUNGER);
                    stoned->OnBlastStart();
                    break;
                }
                default:
                    break;
            }
        }

        // no hook reports a Power change: the bar is checked every update, the cast starts 0.04 - 0.19 s after it fills (capture)
        if (_active && !_blastQueued && me->GetPower(POWER_MANA) >= me->GetMaxPower(POWER_MANA))
        {
            _blastQueued = true;
            _events.ScheduleEvent(EVENT_RENATHAL_BLAST, Milliseconds(urand(BlastDelayMin, BlastDelayMax)));
        }
    }

private:
    EventMap _events;
    bool _active = false;
    bool _blastQueued = false;
    uint8 _intermission = 0;
};

// ---------------------------------------------------------------------------------------------------------------------------------
// General Draven: Aerial Offensive kills one Commando at a time; the Commandos are his summons.
// ---------------------------------------------------------------------------------------------------------------------------------
enum DravenEvents
{
    EVENT_DRAVEN_SPAWN_COMMANDOS = 1,
    EVENT_DRAVEN_INJURY_OFF,
    EVENT_DRAVEN_TALK,
    EVENT_DRAVEN_FLY,
    EVENT_DRAVEN_AERIAL_OFFENSIVE,
    EVENT_DRAVEN_KILL,
    EVENT_DRAVEN_REFILL,
    EVENT_DRAVEN_RETURN,
    EVENT_DRAVEN_INJURY_ON
};

// 170404 - General Draven
struct npc_stone_legion_draven : public ScriptedAI
{
    npc_stone_legion_draven(Creature* creature) : ScriptedAI(creature), _summons(creature) { }

    void Reset() override
    {
        _events.Reset();
        _active = false;
        _summons.DespawnAll();
        me->RemoveAurasDueToSpell(SPELL_AERIAL_OFFENSIVE);
        if (!me->HasAura(SPELL_DEBILITATING_INJURY))
            DoCastSelf(SPELL_DEBILITATING_INJURY, true);
    }

    void JustSummoned(Creature* summon) override
    {
        _summons.Summon(summon);
    }

    void SummonedCreatureDies(Creature* summon, Unit* /*killer*/) override
    {
        // a replacement flies in 1.1 - 2.5 s after each death
        if (_active && summon->GetEntry() == NPC_COMMANDO)
            _events.ScheduleEvent(EVENT_DRAVEN_REFILL, randtime(CommandoRefillMin, CommandoRefillMax));
    }

    void SetData(uint32 id, uint32 value) override
    {
        uint8 const index = value == 2 ? 1 : 0;
        switch (id)
        {
            case DATA_INTERMISSION_START:
                _events.Reset();
                _active = true;
                _intermission = index;
                _node = 0;
                _events.ScheduleEvent(EVENT_DRAVEN_SPAWN_COMMANDOS, CommandoSpawnDelay[index]);
                _events.ScheduleEvent(EVENT_DRAVEN_INJURY_OFF, DravenInjuryRemoved[index]);
                _events.ScheduleEvent(EVENT_DRAVEN_TALK, DravenTalk[index]);
                _events.ScheduleEvent(EVENT_DRAVEN_FLY, DravenFirstNode[index]);
                _events.ScheduleEvent(EVENT_DRAVEN_AERIAL_OFFENSIVE, DravenAerialOffensive[index]);
                _events.ScheduleEvent(EVENT_DRAVEN_KILL, DravenFirstKill[index]);
                // the second node comes just before the first kill
                _events.ScheduleEvent(EVENT_DRAVEN_FLY, DravenFirstKill[index] - DravenMoveBeforeKill);
                break;
            case DATA_BLAST_GO:
                // the alive Commandos die to Shattering Shards; Draven flies home and is wounded again
                _active = false;
                _events.Reset();
                me->RemoveAurasDueToSpell(SPELL_AERIAL_OFFENSIVE);
                _intermission = index;
                _events.ScheduleEvent(EVENT_DRAVEN_RETURN, DravenReturnAfterBlast);
                _events.ScheduleEvent(EVENT_DRAVEN_INJURY_ON, DravenInjuryAfterBlast[index]);
                break;
            case DATA_FIGHT_RESET:
                Reset();
                me->GetMotionMaster()->Clear();
                me->SetDisableGravity(false);
                me->NearTeleportTo(DravenHome);
                break;
            case DATA_FIGHT_END:
                _active = false;
                _events.Reset();
                _summons.DespawnAll();
                me->DespawnOrUnsummon(0ms, 24h);
                break;
            default:
                break;
        }
    }

    void SpawnCommando()
    {
        uint32 alive = 0;
        for (ObjectGuid const& guid : _summons)
            if (Creature* commando = ObjectAccessor::GetCreature(*me, guid))
                if (commando->IsAlive() && commando->GetEntry() == NPC_COMMANDO)
                    ++alive;
        if (alive >= CommandoCount)
            return;

        // a free point: not within 3 yd of a living Commando
        std::vector<Position const*> free;
        for (Position const& point : CommandoPoints)
        {
            bool taken = false;
            for (ObjectGuid const& guid : _summons)
                if (Creature* commando = ObjectAccessor::GetCreature(*me, guid))
                    if (commando->IsAlive() && commando->GetEntry() == NPC_COMMANDO && commando->GetExactDist2d(point) < 3.0f)
                        taken = true;
            if (!taken)
                free.push_back(&point);
        }
        if (free.empty())
            return;

        me->SummonCreature(NPC_COMMANDO, *Trinity::Containers::SelectRandomContainerElement(free), TEMPSUMMON_CORPSE_TIMED_DESPAWN, CommandoCorpseTime);
    }

    void FlyToNextNode()
    {
        std::span<Position const> nodes = _intermission == 1 ? std::span<Position const>(DravenNodesTwo) : std::span<Position const>(DravenNodesOne);
        Position const& node = nodes[_node % nodes.size()];
        ++_node;
        me->SetCanFly(true);
        me->SetDisableGravity(true);
        float const speed = me->GetExactDist(node) / std::chrono::duration_cast<std::chrono::duration<float>>(DravenNodeTime).count();
        me->GetMotionMaster()->MovePoint(POINT_DRAVEN_NODE, node, false, {}, speed);
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);
        while (uint32 eventId = _events.ExecuteEvent())
        {
            switch (eventId)
            {
                case EVENT_DRAVEN_SPAWN_COMMANDOS:
                    for (uint32 i = 0; i < CommandoCount; ++i)
                        SpawnCommando();
                    break;
                case EVENT_DRAVEN_INJURY_OFF:
                    me->RemoveAurasDueToSpell(SPELL_DEBILITATING_INJURY);
                    break;
                case EVENT_DRAVEN_TALK:
                    Talk(SAY_DRAVEN_SKIES);
                    break;
                case EVENT_DRAVEN_FLY:
                    FlyToNextNode();
                    break;
                case EVENT_DRAVEN_AERIAL_OFFENSIVE:
                    DoCastSelf(SPELL_AERIAL_OFFENSIVE, true);
                    break;
                case EVENT_DRAVEN_KILL:
                {
                    // one Commando at a time: stunned for 5 s and drained 24% of its health per second, so it dies at 5.0 s
                    std::vector<Creature*> candidates;
                    for (ObjectGuid const& guid : _summons)
                        if (Creature* commando = ObjectAccessor::GetCreature(*me, guid))
                            if (commando->IsAlive() && commando->GetEntry() == NPC_COMMANDO && !commando->HasAura(SPELL_AERIAL_OFFENSIVE_STUN))
                                candidates.push_back(commando);
                    if (!candidates.empty())
                        me->CastSpell(*Trinity::Containers::SelectRandomContainerElement(candidates), SPELL_AERIAL_OFFENSIVE_STUN, true);
                    _events.ScheduleEvent(EVENT_DRAVEN_KILL, DravenKillInterval);
                    _events.ScheduleEvent(EVENT_DRAVEN_FLY, DravenKillInterval - DravenMoveBeforeKill);
                    break;
                }
                case EVENT_DRAVEN_REFILL:
                    if (_active)
                        SpawnCommando();
                    break;
                case EVENT_DRAVEN_RETURN:
                    me->GetMotionMaster()->MovePoint(POINT_DRAVEN_HOME, DravenHome, false);
                    break;
                case EVENT_DRAVEN_INJURY_ON:
                    DoCastSelf(SPELL_DEBILITATING_INJURY, true);
                    break;
                default:
                    break;
            }
        }
    }

    void MovementInform(uint32 type, uint32 id) override
    {
        if (type == POINT_MOTION_TYPE && id == POINT_DRAVEN_HOME)
        {
            me->SetDisableGravity(false);
            me->SetCanFly(false);
            me->SetFacingTo(DravenHome.GetOrientation());
        }
    }

private:
    EventMap _events;
    SummonList _summons;
    bool _active = false;
    uint8 _intermission = 0;
    uint32 _node = 0;
};

// ---------------------------------------------------------------------------------------------------------------------------------
// Stone Legion Commando: hovers, drops Cluster Bombardment on a player every 8 s and leaves an Anima Orb when it dies (not when
// Shattering Shards kills it).
// ---------------------------------------------------------------------------------------------------------------------------------
enum CommandoEvents
{
    EVENT_COMMANDO_CLUSTER = 1,
    EVENT_COMMANDO_SUICIDE
};

// 169601 - Stone Legion Commando
struct npc_stone_legion_commando : public ScriptedAI
{
    using ScriptedAI::ScriptedAI;

    void IsSummonedBy(WorldObject* /*summoner*/) override
    {
        me->SetReactState(REACT_PASSIVE);
        me->SetDisableGravity(true);
        me->SetAnimTier(AnimTier::Fly);
        _events.ScheduleEvent(EVENT_COMMANDO_CLUSTER, CommandoClusterFirst);
    }

    void DoAction(int32 action) override
    {
        if (action != ACTION_SHATTERED || _shattered)
            return;

        _shattered = true;
        _events.Reset();
        _events.ScheduleEvent(EVENT_COMMANDO_SUICIDE, randtime(CommandoSuicideMin, CommandoSuicideMax));
    }

    void JustDied(Unit* /*killer*/) override
    {
        if (!_shattered)
            me->CastSpell(me->GetPosition(), SPELL_ANIMA_ORB_CAST, true);
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);
        while (uint32 eventId = _events.ExecuteEvent())
        {
            switch (eventId)
            {
                case EVENT_COMMANDO_CLUSTER:
                {
                    // aimed at a player's position; with several players the pick is not settled (a random one is used)
                    std::vector<Player*> targets = PickPlayers(me, 1);
                    if (!targets.empty())
                        me->CastSpell(targets.front()->GetPosition(), SPELL_CLUSTER_BOMBARDMENT, true);
                    _events.ScheduleEvent(EVENT_COMMANDO_CLUSTER, CommandoClusterRepeat);
                    break;
                }
                case EVENT_COMMANDO_SUICIDE:
                    DoCastSelf(SPELL_SUICIDE, true);
                    break;
                default:
                    break;
            }
        }
    }

private:
    EventMap _events;
    bool _shattered = false;
};

// ---------------------------------------------------------------------------------------------------------------------------------
// Stone Legion Goliath: Ravenous Hunger, Ravenous Feast, the Soultaint Effigy at low health; its death leaves a Massive Anima Orb.
// ---------------------------------------------------------------------------------------------------------------------------------
enum GoliathEvents
{
    EVENT_GOLIATH_HUNGER = 1,
    EVENT_GOLIATH_FEAST,
    EVENT_GOLIATH_SOULTAINT
};

// 172858 - Stone Legion Goliath
struct npc_stone_legion_goliath : public ScriptedAI
{
    using ScriptedAI::ScriptedAI;

    void IsSummonedBy(WorldObject* /*summoner*/) override
    {
        me->SetDisableGravity(true);
        me->SetCanFly(true);
        _events.ScheduleEvent(EVENT_GOLIATH_HUNGER, GoliathHungerFirst);
        _events.ScheduleEvent(EVENT_GOLIATH_FEAST, GoliathFeastFirst);
    }

    uint8 EffigyPct() const
    {
        if (me->GetMap()->IsMythic())
            return EffigyPctMythic;
        return me->GetMap()->IsHeroicOrHigher() ? EffigyPctHeroic : EffigyPctNormal;
    }

    void DamageTaken(Unit* /*attacker*/, uint32& damage, DamageEffectType /*damageType*/, SpellInfo const* /*spellInfo*/) override
    {
        if (_effigy || !me->HealthBelowPctDamaged(EffigyPct(), damage))
            return;

        // Soultaint Effigy stops Ravenous Feast; every second each player gains a stack of Soultaint
        _effigy = true;
        _events.CancelEvent(EVENT_GOLIATH_FEAST);
        _events.ScheduleEvent(EVENT_GOLIATH_SOULTAINT, 0ms);
    }

    void JustDied(Unit* /*killer*/) override
    {
        _events.Reset();
        me->CastSpell(me->GetPosition(), SPELL_MASSIVE_ANIMA_ORB_CAST, true);
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim())
            return;

        _events.Update(diff);
        while (uint32 eventId = _events.ExecuteEvent())
        {
            switch (eventId)
            {
                case EVENT_GOLIATH_HUNGER:
                    DoCastSelf(SPELL_RAVENOUS_HUNGER);
                    _events.ScheduleEvent(EVENT_GOLIATH_HUNGER, GoliathHungerRepeat);
                    break;
                case EVENT_GOLIATH_FEAST:
                {
                    std::vector<Player*> targets = PickPlayers(me, 1);
                    if (!targets.empty())
                        DoCast(targets.front(), SPELL_RAVENOUS_FEAST);
                    _events.ScheduleEvent(EVENT_GOLIATH_FEAST, me->GetMap()->IsMythic() ? GoliathFeastRepeatMythic : GoliathFeastRepeat);
                    break;
                }
                case EVENT_GOLIATH_SOULTAINT:
                {
                    // the Effigy areatrigger (create properties 21931) is not in the database: the debuff goes on directly
                    uint8 const cap = me->GetMap()->IsMythic() ? SoultaintStacksMythic : SoultaintStacks;
                    for (MapReference const& ref : me->GetMap()->GetPlayers())
                    {
                        Player* player = ref.GetSource();
                        if (!player || !player->IsAlive() || player->IsGameMaster())
                            continue;

                        Aura const* aura = player->GetAura(SPELL_SOULTAINT);
                        if (!aura || aura->GetStackAmount() < cap)
                            me->CastSpell(player, SPELL_SOULTAINT, true);
                    }
                    _events.ScheduleEvent(EVENT_GOLIATH_SOULTAINT, 1s);
                    break;
                }
                default:
                    break;
            }
        }
    }

private:
    EventMap _events;
    bool _effigy = false;
};

// ---------------------------------------------------------------------------------------------------------------------------------
// Reverberating Eruption Stalker: when it appears every older Stalker answers with an Echoing Annihilation at its own position.
// ---------------------------------------------------------------------------------------------------------------------------------
// 175102 - Reverberating Eruption Stalker
struct npc_stone_legion_eruption_stalker : public ScriptedAI
{
    using ScriptedAI::ScriptedAI;

    void IsSummonedBy(WorldObject* /*summoner*/) override
    {
        me->SetReactState(REACT_PASSIVE);
        DoCastSelf(SPELL_UNSTABLE_GROUND, true);

        // The new Stalker casts 344718 once per older Stalker (sniff: 1, 2 and 3 casts at the 2nd, 3rd and 4th Eruption): a
        // missile (speed 70) to that Stalker's position, which lands as Echoing Annihilation 0.73 - 1.1 s later; the same
        // landing triggers the raid-wide Echoing Blast (spell_stone_legion_echoing_annihilation).
        std::list<Creature*> stalkers;
        me->GetCreatureListWithEntryInGrid(stalkers, NPC_ERUPTION_STALKER, 300.0f);
        for (Creature* older : stalkers)
            if (older != me && older->IsAlive())
                me->CastSpell(older->GetPosition(), SPELL_ECHO_CAST, true);
    }

    void UpdateAI(uint32 /*diff*/) override { }
};

// ---------------------------------------------------------------------------------------------------------------------------------
// Spell and aura scripts
// ---------------------------------------------------------------------------------------------------------------------------------
// 343135, 343126 - Sinsoul Bulwark: the 4 s PERIODIC_DUMMY. Kaal's copy has no trigger id, so the script drives both casts.
class spell_stone_legion_sinsoul_bulwark : public AuraScript
{
    void HandlePeriodic(AuraEffect const* /*aurEff*/)
    {
        if (Creature* general = GetTarget()->ToCreature())
            if (general->IsAIEnabled())
                if (StoneLegionGeneralAI* ai = dynamic_cast<StoneLegionGeneralAI*>(general->AI()))
                    ai->OnBulwarkTick();
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_stone_legion_sinsoul_bulwark::HandlePeriodic, EFFECT_2, SPELL_AURA_PERIODIC_DUMMY);
    }
};

// 334765 Heart Rend, 334338 Seismic Upheaval (mark), 334498 Seismic Upheaval: the script picked the targets (customArg) and the
// area targets are narrowed to them; the core's random choice would not skip the tanks
class spell_stone_legion_preselected_targets : public SpellScript
{
    void FilterTargets(std::list<WorldObject*>& targets)
    {
        auto const* guids = std::any_cast<std::vector<ObjectGuid>>(&GetSpell()->m_customArg);
        if (!guids)
            return;

        targets.clear();
        for (ObjectGuid const& guid : *guids)
            if (Unit* unit = ObjectAccessor::GetUnit(*GetCaster(), guid))
                targets.push_back(unit);
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_stone_legion_preselected_targets::FilterTargets, EFFECT_ALL, TARGET_UNIT_DEST_AREA_ENEMY);
    }
};

// 334765 - Heart Rend: ending the aura (expiry; a dispel by the spell text) gives the player Heart Hemorrhage, which he casts on himself
class spell_stone_legion_heart_rend : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_HEART_HEMORRHAGE });
    }

    void HandleRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        AuraRemoveMode const mode = GetTargetApplication()->GetRemoveMode();
        if (mode != AURA_REMOVE_BY_EXPIRE && mode != AURA_REMOVE_BY_ENEMY_SPELL)
            return;

        Unit* target = GetTarget();
        if (target->IsAlive())
            target->CastSpell(target, SPELL_HEART_HEMORRHAGE, true);
    }

    void Register() override
    {
        AfterEffectRemove += AuraEffectRemoveFn(spell_stone_legion_heart_rend::HandleRemove, EFFECT_0, SPELL_AURA_PERIODIC_DAMAGE, AURA_EFFECT_HANDLE_REAL);
    }
};

// 339690 - Crystalize: when the 5 s aura runs out the player casts Crystalline Burst on himself (8 yd, stun 4 s, cleanses the bleeds)
class spell_stone_legion_crystalize : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_CRYSTALLINE_BURST });
    }

    void HandleRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        if (GetTargetApplication()->GetRemoveMode() != AURA_REMOVE_BY_EXPIRE)
            return;

        Unit* target = GetTarget();
        if (target->IsAlive())
            target->CastSpell(target, SPELL_CRYSTALLINE_BURST, CastSpellExtraArgs(TRIGGERED_FULL_MASK).SetOriginalCaster(GetCasterGUID()));
    }

    void Register() override
    {
        AfterEffectRemove += AuraEffectRemoveFn(spell_stone_legion_crystalize::HandleRemove, EFFECT_0, SPELL_AURA_PERIODIC_DAMAGE, AURA_EFFECT_HANDLE_REAL);
    }
};

// 339728 - Pulverizing Meteor: the damage is split evenly among the players it hits
class spell_stone_legion_pulverizing_meteor : public SpellScript
{
    void CountTargets(std::list<WorldObject*>& targets)
    {
        _targetCount = std::max<size_t>(targets.size(), 1);
    }

    void SplitDamage(SpellEffIndex /*effIndex*/)
    {
        SetHitDamage(GetHitDamage() / int32(_targetCount));
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_stone_legion_pulverizing_meteor::CountTargets, EFFECT_0, TARGET_UNIT_DEST_AREA_ENEMY);
        OnEffectHitTarget += SpellEffectFn(spell_stone_legion_pulverizing_meteor::SplitDamage, EFFECT_0, SPELL_EFFECT_SCHOOL_DAMAGE);
    }

    size_t _targetCount = 1;
};

// 343086 - Ricocheting Shuriken: the chain only reaches players
class spell_stone_legion_players_only : public SpellScript
{
    void FilterTargets(std::list<WorldObject*>& targets)
    {
        targets.remove_if([](WorldObject const* target) { return !target->IsPlayer(); });
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_stone_legion_players_only::FilterTargets, EFFECT_0, TARGET_UNIT_TARGET_ANY);
    }
};

// 334498 - Seismic Upheaval: each tick (0, 2 and 4 s; the spell has the initial tick) the marked player casts a ground missile at his
// own position; it lands 2 s later as 334500 on the players around that spot
class spell_stone_legion_seismic_upheaval : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_SEISMIC_UPHEAVAL_MISSILE });
    }

    void HandlePeriodic(AuraEffect const* aurEff)
    {
        Unit* target = GetTarget();
        target->CastSpell(target->GetPosition(), SPELL_SEISMIC_UPHEAVAL_MISSILE, CastSpellExtraArgs(aurEff).SetOriginalCaster(GetCasterGUID()));
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_stone_legion_seismic_upheaval::HandlePeriodic, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY);
    }
};

// 344500 - Reverberating Eruption impact: everyone hit gets Reverberating Vulnerability
class spell_stone_legion_reverberating_hit : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_REVERBERATING_VULNERABILITY });
    }

    void ApplyVulnerability(SpellEffIndex /*effIndex*/)
    {
        GetCaster()->CastSpell(GetHitUnit(), SPELL_REVERBERATING_VULNERABILITY, true);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_stone_legion_reverberating_hit::ApplyVulnerability, EFFECT_0, SPELL_EFFECT_SCHOOL_DAMAGE);
    }
};

// 344721 - Echoing Annihilation: everyone hit gets Reverberating Vulnerability, and every landing triggers Echoing Blast, raid-wide,
// from the Stalker that cast it
class spell_stone_legion_echoing_annihilation : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_REVERBERATING_VULNERABILITY, SPELL_ECHOING_BLAST });
    }

    void ApplyVulnerability(SpellEffIndex /*effIndex*/)
    {
        GetCaster()->CastSpell(GetHitUnit(), SPELL_REVERBERATING_VULNERABILITY, true);
    }

    void CastBlast(SpellEffIndex /*effIndex*/)
    {
        Unit* caster = GetCaster();
        caster->CastSpell(caster->GetPosition(), SPELL_ECHOING_BLAST, true);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_stone_legion_echoing_annihilation::ApplyVulnerability, EFFECT_0, SPELL_EFFECT_SCHOOL_DAMAGE);
        OnEffectHit += SpellEffectFn(spell_stone_legion_echoing_annihilation::CastBlast, EFFECT_0, SPELL_EFFECT_SCHOOL_DAMAGE);
    }
};

// 332683 - Shattering Blast. Effect 0 reaches the stoned general; effect 4 (ENERGIZE_PCT -100 on Renathal) would wrap around to a
// full bar in the core (negative percent into an unsigned gain), so the script empties the bar itself.
class spell_stone_legion_shattering_blast : public SpellScript
{
    void HandleGeneral(SpellEffIndex /*effIndex*/)
    {
        if (Creature* general = GetHitCreature())
            if (general->IsAIEnabled())
                if (StoneLegionGeneralAI* ai = dynamic_cast<StoneLegionGeneralAI*>(general->AI()))
                    ai->OnBlastGo();
    }

    void HandleEmptyBar(SpellEffIndex effIndex)
    {
        PreventHitDefaultEffect(effIndex);
        if (Unit* caster = GetCaster())
            caster->SetPower(POWER_MANA, 0);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_stone_legion_shattering_blast::HandleGeneral, EFFECT_0, SPELL_EFFECT_DUMMY);
        OnEffectHitTarget += SpellEffectFn(spell_stone_legion_shattering_blast::HandleEmptyBar, EFFECT_4, SPELL_EFFECT_ENERGIZE_PCT);
    }
};

// 332963 - Shattering Blast: the Commandos in range (conditions) are struck by Shattering Shards, then die by Suicide
class spell_stone_legion_shattering_blast_commandos : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_SHATTERING_SHARDS });
    }

    void HandleCommando(SpellEffIndex /*effIndex*/)
    {
        if (Unit* commando = GetHitUnit())
            commando->CastSpell(commando, SPELL_SHATTERING_SHARDS, true);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_stone_legion_shattering_blast_commandos::HandleCommando, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 332965 - Shattering Shards: marks the Commando so that its death drops no orb, and schedules its Suicide
class spell_stone_legion_shattering_shards : public SpellScript
{
    void HandleCommando(SpellEffIndex /*effIndex*/)
    {
        if (Creature* commando = GetHitCreature())
            if (commando->IsAIEnabled())
                commando->AI()->DoAction(ACTION_SHATTERED);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_stone_legion_shattering_shards::HandleCommando, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 332497 - Anima Hunger: Renathal's 8 yd field. Every 250 ms a player inside who carries Anima Infusion (+150) or Anima Infection
// (+400) casts Consume Anima or Feast on Anima on Renathal. All players are scanned, so a player who picks the buff up while
// standing inside is counted too.
class spell_stone_legion_anima_hunger : public AuraScript
{
    static constexpr float FieldRadius = 8.0f;

    void HandlePeriodic(AuraEffect const* /*aurEff*/)
    {
        Unit* renathal = GetTarget();
        for (MapReference const& ref : renathal->GetMap()->GetPlayers())
        {
            Player* player = ref.GetSource();
            if (!player || !player->IsAlive() || player->IsGameMaster() || !player->IsWithinDist(renathal, FieldRadius))
                continue;

            if (player->HasAura(SPELL_ANIMA_INFECTION))
                player->CastSpell(renathal, SPELL_FEAST_ON_ANIMA, true);
            else if (player->HasAura(SPELL_ANIMA_INFUSION))
                player->CastSpell(renathal, SPELL_CONSUME_ANIMA, true);
        }
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_stone_legion_anima_hunger::HandlePeriodic, EFFECT_1, SPELL_AURA_PERIODIC_DUMMY);
    }
};

// 332393, 339884 (areatrigger create properties 20793 and 21568) - Anima Orb and Massive Anima Orb: a player who enters gains Anima
// Infusion (332406) or Anima Infection (339885) and the orb is gone
struct at_stone_legion_anima_orb : AreaTriggerAI
{
    using AreaTriggerAI::AreaTriggerAI;

    void OnUnitEnter(Unit* unit) override
    {
        if (!unit->IsPlayer() || !unit->IsAlive())
            return;

        uint32 const spellId = at->GetSpellId() == SPELL_MASSIVE_ANIMA_ORB ? SPELL_ANIMA_INFECTION : SPELL_ANIMA_INFUSION;
        unit->CastSpell(unit, spellId, CastSpellExtraArgs(TRIGGERED_FULL_MASK).SetOriginalCaster(at->GetCasterGuid()));
        at->Remove();
    }
};

void AddSC_boss_stone_legion_generals()
{
    RegisterCastleNathriaCreatureAI(boss_general_kaal);
    RegisterCastleNathriaCreatureAI(boss_general_grashaal);
    RegisterCastleNathriaCreatureAI(npc_stone_legion_renathal);
    RegisterCastleNathriaCreatureAI(npc_stone_legion_draven);
    RegisterCastleNathriaCreatureAI(npc_stone_legion_commando);
    RegisterCastleNathriaCreatureAI(npc_stone_legion_goliath);
    RegisterCastleNathriaCreatureAI(npc_stone_legion_eruption_stalker);
    RegisterSpellScript(spell_stone_legion_preselected_targets);
    RegisterSpellScript(spell_stone_legion_pulverizing_meteor);
    RegisterSpellScript(spell_stone_legion_players_only);
    RegisterSpellScript(spell_stone_legion_reverberating_hit);
    RegisterSpellScript(spell_stone_legion_echoing_annihilation);
    RegisterSpellScript(spell_stone_legion_shattering_blast);
    RegisterSpellScript(spell_stone_legion_shattering_blast_commandos);
    RegisterSpellScript(spell_stone_legion_shattering_shards);
    RegisterSpellScript(spell_stone_legion_sinsoul_bulwark);
    RegisterSpellScript(spell_stone_legion_heart_rend);
    RegisterSpellScript(spell_stone_legion_crystalize);
    RegisterSpellScript(spell_stone_legion_seismic_upheaval);
    RegisterSpellScript(spell_stone_legion_anima_hunger);
    RegisterAreaTriggerAI(at_stone_legion_anima_orb);
}
