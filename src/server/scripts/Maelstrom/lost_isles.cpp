/*
 * Copyright (C) 2020 BfaCore
 * Copyright (C) 2011 - 2012 ArkCORE <http://www.arkania.net/>
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

#include "PortCompat.h"
#include "CellImpl.h"
#include "Containers.h"
#include "CreatureAI.h"
#include "CreatureTextMgr.h"
#include "Map.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "ScriptedCreature.h"
#include "Spell.h"
#include "SpellInfo.h"
#include "TaskScheduler.h"
#include "TemporarySummon.h"
#include "Log.h"
#include "Creature.h"
#include "GameObject.h"
#include "GameObjectAI.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Item.h"
#include "PhasingHandler.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "ScriptedEscortAI.h"
#include "ScriptedGossip.h"
#include "SpellAuraEffects.h"
#include "SpellAuras.h"
#include "SpellMgr.h"
#include "SpellScript.h"
#include "Vehicle.h"
#include "WorldSession.h"

#define SAY_D_A "Ecrouabouille, what are you doing sitting there? You do not recognize the one who is lying?"
#define SAY_D_B  "That's $ N! It is only thanks to $ he: she, if we still breath instead of being pieces of rind grids has Kezan."
#define SAY_D_C  "That's $ N! Sorry, doc, I thought $ ghe death:she death; !"
#define SAY_D_D  "Remains back, I'm $ ghe: she; revive! Hoping that this soggy defibrillator will not kill us all!"
#define SAY_D_E  "Caution! OFF!"
#define SAY_D_F "That's all I can do. C'' is a $ ghe: she; to react now. You hear me, $ N? Come on, wake up now! Do not go to the Light!"
#define SAY_D_G  "You made the right choice. We need you so much, $ N. Try not to get killed here. "
#define SAY_D_H  "There are other survivors that I must attend. I'll see you on the shore."


// escort paths of this file live in waypoint_path with ids EscortPathBase + creature entry (imported from the script_waypoint table of the reference core)
constexpr uint32 EscortPathBase = 3700000000;

void StartEscortOf(Creature* creature, bool run, ObjectGuid playerGuid, Quest const* quest = nullptr)
{
    if (EscortAI* ai = dynamic_cast<EscortAI*>(creature->AI()))
    {
        ai->LoadPath(EscortPathBase + creature->GetEntry());
        creature->SetWalk(!run);
        ai->Start(false, playerGuid, quest);
    }
}

enum DontGoIntoTheLight
{
    NPC_GIZMO                           = 36600,
    NPC_DOC_ZAPNOZZLE                   = 36608,
    QUEST_DONT_GO_INTO_THE_LIGHT        = 14239,
    QUEST_LIFE_SAVINGS_REWARDED         = 14126,
    SPELL_DEAD_STILL                    = 69010,

    SPELL_DGITL_QUEST_COMPLETE          = 69013,
    SPELL_DGITL_SUMMON_DOC_ZAPNOZZLE    = 69018,
    SPELL_DGITL_JUMP_CABLES             = 69022,
    SPELL_DGITL_DESPAWN_DOC_ZAPNOZZLE   = 69043,
};

enum Texts
{
    SAY_GYRO = 0
};

// 69010
class spell_lost_isles_near_death : public SpellScript
{
    PrepareSpellScript(spell_lost_isles_near_death);

    void HandleHit(SpellEffIndex /*effIndex*/)
    {
        Unit* caster = GetCaster();
        if (!caster)
            return;

        // Area updates reapply Near Death. If Doc has already finished the revive, take the stun back off.
        if (Creature* doc = caster->FindNearestCreature(NPC_DOC_ZAPNOZZLE, 80.0f))
        {
            if (doc->HasNpcFlag(UNIT_NPC_FLAG_QUESTGIVER))
                caster->RemoveAurasDueToSpell(SPELL_DEAD_STILL);
            return;
        }

        caster->CastSpell(nullptr, SPELL_DGITL_SUMMON_DOC_ZAPNOZZLE, true);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_lost_isles_near_death::HandleHit, EFFECT_0, SPELL_EFFECT_APPLY_AURA);
    }
};

// 36608
struct npc_zapnozzle : public ScriptedAI
{
    npc_zapnozzle(Creature* creature) : ScriptedAI(creature) { }

    Position const beachPosition = { 617.317383f, 3142.259277f, -0.114589f, 2.176555f };

    void Reset() override
    {
        me->SetDisableGravity(true);
        me->SetSwim(true);
        me->GetMotionMaster()->MovePoint(1, 538.785034f, 3271.091797f, -1.082726f);
        me->RemoveNpcFlag(UNIT_NPC_FLAG_QUESTGIVER);
    }

    void IsSummonedBy(WorldObject* summoner) override
    {
        _playerGuid = summoner->GetGUID();
    }

    void MovementInform(uint32 /*type*/, uint32 id) override
    {
        switch (id)
        {
            case 1:
                me->SetDisableGravity(false);
                me->SetSwim(false);
                me->GetMotionMaster()->MoveJump(2, Position(537.296875f, 3272.306396f, 0.166942f, 3.011669f), 10.f, {}, 10.f);
                break;
            case 2:
                Talk(0, GetSummoner());
                InitFirstEventSchedulers();
                break;
            case 3:
                me->SetDisableGravity(true);
                me->SetSwim(true);
                Talk(6, GetSummoner());
                me->GetMotionMaster()->MovePoint(4, beachPosition);
                me->DespawnOrUnsummon(5s);
                break;
            default:
                break;
        }
    }

