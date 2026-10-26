/*
 * Copyright (C) 2020 BfaCore
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

/* ScriptData
Scripts by Loukie
SDName: Molten Front
SD%Complete: 20%
SDComment: Placeholder
SDCategory: Molten Front
EndScriptData */

/* ContentData
EndContentData */

#include "AchievementMgr.h"
#include "PortCompat.h"
#include "DB2Stores.h"
#include "ScriptedCreature.h"
#include "ScriptedEscortAI.h"
#include "Player.h"
#include "MotionMaster.h"
#include "TemporarySummon.h"
#include "GameObjectAI.h"
#include "Vehicle.h"
#include "PassiveAI.h"
#include "CombatAI.h"
#include "SpellInfo.h"
#include "CreatureTextMgr.h"
#include "Item.h"
#include "Creature.h"
#include "PhasingHandler.h"
#include "ScriptMgr.h"
#include "Containers.h"
#include "CreatureAI.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "TaskScheduler.h"
#include "SpellAuras.h"
#include "SpellMgr.h"
#include "Log.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "Spell.h"
#include "SpellAuraEffects.h"
#include "SpellScript.h"
#include "GameObject.h"
#include "ScriptedGossip.h"

enum Spells
{

};

enum eEvents
{
    // npc_pyrelord
    EVENT_INCINERATE = 1,
    EVENT_BACKDRAFT = 2,

    //npc_ancient_charhound
    EVENT_FLAME_LASH = 3,
    EVENT_LEAPING_BITE = 4,
    EVENT_SLEEPING_SLEEP = 5,

    //npc_ancient_charhound
    EVENT_INCATFORM = 6,
    EVENT_PYROBLAST = 7,
    EVENT_SCORCH = 8,

    //npc_subterranean_magma_worm
    EVENT_LAVA_SHOWER = 9,
    EVENT_BURNING_HUNGER = 10,

    EVENT_ONE = 11,
    EVENT_TWO = 12,
    EVENT_THREE = 13,
    EVENT_FOUR = 14,
    EVENT_FIVE = 15,
    EVENT_SIX = 16,
    EVENT_CHECK = 17
};

/*######
# npc_pyrelord - 52683
######*/

struct npc_pyrelord : public ScriptedAI
{
        npc_pyrelord(Creature *c) : ScriptedAI(c) {}

        uint32 spell_2_Timer;
        uint32 spell_1_Timer;

        void JustDied(Unit* /*killer*/) override
        {
            me->Respawn();
            Map::PlayerList const &PlayerList = me->GetMap()->GetPlayers();
            if (!PlayerList.empty())
            for (Map::PlayerList::const_iterator i = PlayerList.begin(); i != PlayerList.end(); ++i)
            if (i->GetSource()->IsAlive())
            if (me->GetExactDist(i->GetSource()) < 40.0f)
                i->GetSource()->TeleportTo(861, 1166.23f, 532.65f, 18.0f, 0.0f);
        }

        void JustEngagedWith(Unit* /*pWho*/) override
        {
            DoCast(79938);
        }

        void Reset() override
        {
            spell_1_Timer = 2000;
            spell_2_Timer = 6000;
        }

        void UpdateAI(uint32 diff) override
        {
            if (!UpdateVictim())
                return;

            if (spell_1_Timer <= diff)
            {
                if (!me->IsWithinMeleeRange(me->GetVictim()))
                {
                    DoCast(me->GetVictim(), 79938);
                    spell_1_Timer = 2000;
                }
            }
            else spell_1_Timer -= diff;

            if (spell_2_Timer <= diff)
            {
                if (!me->IsWithinMeleeRange(me->GetVictim()))
                {
                    DoCast(me->GetVictim(), 98839);
                    spell_2_Timer = 6000;
                }
            }
            else spell_2_Timer -= diff;
            (void)0;

        }
    
};


/*######
# npc_ancient_charhound - 54339
######*/

struct npc_ancient_charhound : public ScriptedAI
{
        npc_ancient_charhound(Creature *c) : ScriptedAI(c) {}

        uint32 spell_1_Timer;
        uint32 spell_2_Timer;
        uint32 spell_3_Timer;

        void JustEngagedWith(Unit* /*pWho*/) override
        {
            DoCast(98701);
        }

        void Reset() override
        {
            spell_1_Timer = 3000;
            spell_2_Timer = 6000;
            spell_3_Timer = 14000;
        }

        void UpdateAI(uint32 diff) override
        {
            if (!UpdateVictim())
                return;

            if (spell_1_Timer <= diff)
            {
                if (!me->IsWithinMeleeRange(me->GetVictim()))
                {
                    DoCast(me->GetVictim(), 3356);
                    spell_1_Timer = 3000;
                }
            }
            else spell_1_Timer -= diff;

            if (spell_2_Timer <= diff)
            {
                if (!me->IsWithinMeleeRange(me->GetVictim()))
                {
                    DoCast(me->GetVictim(), 98701);
                    spell_2_Timer = 6000;
                }
            }
            else spell_2_Timer -= diff;

            if (spell_3_Timer <= diff)
            {
                if (!me->IsWithinMeleeRange(me->GetVictim()))
                {
                    DoCast(me->GetVictim(), 42648);
                    spell_3_Timer = 14000;
                }
            }
            else spell_3_Timer -= diff;

            (void)0;
        }
    
};


/*######
# npc_druid_of_the_flame - 54343
######*/

struct npc_druid_of_the_flame : public ScriptedAI
{
        npc_druid_of_the_flame(Creature* c) : ScriptedAI(c)
        {
        }

        EventMap events;

        void Reset() override
        {
            events.Reset();
        }

        void JustEngagedWith(Unit* /*who*/) override
        {
            events.ScheduleEvent(EVENT_INCATFORM, 2000ms);
            events.ScheduleEvent(EVENT_PYROBLAST, 3000ms);
            events.ScheduleEvent(EVENT_SCORCH, 6000ms);
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
                switch (eventId)
                {
                case EVENT_INCATFORM:
                    DoCast(me, 97678);
                    events.ScheduleEvent(EVENT_PYROBLAST, 3000ms);
                    break;
                case EVENT_PYROBLAST:
                    if (Unit *pTarget = SelectTarget(SelectTargetMethod::Random, 0))
                        DoCast(pTarget, 17273);
                    break;
                case EVENT_SCORCH:
                    if (Unit *pTarget = SelectTarget(SelectTargetMethod::Random, 0))
                        DoCast(pTarget, 13878);
                    break;
                }
            }
            (void)0;
        }
    
};


/*######
# npc_subterranean_magma_worm - 53112
######*/

struct npc_subterranean_magma_worm : public PortCompat::Scripted_NoMovementAI
{
        npc_subterranean_magma_worm(Creature* pCreature) :
        Scripted_NoMovementAI(pCreature)
        {
        }

        EventMap events;

        void Reset() override
        {
            events.Reset();
        }

        void JustEngagedWith(Unit* /*who*/) override
        {
            events.ScheduleEvent(EVENT_LAVA_SHOWER, 15000ms);
            events.ScheduleEvent(EVENT_BURNING_HUNGER, Milliseconds(urand(9000, 17000)));
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
                switch (eventId)
                {
                case EVENT_LAVA_SHOWER:
                    if (Unit *pTarget = SelectTarget(SelectTargetMethod::Random, 0))
                        DoCast(pTarget, 97549);
                    events.ScheduleEvent(EVENT_LAVA_SHOWER, 55000ms);
                    break;
                case EVENT_BURNING_HUNGER:
                    if (Unit *pTarget = SelectTarget(SelectTargetMethod::Random, 0))
                        DoCast(pTarget, 98268);
                    events.RescheduleEvent(EVENT_BURNING_HUNGER, Milliseconds(urand(12000, 17000)));
                    break;
                }
            }
            DoSpellAttackIfReady(97306);
        }
    
};


/*######
# npc_ancient_firelord - 53864
######*/

struct npc_ancient_firelord : public ScriptedAI
{
        npc_ancient_firelord(Creature* c) : ScriptedAI(c)
        {
        }

        EventMap events;

        void Reset() override
        {
            events.Reset();
        }

        void JustEngagedWith(Unit* /*who*/) override
        {
            events.ScheduleEvent(EVENT_ONE, 4000ms);
            events.ScheduleEvent(EVENT_TWO, 6000ms);
            events.ScheduleEvent(EVENT_THREE, 9000ms);
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
                switch (eventId)
                {
                case EVENT_ONE:
                    if (Unit *pTarget = SelectTarget(SelectTargetMethod::Random, 0))
                        DoCast(pTarget, 100270);
                    events.ScheduleEvent(EVENT_ONE, 4000ms);
                    break;
                case EVENT_TWO:
                    if (Unit *pTarget = SelectTarget(SelectTargetMethod::Random, 0))
                        DoCast(pTarget, 100267);
                    events.ScheduleEvent(EVENT_TWO, 6000ms);
                    break;
                case EVENT_THREE:
                    if (Unit *pTarget = SelectTarget(SelectTargetMethod::Random, 0))
                        DoCast(pTarget, 100378);
                    events.ScheduleEvent(EVENT_THREE, 9000ms);
                    break;
                }
            }
            (void)0;
        }
    
};


/*######
# npc_firehawk - 53245,53249
######*/

struct npc_firehawk : public ScriptedAI
{
        npc_firehawk(Creature* c) : ScriptedAI(c)
        {
        }

        EventMap events;

        void Reset() override
        {
            me->SetCanFly(true);
            me->SetDisableGravity(true);
            me->SetAnimTier(AnimTier::Hover, true);
            events.Reset();
        }

        void JustEngagedWith(Unit* /*who*/) override
        {
            events.ScheduleEvent(EVENT_ONE, 4000ms);
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
                switch (eventId)
                {
                case EVENT_ONE:
                    if (Unit *pTarget = SelectTarget(SelectTargetMethod::Random, 0))
                        me->CastSpell(pTarget, 43509, true);
                    events.ScheduleEvent(EVENT_ONE, 8000ms);
                    break;
                }
            }
            (void)0;
        }
    
};


/*######
# npc_bear_cub - 40240
######*/

struct npc_bear_cub : public ScriptedAI
{
        npc_bear_cub(Creature* c) : ScriptedAI(c)
        {
        }

        EventMap events;

        void Reset() override
        {
            //me->SetCanFly(true);
            //me->SetDisableGravity(true);
            //me->SetAnimTier(AnimTier::Hover, true);
            events.Reset();
        }

        void JustEngagedWith(Unit* /*who*/) override
        {
            //events.ScheduleEvent(EVENT_ONE, 100ms);
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
                switch (eventId)
                {
                case EVENT_ONE:
                    //if (Unit *pTarget = SelectTarget(SelectTargetMethod::Random, 0))
                    //DoCast(pTarget, 43509, true);
                    //events.ScheduleEvent(EVENT_ONE, 8000ms);
                    break;
                }
            }
            (void)0;
        }
    

    bool OnGossipHello(Player* pPlayer) override
    {
        pPlayer->PlayerTalkClass->ClearMenus();
        CloseGossipMenuFor(pPlayer);
        pPlayer->KilledMonsterCredit(40240);
        pPlayer->UpdateCriteria(CriteriaType::BeSpellTarget, 101181, 0, 0, me);

        me->DespawnOrUnsummon();

        return true;
    }
};


/*######
# npc_squirrel - 52195
######*/

struct npc_squirrel : public ScriptedAI
{
        npc_squirrel(Creature* c) : ScriptedAI(c)
        {
            kick = false;
        }

        EventMap events;
        bool kick;

