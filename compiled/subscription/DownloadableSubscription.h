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

#pragma once

#include <cstdint>
#include <vector>

#include "../base.h"
#include "../bindings/runtime.h"
#include "../filter/Filter.h"
#include "../StringMap.h"
#include "Subscription.h"

ABP_NS_BEGIN

class String;
class DownloadableSubscription_Parser;

class DownloadableSubscription : public Subscription
{
public:
  static constexpr Type classType = Type::DOWNLOADABLE;
  explicit DownloadableSubscription(const String& id);

  SUBSCRIPTION_PROPERTY(bool, mFixedTitle, SUBSCRIPTION_FIXEDTITLE,
      GetFixedTitle, SetFixedTitle);
  SUBSCRIPTION_STRING_PROPERTY(mHomepage, SUBSCRIPTION_HOMEPAGE,
      GetHomepage, SetHomepage);
  SUBSCRIPTION_PROPERTY(uint64_t, mLastCheck, SUBSCRIPTION_LASTCHECK,
      GetLastCheck, SetLastCheck);
  SUBSCRIPTION_PROPERTY(uint64_t, mHardExpiration, NONE,
      GetHardExpiration, SetHardExpiration);
  SUBSCRIPTION_PROPERTY(uint64_t, mSoftExpiration, NONE,
      GetSoftExpiration, SetSoftExpiration);
  SUBSCRIPTION_PROPERTY(uint64_t, mLastDownload, SUBSCRIPTION_LASTDOWNLOAD,
      GetLastDownload, SetLastDownload);
  SUBSCRIPTION_STRING_PROPERTY(mDownloadStatus, SUBSCRIPTION_DOWNLOADSTATUS,
      GetDownloadStatus, SetDownloadStatus);
  SUBSCRIPTION_PROPERTY(uint64_t, mLastSuccess, NONE,
      GetLastSuccess, SetLastSuccess);
  SUBSCRIPTION_PROPERTY(int, mErrorCount, SUBSCRIPTION_ERRORS,
      GetErrorCount, SetErrorCount);
  SUBSCRIPTION_PROPERTY(uint64_t, mDataRevision, NONE,
      GetDataRevision, SetDataRevision);
  SUBSCRIPTION_STRING_PROPERTY(mRequiredVersion, NONE,
      GetRequiredVersion, SetRequiredVersion);
  SUBSCRIPTION_PROPERTY(int, mDownloadCount, NONE,
      GetDownloadCount, SetDownloadCount);

  static DownloadableSubscription_Parser* BINDINGS_EXPORTED ParseDownload();
  OwnedString BINDINGS_EXPORTED Serialize() const;
};

typedef intrusive_ptr<DownloadableSubscription> DownloadableSubscriptionPtr;

class DownloadableSubscription_Parser : public ref_counted
{
  std::vector<OwnedString> mFiltersText;
  OwnedStringMap<OwnedString> mParams;
  bool mFirstLine;
public:
  DownloadableSubscription_Parser();
  void BINDINGS_EXPORTED Process(const String& line);
  // return the expiration interval.
  int64_t BINDINGS_EXPORTED Finalize(DownloadableSubscription&);
  const String& BINDINGS_EXPORTED GetRedirect() const;
  const String& BINDINGS_EXPORTED GetHomepage() const;
private:
  static int64_t ParseExpires(const String& expires);
};

ABP_NS_END