    void InitFirstEventSchedulers()
    {
        me->GetScheduler().
            Schedule(5s, [this](TaskContext /*context*/)
            {
                Talk(1, GetSummoner());
            }).
            Schedule(9s, [this](TaskContext /*context*/)
            {
                if (Creature* gizmo = GetGizmo())
                    gizmo->AI()->Talk(0, GetSummoner());
            }).
            Schedule(15s, [this](TaskContext /*context*/)
            {
                Talk(2, GetSummoner());
            }).
            Schedule(21s, [this](TaskContext /*context*/)
            {
                me->CastSpell(GetSummoner(), SPELL_DGITL_JUMP_CABLES, false);
            }).
            Schedule(27s, [this](TaskContext /*context*/)
            {
                Talk(3, GetSummoner());
            }).
            Schedule(31s, [this](TaskContext /*context*/)
            {
                me->CastSpell(GetSummoner(), SPELL_DGITL_JUMP_CABLES, false);
            }).
            Schedule(37s, [this](TaskContext /*context*/)
            {
                Talk(4, GetSummoner());
                // Jumper Cables is only a dummy. The stun stays until this line, which is when he offers the quest.
                if (Player* player = GetSummoner())
                    player->RemoveAurasDueToSpell(SPELL_DEAD_STILL);
                me->SetNpcFlag(UNIT_NPC_FLAG_QUESTGIVER);
            });
    }

    void OnQuestReward(Player* /*player*/, Quest const* quest, LootItemType /*type*/, uint32 /*opt*/) override
    {
        if (quest->GetQuestId() == 14239)
            InitSecondEventSchedulers();
    }

    void InitSecondEventSchedulers()
    {
        me->GetScheduler().
            Schedule(1s, [this](TaskContext /*context*/)
            {
                Talk(5, GetSummoner());
            }).
            Schedule(4s, [this](TaskContext /*context*/)
            {
                me->GetMotionMaster()->MoveJump(3, Position(539.637451f, 3266.728516f, -1.139869f, 5.053704f), 10.f, {}, 10.f);
            });
    }

private :
    ObjectGuid _playerGuid;

    Player* GetSummoner()
    {
        return ObjectAccessor::GetPlayer(*me, _playerGuid);
    }

    Creature* GetGizmo()
    {
        return me->FindNearestCreature(NPC_GIZMO, 10.f);
    }
};

enum GoblinEscapePod
{
    SPELL_SUMMON_LIVE_GOBLIN_SURVIVOR   = 66137,
    SPELL_TRADE_PRINCE_CONTROLLER_AURA  = 67433,
    SPELL_SUMMON_TRADE_PRINCE           = 67845,

    NCP_TRADE_PRINCE_GALLYWIX           = 35649
};

// 195188
class gob_goblin_escape_pod : public GameObjectAI
{
public:
    gob_goblin_escape_pod(GameObject* go) : GameObjectAI(go) { }

    void OnLootStateChanged(uint32 state, Unit* unit) override
    {
        if (unit && state == GO_ACTIVATED)
        {
            if (Player* player = unit->ToPlayer())
            {
                if (!player->HasAura(SPELL_TRADE_PRINCE_CONTROLLER_AURA))
                {
                    player->CastSpell(nullptr, SPELL_SUMMON_TRADE_PRINCE, true);
                    player->CastSpell(nullptr, SPELL_TRADE_PRINCE_CONTROLLER_AURA, true);
                }
                else
                {
                    player->CastSpell(nullptr, SPELL_SUMMON_LIVE_GOBLIN_SURVIVOR, true);
                }

                player->KilledMonsterCredit(34748);
                me->DestroyForPlayer(player);
            }
        }
    }
};

// 34748
// 35649
struct npc_goblin_espace_pod : public ScriptedAI
{
    npc_goblin_espace_pod(Creature* creature) : ScriptedAI(creature) {}

    Position const beachPosition = { 617.317383f, 3142.259277f, -0.114589f, 2.176555f };

    void Reset() override
    {
        me->SetDisableGravity(true);
        me->SetSwim(true);

        me->GetScheduler().Schedule(1s, [this](TaskContext /*context*/)
        {
            Player* nearestPlayer = me->SelectNearestPlayer(20.f);

            if (me->GetEntry() == NCP_TRADE_PRINCE_GALLYWIX)
                Talk(0, nearestPlayer);
            else
                Talk(urand(0, 3), nearestPlayer);

        }).Schedule(2s, [this](TaskContext /*context*/)
        {
            me->GetMotionMaster()->MovePoint(1, beachPosition);
            me->DespawnOrUnsummon(3s);
        });
    }
};

enum BombMonkey
{
    QUEST_MONKEY_BUSINESS           = 14019,

    SPELL_MONKEY_COSMETIC_THROW_BOMB= 66142,
    SPELL_NITRO_POTASSIUM_BANANAS   = 67917,
    SPELL_EXPLOSIVE_BANANA          = 67919,

