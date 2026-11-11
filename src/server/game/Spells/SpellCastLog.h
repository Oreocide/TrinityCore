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

#ifndef SpellCastLog_h__
#define SpellCastLog_h__

// Spell QA cast log: one line per spell start/go and aura apply/remove, for battleground and
// lifetime tests. Off by default (SpellQA.CastLog.Enable). Each line goes out in a single
// write(2) on an O_APPEND descriptor, so it is in the kernel before the caller continues and
// survives a worldserver crash, and lines from different map threads never interleave.
// Header-only on purpose: a build that does not re-run cmake still links.

#include "Config.h"
#include "Object.h"
#include "ObjectGuid.h"
#include "StringConvert.h"
#include "StringFormat.h"
#include "Util.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <vector>

#ifndef _WIN32
#include <cstdio>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>
#endif

namespace SpellCastLog
{
    struct State
    {
        std::atomic<bool> Enabled{ false };
        std::shared_mutex Lock;                 // guards everything below
        std::vector<uint32> Maps;               // empty = every map
        std::string Path;
        int Fd = -1;
        int64 MaxBytes = 0;
        std::atomic<int64> Written{ 0 };
    };

    inline State& Get()
    {
        static State state;
        return state;
    }

    // Call sites check this first, so a disabled log costs one relaxed load.
    inline bool Enabled() { return Get().Enabled.load(std::memory_order_relaxed); }

    // World::LoadConfigSettings, at boot and on every config reload.
    inline void LoadConfig()
    {
#ifndef _WIN32
        State& s = Get();
        std::unique_lock<std::shared_mutex> guard(s.Lock);

        bool enable = sConfigMgr->GetBoolDefault("SpellQA.CastLog.Enable", false);

        s.Maps.clear();
        std::string maps = sConfigMgr->GetStringDefault("SpellQA.CastLog.Maps", "");
        for (std::string_view token : Trinity::Tokenize(maps, ',', false))
            if (Optional<uint32> mapId = Trinity::StringTo<uint32>(token))
                s.Maps.push_back(*mapId);

        std::string path = sConfigMgr->GetStringDefault("LogsDir", "");
        if (!path.empty() && path.back() != '/')
            path.push_back('/');
        path += sConfigMgr->GetStringDefault("SpellQA.CastLog.File", "castlog.log");
        s.MaxBytes = int64(sConfigMgr->GetIntDefault("SpellQA.CastLog.MaxMB", 256)) * 1024 * 1024;

        if (s.Fd >= 0 && (!enable || path != s.Path))
        {
            ::close(s.Fd);
            s.Fd = -1;
        }

        if (enable && s.Fd < 0)
        {
            s.Fd = ::open(path.c_str(), O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0644);
            struct stat st;
            s.Written = (s.Fd >= 0 && ::fstat(s.Fd, &st) == 0) ? int64(st.st_size) : 0;
        }

        s.Path = std::move(path);
        s.Enabled.store(enable && s.Fd >= 0, std::memory_order_relaxed);
#endif
    }

    // where: the object whose map the event belongs to (the caster for spells, the aura target for auras).
    // Reads only the stored map and instance ids, never GetMap(), so an object off its map is safe.
    inline void Write(WorldObject const* where, std::string_view event, uint32 spellId, ObjectGuid const& caster,
        ObjectGuid const& originalCaster, ObjectGuid const& target, std::string_view extra = {})
    {
#ifndef _WIN32
        State& s = Get();
        uint32 mapId = where->GetMapId();
        bool rotate = false;
        {
            std::shared_lock<std::shared_mutex> guard(s.Lock);
            if (s.Fd < 0)
                return;
            if (!s.Maps.empty() && std::find(s.Maps.begin(), s.Maps.end(), mapId) == s.Maps.end())
                return;

            int64 now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
            std::string line = Trinity::StringFormat("ts={} map={} inst={} tid={} ev={} spell={} caster={} orig={} target={}{}{}\n",
                now, mapId, where->GetInstanceId(), long(::syscall(SYS_gettid)), event, spellId,
                caster.ToString(), originalCaster.IsEmpty() ? "-" : originalCaster.ToString(), target.IsEmpty() ? "-" : target.ToString(),
                extra.empty() ? "" : " ", extra);

            ssize_t written = ::write(s.Fd, line.data(), line.size());
            if (written > 0 && s.MaxBytes > 0)
                rotate = (s.Written += written) > s.MaxBytes;
        }

        if (rotate)
        {
            std::unique_lock<std::shared_mutex> guard(s.Lock);
            if (s.Fd >= 0 && s.Written > s.MaxBytes)
            {
                ::close(s.Fd);
                std::string old = s.Path + ".1";
                ::rename(s.Path.c_str(), old.c_str());
                s.Fd = ::open(s.Path.c_str(), O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0644);
                s.Written = 0;
                s.Enabled.store(s.Fd >= 0, std::memory_order_relaxed);
            }
        }
#else
        (void)where; (void)event; (void)spellId; (void)caster; (void)originalCaster; (void)target; (void)extra;
#endif
    }
}

#endif // SpellCastLog_h__
