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
 * Optimiser component that combines syntactically equivalent functions.
 */

#include <libyul/optimiser/EquivalentFunctionCombiner.h>
#include <libyul/AST.h>
#include <libhyputil/CommonData.h>

using namespace hyperion;
using namespace hyperion::yul;

void EquivalentFunctionCombiner::run(OptimiserStepContext&, Block& _ast)
{
	EquivalentFunctionCombiner{EquivalentFunctionDetector::run(_ast)}(_ast);
}

void EquivalentFunctionCombiner::operator()(FunctionCall& _funCall)
{
	auto it = m_duplicates.find(_funCall.functionName.name);
	if (it != m_duplicates.end())
		_funCall.functionName.name = it->second->name;
	ASTModifier::operator()(_funCall);
}
