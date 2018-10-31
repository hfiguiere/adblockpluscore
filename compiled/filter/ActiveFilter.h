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

#include "../base.h"
#include "Filter.h"
#include "../StringMap.h"
#include "../FilterNotifier.h"
#include "../bindings/runtime.h"

#define FILTER_PROPERTY(type, name, topic, getter, setter) \
    private:\
      type name;\
    public:\
      type BINDINGS_EXPORTED getter() const\
      {\
        return name;\
      }\
      void BINDINGS_EXPORTED setter(type value)\
      {\
        if (name != value)\
        {\
          name = value;\
          if (ABP_NS::FilterNotifier::Topic::topic != ABP_NS::FilterNotifier::Topic::NONE)\
          {\
            ABP_NS::FilterNotifier::FilterChange(ABP_NS::FilterNotifier::Topic::topic, *this);\
          }\
        }\
      }


ABP_NS_BEGIN

struct ParsedDomains
{
  struct Domain {
    String::size_type pos;
    String::size_type len;
    bool reverse;
  };
  bool hasIncludes;
  bool hasEmpty;
  std::vector<Domain> domains;

  ParsedDomains()
    : hasIncludes(false), hasEmpty(false)
  {
  }
};

class ActiveFilter : public Filter
{
public:
  typedef StringMap<bool> DomainMap;
  virtual DomainMap* GetDomains() const;
  static const DependentString DEFAULT_DOMAIN;
protected:
  typedef StringSet SitekeySet;
  static ParsedDomains ParseDomainsInternal(const String& domains,
      String::value_type separator, bool ignoreTrailingDot);
  void FillDomains(const String& domains, const ParsedDomains& parsed) const;
  void ParseDomains(const String& domains,
      String::value_type separator, bool ignoreTrailingDot) const;
  void AddSitekey(const String& sitekey) const;
  virtual SitekeySet* GetSitekeys() const;
  mutable std::unique_ptr<DomainMap> mDomains;
  mutable std::unique_ptr<SitekeySet> mSitekeys;
private:
  bool mIgnoreTrailingDot;
public:
  static constexpr Type classType = Type::ACTIVE;
  explicit ActiveFilter(Type type, const String& text, bool ignoreTrailingDot);
  FILTER_PROPERTY(bool, mDisabled, FILTER_DISABLED, GetDisabled, SetDisabled);
  FILTER_PROPERTY(unsigned int, mHitCount, FILTER_HITCOUNT,
      GetHitCount, SetHitCount);
  FILTER_PROPERTY(unsigned int, mLastHit, FILTER_LASTHIT,
      GetLastHit, SetLastHit);
  bool BINDINGS_EXPORTED IsActiveOnDomain(DependentString& docDomain,
      const String& sitekey = DependentString()) const;
  bool BINDINGS_EXPORTED IsActiveOnlyOnDomain(DependentString& docDomain) const;
  bool BINDINGS_EXPORTED IsGeneric() const;
  OwnedString BINDINGS_EXPORTED Serialize() const;
};

typedef intrusive_ptr<ActiveFilter> ActiveFilterPtr;

ABP_NS_END
