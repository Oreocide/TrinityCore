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

#include "InstanceScriptData.h"
#include "DB2Stores.h"
#include "InstanceScript.h"
#include "Log.h"
#include "Map.h"
#include "StringConvert.h"
#include "World.h"
#include <rapidjson/pointer.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <rapidjson/error/en.h>

namespace
{
    std::string const HeadersKey = "Header";

    std::string const BossStatesSaveDataKey = "BossStates";

    std::string const MoreSaveDataKey = "AdditionalData";
}

InstanceScriptDataReader::Result InstanceScriptDataReader::Load(char const* data)
{
    /*
       Expected JSON

        {
            "Header": "HEADER_STRING_SET_BY_SCRIPT",
            "BossStates": [0,2,0,...] // indexes are boss ids, values are EncounterState
            "AdditionalData: { // optional
                "ExtraKey1": 123
                "AnotherExtraKey": 2.0
            }
        }
    */
    if (_doc.Parse(data, strlen(data)).HasParseError())
    {
        TC_LOG_ERROR("scripts.data.load", "JSON parser error {} at {} while loading data for instance {} [{}-{} | {}-{}]",
            rapidjson::GetParseError_En(_doc.GetParseError()), _doc.GetErrorOffset(),
            GetInstanceId(), GetMapId(), GetMapName(), GetDifficultyId(), GetDifficultyName());
        return Result::MalformedJson;
    }

    if (!_doc.IsObject())
    {
        TC_LOG_ERROR("scripts.data.load", "Root JSON value is not an object for instance {} [{}-{} | {}-{}]",
            GetInstanceId(), GetMapId(), GetMapName(), GetDifficultyId(), GetDifficultyName());
        return Result::RootIsNotAnObject;
    }

    Result result = ParseHeader();
    if (result != Result::Ok)
        return result;

    result = ParseBossStates();
    if (result != Result::Ok)
        return result;

    result = ParseAdditionalData();
    if (result != Result::Ok)
        return result;

    return Result::Ok;
}

InstanceScriptDataReader::Result InstanceScriptDataReader::ParseHeader()
{
    auto headerItr = _doc.FindMember(HeadersKey);
    if (headerItr == _doc.MemberEnd())
    {
        TC_LOG_ERROR("scripts.data.load", "Missing data header for instance {} [{}-{} | {}-{}]",
            GetInstanceId(), GetMapId(), GetMapName(), GetDifficultyId(), GetDifficultyName());
        return Result::MissingHeader;
    }

    if (headerItr->value != _instance.GetHeader())
    {
        TC_LOG_ERROR("scripts.data.load", "Incorrect data header for instance {} [{}-{} | {}-{}], expected \"{}\" got \"{}\"",
            GetInstanceId(), GetMapId(), GetMapName(), GetDifficultyId(), GetDifficultyName(),
            _instance.GetHeader(), headerItr->value.IsString() ? headerItr->value.GetString() : "");
        return Result::UnexpectedHeader;
    }

    return Result::Ok;
}

InstanceScriptDataReader::Result InstanceScriptDataReader::ParseBossStates()
{
    auto bossStatesItr = _doc.FindMember(BossStatesSaveDataKey);
    if (bossStatesItr == _doc.MemberEnd())
    {
        TC_LOG_ERROR("scripts.data.load", "Missing boss states for instance {} [{}-{} | {}-{}]",
            GetInstanceId(), GetMapId(), GetMapName(), GetDifficultyId(), GetDifficultyName());
        return Result::MissingBossStates;
    }

    rapidjson::Value const& bossStates = bossStatesItr->value;
    if (bossStates.IsArray())
    {
        for (uint32 bossId = 0; bossId < bossStates.Size(); ++bossId)
        {
            Result result = ParseBossState(bossId, bossStates[bossId]);
            if (result != Result::Ok)
                return result;
        }

        return Result::Ok;
    }

    // encounter lock data saved by older revisions of InstanceScriptDataWriter::SetBossState holds the states as {"BossId": state} members
    if (bossStates.IsObject())
    {
        for (auto const& member : bossStates.GetObject())
        {
            Optional<uint32> bossId = Trinity::StringTo<uint32>(std::string_view(member.name.GetString(), member.name.GetStringLength()));
            if (!bossId)
            {
                TC_LOG_ERROR("scripts.data.load", "Boss states has entry with non-numeric boss id \"{}\" for instance {} [{}-{} | {}-{}]",
                    member.name.GetString(), GetInstanceId(), GetMapId(), GetMapName(), GetDifficultyId(), GetDifficultyName());
                return Result::UnknownBoss;
            }

            Result result = ParseBossState(*bossId, member.value);
            if (result != Result::Ok)
                return result;
        }

        return Result::Ok;
    }

    TC_LOG_ERROR("scripts.data.load", "Boss states is not an array for instance {} [{}-{} | {}-{}]",
        GetInstanceId(), GetMapId(), GetMapName(), GetDifficultyId(), GetDifficultyName());
    return Result::BossStatesIsNotAnObject;
}

