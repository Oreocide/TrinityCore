/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
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

#include <mutex>
#include <unordered_map>
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include <chrono>
#include <list>
#include <algorithm>
#include "PortCompat.h"
#include "Containers.h"
#include "GameObject.h"
#include "GameObjectAI.h"
#include "Item.h"
#include "Map.h"
#include "MotionMaster.h"
#include "PhasingHandler.h"
#include "CreatureTextMgr.h"
#include "Spell.h"
#include "ScriptMgr.h"
#include "Creature.h"
#include "CreatureAI.h"
#include "Player.h"
#include "ObjectAccessor.h"
#include "Log.h"
#include "ScriptedCreature.h"
#include "SpellAuras.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "SpellScript.h"
#include "TemporarySummon.h"
#include "TaskScheduler.h"
#include "Vehicle.h"

// Kezan (goblin start): quest 14071 "Rolling with my Homies".
// The keys cast spell 66393 which summons the Hot Rod (vehicle 448, summon category vehicle) and boards the player; while the player drives it
// the three homies standing around Kezan (34892 Ace, 34954 Gobber, 34890 Izzy, detected through the quest's invisibility detection auras)
// react to the Hot Rod coming within 10 yd: the player casts the homie's summon spell, the personal summon (34957/34958/34959) gives the
// objective credit and hops into its seat.
namespace
{
enum RollingWithMyHomies
{
    QUEST_ROLLING_WITH_MY_HOMIES            = 14071,

    OBJECTIVE_IZZY_PICKED_UP                = 265516,
    OBJECTIVE_ACE_PICKED_UP                 = 265517,
    OBJECTIVE_GOBBER_PICKED_UP              = 265518,

    NPC_HOT_ROD                             = 34840,
    NPC_KEZAN_CITIZEN_ENTRY                 = 35063,
    NPC_HIRED_LOOTER_ENTRY                  = 35234,

    NPC_IZZY                                = 34890,
    NPC_ACE                                 = 34892,
    NPC_GOBBER                              = 34954,

    NPC_IZZY_SUMMON                         = 34959,
    NPC_ACE_SUMMON                          = 34957,
    NPC_GOBBER_SUMMON                       = 34958,

    SPELL_GENERIC_INVISIBILITY_DETECTION_1  = 49416,
    SPELL_GENERIC_INVISIBILITY_DETECTION_2  = 49417,
    SPELL_GENERIC_INVISIBILITY_DETECTION_3  = 60922,

    SPELL_SUMMON_ACE                        = 66597,
    SPELL_SUMMON_GOBBER                     = 66599,
    SPELL_SUMMON_IZZY                       = 66600,

    SPELL_RESUMMON_ACE                      = 66644,
    SPELL_RESUMMON_GOBBER                   = 66645,
    SPELL_RESUMMON_IZZY                     = 66646,

    SPELL_HOT_ROD_KNOCKBACK                 = 66301,

    ACTION_HOMIES_ENTER_VEHICLE             = 1
};

struct HomieSeat
{
    uint32 SummonEntry;
    int8 Seat;
    uint32 ResummonSpell;
};

constexpr HomieSeat HomieSeats[] =
{
    { NPC_ACE_SUMMON,    1, SPELL_RESUMMON_ACE    },
    { NPC_GOBBER_SUMMON, 2, SPELL_RESUMMON_GOBBER },
    { NPC_IZZY_SUMMON,   3, SPELL_RESUMMON_IZZY   }
};

struct HomieObjective
{
    uint32 StaticEntry;
    uint32 Objective;
    uint32 SummonSpell;
    uint32 DetectionSpell;
};

constexpr HomieObjective HomieObjectives[] =
{
    { NPC_ACE,    OBJECTIVE_ACE_PICKED_UP,    SPELL_SUMMON_ACE,    SPELL_GENERIC_INVISIBILITY_DETECTION_1 },
    { NPC_GOBBER, OBJECTIVE_GOBBER_PICKED_UP, SPELL_SUMMON_GOBBER, SPELL_GENERIC_INVISIBILITY_DETECTION_2 },
    { NPC_IZZY,   OBJECTIVE_IZZY_PICKED_UP,   SPELL_SUMMON_IZZY,   SPELL_GENERIC_INVISIBILITY_DETECTION_3 }
};

// the player's own summon of an entry (the homies are personal summons of the player)
Creature* FindSummonOf(Player* player, uint32 entry)
{
    Creature* creature = player->FindNearestCreature(entry, 300.0f);
    if (creature && creature->IsSummon() && creature->ToTempSummon()->GetSummonerGUID() == player->GetGUID())
        return creature;

    return nullptr;
}

// what the Hot Rod does to the people it runs over: citizens now and then shout at the driver, hired looters get hurt (they are the ones to
// run down for "Robbing Hoods")
void RunOverBystanders(Unit* rod, Player* driver, std::unordered_map<ObjectGuid, std::chrono::steady_clock::time_point>& cooldowns)
{
    auto const now = std::chrono::steady_clock::now();
    std::list<Creature*> bystanders;
    rod->GetCreatureListWithEntryInGrid(bystanders, NPC_KEZAN_CITIZEN_ENTRY, 6.0f);
    rod->GetCreatureListWithEntryInGrid(bystanders, NPC_HIRED_LOOTER_ENTRY, 6.0f);

    for (Creature* bystander : bystanders)
    {
        if (!bystander->IsAlive() || !rod->HasInArc(2.0f * float(M_PI) / 3.0f, bystander))
            continue;

        auto itr = cooldowns.find(bystander->GetGUID());
        if (itr != cooldowns.end() && now < itr->second)
            continue;

        if (bystander->GetEntry() == NPC_HIRED_LOOTER_ENTRY)
        {
            cooldowns[bystander->GetGUID()] = now + 1s;
            Unit::DealDamage(driver, bystander, std::max<uint32>(1, bystander->GetMaxHealth() / 4));
        }
        else if (roll_chance(25))
        {
            cooldowns[bystander->GetGUID()] = now + 10s;
            sCreatureTextMgr->SendChat(bystander, uint8(urand(0, 7)), driver);
        }
    }

    // forget the ones that are long gone
    if (cooldowns.size() > 64)
        for (auto it = cooldowns.begin(); it != cooldowns.end();)
            it = now > it->second ? cooldowns.erase(it) : std::next(it);
}
}

// 14071
class quest_rolling_with_my_homies : public QuestScript
{
public:
    quest_rolling_with_my_homies() : QuestScript("quest_rolling_with_my_homies") { }

    void OnQuestStatusChange(Player* player, Quest const* /*quest*/, QuestStatus /*oldStatus*/, QuestStatus newStatus) override
    {
        if (newStatus == QUEST_STATUS_INCOMPLETE)
        {
            player->CastSpell(player, SPELL_GENERIC_INVISIBILITY_DETECTION_1, true);
            player->CastSpell(player, SPELL_GENERIC_INVISIBILITY_DETECTION_2, true);
            player->CastSpell(player, SPELL_GENERIC_INVISIBILITY_DETECTION_3, true);
        }
        else if (newStatus == QUEST_STATUS_REWARDED)
        {
            // the friends are only along for this quest, they hop out of the Hot Rod and go their own way
            for (HomieSeat const& homie : HomieSeats)
                if (Creature* summon = FindSummonOf(player, homie.SummonEntry))
                    summon->DespawnOrUnsummon();
        }
    }
};

