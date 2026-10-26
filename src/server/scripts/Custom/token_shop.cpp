/*
 * Token shop: redeem a custom WoW Token item for gold at a rate that rises with
 * the number of tokens in circulation. Config row lives in world.token_shop_config.
 */

#include "ScriptMgr.h"
#include "Chat.h"
#include "DatabaseEnv.h"
#include "Player.h"
#include "SpellScript.h"

enum TokenShop
{
    SPELL_TOKEN_REDEEM = 61180  // unused creature-cooldown dummy spell, bound to the token's item effect
};

// 61180 - Token Redeem
class spell_token_shop_redeem : public SpellScript
{
    // The spell is an AoE with a radius; pay out once per cast, never once per unit hit.
    void HandleLaunch(SpellEffIndex /*effIndex*/)
    {
        if (_paid)
            return;
        _paid = true;

        Player* player = GetCaster()->ToPlayer();
        if (!player)
            return;

        QueryResult cfg = WorldDatabase.Query("SELECT item_entry, base_gold, step FROM token_shop_config LIMIT 1");
        if (!cfg)
            return;

        Field* f = cfg->Fetch();
        uint32 itemEntry = f[0].GetUInt32();
        double baseGold = f[1].GetDouble();
        double step = f[2].GetDouble();

        QueryResult circ = CharacterDatabase.Query(fmt::format("SELECT COALESCE(CAST(SUM(count) AS UNSIGNED), 0) FROM item_instance WHERE itemEntry = {}", itemEntry).c_str());
        uint64 inCirculation = circ ? (*circ)[0].GetUInt64() : 0;

        uint64 gold = uint64(baseGold * (1.0 + step * double(inCirculation)));
        if (!player->ModifyMoney(int64(gold) * GOLD))
            return;

        ChatHandler(player->GetSession()).SendSysMessage(fmt::format("Token redeemed for {} gold ({} tokens in circulation).", gold, inCirculation));
    }

    void Register() override
    {
        OnEffectLaunch += SpellEffectFn(spell_token_shop_redeem::HandleLaunch, EFFECT_0, SPELL_EFFECT_DUMMY);
    }

    bool _paid = false;
};

void AddSC_token_shop()
{
    RegisterSpellScript(spell_token_shop_redeem);
}
