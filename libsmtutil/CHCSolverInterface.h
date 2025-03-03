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
 * Interface for constrained Horn solvers.
 */

#pragma once

#include <libsmtutil/SolverInterface.h>

#include <map>
#include <vector>

namespace hyperion::smtutil
{

class CHCSolverInterface
{
public:
	CHCSolverInterface(std::optional<unsigned> _queryTimeout = {}): m_queryTimeout(_queryTimeout) {}

	virtual ~CHCSolverInterface() = default;

	virtual void declareVariable(std::string const& _name, SortPointer const& _sort) = 0;

	/// Takes a function declaration as a relation.
	virtual void registerRelation(Expression const& _expr) = 0;

	/// Takes an implication and adds as rule.
	/// Needs to bound all vars as universally quantified.
	virtual void addRule(Expression const& _expr, std::string const& _name) = 0;

	using CexNode = Expression;
	struct CexGraph
	{
		std::map<unsigned, CexNode> nodes;
		std::map<unsigned, std::vector<unsigned>> edges;
	};

	/// Takes a function application _expr and checks for reachability.
	/// @returns solving result, an invariant, and counterexample graph, if possible.
	virtual std::tuple<CheckResult, Expression, CexGraph> query(
		Expression const& _expr
	) = 0;

protected:
	std::optional<unsigned> m_queryTimeout;
};

}