    NPC_BOMB_THROWING_MONKEY        = 34699,
    NPC_MONKEY_KILL_CREDIT          = 35760
};

// 34699
struct npc_bomb_monkey : public ScriptedAI
{
    npc_bomb_monkey(Creature* creature) : ScriptedAI(creature) { }

    void Reset() override
    {
        me->GetScheduler().Schedule(5s, 30s, [this](TaskContext context)
        {
            me->CastSpell(nullptr, SPELL_MONKEY_COSMETIC_THROW_BOMB, false);
            context.Repeat(5s, 30s);
        });
    }

    void SpellHit(WorldObject* caster, SpellInfo const* spell) override
    {
        if (Player* player = caster->ToPlayer())
        {
            if (spell->Id == SPELL_NITRO_POTASSIUM_BANANAS)
            {
                if (me->IsUninteractible() || me->HasAura(SPELL_EXPLOSIVE_BANANA))
                    return;

                me->SetUninteractible(true);
                if (me->IsNonMeleeSpellCast(true))
                    me->InterruptNonMeleeSpells(true);

                player->KilledMonsterCredit(NPC_MONKEY_KILL_CREDIT);
                me->CastSpell(me, SPELL_EXPLOSIVE_BANANA, true);
                me->DespawnOrUnsummon(3s);
            }
        }
    }
};

// 67917 - Nitro-Potassium Bananas. The item spell is a script effect aimed at the caster,
// so the monkey's SpellHit never sees it.
class spell_lost_isles_nitro_potassium_bananas : public SpellScript
{
    PrepareSpellScript(spell_lost_isles_nitro_potassium_bananas);

    void HandleScript(SpellEffIndex /*effIndex*/)
    {
        Unit* caster = GetCaster();
        if (!caster)
            return;

        Player* player = caster->ToPlayer();
        if (!player || player->GetQuestStatus(QUEST_MONKEY_BUSINESS) != QUEST_STATUS_INCOMPLETE)
            return;

        auto isFreshMonkey = [](Creature* creature)
        {
            return creature->IsAlive() && !creature->IsUninteractible() && !creature->HasAura(SPELL_EXPLOSIVE_BANANA);
        };

        Creature* monkey = nullptr;
        if (Unit* selected = player->GetSelectedUnit())
            if (Creature* creature = selected->ToCreature())
                if (creature->GetEntry() == NPC_BOMB_THROWING_MONKEY && isFreshMonkey(creature) && player->IsWithinDist(creature, 35.0f))
                    monkey = creature;

        if (!monkey)
        {
            std::list<Creature*> monkeys;
            player->GetCreatureListWithEntryInGrid(monkeys, NPC_BOMB_THROWING_MONKEY, 35.0f);
            float best = 35.0f;
            for (Creature* candidate : monkeys)
            {
                if (!isFreshMonkey(candidate))
                    continue;

                float distance = player->GetDistance(candidate);
                if (distance <= best)
                {
                    best = distance;
                    monkey = candidate;
                }
            }
        }

        if (!monkey)
            return;

        // Drop the target immediately so another throw cannot credit this same spawn during the explosion.
        monkey->SetUninteractible(true);
        if (monkey->IsNonMeleeSpellCast(true))
            monkey->InterruptNonMeleeSpells(true);

        player->KilledMonsterCredit(NPC_MONKEY_KILL_CREDIT);
        monkey->CastSpell(monkey, SPELL_EXPLOSIVE_BANANA, true);
        monkey->DespawnOrUnsummon(3s);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_lost_isles_nitro_potassium_bananas::HandleScript, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT);
    }
};

enum MinerTroublesQuest
{
    QUEST_MINER_TROUBLES                = 14021,

    SPELL_MINER_TROUBLESQUEST_ACCEPT    = 68061,
    SPELL_MINER_TROUBLESQUEST_COMPLETE  = 68063,
    SPELL_SUMMON_ORE_CART               = 68064,
    SPELL_MINER_TROUBLES_CHAINS         = 68122,

    NPC_FRIGHTENED_MINER                = 35813,
    NPC_ORE_CART                        = 35814,
};

enum CapturingTheUnknown
{
    QUEST_CAPTURING_THE_UNKNOWN         = 14031,
    SPELL_CTU_FLASH                     = 68281,
    SPELL_CTU_BIND_SIGHT                = 70641,
    SPELL_CTU_SCREEN_EFFECT             = 70649,

    SPELL_CTU_SEE_INVIS_1               = 70661,
    SPELL_CTU_SEE_INVIS_2               = 70678,
    SPELL_CTU_SEE_INVIS_3               = 70680,
    SPELL_CTU_SEE_INVIS_4               = 70681,

    NPC_CTU_BUNNY_1                     = 37872,
    NPC_CTU_BUNNY_2                     = 37895,
    NPC_CTU_BUNNY_3                     = 37896,
    NPC_CTU_BUNNY_4                     = 37897,
};

// 35769
struct npc_foreman_dampwick : public ScriptedAI
{
    npc_foreman_dampwick(Creature* creature) : ScriptedAI(creature) {}

