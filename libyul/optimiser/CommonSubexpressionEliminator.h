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
 * Optimisation stage that replaces expressions known to be the current value of a variable
 * in scope by a reference to that variable.
 */

#pragma once

#include <libyul/optimiser/DataFlowAnalyzer.h>
#include <libyul/optimiser/OptimiserStep.h>
#include <libyul/optimiser/SyntacticalEquality.h>
#include <libyul/optimiser/BlockHasher.h>

#include <set>

namespace hyperion::yul
{

struct Dialect;
struct SideEffects;

/**
 * Optimisation stage that replaces expressions known to be the current value of a variable
 * in scope by a reference to that variable.
 *
 * Prerequisite: Disambiguator, ForLoopInitRewriter.
 */
class CommonSubexpressionEliminator: public DataFlowAnalyzer
{
public:
	static constexpr char const* name{"CommonSubexpressionEliminator"};
	static void run(OptimiserStepContext&, Block& _ast);

	using DataFlowAnalyzer::operator();
	void operator()(FunctionDefinition&) override;

private:
	CommonSubexpressionEliminator(
		Dialect const& _dialect,
		std::map<YulString, SideEffects> _functionSideEffects
	);

protected:
	using ASTModifier::visit;
	void visit(Expression& _e) override;

	void assignValue(YulString _variable, Expression const* _value) override;
private:
	std::set<YulString> m_returnVariables;
	std::unordered_map<
		std::reference_wrapper<Expression const>,
		std::set<YulString>,
		ExpressionHash,
		SyntacticallyEqualExpression
	> m_replacementCandidates;
};


}
