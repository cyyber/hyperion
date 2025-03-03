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
/** @file ErrorCheck.h
 * @author Yoichi Hirai <i@yoichihirai.com>
 * @date 2016
 */

#pragma once

#include <liblangutil/Exceptions.h>

#include <vector>
#include <tuple>

namespace hyperion::frontend::test
{

bool searchErrorMessage(langutil::Error const& _err, std::string const& _substr);

/// Checks that all provided errors are of the given type and have a given substring in their
/// description.
/// If the expectations are not met, returns a nonempty description, otherwise an empty string.
std::string searchErrors(langutil::ErrorList const& _errors, std::vector<std::pair<langutil::Error::Type, std::string>> const& _expectations);

}