    void OnQuestAccept(Player* player, Quest const* quest) override
    {
        if (quest->GetQuestId() == QUEST_MINER_TROUBLES)
        {
            Talk(0);
            me->CastSpell(player, SPELL_MINER_TROUBLESQUEST_ACCEPT, true);

            if (Creature* miner = PortCompat::GetSummonedCreatureByEntry(player, NPC_FRIGHTENED_MINER))
            {
                miner->CastSpell(nullptr, SPELL_SUMMON_ORE_CART, true);
                if (Creature* oreCart = PortCompat::GetSummonedCreatureByEntry(miner, NPC_ORE_CART))
                {
                    oreCart->CastSpell(miner, SPELL_MINER_TROUBLES_CHAINS, true);
                    oreCart->GetMotionMaster()->MoveFollow(miner, 1.f, float(M_PI));
                }

                StartEscortOf(miner, false, player->GetGUID(), quest);
            }
        }
    }
};

// 35813
struct npc_frightened_miner_escort : public EscortAI
{
    npc_frightened_miner_escort(Creature* creature) : EscortAI(creature) {}

    void AttackStart(Unit* /*who*/) override {}
    void JustEngagedWith(Unit* /*who*/) override {}
    void EnterEvadeMode(EvadeReason /*why*/) override {}
    void JustDied(Unit* /*killer*/) override {}
    void OnCharmed(bool /*apply*/) override {}

    void WaypointReached(uint32 pointId, uint32 /*pathId*/) override
    {
        switch(pointId)
        {
            case 3:
                Talk(1, GetPlayerForEscort());
                break;
            case 4:
            case 7:
            case 10:
            case 12:
                me->HandleEmoteCommand(Emote(467));
                break;
            case 13:
                me->SetWalk(false);
                Talk(5, GetPlayerForEscort());
                if (Player *player = GetPlayerForEscort())
                    player->KilledMonsterCredit(35816);

                if (Creature* oreCart = PortCompat::GetSummonedCreatureByEntry(me, NPC_ORE_CART))
                    oreCart->DespawnOrUnsummon();
                break;
            default:
                break;
        }
    }

    void WaypointStarted(uint32 pointId, uint32 /*pathId*/) override
    {
        me->HandleEmoteCommand(EMOTE_ONESHOT_NONE);
        switch(pointId)
        {
            case 0:
                Talk(0, GetPlayerForEscort());
                break;
            case 4:
                Talk(2, GetPlayerForEscort());
                break;
            case 8:
                Talk(3, GetPlayerForEscort());
                break;
            case 10:
                Talk(4, GetPlayerForEscort());
                break;
            case 13:
                me->SetWalk(false);
                Talk(5, GetPlayerForEscort());
                if (Player *player = GetPlayerForEscort())
                    player->KilledMonsterCredit(35816);

                if (Creature* oreCart = PortCompat::GetSummonedCreatureByEntry(me, NPC_ORE_CART))
                    oreCart->DespawnOrUnsummon();
                break;
            default:
                break;
        }
    }
};

std::map<uint32, uint32> const ctuSpellByBunny
{
    { NPC_CTU_BUNNY_1,  SPELL_CTU_SEE_INVIS_1 },
    { NPC_CTU_BUNNY_2,  SPELL_CTU_SEE_INVIS_2 },
    { NPC_CTU_BUNNY_3,  SPELL_CTU_SEE_INVIS_3 },
    { NPC_CTU_BUNNY_4,  SPELL_CTU_SEE_INVIS_4 },
};

// 68280
class spell_ctu_snapflash : public SpellScript
{
    PrepareSpellScript(spell_ctu_snapflash);

    void SelectPainting(std::list<WorldObject*>& targets)
    {
        targets.clear();

        Unit* caster = GetCaster();
        Player* player = caster ? caster->ToPlayer() : nullptr;
        if (!player || player->GetQuestStatus(QUEST_CAPTURING_THE_UNKNOWN) != QUEST_STATUS_INCOMPLETE)
            return;

        // The area target never selects these markers: they are uninteractible and invisible.
        // Credit the painting the player is standing on.
        Creature* painting = nullptr;
        float best = 20.0f;
        for (auto const& paintingEntry : ctuSpellByBunny)
        {
            std::list<Creature*> found;
            player->GetCreatureListWithEntryInGrid(found, paintingEntry.first, 20.0f);
            for (Creature* candidate : found)
            {
                if (!candidate->IsAlive())
                    continue;

                float distance = player->GetDistance(candidate);
                if (distance < best)
                {
                    best = distance;
                    painting = candidate;
                }
            }
        }

        if (painting)
            targets.push_back(painting);
    }

    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        Unit* unit = GetHitUnit();
        if (!unit)
            return;

        auto bunny = ctuSpellByBunny.find(unit->GetEntry());
        if (bunny == ctuSpellByBunny.end())
            return;

        Player* casterPlayer = GetCaster() ? GetCaster()->ToPlayer() : nullptr;
        if (!casterPlayer)
            return;

        casterPlayer->RemoveAurasDueToSpell(bunny->second);

        casterPlayer->CastSpell(nullptr, SPELL_CTU_BIND_SIGHT, true);
        casterPlayer->CastSpell(nullptr, SPELL_CTU_SCREEN_EFFECT, true);

