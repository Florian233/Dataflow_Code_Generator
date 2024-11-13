# Changelog

## 1.3.1

### Added
 * No additions, only minor version for bug fixes
 
### Changed
 * Properly check if file open operation failed, and stop operation in this case without reading from a file that wasn't opened properly
 * Check for multi-reader/writer ports/channels and stop operation if present
 * Don't generate FSM enum declaration if no FSM is present

## 1.3

### Added
 * Architecture refactoring for improved support of different target languages
 * C Code generation

### Changed
 * Fixed a bug in action conversion (Thanks Saltuk)
 * Used paths that are OS independent (Thanks Saltuk)
 * Fixed a bug that caused wrong parsing of strings
 * Used LF for all files instead of CRLF

## 1.2.1

### Added
 * No additions, only minor version for bug fixes
 
### Changed
 * Fixed a bug that lead to missing imported variables etc. when using more than one import in an actor
 * Added a missing include to the generated channel implementation
 * Added std::to_string for println parameters that are not strings

## 1.2

### Added
 * Added a flag to limit the local scheduler loop iterations
 * Added a flag to statically allocate actors and channels
 * Added a flag to use optimized local scheduling by fetching the channel sizes and once and using internal bookkeeping instead of reading them during every iteration of the local scheduler.

### Changed
 * Default channel size is not used as define
 * Superflous "true &&" and "&& true" are not added to the local scheduler functions

## 1.1

### Added
 * Added verbosity for each component - not all in use currently
 * Added new network partitioning / actor to core mapping strategies
 * Added round-robin scheduling strategy

### Changed
 * Fixed a bug that lead to missing brackets in the local scheduler generation for the input channel size condition without guard conditions to consider
 * Fixed a bug that lead to fetching of input tokens for any kind of static actors before checking output channel free space conditions
 * Replaced virtual functions for generated channel code by non-virtual functions to increase performance
 * Fixed a bug that lead to wrong replacement of variables in guard conditions by channel preview function calls

## 1.0

### Added
 * Initial version of the code generator

### Changed
 * No changes, only additions.