// 34840 Hot Rod: logic lives in a VehicleScript (the creature's ScriptName) because the AI of a vehicle that a player drives is
// replaced by the charm AI, which would never reach the callbacks of a creature AI
class vehicle_hot_rod_rolling_my_homies : public VehicleScript
{
public:
    vehicle_hot_rod_rolling_my_homies() : VehicleScript("vehicle_hot_rod_rolling_my_homies") { }

    void OnAddPassenger(Vehicle* vehicle, Unit* passenger, int8 /*seatId*/) override
    {
        Unit* rod = vehicle->GetBase();
        Player* player = passenger->ToPlayer();
        if (!player || rod->GetEntry() != NPC_HOT_ROD || !rod->IsSummon())
            return;

        // characters that accepted the quest before the quest script ran lack the detection auras; grant them here too
        if (player->GetQuestStatus(QUEST_ROLLING_WITH_MY_HOMIES) == QUEST_STATUS_INCOMPLETE)
            for (uint32 detection : { SPELL_GENERIC_INVISIBILITY_DETECTION_1, SPELL_GENERIC_INVISIBILITY_DETECTION_2, SPELL_GENERIC_INVISIBILITY_DETECTION_3 })
                if (!player->HasAura(detection))
                    player->CastSpell(player, detection, true);

        ObjectGuid playerGuid = player->GetGUID();
        TaskScheduler& scheduler = rod->GetScheduler();
        scheduler.CancelAll();

        // the Hot Rod bowls over bystanders while it drives
        auto bumpCooldowns = std::make_shared<std::unordered_map<ObjectGuid, std::chrono::steady_clock::time_point>>();
        scheduler.Schedule(250ms, [rod, playerGuid, bumpCooldowns](TaskContext context)
        {
            if (Player* driver = ObjectAccessor::GetPlayer(*rod, playerGuid))
            {
                if (rod->m_movementInfo.HasMovementFlag(MOVEMENTFLAG_FORWARD))
                {
                    driver->CastSpell(nullptr, SPELL_HOT_ROD_KNOCKBACK, true);
                    RunOverBystanders(rod, driver, *bumpCooldowns);
                }
            }

            context.Repeat();
        });

        // pick up the friends: whoever stands within 10 yd of the Hot Rod (checked here, not through the line-of-sight callback of the
        // friends, which is not reliably raised for a vehicle that a player drives)
        scheduler.Schedule(500ms, [rod, playerGuid](TaskContext context)
        {
            if (Player* driver = ObjectAccessor::GetPlayer(*rod, playerGuid))
            {
                if (driver->GetQuestStatus(QUEST_ROLLING_WITH_MY_HOMIES) == QUEST_STATUS_INCOMPLETE)
                {
                    for (HomieObjective const& homie : HomieObjectives)
                    {
                        int32 progress = driver->GetQuestObjectiveData(QUEST_ROLLING_WITH_MY_HOMIES, homie.Objective);
                        Creature* near = rod->FindNearestCreature(homie.StaticEntry, 12.0f);
                        if (progress)
                            continue;

                        if (near && rod->GetDistance(near) <= 10.0f)
                        {
                            driver->RemoveAurasDueToSpell(homie.DetectionSpell);
                            driver->CastSpell(nullptr, homie.SummonSpell, true);
                        }
                    }
                }
            }

            context.Repeat();
        });

        // homies already picked up take their seats again
        for (HomieSeat const& homie : HomieSeats)
            if (Creature* summon = FindSummonOf(player, homie.SummonEntry))
                summon->AI()->DoAction(ACTION_HOMIES_ENTER_VEHICLE);
    }

    void OnRemovePassenger(Vehicle* vehicle, Unit* passenger) override
    {
        if (passenger->IsPlayer() && vehicle->GetBase()->GetEntry() == NPC_HOT_ROD)
            vehicle->GetBase()->GetScheduler().CancelAll();
    }
};

// 66298 Honk Horn
class spell_klaxon : public SpellScript
{
    PrepareSpellScript(spell_klaxon);

    void HandleOnHit()
    {
        if (Unit* owner = GetCaster()->GetOwner())
            if (Player* player = owner->ToPlayer())
                GetCaster()->PlayDirectSound(22491, player);
    }

    void Register() override
    {
        OnHit += SpellHitFn(spell_klaxon::HandleOnHit);
    }
};

// 66299 Radio
class spell_radio : public SpellScript
{
    PrepareSpellScript(spell_radio);

    void HandleOnHit()
    {
        if (Unit* owner = GetCaster()->GetOwner())
            if (Player* player = owner->ToPlayer())
                GetCaster()->PlayDistanceSound(23406, player);
    }

    void Register() override
    {
        OnHit += SpellHitFn(spell_radio::HandleOnHit);
    }
};

// 34892 Ace, 34954 Gobber, 34890 Izzy (the ones standing around Kezan): kept as a second path, the Hot Rod scan above does the work
struct npc_homies : public ScriptedAI
{
    npc_homies(Creature* creature) : ScriptedAI(creature) { }
};

// 34957 Ace, 34958 Gobber, 34959 Izzy (the personal summons that ride along)
struct npc_homies_summon : public ScriptedAI
{
    npc_homies_summon(Creature* creature) : ScriptedAI(creature) { }

    void IsSummonedBy(WorldObject* summoner) override
    {
        if (Player* player = summoner->ToPlayer())
            player->KilledMonsterCredit(me->GetEntry());

        // give the summon a moment to be added to the world before it boards
        me->GetScheduler().Schedule(100ms, [this](TaskContext /*context*/) { DoAction(ACTION_HOMIES_ENTER_VEHICLE); });
    }

    void DoAction(int32 /*action*/) override
    {
        if (!me->IsSummon())
            return;

        Unit* owner = me->ToTempSummon()->GetSummonerUnit();
        Vehicle* vehicle = owner ? owner->GetVehicle() : nullptr;
        if (!vehicle)
            return;

        for (HomieSeat const& homie : HomieSeats)
        {
            if (homie.SummonEntry != me->GetEntry())
                continue;

            // the ride spell's own target selection drops the player-driven Hot Rod, so the control vehicle aura is attached directly
            // (same fallback as Unit::HandleSpellClick uses)
            SpellInfo const* rideSpell = sSpellMgr->AssertSpellInfo(VEHICLE_SPELL_RIDE_HARDCODED, me->GetMap()->GetDifficultyID());
            std::array<SpellEffectValue, MAX_SPELL_EFFECTS> basePoints = { };
            uint8 effectIndex = 0;
            for (SpellEffectInfo const& effect : rideSpell->GetEffects())
            {
                basePoints[effect.EffectIndex] = effect.BasePoints;
                if (effect.ApplyAuraName == SPELL_AURA_CONTROL_VEHICLE)
                    effectIndex = effect.EffectIndex;
            }
            basePoints[effectIndex] = homie.Seat + 1;

            Unit* rod = vehicle->GetBase();
            AuraCreateInfo createInfo(ObjectGuid::Create<HighGuid::Cast>(SPELL_CAST_SOURCE_NORMAL, me->GetMapId(), rideSpell->Id, me->GetMap()->GenerateLowGuid<HighGuid::Cast>()), rideSpell, me->GetMap()->GetDifficultyID(), MAX_EFFECT_MASK, rod);
            createInfo.SetCaster(me).SetBaseAmount(basePoints.data());
            Aura::TryRefreshStackOrCreate(createInfo);
        }
    }
};