        void MoveInLineOfSight(Unit* who) override
        {
            Creature* creature1 = GetClosestCreatureWithEntry(me, 39939, 15.0f);
            Creature* creature2 = GetClosestCreatureWithEntry(me, 39974, 15.0f);
            Creature* creature3 = GetClosestCreatureWithEntry(me, 52816, 15.0f);
            Creature* creature4 = GetClosestCreatureWithEntry(me, 52219, 15.0f);

            if (!creature1 && !creature2 && !creature3 && !creature4)
                return;

            if (who->GetTypeId() != TYPEID_PLAYER)
            {
                if (me->GetDistance(who) > 15.0f)
                    return;
                else
                {
                    DoCast(who, 96514, true);
                    kick = true;
                }
            }
            else
            {
                if (me->GetDistance(who) > 15.0f)
                    return;
                if (kick == true)
                    who->ToPlayer()->UpdateCriteria(CriteriaType::BeSpellTarget, 100964, 0, 0, me);
            }
        }

        void Reset() override
        {
            //me->SetCanFly(true);
            //me->SetDisableGravity(true);
            //me->SetAnimTier(AnimTier::Hover, true);
            events.Reset();
        }

        void JustEngagedWith(Unit* /*who*/) override
        {
            //events.ScheduleEvent(EVENT_ONE, 100ms);
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
                switch (eventId)
                {
                case EVENT_ONE:
                    //if (Unit *pTarget = SelectTarget(SelectTargetMethod::Random, 0))
                    //DoCast(pTarget, 43509, true);
                    //events.ScheduleEvent(EVENT_ONE, 8000ms);
                    break;
                }
            }
        }
    
};


/*######
# npc_child_of_tortolla - 52177
######*/

struct npc_child_of_tortolla : public ScriptedAI
{
        npc_child_of_tortolla(Creature* c) : ScriptedAI(c)
        {
        }

        EventMap events;

        void Reset() override
        {
            //me->SetCanFly(true);
            //me->SetDisableGravity(true);
            //me->SetAnimTier(AnimTier::Hover, true);
            events.Reset();
        }

        void JustEngagedWith(Unit* /*who*/) override
        {
            //events.ScheduleEvent(EVENT_ONE, 100ms);
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
                switch (eventId)
                {
                case EVENT_ONE:
                    //if (Unit *pTarget = SelectTarget(SelectTargetMethod::Random, 0))
                    //DoCast(pTarget, 43509, true);
                    //events.ScheduleEvent(EVENT_ONE, 8000ms);
                    break;
                }
            }
            (void)0;
        }
    

    bool OnGossipHello(Player* pPlayer) override
    {
        pPlayer->PlayerTalkClass->ClearMenus();
        CloseGossipMenuFor(pPlayer);
        pPlayer->KilledMonsterCredit(52208);
        pPlayer->UpdateCriteria(CriteriaType::BeSpellTarget, 98102, 0, 0, me);

        me->DespawnOrUnsummon();

        return true;
    }
};


/*######
# npc_alpine_songbird - 52595
######*/

struct npc_alpine_songbird : public ScriptedAI
{
        npc_alpine_songbird(Creature* c) : ScriptedAI(c)
        {
        }

        EventMap events;

        void MoveInLineOfSight(Unit* who) override
        {
            if (who->GetTypeId() != TYPEID_PLAYER)
                return;

            if (me->GetDistance(who) > 10.0f)
                return;

            who->ToPlayer()->KilledMonsterCredit(52595);

            if (who->ToPlayer()->GetQuestStatus(29147) == QUEST_STATUS_INCOMPLETE)
            {
                if (Aura * aura = who->GetAura(98027))
                {
                    if (aura->GetStackAmount() < 67)
                    {
                        uint32 stack = aura->GetStackAmount() + 1;
                        me->SetAuraStack(98027, who, stack);
                    }
                    if (aura->GetStackAmount() > 64)
                        who->ToPlayer()->UpdateCriteria(CriteriaType::BeSpellTarget, 100957, 0, 0, me);
                }
                else
                    me->SetAuraStack(98027, who, 1);

                me->DespawnOrUnsummon();
            }
        }

        void Reset() override
        {
            me->SetCanFly(true);
            me->SetDisableGravity(true);
            me->SetAnimTier(AnimTier::Hover, true);
            events.Reset();
        }

        void JustEngagedWith(Unit* /*who*/) override
        {
            //events.ScheduleEvent(EVENT_ONE, 100ms);
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
                switch (eventId)
                {
                case EVENT_ONE:
                    //if (Unit *pTarget = SelectTarget(SelectTargetMethod::Random, 0))
                    //DoCast(pTarget, 43509, true);
                    //events.ScheduleEvent(EVENT_ONE, 8000ms);
                    break;
                }
            }
            (void)0;
        }
    
};


/*######
# npc_forest_owl - 52596
######*/

struct npc_forest_owl : public ScriptedAI
{
        npc_forest_owl(Creature* c) : ScriptedAI(c)
        {
        }

        EventMap events;

        void MoveInLineOfSight(Unit* who) override
        {
            if (who->GetTypeId() != TYPEID_PLAYER)
                return;

            if (me->GetDistance(who) > 10.0f)
                return;

            who->ToPlayer()->KilledMonsterCredit(52596);

            if (who->ToPlayer()->GetQuestStatus(29147) == QUEST_STATUS_INCOMPLETE)
            {
                if (Aura * aura = who->GetAura(98027))
                {
                    if (aura->GetStackAmount() < 67)
                    {
                        uint32 stack = aura->GetStackAmount() + 5;
                        me->SetAuraStack(98027, who, stack);
                    }
                    if (aura->GetStackAmount() > 64)
                        who->ToPlayer()->UpdateCriteria(CriteriaType::BeSpellTarget, 100957, 0, 0, me);
                }
                else
                    me->SetAuraStack(98027, who, 5);

                me->DespawnOrUnsummon();
            }
        }

        void Reset() override
        {
            me->SetCanFly(true);
            me->SetDisableGravity(true);
            me->SetAnimTier(AnimTier::Hover, true);
            events.Reset();
        }

        void JustEngagedWith(Unit* /*who*/) override
        {
            //events.ScheduleEvent(EVENT_ONE, 100ms);
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
                switch (eventId)
                {
                case EVENT_ONE:
                    //if (Unit *pTarget = SelectTarget(SelectTargetMethod::Random, 0))
                    //DoCast(pTarget, 43509, true);
                    //events.ScheduleEvent(EVENT_ONE, 8000ms);
                    break;
                }
            }
            (void)0;
        }
    
};


/*######
# npc_goldwing_hawk - 52594
######*/

struct npc_goldwing_hawk : public ScriptedAI
{
        npc_goldwing_hawk(Creature* c) : ScriptedAI(c)
        {
        }

        EventMap events;

        void MoveInLineOfSight(Unit* who) override
        {
            if (who->GetTypeId() != TYPEID_PLAYER)
                return;

            if (me->GetDistance(who) > 10.0f)
                return;

            who->ToPlayer()->KilledMonsterCredit(52594);

            if (who->ToPlayer()->GetQuestStatus(29147) == QUEST_STATUS_INCOMPLETE)
            {
                if (Aura * aura = who->GetAura(98027))
                {
                    if (aura->GetStackAmount() < 67)
                    {
                        uint32 stack = aura->GetStackAmount() + 15;
                        me->SetAuraStack(98027, who, stack);
                    }
                    if (aura->GetStackAmount() > 64)
                        who->ToPlayer()->UpdateCriteria(CriteriaType::BeSpellTarget, 100957, 0, 0, me);
                }
                else
                    me->SetAuraStack(98027, who, 15);

                me->DespawnOrUnsummon();
            }
        }

        void Reset() override
        {
            me->SetCanFly(true);
            me->SetDisableGravity(true);
            me->SetAnimTier(AnimTier::Hover, true);
            events.Reset();
        }

        void JustEngagedWith(Unit* /*who*/) override
        {
            //events.ScheduleEvent(EVENT_ONE, 100ms);
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
                switch (eventId)
                {
                case EVENT_ONE:
                    //if (Unit *pTarget = SelectTarget(SelectTargetMethod::Random, 0))
                    //DoCast(pTarget, 43509, true);
                    //events.ScheduleEvent(EVENT_ONE, 8000ms);
                    break;
                }
            }
            (void)0;
        }
    
};


/*######
# npc_pyrachnis - 52749
######*/

struct npc_pyrachnis : public ScriptedAI
{
        npc_pyrachnis(Creature* pCreature) : ScriptedAI(pCreature)
        {
        }

        EventMap events;

        void Reset() override
        {
            //me->SetCanFly(true);
            //me->SetDisableGravity(true);
            //me->SetAnimTier(AnimTier::Hover, true);
            events.Reset();
        }

        void JustEngagedWith(Unit* /*who*/) override
        {
            events.ScheduleEvent(EVENT_ONE, Milliseconds(urand(3000, 7000)));
            events.ScheduleEvent(EVENT_TWO, Milliseconds(urand(9000, 10000)));
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
                switch (eventId)
                {
                case EVENT_ONE:
                    if (Unit *pTarget = SelectTarget(SelectTargetMethod::Random, 0))
                        me->CastSpell(pTarget, 97515, true);
                    events.ScheduleEvent(EVENT_ONE, Milliseconds(urand(5000, 8000)));
                    break;
                case EVENT_TWO:
                    if (Unit *pTarget = SelectTarget(SelectTargetMethod::Random, 0))
                        me->CastSpell(pTarget, 97516, true);
                    events.ScheduleEvent(EVENT_TWO, Milliseconds(urand(4000, 7000)));
                    break;
                }
            }
            (void)0;
        }
    
};


/*######
# npc_tooga - 52425
######*/

struct npc_tooga : public ScriptedAI
{
        npc_tooga(Creature* pCreature) : ScriptedAI(pCreature)
        {
        }

        EventMap events;

        void Reset() override
        {
            //me->SetCanFly(true);
            //me->SetDisableGravity(true);
            //me->SetAnimTier(AnimTier::Hover, true);
            events.Reset();
        }

        void JustEngagedWith(Unit* /*who*/) override
        {
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
                switch (eventId)
                {
                case EVENT_ONE:
                    //if (Unit *pTarget = SelectTarget(SelectTargetMethod::Random, 0))
                    //    me->CastSpell(pTarget, 97515, true);
                    //events.ScheduleEvent(EVENT_ONE, Milliseconds(urand(5000,8000)));
                    break;
                }
            }
            (void)0;
        }
    

    bool OnGossipHello(Player* pPlayer) override
    {
        pPlayer->PlayerTalkClass->ClearMenus();
        CloseGossipMenuFor(pPlayer);

        if (pPlayer->GetQuestStatus(29122) == QUEST_STATUS_INCOMPLETE)
        {
            if (Creature* nemesis = pPlayer->SummonCreature(52383, 4595.44f, -1949.5f, 1180.39f, 0, TEMPSUMMON_TIMED_DESPAWN, 180000ms))
                nemesis->AI()->AttackStart(pPlayer);
        }
        return true;
    }
};


/*######
# npc_nemesis - 52383
######*/

struct npc_nemesis : public ScriptedAI
{
        npc_nemesis(Creature* c) : ScriptedAI(c)
        {
        }

        EventMap events;

        void Reset() override
        {
            //me->SetCanFly(true);
            //me->SetDisableGravity(true);
            //me->SetAnimTier(AnimTier::Hover, true);
            events.Reset();
        }

        void JustEngagedWith(Unit* /*who*/) override
        {
            events.ScheduleEvent(EVENT_ONE, Milliseconds(urand(3000, 7000)));
            //events.ScheduleEvent(EVENT_TWO, Milliseconds(urand(9000,10000)));
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
                switch (eventId)
                {
                case EVENT_ONE:
                    if (Unit *pTarget = SelectTarget(SelectTargetMethod::Random, 0))
                        me->CastSpell(pTarget, 96917, true);
                    events.ScheduleEvent(EVENT_ONE, Milliseconds(urand(15000, 18000)));
                    break;
                }
            }
            (void)0;
        }
    
};


