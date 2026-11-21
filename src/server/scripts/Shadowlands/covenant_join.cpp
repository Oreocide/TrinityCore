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
 * Covenant join (Shadowlands, Oribos).
 *
 * Retail: using the Shadowlands Covenant Map (gameobject 355352, player choice 644) shows the covenant choice. "Preview Covenant" answers with
 * SMSG_COVENANT_PREVIEW_OPEN_NPC and leaves the choice open, "Join" casts "Become a <covenant>", sets PlayerData.CovenantID, clears the choice,
 * swaps the covenant spells and grants 10 Soulbind Conduit Energy. Everything here is off unless Covenant.Enable = 1.
 */

#include "ScriptMgr.h"
#include "DB2Stores.h"
#include "GameObject.h"
#include "GameObjectAI.h"
#include "GossipDef.h"
#include "NPCPackets.h"
#include "Player.h"
#include "PlayerChoice.h"
#include "QuestPackets.h"
#include "SpellDefines.h"
#include "World.h"
#include <array>

namespace
{
enum CovenantJoin
{
    PLAYERCHOICE_SHADOWLANDS_COVENANT   = 644,

    CURRENCY_SOULBIND_CONDUIT_ENERGY    = 1883,
    SOULBIND_CONDUIT_ENERGY_ON_JOIN     = 10
};

struct CovenantEntry
{
    int32 CovenantID;
    uint32 JoinSpell;       // "Become a <covenant>"
    uint32 CovenantSpell;   // the covenant's own spell (on its skill line)
    uint32 OpeningSpell;    // "Opening", the covenant's dungeon lock spell
    uint32 SkillLineID;
    uint32 PreviewResponseID;
    uint32 JoinResponseID;
};

// Covenant.db2 ids 1-4 with their SkillLineID (2730-2733). Spell ids from SpellName/Spell descriptions and SkillLineAbility of those skill lines;
// retail learned exactly CovenantSpell and OpeningSpell of the joined covenant (nov18a: 340010 + 321076 for Kyrian).
constexpr std::array<CovenantEntry, 4> Covenants =
{ {
    { .CovenantID = 1, .JoinSpell = 337779, .CovenantSpell = 321076, .OpeningSpell = 340010, .SkillLineID = 2730, .PreviewResponseID = 2708, .JoinResponseID = 2689 }, // Kyrian
    { .CovenantID = 2, .JoinSpell = 337782, .CovenantSpell = 321079, .OpeningSpell = 340013, .SkillLineID = 2731, .PreviewResponseID = 2686, .JoinResponseID = 2702 }, // Venthyr
    { .CovenantID = 3, .JoinSpell = 337784, .CovenantSpell = 321077, .OpeningSpell = 340004, .SkillLineID = 2732, .PreviewResponseID = 2707, .JoinResponseID = 2688 }, // Night Fae
    { .CovenantID = 4, .JoinSpell = 337781, .CovenantSpell = 321078, .OpeningSpell = 340014, .SkillLineID = 2733, .PreviewResponseID = 2706, .JoinResponseID = 2687 }  // Necrolord
} };

bool IsOnSkillLine(uint32 skillLineId, uint32 spellId)
{
    if (std::vector<SkillLineAbilityEntry const*> const* abilities = sDB2Manager.GetSkillLineAbilitiesBySkill(skillLineId))
        for (SkillLineAbilityEntry const* ability : *abilities)
            if (ability->Spell == int32(spellId))
                return true;

    return false;
}

// forgets every spell the player knows that belongs to another covenant's skill line (and not to the joined one's)
void RemoveOtherCovenantSpells(Player* player, CovenantEntry const& joined)
{
    for (CovenantEntry const& other : Covenants)
    {
        if (other.CovenantID == joined.CovenantID)
            continue;

        std::vector<SkillLineAbilityEntry const*> const* abilities = sDB2Manager.GetSkillLineAbilitiesBySkill(other.SkillLineID);
        if (!abilities)
            continue;

        for (SkillLineAbilityEntry const* ability : *abilities)
        {
            uint32 spellId = ability->Spell;
            if (!player->HasSpell(spellId) || IsOnSkillLine(joined.SkillLineID, spellId))
                continue;

            player->RemoveSpell(spellId, false, false);
        }
    }
}
}

// Shadowlands Covenant Map
struct go_shadowlands_covenant_map : public GameObjectAI
{
    using GameObjectAI::GameObjectAI;

    bool OnGossipHello(Player* player) override
    {
        if (!sWorld->getBoolConfig(CONFIG_COVENANT_ENABLE) || player->GetCovenant())
            return false;

        player->SendPlayerChoice(me->GetGUID(), PLAYERCHOICE_SHADOWLANDS_COVENANT);
        return true;
    }
};

class playerchoice_covenant_join : public PlayerChoiceScript
{
public:
    playerchoice_covenant_join() : PlayerChoiceScript("playerchoice_covenant_join") { }

    void OnResponse(WorldObject* object, Player* player, PlayerChoice const* /*choice*/, PlayerChoiceResponse const* response, uint16 /*clientIdentifier*/) override
    {
        if (!sWorld->getBoolConfig(CONFIG_COVENANT_ENABLE) || player->GetCovenant())
            return;

        for (CovenantEntry const& covenant : Covenants)
        {
            if (uint32(response->ResponseId) == covenant.PreviewResponseID)
            {
                // the choice stays open: retail sends no clear between previews
                WorldPackets::NPC::CovenantPreviewOpenNpc preview;
                preview.NpcGUID = object ? object->GetGUID() : player->PlayerTalkClass->GetInteractionData().SourceGuid;
                preview.CovenantID = covenant.CovenantID;
                player->SendDirectMessage(preview.Write());
                return;
            }

            if (uint32(response->ResponseId) == covenant.JoinResponseID)
            {
                Join(player, covenant);
                return;
            }
        }
    }

private:
    static void Join(Player* player, CovenantEntry const& covenant)
    {
        // CovenantID first: retail sends the phase shift before the cast, and the cast's effects refresh phases
        player->SetCovenant(covenant.CovenantID);
        player->CastSpell(player, covenant.JoinSpell, CastSpellExtraArgs(true));

        player->PlayerTalkClass->GetInteractionData().Reset();
        player->SendDirectMessage(WorldPackets::Quest::PlayerChoiceClear().Write());

        player->LearnSpell(covenant.OpeningSpell, false);
        player->LearnSpell(covenant.CovenantSpell, false);
        RemoveOtherCovenantSpells(player, covenant);

        player->AddCurrency(CURRENCY_SOULBIND_CONDUIT_ENERGY, SOULBIND_CONDUIT_ENERGY_ON_JOIN, CurrencyGainSource::Script);
    }
};

void AddSC_covenant_join()
{
    RegisterGameObjectAI(go_shadowlands_covenant_map);
    new playerchoice_covenant_join();
}