// ---- Kezan chapter 1 quests and events, ported from the BfA-era reference core (vSLDB Maelstrom/LostIsles/kezan.cpp) ----
namespace Chapter1
{
constexpr uint32 SPELL_GENERIC_INVISIBILITY_DETECTION_4 = 90161;

enum NPC_DeffiantTroll
{
    SPELL_RAGE                      = 45111,
    SPELL_SLEEP                     = 62248,
    DEFFIANT_KILL_CREDIT            = 34830,
    SPELL_GOBLIN_BELT_SHOCKER       = 66306,
    QUEST_GOOD_HELP_IS_HARD_TO_FIND = 14069,
    GO_DEPOSIT                      = 195489,
    GO_DEPOSIT_2                    = 195488,
    GO_DEPOSIT_CHUNK                = 195492,
};

// 34830
struct npc_defiant_troll : public ScriptedAI
{
    npc_defiant_troll(Creature* creature) : ScriptedAI(creature) {}

    void Reset() override
    {
        rebuffTimer = 5000;
    }

    void MovementInform(uint32 /*type*/, uint32 /*id*/) override
    {
        if (GameObject* Deposit = me->FindNearestGameObject(GO_DEPOSIT, 20.f))
            me->SetFacingToObject(Deposit);
        me->HandleEmoteCommand(EMOTE_STATE_WORK_MINING);
        me->LoadEquipment(3, true);
    }

    void JustReachedHome() override { }

    void SpellHit(WorldObject* caster, SpellInfo const* spell) override
    {
        if (!me->HasAura(SPELL_RAGE))
            return;

        if (spell->Id == SPELL_GOBLIN_BELT_SHOCKER && caster->IsPlayer()
            && caster->ToPlayer()->GetQuestStatus(QUEST_GOOD_HELP_IS_HARD_TO_FIND) == QUEST_STATUS_INCOMPLETE)
        {
            caster->ToPlayer()->KilledMonsterCredit(DEFFIANT_KILL_CREDIT, me->GetGUID());
            me->HandleEmoteCommand(EMOTE_ONESHOT_NONE);
            me->RemoveAllAuras();

            Talk(urand(0, 6), caster);

            if (GameObject* Deposit = me->FindNearestGameObject(GO_DEPOSIT, 20.f))
                me->GetMotionMaster()->MovePoint(1, Deposit->GetPositionX() - 2.f, Deposit->GetPositionY(), Deposit->GetPositionZ());
        }
    }



    void UpdateAI(uint32 diff) override
    {
        if (me->HasAura(SPELL_RAGE))
            return;

        if (rebuffTimer <= diff)
        {
            switch (irand(0, 2))
            {
                case 0:
                    me->HandleEmoteCommand(EMOTE_STATE_EXCLAIM);
                    break;
                case 1:
                    me->HandleEmoteCommand(EMOTE_STATE_DANCE);
                    break;
                case 2:
                    me->AddAura(SPELL_SLEEP, me);
                    break;
            }

            me->LoadEquipment(urand(1, 2), true);
            rebuffTimer = 60000; //Rebuff again in 1 minutes
            me->NearTeleportTo(me->GetHomePosition());
            me->AddAura(SPELL_RAGE, me);
        }
        else
            rebuffTimer -= diff;
    }

private:
    uint32 rebuffTimer;
};


enum NPC_BlingBling
{
    SPELL_OUTFIT            = 66781,
    SPELL_COOL_SHADE        = 66782,
    SPELL_SHINY_BLING       = 66780,
    QUEST_BLINGBLING_MALE   = 14109,
    QUEST_BLINGBLING_FEMALE = 14110,
    NPC_SCABO               = 35128,
    NPC_MISSA               = 35130,
    NPC_DIASTEME            = 35126,
};


// 35128, 35130, 35126: the outfit vendors of the Bling Bling quest hand out their part of the outfit when the gossip option is chosen
struct npc_bling_bling : public ScriptedAI
{
    npc_bling_bling(Creature* creature) : ScriptedAI(creature) { }

    bool OnGossipSelect(Player* player, uint32 /*menuId*/, uint32 /*gossipListId*/) override
    {
        if (player->GetQuestStatus(QUEST_BLINGBLING_MALE) == QUEST_STATUS_INCOMPLETE ||
            player->GetQuestStatus(QUEST_BLINGBLING_FEMALE) == QUEST_STATUS_INCOMPLETE)
        {
            switch (me->GetEntry())
            {
                case NPC_SCABO:
                    player->CastSpell(player, SPELL_OUTFIT, true);
                    break;
                case NPC_MISSA:
                    player->CastSpell(player, SPELL_COOL_SHADE, true);
                    break;
                case NPC_DIASTEME:
                    player->CastSpell(player, SPELL_SHINY_BLING, true);
                    break;
                default:
                    break;
            }
        }

        return false;
    }
};


enum KezanPartygoer: uint32
{
    QUEST_LIFE_OF_THE_PARTY_MALE    = 14113,
    QUEST_LIFE_OF_THE_PARTY_FEMALE  = 14153,

    NPC_PARTYGOER_KILLCREDIT        = 35175,

    SPELL_AWESOME_PARTY_ENSEMBLE    = 66908,
    SPELL_LOTP_OUTFIT_FEMALE        = 66927,
    SPELL_LOTP_OUTFIT_MALE          = 66928,

    SPELL_HAPPY_PARTYGOER           = 66916,
    SPELL_COSMETIC_STUN             = 46957,
    SPELL_COSMETIC_DRUNKEN          = 55664,
    SPELL_FIREWORKS_BLUE            = 66917,
    SPELL_FIREWORKS_GREEN           = 66918,
    SPELL_FIREWORKS_RED             = 66919,
    SPELL_SUMMON_DISCO_BALL         = 66930,
    SPELL_SUMMON_BUCKET             = 66931,

    SPELL_DRINK                     = 66909,
    SPELL_BUCKET                    = 66910,
    SPELL_DANCE                     = 66911,
    SPELL_FIREWORKS                 = 66912,
    SPELL_FOOD                      = 66913,

    SPELL_NEED_DRINK                = 75042,
    SPELL_NEED_BUCKET               = 75044,
    SPELL_NEED_DANCE                = 75046,
    SPELL_NEED_FIREWORKS            = 75048,
    SPELL_NEED_FOOD                 = 75050,

    TALK_NEED_DANCE                 = 0,
    TALK_NEED_BUCKET                = 1,
    TALK_NEED_DRINK                 = 2,
    TALK_NEED_FIREWORKS             = 3,
    TALK_NEED_FOOD                  = 4,

    TALK_DRINK                      = 5,
    TALK_BUCKET                     = 6,
    TALK_DANCE                      = 7,
    TALK_FIREWORKS                  = 8,
    TALK_FOOD                       = 9,
};

// 35175
// 35186
struct npc_kezan_partygoer : public ScriptedAI
{
    npc_kezan_partygoer(Creature* creature) : ScriptedAI(creature) { }

