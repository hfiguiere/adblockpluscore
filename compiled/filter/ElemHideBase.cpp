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

#include <cstring>

#include "ElemHideBase.h"
#include "../StringScanner.h"
#include "../Utils.h"

ABP_NS_USING

namespace
{
  void NormalizeWhitespace(DependentString& text, String::size_type& domainsEnd,
      String::size_type& selectorStart)
  {
    // For element hiding filters we only want to remove spaces preceding the
    // selector part. The positions we've determined already have to be adjusted
    // accordingly.

    String::size_type delta = 0;
    String::size_type len = text.length();

    // The first character is guaranteed to be a non-space, the string has been
    // trimmed earlier.
    for (String::size_type pos = 1; pos < len; pos++)
    {
      if (pos == domainsEnd)
        domainsEnd -= delta;

      // Only spaces before selectorStart position should be removed.
      if (pos < selectorStart && text[pos] == ABP_TEXT(' '))
        delta++;
      else
        text[pos - delta] = text[pos];
    }
    selectorStart -= delta;

    text.reset(text, 0, len - delta);
  }
}

ElemHideBase::ElemHideBase(Type type, const String& text, const ElemHideData& data, const ParsedDomains& parsedDomains)
    : ActiveFilter(type, text, false), mData(data)
{
  if (mData.HasDomains())
    FillDomains(mData.GetDomainsSource(mText), parsedDomains);
}

Filter::Type ElemHideBase::Parse(DependentString& text, DependentString& error,
                                 ElemHideData& data,
                                 ParsedDomains& parsedDomains)
{
  StringScanner scanner(text);

  // Domains part
  bool seenSpaces = false;
  while (!scanner.done())
  {
    String::value_type next = scanner.next();
    if (next == ABP_TEXT('#'))
    {
      data.mDomainsEnd = scanner.position();
      break;
    }

    switch (next)
    {
      case ABP_TEXT('/'):
      case ABP_TEXT('*'):
      case ABP_TEXT('|'):
      case ABP_TEXT('@'):
      case ABP_TEXT('"'):
      case ABP_TEXT('!'):
        return Type::UNKNOWN;
      case ABP_TEXT(' '):
        seenSpaces = true;
        break;
    }
  }

  seenSpaces |= scanner.skip(ABP_TEXT(' '));
  bool emulation = false;
  bool exception = scanner.skipOne(ABP_TEXT('@'));
  if (exception)
    seenSpaces |= scanner.skip(ABP_TEXT(' '));
  else
    emulation = scanner.skipOne(ABP_TEXT('?'));

  String::value_type next = scanner.next();
  if (next != ABP_TEXT('#'))
    return Type::UNKNOWN;

  // Selector part

  // Selector shouldn't be empty
  seenSpaces |= scanner.skip(ABP_TEXT(' '));
  if (scanner.done())
    return Type::UNKNOWN;

  data.mSelectorStart = scanner.position() + 1;

  // We are done validating, now we can normalize whitespace and the domain part
  if (seenSpaces)
    NormalizeWhitespace(text, data.mDomainsEnd, data.mSelectorStart);
  DependentString(text, 0, data.mDomainsEnd).toLower();

  parsedDomains =
    ParseDomainsInternal(data.GetDomainsSource(text), ABP_TEXT(','), false);
  if (parsedDomains.hasEmpty)
  {
    error = ABP_TEXT("filter_invalid_domain"_str);
    return Type::INVALID;
  }

  if (exception)
    return Type::ELEMHIDEEXCEPTION;

  if (emulation)
    return Type::ELEMHIDEEMULATION;

  return Type::ELEMHIDE;
}

namespace
{
  static constexpr String::value_type OPENING_CURLY_REPLACEMENT[] = ABP_TEXT("\\7B ");
  static constexpr String::value_type CLOSING_CURLY_REPLACEMENT[] = ABP_TEXT("\\7D ");
  static constexpr String::size_type CURLY_REPLACEMENT_SIZE = str_length_of(OPENING_CURLY_REPLACEMENT);

  OwnedString EscapeCurlies(String::size_type replacementCount,
                            const DependentString& str)
  {
    OwnedString result(str.length() + replacementCount * (CURLY_REPLACEMENT_SIZE - 1));

    String::value_type* current = result.data();
    for (String::size_type i = 0; i < str.length(); i++)
    {
      switch(str[i])
      {
      case ABP_TEXT('}'):
        std::memcpy(current, CLOSING_CURLY_REPLACEMENT,
                    sizeof(String::value_type) * CURLY_REPLACEMENT_SIZE);
        current += CURLY_REPLACEMENT_SIZE;
        break;
      case ABP_TEXT('{'):
        std::memcpy(current, OPENING_CURLY_REPLACEMENT,
                    sizeof(String::value_type) * CURLY_REPLACEMENT_SIZE);
        current += CURLY_REPLACEMENT_SIZE;
        break;
      default:
        *current = str[i];
        current++;
        break;
      }
    }

    return result;
  }
}

OwnedString ElemHideBase::GetSelector() const
{
  const DependentString selector = mData.GetSelector(mText);
  String::size_type replacementCount = 0;
  for (String::size_type i = 0; i < selector.length(); i++)
    if (selector[i] == ABP_TEXT('}') || selector[i] == ABP_TEXT('{'))
      replacementCount++;
  if (replacementCount)
    return EscapeCurlies(replacementCount, selector);

  return OwnedString(selector);
}

OwnedString ElemHideBase::GetSelectorDomain() const
{
  /* TODO this is inefficient */
  OwnedString result;
  if (mDomains)
  {
    for (const auto& item : *mDomains)
    {
      if (item.second && !item.first.empty())
      {
        if (!result.empty())
          result.append(ABP_TEXT(','));
        result.append(item.first);
      }
    }
  }
  return result;
}
