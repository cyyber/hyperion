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

#include <liblangutil/Exceptions.h>

#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string.hpp>

#include <iostream>
#include <limits>

using namespace hyperion;
using namespace hyperion::langutil;

namespace
{

int parseSourceLocationField(std::string const& _field, std::string const& _value)
{
	astAssert(!_value.empty(), "SourceLocation field '" + _field + "' must be an integer.");

	size_t pos = 0;
	bool const negative = _value[pos] == '-';
	if (negative)
		++pos;

	astAssert(pos < _value.size(), "SourceLocation field '" + _field + "' must be an integer.");

	long long const limit =
		negative ?
		static_cast<long long>(std::numeric_limits<int>::max()) + 1 :
		std::numeric_limits<int>::max();
	long long value = 0;
	for (; pos < _value.size(); ++pos)
	{
		astAssert(
			'0' <= _value[pos] && _value[pos] <= '9',
			"SourceLocation field '" + _field + "' must be an integer."
		);
		value = value * 10 + (_value[pos] - '0');
		astAssert(value <= limit, "SourceLocation field '" + _field + "' is out of range.");
	}

	if (!negative)
		return static_cast<int>(value);
	if (value == limit)
		return std::numeric_limits<int>::min();
	return static_cast<int>(-value);
}

}

SourceLocation hyperion::langutil::parseSourceLocation(std::string const& _input, std::vector<std::shared_ptr<std::string const>> const& _sourceNames)
{
	// Expected input: "start:length:sourceindex"
	enum SrcElem: size_t { Start, Length, Index };

	std::vector<std::string> pos;

	boost::algorithm::split(pos, _input, boost::is_any_of(":"));

	astAssert(pos.size() == 3, "SourceLocation string must have 3 colon separated numeric fields.");
	int const sourceIndex = parseSourceLocationField("source index", pos[Index]);

	astAssert(
		sourceIndex == -1 || (0 <= sourceIndex && static_cast<size_t>(sourceIndex) < _sourceNames.size()),
		"'src'-field ill-formatted or src-index too high"
	);

	int const start = parseSourceLocationField("start", pos[Start]);
	int const length = parseSourceLocationField("length", pos[Length]);
	long long const end = static_cast<long long>(start) + length;
	astAssert(
		std::numeric_limits<int>::min() <= end && end <= std::numeric_limits<int>::max(),
		"'src'-field ill-formatted or source location range too large"
	);

	SourceLocation result{start, static_cast<int>(end), {}};
	if (sourceIndex != -1)
		result.sourceName = _sourceNames.at(static_cast<size_t>(sourceIndex));
	return result;
}

std::ostream& hyperion::langutil::operator<<(std::ostream& _out, SourceLocation const& _location)
{
	if (!_location.isValid())
		return _out << "NO_LOCATION_SPECIFIED";

	if (_location.sourceName)
		_out << *_location.sourceName;

	_out << "[" << _location.start << "," << _location.end << "]";

	return _out;
}
