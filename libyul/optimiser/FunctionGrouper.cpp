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
 * statements are moved to a block of their own followed by all function definitions.
 */

#include <libyul/optimiser/FunctionGrouper.h>

#include <libyul/AST.h>

using namespace hyperion;
using namespace hyperion::yul;


void FunctionGrouper::operator()(Block& _block)
{
	if (alreadyGrouped(_block))
		return;

	std::vector<Statement> reordered;
	reordered.emplace_back(Block{_block.debugData, {}});

	for (auto&& statement: _block.statements)
	{
		if (std::holds_alternative<FunctionDefinition>(statement))
			reordered.emplace_back(std::move(statement));
		else
			std::get<Block>(reordered.front()).statements.emplace_back(std::move(statement));
	}
	_block.statements = std::move(reordered);
}

bool FunctionGrouper::alreadyGrouped(Block const& _block)
{
	if (_block.statements.empty())
		return false;
	if (!std::holds_alternative<Block>(_block.statements.front()))
		return false;
	for (size_t i = 1; i < _block.statements.size(); ++i)
		if (!std::holds_alternative<FunctionDefinition>(_block.statements.at(i)))
			return false;
	return true;
}
