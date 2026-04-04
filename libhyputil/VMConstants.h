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
/**
 * Constants defining the QRVM word size and related values.
 * Changing VMWordBytes from 32 to 64 is the single point of change
 * needed to switch the VM word size.
 */

#pragma once

#include <cstddef>

namespace hyperion
{

/// VM word size in bytes. Currently 32 (256-bit), target: 64 (512-bit).
static constexpr unsigned VMWordBytes = 32;
/// VM word size in bits.
static constexpr unsigned VMWordBits = VMWordBytes * 8;
/// Hex representation of VM word size (used in Yul templates as 0x20 or 0x40).
static constexpr unsigned VMWordHex = VMWordBytes;
/// Alignment mask for rounding up to VM word boundary: (value + VMWordAlignmentMask) & ~VMWordAlignmentMask
static constexpr unsigned VMWordAlignmentMask = VMWordBytes - 1;

}
