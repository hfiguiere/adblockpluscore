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

#include "Filter.h"
#include "CommentFilter.h"
#include "InvalidFilter.h"
#include "RegExpFilter.h"
#include "BlockingFilter.h"
#include "WhitelistFilter.h"
#include "ElemHideBase.h"
#include "ElemHideFilter.h"
#include "ElemHideException.h"
#include "ElemHideEmulationFilter.h"
#include "../StringMap.h"

ABP_NS_USING

namespace
{
  StringMap<Filter*> knownFilters(8192);

  void NormalizeWhitespace(DependentString& text)
  {
    String::size_type start = 0;
    String::size_type end = text.length();

    // Remove leading spaces and special characters like line breaks
    for (; start < end; start++)
      if (text[start] > ABP_TEXT(' '))
        break;

    // Now look for invalid characters inside the string
    String::size_type pos;
    for (pos = start; pos < end; pos++)
      if (text[pos] < ABP_TEXT(' '))
        break;

    if (pos < end)
    {
      // Found invalid characters, copy all the valid characters while skipping
      // the invalid ones.
      String::size_type delta = 1;
      for (pos = pos + 1; pos < end; pos++)
      {
        if (text[pos] < ABP_TEXT(' '))
          delta++;
        else
          text[pos - delta] = text[pos];
      }
      end -= delta;
    }

    // Remove trailing spaces
    for (; end > 0; end--)
      if (text[end - 1] != ABP_TEXT(' '))
        break;

    // Set new string boundaries
    text.reset(text, start, end - start);
  }
}

Filter::Filter(Type type, const String& text)
    : mText(text), mType(type)
{
  annotate_address(this, "Filter");
}

Filter::~Filter()
{
  knownFilters.erase(mText);
}

OwnedString Filter::Serialize() const
{
  OwnedString result(ABP_TEXT("[Filter]\ntext="_str));
  result.append(mText);
  result.append(ABP_TEXT('\n'));
  return result;
}

Filter* Filter::FromText(DependentString& text)
{
  NormalizeWhitespace(text);
  if (text.empty())
    return nullptr;

  // Parsing also normalizes the filter text, so it has to be done before the
  // lookup in knownFilters.
  union
  {
    RegExpFilterData regexp;
    ElemHideData elemhide;
  } data;
  ParsedDomains parsedDomains;
  DependentString error;

  Filter::Type type = CommentFilter::Parse(text);
  if (type == Filter::Type::UNKNOWN)
    type = ElemHideBase::Parse(text, error, data.elemhide, parsedDomains);
  if (type == Filter::Type::UNKNOWN)
    type = RegExpFilter::Parse(text, error, data.regexp);

  auto knownFilter = knownFilters.find(text);
  if (knownFilter)
  {
    knownFilter->second->AddRef();
    return knownFilter->second;
  }

  FilterPtr filter;
  switch (type)
  {
    case CommentFilter::classType:
      filter = FilterPtr(new CommentFilter(text), false);
      break;
    case InvalidFilter::classType:
      filter = FilterPtr(new InvalidFilter(text, error), false);
      break;
    case BlockingFilter::classType:
      filter = FilterPtr(new BlockingFilter(text, data.regexp), false);
      break;
    case WhitelistFilter::classType:
      filter = FilterPtr(new WhitelistFilter(text, data.regexp), false);
      break;
    case ElemHideFilter::classType:
      filter = FilterPtr(new ElemHideFilter(text, data.elemhide,
        parsedDomains), false);
      break;
    case ElemHideException::classType:
      filter = FilterPtr(new ElemHideException(text, data.elemhide, parsedDomains), false);
      break;
    case ElemHideEmulationFilter::classType:
      filter = FilterPtr(new ElemHideEmulationFilter(text, data.elemhide, parsedDomains), false);
      if (static_cast<ElemHideEmulationFilter*>(filter.get())->IsGeneric())
        filter = FilterPtr(new InvalidFilter(text, ABP_TEXT("filter_elemhideemulation_nodomain"_str)), false);
      break;
    default:
      // This should never happen but just in case
      return nullptr;
  }

  enter_context("Adding to known filters");
  if (text != filter->mText)
    knownFilters[filter->mText] = filter.get();
  else
    // This is a hack: we looked up the entry using text but create it using
    // filter->mText. This works because both are equal at this point. However,
    // text refers to a temporary buffer which will go away.
    knownFilter.assign(filter->mText, filter.get());
  exit_context();

  return filter.release();
}