/*######
# npc_lylagar - 52766
######*/

struct npc_lylagar : public ScriptedAI
{
        npc_lylagar(Creature* c) : ScriptedAI(c)
        {
        }

        EventMap events;

        void Reset() override
        {
            //me->SetCanFly(true);
            //me->SetDisableGravity(true);
            //me->SetAnimTier(AnimTier::Hover, true);
            events.Reset();
        }

        void JustEngagedWith(Unit* /*who*/) override
        {
            events.ScheduleEvent(EVENT_ONE, Milliseconds(urand(3000, 5000)));
            //events.ScheduleEvent(EVENT_TWO, Milliseconds(urand(9000,10000)));
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
                switch (eventId)
                {
                case EVENT_ONE:
                    if (Unit *pTarget = SelectTarget(SelectTargetMethod::Random, 0))
                        me->CastSpell(pTarget, 84867, true);
                    events.ScheduleEvent(EVENT_ONE, Milliseconds(urand(5000, 9000)));
                    break;
                }
            }
            (void)0;
        }
    
};


/*######
# npc_millagazor - 52649
######*/

struct npc_millagazor : public ScriptedAI
{
        npc_millagazor(Creature* c) : ScriptedAI(c)
        {
            phase2 = false;
            phase3 = false;
        }

        EventMap events;
        bool phase2, phase3;

        void Reset() override
        {
            //me->SetCanFly(true);
            //me->SetDisableGravity(true);
            //me->SetAnimTier(AnimTier::Hover, true);
            events.Reset();
        }

        void JustEngagedWith(Unit* /*who*/) override
        {
            events.ScheduleEvent(EVENT_ONE, Milliseconds(urand(3000, 5000)));
            //events.ScheduleEvent(EVENT_TWO, Milliseconds(urand(9000,10000)));
        }

        void UpdateAI(uint32 diff) override
        {
            if (!UpdateVictim())
                return;

            if (HealthBelowPct(10) && !phase2)
            {
                phase2 = true;
                if (Creature* creature1 = me->SummonCreature(52594, me->GetPositionX(), me->GetPositionY() + 20, me->GetPositionZ(), 0, TEMPSUMMON_TIMED_DESPAWN, 180000ms))
                    creature1->AI()->AttackStart(me);
                if (Creature* creature2 = me->SummonCreature(52596, me->GetPositionX(), me->GetPositionY() - 20, me->GetPositionZ(), 0, TEMPSUMMON_TIMED_DESPAWN, 180000ms))
                    creature2->AI()->AttackStart(me);
                if (Creature* creature3 = me->SummonCreature(52595, me->GetPositionX() + 20, me->GetPositionY(), me->GetPositionZ(), 0, TEMPSUMMON_TIMED_DESPAWN, 180000ms))
                    creature3->AI()->AttackStart(me);
                if (Creature* creature4 = me->SummonCreature(52595, me->GetPositionX() - 20, me->GetPositionY(), me->GetPositionZ(), 0, TEMPSUMMON_TIMED_DESPAWN, 180000ms))
                    creature4->AI()->AttackStart(me);
                events.ScheduleEvent(EVENT_TWO, 10000ms);
            }

            if (HealthBelowPct(5) && !phase3)
            {
                phase3 = true;
                me->DespawnOrUnsummon();
            }

            events.Update(diff);

            if (me->HasUnitState(UNIT_STATE_CASTING))
                return;

            while (uint32 eventId = events.ExecuteEvent())
            {
                switch (eventId)
                {
                case EVENT_ONE:
                    if (Unit *pTarget = SelectTarget(SelectTargetMethod::Random, 0))
                        me->CastSpell(pTarget, 11839, true);
                    events.ScheduleEvent(EVENT_ONE, Milliseconds(urand(4000, 6000)));
                    break;
                case EVENT_TWO:
                    if (Unit *pTarget = SelectTarget(SelectTargetMethod::Random, 0))
                    {
                        pTarget->ToPlayer()->UpdateCriteria(CriteriaType::KillCreature, 52649, 0, 0, me);
                        pTarget->ToPlayer()->KilledMonsterCredit(52649);
                        me->DespawnOrUnsummon();
                    }
                    break;
                }
            }
            (void)0;
        }
    
};


/*######
# npc_galenges - 52399
######*/

struct npc_galenges : public ScriptedAI
{
        npc_galenges(Creature* c) : ScriptedAI(c)
        {
        }

        EventMap events;

        void Reset() override
        {
            //me->SetCanFly(true);
            //me->SetDisableGravity(true);
            //me->SetAnimTier(AnimTier::Hover, true);
            events.Reset();
        }

        void JustEngagedWith(Unit* /*who*/) override
        {
            events.ScheduleEvent(EVENT_ONE, Milliseconds(urand(3000, 5000)));
            events.ScheduleEvent(EVENT_TWO, Milliseconds(urand(27000, 30000)));
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
                switch (eventId)
                {
                case EVENT_ONE:
                    if (Unit *pTarget = SelectTarget(SelectTargetMethod::Random, 0))
                        me->CastSpell(pTarget, 82615, true);
                    events.ScheduleEvent(EVENT_ONE, Milliseconds(urand(2000, 3000)));
                    break;
                case EVENT_TWO:
                    if (Creature* malorne = me->SummonCreature(53493, me->GetPositionX(), me->GetPositionY() + 20, me->GetPositionZ(), 0, TEMPSUMMON_TIMED_DESPAWN, 20000ms))
                    {
                        malorne->AI()->AttackStart(me);
                        me->AI()->AttackStart(malorne);
                    }
                    if (HealthBelowPct(5))
                    {
                        if (Unit *pTarget = SelectTarget(SelectTargetMethod::Random, 0))
                        {
                            pTarget->ToPlayer()->KilledMonsterCredit(52399);
                            pTarget->ToPlayer()->UpdateCriteria(CriteriaType::BeSpellTarget, 100979, 0, 0, me);
                        }
                        me->DespawnOrUnsummon(0ms);
                    }
                    events.ScheduleEvent(EVENT_THREE, Milliseconds(urand(27000, 30000)));
                    break;
                case EVENT_THREE:
                    if (Creature* malorne = me->SummonCreature(53493, me->GetPositionX(), me->GetPositionY() + 20, me->GetPositionZ(), 0, TEMPSUMMON_TIMED_DESPAWN, 20000ms))
                    {
                        malorne->AI()->AttackStart(me);
                        me->AI()->AttackStart(malorne);
                    }
                    if (HealthBelowPct(5))
                    {
                        if (Unit *pTarget = SelectTarget(SelectTargetMethod::Random, 0))
                            pTarget->ToPlayer()->KilledMonsterCredit(52399);
                        me->DespawnOrUnsummon(0ms);
                    }
                    break;
                }
            }
            (void)0;
        }
    
};


/*######
# npc_arch_druid_hamuul_runetotem - 52838
######*/

struct npc_arch_druid_hamuul_runetotem : public ScriptedAI
{
        npc_arch_druid_hamuul_runetotem(Creature* c) : ScriptedAI(c)
        {
        }

        EventMap events;

        void MoveInLineOfSight(Unit* who) override
        {
            if (who->GetTypeId() != TYPEID_PLAYER)
                return;

            if (me->GetDistance(who) > 20.0f)
                return;

            if (who->ToPlayer()->GetQuestStatus(29195) == QUEST_STATUS_INCOMPLETE)
            {
                Creature* leyara = GetClosestCreatureWithEntry(who, 52863, 50.0f);
                if (!leyara)
                if (Creature* leyara2 = who->SummonCreature(52863, 4672.59f, -2067.19f, 1224.8f, 0, TEMPSUMMON_TIMED_DESPAWN, 180000ms))
                    leyara2->AI()->AttackStart(who);
            }
        }

        void Reset() override
        {
            //me->SetCanFly(true);
            //me->SetDisableGravity(true);
            //me->SetAnimTier(AnimTier::Hover, true);
            events.Reset();
        }

        void JustEngagedWith(Unit* /*who*/) override
        {
            //events.ScheduleEvent(EVENT_ONE, 100ms);
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
                switch (eventId)
                {
                case EVENT_ONE:
                    //if (Unit *pTarget = SelectTarget(SelectTargetMethod::Random, 0))
                    //DoCast(pTarget, 43509, true);
                    //events.ScheduleEvent(EVENT_ONE, 8000ms);
                    break;
                }
            }
            (void)0;
        }
    

    bool OnGossipHello(Player* pPlayer) override
    {
        if (pPlayer->GetQuestStatus(29199))
        {
            if (pPlayer->GetCurrencyQuantity(416) >= 20)
                return false;

            return true;
        }

        return false;
    }
};


/*######
# npc_leyara - 52863
######*/

struct npc_leyara : public ScriptedAI
{
        npc_leyara(Creature* c) : ScriptedAI(c)
        {
        }

        EventMap events;

        void JustDied(Unit* /*killer*/) override
        {
            Map::PlayerList const &PlayerList = me->GetMap()->GetPlayers();
            if (!PlayerList.empty())
            for (Map::PlayerList::const_iterator i = PlayerList.begin(); i != PlayerList.end(); ++i)
            if (i->GetSource()->IsAlive())
            if (me->GetExactDist(i->GetSource()) < 50.0f)
                i->GetSource()->KilledMonsterCredit(52863);
        }

        void Reset() override
        {
            //me->SetCanFly(true);
            //me->SetDisableGravity(true);
            //me->SetAnimTier(AnimTier::Hover, true);
            events.Reset();
        }

        void JustEngagedWith(Unit* /*who*/) override
        {
            events.ScheduleEvent(EVENT_ONE, Milliseconds(urand(3000, 5000)));
            events.ScheduleEvent(EVENT_TWO, Milliseconds(urand(9000, 10000)));
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
                switch (eventId)
                {
                case EVENT_ONE:
                    if (Unit *pTarget = SelectTarget(SelectTargetMethod::Random, 0))
                        me->CastSpell(pTarget, 35377, true);
                    events.ScheduleEvent(EVENT_ONE, Milliseconds(urand(4000, 6000)));
                    break;
                case EVENT_TWO:
                    if (Unit *pTarget = SelectTarget(SelectTargetMethod::Random, 0))
                        me->CastSpell(pTarget, 97721, true);
                    events.ScheduleEvent(EVENT_TWO, Milliseconds(urand(12000, 15000)));
                    break;
                }
            }
            (void)0;
        }
    
};


/*######
# npc_elderlimb - 52906
######*/

struct npc_elderlimb : public ScriptedAI
{
        npc_elderlimb(Creature* c) : ScriptedAI(c)
        {
        }

        EventMap events;

        void MoveInLineOfSight(Unit* who) override
        {
            if (who->GetTypeId() != TYPEID_PLAYER)
                return;

            if (me->GetDistance(who) > 20.0f)
                return;

            if (who->ToPlayer()->GetQuestStatus(29197) == QUEST_STATUS_INCOMPLETE)
            {
                Creature* raging = GetClosestCreatureWithEntry(who, 52557, 20.0f);
                if (!raging)
                if (Creature* raging2 = me->SummonCreature(52557, who->GetPositionX(), who->GetPositionY(), who->GetPositionZ(), 0, TEMPSUMMON_TIMED_DESPAWN, 180000ms))
                {
                    raging2->AI()->AttackStart(who);
                    who->ToPlayer()->KilledMonsterCredit(52906);
                }
            }
        }

        void Reset() override
        {
            //me->SetCanFly(true);
            //me->SetDisableGravity(true);
            //me->SetAnimTier(AnimTier::Hover, true);
            events.Reset();
        }

