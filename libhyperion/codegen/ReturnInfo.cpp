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

#include <libhyperion/codegen/ReturnInfo.h>

#include <libhyperion/ast/Types.h>
#include <libhyperion/ast/AST.h>

using namespace hyperion::frontend;
using namespace hyperion::langutil;

ReturnInfo::ReturnInfo(FunctionType const& _functionType)
{
	FunctionType::Kind const funKind = _functionType.kind();
	bool const returnSuccessConditionAndReturndata =
		funKind == FunctionType::Kind::BareCall ||
		funKind == FunctionType::Kind::BareDelegateCall ||
		funKind == FunctionType::Kind::BareStaticCall;

	if (!returnSuccessConditionAndReturndata)
	{
		returnTypes = _functionType.returnParameterTypes();
		for (auto const& retType: returnTypes)
		{
			hypAssert(retType->decodingType(), "");
			if (retType->decodingType()->isDynamicallyEncoded())
			{
				dynamicReturnSize = true;
				estimatedReturnSize = 0;
				break;
			}
			else
				estimatedReturnSize += retType->decodingType()->calldataEncodedSize();
		}
	}
	if (dynamicReturnSize)
		hypAssert(estimatedReturnSize == 0);
}
