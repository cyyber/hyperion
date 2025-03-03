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
 * Information generated during analyzer part of inline assembly.
 */

#pragma once

#include <libyul/ASTForward.h>

#include <map>
#include <memory>
#include <vector>

namespace hyperion::yul
{

struct Scope;

struct AsmAnalysisInfo
{
	using StackHeightInfo = std::map<void const*, int>;
	using Scopes = std::map<Block const*, std::shared_ptr<Scope>>;
	Scopes scopes;
	/// Virtual blocks which will be used for scopes for function arguments and return values.
	std::map<FunctionDefinition const*, std::shared_ptr<Block const>> virtualBlocks;
};

}