        uint32 unitEntry = unit->GetEntry();
        ObjectGuid unitGuid = unit->GetGUID();
        casterPlayer->GetScheduler().Schedule(3s, [casterPlayer, unitEntry, unitGuid](TaskContext /*context*/)
        {
            if (!casterPlayer->IsInWorld())
                return;

            casterPlayer->RemoveAurasDueToSpell(SPELL_CTU_SCREEN_EFFECT);
            casterPlayer->KilledMonsterCredit(unitEntry, unitGuid);
        });
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_ctu_snapflash::SelectPainting, EFFECT_0, TARGET_UNIT_SRC_AREA_ENTRY);
        OnEffectHitTarget += SpellEffectFn(spell_ctu_snapflash::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 70641
class spell_ctu_bind_sight : public SpellScript
{
    PrepareSpellScript(spell_ctu_bind_sight);

    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        if (Unit* unit = GetHitUnit())
            unit->CastSpell(unit, SPELL_CTU_FLASH, true);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_ctu_bind_sight::HandleDummy, EFFECT_0, SPELL_EFFECT_APPLY_AURA);
    }
};

enum LostIslesWeed
{
    QUEST_WEED_WHAKER       = 14236,
    SPELL_WEED_WHACKER_AURA = 68212,
    // 12.1 has no SpellXSpellVisual for 68212, so the retail blade does not draw.
    // A plain two-hand sword is the held weapon while the aura is up.
    ITEM_WEED_WHACKER_BLADE = 2489
};

struct npc_lost_isles_weed : public ScriptedAI
{
    npc_lost_isles_weed(Creature* creature) : ScriptedAI(creature) {}

    void SpellHit(WorldObject* caster, SpellInfo const* spell) override
    {
        if (me->IsAlive())
        {
            if (spell->Id == 68213)
            {
                if (Player* player = caster->ToPlayer())
                {
                    player->KilledMonsterCredit(35897);
                    Unit::Kill(player, me);
                }
            }
        }
    }

    void AttackStart(Unit* /*who*/) override {}
    void JustEngagedWith(Unit* /*who*/) override {}
};

class spell_weed_whacker : public SpellScript
{
    PrepareSpellScript(spell_weed_whacker);

    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        if (GetCastItem())
        {
            if (Unit* caster = GetCastItem()->GetOwner())
            {
                if (Player* player = caster->ToPlayer())
                {
                    if (player->HasAura(SPELL_WEED_WHACKER_AURA))
                        player->RemoveAura(SPELL_WEED_WHACKER_AURA);
                    else if (player->GetQuestStatus(QUEST_WEED_WHAKER) == QUEST_STATUS_INCOMPLETE)
                        player->CastSpell(player, SPELL_WEED_WHACKER_AURA, true);
                }
            }
        }
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_weed_whacker::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

class spell_weed_whacker_aura : public AuraScript
{
    PrepareAuraScript(spell_weed_whacker_aura);

    static void ShowBlade(Player* player)
    {
        Item* blade = Item::CreateItem(ITEM_WEED_WHACKER_BLADE, 1, ItemContext::NONE, player, false);
        if (!blade)
            return;

        player->SetVisibleItemSlot(EQUIPMENT_SLOT_MAINHAND, blade);
        player->SetVisibleItemSlot(EQUIPMENT_SLOT_OFFHAND, nullptr);
        delete blade;
        player->SetSheath(SHEATH_STATE_MELEE);
    }

    static void RestoreWeapons(Player* player)
    {
        player->SetVisibleItemSlot(EQUIPMENT_SLOT_MAINHAND, player->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND));
        player->SetVisibleItemSlot(EQUIPMENT_SLOT_OFFHAND, player->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_OFFHAND));
    }

    void OnApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        if (Player* player = GetTarget()->ToPlayer())
            ShowBlade(player);
    }

    void OnRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        if (Player* player = GetTarget()->ToPlayer())
            RestoreWeapons(player);
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(spell_weed_whacker_aura::OnApply, EFFECT_0, SPELL_AURA_SET_VEHICLE_ID, AURA_EFFECT_HANDLE_REAL);
        AfterEffectRemove += AuraEffectRemoveFn(spell_weed_whacker_aura::OnRemove, EFFECT_0, SPELL_AURA_SET_VEHICLE_ID, AURA_EFFECT_HANDLE_REAL);
    }
};

enum Infrared
{
    QUEST_INFRARED_INFRADEAD        = 14238,
    QUEST_TO_THE_CLIFFS             = 14240,

    NPC_ORC_SCOUT                   = 36100,
    NPC_CLIFF_BASTIA                = 36585,

    SPELL_INFRARED_QUEST_ACCEPT     = 68344,
    SPELL_INFRARED_ORC_SCOUT_AURA   = 68338,
    SPELL_CLIFF_SUMMON_BASTIA       = 68973,
};

// 35917
struct npc_killag_sangrecroc : public ScriptedAI
{
    npc_killag_sangrecroc(Creature* creature) : ScriptedAI(creature) {}

