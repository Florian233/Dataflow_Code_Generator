#pragma once

//Set by preprocessor define
#define DEBUG

#ifdef DEBUG
#define DEBUG_NETWORK_READ
#define DEBUG_IR_TRANSFORMATION
#define DEBUG_CLASSIFICATION
#define DEBUG_OPTIMIZATION
#define DEBUG_MAPPING
#define DEBUG_CODE_GENERATION
#endif

#ifdef DEBUG_NETWORK_READ
#define DEBUG_READER_PRINT_EDGES
#define DEBUG_READER_ACTORS
#endif

#ifdef DEBUG_OPTIMIZATION
#define DEBUG_OPTIMIZATION_PHASE1
#define DEBUG_OPTIMIZATION_PHASE2
#endif

#ifdef DEBUG_CODE_GENERATION
#define DEBUG_MAIN_GENERATION
#define DEBUG_ACTOR_GENERATION
#define DEBUG_SCHEDULER_GENERATION
#endif