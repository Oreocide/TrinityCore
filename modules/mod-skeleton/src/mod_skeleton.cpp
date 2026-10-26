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

#include "Config.h"
#include "Log.h"
#include "ScriptMgr.h"

class mod_skeleton_world : public WorldScript
{
public:
    mod_skeleton_world() : WorldScript("mod_skeleton_world") { }

    void OnConfigLoad(bool /*reload*/) override
    {
        _enabled = sConfigMgr->GetBoolDefault("Skeleton.Enable", false);
    }

    void OnStartup() override
    {
        if (_enabled)
            TC_LOG_INFO("server.loading", "mod-skeleton: module loaded (Skeleton.Enable = 1)");
    }

private:
    bool _enabled = false;
};

void AddSC_mod_skeleton()
{
    new mod_skeleton_world();
}