    std::vector<std::tuple<uint32, uint32, uint32, uint32>> const talkAndSpells =
    {
        { SPELL_DRINK,      SPELL_NEED_DRINK,       TALK_NEED_DRINK,        TALK_DRINK      },
        { SPELL_BUCKET,     SPELL_NEED_BUCKET,      TALK_NEED_BUCKET,       TALK_BUCKET     },
        { SPELL_DANCE,      SPELL_NEED_DANCE,       TALK_NEED_DANCE,        TALK_DANCE      },
        { SPELL_FIREWORKS,  SPELL_NEED_FIREWORKS,   TALK_NEED_FIREWORKS,    TALK_FIREWORKS  },
        { SPELL_FOOD,       SPELL_NEED_FOOD,        TALK_NEED_FOOD,         TALK_FOOD       },
    };

    void Reset() override
    {
        ApplyNeedAura();
    }

    void ApplyNeedAura()
    {
        _selectedNeed = urand(0, 4);
        uint32 needAura = std::get<1>(talkAndSpells[_selectedNeed]);
        me->AddAura(needAura, me);
        me->RemoveAura(SPELL_HAPPY_PARTYGOER);

        // Specific cosmetics
        switch (needAura)
        {
            case SPELL_NEED_BUCKET:
                me->AddAura(SPELL_COSMETIC_STUN, me);
                me->AddAura(SPELL_COSMETIC_DRUNKEN, me);
                break;
            default:
                break;
        }

        me->GetScheduler().Schedule(10s, 120s, [this](TaskContext context)
        {
            Talk(std::get<2>(talkAndSpells[_selectedNeed]));
            context.Repeat(10s, 120s);
        });
    }

    void SpellHit(WorldObject* caster, SpellInfo const* spell) override
    {
        if (me->HasAura(SPELL_HAPPY_PARTYGOER))
            return;

        if (spell->Id != std::get<0>(talkAndSpells[_selectedNeed]))
            return;

        Player* player = caster->ToPlayer();
        if (!player)
            return;

        if ((player->GetQuestStatus(QUEST_LIFE_OF_THE_PARTY_MALE)   != QUEST_STATUS_INCOMPLETE &&
            player->GetQuestStatus(QUEST_LIFE_OF_THE_PARTY_FEMALE) != QUEST_STATUS_INCOMPLETE))
            return;

        me->RemoveAllAuras();
        Talk(std::get<3>(talkAndSpells[_selectedNeed]), caster);
        me->AddAura(SPELL_HAPPY_PARTYGOER, me);
        player->KilledMonsterCredit(NPC_PARTYGOER_KILLCREDIT);

        // Specific cosmetics
        switch (spell->Id)
        {
            case SPELL_BUCKET:
                me->CastSpell(nullptr, SPELL_SUMMON_BUCKET, true);
                break;
            case SPELL_DANCE:
                me->CastSpell(nullptr, SPELL_SUMMON_DISCO_BALL, true);
                break;
            default:
                break;
        }

        me->GetScheduler().CancelAll();
        me->GetScheduler().Schedule(30s, [this](TaskContext /*context*/)
        {
            ApplyNeedAura();
        });
    }

private:
    int8 _selectedNeed;
};

// 66912
class spell_kezan_fireworks : public SpellScript
{
    PrepareSpellScript(spell_kezan_fireworks);

    void HandleFireworks(SpellEffIndex /*effIndex*/)
    {
        GetCaster()->CastSpell(nullptr, Trinity::Containers::SelectRandomContainerElement(std::array<uint32, 3>{ SPELL_FIREWORKS_BLUE, SPELL_FIREWORKS_GREEN, SPELL_FIREWORKS_RED }), true);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_kezan_fireworks::HandleFireworks, EFFECT_1, SPELL_EFFECT_SCRIPT_EFFECT);
    }
};

enum GreatBankHeist
{
    NPC_FBOK_VAULT              = 35486,
    QUEST_THE_GREAT_BANK_HEIST  = 14122,

    SPELL_CONTROL_VEHICLE       = 67476,
    SPELL_BLASTCRACKERS         = 67508,
    SPELL_KAJAMITE_DRILL        = 67522,
    SPELL_EAR_O_SCOPE           = 67524,
    SPELL_INFINIFOLD_LOCKPICK   = 67525,
    SPELL_AMAZIN_G_RAY          = 67526,

    SPELL_POWER_CORRECT         = 67493,
    SPELL_POWER_INCORRECT       = 67494,

    SPELL_TIMER                 = 67502,

    SPELL_SUCCESS_CREATE_ITEM   = 67492,

    TALK_TRIPLE_DOT             = 0,
    TALK_INTRO_1                = 1,
    TALK_INTRO_2                = 2,
    TALK_INTRO_3                = 3,
    TALK_INTRO_4                = 4,
    TALK_CORRECT                = 5,
    TALK_TIMED_OUT              = 6,
    TALK_INCORRECT              = 7,
    TALK_USE_BLASTCRACKERS      = 8,
    TALK_USE_EAR_O_SCOPE        = 9,
    TALK_USE_INFINIFOLD_LOCKPICK= 10,
    TALK_USE_KAJAMITE_DRILL     = 11,
    TALK_USE_AMAZIN_G_RAY       = 12,
    TALK_SUCCESS                = 13,
};

// 67555: the vault game object (type 22) casts this on the player that clicked it; the player gets into a personal vault vehicle
class spell_great_bank_heist_vault_interact : public SpellScript
{
    PrepareSpellScript(spell_great_bank_heist_vault_interact);

    void HandleHit(SpellEffIndex /*effIndex*/)
    {
        Player* player = GetHitUnit() ? GetHitUnit()->ToPlayer() : nullptr;
        if (!player || player->GetVehicle())
            return;

        if (Creature* vault = player->SummonCreature(NPC_FBOK_VAULT, player->GetPosition(), TEMPSUMMON_MANUAL_DESPAWN, 0s, 0, 0, player->GetGUID()))
            player->CastSpell(vault, SPELL_CONTROL_VEHICLE, true);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_great_bank_heist_vault_interact::HandleHit, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT);
    }
};

// 195449 First Bank of Kezan Vault (game object type 22): the click is handled here instead of through the object's own spell cast, which
// the client/server refuse with "You can't do that yet" for a player that is, for example, still on a vehicle or mounted
struct gob_first_bank_of_kezan_vault : public GameObjectAI
{
    gob_first_bank_of_kezan_vault(GameObject* go) : GameObjectAI(go) { }

    bool OnGossipHello(Player* player) override
    {
        if (player->GetVehicle() || player->GetQuestStatus(QUEST_THE_GREAT_BANK_HEIST) != QUEST_STATUS_INCOMPLETE)
            return true;

        if (Creature* vault = player->SummonCreature(NPC_FBOK_VAULT, player->GetPosition(), TEMPSUMMON_MANUAL_DESPAWN, 0s, 0, 0, player->GetGUID()))
            player->CastSpell(vault, SPELL_CONTROL_VEHICLE, true);

        return true;
    }
};

// the vault game: the player sits in a controlled seat, so the AI of the vault is the charm AI and never sees SpellHit or PassengerBoarded.
// The sequence therefore runs from a VehicleScript on the vault's scheduler and the five tool spells report to it through a spell script
struct VaultState
{
    ObjectGuid Player;
    uint8 NextAction = 0;
    float Progress = 0.0f;  // vault breaking bar, 0-100 (kept here because the power of a vehicle is not a reliable counter)
};

std::mutex VaultStatesLock;
std::unordered_map<ObjectGuid, VaultState> VaultStates;

