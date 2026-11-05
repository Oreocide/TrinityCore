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

#include "ResetClock.h"
#include "Util.h"
#include <atomic>

namespace
{
std::atomic<bool> RealmClock = false;
std::atomic<int64> Epoch = Trinity::ResetClock::DefaultEpoch;

int64 FloorDiv(int64 a, int64 b)
{
    int64 q = a / b;
    if ((a % b != 0) && ((a < 0) != (b < 0)))
        --q;
    return q;
}
}

namespace Trinity::ResetClock
{
void Configure(bool realmClock, int64 epoch)
{
    RealmClock = realmClock;
    Epoch = epoch;
}

bool IsRealmClock()
{
    return RealmClock;
}

int64 GetEpoch()
{
    return Epoch;
}

int64 PeriodIndex(time_t t, int64 period, int64 offset /*= 0*/)
{
    return FloorDiv(int64(t) - Epoch - offset, period);
}

time_t PeriodStart(int64 index, int64 period, int64 offset /*= 0*/)
{
    return time_t(Epoch + offset + index * period);
}

time_t NextBoundary(time_t t, int64 period, int64 offset /*= 0*/)
{
    return PeriodStart(PeriodIndex(t, period, offset) + 1, period, offset);
}

time_t NextDaily(time_t t)
{
    return NextBoundary(t, DailyPeriod);
}

time_t NextHalfWeek(time_t t)
{
    return NextBoundary(t, HalfWeekPeriod);
}

time_t NextWeekly(time_t t)
{
    return NextBoundary(t, WeeklyPeriod);
}

time_t NextMonthly(time_t t)
{
    time_t next = NextDaily(t);
    tm date;
    gmtime_r(&next, &date);
    if (date.tm_mday == 1)
        return next;

    // daily resets share one UTC time of day, so the reset on the 1st keeps hour/min/sec
    date.tm_mday = 1;
    date.tm_mon += 1;
    return timegm(&date);
}

int64 DayIndex(time_t t)
{
    return PeriodIndex(t, DailyPeriod);
}

int64 WeekIndex(time_t t)
{
    return PeriodIndex(t, WeeklyPeriod);
}
}