        void JustEngagedWith(Unit* /*who*/) override
        {
            //events.ScheduleEvent(EVENT_ONE, 100ms);
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
                switch (eventId)
                {
                case EVENT_ONE:
                    //if (Unit *pTarget = SelectTarget(SelectTargetMethod::Random, 0))
                    //DoCast(pTarget, 43509, true);
                    //events.ScheduleEvent(EVENT_ONE, 8000ms);
                    break;
                }
            }
            (void)0;
        }
    

    bool OnGossipHello(Player* pPlayer) override
    {
        if (pPlayer->GetQuestStatus(29283) == QUEST_STATUS_COMPLETE)
        {
            if (pPlayer->GetCurrencyQuantity(416) >= 125)
                return false;

            return true;
        }
        return false;
    }
};


/*######
# npc_thisalee_crow - 52907
######*/

struct npc_thisalee_crow : public ScriptedAI
{
        npc_thisalee_crow(Creature* c) : ScriptedAI(c)
        {
        }

        EventMap events;

        void MoveInLineOfSight(Unit* who) override
        {
            if (who->GetTypeId() != TYPEID_PLAYER)
                return;

            if (me->GetDistance(who) > 20.0f)
                return;

            if (who->ToPlayer()->GetQuestStatus(29197) == QUEST_STATUS_INCOMPLETE)
            {
                Creature* raging = GetClosestCreatureWithEntry(who, 52557, 20.0f);
                if (!raging)
                if (Creature* raging2 = me->SummonCreature(52557, who->GetPositionX(), who->GetPositionY(), who->GetPositionZ(), 0, TEMPSUMMON_TIMED_DESPAWN, 180000ms))
                {
                    raging2->AI()->AttackStart(who);
                    who->ToPlayer()->KilledMonsterCredit(52907);
                }
            }
        }

        void Reset() override
        {
            //me->SetCanFly(true);
            //me->SetDisableGravity(true);
            //me->SetAnimTier(AnimTier::Hover, true);
            events.Reset();
        }

        void JustEngagedWith(Unit* /*who*/) override
        {
            //events.ScheduleEvent(EVENT_ONE, 100ms);
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
                switch (eventId)
                {
                case EVENT_ONE:
                    //if (Unit *pTarget = SelectTarget(SelectTargetMethod::Random, 0))
                    //DoCast(pTarget, 43509, true);
                    //events.ScheduleEvent(EVENT_ONE, 8000ms);
                    break;
                }
            }
            (void)0;
        }
    

    bool OnGossipHello(Player* pPlayer) override
    {
        pPlayer->ToPlayer()->UpdateCriteria(CriteriaType::BeSpellTarget, 101176, 0, 0, me);
        return false;
    }
};


/*######
# npc_tholo_whitehoof - 52903
######*/

struct npc_tholo_whitehoof : public ScriptedAI
{
        npc_tholo_whitehoof(Creature* c) : ScriptedAI(c)
        {
        }

        EventMap events;

        void MoveInLineOfSight(Unit* who) override
        {
            if (who->GetTypeId() != TYPEID_PLAYER)
                return;

            if (me->GetDistance(who) > 20.0f)
                return;

            if (who->ToPlayer()->GetQuestStatus(29197) == QUEST_STATUS_INCOMPLETE)
            {
                Creature* raging = GetClosestCreatureWithEntry(who, 52557, 20.0f);
                if (!raging)
                if (Creature* raging2 = me->SummonCreature(52557, who->GetPositionX(), who->GetPositionY(), who->GetPositionZ(), 0, TEMPSUMMON_TIMED_DESPAWN, 180000ms))
                {
                    raging2->AI()->AttackStart(who);
                    who->ToPlayer()->KilledMonsterCredit(52903);
                }
            }
        }

        void Reset() override
        {
            //me->SetCanFly(true);
            //me->SetDisableGravity(true);
            //me->SetAnimTier(AnimTier::Hover, true);
            events.Reset();
        }

        void JustEngagedWith(Unit* /*who*/) override
        {
            //events.ScheduleEvent(EVENT_ONE, 100ms);
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
                switch (eventId)
                {
                case EVENT_ONE:
                    //if (Unit *pTarget = SelectTarget(SelectTargetMethod::Random, 0))
                    //DoCast(pTarget, 43509, true);
                    //events.ScheduleEvent(EVENT_ONE, 8000ms);
                    break;
                }
            }
            (void)0;
        }
    
};


/*######
# npc_melfurion_stormrage - 52845
######*/

struct npc_melfurion_stormrage : public ScriptedAI
{
        npc_melfurion_stormrage(Creature* c) : ScriptedAI(c)
        {
        }

        EventMap events;

        void MoveInLineOfSight(Unit* who) override
        {
            if (who->GetTypeId() != TYPEID_PLAYER)
                return;

            if (me->GetDistance(who) > 10.0f)
                return;

            if (who->ToPlayer()->GetQuestStatus(29198) == QUEST_STATUS_INCOMPLETE)
            {
                Creature* raging1 = me->SummonCreature(52557, me->GetPositionX(), me->GetPositionY() + 2, who->GetPositionZ(), 0, TEMPSUMMON_TIMED_DESPAWN, 180000ms);
                Creature* raging2 = me->SummonCreature(52557, me->GetPositionX(), me->GetPositionY() - 2, who->GetPositionZ(), 0, TEMPSUMMON_TIMED_DESPAWN, 180000ms);
                Creature* raging3 = me->SummonCreature(52557, me->GetPositionX() + 2, me->GetPositionY(), who->GetPositionZ(), 0, TEMPSUMMON_TIMED_DESPAWN, 180000ms);
                Creature* raging4 = me->SummonCreature(52557, me->GetPositionX() - 2, me->GetPositionY(), who->GetPositionZ(), 0, TEMPSUMMON_TIMED_DESPAWN, 180000ms);
                raging1->AI()->AttackStart(who);
                raging2->AI()->AttackStart(who);
                raging3->AI()->AttackStart(who);
                raging4->AI()->AttackStart(who);
                who->ToPlayer()->KilledMonsterCredit(52845);
            }
        }

        void Reset() override
        {
            //me->SetCanFly(true);
            //me->SetDisableGravity(true);
            //me->SetAnimTier(AnimTier::Hover, true);
            events.Reset();
        }

        void JustEngagedWith(Unit* /*who*/) override
        {
            //events.ScheduleEvent(EVENT_ONE, 100ms);
        }

        void JustDied(Unit* /*killer*/) override
        {
        }

        void KilledUnit(Unit* /*who*/) override
        {
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
                switch (eventId)
                {
                case EVENT_ONE:
                    //if (Unit *pTarget = SelectTarget(SelectTargetMethod::Random, 0))
                    //DoCast(pTarget, 43509, true);
                    //events.ScheduleEvent(EVENT_ONE, 8000ms);
                    break;
                }
            }
            (void)0;
        }
    

    bool OnGossipHello(Player* pPlayer) override
    {
        pPlayer->ToPlayer()->UpdateCriteria(CriteriaType::BeSpellTarget, 101174, 0, 0, me);
        return false;
    }
};


/*######
# npc_leyara2 - 53014
######*/

struct npc_leyara2 : public ScriptedAI
{
        npc_leyara2(Creature* c) : ScriptedAI(c)
        {
        }

        EventMap events;

        void MoveInLineOfSight(Unit* who) override
        {
            if (who->GetTypeId() != TYPEID_PLAYER)
                return;

            if (me->GetDistance(who) > 10.0f)
                return;

            if (who->ToPlayer()->GetQuestStatus(29200) == QUEST_STATUS_INCOMPLETE)
                who->ToPlayer()->KilledMonsterCredit(53014);
            if (who->ToPlayer()->GetQuestStatus(29310) == QUEST_STATUS_INCOMPLETE)
                who->ToPlayer()->KilledMonsterCredit(53302);
        }

        void Reset() override
        {
            //me->SetCanFly(true);
            //me->SetDisableGravity(true);
            //me->SetAnimTier(AnimTier::Hover, true);
            events.Reset();
        }

        void JustEngagedWith(Unit* /*who*/) override
        {
            //events.ScheduleEvent(EVENT_ONE, 100ms);
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
                switch (eventId)
                {
                case EVENT_ONE:
                    //if (Unit *pTarget = SelectTarget(SelectTargetMethod::Random, 0))
                    //DoCast(pTarget, 43509, true);
                    //events.ScheduleEvent(EVENT_ONE, 8000ms);
                    break;
                }
            }
            (void)0;
        }
    
};


/*######
# npc_molten_behemoth - 52552
######*/

struct npc_molten_behemoth : public ScriptedAI
{
        npc_molten_behemoth(Creature* c) : ScriptedAI(c)
        {
        }

        EventMap events;

        void Reset() override
        {
            //me->SetCanFly(true);
            //me->SetDisableGravity(true);
            //me->SetAnimTier(AnimTier::Hover, true);
            events.Reset();
        }

        void JustEngagedWith(Unit* /*who*/) override
        {
            events.ScheduleEvent(EVENT_ONE, Milliseconds(urand(3000, 5000)));
            //events.ScheduleEvent(EVENT_TWO, Milliseconds(urand(9000,10000)));
        }

        void JustDied(Unit* /*killer*/) override
        {
            Map::PlayerList const &PlayerList = me->GetMap()->GetPlayers();
            if (!PlayerList.empty())
            for (Map::PlayerList::const_iterator i = PlayerList.begin(); i != PlayerList.end(); ++i)
            if (i->GetSource()->IsAlive())
            if (me->GetExactDist(i->GetSource()) < 50.0f)
            if (i->GetSource())
                i->GetSource()->ToPlayer()->UpdateCriteria(CriteriaType::BeSpellTarget, 101167, 0, 0, me);
        }

        void KilledUnit(Unit* /*who*/) override
        {
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
                switch (eventId)
                {
                case EVENT_ONE:
                    if (Unit *pTarget = SelectTarget(SelectTargetMethod::Random, 0))
                        me->CastSpell(pTarget, 97243, true);
                    events.ScheduleEvent(EVENT_ONE, Milliseconds(urand(8000, 12000)));
                    break;/*
                          case EVENT_TWO:
                          if (Unit *pTarget = SelectTarget(SelectTargetMethod::Random, 0))
                          me->CastSpell(pTarget, 97721, true);
                          events.ScheduleEvent(EVENT_ONE, Milliseconds(urand(12000,15000)));
                          break;    */
                }
            }
            (void)0;
        }
    
};


/*######
# npc_rayne_feathersong - 52467
######*/

struct npc_rayne_feathersong : public ScriptedAI
{
        npc_rayne_feathersong(Creature* c) : ScriptedAI(c)
        {
        }

        EventMap events;

        void Reset() override
        {
            //me->SetCanFly(true);
            //me->SetDisableGravity(true);
            //me->SetAnimTier(AnimTier::Hover, true);
            events.Reset();
        }

        void JustEngagedWith(Unit* /*who*/) override
        {
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
                switch (eventId)
                {
                case EVENT_ONE:

                    //if (Unit *pTarget = SelectTarget(SelectTargetMethod::Random, 0))
                    //me->CastSpell(pTarget, 97243, true);
                    //events.ScheduleEvent(EVENT_ONE, Milliseconds(urand(8000,12000)));
                    break;
                }
            }
            (void)0;
        }
    

    bool OnGossipHello(Player* pPlayer) override
    {
        Creature* summoned = GetClosestCreatureWithEntry(pPlayer, 53083, 100.0f);

        if (!summoned)
        if (pPlayer->GetQuestStatus(29143) == QUEST_STATUS_INCOMPLETE)
        {
            me->SummonCreature(53083, pPlayer->GetPositionX(), pPlayer->GetPositionY(), pPlayer->GetPositionZ(), 0, TEMPSUMMON_TIMED_DESPAWN, 180000ms);
            return true;
        }
        return false;
    }
};



/*######
# npc_cinderweb_spinner2 - 52981
######*/

struct npc_cinderweb_spinner2 : public ScriptedAI
{
        npc_cinderweb_spinner2(Creature* c) : ScriptedAI(c)
        {
        }