constexpr std::pair<uint32, uint32> VaultTools[] =
{
    { SPELL_BLASTCRACKERS,       TALK_USE_BLASTCRACKERS       },
    { SPELL_KAJAMITE_DRILL,      TALK_USE_KAJAMITE_DRILL      },
    { SPELL_EAR_O_SCOPE,         TALK_USE_EAR_O_SCOPE         },
    { SPELL_INFINIFOLD_LOCKPICK, TALK_USE_INFINIFOLD_LOCKPICK },
    { SPELL_AMAZIN_G_RAY,        TALK_USE_AMAZIN_G_RAY        },
};

void VaultTalk(Creature* vault, Player* player, uint8 id)
{
    if (!player)
        return;

    sCreatureTextMgr->SendChat(vault, id, player);
    sCreatureTextMgr->SendChat(vault, TALK_TRIPLE_DOT, player);
}

void VaultSelectNextAction(Creature* vault)
{
    vault->GetScheduler().Schedule(1s, 1u, [vault](TaskContext /*context*/)
    {
        Player* player = nullptr;
        {
            std::lock_guard<std::mutex> lock(VaultStatesLock);
            auto itr = VaultStates.find(vault->GetGUID());
            if (itr == VaultStates.end())
                return;

            itr->second.NextAction = urand(0, 4);
            player = ObjectAccessor::GetPlayer(*vault, itr->second.Player);
            if (player)
                VaultTalk(vault, player, VaultTools[itr->second.NextAction].second);
        }

        if (!player)
            return;

        vault->CastSpell(player, SPELL_TIMER, true);
        vault->GetScheduler().Schedule(5s, 1u, [vault](TaskContext /*context*/)
        {
            Player* timedOutPlayer = nullptr;
            {
                std::lock_guard<std::mutex> lock(VaultStatesLock);
                auto itr = VaultStates.find(vault->GetGUID());
                if (itr == VaultStates.end())
                    return;

                timedOutPlayer = ObjectAccessor::GetPlayer(*vault, itr->second.Player);
            }

            VaultTalk(vault, timedOutPlayer, TALK_TIMED_OUT);
            VaultSelectNextAction(vault);
        });
    });
}

// 35486
class vehicle_first_bank_of_kezan_vault : public VehicleScript
{
public:
    vehicle_first_bank_of_kezan_vault() : VehicleScript("vehicle_first_bank_of_kezan_vault") { }

    void OnAddPassenger(Vehicle* vehicle, Unit* passenger, int8 /*seatId*/) override
    {
        Creature* vault = vehicle->GetBase()->ToCreature();
        Player* player = passenger->ToPlayer();
        if (!vault || !player || vault->GetEntry() != NPC_FBOK_VAULT)
            return;

        {
            std::lock_guard<std::mutex> lock(VaultStatesLock);
            VaultStates[vault->GetGUID()] = VaultState{ player->GetGUID(), 0 };
        }

        // the bar must only move when the player answers: no natural power regeneration
        vault->RemoveUnitFlag2(UNIT_FLAG2_REGENERATE_POWER);
        vault->SetMaxPower(POWER_MANA, 100);
        vault->SetPower(POWER_MANA, 0);
        vault->SetControlled(true, UNIT_STATE_ROOT);

        ObjectGuid playerGuid = player->GetGUID();
        vault->GetScheduler().CancelAll();

        // the bar shows the script's own progress: natural regeneration (a third of the maximum per second for a controlled creature) and the
        // energize of the correct/incorrect spells are overwritten every tick
        vault->GetScheduler().Schedule(100ms, [vault](TaskContext context)
        {
            float progress = 0.0f;
            {
                std::lock_guard<std::mutex> lock(VaultStatesLock);
                auto itr = VaultStates.find(vault->GetGUID());
                if (itr == VaultStates.end())
                    return;

                progress = itr->second.Progress;
            }

            vault->RemoveUnitFlag2(UNIT_FLAG2_REGENERATE_POWER);
            if (vault->GetPower(POWER_MANA) != int32(progress))
                vault->SetPower(POWER_MANA, int32(progress));

            context.Repeat();
        });

        vault->GetScheduler().Schedule(1s, 1u, [vault, playerGuid](TaskContext /*context*/) { VaultTalk(vault, ObjectAccessor::GetPlayer(*vault, playerGuid), TALK_INTRO_1); })
            .Schedule(6s, 1u, [vault, playerGuid](TaskContext /*context*/) { VaultTalk(vault, ObjectAccessor::GetPlayer(*vault, playerGuid), TALK_INTRO_2); })
            .Schedule(13s, 1u, [vault, playerGuid](TaskContext /*context*/) { VaultTalk(vault, ObjectAccessor::GetPlayer(*vault, playerGuid), TALK_INTRO_3); })
            .Schedule(20s, 1u, [vault, playerGuid](TaskContext /*context*/) { VaultTalk(vault, ObjectAccessor::GetPlayer(*vault, playerGuid), TALK_INTRO_4); })
            .Schedule(23s, 1u, [vault](TaskContext /*context*/) { VaultSelectNextAction(vault); });
    }

    void OnRemovePassenger(Vehicle* vehicle, Unit* passenger) override
    {
        Creature* vault = vehicle->GetBase()->ToCreature();
        if (!vault || !passenger->IsPlayer() || vault->GetEntry() != NPC_FBOK_VAULT)
            return;

        {
            std::lock_guard<std::mutex> lock(VaultStatesLock);
            VaultStates.erase(vault->GetGUID());
        }

        vault->GetScheduler().CancelAll();
        vault->DespawnOrUnsummon(2s);
    }
};

// 67508, 67522, 67524, 67525, 67526: the five vault tools (cast by the vault vehicle)
class spell_great_bank_heist_vault_tool : public SpellScript
{
    PrepareSpellScript(spell_great_bank_heist_vault_tool);

    void HandleAfterCast()
    {
        // the tools are on the vault's action bar but are cast by the player sitting in it (target: the vehicle), so look at both
        Creature* vault = GetCaster()->ToCreature();
        if (!vault)
            if (Unit* vehicleBase = GetCaster()->GetVehicleBase())
                vault = vehicleBase->ToCreature();

        if (!vault || vault->GetEntry() != NPC_FBOK_VAULT)
            return;

        Player* player = nullptr;
        uint8 nextAction = 0;
        {
            std::lock_guard<std::mutex> lock(VaultStatesLock);
            auto itr = VaultStates.find(vault->GetGUID());
            if (itr == VaultStates.end())
                return;

            player = ObjectAccessor::GetPlayer(*vault, itr->second.Player);
            nextAction = itr->second.NextAction;
        }

        if (!player || !player->HasAura(SPELL_TIMER))
            return;

        vault->GetScheduler().CancelGroup(1);
        player->RemoveAurasDueToSpell(SPELL_TIMER);

        constexpr float StepPct = 6.5f;
        bool correct = VaultTools[nextAction].first == GetSpellInfo()->Id;
        float progress = 0.0f;
        {
            std::lock_guard<std::mutex> lock(VaultStatesLock);
            auto itr = VaultStates.find(vault->GetGUID());
            if (itr == VaultStates.end())
                return;

            itr->second.Progress = std::clamp(itr->second.Progress + (correct ? StepPct : -StepPct), 0.0f, 100.0f);
            progress = itr->second.Progress;
        }

        TC_LOG_ERROR("scripts.kezan", "Vault tool {} expected {} -> {} progress {:.1f} (vault power {}/{})", GetSpellInfo()->Id, VaultTools[nextAction].first,
            correct ? "correct" : "incorrect", progress, vault->GetPower(POWER_MANA), vault->GetMaxPower(POWER_MANA));

        vault->CastSpell(vault, correct ? SPELL_POWER_CORRECT : SPELL_POWER_INCORRECT, true);
        vault->SetMaxPower(POWER_MANA, 100);
        vault->SetPower(POWER_MANA, int32(progress));

        if (!correct)
        {
            VaultTalk(vault, player, TALK_INCORRECT);
            VaultSelectNextAction(vault);
            return;
        }

        VaultTalk(vault, player, TALK_CORRECT);
        if (progress >= 100.0f)
        {
            VaultTalk(vault, player, TALK_SUCCESS);
            player->KilledMonsterCredit(vault->GetEntry());
            vault->CastSpell(player, SPELL_SUCCESS_CREATE_ITEM, true);
            Unit::Kill(vault, vault);
        }
        else
            VaultSelectNextAction(vault);
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_great_bank_heist_vault_tool::HandleAfterCast);
    }
};

