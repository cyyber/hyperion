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
 * Optimiser component that uses the simplification rules to simplify expressions.
 */

#include <libyul/optimiser/ExpressionSimplifier.h>

#include <libyul/optimiser/SimplificationRules.h>
#include <libyul/optimiser/OptimiserStep.h>
#include <libyul/optimiser/OptimizerUtilities.h>
#include <libyul/AST.h>
#include <libyul/Utilities.h>

#include <libzvmasm/SemanticInformation.h>

using namespace hyperion;
using namespace hyperion::yul;

void ExpressionSimplifier::run(OptimiserStepContext& _context, Block& _ast)
{
	ExpressionSimplifier{_context.dialect}(_ast);
}

void ExpressionSimplifier::visit(Expression& _expression)
{
	ASTModifier::visit(_expression);

	while (auto const* match = SimplificationRules::findFirstMatch(
		_expression,
		m_dialect,
		[this](YulString _var) { return variableValue(_var); }
	))
		_expression = match->action().toExpression(debugDataOf(_expression), zvmVersionFromDialect(m_dialect));

	if (auto* functionCall = std::get_if<FunctionCall>(&_expression))
		if (std::optional<zvmasm::Instruction> instruction = toZVMInstruction(m_dialect, functionCall->functionName.name))
			for (auto op: zvmasm::SemanticInformation::readWriteOperations(*instruction))
				if (op.startParameter && op.lengthParameter)
				{
					Expression& startArgument = functionCall->arguments.at(*op.startParameter);
					Expression const& lengthArgument = functionCall->arguments.at(*op.lengthParameter);
					if (
						knownToBeZero(lengthArgument) &&
						!knownToBeZero(startArgument) &&
						!std::holds_alternative<FunctionCall>(startArgument)
					)
						startArgument = Literal{debugDataOf(startArgument), LiteralKind::Number, "0"_yulstring, {}};
				}
}

bool ExpressionSimplifier::knownToBeZero(Expression const& _expression) const
{
	if (auto const* literal = std::get_if<Literal>(&_expression))
		return valueOfLiteral(*literal) == 0;
	else if (auto const* identifier = std::get_if<Identifier>(&_expression))
		return valueOfIdentifier(identifier->name) == 0;
	else
		return false;
}
