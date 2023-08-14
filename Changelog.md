# Changelog

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
