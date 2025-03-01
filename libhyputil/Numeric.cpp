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

#include <libhyputil/Numeric.h>

#include <liblangutil/Exceptions.h>

using namespace hyperion;

bool hyperion::fitsPrecisionBaseX(bigint const& _mantissa, double _log2OfBase, uint32_t _exp)
{
	if (_mantissa == 0)
		return true;

	hypAssert(_mantissa > 0, "");

	size_t const bitsMax = 4096;

	size_t mostSignificantMantissaBit = static_cast<size_t>(boost::multiprecision::msb(_mantissa));
	if (mostSignificantMantissaBit > bitsMax) // _mantissa >= 2 ^ 4096
		return false;

	bigint bitsNeeded = mostSignificantMantissaBit + bigint(floor(double(_exp) * _log2OfBase)) + 1;
	return bitsNeeded <= bitsMax;
}