    void OnQuestAccept(Player* player, Quest const* quest) override
    {
        if (quest->GetQuestId() == QUEST_INFRARED_INFRADEAD)
        {
            Talk(0, player);
            me->CastSpell(player, SPELL_INFRARED_QUEST_ACCEPT, true);
        }

        if (quest->GetQuestId() == QUEST_TO_THE_CLIFFS)
        {
            player->CastSpell(player, SPELL_CLIFF_SUMMON_BASTIA, true);

            if (Creature* bastia = PortCompat::GetSummonedCreatureByEntry(player, NPC_CLIFF_BASTIA))
            {
                Talk(1, player);
                StartEscortOf(bastia, true, player->GetGUID(), quest);
            }
        }
    }

    void OnQuestReward(Player* player, Quest const* quest, LootItemType /*type*/, uint32 /*opt*/) override
    {
        if (quest->GetQuestId() == QUEST_INFRARED_INFRADEAD)
            player->RemoveAurasDueToSpell(SPELL_INFRARED_ORC_SCOUT_AURA);
    }
};

// 68338
class aura_infrared_orc_scout : public AuraScript
{
    PrepareAuraScript(aura_infrared_orc_scout);

    void OnRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        if (Creature* orcScout = PortCompat::GetSummonedCreatureByEntry(GetTarget(), NPC_ORC_SCOUT))
            orcScout->DespawnOrUnsummon();
    }

    void Register() override
    {
        AfterEffectRemove += AuraEffectRemoveFn(aura_infrared_orc_scout::OnRemove, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }
};

// 36578
struct npc_cliff_bastia : public EscortAI
{
    npc_cliff_bastia(Creature* creature) : EscortAI(creature) {}

    void AttackStart(Unit* /*who*/) override {}
    void JustEngagedWith(Unit* /*who*/) override {}
    void EnterEvadeMode(EvadeReason /*why*/) override {}
    void OnCharmed(bool /*apply*/) override {}

    void Reset() override
    {
        me->SetSpeed(MOVE_RUN, 14.f);
    }

    void WaypointReached(uint32 pointId, uint32 /*pathId*/) override
    {
        if (pointId == 20)
            me->DespawnOrUnsummon();
    }
};

enum PreciousCargo
{
    QUEST_PRECIOUS_CARO                 = 14242,
    QUEST_MEET_ME_UP_TOP                = 14326,

    SPELL_PRECIOUS_CARGO_QUEST_ACCEPT   = 68386,
    SPELL_PRECIOUS_CARGO_KILL_CREDIT    = 69081,

    NPC_GYROCOPTER                      = 36143,

    GOB_ARCANE_CAGE                     = 195704,
};

// 36127
struct npc_gyrocopterequest_giver : public ScriptedAI
{
    npc_gyrocopterequest_giver(Creature* creature) : ScriptedAI(creature) {}

    void OnQuestAccept(Player* player, Quest const* quest) override
    {
        if (quest->GetQuestId() == QUEST_PRECIOUS_CARO)
        {
            player->CastSpell(player, SPELL_PRECIOUS_CARGO_QUEST_ACCEPT, true);

            if (Creature* gyrocopter = PortCompat::GetSummonedCreatureByEntry(player, NPC_GYROCOPTER))
                StartEscortOf(gyrocopter, true, player->GetGUID(), quest);
        }
    }
};

// 36143
struct npc_precious_cargo_gyrocopter : public EscortAI
{
    npc_precious_cargo_gyrocopter(Creature* creature) : EscortAI(creature) {}

    void AttackStart(Unit* /*who*/) override {}
    void JustEngagedWith(Unit* /*who*/) override {}
    void EnterEvadeMode(EvadeReason /*why*/) override {}
    void OnCharmed(bool /*apply*/) override {}

    void Reset() override
    {
        me->SetCanFly(true);
        me->SetSpeed(MOVE_FLIGHT, 21.0f);
    }

    void WaypointReached(uint32 pointId, uint32 /*pathId*/) override
    {
        if (pointId == 3)
        {
            if (Player* player = GetPlayerForEscort())
                me->CastSpell(player, SPELL_PRECIOUS_CARGO_KILL_CREDIT, true);

            me->DespawnOrUnsummon();
        }
    }
};

// 36145
struct npc_lost_isles_thrall_prisonner : public ScriptedAI
{
    npc_lost_isles_thrall_prisonner(Creature* creature) : ScriptedAI(creature) {}

    void MoveInLineOfSight(Unit* who) override
    {
        if (Player* player = who->ToPlayer())
            if (player->GetQuestStatus(QUEST_PRECIOUS_CARO) == QUEST_STATUS_INCOMPLETE)
                if (player->GetDistance(me) < 5.f)
                    player->KilledMonsterCredit(me->GetEntry());
    }

    void OnQuestAccept(Player* player, Quest const* /*quest*/) override
    {
        Talk(0, player);
    }
};

enum WarchiefsRevenge
{
    QUEST_WARCHIEF_REVENGE  = 14243,

    SPELL_WR_QUEST_ACCEPT   = 68408,

    NPC_WR_CYCLONE          = 36178,
};

// 36127
struct npc_lost_isles_thrall_top_boat : public ScriptedAI
{
    npc_lost_isles_thrall_top_boat(Creature* creature) : ScriptedAI(creature) {}