InstanceScriptDataReader::Result InstanceScriptDataReader::ParseBossState(uint32 bossId, rapidjson::Value const& bossState)
{
    if (bossId >= _instance.GetEncounterCount())
    {
        TC_LOG_ERROR("scripts.data.load", "Boss states has entry for boss with higher id ({}) than number of bosses ({}) for instance {} [{}-{} | {}-{}]",
            bossId, _instance.GetEncounterCount(), GetInstanceId(), GetMapId(), GetMapName(), GetDifficultyId(), GetDifficultyName());
        return Result::UnknownBoss;
    }

    if (!bossState.IsNumber())
    {
        TC_LOG_ERROR("scripts.data.load", "Boss state for boss ({}) is not a number for instance {} [{}-{} | {}-{}]",
            bossId, GetInstanceId(), GetMapId(), GetMapName(), GetDifficultyId(), GetDifficultyName());
        return Result::BossStateIsNotAnObject;
    }

    EncounterState state = EncounterState(bossState.GetInt());
    if (state == IN_PROGRESS || state == FAIL || state == SPECIAL)
        state = NOT_STARTED;

    if (state < TO_BE_DECIDED)
        _instance.SetBossState(bossId, state);

    return Result::Ok;
}

InstanceScriptDataReader::Result InstanceScriptDataReader::ParseAdditionalData()
{
    auto moreDataItr = _doc.FindMember(MoreSaveDataKey);
    if (moreDataItr == _doc.MemberEnd())
        return Result::Ok;

    if (!moreDataItr->value.IsObject())
    {
        TC_LOG_ERROR("scripts.data.load", "Additional data is not an object for instance {} [{}-{} | {}-{}]",
            GetInstanceId(), GetMapId(), GetMapName(), GetDifficultyId(), GetDifficultyName());
        return Result::AdditionalDataIsNotAnObject;
    }

    for (PersistentInstanceScriptValueBase* value : _instance.GetPersistentScriptValues())
    {
        auto valueItr = moreDataItr->value.FindMember(value->GetName());
        if (valueItr != moreDataItr->value.MemberEnd() && !valueItr->value.IsNull())
        {
            if (!valueItr->value.IsNumber())
            {
                TC_LOG_ERROR("scripts.data.load", "Additional data value for key {} is not a number for instance {} [{}-{} | {}-{}]",
                    value->GetName(), GetInstanceId(), GetMapId(), GetMapName(), GetDifficultyId(), GetDifficultyName());
                return Result::AdditionalDataUnexpectedValueType;
            }

            if (valueItr->value.IsDouble())
                value->LoadValue(valueItr->value.GetDouble());
            else
                value->LoadValue(valueItr->value.GetInt64());
        }
    }

    return Result::Ok;
}

uint32 InstanceScriptDataReader::GetInstanceId() const { return _instance.instance->GetInstanceId(); }
uint32 InstanceScriptDataReader::GetMapId() const { return _instance.instance->GetId(); }
char const* InstanceScriptDataReader::GetMapName() const { return _instance.instance->GetMapName(); }
uint32 InstanceScriptDataReader::GetDifficultyId() const { return uint32(_instance.instance->GetDifficultyID()); }
std::string_view InstanceScriptDataReader::GetDifficultyName() const { return DB2Manager::GetDifficultyName(_instance.instance->GetDifficultyID()); }

std::string InstanceScriptDataWriter::GetString()
{
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    _doc.Accept(writer);

    return std::string(buffer.GetString(), buffer.GetSize());
}

