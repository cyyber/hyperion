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
 * Hyperion and Yul both share the same Token (and Scanner) API.
 *
 * This may (or may not) change in the future. But for the time being, we've put both
 * at a shared place, and *just* import them.
*/
#pragma once

#include <liblangutil/Token.h>

namespace hyperion::frontend
{
namespace TokenTraits = langutil::TokenTraits;

using langutil::Token;
using langutil::ElementaryTypeNameToken;
}
