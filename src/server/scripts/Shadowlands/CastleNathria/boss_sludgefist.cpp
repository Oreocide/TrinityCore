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
 * Colossal Roar, Chain Link, two Destructive Stomps, a second Roar, then Hateful Gaze marks a player, Sludgefist charges at it 6 s
 * later and the Impact stuns him for 12 s, after which every timer restarts from the same offsets. Gruesome Rage at 30% health.
 * Offsets are Warcraft Logs medians (30 Normal kills; cycle 2 matches cycle 1 within 0.3 s) and agree with BigWigs and a retail
 * capture (trinitycore-tracking/castle-nathria/bosses/sludgefist.md).
 *
 * Not done: a charge into a pillar (pillar collapse, Crumbling Foundation) is not detected, every Impact counts as a wall hit;
 * Falling Rubble, the Stonequake and rubble missiles of the Stomp, Giant Fists and the Heroic Chain Slam are not cast; the Chain Link
 * break is checked on the link's 2 s damage tick.
 */

#include "Containers.h"
#include "InstanceScript.h"
#include "Map.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "SpellAuraEffects.h"
#include "SpellScript.h"
#include "castle_nathria.h"

enum SludgefistSpells
{
    SPELL_RAGE_GAIN                 = 331542,   // +2 energy per second from each anchor until the Gaze
    SPELL_SEISMIC_SHIFT             = 342437,
    SPELL_COLOSSAL_ROAR             = 332687,
    SPELL_COLOSSAL_ROAR_DAMAGE      = 332698,
    SPELL_CHAIN_LINK                = 335300,
    SPELL_CHAIN_THEM                = 342420,   // 6 s mark on both players of a pair
    SPELL_CHAIN_LINK_AURA           = 335293,   // the link itself, 2 s damage tick
    SPELL_SHATTERING_CHAIN          = 335295,   // the link broke: damage + stun
    SPELL_DESTRUCTIVE_STOMP_WARNING = 332358,
    SPELL_DESTRUCTIVE_STOMP         = 332318,
    SPELL_HATEFUL_GAZE              = 339057,
    SPELL_HATEFUL_GAZE_MARK         = 331209,
    SPELL_HEEDLESS_CHARGE           = 331212,
    SPELL_HEEDLESS_CHARGE_HIT       = 339067,
    SPELL_DESTRUCTIVE_IMPACT        = 332969,
    SPELL_DESTRUCTIVE_IMPACT_STUN   = 331314,
    SPELL_COLLAPSING_FOUNDATION     = 332197,
    SPELL_GRUESOME_RAGE             = 341250
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
    EVENT_STUN_END
};

enum SludgefistTexts
{
    SAY_AGGRO                       = 0,
    SAY_STOMP                       = 1,    // a random line on each Destructive Stomp cast start
    SAY_HATEFUL_GAZE                = 2,
    SAY_STUN_END                    = 3,
    SAY_GRUESOME_RAGE               = 4,
    SAY_DEATH                       = 5
};

// offsets from the anchor (the pull or the end of the stun), Warcraft Logs Normal medians
static constexpr Milliseconds RoarOffsets[] = { 0ms, 36300ms };
static constexpr Milliseconds ChainLinkOffset = 4700ms;
static constexpr Milliseconds StompOffsets[] = { 18400ms, 44000ms };
static constexpr Milliseconds GazeOffset = 51200ms;
static constexpr Milliseconds ChargeAfterGaze = 6s;
static constexpr Milliseconds RoarDamageDelay = 750ms;      // sniff: the roar damage lands 0.75 s after the aura
static constexpr Milliseconds ChainThemDuration = 6s;
static constexpr Milliseconds ImpactStun = 12s;
static constexpr float ChargeHitRange = 6.0f;               // players this close to the arrival point take the charge hit
static constexpr float ChainBreakRange = 12.0f;
static constexpr uint8 GruesomeRagePct = 30;

// 164407 - Sludgefist
struct boss_sludgefist : public BossAI
{
    boss_sludgefist(Creature* creature) : BossAI(creature, DATA_SLUDGEFIST) { }

    void Reset() override
    {
        _Reset();
        _gruesomeRage = false;
        _gazeTarget.Clear();
        _pairs.clear();
    }

    void JustEngagedWith(Unit* who) override
    {
        BossAI::JustEngagedWith(who);
        Talk(SAY_AGGRO);
        DoCastSelf(SPELL_SEISMIC_SHIFT, true);
        StartCycle();
    }

    void StartCycle()
    {
        DoCastSelf(SPELL_RAGE_GAIN, true);
        for (Milliseconds offset : RoarOffsets)
            events.ScheduleEvent(EVENT_COLOSSAL_ROAR, offset);
        events.ScheduleEvent(EVENT_CHAIN_LINK, ChainLinkOffset);
        for (Milliseconds offset : StompOffsets)
            events.ScheduleEvent(EVENT_DESTRUCTIVE_STOMP, offset);
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

    void MovementInform(uint32 /*type*/, uint32 id) override
    {
        if (id != EVENT_CHARGE)
            return;

        // the charge ends against a wall (pillar collisions are not modelled): Destructive Impact and the 12 s stun
        for (MapReference const& ref : me->GetMap()->GetPlayers())
            if (Player* player = ref.GetSource())
                if (player->IsAlive() && !player->IsGameMaster() && player->GetDistance(me) <= ChargeHitRange)
                    DoCast(player, SPELL_HEEDLESS_CHARGE_HIT, true);

        DoCastSelf(SPELL_DESTRUCTIVE_IMPACT, true);
        DoCastSelf(SPELL_DESTRUCTIVE_IMPACT_STUN, true);
        for (MapReference const& ref : me->GetMap()->GetPlayers())
            if (Player* player = ref.GetSource())
                if (player->IsAlive() && !player->IsGameMaster())
                    player->CastSpell(player, SPELL_COLLAPSING_FOUNDATION, CastSpellExtraArgs(TRIGGERED_FULL_MASK).SetOriginalCaster(me->GetGUID()));
        events.ScheduleEvent(EVENT_STUN_END, ImpactStun);
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
            {
                Unit* target = ObjectAccessor::GetUnit(*me, _gazeTarget);
                if (!target || !target->IsAlive())
                    target = me->GetVictim();
                if (target)
                    me->CastSpell(target->GetPosition(), SPELL_HEEDLESS_CHARGE, true);
                break;
            }
            case EVENT_STUN_END:
                Talk(SAY_STUN_END);
                StartCycle();
                break;
            default:
                break;
        }
    }

    void JustDied(Unit* killer) override
    {
        BossAI::JustDied(killer);
        Talk(SAY_DEATH);
        for (uint32 spellId : { SPELL_COLLAPSING_FOUNDATION, SPELL_CHAIN_LINK_AURA, SPELL_CHAIN_THEM, SPELL_HATEFUL_GAZE_MARK })
            instance->DoRemoveAurasDueToSpellOnPlayers(spellId);
    }

    void EnterEvadeMode(EvadeReason why) override
    {
        for (uint32 spellId : { SPELL_COLLAPSING_FOUNDATION, SPELL_CHAIN_LINK_AURA, SPELL_CHAIN_THEM, SPELL_HATEFUL_GAZE_MARK })
            instance->DoRemoveAurasDueToSpellOnPlayers(spellId);
        BossAI::EnterEvadeMode(why);
    }

private:
    bool _gruesomeRage = false;
    ObjectGuid _gazeTarget;
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

void AddSC_boss_sludgefist()
{
    RegisterCastleNathriaCreatureAI(boss_sludgefist);
    RegisterSpellScript(spell_sludgefist_chain_link);
}
