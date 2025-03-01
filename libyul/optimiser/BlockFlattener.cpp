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

#include <libyul/optimiser/BlockFlattener.h>
#include <libyul/AST.h>

#include <libhyputil/CommonData.h>

#include <functional>

using namespace hyperion;
using namespace hyperion::yul;
using namespace hyperion::util;

void BlockFlattener::operator()(Block& _block)
{
	ASTModifier::operator()(_block);

	iterateReplacing(
		_block.statements,
		[](Statement& _s) -> std::optional<std::vector<Statement>>
		{
			if (std::holds_alternative<Block>(_s))
				return std::move(std::get<Block>(_s).statements);
			else
				return {};
		}
	);
}

void BlockFlattener::run(OptimiserStepContext&, Block& _ast)
{
	BlockFlattener flattener;
	for (auto& statement: _ast.statements)
		if (auto* block = std::get_if<Block>(&statement))
			flattener(*block);
		else if (auto* function = std::get_if<FunctionDefinition>(&statement))
			flattener(function->body);
		else
			yulAssert(false, "BlockFlattener requires the FunctionGrouper.");
}
