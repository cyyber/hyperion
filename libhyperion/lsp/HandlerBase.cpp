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
#include <libhyputil/Exceptions.h>

#include <libhyperion/lsp/HandlerBase.h>
#include <libhyperion/lsp/LanguageServer.h>
#include <libhyperion/lsp/Utils.h>
#include <libhyperion/ast/AST.h>

#include <liblangutil/Exceptions.h>

#include <fmt/format.h>

#include <limits>

using namespace hyperion::langutil;
using namespace hyperion::lsp;
using namespace hyperion::util;

Json::Value HandlerBase::toRange(SourceLocation const& _location) const
{
	if (!_location.hasText())
		return toJsonRange({}, {});

	hypAssert(_location.sourceName, "");
	langutil::CharStream const& stream = charStreamProvider().charStream(*_location.sourceName);
	return sourceLocationToJsonRange(stream.source(), _location);
}

Json::Value HandlerBase::toJson(SourceLocation const& _location) const
{
	hypAssert(_location.sourceName);
	Json::Value item = Json::objectValue;
	item["uri"] = fileRepository().sourceUnitNameToUri(*_location.sourceName);
	item["range"] = toRange(_location);
	return item;
}

std::pair<std::string, LineColumn> HandlerBase::extractSourceUnitNameAndLineColumn(Json::Value const& _args) const
{
	std::string const uri = _args["textDocument"]["uri"].asString();
	std::string const sourceUnitName = fileRepository().uriToSourceUnitName(uri);
	if (!fileRepository().sourceUnits().count(sourceUnitName))
		BOOST_THROW_EXCEPTION(
			RequestError(ErrorCode::RequestFailed) <<
			errinfo_comment("Unknown file: " + uri)
		);

	auto const lineColumn = parseLineColumn(_args["position"]);
	if (!lineColumn)
		BOOST_THROW_EXCEPTION(
			RequestError(ErrorCode::InvalidParams) <<
			errinfo_comment("Invalid position parameter.")
		);

	if (std::optional<SourceLocation> const position = parsePosition(fileRepository(), sourceUnitName, _args["position"]))
		return {
			sourceUnitName,
			byteOffsetToByteLineColumn(fileRepository().sourceUnits().at(sourceUnitName), position->start)
		};
	return {sourceUnitName, LineColumn{lineColumn->line, std::numeric_limits<int>::max()}};
}
