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
 * Optimiser component that changes the code of a block so that all non-function definition
 * instructions are moved to a block of their own followed by all function definitions.
 */

#pragma once

#include <libyul/ASTForward.h>

namespace hyperion::yul
{

struct OptimiserStepContext;

/**
 * Moves all instructions in a block into a new block at the start of the block, followed by
 * all function definitions.
 *
 * After this step, a block is of the form
 * { { I... } F... }
 * Where I are (non-function-definition) instructions and F are function definitions.
 */
class FunctionGrouper
{
public:
	static constexpr char const* name{"FunctionGrouper"};
	static void run(OptimiserStepContext&, Block& _ast) { FunctionGrouper{}(_ast); }

	void operator()(Block& _block);

private:
	FunctionGrouper() = default;

	bool alreadyGrouped(Block const& _block);
};

}
