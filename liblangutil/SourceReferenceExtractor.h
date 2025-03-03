/*
	This file is part of hyperion.

	hyperion is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	hyperion is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with hyperion.  If not, see <http://www.gnu.org/licenses/>.
*/
// SPDX-License-Identifier: GPL-3.0
#pragma once

#include <liblangutil/Exceptions.h>

#include <iosfwd>
#include <optional>
#include <string>
#include <tuple>
#include <vector>
#include <variant>

namespace hyperion::langutil
{

class CharStreamProvider;

struct SourceReference
{
	std::string message;      ///< A message that relates to this source reference (such as a warning, info or an error message).
	std::string sourceName;   ///< Underlying source name (for example the filename).
	LineColumn position;      ///< Actual (error) position this source reference is surrounding.
	bool multiline = {false}; ///< Indicates whether the actual SourceReference is truncated to one line.
	std::string text;         ///< Extracted source code text (potentially truncated if multiline or too long).
	int startColumn = {-1};   ///< Highlighting range-start of text field.
	int endColumn = {-1};     ///< Highlighting range-end of text field.

	/// Constructs a SourceReference containing a message only.
	static SourceReference MessageOnly(std::string _msg, std::string _sourceName = {})
	{
		SourceReference sref;
		sref.message = std::move(_msg);
		sref.sourceName = std::move(_sourceName);
		return sref;
	}
};

namespace SourceReferenceExtractor
{
	struct Message
	{
		SourceReference primary;
		std::variant<Error::Type, Error::Severity> _typeOrSeverity;
		std::vector<SourceReference> secondary;
		std::optional<ErrorId> errorId;
	};

	Message extract(
		CharStreamProvider const& _charStreamProvider,
		util::Exception const& _exception,
		std::variant<Error::Type, Error::Severity> _typeOrSeverity
	);
	Message extract(
		CharStreamProvider const& _charStreamProvider,
		Error const& _error,
		std::variant<Error::Type, Error::Severity> _typeOrSeverity
	);
	Message extract(CharStreamProvider const& _charStreamProvider, Error const& _error);
	SourceReference extract(CharStreamProvider const& _charStreamProvider, SourceLocation const* _location, std::string message = "");
}

}