enum
{
    NPC_MOOK_DISGUISE   = 48925
};

// 67435
class aura_hobart_ingenious_cap_of_mook_foolery : public AuraScript
{
    PrepareAuraScript(aura_hobart_ingenious_cap_of_mook_foolery);

    void OnApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        Unit* target = GetTarget();
        // The Hot Rod is a controlled vehicle. Drop it (and any mount) before the mook disguise boards the player.
        if (target->GetVehicle())
            target->ExitVehicle();
        target->Dismount();
        target->RemoveAurasByType(SPELL_AURA_MOUNTED);

        if (Creature* mookDisguise = target->SummonCreature(NPC_MOOK_DISGUISE, *target, TEMPSUMMON_MANUAL_DESPAWN))
            mookDisguise->EnterVehicle(target);
    }

    void OnRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        if (Creature* mookDisguise = PortCompat::GetSummonedCreatureByEntry(GetTarget(), NPC_MOOK_DISGUISE))
            mookDisguise->DespawnOrUnsummon();
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(aura_hobart_ingenious_cap_of_mook_foolery::OnApply, EFFECT_1, SPELL_AURA_SET_VEHICLE_ID, AURA_EFFECT_HANDLE_REAL);
        AfterEffectRemove += AuraEffectRemoveFn(aura_hobart_ingenious_cap_of_mook_foolery::OnRemove, EFFECT_1, SPELL_AURA_SET_VEHICLE_ID, AURA_EFFECT_HANDLE_REAL);
    }
};

// 67682
class spell_kabummbomb : public SpellScript
{
    PrepareSpellScript(spell_kabummbomb);

    void HandleActivateGameobject(SpellEffIndex /*effIndex*/)
    {
        if (Unit* caster = GetCastItem()->GetOwner())
        {
            if (caster->ToPlayer()->GetQuestStatus(14124) != QUEST_STATUS_INCOMPLETE)
                return;

            if (GameObject* Deposit = caster->FindNearestGameObject(GO_DEPOSIT_2, 20))
            {
                // The old offsets were a straight line one yard above the deposit. Scatter on the ground instead.
                for (uint8 i = 0; i < 3; ++i)
                {
                    Position pos = Deposit->GetRandomNearPosition(1.6f);
                    pos.SetOrientation(frand(0.f, 2.f * float(M_PI)));
                    caster->SummonGameObject(GO_DEPOSIT_CHUNK, pos, QuaternionData(), 0s);
                }
                Deposit->Delete();
            }
        }
    }

    void Register() override
    {
        OnEffectHit += SpellEffectFn(spell_kabummbomb::HandleActivateGameobject,EFFECT_0,SPELL_EFFECT_ACTIVATE_OBJECT);
    }
};

enum
{
    NPC_KEZAN_CITIZEN   = 35063,
    NPC_HIRED_LOOTER    = 35234,
};

struct npc_citoyen_gob : public ScriptedAI
{
    npc_citoyen_gob(Creature* creature) : ScriptedAI(creature) {}

    void SpellHit(WorldObject* caster, SpellInfo const* spell) override
    {
        if (spell->Id != SPELL_HOT_ROD_KNOCKBACK)
            return;

        if (!caster->IsPlayer())
            return;

        me->GetScheduler().CancelAll();

        if (me->GetEntry() == NPC_HIRED_LOOTER)
        {
            me->KillSelf();
            if (caster->IsPlayer())
                if (caster->ToPlayer()->GetQuestStatus(14121) == QUEST_STATUS_INCOMPLETE)
                    caster->ToPlayer()->AddItem(47530, 1);

            return;
        }

        if (me->GetEntry() == NPC_KEZAN_CITIZEN)
            if (roll_chance(50.f))
                Talk(urand(0, 7));

        me->GetScheduler().Schedule(3s, [this](TaskContext /*context*/)
        {
            me->GetMotionMaster()->MoveTargetedHome();
        });
    }
};


// 195186?: the cannon throws the player to the Kezan hills
struct gob_canon_gobelin : public GameObjectAI
{
    gob_canon_gobelin(GameObject* go) : GameObjectAI(go) { }

    bool OnGossipHello(Player* player) override
    {
        player->GetMotionMaster()->MoveJump(0, Position(-7851.79f, 1838.72f, 8.0f, 0.0f), 20.0f, {}, 20.0f);
        return true;
    }
};

enum LifeSavingsBoarding
{
    QUEST_LIFE_SAVINGS              = 14126,
    SPELL_YACHT_MORTAR_COVER        = 92629,
    SPELL_YACHT_LADDER_COVER        = 70389,
};

// Deck height around the phase-384 yacht crowd (Gallywix 35222 is at z 7.2).
static Position const YachtDeckFromMortar = { -7851.79f, 1838.72f, 8.0f, 1.2f };
static Position const YachtDeckFromLadder = { -7854.0f, 1836.0f, 7.9f, 1.2f };

static void JumpOntoYacht(Player* player, Position const& dest, float speedXY, float speedZ)
{
    QuestStatus status = player->GetQuestStatus(QUEST_LIFE_SAVINGS);
    if (status != QUEST_STATUS_INCOMPLETE && status != QUEST_STATUS_COMPLETE)
        return;

    // The Hot Rod is a controlled vehicle. Leave it before the jump, or the player stays seated on the dock.
    if (player->GetVehicle())
        player->ExitVehicle();

    player->GetMotionMaster()->MoveJump(0, dest, speedXY, {}, speedZ);
}

// 92629 Life Savings: Last Chance Yacht Boarding Mortar - Cover (GO 207355)
class spell_life_savings_yacht_mortar : public SpellScript
{
    PrepareSpellScript(spell_life_savings_yacht_mortar);

    void HandleHit(SpellEffIndex /*effIndex*/)
    {
        if (Player* player = GetCaster()->ToPlayer())
            JumpOntoYacht(player, YachtDeckFromMortar, 20.0f, 20.0f);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_life_savings_yacht_mortar::HandleHit, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT);
    }
};

// 70389 Life Savings: Rope Ladder - Cover (GO 201791). 70390 is the jump, but it targets a DB destination this core does not have.
class spell_life_savings_rope_ladder : public SpellScript
{
    PrepareSpellScript(spell_life_savings_rope_ladder);

