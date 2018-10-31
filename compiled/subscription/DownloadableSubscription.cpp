/*
 * This file is part of Adblock Plus <https://adblockplus.org/>,
 * Copyright (C) 2006-present eyeo GmbH
 *
 * Adblock Plus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * Adblock Plus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Adblock Plus.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <cwctype>
#include <limits>

#include "DownloadableSubscription.h"
#include "../FilterNotifier.h"
#include "../StringScanner.h"
#include "../filter/CommentFilter.h"

ABP_NS_USING

namespace {
  constexpr int MILLIS_IN_HOUR = 60 * 60 * 1000;
  constexpr int MILLIS_IN_DAY = 24 * MILLIS_IN_HOUR;
  // limits
  constexpr int64_t MAX_HOUR = std::numeric_limits<int64_t>::max() / MILLIS_IN_HOUR;
  constexpr int64_t MAX_DAY = std::numeric_limits<int64_t>::max() / MILLIS_IN_DAY;

  typedef std::pair<DependentString, DependentString> Param;

  Param ParseParam(const String& text)
  {
    Param param;

    if (text[0] == u'!')
    {
      bool foundColon = false;
      String::size_type beginParam = 0;
      String::size_type endParam = 0;
      String::size_type beginValue = 0;
      for (String::size_type i = 1; i < text.length(); i++)
      {
        switch (text[i])
        {
        case ' ':
        case '\t':
          if (beginParam > 0 && !foundColon)
          {
            endParam = i;
          }
          break;
        case ':':
          foundColon = true;
          endParam = i;
          break;
        default:
          if (foundColon)
          {
            beginValue = i;
          }
          else
          {
            if (beginParam == 0)
              beginParam = i;
          }
          break;
        }
        if (beginValue > 0)
          break;
      }
      if (beginValue > 0)
      {
        param.first = DependentString(text, beginParam, endParam - beginParam);
        param.first.toLower();
        param.second = DependentString(
          text, beginValue, text.length() - beginValue);
      }
    }
    return param;
  }
}

DownloadableSubscription_Parser::DownloadableSubscription_Parser()
{
  annotate_address(this, "DownloadableSubscription_Parser");
}

namespace {
  const DependentString ADBLOCK_HEADER(u"[Adblock"_str);
  const DependentString ADBLOCK_PLUS_EXTRA_HEADER(u"Plus"_str);

  const DependentString ERROR_INVALID_DATA(u"synchronize_invalid_data"_str);
}

/// Return true if more line expected.
bool DownloadableSubscription_Parser::GetNextLine(DependentString& buffer, DependentString& line)
{
  StringScanner scanner(buffer);
  String::value_type ch = 0;
  while (ch != u'\r' && ch != u'\n')
  {
    ch = scanner.next();
    if (ch == 0)
      break;
  }

  auto eol = scanner.position();
  line.reset(buffer, 0, eol);
  if (eol == 0 || ch == 0)
    return false;
  while (scanner.skipOne(u'\r') || scanner.skipOne(u'\n'))
    ;
  buffer.reset(buffer, scanner.position() + 1);
  return true;
}

bool DownloadableSubscription_Parser::Process(const String& buffer)
{
  DependentString currentBuffer(buffer);
  bool firstLine = true;

  DependentString line;
  while (true)
  {
    bool more = GetNextLine(currentBuffer, line);
    if (firstLine)
    {
      if (!ProcessFirstLine(line))
      {
        mError = ERROR_INVALID_DATA;
        return false;
      }
      firstLine = false;
    }
    else
      ProcessLine(line);
    if (!more)
      break;
  }
  return true;
}

bool DownloadableSubscription_Parser::ProcessFirstLine(const String& line)
{
  auto index = line.find(ADBLOCK_HEADER);
  if (index == String::npos)
    return false;

  DependentString minVersion;
  DependentString current(line, index + ADBLOCK_HEADER.length());
  StringScanner scanner(current);
  if (scanner.skipWhiteSpace() && scanner.skipString(ADBLOCK_PLUS_EXTRA_HEADER))
    scanner.skipWhiteSpace();
  index = scanner.position() + 1;
  String::value_type ch = u'\0';
  while((ch = scanner.next()) && (ch == u'.' || std::iswdigit(ch)))
    ;
  if (ch)
    scanner.back();
  if (scanner.position() + 1 > index)
    minVersion.reset(current, index, scanner.position() + 1 - index);

  if (ch != u']')
    return false;

  mRequiredVersion = minVersion;
  return true;
}

void DownloadableSubscription_Parser::ProcessLine(const String& line)
{
  auto param = ParseParam(line);
  if (param.first.is_invalid())
  {
    if (!line.empty())
      mFiltersText.emplace_back(line);
  }
  else
    mParams[param.first] = param.second;
}

int64_t DownloadableSubscription_Parser::ParseExpires(const String& expires)
{
  bool isHour = false;
  StringScanner scanner(expires);
  String::size_type numStart = 0;
  String::size_type numLen = 0;
  while(!scanner.done())
  {
    auto ch = scanner.next();
    if (std::iswdigit(ch))
    {
      if (numLen == 0)
        numStart = scanner.position();
      numLen++;
    }
    else if (std::iswspace(ch))
    {
      if (numLen)
        break;
    }
    else
    {
      if (numLen)
        scanner.back();
      break;
    }
  }

  DependentString numStr(expires, numStart, numLen);
  int64_t num = lexical_cast<int64_t>(numStr);
  if (num == 0)
    return 0;

  while (!scanner.done())
  {
    auto ch = scanner.next();
    if (std::iswspace(ch))
      continue;

    if (ch == u'h')
      isHour = true;

    // assume we are done here. The rest is ignored.
    break;
  }
  // check for overflow.
  if ((isHour && (num > MAX_HOUR)) || (num > MAX_DAY))
    return 0;

  num *= isHour ? MILLIS_IN_HOUR : MILLIS_IN_DAY;
  return num;
}

int64_t DownloadableSubscription_Parser::Finalize(DownloadableSubscription& subscription)
{
  FilterNotifier::SubscriptionChange(
    FilterNotifier::Topic::SUBSCRIPTION_BEFORE_FILTERS_REPLACED,
    subscription);

  if (!mRequiredVersion.empty())
    subscription.SetRequiredVersion(mRequiredVersion);

  auto entry = mParams.find(u"title"_str);
  if (entry)
  {
    subscription.SetTitle(entry->second);
    subscription.SetFixedTitle(true);
  }
  else
    subscription.SetFixedTitle(false);

  int32_t version = 0;
  entry = mParams.find(u"version"_str);
  if (entry)
    version = lexical_cast<int32_t>(entry->second);
  subscription.SetDataRevision(version);

  int64_t expires = 0;
  entry = mParams.find(u"expires"_str);
  if (entry)
    expires = ParseExpires(entry->second);

  Subscription::Filters filters;
  filters.reserve(mFiltersText.size());
  for (auto text : mFiltersText)
  {
    DependentString dependent(text);
    filters.emplace_back(Filter::FromText(dependent), false);
  }

  subscription.SetFilters(std::move(filters));
  FilterNotifier::SubscriptionChange(
    FilterNotifier::Topic::SUBSCRIPTION_FILTERS_REPLACED, subscription);

  return expires;
}

namespace {
  DependentString emptyString = u""_str;
}

const String& DownloadableSubscription_Parser::GetRedirect() const
{
  auto entry = mParams.find(u"redirect"_str);
  if (entry)
    return entry->second;
  return emptyString;
}

const String& DownloadableSubscription_Parser::GetHomepage() const
{
  auto entry = mParams.find(u"homepage"_str);
  if (entry)
    return entry->second;
  return emptyString;
}

ABP_NS_USING

DownloadableSubscription::DownloadableSubscription(const String& id)
    : Subscription(classType, id), mFixedTitle(false), mLastCheck(0),
      mHardExpiration(0), mSoftExpiration(0), mLastDownload(0), mLastSuccess(0),
      mErrorCount(0), mDataRevision(0), mDownloadCount(0)
{
  SetTitle(id);
}

DownloadableSubscription_Parser* DownloadableSubscription::ParseDownload()
{
  return new DownloadableSubscription_Parser();
}

OwnedString DownloadableSubscription::Serialize() const
{
  OwnedString result(Subscription::Serialize());
  if (mFixedTitle)
    result.append(ABP_TEXT("fixedTitle=true\n"_str));
  if (!mHomepage.empty())
  {
    result.append(ABP_TEXT("homepage="_str));
    result.append(mHomepage);
    result.append(ABP_TEXT('\n'));
  }
  if (mLastCheck)
  {
    result.append(ABP_TEXT("lastCheck="_str));
    result.append(mLastCheck);
    result.append(ABP_TEXT('\n'));
  }
  if (mHardExpiration)
  {
    result.append(ABP_TEXT("expires="_str));
    result.append(mHardExpiration);
    result.append(ABP_TEXT('\n'));
  }
  if (mSoftExpiration)
  {
    result.append(ABP_TEXT("softExpiration="_str));
    result.append(mSoftExpiration);
    result.append(ABP_TEXT('\n'));
  }
  if (mLastDownload)
  {
    result.append(ABP_TEXT("lastDownload="_str));
    result.append(mLastDownload);
    result.append(ABP_TEXT('\n'));
  }
  if (!mDownloadStatus.empty())
  {
    result.append(ABP_TEXT("downloadStatus="_str));
    result.append(mDownloadStatus);
    result.append(ABP_TEXT('\n'));
  }
  if (mLastSuccess)
  {
    result.append(ABP_TEXT("lastSuccess="_str));
    result.append(mLastSuccess);
    result.append(ABP_TEXT('\n'));
  }
  if (mErrorCount)
  {
    result.append(ABP_TEXT("errors="_str));
    result.append(mErrorCount);
    result.append(ABP_TEXT('\n'));
  }
  if (mDataRevision)
  {
    result.append(ABP_TEXT("version="_str));
    result.append(mDataRevision);
    result.append(ABP_TEXT('\n'));
  }
  if (!mRequiredVersion.empty())
  {
    result.append(ABP_TEXT("requiredVersion="_str));
    result.append(mRequiredVersion);
    result.append(ABP_TEXT('\n'));
  }
  if (mDownloadCount)
  {
    result.append(ABP_TEXT("downloadCount="_str));
    result.append(mDownloadCount);
    result.append(ABP_TEXT('\n'));
  }
  return result;
}
