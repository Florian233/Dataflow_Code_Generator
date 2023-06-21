add_subdirectory(Scheduling)

target_sources(Dataflow_Code_Generator PRIVATE
	Action_Conversion.cpp
	Buffer_Creation.cpp
	CMake_Generation.cpp
	Code_Generation.cpp
	Composit_Actor_Creation.cpp
	Core_Creation.cpp
	ORCC_Compatibility.cpp
	Single_Actor_Creation.cpp)