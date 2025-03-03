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
#include <libhyperion/lsp/Transport.h>
#include <libhyperion/lsp/Utils.h>

#include <libhyputil/JSON.h>
#include <libhyputil/Visitor.h>
#include <libhyputil/CommonIO.h>
#include <liblangutil/Exceptions.h>

#include <fmt/format.h>

#include <boost/algorithm/string.hpp>

#include <iostream>
#include <sstream>
#include <string>


#if defined(_WIN32)
#include <io.h>
#include <fcntl.h>
#endif

using namespace hyperion::lsp;

// {{{ Transport
std::optional<Json::Value> Transport::receive()
{
	auto const headers = parseHeaders();
	if (!headers)
	{
		error({}, ErrorCode::ParseError, "Could not parse RPC headers.");
		return std::nullopt;
	}

	if (!headers->count("content-length"))
	{
		error({}, ErrorCode::ParseError, "No content-length header found.");
		return std::nullopt;
	}

	std::string const data = readBytes(stoul(headers->at("content-length")));

	Json::Value jsonMessage;
	std::string jsonParsingErrors;
	hyperion::util::jsonParseStrict(data, jsonMessage, &jsonParsingErrors);
	if (!jsonParsingErrors.empty() || !jsonMessage || !jsonMessage.isObject())
	{
		error({}, ErrorCode::ParseError, "Could not parse RPC JSON payload. " + jsonParsingErrors);
		return std::nullopt;
	}

	return {std::move(jsonMessage)};
}

void Transport::trace(std::string _message, Json::Value _extra)
{
	if (m_logTrace != TraceValue::Off)
	{
		Json::Value params;
		if (_extra.isObject())
			params = std::move(_extra);
		params["message"] = std::move(_message);
		notify("$/logTrace", std::move(params));
	}
}

std::optional<std::map<std::string, std::string>> Transport::parseHeaders()
{
	std::map<std::string, std::string> headers;

	while (true)
	{
		auto line = getline();
		if (boost::trim_copy(line).empty())
			break;

		auto const delimiterPos = line.find(':');
		if (delimiterPos == std::string::npos)
			return std::nullopt;

		auto const name = boost::to_lower_copy(line.substr(0, delimiterPos));
		auto const value = line.substr(delimiterPos + 1);
		if (!headers.emplace(boost::trim_copy(name), boost::trim_copy(value)).second)
			return std::nullopt;
	}
	return {std::move(headers)};
}

void Transport::notify(std::string _method, Json::Value _message)
{
	Json::Value json;
	json["method"] = std::move(_method);
	json["params"] = std::move(_message);
	send(std::move(json));
}

void Transport::reply(MessageID _id, Json::Value _message)
{
	Json::Value json;
	json["result"] = std::move(_message);
	send(std::move(json), _id);
}

void Transport::error(MessageID _id, ErrorCode _code, std::string _message)
{
	Json::Value json;
	json["error"]["code"] = static_cast<int>(_code);
	json["error"]["message"] = std::move(_message);
	send(std::move(json), _id);
}

void Transport::send(Json::Value _json, MessageID _id)
{
	hypAssert(_json.isObject());
	_json["jsonrpc"] = "2.0";
	if (_id != Json::nullValue)
		_json["id"] = _id;

	// Trailing CRLF only for easier readability.
	std::string const jsonString = hyperion::util::jsonCompactPrint(_json);

	writeBytes(fmt::format("Content-Length: {}\r\n\r\n", jsonString.size()));
	writeBytes(jsonString);
	flushOutput();
}
// }}}

// {{{ IOStreamTransport
IOStreamTransport::IOStreamTransport(std::istream& _in, std::ostream& _out):
	m_input{_in},
	m_output{_out}
{
}

bool IOStreamTransport::closed() const noexcept
{
	return m_input.eof();
}

std::string IOStreamTransport::readBytes(size_t _length)
{
	return util::readBytes(m_input, _length);
}

std::string IOStreamTransport::getline()
{
	std::string line;
	std::getline(m_input, line);
	return line;
}

void IOStreamTransport::writeBytes(std::string_view _data)
{
	m_output.write(_data.data(), static_cast<std::streamsize>(_data.size()));
}

void IOStreamTransport::flushOutput()
{
	m_output.flush();
}
// }}}

// {{{ StdioTransport
StdioTransport::StdioTransport()
{
	#if defined(_WIN32)
	// Attempt to change the modes of stdout from text to binary.
	setmode(fileno(stdout), O_BINARY);
	#endif
}

bool StdioTransport::closed() const noexcept
{
	return feof(stdin);
}

std::string StdioTransport::readBytes(size_t _byteCount)
{
	std::string buffer;
	buffer.resize(_byteCount);
	auto const n = fread(buffer.data(), 1, _byteCount, stdin);
	if (n < _byteCount)
		buffer.resize(n);
	return buffer;
}

std::string StdioTransport::getline()
{
	std::string line;
	std::getline(std::cin, line);
	lspDebug(fmt::format("Received: {}", line));
	return line;
}

void StdioTransport::writeBytes(std::string_view _data)
{
	lspDebug(fmt::format("Sending: {}", _data));
	auto const bytesWritten = fwrite(_data.data(), 1, _data.size(), stdout);
	hypAssert(bytesWritten == _data.size());
}

void StdioTransport::flushOutput()
{
	fflush(stdout);
}
// }}}