void InstanceScriptDataWriter::FillData(bool withValues)
{
    _doc.SetObject();
    _doc.AddMember(rapidjson::StringRef(HeadersKey), _instance.GetHeader(), _doc.GetAllocator());

    rapidjson::Value bossStates(rapidjson::kArrayType);
    for (uint32 bossId = 0; bossId < _instance.GetEncounterCount(); ++bossId)
    {
        rapidjson::Value bossStateValue(rapidjson::kNumberType);
        bossStateValue.SetInt(withValues ? _instance.GetBossState(bossId) : NOT_STARTED);
        bossStates.PushBack(bossStateValue.Move(), _doc.GetAllocator());
    }
    _doc.AddMember(rapidjson::StringRef(BossStatesSaveDataKey), bossStates.Move(), _doc.GetAllocator());

    if (!_instance.GetPersistentScriptValues().empty())
    {
        rapidjson::Value moreData(rapidjson::kObjectType);
        for (PersistentInstanceScriptValueBase* additionalValue : _instance.GetPersistentScriptValues())
        {
            if (withValues)
            {
                UpdateAdditionalSaveDataEvent data = additionalValue->CreateEvent();
                std::visit([&](auto v)
                {
                    moreData.AddMember(rapidjson::StringRef(data.Key), rapidjson::Value(v), _doc.GetAllocator());
                }, data.Value);
            }
            else
                moreData.AddMember(rapidjson::StringRef(additionalValue->GetName()), rapidjson::Value(), _doc.GetAllocator());
        }

        _doc.AddMember(rapidjson::StringRef(MoreSaveDataKey), moreData.Move(), _doc.GetAllocator());
    }
}

void InstanceScriptDataWriter::FillDataFrom(std::string const& data)
{
    if (_doc.Parse(data).HasParseError() || !_doc.IsObject())
        FillData(false);
}

void InstanceScriptDataWriter::SetBossState(UpdateBossStateSaveDataEvent const& data)
{
    rapidjson::Document::AllocatorType& allocator = _doc.GetAllocator();

    auto bossStatesItr = _doc.FindMember(BossStatesSaveDataKey);
    if (bossStatesItr == _doc.MemberEnd())
    {
        _doc.AddMember(rapidjson::StringRef(BossStatesSaveDataKey), rapidjson::Value(rapidjson::kArrayType), allocator);
        bossStatesItr = _doc.FindMember(BossStatesSaveDataKey);
    }

    // a json pointer with an object member token ("/BossStates/BossId") would turn the array into an object, which the reader rejects
    rapidjson::Value& bossStates = bossStatesItr->value;
    if (!bossStates.IsArray())
    {
        // data saved before this fix holds the states as {"BossId": state} members
        rapidjson::Value converted(rapidjson::kArrayType);
        for (uint32 bossId = 0; bossId < _instance.GetEncounterCount(); ++bossId)
            converted.PushBack(int32(NOT_STARTED), allocator);

        if (bossStates.IsObject())
            for (auto const& member : bossStates.GetObject())
                if (Optional<uint32> bossId = Trinity::StringTo<uint32>(std::string_view(member.name.GetString(), member.name.GetStringLength())))
                    if (*bossId < converted.Size() && member.value.IsNumber())
                        converted[*bossId].SetInt(member.value.GetInt());

        bossStates = converted.Move();
    }

    while (bossStates.Size() <= data.BossId)
        bossStates.PushBack(int32(NOT_STARTED), allocator);

    bossStates[data.BossId].SetInt(data.NewState);
}

void InstanceScriptDataWriter::SetAdditionalData(UpdateAdditionalSaveDataEvent const& data)
{
    rapidjson::Pointer::Token tokens[] =
    {
        { MoreSaveDataKey.c_str(), uint32(MoreSaveDataKey.length()), rapidjson::kPointerInvalidIndex },
        { data.Key, uint32(strlen(data.Key)), rapidjson::kPointerInvalidIndex }
    };
    rapidjson::Pointer ptr(tokens, std::size(tokens));

    // jsonptr("/AdditionalData/CustomValueName")
    std::visit([&](auto v)
    {
        rapidjson::SetValueByPointer(_doc, ptr, v);
    }, data.Value);
}
