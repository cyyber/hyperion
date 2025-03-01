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


#include <libsmtutil/Sorts.h>

namespace hyperion::smtutil
{

std::shared_ptr<Sort> const SortProvider::boolSort{std::make_shared<Sort>(Kind::Bool)};
std::shared_ptr<IntSort> const SortProvider::uintSort{std::make_shared<IntSort>(false)};
std::shared_ptr<IntSort> const SortProvider::sintSort{std::make_shared<IntSort>(true)};

std::shared_ptr<IntSort> SortProvider::intSort(bool _signed)
{
	if (_signed)
		return sintSort;
	return uintSort;
}

std::shared_ptr<BitVectorSort> const SortProvider::bitVectorSort{std::make_shared<BitVectorSort>(256)};

}
