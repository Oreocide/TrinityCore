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

#include "ChatPackets.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "Spell.h"
#include "SpellAuraEffects.h"
#include "SpellScript.h"

namespace Scripts::QuelThalas::SilvermoonCityMidnight
{
namespace Quests
{
    static constexpr uint32 CompleteMidnightCampaign = 90806;
    static constexpr uint32 NoFearOfTheDark = 86549;
}

namespace QuestObjectives
{
    static constexpr uint32 PortalEntered = 458798;
}

namespace Spells
{
    static constexpr uint32 PortalToVoidstormQuest = 1214507;
    static constexpr uint32 PortalToVoidstorm = 1225695;
}

// 1214506 - Portal to Voidstorm (gameobject 504395, Duskglow Gloriette)
// Both effects are dummies naming a teleport. Retail casts 1214507 (teleport plus the "Portal entered" credit of 86549) while
// that objective is open, and 1225695 (teleport only) on every later use.
class spell_silvermoon_portal_to_voidstorm : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ Spells::PortalToVoidstormQuest, Spells::PortalToVoidstorm });
    }

    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        Player* player = Object::ToPlayer(GetHitUnit());
        if (!player)
            return;

        bool onStep = player->GetQuestStatus(Quests::NoFearOfTheDark) == QUEST_STATUS_INCOMPLETE
            && !player->IsQuestObjectiveComplete(Quests::NoFearOfTheDark, QuestObjectives::PortalEntered);
        player->CastSpell(player, onStep ? Spells::PortalToVoidstormQuest : Spells::PortalToVoidstorm, CastSpellExtraArgs(TRIGGERED_FULL_MASK)
            .SetOriginalCastId(GetSpell()->m_castId));
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_silvermoon_portal_to_voidstorm::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 1226971 - Out of Bounds (Alliance in the guarded sections: Walk of Elders, Royal Exchange, Court of Blood, Court of the Sun)
// Applied by spell_area on entry. Effect 0 is a script effect: retail sends the warning as a raid boss whisper from the player to themself
// (SMSG_CHAT type 42 with both names and chat flags 0, every application in the captures), which the client shows mid-screen.
// Player::Whisper ignores isBossWhisper and Chat::SetSender names only creatures, so the packet is built here.
// Effect 1 ticks once after 10 s into 1226974, which forces faction 2720 hostile, so the guards attack.
class spell_silvermoon_out_of_bounds_warning : public SpellScript
{
    void HandleScript(SpellEffIndex /*effIndex*/)
    {
        Player* player = Object::ToPlayer(GetHitUnit());
        if (!player)
            return;

        WorldPackets::Chat::Chat packet;
        packet.Initialize(CHAT_MSG_RAID_BOSS_WHISPER, LANG_UNIVERSAL, player, player, "The guards' eyes are on you. Return to neutral territory.");
        packet.SenderName = player->GetName();
        packet.TargetName = player->GetName();
        packet._ChatFlags = CHAT_FLAG_NONE;
        player->SendDirectMessage(packet.Write());
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_silvermoon_out_of_bounds_warning::HandleScript, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT);
    }
};

// 1278767 - [DNT] Skip Midnight Intro - Flag Campaign
class spell_silvermoon_skip_midnight_intro : public SpellScript
{
    void HandleHitTarget(SpellEffIndex /*effIndex*/)
    {
        // HACK: This should be rewarded after completing the Intro which is nyi atm.
        if (Player* player = GetCaster()->ToPlayer())
            player->CompleteQuest(Quests::CompleteMidnightCampaign);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_silvermoon_skip_midnight_intro::HandleHitTarget, EFFECT_0, SPELL_EFFECT_COMPLETE_CAMPAIGN);
    }
};
}

void AddSC_silvermoon_city_midnight()
{
    using namespace Scripts::QuelThalas::SilvermoonCityMidnight;

    // Spells
    RegisterSpellScript(spell_silvermoon_skip_midnight_intro);
    RegisterSpellScript(spell_silvermoon_portal_to_voidstorm);
    RegisterSpellScript(spell_silvermoon_out_of_bounds_warning);
}
