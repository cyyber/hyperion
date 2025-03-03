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

#pragma once

#include <liblangutil/Exceptions.h>

#include <functional>
#include <string>

namespace hyperion::frontend
{

class ReadCallback
{
public:
	/// Noncopyable.
	ReadCallback(ReadCallback const&) = delete;
	ReadCallback& operator=(ReadCallback const&) = delete;

	/// File reading or generic query result.
	struct Result
	{
		bool success;
		std::string responseOrErrorMessage;
	};

	enum class Kind
	{
		ReadFile,
		SMTQuery
	};

	static std::string kindString(Kind _kind)
	{
		switch (_kind)
		{
		case Kind::ReadFile:
			return "source";
		case Kind::SMTQuery:
			return "smt-query";
		default:
			hypAssert(false, "");
		}
	}

	/// File reading or generic query callback.
	using Callback = std::function<Result(std::string const&, std::string const&)>;
};

}
