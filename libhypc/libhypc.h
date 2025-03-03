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
 * @author Christian <c@ethdev.com>
 * @date 2014
 * Public compiler API.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
#define HYPC_NOEXCEPT noexcept
#else
#define HYPC_NOEXCEPT
#endif

#ifdef __cplusplus
extern "C" {
#endif

/// Callback used to retrieve additional source files or data.
///
/// @param _context The readContext passed to hyperion_compile. Can be NULL.
/// @param _kind The kind of callback (a string).
/// @param _data The data for the callback (a string).
/// @param o_contents A pointer to the contents of the file, if found. Allocated via hyperion_alloc().
/// @param o_error A pointer to an error message, if there is one.
///
/// The file (as well as error) contents that is to be allocated by the callback
/// implementor must use the hyperion_alloc() API to allocate its underlying
/// storage. Ownership is then transferred to the compiler which will take care
/// of the deallocation.
///
/// If the callback is not supported, *o_contents and *o_error must be set to NULL.
typedef void (*CStyleReadFileCallback)(void* _context, char const* _kind, char const* _data, char** o_contents, char** o_error);

/// Returns the complete license document.
///
/// The pointer returned must NOT be freed by the caller.
char const* hyperion_license() HYPC_NOEXCEPT;

/// Returns the compiler version.
///
/// The pointer returned must NOT be freed by the caller.
char const* hyperion_version() HYPC_NOEXCEPT;

/// Allocates a chunk of memory of @p _size bytes.
///
/// Use this function inside callbacks to allocate data that is to be passed to
/// the compiler. You may use hyperion_free() or hyperion_reset() to free this
/// memory again but it is not required as the compiler takes ownership for any
/// data passed to it via callbacks.
///
/// This function will return NULL if the requested memory region could not be allocated.
char* hyperion_alloc(size_t _size) HYPC_NOEXCEPT;

/// Explicitly frees the memory (@p _data) that was being allocated with hyperion_alloc()
/// or returned by a call to hyperion_compile().
///
/// Important, this call will abort() in case of any invalid argument being passed to this call.
void hyperion_free(char* _data) HYPC_NOEXCEPT;

/// Takes a "Standard Input JSON" and an optional callback (can be set to null). Returns
/// a "Standard Output JSON". Both are to be UTF-8 encoded.
///
/// @param _input The input JSON to process.
/// @param _readCallback The optional callback pointer. Can be NULL, but if not NULL,
///                      it can be called by the compiler to request additional input.
///                      Please see the documentation of the type for details.
/// @param _readContext An optional context pointer passed to _readCallback. Can be NULL.
///
/// @returns A pointer to the result. The pointer returned must be freed by the caller using hyperion_free() or hyperion_reset().
char* hyperion_compile(char const* _input, CStyleReadFileCallback _readCallback, void* _readContext) HYPC_NOEXCEPT;

/// Frees up any allocated memory.
///
/// NOTE: the pointer returned by hyperion_compile as well as any other pointer retrieved via hyperion_alloc()
/// is invalid after calling this!
void hyperion_reset() HYPC_NOEXCEPT;

#ifdef __cplusplus
}
#endif