        EventMap events;

        void Reset() override
        {
            //me->SetCanFly(true);
            //me->SetDisableGravity(true);
            //me->SetAnimTier(AnimTier::Hover, true);
            events.Reset();
        }

        void JustEngagedWith(Unit* /*who*/) override
        {
            events.ScheduleEvent(EVENT_ONE, Milliseconds(urand(500, 1000)));
            //events.ScheduleEvent(EVENT_TWO, Milliseconds(urand(9000,10000)));
        }

        void JustDied(Unit* /*killer*/) override
        {
        }

        void KilledUnit(Unit* /*who*/) override
        {
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
                switch (eventId)
                {
                case EVENT_ONE:
                    if (Unit *pTarget = SelectTarget(SelectTargetMethod::Random, 0))
                    {
                        pTarget->ToPlayer()->TeleportTo(861, me->GetPositionX(), me->GetPositionY(), me->GetPositionZ(), 0.0f);
                        me->CastSpell(pTarget, 97959, true);
                        me->AI()->AttackStart(pTarget);
                        break;
                    }
                    events.ScheduleEvent(EVENT_ONE, Milliseconds(urand(3000, 5000)));
                    break;/*
                          case EVENT_TWO:
                          if (Unit *pTarget = SelectTarget(SelectTargetMethod::Random, 0))
                          me->CastSpell(pTarget, 97721, true);
                          events.ScheduleEvent(EVENT_ONE, Milliseconds(urand(12000,15000)));
                          break;    */
                }
            }
            (void)0;
        }
    
};


/*######
# npc_Achievement_helper - 529810
######*/

struct npc_Achievement_helper : public ScriptedAI
{
        npc_Achievement_helper(Creature* c) : ScriptedAI(c)
        {
        }

        EventMap events;

        void MoveInLineOfSight(Unit* who) override
        {
            if (who->GetTypeId() != TYPEID_PLAYER)
                return;

            if (me->GetDistance(who) > 1.0f)
                return;

            AchievementEntry const* king_of_the_spider_hill = sAchievementStore.LookupEntry(5872);
            who->ToPlayer()->CompletedAchievement(king_of_the_spider_hill);
        }

        void Reset() override
        {
            //me->SetCanFly(true);
            //me->SetDisableGravity(true);
            //me->SetAnimTier(AnimTier::Hover, true);
            events.Reset();
        }

        void JustEngagedWith(Unit* /*who*/) override
        {
            //events.ScheduleEvent(EVENT_ONE, 100ms);
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
                switch (eventId)
                {
                case EVENT_ONE:
                    //if (Unit *pTarget = SelectTarget(SelectTargetMethod::Random, 0))
                    //DoCast(pTarget, 43509, true);
                    //events.ScheduleEvent(EVENT_ONE, 8000ms);
                    break;
                }
            }
            (void)0;
        }
    
};


/*######
# npc_fireside_chat - 52921(101179) - 53259(101177) - 52824(101182) - 52134(101173) - 52986(101170) - 52669(101175) - 52845(101174 - ok) - 52907(101176 - ok)
######*/

struct npc_fireside_chat : public ScriptedAI
{
        npc_fireside_chat(Creature* c) : ScriptedAI(c)
        {
        }

        EventMap events;

        void Reset() override
        {
            //me->SetCanFly(true);
            //me->SetDisableGravity(true);
            //me->SetAnimTier(AnimTier::Hover, true);
            events.Reset();
        }

        void JustEngagedWith(Unit* /*who*/) override
        {
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
                switch (eventId)
                {
                case EVENT_ONE:

                    //if (Unit *pTarget = SelectTarget(SelectTargetMethod::Random, 0))
                    //me->CastSpell(pTarget, 97243, true);
                    //events.ScheduleEvent(EVENT_ONE, Milliseconds(urand(8000,12000)));
                    break;
                }
            }
            (void)0;
        }
    

    bool OnGossipHello(Player* pPlayer) override
    {
        if (me->GetEntry() == 52921)
            pPlayer->ToPlayer()->UpdateCriteria(CriteriaType::BeSpellTarget, 101179, 0, 0, me);
        else if (me->GetEntry() == 53259)
            pPlayer->ToPlayer()->UpdateCriteria(CriteriaType::BeSpellTarget, 101177, 0, 0, me);
        else if (me->GetEntry() == 52824)
            pPlayer->ToPlayer()->UpdateCriteria(CriteriaType::BeSpellTarget, 101182, 0, 0, me);
        else if (me->GetEntry() == 52134)
            pPlayer->ToPlayer()->UpdateCriteria(CriteriaType::BeSpellTarget, 101173, 0, 0, me);
        else if (me->GetEntry() == 52986)
            pPlayer->ToPlayer()->UpdateCriteria(CriteriaType::BeSpellTarget, 101170, 0, 0, me);
        else if (me->GetEntry() == 52669)
        {
            pPlayer->ToPlayer()->UpdateCriteria(CriteriaType::BeSpellTarget, 101175, 0, 0, me);
            if (pPlayer->GetQuestStatus(29279) == QUEST_STATUS_COMPLETE || pPlayer->GetQuestStatus(29281) == QUEST_STATUS_COMPLETE)
            {
                if (pPlayer->GetCurrencyQuantity(416) >= 125)
                    return false;

                return true;
            }
        }

        return false;
    }
};


/*######
# npc_currency_required_quest_taker - 52490(29181) - 52493(29214) - 52669(29279) - 52669(29281) - 52906(29283)
######*/

struct npc_currency_required_quest_taker : public ScriptedAI
{
        npc_currency_required_quest_taker(Creature* c) : ScriptedAI(c)
        {
        }

        EventMap events;

        void Reset() override
        {
            //me->SetCanFly(true);
            //me->SetDisableGravity(true);
            //me->SetAnimTier(AnimTier::Hover, true);
            events.Reset();
        }

        void JustEngagedWith(Unit* /*who*/) override
        {
            //events.ScheduleEvent(EVENT_ONE, 100ms);
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
                switch (eventId)
                {
                case EVENT_ONE:
                    //if (Unit *pTarget = SelectTarget(SelectTargetMethod::Random, 0))
                    //DoCast(pTarget, 43509, true);
                    //events.ScheduleEvent(EVENT_ONE, 8000ms);
                    return;
                }
            }
            (void)0;
        }
    

    bool OnGossipHello(Player* pPlayer) override
    {
        if (me->GetEntry() == 52490 && pPlayer->GetQuestStatus(29181) == QUEST_STATUS_COMPLETE)
        {
            if (pPlayer->GetCurrencyQuantity(416) >= 150)
                return false;

            return true;
        }
        else if (me->GetEntry() == 52493 && pPlayer->GetQuestStatus(29214) == QUEST_STATUS_COMPLETE)
        {
            if (pPlayer->GetCurrencyQuantity(416) >= 150)
                return false;

            return true;
        }

        return false;
    }
};


/*######
# npc_flamewaker_shaman - 53093
######*/

struct npc_flamewaker_shaman : public ScriptedAI
{
        npc_flamewaker_shaman(Creature* c) : ScriptedAI(c)
        {
        }

        EventMap events;

        void Reset() override
        {
            //me->SetCanFly(true);
            //me->SetDisableGravity(true);
            //me->SetAnimTier(AnimTier::Hover, true);
            events.Reset();
        }

        void JustEngagedWith(Unit* /*who*/) override
        {
            events.ScheduleEvent(EVENT_ONE, Milliseconds(urand(3000, 5000)));
            events.ScheduleEvent(EVENT_TWO, Milliseconds(urand(7000, 9000)));
            events.ScheduleEvent(EVENT_THREE, Milliseconds(urand(12000, 15000)));
        }

        void JustDied(Unit* /*killer*/) override
        {
        }

        void KilledUnit(Unit* /*who*/) override
        {
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
                switch (eventId)
                {
                case EVENT_ONE:
                    if (Unit *pTarget = SelectTarget(SelectTargetMethod::Random, 0))
                        me->CastSpell(pTarget, 15096, true);
                    events.ScheduleEvent(EVENT_ONE, Milliseconds(urand(15000, 18000)));
                    break;
                case EVENT_TWO:
                    if (Unit *pTarget = SelectTarget(SelectTargetMethod::Random, 0))
                        me->CastSpell(pTarget, 32062, true);
                    events.ScheduleEvent(EVENT_TWO, Milliseconds(urand(12000, 15000)));
                    break;
                case EVENT_THREE:
                    if (Unit *pTarget = SelectTarget(SelectTargetMethod::Random, 0))
                    {
                        me->CastSpell(pTarget, 98210, true);
                        if (Creature* helper = me->SummonCreature(530930, pTarget->GetPositionX(), pTarget->GetPositionY(), pTarget->GetPositionZ(), 0, TEMPSUMMON_TIMED_DESPAWN, 5000ms))
                            helper->AI()->AttackStart(me);
                    }
                    events.ScheduleEvent(EVENT_THREE, Milliseconds(urand(8000, 15000)));
                    break;
                }
            }
            (void)0;
        }
    
};


/*######
# npc_flamewaker_shaman_helper - 530930
######*/

struct npc_flamewaker_shaman_helper : public ScriptedAI
{
        npc_flamewaker_shaman_helper(Creature* c) : ScriptedAI(c)
        {
        }

        EventMap events;

        void Reset() override
        {
            //me->SetCanFly(true);
            //me->SetDisableGravity(true);
            //me->SetAnimTier(AnimTier::Hover, true);
            events.Reset();
        }

        void JustEngagedWith(Unit* /*who*/) override
        {
            events.ScheduleEvent(EVENT_ONE, 1000ms);
        }

        void JustDied(Unit* /*killer*/) override
        {
        }

        void KilledUnit(Unit* who) override
        {
            if (who->GetEntry() == 53093)
            {
                Map::PlayerList const &PlayerList = me->GetMap()->GetPlayers();
                if (!PlayerList.empty())
                for (Map::PlayerList::const_iterator i = PlayerList.begin(); i != PlayerList.end(); ++i)
                if (i->GetSource()->IsAlive())
                if (me->GetExactDist(i->GetSource()) < 100.0f)
                    i->GetSource()->UpdateCriteria(CriteriaType::BeSpellTarget, 100992, 0, 0, me);
            }
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
                switch (eventId)
                {
                case EVENT_ONE:
                    DoCast(98185);
                    events.ScheduleEvent(EVENT_ONE, 1000ms);
                    break;
                }
            }
        }
    
};



/*######
# npc_flamewaker_hunter - 53143
######*/

struct npc_flamewaker_hunter : public ScriptedAI
{
        npc_flamewaker_hunter(Creature* c) : ScriptedAI(c)
        {
        }

        EventMap events;

        void Reset() override
        {
            //me->SetCanFly(true);
            //me->SetDisableGravity(true);
            //me->SetAnimTier(AnimTier::Hover, true);
            events.Reset();
        }

        void JustEngagedWith(Unit* /*who*/) override
        {
            events.ScheduleEvent(EVENT_ONE, Milliseconds(urand(1000, 2000)));
            events.ScheduleEvent(EVENT_TWO, Milliseconds(urand(7000, 10000)));
            events.ScheduleEvent(EVENT_THREE, Milliseconds(urand(15000, 18000)));
        }

        void JustDied(Unit* /*killer*/) override
        {
        }

