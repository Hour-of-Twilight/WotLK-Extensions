# Run in script mode on every build so the hash tracks HEAD without a reconfigure.
# Expects HOTDLL_SOURCE_DIR, HOTDLL_TEMPLATE and HOTDLL_OUTPUT on the command line.

set(HOTDLL_COMMIT_HASH "0000000000000000000000000000000000000000")
set(HOTDLL_COMMIT_DIRTY 0)

find_package(Git QUIET)
if(GIT_FOUND)
	execute_process(
		COMMAND "${GIT_EXECUTABLE}" rev-parse HEAD
		WORKING_DIRECTORY "${HOTDLL_SOURCE_DIR}"
		OUTPUT_VARIABLE _hash
		OUTPUT_STRIP_TRAILING_WHITESPACE
		ERROR_QUIET
		RESULT_VARIABLE _hash_result
	)
	# CMake's regex engine has no bounded repetition, so check the length separately.
	string(LENGTH "${_hash}" _hash_length)
	if(_hash_result EQUAL 0 AND _hash_length EQUAL 40 AND _hash MATCHES "^[0-9a-fA-F]+$")
		string(TOLOWER "${_hash}" HOTDLL_COMMIT_HASH)
	endif()

	execute_process(
		COMMAND "${GIT_EXECUTABLE}" status --porcelain --untracked-files=no
		WORKING_DIRECTORY "${HOTDLL_SOURCE_DIR}"
		OUTPUT_VARIABLE _dirty
		OUTPUT_STRIP_TRAILING_WHITESPACE
		ERROR_QUIET
	)
	if(NOT _dirty STREQUAL "")
		set(HOTDLL_COMMIT_DIRTY 1)
	endif()
endif()

# configure_file only rewrites the output when the content actually changes, so this
# does not force a rebuild of everything on each run.
configure_file("${HOTDLL_TEMPLATE}" "${HOTDLL_OUTPUT}" @ONLY)
