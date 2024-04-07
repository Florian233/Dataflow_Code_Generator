#include "Code_Generation.hpp"
#include "Config/config.h"
#include <string>
#include <fstream>

/* Add a simple ORCC compatibility that was copied from the code generated by ORCC.
 * This allows the use the ORCC demo projects for testing.
 */

std::string parse_command_line_input_function =
"#include \"options.h\"\n"
"#include <string.h>\n"
"#include <iostream>\n"
"\n"
"options_t *opt;\n"
"\n"
"void parse_command_line_input(int argc, char *argv[]) {\n"
"\topt = new options_t;\n"
"\t//set default\n"
"\topt->display_flags = 1;\n"
"\topt->nbLoops = -1;\n"
"\topt->nbFrames = -1;\n"
"\topt->nb_processors = 1;\n"
"\topt->enable_dynamic_mapping = 0;\n"
"\topt->nbProfiledFrames = 10;\n"
"\topt->mapping_repetition = 1;\n"
"\topt->print_firings = 0;\n"
"\topt->yuv_file = NULL;\n"
"\topt->input_directory = NULL;\n"
"\topt->input_file = NULL;\n"
"\topt->write_file = NULL;\n"
"\topt->mapping_input_file = NULL;\n"
"\topt->mapping_output_file = NULL;\n"
"//read command line parameters\n"
"\tfor (int i = 1; i < argc; i++) {\n"
"\t\tif (strcmp(argv[i], \"-i\") == 0) {\n"
"\t\t\topt->input_file = argv[++i];\n"
"\t\t}\n"
"\t\telse if (strcmp(argv[i], \"-h\") == 0) {\n"
"\t\t\tstd::cout << \"\\nUsage: %s [options]\\n\"\n"
"\t\t\t\"\\nCommon arguments:\\n\"\n"
"\t\t\t\"-i <file>          Specify an input file.\\n\"\n"
"\t\t\t\"-h                 Print this message.\\n\"\n"
"\t\t\t\"\\nVideo-specific arguments:\\n\"\n"
"\t\t\t\"-f <nb frames>     Set the number of frames to decode before exiting.\\n\"\n"
"\t\t\t\"-n                 Ensure that the display will not be initialized (useful on non-graphic terminals).\\n\"\n"
"\t\t\t\"\\nRuntime arguments:\\n\"\n"
"\t\t\t\"-p <file>          Filename to write the profiling information.\\n\"\n"
"\t\t\t\"-r <nb frames>     Specify the number of frames before mapping or between each mapping {Default : 10}.\\n\"\n"
"\t\t\t\"-a                 Do a new mapping every <nb frames> setted by previous option.\\n\"\n"
"\t\t\t\"\\nOther specific arguments:\\n\"\n"
"\t\t\t\"Depending on how the application has been designed, one of these arguments can be used.\\n\"\n"
"\t\t\t\"-l <nb loops>      Set the number of readings of the input file before exiting.\\n\"\n"
"\t\t\t\"-d <directory>     Set the path when multiple input files are required.\\n\"\n"
"\t\t\t\"-w <file>          Specify a file to write the output stream.\\n\";\n"
"\t\t\texit(0);\n"
"\t\t}\n"
"\t\telse if (strcmp(argv[i], \"-f\") == 0) {\n"
"\t\t\topt->nbFrames = atoi(argv[++i]);\n"
"\t\t}\n"
"\t\telse if (strcmp(argv[i], \"-n\") == 0) {\n"
"\t\t\topt->display_flags = 0;\n"
"\t\t}\n"
"\t\telse if (strcmp(argv[i], \"-p\") == 0) {\n"
"\t\t\topt->profiling_file = argv[++i];\n"
"\t\t}\n"
"\t\telse if (strcmp(argv[i], \"-r\") == 0) {\n"
"\t\t\topt->nbProfiledFrames = atoi(argv[++i]);\n"
"\t\t}\n"
"\t\telse if (strcmp(argv[i], \"-a\") == 0) {\n"
"\t\t\topt->mapping_repetition = -1;\n"
"\t\t}\n"
"\t\telse if (strcmp(argv[i], \"-l\") == 0) {\n"
"\t\t\topt->nbLoops = atoi(argv[++i]);\n"
"\t\t}\n"
"\t\telse if (strcmp(argv[i], \"-d\") == 0) {\n"
"\t\t\topt->input_directory = argv[++i];\n"
"\t\t}\n"
"\t\telse if (strcmp(argv[i], \"-w\") == 0) {\n"
"\t\t\topt->write_file = argv[++i];\n"
"\t\t}\n"
"\t\telse {\n"
"\t\t\tstd::cout << \"Error:Unknown input\" << std::endl;\n"
"\t\t\texit(0);\n"
"\t\t}\n"
"\t}\n"
"}\n";

std::string header = "#ifndef OPTIONS_HEADER\n#define OPTIONS_HEADER\n	//Struct copied from ORCC - the native code seems to use this to read several input options from.\n"
"struct ORCC_options {\n"
"\t/* Video specific options */\n"
"\tchar *input_file;\n"
"\tchar *input_directory;             // Directory for input files.\n"
"\t/* Video specific options */\n"
"\tchar display_flags;              // Display flags\n"
"\tint nbLoops;                         // (Deprecated) Number of times the input file is read\n"
"\tint nbFrames;                       // Number of frames to display before closing application\n"
"\tchar *yuv_file;                      // Reference YUV file\n"
"\t/* Runtime options */\n"
"\t//schedstrategy_et sched_strategy;     // Strategy for the actor scheduling\n"
"\tchar *mapping_input_file;            // Predefined mapping configuration\n"
"\tchar *mapping_output_file;           //\n"
"\tint nb_processors;\n"
"\tint enable_dynamic_mapping;\n"
"\t//mappingstrategy_et mapping_strategy; // Strategy for the actor mapping\n"
"\tint nbProfiledFrames;                // Number of frames to display before remapping application\n"
"\tint mapping_repetition;              // Repetition of the actor remapping\n"
"\t char *profiling_file; // profiling file\n"
"\t char *write_file; // write file\n"
"\t/* Debugging options */\n"
"\tint print_firings;\n"
"};\n"
"typedef struct ORCC_options options_t;\n"
"extern options_t *opt;\n"
"void parse_command_line_input(int argc, char *argv[]);\n"
"#endif";

void Code_Generation::generate_ORCC_compatibility_layer(std::string path) {

	{
		std::ofstream output_file{ path + "/options.h" };
		if (output_file.bad()) {
			throw Code_Generation_Exception{ "Cannot open the file " + path + "/options.h" };
		}
		output_file << header;

		output_file.close();
	}

	std::ofstream source_file{ path + "/orcc_compatibility.cpp" };
	if (source_file.bad()) {
		throw Code_Generation_Exception{ "Cannot open file " + path + "/orcc_compatibility.cpp" };
	}
	source_file << parse_command_line_input_function;
	source_file.close();
}