        void KilledUnit(Unit* /*who*/) override
        {
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
                switch (eventId)
                {
                case EVENT_ONE:
                    if (Unit *pTarget = SelectTarget(SelectTargetMethod::Random, 0))
                        me->CastSpell(pTarget, 98393, true);
                    events.ScheduleEvent(EVENT_ONE, Milliseconds(urand(2000, 3000)));
                    break;
                case EVENT_TWO:
                    if (Unit *pTarget = SelectTarget(SelectTargetMethod::Random, 0))
                        me->CastSpell(pTarget, 82707, true);
                    events.ScheduleEvent(EVENT_TWO, Milliseconds(urand(9000, 12000)));
                    break;
                case EVENT_THREE:
                    me->SetUnitFlag(UNIT_FLAG_REMOVE_CLIENT_CONTROL);
                    if (Unit *pTarget = SelectTarget(SelectTargetMethod::Random, 0))
                    {
                        if (Creature* helper = me->SummonCreature(531430, pTarget->GetPositionX(), pTarget->GetPositionY() + urand(1, 3), pTarget->GetPositionZ(), 0, TEMPSUMMON_TIMED_DESPAWN, 3000ms))
                        {
                            if (Creature* helper2 = me->SummonCreature(531431, pTarget->GetPositionX(), pTarget->GetPositionY(), pTarget->GetPositionZ() + 20, 0, TEMPSUMMON_TIMED_DESPAWN, 7000ms))
                                helper2->AI()->AttackStart(helper);

                            Map::PlayerList const &PlayerList = me->GetMap()->GetPlayers();
                            if (!PlayerList.empty())
                            for (Map::PlayerList::const_iterator i = PlayerList.begin(); i != PlayerList.end(); ++i)
                            if (i->GetSource()->IsAlive())
                            if (me->GetExactDist(i->GetSource()) < 7.0f)
                                helper->AI()->AttackStart(i->GetSource());
                        }
                    }
                    events.ScheduleEvent(EVENT_FOUR, 1500ms);
                    events.ScheduleEvent(EVENT_THREE, Milliseconds(urand(8000, 15000)));
                    break;
                case EVENT_FOUR:
                    if (Unit *pTarget = SelectTarget(SelectTargetMethod::Random, 0))
                    {
                        if (Creature* helper = me->SummonCreature(531430, pTarget->GetPositionX(), pTarget->GetPositionY() - urand(1, 3), pTarget->GetPositionZ(), 0, TEMPSUMMON_TIMED_DESPAWN, 3000ms))
                        {
                            if (Creature* helper2 = me->SummonCreature(531431, pTarget->GetPositionX(), pTarget->GetPositionY(), pTarget->GetPositionZ() + 20, 0, TEMPSUMMON_TIMED_DESPAWN, 7000ms))
                                helper2->AI()->AttackStart(helper);

                            Map::PlayerList const &PlayerList = me->GetMap()->GetPlayers();
                            if (!PlayerList.empty())
                            for (Map::PlayerList::const_iterator i = PlayerList.begin(); i != PlayerList.end(); ++i)
                            if (i->GetSource()->IsAlive())
                            if (me->GetExactDist(i->GetSource()) < 7.0f)
                                helper->AI()->AttackStart(i->GetSource());
                        }
                    }
                    events.ScheduleEvent(EVENT_FIVE, 1500ms);
                    break;
                case EVENT_FIVE:
                    if (Unit *pTarget = SelectTarget(SelectTargetMethod::Random, 0))
                    {
                        if (Creature* helper = me->SummonCreature(531430, pTarget->GetPositionX() + urand(1, 3), pTarget->GetPositionY(), pTarget->GetPositionZ(), 0, TEMPSUMMON_TIMED_DESPAWN, 3000ms))
                        {
                            if (Creature* helper2 = me->SummonCreature(531431, pTarget->GetPositionX(), pTarget->GetPositionY(), pTarget->GetPositionZ() + 20, 0, TEMPSUMMON_TIMED_DESPAWN, 7000ms))
                                helper2->AI()->AttackStart(helper);

                            Map::PlayerList const &PlayerList = me->GetMap()->GetPlayers();
                            if (!PlayerList.empty())
                            for (Map::PlayerList::const_iterator i = PlayerList.begin(); i != PlayerList.end(); ++i)
                            if (i->GetSource()->IsAlive())
                            if (me->GetExactDist(i->GetSource()) < 7.0f)
                                helper->AI()->AttackStart(i->GetSource());
                        }
                    }
                    events.ScheduleEvent(EVENT_SIX, 1500ms);
                    break;
                case EVENT_SIX:
                    me->RemoveUnitFlag(UNIT_FLAG_REMOVE_CLIENT_CONTROL);
                    if (Unit *pTarget = SelectTarget(SelectTargetMethod::Random, 0))
                    {
                        if (Creature* helper = me->SummonCreature(531430, pTarget->GetPositionX() - urand(1, 3), pTarget->GetPositionY(), pTarget->GetPositionZ(), 0, TEMPSUMMON_TIMED_DESPAWN, 3000ms))
                        {
                            if (Creature* helper2 = me->SummonCreature(531431, pTarget->GetPositionX(), pTarget->GetPositionY(), pTarget->GetPositionZ() + 20, 0, TEMPSUMMON_TIMED_DESPAWN, 7000ms))
                                helper2->AI()->AttackStart(helper);

                            Map::PlayerList const &PlayerList = me->GetMap()->GetPlayers();
                            if (!PlayerList.empty())
                            for (Map::PlayerList::const_iterator i = PlayerList.begin(); i != PlayerList.end(); ++i)
                            if (i->GetSource()->IsAlive())
                            if (me->GetExactDist(i->GetSource()) < 7.0f)
                                helper->AI()->AttackStart(i->GetSource());
                        }
                    }
                    break;
                }
            }
            (void)0;
        }
    
};


/*######
# npc_flamewaker_hunter_helper2 - 530931
######*/

struct npc_flamewaker_hunter_helper2 : public ScriptedAI
{
        npc_flamewaker_hunter_helper2(Creature* c) : ScriptedAI(c)
        {
        }

        EventMap events;

        void Reset() override
        {
            me->SetCanFly(true);
            me->SetDisableGravity(true);
            me->SetAnimTier(AnimTier::Hover, true);
            events.Reset();
        }

        void JustEngagedWith(Unit* /*who*/) override
        {
            Creature* helper1 = GetClosestCreatureWithEntry(me, 531430, 50.0f);
            me->CastSpell(helper1, 98393, true);
            events.ScheduleEvent(EVENT_ONE, 2000ms);
        }

        void JustDied(Unit* /*killer*/) override
        {
        }

        void KilledUnit(Unit* /*who*/) override
        {
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
                switch (eventId)
                {
                case EVENT_ONE:
                    me->DespawnOrUnsummon();
                    break;
                }
            }
        }
    
};


/*######
# npc_flamewaker_hunter_helper - 530930
######*/

struct npc_flamewaker_hunter_helper : public ScriptedAI
{
        npc_flamewaker_hunter_helper(Creature* c) : ScriptedAI(c)
        {
        }

        EventMap events;

        void Reset() override
        {
            //me->SetCanFly(true);
            //me->SetDisableGravity(true);
            //me->SetAnimTier(AnimTier::Hover, true);
            events.Reset();
        }

        void JustEngagedWith(Unit* /*who*/) override
        {
            events.ScheduleEvent(EVENT_ONE, 2000ms);
        }

        void JustDied(Unit* /*killer*/) override
        {
        }

        void KilledUnit(Unit* /*who*/) override
        {
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
                switch (eventId)
                {
                case EVENT_ONE:
                {
                                  me->CastSpell(me, 98371, true);
                                  Map::PlayerList const &PlayerList = me->GetMap()->GetPlayers();
                                  if (!PlayerList.empty())
                                  for (Map::PlayerList::const_iterator i = PlayerList.begin(); i != PlayerList.end(); ++i)
                                  if (i->GetSource()->IsAlive())
                                  {
                                      if (me->GetExactDist(i->GetSource()) > 10.0f)
                                          i->GetSource()->UpdateCriteria(CriteriaType::BeSpellTarget, 100996, 0, 0, me);
                                  }
                                  events.ScheduleEvent(EVENT_TWO, 1000ms);
                }
                    break;
                case EVENT_TWO:
                    me->DespawnOrUnsummon();
                    break;
                }
            }
        }
    
};


/*######
# npc_flamewaker_sentinel - 53085
######*/

struct npc_flamewaker_sentinel : public ScriptedAI
{
        npc_flamewaker_sentinel(Creature* c) : ScriptedAI(c)
        {
        }

        EventMap events;

        void Reset() override
        {
            //me->SetCanFly(true);
            //me->SetDisableGravity(true);
            //me->SetAnimTier(AnimTier::Hover, true);
            events.Reset();
        }

        void JustEngagedWith(Unit* /*who*/) override
        {
            if (Unit *pTarget = SelectTarget(SelectTargetMethod::Random, 0))
                me->CastSpell(pTarget, 32323, true);
            events.ScheduleEvent(EVENT_ONE, Milliseconds(urand(3000, 5000)));
            events.ScheduleEvent(EVENT_TWO, Milliseconds(urand(22000, 29000)));
        }

        void JustDied(Unit* killer) override
        {
            if (me->HasAura(98169))
                killer->ToPlayer()->UpdateCriteria(CriteriaType::KillCreature, 53085, 0, 0, me);
        }

        void KilledUnit(Unit* /*who*/) override
        {
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
                switch (eventId)
                {
                case EVENT_ONE:
                    if (Unit *pTarget = SelectTarget(SelectTargetMethod::Random, 0))
                        me->CastSpell(pTarget, 79881, true);
                    events.ScheduleEvent(EVENT_ONE, Milliseconds(urand(4000, 7000)));
                    break;
                case EVENT_TWO:
                    if (Unit *pTarget = SelectTarget(SelectTargetMethod::Random, 0))
                        me->CastSpell(pTarget, 98169, true);
                    break;
                }
            }
            (void)0;
        }
    
};


/*######
# npc_emberspit_scorpion - 53240
######*/

struct npc_emberspit_scorpion : public ScriptedAI
{
        npc_emberspit_scorpion(Creature* c) : ScriptedAI(c)
        {
        }

        EventMap events;

        void Reset() override
        {
            //me->SetCanFly(true);
            //me->SetDisableGravity(true);
            //me->SetAnimTier(AnimTier::Hover, true);
            events.Reset();
        }

        void JustEngagedWith(Unit* /*who*/) override
        {
            if (Unit *pTarget = SelectTarget(SelectTargetMethod::Random, 0))
                me->CastSpell(pTarget, 53148, true);
            events.ScheduleEvent(EVENT_ONE, Milliseconds(urand(3000, 5000)));
        }

        void JustDied(Unit* /*killer*/) override
        {
            //if (killer)
            //killer->ToPlayer()->KilledMonsterCredit(53263);
            me->SummonCreature(53387, me->GetPositionX(), me->GetPositionY() + urand(4, 7), me->GetPositionZ(), 0, TEMPSUMMON_TIMED_DESPAWN, 5000ms);
            me->SummonCreature(53013, me->GetPositionX(), me->GetPositionY() + urand(4, 7), me->GetPositionZ(), 0, TEMPSUMMON_TIMED_DESPAWN, 5000ms);
        }

        void KilledUnit(Unit* /*who*/) override
        {
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
                switch (eventId)
                {
                case EVENT_ONE:
                    if (Unit *pTarget = SelectTarget(SelectTargetMethod::Random, 0))
                        me->CastSpell(pTarget, 11985, true);
                    events.ScheduleEvent(EVENT_ONE, Milliseconds(urand(9000, 12000)));
                    break;
                }
            }
            (void)0;
        }
    
};


/*######
# npc_trained_firehawk - 53297
######*/

struct npc_trained_firehawk : public ScriptedAI
{
        npc_trained_firehawk(Creature* c) : ScriptedAI(c)
        {
        }

        EventMap events;

        void Reset() override
        {
            //me->SetCanFly(true);
            //me->SetDisableGravity(true);
            //me->SetAnimTier(AnimTier::Hover, true);
        }

        void JustEngagedWith(Unit* /*who*/) override
        {
            //events.ScheduleEvent(EVENT_ONE, 100ms);
        }

