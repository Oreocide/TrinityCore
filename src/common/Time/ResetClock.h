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

#ifndef TRINITYCORE_RESET_CLOCK_H
#define TRINITYCORE_RESET_CLOCK_H

#include "Define.h"
#include <ctime>

// Realm reset clock: every reset boundary is a whole number of periods after one fixed
// reset instant (the epoch), so daily, half-week and weekly resets never drift with DST.
// The epoch is a weekly reset instant; it fixes both the reset hour (in UTC) and the reset weekday.
namespace Trinity::ResetClock
{
inline constexpr int64 DailyPeriod = 86400;
inline constexpr int64 HalfWeekPeriod = 302400;
inline constexpr int64 WeeklyPeriod = 604800;

// Tue 2026-02-24 15:00 UTC: retail US weekly reset, week index 0 of the 2026 sniff measurements
inline constexpr int64 DefaultEpoch = 1771945200;

TC_COMMON_API void Configure(bool realmClock, int64 epoch);

// true when daily/weekly/monthly quest, instance and cap resets follow the realm clock instead of local hours
TC_COMMON_API bool IsRealmClock();
TC_COMMON_API int64 GetEpoch();

// floor((t - epoch - offset) / period)
TC_COMMON_API int64 PeriodIndex(time_t t, int64 period, int64 offset = 0);
TC_COMMON_API time_t PeriodStart(int64 index, int64 period, int64 offset = 0);
// first boundary strictly after t
TC_COMMON_API time_t NextBoundary(time_t t, int64 period, int64 offset = 0);

TC_COMMON_API time_t NextDaily(time_t t);
TC_COMMON_API time_t NextHalfWeek(time_t t);
TC_COMMON_API time_t NextWeekly(time_t t);
// first daily reset falling on the 1st of a month (UTC date), strictly after t
TC_COMMON_API time_t NextMonthly(time_t t);

TC_COMMON_API int64 DayIndex(time_t t);
TC_COMMON_API int64 WeekIndex(time_t t);
}

#endif // TRINITYCORE_RESET_CLOCK_H
