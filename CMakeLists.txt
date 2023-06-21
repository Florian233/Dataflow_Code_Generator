cmake_minimum_required (VERSION 3.8)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

cmake_policy(SET CMP0076 NEW)

project ("Dataflow_Code_Generator")

# Uncomment if you want to build with verbose output
#add_compile_definitions(DEBUG)

add_executable (Dataflow_Code_Generator "Code_Generator.cpp")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET Dataflow_Code_Generator PROPERTY CXX_STANDARD 20)
endif()

set_property(TARGET Dataflow_Code_Generator PROPERTY CXX_STANDARD_REQUIRED 17)

add_subdirectory(Actor_Classification)
add_subdirectory(Tokenizer)
add_subdirectory(Reader)
add_subdirectory(rapidxml-1.13)
add_subdirectory(Optimization_Phase1)
add_subdirectory(Optimization_Phase2)
add_subdirectory(Mapping)
add_subdirectory(IR)
add_subdirectory(Conversion)
add_subdirectory(Config)
add_subdirectory(common)
add_subdirectory(Code_Generation)

target_include_directories(Dataflow_Code_Generator PUBLIC ${CMAKE_CURRENT_LIST_DIR})


