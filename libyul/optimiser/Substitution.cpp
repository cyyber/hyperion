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
 * Specific AST copier that replaces certain identifiers with expressions.
 */

#include <libyul/optimiser/Substitution.h>

#include <libyul/AST.h>

using namespace hyperion;
using namespace hyperion::yul;

Expression Substitution::translate(Expression const& _expression)
{
	if (std::holds_alternative<Identifier>(_expression))
	{
		YulString name = std::get<Identifier>(_expression).name;
		if (m_substitutions.count(name))
			// No recursive substitution
			return ASTCopier().translate(*m_substitutions.at(name));
	}
	return ASTCopier::translate(_expression);
}