    void HandleHit(SpellEffIndex /*effIndex*/)
    {
        if (Player* player = GetCaster()->ToPlayer())
            JumpOntoYacht(player, YachtDeckFromLadder, 12.0f, 16.0f);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_life_savings_rope_ladder::HandleHit, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT);
    }
};


enum
{
    SPELL_GASBOT_SUMMON         = 70252,
    SPELL_GASBOT_SUMMON_VISUAL  = 70254,
    SPELL_GASBOT_GAS_STREAM     = 70256,
    SPELL_GASBOT_BIG_FIRE       = 70259,
    SPELL_GASBOT_EXPLOSION      = 70262,
};

// 70253
class spell_summon_gasbot : public SpellScript
{
    PrepareSpellScript(spell_summon_gasbot);

    void HandleHit(SpellEffIndex /*effIndex*/)
    {
        GetCaster()->CastSpell(nullptr, SPELL_GASBOT_SUMMON, true);
        GetCaster()->CastSpell(nullptr, SPELL_GASBOT_SUMMON_VISUAL, true);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_summon_gasbot::HandleHit, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT);
    }
};

// 37598
struct npc_kezan_gasbot : public ScriptedAI
{
    npc_kezan_gasbot(Creature* creature) : ScriptedAI(creature) {}

    void Reset() override
    {
        DoCast(SPELL_GASBOT_GAS_STREAM);

        me->GetScheduler().Schedule(500ms, [this](TaskContext /*context*/)
        {
            me->GetMotionMaster()->MovePoint(1, -8423.85f, 1365.88f, 104.7f);
        });
    }

    void MovementInform(uint32 /*type*/, uint32 id) override
    {
        if (id == 1)
        {
            DoCast(SPELL_GASBOT_BIG_FIRE);
            DoCast(SPELL_GASBOT_EXPLOSION);

            if (me->IsSummon())
                if (Unit* summoner = me->ToTempSummon()->GetSummonerUnit())
                    if (Player* player = summoner->ToPlayer())
                        player->KilledMonsterCredit(me->GetEntry());
        }
    }
};

enum
{
    QUEST_NECESSARY_ROUGHNESS           = 24502,
    QUEST_FOURTH_AND_GOAL               = 28414,

    SPELL_DESPAWN_SHARKS                = 69987,
    SPELL_SUMMON_BILGWATER_BUCCANEER    = 70015,
    SPELL_SUMMON_BILGWATER_BUCCANEER_2  = 70075,
    SPELL_GROUND_RUMBLE_EARTHQUAKE      = 78607,

    SPELL_THROW_FOOTBOMB                = 69993,
    SPELL_KICK_FOOTBOMB                 = 70052,

    SPELL_SUMMON_DEATHWING              = 66322,

    NPC_SUMMON_BILGWATER_BUCCANEER      = 37179,
    NPC_SUMMON_BILGWATER_BUCCANEER_2    = 37213,
};

// 24502
// 28414
class quest_fourth_and_goal : public QuestScript
{
public:
    quest_fourth_and_goal() : QuestScript("quest_fourth_and_goal") { }

    void OnQuestStatusChange(Player* player, Quest const* /*quest*/, QuestStatus /*oldStatus*/, QuestStatus newStatus) override
    {
        if (newStatus == QUEST_STATUS_INCOMPLETE)
        {
            player->ExitVehicle();

            player->GetScheduler().Schedule(1s, [player](TaskContext /*context*/)
            {
                player->RemoveAurasDueToSpell(SPELL_GENERIC_INVISIBILITY_DETECTION_4);
                player->NearTeleportTo(-8250.910156f, 1484.290039f, 41.499901f, 3.124140f);

                player->GetScheduler().Schedule(1s, [player](TaskContext /*context*/)
                {
                    player->CastSpell(nullptr, SPELL_SUMMON_BILGWATER_BUCCANEER_2, true);
                });
            });
        }
        else if (newStatus == QUEST_STATUS_COMPLETE)
        {
            player->CastSpell(player, SPELL_GROUND_RUMBLE_EARTHQUAKE, true);
            player->CastSpell(nullptr, SPELL_SUMMON_DEATHWING, true);
        }
        else if (newStatus == QUEST_STATUS_REWARDED)
        {
            player->CastSpell(player, SPELL_GROUND_RUMBLE_EARTHQUAKE, true);
        }
    }
};

// 37106
struct npc_coach_crosscheck : public ScriptedAI
{
    npc_coach_crosscheck(Creature* creature) : ScriptedAI(creature) { }

    void OnQuestAccept(Player* player, Quest const* quest) override
    {
        if (quest->GetQuestId() == QUEST_NECESSARY_ROUGHNESS)
        {
            player->CastSpell(nullptr, SPELL_SUMMON_BILGWATER_BUCCANEER, true);
            player->RemoveAurasDueToSpell(SPELL_GENERIC_INVISIBILITY_DETECTION_4);
        }
    }
};

const Position SharkPos[8] =
{
    { -8278.00f, 1485.00f, 43.14f, 6.28f },
    { -8285.00f, 1485.00f, 43.90f, 6.28f },
    { -8285.61f, 1489.94f, 43.90f, 6.28f },
    { -8285.76f, 1480.03f, 43.90f, 6.28f },
    { -8290.68f, 1475.38f, 43.95f, 6.28f },
    { -8291.06f, 1480.00f, 43.98f, 6.28f },
    { -8290.98f, 1487.85f ,43.99f, 6.28f },
    { -8290.90f, 1492.98f, 43.90f, 6.28f },
};

// 37179
// 37213
struct npc_boucanier_gob : public ScriptedAI
{
    npc_boucanier_gob(Creature* creature) : ScriptedAI(creature){ }

    void Reset() override
    {
        if (me->GetEntry() == NPC_SUMMON_BILGWATER_BUCCANEER_2)
            if (me->IsSummon())
                if (Unit* summoner = me->ToTempSummon()->GetSummonerUnit())
                    if (summoner->IsPlayer())
                        summoner->EnterVehicle(me);
    }

    void OnCharmed(bool /*apply*/) override { }

    void PassengerBoarded(Unit* who, int8 /*seatId*/, bool apply) override
    {
        Player* player = who->ToPlayer();
        if (!player)
            return;

        if (apply)
        {
            me->SetSpeed(MOVE_RUN, 0.001f);
            player->RemoveAurasDueToSpell(SPELL_GENERIC_INVISIBILITY_DETECTION_4);

            if (player->GetQuestStatus(QUEST_NECESSARY_ROUGHNESS) == QUEST_STATUS_INCOMPLETE)
            {
                player->KilledMonsterCredit(48271, me->GetGUID());
                me->AddAura(70016, me);

                for (uint8 count = 0; count < 8; ++count)
                    me->SummonCreature(37114, SharkPos[count], TEMPSUMMON_TIMED_OR_CORPSE_DESPAWN, 120s);
            }
            else if (player->GetQuestStatus(QUEST_FOURTH_AND_GOAL) == QUEST_STATUS_INCOMPLETE)
                me->AddAura(70065, me);
        }
        else
        {
            player->CastSpell(player, SPELL_GENERIC_INVISIBILITY_DETECTION_4, true);
            me->CastSpell(nullptr, SPELL_DESPAWN_SHARKS, true);
            me->GetScheduler().Schedule(1s, [this](TaskContext /*context*/)
            {
                me->DespawnOrUnsummon();
            });
        }
    }
};

