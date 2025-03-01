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
/** @file ErrorCheck.cpp
 * @author Yoichi Hirai <i@yoichihirai.com>
 * @date 2016
 */

#include <test/libhyperion/ErrorCheck.h>
#include <libhyputil/Exceptions.h>

#include <string>
#include <set>
#include <iostream>

using namespace hyperion;
using namespace hyperion::langutil;
using namespace hyperion::frontend;

namespace
{
std::string errorMessage(Error const& _e)
{
	return _e.comment() ? *_e.comment() : "NONE";
}
}

bool hyperion::frontend::test::searchErrorMessage(Error const& _err, std::string const& _substr)
{
	if (std::string const* errorMessage = _err.comment())
	{
		if (errorMessage->find(_substr) == std::string::npos)
		{
			std::cout << "Expected message \"" << _substr << "\" but found \"" << *errorMessage << "\".\n";
			return false;
		}
		return true;
	}
	else
		std::cout << "Expected error message but found none." << std::endl;
	return _substr.empty();
}

std::string hyperion::frontend::test::searchErrors(ErrorList const& _errors, std::vector<std::pair<Error::Type, std::string>> const& _expectations)
{
	auto expectations = _expectations;
	for (auto const& error: _errors)
	{
		std::string msg = errorMessage(*error);
		bool found = false;
		for (auto it = expectations.begin(); it != expectations.end(); ++it)
			if (msg.find(it->second) != std::string::npos && error->type() == it->first)
			{
				found = true;
				expectations.erase(it);
				break;
			}
		if (!found)
			return "Unexpected error: " + Error::formatErrorType(error->type()) + ": " + msg;
	}
	if (!expectations.empty())
	{
		std::string msg = "Expected error(s) not present:\n";
		for (auto const& expectation: expectations)
			msg += expectation.second + "\n";
		return msg;
	}

	return "";
}
