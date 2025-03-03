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
 * Scopes for identifiers.
 */

#pragma once

#include <liblangutil/Exceptions.h>

#include <libyul/YulString.h>

#include <functional>
#include <memory>
#include <optional>
#include <variant>

namespace hyperion::yul
{

struct Scope
{
	using YulType = YulString;

	struct Variable
	{
		YulType type;
		YulString name;
	};
	struct Function
	{
		std::vector<YulType> arguments;
		std::vector<YulType> returns;
		YulString name;
	};

	using Identifier = std::variant<Variable, Function>;

	bool registerVariable(YulString _name, YulType const& _type);
	bool registerFunction(
		YulString _name,
		std::vector<YulType> _arguments,
		std::vector<YulType> _returns
	);

	/// Looks up the identifier in this or super scopes and returns a valid pointer if found
	/// or a nullptr if not found. Variable lookups up across function boundaries will fail, as
	/// will any lookups across assembly boundaries.
	/// The pointer will be invalidated if the scope is modified.
	/// @param _crossedFunction if true, we already crossed a function boundary during recursive lookup
	Identifier* lookup(YulString _name);
	/// Looks up the identifier in this and super scopes (will not find variables across function
	/// boundaries and generally stops at assembly boundaries) and calls the visitor, returns
	/// false if not found.
	template <class V>
	bool lookup(YulString _name, V const& _visitor)
	{
		if (Identifier* id = lookup(_name))
		{
			std::visit(_visitor, *id);
			return true;
		}
		else
			return false;
	}
	/// @returns true if the name exists in this scope or in super scopes (also searches
	/// across function and assembly boundaries).
	bool exists(YulString _name) const;

	/// @returns the number of variables directly registered inside the scope.
	size_t numberOfVariables() const;
	/// @returns true if this scope is inside a function.
	bool insideFunction() const;

	Scope* superScope = nullptr;
	/// If true, variables from the super scope are not visible here (other identifiers are),
	/// but they are still taken into account to prevent shadowing.
	bool functionScope = false;
	std::map<YulString, Identifier> identifiers;
};

}
