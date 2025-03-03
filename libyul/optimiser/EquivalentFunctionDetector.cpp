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

#include <libyul/optimiser/EquivalentFunctionDetector.h>
#include <libyul/optimiser/SyntacticalEquality.h>

#include <libyul/AST.h>
#include <libyul/optimiser/Metrics.h>

using namespace hyperion;
using namespace hyperion::yul;

void EquivalentFunctionDetector::operator()(FunctionDefinition const& _fun)
{
	uint64_t bodyHash = m_blockHashes[&_fun.body];
	auto& candidates = m_candidates[bodyHash];
	for (auto const& candidate: candidates)
		if (SyntacticallyEqual{}.statementEqual(_fun, *candidate))
		{
			m_duplicates[_fun.name] = candidate;
			return;
		}
	candidates.push_back(&_fun);
}