        void UpdateAI(uint32 diff) override
        {
            events.Update(diff);

            while (uint32 eventId = events.ExecuteEvent())
            {
                switch (eventId)
                {
                case EVENT_ONE:
                    break;
                }
            }
        }
    

    bool OnGossipHello(Player* pPlayer) override
    {
        pPlayer->PlayerTalkClass->ClearMenus();
        CloseGossipMenuFor(pPlayer);

        if (pPlayer->GetQuestStatus(29290) == QUEST_STATUS_INCOMPLETE)
        {
            pPlayer->TeleportTo(861, 365.84f, -353.13f, 219.64f, 0.0f);
            Creature* windsong = GetClosestCreatureWithEntry(me, 53259, 20.0f);
            if (!windsong)
            {
                pPlayer->EnterVehicle(me, 0);
                me->GetMotionMaster()->MovePoint(0, 514.87f, -467.04f, 188.76f);
            }

        }

        return true;
    }
};


/*######
# npc_firehawk_helper - 532971-9
######*/

struct npc_firehawk_helper : public ScriptedAI
{
        npc_firehawk_helper(Creature* c) : ScriptedAI(c)
        {
            kick = false;
        }

        EventMap events;
        bool kick;

        void MoveInLineOfSight(Unit* who) override
        {
            if (me->GetDistance(who) > 10.0f)
                return;

            if (me->GetEntry() == 532971)
                who->GetMotionMaster()->MovePoint(0, 560.34f, -604.76f, 204.38f);
            if (me->GetEntry() == 532972)
                who->GetMotionMaster()->MovePoint(0, 892.18f, -455.54f, 237.11f);
            if (me->GetEntry() == 532973)
                who->GetMotionMaster()->MovePoint(0, 878.91f, -371.4f, 221.36f);
            if (me->GetEntry() == 532974)
                who->GetMotionMaster()->MovePoint(0, 826.62f, -379.41f, 233.25f);
            if (me->GetEntry() == 532975)
                who->GetMotionMaster()->MovePoint(0, 804.6f, -191.4f, 254.07f);
            if (me->GetEntry() == 532976)
                who->GetMotionMaster()->MovePoint(0, 773.41f, -325.75f, 212.82f);
            if (me->GetEntry() == 532977)
                who->GetMotionMaster()->MovePoint(0, 669.86f, -384.36f, 191.38f);
            if (me->GetEntry() == 532978)
                who->GetMotionMaster()->MovePoint(0, 598.43f, -519.02f, 192.0f);
            if (me->GetEntry() == 532979)
                who->GetMotionMaster()->MovePoint(0, 509.3f, -443.67f, 165.64f);
            if (me->GetEntry() == 532980)
            if (who->GetTypeId() == TYPEID_PLAYER)
                who->ToPlayer()->TeleportTo(861, 1244.75f, 530.091f, 59.8688f, 0.0f);
        }

        void Reset() override
        {
            me->SetCanFly(true);
            me->SetDisableGravity(true);
            me->SetAnimTier(AnimTier::Hover, true);
            events.Reset();
        }

        void JustEngagedWith(Unit* /*who*/) override
        {
            //events.ScheduleEvent(EVENT_ONE, 100ms);
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
                switch (eventId)
                {
                case EVENT_ONE:
                    //if (Unit *pTarget = SelectTarget(SelectTargetMethod::Random, 0))
                    //DoCast(pTarget, 43509, true);
                    //events.ScheduleEvent(EVENT_ONE, 8000ms);
                    break;
                }
            }
        }
    
};


/*######
# death_from_above - 53240
######*/

struct death_from_above : public ScriptedAI
{
        death_from_above(Creature* c) : ScriptedAI(c)
        {
        }

        EventMap events;

        void Reset() override
        {
            //me->SetCanFly(true);
            //me->SetDisableGravity(true);
            //me->SetAnimTier(AnimTier::Hover, true);
            events.Reset();
        }

        void JustEngagedWith(Unit* /*who*/) override
        {
        }

        void JustDied(Unit* /*killer*/) override
        {
            Map::PlayerList const &PlayerList = me->GetMap()->GetPlayers();
            if (!PlayerList.empty())
            for (Map::PlayerList::const_iterator i = PlayerList.begin(); i != PlayerList.end(); ++i)
            if (i->GetSource()->IsAlive())
            if (me->GetExactDist(i->GetSource()) < 120.0f)
            {
                if (me->GetEntry() == 54252)
                    i->GetSource()->ToPlayer()->UpdateCriteria(CriteriaType::BeSpellTarget, 101098, 0, 0, me);
                if (me->GetEntry() == 54253)
                    i->GetSource()->ToPlayer()->UpdateCriteria(CriteriaType::BeSpellTarget, 101097, 0, 0, me);
                if (me->GetEntry() == 54254)
                    i->GetSource()->ToPlayer()->UpdateCriteria(CriteriaType::BeSpellTarget, 101099, 0, 0, me);
                if (me->GetEntry() == 54255)
                    i->GetSource()->ToPlayer()->UpdateCriteria(CriteriaType::BeSpellTarget, 101100, 0, 0, me);
                if (me->GetEntry() == 54256)
                    i->GetSource()->ToPlayer()->UpdateCriteria(CriteriaType::BeSpellTarget, 101101, 0, 0, me);
                if (me->GetEntry() == 54257)
                    i->GetSource()->ToPlayer()->UpdateCriteria(CriteriaType::BeSpellTarget, 101103, 0, 0, me);
            }
        }

        void KilledUnit(Unit* /*who*/) override
        {
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
                switch (eventId)
                {
                case EVENT_ONE:
                    //if (Unit *pTarget = SelectTarget(SelectTargetMethod::Random, 0))
                    //me->CastSpell(pTarget, 11985, true);
                    //events.ScheduleEvent(EVENT_ONE, Milliseconds(urand(9000,12000)));
                    break;
                }
            }
            (void)0;
        }
    
};


struct go_ungoro_soil : public GameObjectAI
{
    go_ungoro_soil(GameObject* go) : GameObjectAI(go) { }


    bool OnGossipHello(Player* player) override
    {
        if (player->GetQuestStatus(29249) == QUEST_STATUS_INCOMPLETE)
        {
            player->KilledMonsterCredit(53084);
            me->SummonCreature(53013, me->GetPositionX(), me->GetPositionY(), me->GetPositionZ(), 0, TEMPSUMMON_TIMED_DESPAWN, 180000ms);
            CloseGossipMenuFor(player);
            return false;
        }
        return true;
    }
};


/*######
# npc_little_lasher - 53013
######*/

struct npc_little_lasher : public ScriptedAI
{
        npc_little_lasher(Creature* c) : ScriptedAI(c)
        {
        }

        EventMap events;

        void Reset() override
        {
            //me->SetCanFly(true);
            //me->SetDisableGravity(true);
            //me->SetAnimTier(AnimTier::Hover, true);
        }

        void JustEngagedWith(Unit* /*who*/) override
        {
            //events.ScheduleEvent(EVENT_ONE, 100ms);
        }

        void UpdateAI(uint32 diff) override
        {
            events.Update(diff);

            while (uint32 eventId = events.ExecuteEvent())
            {
                switch (eventId)
                {
                case EVENT_ONE:
                    break;
                }
            }
        }
    

    bool OnGossipHello(Player* pPlayer) override
    {
        pPlayer->PlayerTalkClass->ClearMenus();
        CloseGossipMenuFor(pPlayer);

        if (pPlayer->GetQuestStatus(29254) == QUEST_STATUS_INCOMPLETE)
        {
            pPlayer->KilledMonsterCredit(53079);
            Creature* avrilla = GetClosestCreatureWithEntry(me, 52489, 200.0f);
            if (avrilla)
                me->GetMotionMaster()->MovePoint(0, avrilla->GetPositionX(), avrilla->GetPositionY(), avrilla->GetPositionZ());

        }

        return true;
    }
};


/*######
# npc_hyjal_druid - 525010
######*/

struct npc_hyjal_druid : public ScriptedAI
{
        npc_hyjal_druid(Creature* c) : ScriptedAI(c)
        {
        }

        EventMap events;

        void MoveInLineOfSight(Unit* who) override
        {
            if (me->GetDistance(who) > 20.0f)
                return;

            if (who->GetTypeId() != TYPEID_PLAYER)
                return;

            Creature* flamewatch = GetClosestCreatureWithEntry(me, 52399, 20.0f);
            Creature* flamewatch2 = GetClosestCreatureWithEntry(me, 52663, 20.0f);
            Creature* pyrelord = GetClosestCreatureWithEntry(me, 52998, 20.0f);
            if (flamewatch)
                me->AI()->AttackStart(flamewatch);
            else if (flamewatch2)
                me->AI()->AttackStart(flamewatch2);
            else if (pyrelord)
                me->AI()->AttackStart(pyrelord);
            else if (who->ToPlayer()->GetQuestStatus(29205) == QUEST_STATUS_INCOMPLETE && !pyrelord && !flamewatch)
                who->ToPlayer()->KilledMonsterCredit(53012);
        }

        void Reset() override
        {
            //me->SetCanFly(true);
            //me->SetDisableGravity(true);
            //me->SetAnimTier(AnimTier::Hover, true);
        }

        void JustEngagedWith(Unit* /*who*/) override
        {
            //events.ScheduleEvent(EVENT_ONE, 100ms);
        }

        void UpdateAI(uint32 diff) override
        {
            events.Update(diff);

            while (uint32 eventId = events.ExecuteEvent())
            {
                switch (eventId)
                {
                case EVENT_ONE:
                    break;
                }
            }
            (void)0;
        }
    
};


/*######
# npc_flame_protection_rune - 52884,52885,52886,52887,52888,52889,52890,53887
######*/

struct npc_flame_protection_rune : public ScriptedAI
{
        npc_flame_protection_rune(Creature* c) : ScriptedAI(c)
        {
        }

        EventMap events;

        void Reset() override
        {
            //me->SetCanFly(true);
            //me->SetDisableGravity(true);
            //me->SetAnimTier(AnimTier::Hover, true);
        }

        void JustEngagedWith(Unit* /*who*/) override
        {
            //events.ScheduleEvent(EVENT_ONE, 100ms);
        }

        void UpdateAI(uint32 diff) override
        {
            events.Update(diff);

            while (uint32 eventId = events.ExecuteEvent())
            {
                switch (eventId)
                {
                case EVENT_ONE:
                    break;
                }
            }
        }
    

    bool OnGossipHello(Player* pPlayer) override
    {
        pPlayer->PlayerTalkClass->ClearMenus();
        CloseGossipMenuFor(pPlayer);

        if (pPlayer->GetQuestStatus(29210) == QUEST_STATUS_INCOMPLETE)
        {
            me->DespawnOrUnsummon();

            return false;
        }

        return true;
    }
};


/*######
# npc_flame_protection_rune1 - 528840
######*/

struct npc_flame_protection_rune1 : public ScriptedAI
{
        npc_flame_protection_rune1(Creature* c) : ScriptedAI(c)
        {
        }

        EventMap events;

        void MoveInLineOfSight(Unit* who) override
        {
            if (me->GetDistance(who) > 100.0f)
                return;

            if (who->GetTypeId() != TYPEID_PLAYER)
                return;

            Creature* rune1 = GetClosestCreatureWithEntry(me, 52884, 200.0f);
            Creature* rune2 = GetClosestCreatureWithEntry(me, 52885, 200.0f);
            Creature* rune3 = GetClosestCreatureWithEntry(me, 52886, 200.0f);
            Creature* rune4 = GetClosestCreatureWithEntry(me, 52887, 200.0f);
            Creature* rune5 = GetClosestCreatureWithEntry(me, 52888, 200.0f);
            Creature* rune6 = GetClosestCreatureWithEntry(me, 52889, 200.0f);
            Creature* rune7 = GetClosestCreatureWithEntry(me, 52890, 200.0f);
            Creature* rune8 = GetClosestCreatureWithEntry(me, 53887, 200.0f);
            if (!rune1 && !rune2 && !rune3 && !rune4 && !rune5 && !rune6 && !rune7 && !rune8)
                who->ToPlayer()->KilledMonsterCredit(52891);
        }