    void OnQuestAccept(Player* player, Quest const* quest) override
    {
        if (quest->GetQuestId() == QUEST_WARCHIEF_REVENGE)
        {
            player->CastSpell(player, SPELL_WR_QUEST_ACCEPT, true);

            if (Creature* cyclone = PortCompat::GetSummonedCreatureByEntry(player, NPC_WR_CYCLONE))
                cyclone->AI()->DoAction(0);
        }
    }
};

// 36178
struct npc_warchief_revenge_cyclone : public EscortAI
{
    npc_warchief_revenge_cyclone(Creature* creature) : EscortAI(creature) {}

    void AttackStart(Unit* /*who*/) override {}
    void JustEngagedWith(Unit* /*who*/) override {}
    void EnterEvadeMode(EvadeReason /*why*/) override {}
    void OnCharmed(bool /*apply*/) override {}

    void Reset() override
    {
        me->SetCanFly(true);
        me->SetSpeed(MOVE_FLIGHT, 7.0f);
    }

    void DoAction(int32 /*action*/) override
    {
        me->GetMotionMaster()->MovePoint(1001, 1001.373047f, 3832.485840f, 14.062465f);

        me->GetScheduler().Schedule(1s, [this](TaskContext context)
        {
            if (Vehicle* vehicle = me->GetVehicleKit())
            {
                if (Unit* passenger = vehicle->GetPassenger(0))
                {
                    if (Player* player = passenger->ToPlayer())
                    {
                        if (player->GetQuestStatus(QUEST_WARCHIEF_REVENGE) == QUEST_STATUS_COMPLETE)
                        {
                            SetEscortPaused(true);
                            me->SetSpeed(MOVE_FLIGHT, 35.0f);
                            me->GetMotionMaster()->MovePoint(1002, 885.869568f, 3104.882324f, 175.560944f, false);
                        }
                        else
                            context.Repeat();
                    }
                }
            }
        });
    }

    void MovementInform(uint32 /*type*/, uint32 id) override
    {
        switch (id)
        {
            case 1001:
                LoadPath(EscortPathBase + me->GetEntry());
                me->SetWalk(false);
                Start(false, ObjectGuid::Empty, nullptr, false, true);
                break;
            case 1002:
                me->GetMotionMaster()->MovePoint(1003, 863.672180f, 2778.023193f, 126.243454f, false);
                break;
            case 1003:
                //Add teleport cuz the npc drops you mid air before reaching the wp for some reason
                if (Vehicle* vehicle = me->GetVehicleKit())
                    if (Unit* passenger = vehicle->GetPassenger(0))
                        if (Player* player = passenger->ToPlayer())
                            player->NearTeleportTo(863.672180f, 2778.023193f, 126.243454f, 0, false);
                if (me->IsAlive())
                    me->KillSelf();
               // me->DespawnOrUnsummon();
                break;
            default:
                break;
        }
    }
};

enum UpUpAndAway
{
    QUEST_UP_UP_AND_AWAY            = 14244,

    SPELL_ROCKET_BLAST              = 66110,
    SPELL_SUMMON_CHARACTER_ROCKET   = 68806,
    SPELL_UUAA_KILL_CREDIT          = 68813,
    SPELL_SUMMON_GALLYWIX           = 68815,
    SPELL_SUMMON_GALLYWIX_ROCKET    = 68819,

    NPC_GALLYWIX_ROCKET             = 36514
};

// 36425
struct npc_sassy_rocket_sling : public ScriptedAI
{
    npc_sassy_rocket_sling(Creature* creature) : ScriptedAI(creature) {}

    void OnQuestAccept(Player* player, Quest const* quest) override
    {
        if (quest->GetQuestId() == QUEST_UP_UP_AND_AWAY)
            player->CastSpell(player, SPELL_SUMMON_GALLYWIX, true);
    }
};

// 36513
struct npc_gallywix_sling_rocket : public ScriptedAI
{
    npc_gallywix_sling_rocket(Creature* creature) : ScriptedAI(creature) {}

    void Reset() override
    {
        Talk(0);

        me->GetScheduler().Schedule(5s, [this](TaskContext /*context*/)
        {
            Talk(1);
        }).Schedule(5s, [this](TaskContext /*context*/)
        {
            if (me->ToTempSummon())
                Talk(2, me->ToTempSummon()->GetSummoner());
        });

        me->CastSpell(me, SPELL_SUMMON_GALLYWIX_ROCKET, true);

        if (Creature* rocket = PortCompat::GetSummonedCreatureByEntry(me, NPC_GALLYWIX_ROCKET))
        {
            rocket->SetCanFly(true);
            rocket->SetSpeed(MOVE_FLIGHT, 21.f);
        }

        me->GetMotionMaster()->MovePoint(1, 869.895996f, 2744.302246f, 122.355782f);
    }

    void MovementInform(uint32 /*type*/, uint32 id) override
    {
        if (id == 1)
        {
            if (Creature* rocket = PortCompat::GetSummonedCreatureByEntry(me, NPC_GALLYWIX_ROCKET))
            {
                me->EnterVehicle(rocket);

                me->GetScheduler().Schedule(2s, [rocket](TaskContext /*context*/)
                {
                    rocket->CastSpell(rocket, SPELL_ROCKET_BLAST, true);
                    rocket->GetMotionMaster()->MovePoint(1, 907.387085f, 2662.028320f, 193.833740f);
                    rocket->DespawnOrUnsummon(5s);
                });
            }
        }
    }
};

