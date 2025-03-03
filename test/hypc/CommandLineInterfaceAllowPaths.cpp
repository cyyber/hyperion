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

/// Unit tests for hypc/CommandLineInterface.h

#include <hypc/CommandLineInterface.h>

#include <test/hypc/Common.h>

#include <test/Common.h>
#include <test/FilesystemUtils.h>
#include <test/libhyperion/util/HyptestErrors.h>

#include <libhyputil/TemporaryDirectory.h>

#include <boost/algorithm/string/predicate.hpp>

#include <fstream>
#include <regex>
#include <string>
#include <vector>

using namespace std;
using namespace hyperion::frontend;
using namespace hyperion::util;
using namespace hyperion::test;

#define TEST_CASE_NAME (boost::unit_test::framework::current_test_case().p_name)

namespace
{

struct ImportCheck
{
	enum class Result
	{
		Unknown,        ///< Status is unknown due to a failure of the status check.
		OK,             ///< Passed compilation without errors.
		FileNotFound,   ///< Error was reported: file not found.
		PathDisallowed, ///< Error was reported: file not allowed paths.
	};

	bool operator==(ImportCheck const& _other) const { return result == _other.result && message == _other.message; }
	bool operator!=(ImportCheck const& _other) const { return !(*this == _other); }

	operator bool() const { return this->result == Result::OK; }

	static ImportCheck const OK() { return {Result::OK, ""}; }
	static ImportCheck const FileNotFound() { return {Result::FileNotFound, ""}; }
	static ImportCheck const PathDisallowed() { return {Result::PathDisallowed, ""}; }
	static ImportCheck const Unknown(const string& _message) { return {Result::Unknown, _message}; }

	Result result;
	std::string message;
};

ImportCheck checkImport(
	string const& _import,
	vector<string> const& _cliOptions
)
{
	hyptestAssert(regex_match(_import, regex{R"(import '[^']*')"}), "");
	for (string const& option: _cliOptions)
		hyptestAssert(
			boost::starts_with(option, "--base-path") ||
			boost::starts_with(option, "--include-path") ||
			boost::starts_with(option, "--allow-paths") ||
			!boost::starts_with(option, "--"),
			""
		);

	vector<string> commandLine = {
		"hypc",
		"-",
		"--no-color",
		"--error-codes",
	};
	commandLine += _cliOptions;

	string standardInputContent =
		"// SPDX-License-Identifier: GPL-3.0\n"
		"pragma hyperion >=0.0;\n" +
		_import + ";";

	test::OptionsReaderAndMessages cliResult = test::runCLI(commandLine, standardInputContent);
	if (cliResult.success)
		return ImportCheck::OK();

	static regex const sourceNotFoundErrorRegex{
		R"(^Error \(6275\): Source "[^"]+" not found: (.*)\.\n)"
		R"(\s*--> .*<stdin>:\d+:\d+:\n)"
		R"(\s*\|\n)"
		R"(\d+\s*\| import '.+';\n)"
		R"(\s*\| \^+\n\s*$)"
	};

	smatch submatches;
	if (!regex_match(cliResult.stderrContent, submatches, sourceNotFoundErrorRegex))
		return ImportCheck::Unknown("Unexpected stderr content: '" + cliResult.stderrContent + "'");
	if (submatches[1] != "File not found" && !boost::starts_with(string(submatches[1]), "File outside of allowed directories"))
		return ImportCheck::Unknown("Unexpected error message: '" + cliResult.stderrContent + "'");

	if (submatches[1] == "File not found")
		return ImportCheck::FileNotFound();
	else if (boost::starts_with(string(submatches[1]), "File outside of allowed directories"))
		return ImportCheck::PathDisallowed();
	else
		return ImportCheck::Unknown("Unexpected error message '" + submatches[1].str() + "'");
}

class AllowPathsFixture
{
protected:
	AllowPathsFixture():
		m_tempDir({"code/", "work/"}, TEST_CASE_NAME),
		m_tempWorkDir(m_tempDir.path() / "work"),
		m_codeDir(m_tempDir.path() / "code"),
		m_workDir(m_tempDir.path() / "work"),
		m_portablePrefix(("/" / boost::filesystem::canonical(m_codeDir).relative_path()).generic_string())
	{
		createFilesWithParentDirs(
			{
				m_codeDir / "a/b/c/d.hyp",
				m_codeDir / "a/b/c.hyp",
				m_codeDir / "a/b/X.hyp",
				m_codeDir / "a/X/c.hyp",
				m_codeDir / "X/b/c.hyp",

				m_codeDir / "a/bc/d.hyp",
				m_codeDir / "X/bc/d.hyp",

				m_codeDir / "x/y/z.hyp",
				m_codeDir / "1/2/3.hyp",
				m_codeDir / "contract.hyp",

				m_workDir / "a/b/c/d.hyp",
				m_workDir / "a/b/c.hyp",
				m_workDir / "a/b/X.hyp",
				m_workDir / "a/X/c.hyp",
				m_workDir / "X/b/c.hyp",

				m_workDir / "contract.hyp",
			},
			"// SPDX-License-Identifier: GPL-3.0\npragma hyperion >=0.0;"
		);

		if (
			!createSymlinkIfSupportedByFilesystem("b", m_codeDir / "a/b_sym", true) ||
			!createSymlinkIfSupportedByFilesystem("../x/y", m_codeDir / "a/y_sym", true) ||
			!createSymlinkIfSupportedByFilesystem("../../a/b/c.hyp", m_codeDir / "a/b/c_sym.hyp", false) ||
			!createSymlinkIfSupportedByFilesystem("../../x/y/z.hyp", m_codeDir / "a/b/z_sym.hyp", false)
		)
			return;

		m_caseSensitiveFilesystem = boost::filesystem::create_directories(m_codeDir / "A/B/C");
		hyptestAssert(boost::filesystem::equivalent(m_codeDir / "a/b/c", m_codeDir / "A/B/C") != m_caseSensitiveFilesystem, "");
	}