// 37114
struct npc_shark_gob : public ScriptedAI
{
    npc_shark_gob(Creature* creature) : ScriptedAI(creature) {}

    void Reset() override
    {
        me->SetReactState(REACT_PASSIVE);
        me->SetUninteractible(true);
        me->SetWalk(true);

        me->GetScheduler().Schedule(3s, [this](TaskContext /*context*/)
        {
            me->GetMotionMaster()->MovePoint(1, -8260.0f, me->GetPositionY(), me->GetPositionZ());
        });
    }

    void SpellHit(WorldObject* caster, SpellInfo const* spell) override
    {
        if (spell->Id == SPELL_THROW_FOOTBOMB)
        {
            if (Unit* casterUnit = caster->ToUnit())
                if (Unit* passenger = casterUnit->GetVehicleKit() ? casterUnit->GetVehicleKit()->GetPassenger(0) : nullptr)
                    if (Player* player = passenger->ToPlayer())
                        player->KilledMonsterCredit(me->GetEntry(), me->GetGUID());

            if (Unit* casterUnit = caster->ToUnit())
                Unit::Kill(casterUnit, me);
        }
    }

    void JustReachedHome() override { }
};

// 70052
class npc_fourth_and_goal_kick_footbomb : public SpellScript
{
    PrepareSpellScript(npc_fourth_and_goal_kick_footbomb);

    void After()
    {
        if (GetCaster()->IsVehicle())
            if (Unit* passenger = GetCaster()->GetVehicleKit()->GetPassenger(0))
                if (Player* player = passenger->ToPlayer())
                    player->KilledMonsterCredit(37203);
    }

    void Register() override
    {
        AfterCast += SpellCastFn(npc_fourth_and_goal_kick_footbomb::After);
    }
};

// 37203
struct npc_fourth_and_goal_target : public ScriptedAI
{
    npc_fourth_and_goal_target(Creature* creature) : ScriptedAI(creature) {}

    void Reset() override
    {
        me->setActive(true);
    }

    void SpellHit(WorldObject* caster, SpellInfo const* spell) override
    {
        if (spell->Id == SPELL_KICK_FOOTBOMB)
        {
            if (Unit* casterUnit = caster->ToUnit())
                if (Unit* passenger = casterUnit->GetVehicleKit() ? casterUnit->GetVehicleKit()->GetPassenger(0) : nullptr)
                    if (Player* player = passenger->ToPlayer())
                        player->KilledMonsterCredit(me->GetEntry(), me->GetGUID());
        }
    }

    void JustReachedHome() override { }
};

// 48572
struct npc_fourth_and_goal_deathwing : public ScriptedAI
{
    npc_fourth_and_goal_deathwing(Creature* creature) : ScriptedAI(creature) {}

    void Reset() override
    {
        me->setActive(true);

        // TODO waypoints when parser is available
    }
};

// 69987
class spell_kezan_despawn_sharks : public SpellScript
{
    PrepareSpellScript(spell_kezan_despawn_sharks);

    void HandleHit(SpellEffIndex /*effIndex*/)
    {
        if (Creature* creature = GetHitCreature())
            if (creature->IsSummon())
                if (creature->ToTempSummon()->GetSummonerUnit() == GetCaster())
                    creature->DespawnOrUnsummon();
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_kezan_despawn_sharks::HandleHit, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT);
    }
};

enum EndHotRod: uint32
{
    PATH_END_HOT_ROD    = 2102191151,
};

// 37676
struct npc_end_hot_rod : public ScriptedAI
{
    npc_end_hot_rod(Creature* creature) : ScriptedAI(creature) {}

    void EnterEvadeMode(EvadeReason /*why*/) override {}

    void Reset() override
    {
        me->AddUnitState(UNIT_STATE_IGNORE_PATHFINDING);
        me->SetSpeed(MOVE_RUN, 20.f);
        me->SetSpeed(MOVE_WALK, 20.f);
    }

    void PassengerBoarded(Unit* /*who*/, int8 /*seatId*/, bool /*apply*/) override
    {
        me->GetMotionMaster()->MovePath(PATH_END_HOT_ROD, false);
    }

    void MovementInform(uint32 type, uint32 id) override
    {
        if (type != WAYPOINT_MOTION_TYPE)
            return;

        switch (id)
        {
            case 4:
                SassyTalk(0);
                break;
            case 14:
                SassyTalk(1);
                break;
            case 47:
                SassyTalk(2);
                break;
            case 48:
                me->KillSelf();
                break;
            default:
                break;
        }
    }

    void SassyTalk(uint8 id)
    {
        if (me->GetVehicleKit())
            if (Unit* sassy = me->GetVehicleKit()->GetPassenger(0))
                if (sassy->IsCreature())
                    sCreatureTextMgr->SendChat(sassy->ToCreature(), id, me->GetVehicleKit()->GetPassenger(1));
    }
};

// 91847 Life Savings complete plays movie 22; when the client reports the movie finished the player is moved to the Lost Isles
class player_kezan_life_savings_movie : public PlayerScript
{
public:
    player_kezan_life_savings_movie() : PlayerScript("player_kezan_life_savings_movie") { }

    void OnMovieComplete(Player* player, uint32 movieId) override
    {
        if (movieId != 22 || !player->HasAura(91847))
            return;

        player->RemoveAurasDueToSpell(91847);
        player->CastSpell(nullptr, 74100, true); // Life Savings: Teleport & Bind to the Lost Isles
    }
};


} // namespace Chapter1
using namespace Chapter1;

void AddSC_kezan()
{
    new quest_rolling_with_my_homies();
    new vehicle_hot_rod_rolling_my_homies();
    RegisterSpellScript(spell_klaxon);
    RegisterSpellScript(spell_radio);
    RegisterCreatureAI(npc_homies);
    RegisterCreatureAI(npc_homies_summon);
    RegisterCreatureAI(npc_defiant_troll);
    RegisterCreatureAI(npc_bling_bling);
    RegisterCreatureAI(npc_kezan_partygoer);
    RegisterSpellScript(spell_kezan_fireworks);
    RegisterSpellScript(spell_great_bank_heist_vault_interact);
    new vehicle_first_bank_of_kezan_vault();
    RegisterGameObjectAI(gob_first_bank_of_kezan_vault);
    RegisterSpellScript(spell_great_bank_heist_vault_tool);
    RegisterSpellScript(spell_kabummbomb);
    RegisterSpellScript(aura_hobart_ingenious_cap_of_mook_foolery);
    RegisterCreatureAI(npc_citoyen_gob);
    RegisterGameObjectAI(gob_canon_gobelin);
    RegisterSpellScript(spell_life_savings_yacht_mortar);
    RegisterSpellScript(spell_life_savings_rope_ladder);
    RegisterSpellScript(spell_summon_gasbot);
    RegisterCreatureAI(npc_kezan_gasbot);
    new quest_fourth_and_goal();
    RegisterCreatureAI(npc_coach_crosscheck);
    RegisterCreatureAI(npc_boucanier_gob);
    RegisterCreatureAI(npc_shark_gob);
    RegisterSpellScript(npc_fourth_and_goal_kick_footbomb);
    RegisterCreatureAI(npc_fourth_and_goal_target);
    RegisterCreatureAI(npc_fourth_and_goal_deathwing);
    RegisterSpellScript(spell_kezan_despawn_sharks);
    RegisterCreatureAI(npc_end_hot_rod);
    new player_kezan_life_savings_movie();
}
