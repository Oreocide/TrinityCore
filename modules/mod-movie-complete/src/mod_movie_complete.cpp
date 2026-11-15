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

// Casts a spell on the player when the client finishes a movie (Oreocide/TrinityCore#211).
//
// Retail answers CMSG_COMPLETE_MOVIE on its own: the server casts a spell on the player in the same tick (captures,
// 2026-11-15: CMSG_COMPLETE_MOVIE, then SMSG_SPELL_START/GO with no client cast). The client data does not link the
// movie to that spell, so the link is the table `movie_complete_spell`. That spell carries the follow-up: a quest
// credit, a teleport, a conversation, an aura cleanup.
// Live's only hook is WorldSession::HandleCompleteMovie -> ScriptMgr::OnMovieComplete (MiscHandler.cpp:799-810);
// SmartAI has no movie-complete event.
#include "DB2Stores.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include <unordered_map>
#include <vector>

namespace
{
// movie id -> spells the player casts on itself when it completes the movie; filled once at startup
std::unordered_map<uint32, std::vector<uint32>> MovieCompleteSpells;

class mod_movie_complete_world : public WorldScript
{
public:
    mod_movie_complete_world() : WorldScript("mod_movie_complete_world") { }

    // OnStartup, not OnConfigLoad: the spell store is not loaded at config time
    void OnStartup() override
    {
        MovieCompleteSpells.clear();
        QueryResult result = WorldDatabase.Query("SELECT MovieId, SpellId FROM movie_complete_spell ORDER BY MovieId, SpellId");
        uint32 count = 0;
        if (result)
        {
            do
            {
                Field* fields = result->Fetch();
                uint32 const movieId = fields[0].GetUInt32();
                uint32 const spellId = fields[1].GetUInt32();
                if (!sMovieStore.LookupEntry(movieId))
                {
                    TC_LOG_ERROR("sql.sql", "Table `movie_complete_spell` has movie {} that is not in Movie.db2, skipped.", movieId);
                    continue;
                }
                if (!sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE))
                {
                    TC_LOG_ERROR("sql.sql", "Table `movie_complete_spell` has movie {} with unknown spell {}, skipped.", movieId, spellId);
                    continue;
                }
                MovieCompleteSpells[movieId].push_back(spellId);
                ++count;
            } while (result->NextRow());
        }
        TC_LOG_INFO("server.loading", ">> Loaded {} movie completion spells for {} movies", count, uint32(MovieCompleteSpells.size()));
    }
};

class mod_movie_complete_player : public PlayerScript
{
public:
    mod_movie_complete_player() : PlayerScript("mod_movie_complete_player") { }

    // HandleCompleteMovie calls this only for the movie the server started (Player::GetMovie), after clearing it
    void OnMovieComplete(Player* player, uint32 movieId) override
    {
        auto itr = MovieCompleteSpells.find(movieId);
        if (itr == MovieCompleteSpells.end())
            return;

        for (uint32 spellId : itr->second)
            player->CastSpell(player, spellId, CastSpellExtraArgs(TRIGGERED_FULL_MASK));
    }
};
}

void AddSC_mod_movie_complete()
{
    new mod_movie_complete_world();
    new mod_movie_complete_player();
}
