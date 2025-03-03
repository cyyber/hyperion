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

#include <libyul/optimiser/ForLoopConditionOutOfBody.h>
#include <libyul/optimiser/Semantics.h>
#include <libyul/AST.h>
#include <libyul/Utilities.h>

#include <libhyputil/CommonData.h>

using namespace hyperion;
using namespace hyperion::yul;

void ForLoopConditionOutOfBody::run(OptimiserStepContext& _context, Block& _ast)
{
	ForLoopConditionOutOfBody{_context.dialect}(_ast);
}

void ForLoopConditionOutOfBody::operator()(ForLoop& _forLoop)
{
	ASTModifier::operator()(_forLoop);

	if (
		!m_dialect.booleanNegationFunction() ||
		!std::holds_alternative<Literal>(*_forLoop.condition) ||
		valueOfLiteral(std::get<Literal>(*_forLoop.condition)) == u256(0) ||
		_forLoop.body.statements.empty() ||
		!std::holds_alternative<If>(_forLoop.body.statements.front())
	)
		return;

	If& firstStatement = std::get<If>(_forLoop.body.statements.front());
	if (
		firstStatement.body.statements.empty() ||
		!std::holds_alternative<Break>(firstStatement.body.statements.front())
	)
		return;
	if (!SideEffectsCollector(m_dialect, *firstStatement.condition).movable())
		return;

	YulString iszero = m_dialect.booleanNegationFunction()->name;
	std::shared_ptr<DebugData const> debugData = debugDataOf(*firstStatement.condition);

	if (
		std::holds_alternative<FunctionCall>(*firstStatement.condition) &&
		std::get<FunctionCall>(*firstStatement.condition).functionName.name == iszero
	)
		_forLoop.condition = std::make_unique<Expression>(std::move(std::get<FunctionCall>(*firstStatement.condition).arguments.front()));
	else
		_forLoop.condition = std::make_unique<Expression>(FunctionCall{
			debugData,
			Identifier{debugData, iszero},
			util::make_vector<Expression>(
				std::move(*firstStatement.condition)
			)
		});

	_forLoop.body.statements.erase(_forLoop.body.statements.begin());
}