        void Reset() override
        {
            //me->SetCanFly(true);
            //me->SetDisableGravity(true);
            //me->SetAnimTier(AnimTier::Hover, true);
        }

        void JustEngagedWith(Unit* /*who*/) override
        {
            //events.ScheduleEvent(EVENT_ONE, 100ms);
        }

        void UpdateAI(uint32 diff) override
        {
            events.Update(diff);

            while (uint32 eventId = events.ExecuteEvent())
            {
                switch (eventId)
                {
                case EVENT_ONE:
                    break;
                }
            }
        }
    
};


struct go_solar_core : public GameObjectAI
{
    go_solar_core(GameObject* go) : GameObjectAI(go) { }


    bool OnGossipHello(Player* player) override
    {
        if (player->GetQuestStatus(29211) == QUEST_STATUS_INCOMPLETE)
        {
            player->KilledMonsterCredit(52950);
            me->RemoveFromWorld();
            CloseGossipMenuFor(player);
            return false;
        }
        return true;
    }
};


/*######
# npc_druid_of_the_flame_molten_front - 52871,52872
######*/

struct npc_druid_of_the_flame_molten_front : public ScriptedAI
{
        npc_druid_of_the_flame_molten_front(Creature* c) : ScriptedAI(c)
        {
            phase = false;
        }

        EventMap events;
        bool phase;

        void Reset() override
        {
            //me->SetCanFly(true);
            //me->SetDisableGravity(true);
            //me->SetAnimTier(AnimTier::Hover, true);
        }

        void JustEngagedWith(Unit* /*who*/) override
        {
            events.ScheduleEvent(EVENT_ONE, Milliseconds(urand(2000, 5000)));
            events.ScheduleEvent(EVENT_TWO, Milliseconds(urand(7000, 10000)));
        }


        void SpellHit(WorldObject* /*caster*/, SpellInfo const* spell) override
        {
            if (spell->Id == 98628)
            {
                Map::PlayerList const &PlayerList = me->GetMap()->GetPlayers();
                if (!PlayerList.empty())
                for (Map::PlayerList::const_iterator i = PlayerList.begin(); i != PlayerList.end(); ++i)
                if (i->GetSource()->IsAlive())
                if (me->GetExactDist(i->GetSource()) < 50.0f)
                if (i->GetSource()->GetQuestStatus(29297) == QUEST_STATUS_INCOMPLETE)
                if (me->HasAura(98827))
                {
                    i->GetSource()->KilledMonsterCredit(53251);
                    me->DespawnOrUnsummon();
                }
            }
        }

        void UpdateAI(uint32 diff) override
        {
            if (!UpdateVictim())
                return;

            if (me->HasUnitState(UNIT_STATE_CASTING))
                return;

            events.Update(diff);

            if (HealthBelowPct(50) && !phase)
            {
                phase = true;
                Map::PlayerList const &PlayerList = me->GetMap()->GetPlayers();
                if (!PlayerList.empty())
                for (Map::PlayerList::const_iterator i = PlayerList.begin(); i != PlayerList.end(); ++i)
                if (i->GetSource()->IsAlive())
                if (me->GetExactDist(i->GetSource()) < 50.0f)
                if (i->GetSource()->GetQuestStatus(29192) == QUEST_STATUS_INCOMPLETE)
                if (Creature* shadowwarden = me->SummonCreature(52804, me->GetPositionX(), me->GetPositionY() + 10, me->GetPositionZ(), 0, TEMPSUMMON_TIMED_DESPAWN, 5000ms))
                {
                    i->GetSource()->KilledMonsterCredit(52815);
                    shadowwarden->AI()->AttackStart(me);
                    me->CastSpell(me, 97565, true);
                }
            }

            while (uint32 eventId = events.ExecuteEvent())
            {
                switch (eventId)
                {
                case EVENT_ONE:
                    if (Unit *pTarget = SelectTarget(SelectTargetMethod::Random, 0))
                        me->CastSpell(pTarget, 17273, true);
                    events.ScheduleEvent(EVENT_ONE, Milliseconds(urand(15000, 18000)));
                    break;
                case EVENT_TWO:
                    if (Unit *pTarget = SelectTarget(SelectTargetMethod::Random, 0))
                        me->CastSpell(pTarget, 13878, true);
                    events.ScheduleEvent(EVENT_TWO, Milliseconds(urand(10000, 12000)));
                    events.ScheduleEvent(EVENT_THREE, Milliseconds(urand(5000, 10000)));
                    break;
                case EVENT_THREE:
                    if (Unit *pTarget = SelectTarget(SelectTargetMethod::Random, 0))
                    if (pTarget)
                    if (pTarget->ToPlayer()->GetQuestStatus(29297) == QUEST_STATUS_INCOMPLETE)
                    {
                        me->CastSpell(me, 98827, true);
                        me->SetCanFly(true);
                        me->SetDisableGravity(true);
                        me->SetAnimTier(AnimTier::Hover, true);
                        me->SetDisplayId(38814);
                        me->GetMotionMaster()->MovePoint(0, pTarget->GetPositionX() + 10, pTarget->GetPositionY() + 10, pTarget->GetPositionZ() + 10);
                    }
                    break;
                }
            }
            (void)0;
        }
    
};


/*######
# npc_cinderweb_cocoon - 52751,52783,52784,52989,52991,52992
######*/

struct npc_cinderweb_cocoon : public ScriptedAI
{
        npc_cinderweb_cocoon(Creature* c) : ScriptedAI(c)
        {
        }

        EventMap events;

        void Reset() override
        {
            //me->SetCanFly(true);
            //me->SetDisableGravity(true);
            //me->SetAnimTier(AnimTier::Hover, true);
        }

        void JustEngagedWith(Unit* /*who*/) override
        {
            //events.ScheduleEvent(EVENT_ONE, 100ms);
        }

        void UpdateAI(uint32 diff) override
        {
            events.Update(diff);

            while (uint32 eventId = events.ExecuteEvent())
            {
                switch (eventId)
                {
                case EVENT_ONE:
                    break;
                }
            }
        }
    

    bool OnGossipHello(Player* pPlayer) override
    {
        pPlayer->PlayerTalkClass->ClearMenus();
        CloseGossipMenuFor(pPlayer);

        if (pPlayer->GetQuestStatus(29189) == QUEST_STATUS_INCOMPLETE)
        {
            pPlayer->KilledMonsterCredit(52751);
            me->DespawnOrUnsummon();
            return false;
        }

        return true;
    }
};


/*######
# npc_molten_front_vendor - 53882,53214,53881
######*/

struct npc_molten_front_vendor : public ScriptedAI
{
        npc_molten_front_vendor(Creature* c) : ScriptedAI(c)
        {
        }

        EventMap events;

        void Reset() override
        {
            //me->SetCanFly(true);
            //me->SetDisableGravity(true);
            //me->SetAnimTier(AnimTier::Hover, true);
            events.Reset();
        }

        void JustEngagedWith(Unit* /*who*/) override
        {
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
                switch (eventId)
                {
                case EVENT_ONE:

                    //if (Unit *pTarget = SelectTarget(SelectTargetMethod::Random, 0))
                    //me->CastSpell(pTarget, 97243, true);
                    //events.ScheduleEvent(EVENT_ONE, Milliseconds(urand(8000,12000)));
                    break;
                }
            }
            (void)0;
        }
    

    bool OnGossipHello(Player* pPlayer) override
    {
        if (me->GetEntry() == 53882)
        {
            if (pPlayer->GetQuestStatus(29284))
                return false;
            else
                return true;
        }
        if (me->GetEntry() == 53214)
        {
            if (pPlayer->GetQuestStatus(29282))
                return false;
            else
                return true;
        }
        if (me->GetEntry() == 53881)
        {
            if (pPlayer->GetQuestStatus(29280))
                return false;
            else
                return true;
        }
        return true;
    }
};


/*######
# npc_fireland_teleporter - 100005
######*/

struct npc_fireland_teleporter : public ScriptedAI
{
        npc_fireland_teleporter(Creature* c) : ScriptedAI(c)
        {
        }

        void MoveInLineOfSight(Unit* who) override
        {
            if (me->GetDistance(who) > 15.0f)
                return;

            if (who->GetTypeId() != TYPEID_PLAYER)
                return;

            if (who->ToPlayer()->GetQuestStatus(29201) == QUEST_STATUS_COMPLETE)
                who->ToPlayer()->TeleportTo(720, -551.815f, 320.05f, 115.489f, 5.94812f);
        }
    
};



void AddSC_molten_front()
{
    RegisterCreatureAI(npc_pyrelord);
    RegisterCreatureAI(npc_ancient_charhound);
    RegisterCreatureAI(npc_druid_of_the_flame);
    RegisterCreatureAI(npc_subterranean_magma_worm);
    RegisterCreatureAI(npc_ancient_firelord);
    RegisterCreatureAI(npc_firehawk);
    RegisterCreatureAI(npc_bear_cub);
    RegisterCreatureAI(npc_squirrel);
    RegisterCreatureAI(npc_child_of_tortolla);
    RegisterCreatureAI(npc_alpine_songbird);
    RegisterCreatureAI(npc_forest_owl);
    RegisterCreatureAI(npc_goldwing_hawk);
    RegisterCreatureAI(npc_pyrachnis);
    RegisterCreatureAI(npc_tooga);
    RegisterCreatureAI(npc_nemesis);
    RegisterCreatureAI(npc_lylagar);
    RegisterCreatureAI(npc_millagazor);
    RegisterCreatureAI(npc_galenges);
    RegisterCreatureAI(npc_arch_druid_hamuul_runetotem);
    RegisterCreatureAI(npc_leyara);
    RegisterCreatureAI(npc_elderlimb);
    RegisterCreatureAI(npc_thisalee_crow);
    RegisterCreatureAI(npc_tholo_whitehoof);
    RegisterCreatureAI(npc_melfurion_stormrage);
    RegisterCreatureAI(npc_leyara2);
    RegisterCreatureAI(npc_molten_behemoth);
    RegisterCreatureAI(npc_rayne_feathersong);
    RegisterCreatureAI(npc_cinderweb_spinner2);
    RegisterCreatureAI(npc_Achievement_helper);
    RegisterCreatureAI(npc_fireside_chat);
    RegisterCreatureAI(npc_currency_required_quest_taker);
    RegisterCreatureAI(npc_flamewaker_shaman);
    RegisterCreatureAI(npc_flamewaker_shaman_helper);
    RegisterCreatureAI(npc_flamewaker_hunter);
    RegisterCreatureAI(npc_flamewaker_hunter_helper2);
    RegisterCreatureAI(npc_flamewaker_hunter_helper);
    RegisterCreatureAI(npc_flamewaker_sentinel);
    RegisterCreatureAI(npc_emberspit_scorpion);
    RegisterCreatureAI(npc_trained_firehawk);
    RegisterCreatureAI(npc_firehawk_helper);
    RegisterGameObjectAI(go_ungoro_soil);
    RegisterCreatureAI(npc_little_lasher);
    RegisterCreatureAI(npc_hyjal_druid);
    RegisterCreatureAI(npc_flame_protection_rune);
    RegisterCreatureAI(npc_flame_protection_rune1);
    RegisterGameObjectAI(go_solar_core);
    RegisterCreatureAI(npc_druid_of_the_flame_molten_front);
    RegisterCreatureAI(npc_cinderweb_cocoon);
    RegisterCreatureAI(npc_molten_front_vendor);
    RegisterCreatureAI(npc_fireland_teleporter);
}
