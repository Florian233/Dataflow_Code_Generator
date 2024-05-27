add_subdirectory(ABI)

target_sources(Dataflow_Code_Generator PRIVATE
	Action_Conversion.cpp
	CMake_Generation.cpp
	Converter_RVC_Cpp.cpp
	Composit_Actor_Creation.cpp
	Core_Creation.cpp
	Generate_C_Cpp.cpp
	Global_Scheduling.cpp
	Local_Scheduling.cpp
	ORCC_Compatibility.cpp
	Single_Actor_Creation.cpp)