// 196439
class gob_rocket_sling : public GameObjectAI
{
public:
    gob_rocket_sling(GameObject* go) : GameObjectAI(go) { }

    bool OnGossipSelect(Player* player, uint32 /*menuId*/, uint32 /*gossipListId*/) override
    {
        if (player->GetQuestStatus(QUEST_UP_UP_AND_AWAY) != QUEST_STATUS_NONE)
            player->CastSpell(player, SPELL_SUMMON_CHARACTER_ROCKET, true);

        return true;
    }
};

// 36505
struct npc_sling_rocket : public EscortAI
{
    npc_sling_rocket(Creature* creature) : EscortAI(creature) {}

    void AttackStart(Unit* /*who*/) override {}
    void JustEngagedWith(Unit* /*who*/) override {}
    void EnterEvadeMode(EvadeReason /*why*/) override {}
    void OnCharmed(bool /*apply*/) override {}

    void PassengerBoarded(Unit* who, int8 /*seatId*/, bool apply) override
    {
        if (apply)
        {
            me->SetCanFly(true);
            me->SetDisableGravity(true);
            me->SetSpeed(MOVE_FLIGHT, 21.0f);
            me->CastSpell(me, SPELL_ROCKET_BLAST, true);
            LoadPath(EscortPathBase + me->GetEntry());
            me->SetWalk(false);
            Start(false, who->GetGUID());
        }
    }

    void WaypointReached(uint32 pointId, uint32 /*pathId*/) override
    {
        if (pointId == 3)
        {
            if (Vehicle* vehicle = me->GetVehicleKit())
                if (Unit* passenger = vehicle->GetPassenger(0))
                    me->CastSpell(passenger, SPELL_UUAA_KILL_CREDIT, true);

            me->DespawnOrUnsummon();
        }
    }
};

// 71091
class spell_tiab_effect2 : public SpellScript
{
    PrepareSpellScript(spell_tiab_effect2);

    void HandleDummy(SpellEffIndex effIndex)
    {
        PreventHitDefaultEffect(effIndex);

        if (Player* casterPlayer = GetCaster()->ToPlayer())
            casterPlayer->KilledMonsterCredit(38024);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_tiab_effect2::HandleDummy, EFFECT_0, SPELL_EFFECT_FORCE_CAST);
    }
};


// A world restart despawns the summoned doc and leaves Near Death saved on the player.
class player_lost_isles_near_death_recover : public PlayerScript
{
public:
    player_lost_isles_near_death_recover() : PlayerScript("player_lost_isles_near_death_recover") { }

    void OnLogin(Player* player, bool /*firstLogin*/) override
    {
        player->GetScheduler().Schedule(2s, [player](TaskContext /*context*/)
        {
            if (!player->IsInWorld())
                return;
            if (player->GetQuestStatus(QUEST_LIFE_SAVINGS_REWARDED) != QUEST_STATUS_REWARDED)
                return;
            if (player->GetQuestStatus(QUEST_DONT_GO_INTO_THE_LIGHT) != QUEST_STATUS_NONE)
                return;
            if (player->FindNearestCreature(NPC_DOC_ZAPNOZZLE, 80.0f))
                return;

            player->CastSpell(nullptr, SPELL_DGITL_SUMMON_DOC_ZAPNOZZLE, true);
        });
    }
};

void AddSC_lost_isles()
{
    RegisterSpellScript(spell_lost_isles_near_death);
    RegisterCreatureAI(npc_zapnozzle);
    new player_lost_isles_near_death_recover();
    RegisterGameObjectAI(gob_goblin_escape_pod);
    RegisterCreatureAI(npc_goblin_espace_pod);
    RegisterCreatureAI(npc_bomb_monkey);
    RegisterSpellScript(spell_lost_isles_nitro_potassium_bananas);
    RegisterCreatureAI(npc_foreman_dampwick);
    RegisterCreatureAI(npc_frightened_miner_escort);
    RegisterSpellScript(spell_ctu_snapflash);
    RegisterSpellScript(spell_ctu_bind_sight);
    RegisterCreatureAI(npc_lost_isles_weed);
    RegisterSpellScript(spell_weed_whacker);
    RegisterSpellScript(spell_weed_whacker_aura);
    RegisterCreatureAI(npc_killag_sangrecroc);
    RegisterSpellScript(aura_infrared_orc_scout);
    RegisterCreatureAI(npc_cliff_bastia);
    RegisterCreatureAI(npc_gyrocopterequest_giver);
    RegisterCreatureAI(npc_precious_cargo_gyrocopter);
    RegisterCreatureAI(npc_lost_isles_thrall_prisonner);
    RegisterCreatureAI(npc_lost_isles_thrall_top_boat);
    RegisterCreatureAI(npc_warchief_revenge_cyclone);
    RegisterCreatureAI(npc_sassy_rocket_sling);
    RegisterCreatureAI(npc_gallywix_sling_rocket);
    RegisterGameObjectAI(gob_rocket_sling);
    RegisterCreatureAI(npc_sling_rocket);
    RegisterSpellScript(spell_tiab_effect2);
}
