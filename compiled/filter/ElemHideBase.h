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

#include <cstddef>

#include "../base.h"
#include "ActiveFilter.h"
#include "../bindings/runtime.h"

ABP_NS_BEGIN

struct ElemHideData
{
  String::size_type mDomainsEnd;
  String::size_type mSelectorStart;

  bool HasDomains() const
  {
    return mDomainsEnd != 0;
  }

  DependentString GetDomainsSource(String& text) const
  {
    return DependentString(text, 0, mDomainsEnd);
  }

  const DependentString GetDomainsSource(const String& text) const
  {
    return DependentString(text, 0, mDomainsEnd);
  }

  DependentString GetSelector(String& text) const
  {
    return DependentString(text, mSelectorStart);
  }

  const DependentString GetSelector(const String& text) const
  {
    return DependentString(text, mSelectorStart);
  }
};

class ElemHideBase : public ActiveFilter
{
protected:
  ElemHideData mData;
public:
  static constexpr Type classType = Type::ELEMHIDEBASE;
  explicit ElemHideBase(Type type, const String& text, const ElemHideData& data,
    const ParsedDomains& parsedDomains);
  static Type Parse(DependentString& text, DependentString& error,
    ElemHideData& data, bool& needConversion, ParsedDomains& parsedDomains);
  static DependentString ConvertFilter(String& text, String::size_type& at);

  OwnedString BINDINGS_EXPORTED GetSelector() const;
  OwnedString BINDINGS_EXPORTED GetSelectorDomain() const;
};

typedef intrusive_ptr<ElemHideBase> ElemHideBasePtr;

ABP_NS_END