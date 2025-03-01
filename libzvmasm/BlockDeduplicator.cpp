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
 * @file BlockDeduplicator.cpp
 * @author Christian <c@ethdev.com>
 * @date 2015
 * Unifies basic blocks that share content.
 */

#include <libzvmasm/BlockDeduplicator.h>

#include <libzvmasm/AssemblyItem.h>
#include <libzvmasm/SemanticInformation.h>

#include <functional>
#include <set>

using namespace hyperion;
using namespace hyperion::zvmasm;


bool BlockDeduplicator::deduplicate()
{
	// Compares indices based on the suffix that starts there, ignoring tags and stopping at
	// opcodes that stop the control flow.

	// Virtual tag that signifies "the current block" and which is used to optimise loops.
	// We abort if this virtual tag actually exists.
	AssemblyItem pushSelf{PushTag, u256(-4)};
	if (
		std::count(m_items.cbegin(), m_items.cend(), pushSelf.tag()) ||
		std::count(m_items.cbegin(), m_items.cend(), pushSelf.pushTag())
	)
		return false;

	std::function<bool(size_t, size_t)> comparator = [&](size_t _i, size_t _j)
	{
		if (_i == _j)
			return false;

		// To compare recursive loops, we have to already unify PushTag opcodes of the
		// block's own tag.
		AssemblyItem pushFirstTag{pushSelf};
		AssemblyItem pushSecondTag{pushSelf};

		if (_i < m_items.size() && m_items.at(_i).type() == Tag)
			pushFirstTag = m_items.at(_i).pushTag();
		if (_j < m_items.size() && m_items.at(_j).type() == Tag)
			pushSecondTag = m_items.at(_j).pushTag();

		using diff_type = BlockIterator::difference_type;
		BlockIterator first{m_items.begin() + diff_type(_i), m_items.end(), &pushFirstTag, &pushSelf};
		BlockIterator second{m_items.begin() + diff_type(_j), m_items.end(), &pushSecondTag, &pushSelf};
		BlockIterator end{m_items.end(), m_items.end()};

		if (first != end && (*first).type() == Tag)
			++first;
		if (second != end && (*second).type() == Tag)
			++second;

		return std::lexicographical_compare(first, end, second, end);
	};

	size_t iterations = 0;
	for (; ; ++iterations)
	{
		//@todo this should probably be optimized.
		std::set<size_t, std::function<bool(size_t, size_t)>> blocksSeen(comparator);
		for (size_t i = 0; i < m_items.size(); ++i)
		{
			if (m_items.at(i).type() != Tag)
				continue;
			auto it = blocksSeen.find(i);
			if (it == blocksSeen.end())
				blocksSeen.insert(i);
			else
				m_replacedTags[m_items.at(i).data()] = m_items.at(*it).data();
		}

		if (!applyTagReplacement(m_items, m_replacedTags))
			break;
	}
	return iterations > 0;
}

bool BlockDeduplicator::applyTagReplacement(
	AssemblyItems& _items,
	std::map<u256, u256> const& _replacements,
	size_t _subId
)
{
	bool changed = false;
	for (AssemblyItem& item: _items)
		if (item.type() == PushTag)
		{
			size_t subId;
			size_t tagId;
			std::tie(subId, tagId) = item.splitForeignPushTag();
			if (subId != _subId)
				continue;
			auto it = _replacements.find(tagId);
			// Recursively look for the element replaced by tagId
			for (auto _it = it; _it != _replacements.end(); _it = _replacements.find(_it->second))
				it = _it;

			if (it != _replacements.end())
			{
				changed = true;
				item.setPushTagSubIdAndTag(subId, static_cast<size_t>(it->second));
			}
		}
	return changed;
}

BlockDeduplicator::BlockIterator& BlockDeduplicator::BlockIterator::operator++()
{
	if (it == end)
		return *this;
	if (SemanticInformation::altersControlFlow(*it) && *it != AssemblyItem{Instruction::JUMPI})
		it = end;
	else
	{
		++it;
		while (it != end && it->type() == Tag)
			++it;
	}
	return *this;
}

AssemblyItem const& BlockDeduplicator::BlockIterator::operator*() const
{
	if (replaceItem && replaceWith && *it == *replaceItem)
		return *replaceWith;
	else
		return *it;
}