	TemporaryDirectory m_tempDir;
	TemporaryWorkingDirectory m_tempWorkDir;
	boost::filesystem::path const m_codeDir;
	boost::filesystem::path const m_workDir;
	string m_portablePrefix;
	bool m_caseSensitiveFilesystem = true;
};

ostream& operator<<(ostream& _out, ImportCheck const& _value)
{
	switch (_value.result)
	{
	case ImportCheck::Result::Unknown: _out << "Unknown"; break;
	case ImportCheck::Result::OK: _out << "OK"; break;
	case ImportCheck::Result::FileNotFound: _out << "FileNotFound"; break;
	case ImportCheck::Result::PathDisallowed: _out << "PathDisallowed"; break;
	}
	if (_value.message != "")
		_out << "(" << _value.message << ")";
	return _out;
}

} // namespace

namespace boost::test_tools::tt_detail
{

// Boost won't find the << operator unless we put it in the std namespace which is illegal.
// The recommended solution is to overload print_log_value<> struct and make it use our operator.

template<>
struct print_log_value<ImportCheck>
{
	void operator()(std::ostream& _out, ImportCheck const& _value) { ::operator<<(_out, _value); }
};

} // namespace boost::test_tools::tt_detail

namespace hyperion::frontend::test
{

BOOST_AUTO_TEST_SUITE(CommandLineInterfaceAllowPathsTest)

BOOST_FIXTURE_TEST_CASE(allow_path_multiple_paths, AllowPathsFixture)
{
	string allowedPaths =
		m_codeDir.generic_string() + "/a/b/X.hyp," +
		m_codeDir.generic_string() + "/X/," +
		m_codeDir.generic_string() + "/z," +
		m_codeDir.generic_string() + "/a/b";

	BOOST_TEST(checkImport("import '" + m_portablePrefix + "/a/b/c.hyp'", {"--allow-paths", allowedPaths}));
	BOOST_TEST(checkImport("import '" + m_portablePrefix + "/a/b/X.hyp'", {"--allow-paths", allowedPaths}));
	BOOST_TEST(checkImport("import '" + m_portablePrefix + "/a/X/c.hyp'", {"--allow-paths", allowedPaths}) == ImportCheck::PathDisallowed());
	BOOST_TEST(checkImport("import '" + m_portablePrefix + "/X/b/c.hyp'", {"--allow-paths", allowedPaths}));
}

BOOST_FIXTURE_TEST_CASE(allow_path_should_work_with_various_path_forms, AllowPathsFixture)
{
	string import = "import '" + m_portablePrefix + "/a/b/c.hyp'";

	// Without --allow-path
	BOOST_TEST(checkImport(import, {}) == ImportCheck::PathDisallowed());

	// Absolute paths allowed
	BOOST_TEST(checkImport(import, {"--allow-paths", m_codeDir.string()}));
	BOOST_TEST(checkImport(import, {"--allow-paths", m_codeDir.string() + "/a"}));
	BOOST_TEST(checkImport(import, {"--allow-paths", m_codeDir.string() + "/a/"}));
	BOOST_TEST(checkImport(import, {"--allow-paths", m_codeDir.string() + "/a/b"}));
	BOOST_TEST(checkImport(import, {"--allow-paths", m_codeDir.string() + "/a/b/c.hyp"}));

	// Relative paths allowed
	BOOST_TEST(checkImport(import, {"--allow-paths=../code/a"}));
	BOOST_TEST(checkImport(import, {"--allow-paths=../code/a/"}));
	BOOST_TEST(checkImport(import, {"--allow-paths=../code/a/b"}));
	BOOST_TEST(checkImport(import, {"--allow-paths=../code/a/b/c.hyp"}));

	// Non-normalized paths allowed
	BOOST_TEST(checkImport(import, {"--allow-paths", "./../code/."}));
	BOOST_TEST(checkImport(import, {"--allow-paths", "./../code/./"}));
	BOOST_TEST(checkImport(import, {"--allow-paths", "./../code/a/.."}));
	BOOST_TEST(checkImport(import, {"--allow-paths", "./../code/a/../"}));
	BOOST_TEST(checkImport(import, {"--allow-paths", "./../code/a/b"}));
	BOOST_TEST(checkImport(import, {"--allow-paths", "./../code/a/./b"}));
	BOOST_TEST(checkImport(import, {"--allow-paths", "./../code/a/../a/b"}));
	BOOST_TEST(checkImport(import, {"--allow-paths", "./../code/a///b"}));
	BOOST_TEST(checkImport(import, {"--allow-paths", "./../code/a/b//"}));
	BOOST_TEST(checkImport(import, {"--allow-paths", "./../code/a/b///"}));

	// Root path allowed
	BOOST_TEST(checkImport(import, {"--allow-paths=/"}));
	BOOST_TEST(checkImport(import, {"--allow-paths=///"}));

	// UNC paths should be treated differently from normal paths
	hyptestAssert(FileReader::isUNCPath("/" + m_portablePrefix), "");
	BOOST_TEST(checkImport(import, {"--allow-paths=//"}) == ImportCheck::PathDisallowed());
	BOOST_TEST(checkImport(import, {"--allow-paths=/" + m_portablePrefix}) == ImportCheck::PathDisallowed());

	// Paths going beyond root allowed
	BOOST_TEST(checkImport(import, {"--allow-paths=/../../"}));
	BOOST_TEST(checkImport(import, {"--allow-paths=/../.."}));
	BOOST_TEST(checkImport(import, {"--allow-paths=/../../a/../"}));
	BOOST_TEST(checkImport(import, {"--allow-paths=/../../" + m_portablePrefix}));

	// File named like a directory
	BOOST_TEST(checkImport(import, {"--allow-paths", m_codeDir.string() + "/a/b/c.hyp/"}));
}

BOOST_FIXTURE_TEST_CASE(allow_path_should_handle_empty_paths, AllowPathsFixture)
{
	// Work dir is base path
	BOOST_TEST(checkImport("import 'a/../../work/a/b/c.hyp'", {"--allow-paths", ""}));
	BOOST_TEST(checkImport("import 'a/../../work/a/b/c.hyp'", {"--allow-paths", "x,,y"}));
	BOOST_TEST(checkImport("import 'a/../../code/a/b/c.hyp'", {"--allow-paths", ""}) == ImportCheck::PathDisallowed());
	BOOST_TEST(checkImport("import 'a/../../code/a/b/c.hyp'", {"--allow-paths", "x,,y"}) == ImportCheck::PathDisallowed());

	// Work dir is not base path
	BOOST_TEST(checkImport("import 'a/../../work/a/b/c.hyp'", {"--allow-paths", "", "--base-path=../code/"}) == ImportCheck::PathDisallowed());
	BOOST_TEST(checkImport("import 'a/../../work/a/b/c.hyp'", {"--allow-paths", "x,,y", "--base-path=../code/"}) == ImportCheck::PathDisallowed());
	BOOST_TEST(checkImport("import 'a/../../code/a/b/c.hyp'", {"--allow-paths", "", "--base-path=../code/"}));
	BOOST_TEST(checkImport("import 'a/../../code/a/b/c.hyp'", {"--allow-paths", "x,,y", "--base-path=../code/"}));
}

BOOST_FIXTURE_TEST_CASE(allow_path_case_sensitive, AllowPathsFixture)
{
	// Allowed paths are case-sensitive even on case-insensitive filesystems
	BOOST_TEST(
		checkImport("import '" + m_portablePrefix + "/a/b/c.hyp'", {"--allow-paths", m_codeDir.string() + "/A/B/"}) ==
		ImportCheck::PathDisallowed()
	);
}

BOOST_FIXTURE_TEST_CASE(allow_path_should_work_with_various_import_forms, AllowPathsFixture)
{
	// Absolute import paths
	BOOST_TEST(checkImport("import '" + m_portablePrefix + "/a/b/c.hyp'", {"--allow-paths", "../code/a/b/c.hyp"}));
	BOOST_TEST(checkImport("import '" + m_portablePrefix + "/X/b/c.hyp'", {"--allow-paths", "../code/a/b/c.hyp"}) == ImportCheck::PathDisallowed());
	BOOST_TEST(checkImport("import '" + m_portablePrefix + "/a/X/c.hyp'", {"--allow-paths", "../code/a/b/c.hyp"}) == ImportCheck::PathDisallowed());
	BOOST_TEST(checkImport("import '" + m_portablePrefix + "/a/b/X.hyp'", {"--allow-paths", "../code/a/b/c.hyp"}) == ImportCheck::PathDisallowed());

	BOOST_TEST(checkImport("import '" + m_portablePrefix + "/a/b/c.hyp'", {"--allow-paths", "../code/a/b/"}));
	BOOST_TEST(checkImport("import '" + m_portablePrefix + "/X/b/c.hyp'", {"--allow-paths", "../code/a/b/"}) == ImportCheck::PathDisallowed());
	BOOST_TEST(checkImport("import '" + m_portablePrefix + "/a/X/c.hyp'", {"--allow-paths", "../code/a/b/"}) == ImportCheck::PathDisallowed());
	BOOST_TEST(checkImport("import '" + m_portablePrefix + "/a/b/X.hyp'", {"--allow-paths", "../code/a/b/"}));

	BOOST_TEST(checkImport("import '" + m_portablePrefix + "/a/b/c.hyp'", {"--allow-paths", "../code/a/b"}));
	BOOST_TEST(checkImport("import '" + m_portablePrefix + "/X/b/c.hyp'", {"--allow-paths", "../code/a/b"}) == ImportCheck::PathDisallowed());
	BOOST_TEST(checkImport("import '" + m_portablePrefix + "/a/X/c.hyp'", {"--allow-paths", "../code/a/b"}) == ImportCheck::PathDisallowed());
	BOOST_TEST(checkImport("import '" + m_portablePrefix + "/a/b/X.hyp'", {"--allow-paths", "../code/a/b"}));

	BOOST_TEST(checkImport("import '" + m_portablePrefix + "/a/b/c.hyp'", {"--allow-paths", "../code/a"}));
	BOOST_TEST(checkImport("import '" + m_portablePrefix + "/X/b/c.hyp'", {"--allow-paths", "../code/a"}) == ImportCheck::PathDisallowed());
	BOOST_TEST(checkImport("import '" + m_portablePrefix + "/a/X/c.hyp'", {"--allow-paths", "../code/a"}));
	BOOST_TEST(checkImport("import '" + m_portablePrefix + "/a/b/X.hyp'", {"--allow-paths", "../code/a"}));

	// Relative import paths
	// NOTE: Base path is whitelisted by default so we need the 'a/../../code/' part to get
	// outside of it. And it can't be just '../code/' because that would not be a direct import.
	BOOST_TEST(checkImport("import 'a/../../code/a/b/c.hyp'", {"--allow-paths", "../code/a/b"}));
	BOOST_TEST(checkImport("import 'a/../../code/X/b/c.hyp'", {"--allow-paths", "../code/a/b"}) == ImportCheck::PathDisallowed());
	BOOST_TEST(checkImport("import 'a/../../code/a/X/c.hyp'", {"--allow-paths", "../code/a/b"}) == ImportCheck::PathDisallowed());
	BOOST_TEST(checkImport("import 'a/../../code/a/b/X.hyp'", {"--allow-paths", "../code/a/b"}));

	// Non-normalized relative import paths
	BOOST_TEST(checkImport("import 'a/../../code/a/./b/c.hyp'", {"--allow-paths", "../code/a/b/c.hyp"}));
	BOOST_TEST(checkImport("import 'a/../../code/a/../a/b/c.hyp'", {"--allow-paths", "../code/a/b/c.hyp"}));
	BOOST_TEST(checkImport("import 'a/../../code/a///b/c.hyp'", {"--allow-paths", "../code/a/b/c.hyp"}));

#if !defined(_WIN32)
	// UNC paths in imports.
	// Unfortunately can't test it on Windows without having an existing UNC path. On Linux we can
	// at least rely on the fact that `//` works like `/`.
	string uncImportPath = "/" + m_portablePrefix + "/a/b/c.hyp";
	hyptestAssert(FileReader::isUNCPath(uncImportPath), "");
	BOOST_TEST(checkImport("import '" + uncImportPath + "'", {"--allow-paths", "../code/a/b/c.hyp"}) == ImportCheck::PathDisallowed());
#endif
}

BOOST_FIXTURE_TEST_CASE(allow_path_automatic_whitelisting_input_files, AllowPathsFixture)
{
	// By default none of the files is whitelisted
	BOOST_TEST(checkImport("import '" + m_portablePrefix + "/a/b/c.hyp'", {}) == ImportCheck::PathDisallowed());
	BOOST_TEST(checkImport("import '" + m_portablePrefix + "/a/b/c/d.hyp'", {}) == ImportCheck::PathDisallowed());
	BOOST_TEST(checkImport("import '" + m_portablePrefix + "/a/b/X.hyp'", {}) == ImportCheck::PathDisallowed());
	BOOST_TEST(checkImport("import '" + m_portablePrefix + "/a/X/c.hyp'", {}) == ImportCheck::PathDisallowed());
	BOOST_TEST(checkImport("import '" + m_portablePrefix + "/X/b/c.hyp'", {}) == ImportCheck::PathDisallowed());
	BOOST_TEST(checkImport("import '" + m_portablePrefix + "/a/bc/d.hyp'", {}) == ImportCheck::PathDisallowed());

	// Compiling a file whitelists its directory and subdirectories
	BOOST_TEST(checkImport("import '" + m_portablePrefix + "/a/b/c.hyp'", {m_codeDir.string() + "/a/b/c.hyp"}));
	BOOST_TEST(checkImport("import '" + m_portablePrefix + "/a/b/c/d.hyp'", {m_codeDir.string() + "/a/b/c.hyp"}));
	BOOST_TEST(checkImport("import '" + m_portablePrefix + "/a/b/X.hyp'", {m_codeDir.string() + "/a/b/c.hyp"}));
	BOOST_TEST(checkImport("import '" + m_portablePrefix + "/a/X/c.hyp'", {m_codeDir.string() + "/a/b/c.hyp"}) == ImportCheck::PathDisallowed());
	BOOST_TEST(checkImport("import '" + m_portablePrefix + "/X/b/c.hyp'", {m_codeDir.string() + "/a/b/c.hyp"}) == ImportCheck::PathDisallowed());
	BOOST_TEST(checkImport("import '" + m_portablePrefix + "/a/bc/d.hyp'", {m_codeDir.string() + "/a/b/c.hyp"}) == ImportCheck::PathDisallowed());

	// If only file name is specified, its parent dir path is empty. This should be equivalent to
	// whitelisting the work dir.
	BOOST_TEST(checkImport("import '" + m_portablePrefix + "/contract.hyp'", {"contract.hyp"}) == ImportCheck::PathDisallowed());
	BOOST_TEST(checkImport("import 'contract.hyp'", {"contract.hyp"}));
}

BOOST_FIXTURE_TEST_CASE(allow_path_automatic_whitelisting_remappings, AllowPathsFixture)
{
	// Adding a remapping whitelists target's parent directory and subdirectories
	BOOST_TEST(checkImport("import '" + m_portablePrefix + "/a/b/c.hyp'", {"x=../code/a/b/c.hyp"}));
	BOOST_TEST(checkImport("import '" + m_portablePrefix + "/a/b/c/d.hyp'", {"x=../code/a/b/c.hyp"}));
	BOOST_TEST(checkImport("import '" + m_portablePrefix + "/a/b/X.hyp'", {"x=../code/a/b/c.hyp"}));
	BOOST_TEST(checkImport("import '" + m_portablePrefix + "/a/X/c.hyp'", {"x=../code/a/b/c.hyp"}) == ImportCheck::PathDisallowed());
	BOOST_TEST(checkImport("import '" + m_portablePrefix + "/X/b/c.hyp'", {"x=../code/a/b/c.hyp"}) == ImportCheck::PathDisallowed());
	BOOST_TEST(checkImport("import '" + m_portablePrefix + "/a/bc/d.hyp'", {"x=../code/a/b/c.hyp"}) == ImportCheck::PathDisallowed());

	BOOST_TEST(checkImport("import '" + m_portablePrefix + "/a/b/c.hyp'", {"x=/contract.hyp"}));
	BOOST_TEST(checkImport("import '" + m_portablePrefix + "/a/b/c.hyp'", {"x=/contract.hyp/"}) == ImportCheck::PathDisallowed());

	BOOST_TEST(checkImport("import '" + m_portablePrefix + "/a/b/c.hyp'", {m_portablePrefix + "/a/b=../code/X/b"}));
	BOOST_TEST(checkImport("import '" + m_portablePrefix + "/a/b/c.hyp'", {m_portablePrefix + "/a/b/=../code/X/b/"}));
	BOOST_TEST(checkImport("import '" + m_portablePrefix + "/a/bc/d.hyp'", {m_portablePrefix + "/a/b=../code/X/b"}));
	BOOST_TEST(checkImport("import '" + m_portablePrefix + "/a/bc/d.hyp'", {m_portablePrefix + "/a/b/=../code/X/b/"}) == ImportCheck::PathDisallowed());
	BOOST_TEST(checkImport("import '" + m_portablePrefix + "/a/b/c.hyp'", {m_portablePrefix + "/a/b:y/z=x/w"}) == ImportCheck::PathDisallowed());
	BOOST_TEST(checkImport("import '" + m_portablePrefix + "/a/b/c.hyp'", {m_portablePrefix + "/a/b:y/z=x/w"}) == ImportCheck::PathDisallowed());

	// Adding a remapping whitelists the target and subdirectories when the target is a directory
	BOOST_TEST(checkImport("import '" + m_portablePrefix + "/a/b/c.hyp'", {"x=../code/a/b/"}));
	BOOST_TEST(checkImport("import '" + m_portablePrefix + "/a/b/c/d.hyp'", {"x=../code/a/b/"}));
	BOOST_TEST(checkImport("import '" + m_portablePrefix + "/a/b/X.hyp'", {"x=../code/a/b/"}));
	BOOST_TEST(checkImport("import '" + m_portablePrefix + "/a/X/c.hyp'", {"x=../code/a/b/"}) == ImportCheck::PathDisallowed());
	BOOST_TEST(checkImport("import '" + m_portablePrefix + "/X/b/c.hyp'", {"x=../code/a/b/"}) == ImportCheck::PathDisallowed());
	BOOST_TEST(checkImport("import '" + m_portablePrefix + "/a/bc/d.hyp'", {"x=../code/a/b/"}) == ImportCheck::PathDisallowed());
	BOOST_TEST(checkImport("import '" + m_portablePrefix + "/a/bc/d.hyp'", {"x=../code/a/c/"}) == ImportCheck::PathDisallowed());

	// Adding a remapping whitelists target's parent directory and subdirectories when the target
	// is a directory but does not have a trailing slash
	BOOST_TEST(checkImport("import '" + m_portablePrefix + "/a/b/c.hyp'", {"x=../code/a/b"}));
	BOOST_TEST(checkImport("import '" + m_portablePrefix + "/a/b/c/d.hyp'", {"x=../code/a/b"}));
	BOOST_TEST(checkImport("import '" + m_portablePrefix + "/a/b/X.hyp'", {"x=../code/a/b"}));
	BOOST_TEST(checkImport("import '" + m_portablePrefix + "/a/X/c.hyp'", {"x=../code/a/b"}));
	BOOST_TEST(checkImport("import '" + m_portablePrefix + "/X/b/c.hyp'", {"x=../code/a/b"}) == ImportCheck::PathDisallowed());
	BOOST_TEST(checkImport("import '" + m_portablePrefix + "/a/bc/d.hyp'", {"x=../code/a/b"}));
	BOOST_TEST(checkImport("import '" + m_portablePrefix + "/a/bc/d.hyp'", {"x=../code/a/c"}));

	// Adding a remapping to a relative target at VFS root whitelists the work dir
	BOOST_TEST(checkImport("import '/../../x/y/z.hyp'", {"x=contract.hyp", "--base-path=../code/a/b/"}) == ImportCheck::PathDisallowed());
	BOOST_TEST(checkImport("import '/../../../work/a/b/c.hyp'", {"x=contract.hyp", "--base-path=../code/a/b/"}));

	BOOST_TEST(checkImport("import '/../../x/y/z.hyp'", {"x=contract.hyp/", "--base-path=../code/a/b/"}) == ImportCheck::PathDisallowed());
	BOOST_TEST(checkImport("import '/../../../work/a/b/c.hyp'", {"x=contract.hyp/", "--base-path=../code/a/b/"}) == ImportCheck::PathDisallowed());

	// Adding a remapping with an empty target does not whitelist anything
	BOOST_TEST(checkImport("import '" + m_portablePrefix + m_portablePrefix + "/a/b/c.hyp'", {m_portablePrefix + "="}) == ImportCheck::PathDisallowed());
	BOOST_TEST(checkImport("import '" + m_portablePrefix + "/a/b/c.hyp'", {"../code/="}) == ImportCheck::PathDisallowed());
	BOOST_TEST(checkImport("import '/../work/a/b/c.hyp'", {"../code/=", "--base-path", m_portablePrefix}) == ImportCheck::PathDisallowed());

	// Adding a remapping that includes .. or . segments whitelists the parent dir and subdirectories
	// of the resolved target
	BOOST_TEST(checkImport("import '" + m_portablePrefix + "/a/b/c.hyp'", {"x=."}) == ImportCheck::PathDisallowed());
	BOOST_TEST(checkImport("import '" + m_portablePrefix + "/X/b/c.hyp'", {"x=."}) == ImportCheck::PathDisallowed());

	BOOST_TEST(checkImport("import '" + m_portablePrefix + "/a/b/c.hyp'", {"x=./"}) == ImportCheck::PathDisallowed());
	BOOST_TEST(checkImport("import '" + m_portablePrefix + "/X/b/c.hyp'", {"x=./"}) == ImportCheck::PathDisallowed());

	BOOST_TEST(checkImport("import '" + m_portablePrefix + "/a/b/c.hyp'", {"x=.."}));
	BOOST_TEST(checkImport("import '" + m_portablePrefix + "/X/b/c.hyp'", {"x=.."}));

	BOOST_TEST(checkImport("import '" + m_portablePrefix + "/a/b/c.hyp'", {"x=../"}));
	BOOST_TEST(checkImport("import '" + m_portablePrefix + "/X/b/c.hyp'", {"x=../"}));

	BOOST_TEST(checkImport("import '" + m_portablePrefix + "/a/b/c.hyp'", {"x=../code/a/b/./.."}));
	BOOST_TEST(checkImport("import '" + m_portablePrefix + "/a/X/c.hyp'", {"x=../code/a/b/./.."}));
	BOOST_TEST(checkImport("import '" + m_portablePrefix + "/X/b/c.hyp'", {"x=../code/a/b/./.."}) == ImportCheck::PathDisallowed());

	BOOST_TEST(checkImport("import '" + m_portablePrefix + "/a/b/c.hyp'", {"x=../code/a/b/./../"}));
	BOOST_TEST(checkImport("import '" + m_portablePrefix + "/a/X/c.hyp'", {"x=../code/a/b/./../"}));
	BOOST_TEST(checkImport("import '" + m_portablePrefix + "/X/b/c.hyp'", {"x=../code/a/b/./../"}) == ImportCheck::PathDisallowed());

	BOOST_TEST(checkImport("import '" + m_portablePrefix + "/a/b/c.hyp'", {"x=../code/a/b/./../b"}));
	BOOST_TEST(checkImport("import '" + m_portablePrefix + "/a/X/c.hyp'", {"x=../code/a/b/./../b"}));
	BOOST_TEST(checkImport("import '" + m_portablePrefix + "/X/b/c.hyp'", {"x=../code/a/b/./../b"}) == ImportCheck::PathDisallowed());

	BOOST_TEST(checkImport("import '" + m_portablePrefix + "/a/b/c.hyp'", {"x=../code/a/b/./../b/"}));
	BOOST_TEST(checkImport("import '" + m_portablePrefix + "/a/X/c.hyp'", {"x=../code/a/b/./../b/"}) == ImportCheck::PathDisallowed());
	BOOST_TEST(checkImport("import '" + m_portablePrefix + "/X/b/c.hyp'", {"x=../code/a/b/./../b/"}) == ImportCheck::PathDisallowed());

	// If the target is just a file name, its parent dir path is empty. This should be equivalent to
	// whitelisting the work dir.
	BOOST_TEST(checkImport("import '" + m_portablePrefix + "/contract.hyp'", {"x=contract.hyp"}) == ImportCheck::PathDisallowed());
	BOOST_TEST(checkImport("import 'contract.hyp'", {"x=contract.hyp"}));
}

BOOST_FIXTURE_TEST_CASE(allow_path_automatic_whitelisting_base_path, AllowPathsFixture)
{
	// Relative base path whitelists its content
	BOOST_TEST(checkImport("import 'b/c.hyp'", {"--base-path=../code/a"}));
	BOOST_TEST(checkImport("import 'b/c/d.hyp'", {"--base-path=../code/a"}));
	BOOST_TEST(checkImport("import 'b/X.hyp'", {"--base-path=../code/a"}));
	BOOST_TEST(checkImport("import 'X/c.hyp'", {"--base-path=../code/a"}));

	BOOST_TEST(checkImport("import 'b/c.hyp'", {"--base-path=../code/a/"}));
	BOOST_TEST(checkImport("import 'b/c/d.hyp'", {"--base-path=../code/a/"}));
	BOOST_TEST(checkImport("import 'b/X.hyp'", {"--base-path=../code/a/"}));
	BOOST_TEST(checkImport("import 'X/c.hyp'", {"--base-path=../code/a/"}));

	BOOST_TEST(checkImport("import 'a/b/c.hyp'", {"--base-path=../code/."}));
	BOOST_TEST(checkImport("import 'a/b/c.hyp'", {"--base-path=../code/./"}));
	BOOST_TEST(checkImport("import 'code/a/b/c.hyp'", {"--base-path=.."}));
	BOOST_TEST(checkImport("import 'code/a/b/c.hyp'", {"--base-path=../"}));

	// Absolute base path whitelists its content
	BOOST_TEST(checkImport("import 'b/c.hyp'", {"--base-path", m_codeDir.string() + "/a"}));
	BOOST_TEST(checkImport("import 'b/c/d.hyp'", {"--base-path", m_codeDir.string() + "/a"}));
	BOOST_TEST(checkImport("import 'b/X.hyp'", {"--base-path", m_codeDir.string() + "/a"}));
	BOOST_TEST(checkImport("import 'X/c.hyp'", {"--base-path", m_codeDir.string() + "/a"}));
}

BOOST_FIXTURE_TEST_CASE(allow_path_automatic_whitelisting_work_dir, AllowPathsFixture)
{
	// Work dir is only automatically whitelisted if it matches base path
	BOOST_TEST(checkImport("import 'b/../../../work/a/b/c.hyp'", {"--base-path=../code/a/"}) == ImportCheck::PathDisallowed());

	// Compiling a file in the work dir whitelists it even if it's not in base path
	BOOST_TEST(checkImport("import 'b/../../../work/a/b/c.hyp'", {"--base-path", "../code/a/", "a/b/c.hyp"}));

	// Work dir can also be whitelisted manually
	BOOST_TEST(checkImport("import 'b/../../../work/a/b/c.hyp'", {"--base-path", "../code/a/", "--allow-paths=."}));

	// Not setting base path whitelists the working directory
	BOOST_TEST(checkImport("import 'a/b/c.hyp'", {}));
	BOOST_TEST(checkImport("import 'a/b/c/d.hyp'", {}));
	BOOST_TEST(checkImport("import 'a/b/X.hyp'", {}));
	BOOST_TEST(checkImport("import 'a/X/c.hyp'", {}));

	// Setting base path to an empty value whitelists the working directory
	BOOST_TEST(checkImport("import 'a/b/c.hyp'", {"--base-path", ""}));
	BOOST_TEST(checkImport("import 'a/b/c/d.hyp'", {"--base-path", ""}));
	BOOST_TEST(checkImport("import 'a/b/X.hyp'", {"--base-path", ""}));
	BOOST_TEST(checkImport("import 'a/X/c.hyp'", {"--base-path", ""}));
}

BOOST_FIXTURE_TEST_CASE(allow_path_automatic_whitelisting_include_paths, AllowPathsFixture)
{
	// Relative include path whitelists its content
	BOOST_TEST(checkImport("import 'b/c.hyp'", {"--base-path=a/b/c", "--include-path=../code/a"}));
	BOOST_TEST(checkImport("import 'b/c/d.hyp'", {"--base-path=a/b/c", "--include-path=../code/a"}));
	BOOST_TEST(checkImport("import 'b/X.hyp'", {"--base-path=a/b/c", "--include-path=../code/a"}));
	BOOST_TEST(checkImport("import 'X/c.hyp'", {"--base-path=a/b/c", "--include-path=../code/a"}));

	BOOST_TEST(checkImport("import 'b/c.hyp'", {"--base-path=a/b/c", "--include-path=../code/a/"}));
	BOOST_TEST(checkImport("import 'b/c/d.hyp'", {"--base-path=a/b/c", "--include-path=../code/a/"}));
	BOOST_TEST(checkImport("import 'b/X.hyp'", {"--base-path=a/b/c", "--include-path=../code/a/"}));
	BOOST_TEST(checkImport("import 'X/c.hyp'", {"--base-path=a/b/c", "--include-path=../code/a/"}));

	BOOST_TEST(checkImport("import 'a/b/c.hyp'", {"--base-path=a/b/c", "--include-path=../code/."}));
	BOOST_TEST(checkImport("import 'a/b/c.hyp'", {"--base-path=a/b/c", "--include-path=../code/./"}));
	BOOST_TEST(checkImport("import 'code/a/b/c.hyp'", {"--base-path=a/b/c", "--include-path=.."}));
	BOOST_TEST(checkImport("import 'code/a/b/c.hyp'", {"--base-path=a/b/c", "--include-path=../"}));

	// Absolute include path whitelists its content
	BOOST_TEST(checkImport("import 'b/c.hyp'", {"--base-path=a/b/c", "--include-path", m_codeDir.string() + "/a"}));
	BOOST_TEST(checkImport("import 'b/c/d.hyp'", {"--base-path=a/b/c", "--include-path", m_codeDir.string() + "/a"}));
	BOOST_TEST(checkImport("import 'b/X.hyp'", {"--base-path=a/b/c", "--include-path", m_codeDir.string() + "/a"}));
	BOOST_TEST(checkImport("import 'X/c.hyp'", {"--base-path=a/b/c", "--include-path", m_codeDir.string() + "/a"}));

	// If there are multiple include paths, all of them get whitelisted
	BOOST_TEST(checkImport("import 'b/c.hyp'", {"--base-path=a/b/c", "--include-path=../code/a", "--include-path=../code/1"}));
	BOOST_TEST(checkImport("import '2/3.hyp'", {"--base-path=a/b/c", "--include-path=../code/a", "--include-path=../code/1"}));
	BOOST_TEST(checkImport("import 'b/c.hyp'", {"--base-path=a/b/c", "--include-path=../code/1", "--include-path=../code/a"}));
	BOOST_TEST(checkImport("import '2/3.hyp'", {"--base-path=a/b/c", "--include-path=../code/1", "--include-path=../code/a"}));
}

BOOST_FIXTURE_TEST_CASE(allow_path_symlinks_within_whitelisted_dir, AllowPathsFixture)
{
	BOOST_TEST(checkImport("import '" + m_portablePrefix + "/a/b_sym/c.hyp'", {"--allow-paths=../code/a/b/"}));
	BOOST_TEST(checkImport("import '" + m_portablePrefix + "/a/b/c.hyp'", {"--allow-paths=../code/a/b_sym/"}));
	BOOST_TEST(checkImport("import '" + m_portablePrefix + "/a/b_sym/c.hyp'", {"--allow-paths=../code/a/b_sym/"}));

	BOOST_TEST(checkImport("import '" + m_portablePrefix + "/a/b_sym/c.hyp'", {"--allow-paths=../code/a/b"}));
	BOOST_TEST(checkImport("import '" + m_portablePrefix + "/a/b/c.hyp'", {"--allow-paths=../code/a/b_sym"}));
	BOOST_TEST(checkImport("import '" + m_portablePrefix + "/a/b_sym/c.hyp'", {"--allow-paths=../code/a/b_sym"}));

	BOOST_TEST(checkImport("import '" + m_portablePrefix + "/a/b/c_sym.hyp'", {"--allow-paths=../code/a/b/c.hyp"}));
	BOOST_TEST(checkImport("import '" + m_portablePrefix + "/a/b/c.hyp'", {"--allow-paths=../code/a/b/c_sym.hyp"}));
	BOOST_TEST(checkImport("import '" + m_portablePrefix + "/a/b/c_sym.hyp'", {"--allow-paths=../code/a/b/c_sym.hyp"}));
}

BOOST_FIXTURE_TEST_CASE(allow_path_symlinks_outside_whitelisted_dir, AllowPathsFixture)
{
	BOOST_TEST(checkImport("import '" + m_portablePrefix + "/a/y_sym/z.hyp'", {"--allow-paths=../code/a/"}) == ImportCheck::PathDisallowed());
	BOOST_TEST(checkImport("import '" + m_portablePrefix + "/a/y_sym/z.hyp'", {"--allow-paths=../code/x/"}));

	BOOST_TEST(checkImport("import '" + m_portablePrefix + "/a/b/z_sym.hyp'", {"--allow-paths=../code/a/"}) == ImportCheck::PathDisallowed());
	BOOST_TEST(checkImport("import '" + m_portablePrefix + "/a/b/z_sym.hyp'", {"--allow-paths=../code/x/"}));
	BOOST_TEST(checkImport("import '" + m_portablePrefix + "/a/b/z_sym.hyp'", {"--allow-paths=../code/a/b/z_sym.hyp"}));
	BOOST_TEST(checkImport("import '" + m_portablePrefix + "/a/b/z_sym.hyp'", {"--allow-paths=../code/x/y/z.hyp"}));
}

BOOST_AUTO_TEST_SUITE_END()

} // namespace hyperion::frontend::test
