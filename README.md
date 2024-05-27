# Code Generator for Dataflow Networks

This code generator can convert dataflow networks specified in CAL and XDF to C/C++ code.
The scope of this tool is to generate multi-threaded software as dataflow networks are well
suited for this purpose.
It shall serve as exploration tool for different optimization and mapping strategies.

This project is work in progress and, therefore, proper functionality cannot be guaranteed.

Please note that this tool can convert CAL+XDF networks to C/C++, but it requires valid
specifications as input!
Only partial checks are applied, in the worst case the tool might occasionally assume that
the input is a completely valid CAL+XDF specification.


## Code Generator Design

      |-----------------------|
      |                       |
      |    Network Reader     |
      |                       |
      |-----------------------|
                |
                |
                |
      |-----------------------|
      |                       |
      |   IR Transformation   |
      |                       |
      |-----------------------|
                |
                |
                |
      |-----------------------|
      |                       |
      |  Actor Classification |
      |                       |
      |-----------------------|
                |
                |
                |
      |-----------------------|
      |                       |
      |  Optimization Phase 1 |
      |                       |
      |-----------------------|
                |
                |
                |
      |-----------------------|
      |                       |
      |     Actor Mapping     |
      |                       |
      |-----------------------|
                |
                |
                |
      |-----------------------|
      |                       |
      |  Optimization Phase 2 |
      |                       |
      |-----------------------|
                |
                |
                |
      |-----------------------|
      |                       |
      |   Code Generation     |
      |                       |
      |-----------------------|
	  
	  
**Network Reader**: In this step the network is read for the CAL and XDF files. Hierarchical networks are made flat and network instances and network ports are replaced by the instances and connections they contain.

**IR Transform**: Transform the read network and actors into a representation that is used throughout the whole code generation and all of the necessary preprocessings.

**Actor Classification**: The actors are classified into different classes, e.g. static and dynamic. Input and output behaviour are classified separately.

**Otimization Phase 1**: Optimization that should be carried out before computing the mapping, e.g. the deletion of certain actors or connections. This might simply the mapping or avoid a mapping based on a changing network.

**Actor Mapping**: Computing a mapping for the actor instances to the cores or read a mapping for a file. This is an essential step for this code generator as multithreaded software is generated. Without a mapping atomics have to be used to guarantee that an actor instance is only execute on one core at a time. This significantly reduces performance though.

**Optimization Phase 2**: Optimization that should be done after the mapping is computed, e.g. if it depends on the mapping for instance to perform actor merging.

**Code Generation**: Code generation for the actors (class including functions, actions, variables and local scheduler), the main (initialize channels, actors, call their init methods and global scheduling), cmake, and buffer implementation. The generated actor code includes one class per actor variant and not one class per actor instance, this significantly reduces the generated code size. The actor variant with the least changes is named like the actor. Other versions get version specific names. Different versions might be generated with actor ports or actions are deleted or changed by optimization or for unconnected ports. For local scheduler generation the actor classification are taken into consideration. If the actor is static or static for each state the local scheduler fetches the tokens already and passes them as argument to the actions (functions generated for them). This simplifies guard condition computation. Currently, the generated code for actors consists of only one header per actor. Splitting the code into header and source files would make re-compilation of the generated project more efficient.
	  

### Directory Structure

The top-level directories of this repo:

* **Reader**: Read the network and actors
* **rapidxml-1.13**: Code used to read the XML files, the code is not developed in the scope of this project and is an external dependency: https://github.com/Fe-Bell/RapidXML
* **IR**: Transform the read network to some intermediate representation, e.g. extracting token rates, guard conditions, FSM, priorities, ...
* **common**: Common (header) files, e.g. for Exceptions and some Helpers
* **Config**: Configuration (storing the configuration made by command line options) and Debug Defines header
* **Tokenizer**: Tokenizer and Token Buffers to temporarily postpone their processing
* **Actor_Classification**: Classification of the actors into static, cyclo-static, ... , dynamic. For all classifications read the header in this directory
* **Conversion**: Library to convert CAL to C/C++, provides also code to check for unsupported features
* **Optimization_Phase1**: Optimization phase 1 code
* **Optimization_Phase2**: Optimization phase 2 code
* **Mapping**: Mapping computation
* **Code_Generation**: Code generation for the actors (class including functions, actions, variables and local scheduler), the main (initialize channels, actors, call their init methods and global scheduling), cmake, and buffer implementation. This directory contains a subdirectory for *scheduling* including a lib that can be used to implement new scheduling strategies.

### Adding Mapping, Optimization and Scheduling Strategies
When adding further optimizations or mapping or scheduling strategies please consider adding a command line option to enable/disable it.
To do so please it to the help printout and command line options processing in *Code_Generator.cpp*. For mapping and scheduling command line options already exist that can be extended. To make the configuration options accessible to the other components where the correspondig functionality shall be implement please add it to *Config/config.h*.

* **Mapping**: Add a new file to the *Mapping* directory, add the core function to *Mapping.h* and make this function being called by *Mapping.cpp* if the corresponding configuration option is set.
* **Scheduling**: Add a new file to the *Code_Generation/Scheduling* directory, add the core function to *Scheduling.hpp* and make this function being called by *Global_Scheduling.cpp* and *Local_Scheduling.cpp* if the corresponding configuration option is set. Please consider checking the *Scheduling_Lib.hpp* header for functions that can be used to ease scheduler generation implementation.
* **Optimization**: Add a new file to either the *Optimization_Phase1* or *Optimization_Phase2* directory depending on whether the optimization shall be performed before or after mapping. Add the core function to either *Optimization_Phase1.hpp* or *Optimization_Phase2.hpp* and make it being called by either *Optimization_Phase1.cpp* or "Optimization_Phase2.cpp* if the corresponding configuration option is set.

### Remarks about C/C++ Code Inclusion
Function declarations of functions external to CAL can be marked by *@native*.
The code generator converts them into extern "C" function declarations.
If a different linking is required the declaration can be adjusted manually or
before compiling the code generator in the its sources to generate a different declaration.

## Unsupported CAL Features
The following CAL features are not supported:
* Choose
* Set
* Collection
* Map
* Time and Delay (Use state variables instead)
* multichannel access (at and all keywords for channel access not supported)
* Regular Expression Schedules (Priority schedules and FSM are supported, use them instead)
* old (use temporary variable instead)
* let
* mutable (assignment operators = and := are supported, that should be sufficient)
* Type inference for actors
* lambda (use function or procedure instead)
* import with renaming
* Tokenrates of channels must be constant per action, they can depend on imported constants but not on other variables

## Command Line Options
The code generator provides the following command line options:

### Common

* -h / --help : Print the help message that also displays all command line options
* -w \<directory\> : Specify the output directory for the generated code
* -d \<directory\> : Specifiy the directory of the sources
* -n \<file\> : Specifiy the top network that shall be converted to the target language
* --orcc : Add ORCC compatibility, the generated code can parse the command lines and provides the options.h header that is required by the majority of the native code. This code is copied from ORCC generated code. It allows to use the ORCC demo applications, e.g. for testing.
* --cmake : Generates a simple CMake file to build the generated project. Additional files, e.g. required to fulfill the @native references must be added manually!
* --static_alloc : Allocate channels and actions in the main statically and avoid usage of the new operator
* --abi=\<stdc|stdcpp\> : Target ABI/language, c++ is the default

### Communication Channels
* -s \<number\> : The default size of the buffers. This size is used if no optimization step determines another size or the size of the channel is given in the XDF file.

### OpenMP
* --omp_tasking : Uses OpenMP tasking instead of std::thread for parallel execution of the actors/global schedulers. This feature is rather experimental currently.

### Mapping
* -c \<number\> : Specifiy the number of cores to use. This shall determine the number of clusters/partitions created during the mapping process.
* --map=(all|lft|random)
  * all: Map all actor instances to all cores. This is the default. This options doesn't lead to a proper mapping. Instead the generated global scheduling routines for each core can execute all actor instances. To ensure execution of an actor instance only once at a time atomics are used (only if more than one core is used). Atomics deacrease the performance significantly!
  * random: Distribute actor instances round-robin amoung the cores, list of actor instances is not sorted, order unknown.
  * lft: Different methods based on the last finish time of actor instances, can take weights into consideration.
* --map_file \<file\> : Uses the mapping for file. Ignores -c and --map. The file has to contain an XML description of the partitioning in the following form: \<Mapping\> \<Cluster\> \<Node name="some_actor_instance_name"/\>...\</Cluster\>\<multiple cluster containing multiple nodes\>\</Mapping\>   
* --output_nodes_file \<file\>: Uses the nodes defined in the file as output nodes of the network, this is where the computation of the last finish times starts. The file has to contain an XML description in the following form: \<Mapping\> \<Output\> \<Node name="some_actor_instance_name"\>...\</Output\>\</Mapping\>
* --input_nodes_file \<file\>: Uses the nodes defined in the file as input nodes of the network, this is where the computation of the earliest start times starts. The file has to contain an XML description in the following form: \<Mapping\> \<Input\> \<Node name="some_actor_instance_name"\>...\</Input\>\</Mapping\>
* --map_weights \<file\>: Uses the weights defined in the file as weights for the actor instances for mapping. The file has to contain an XML description in the following form: \<Mapping\> \<Weights\> \<Weight name="some_actor_instance_name" Value=X\>...\</Weights\>\</Mapping\>

### Scheduling
* --topology_sort : Use topology sorting for the list of actor instances before generating the global scheduler. This might lead to less calls of local schedulers that return without peforming any firings of the corresponding actor instance. With a topologically sorted list for scheduling the actor instances might be executed in the order or the token flow.
* --schedule=(non_preemptive|round_robin)
  * non_preemptive : Use non-preemptive scheduling. This is the default. Actor instances are executed as long as they can fire, only after all possible firings are done they return to global scheduling.
  * round_robin: Use round-robin scheduling. Actor instances can only fire one action, then they return to the global scheduler.
* --list_schedule : Use a list for scheduling instead of hard-coded order of local scheduler calls in the code. This produces more flexible code, but has no specific purpose.
* --bound_sched \<number\>: Execute the local scheduler a bounded number of times at maximum before returning.

### Optimizations
* --prune_unconnected : Remove unconnected channels from actors instances and generate separate code for them. Otherwise the unconnected channels are set to nullptr for the constructor parameters. This feature is rather experimental and not properly tested! Reading tokens from ports without an attached channel are replaced by using variables initalized with zero or uninitialized arrays.
* --opt_sched : Fetch channel sizes before entering the local scheduler loop and use this values for scheduling. This avoids reading the channel sizes during each scheduler iteration, in the worst-case several times.

## Future Work
* Implementation of Actor Merging
* Improved Mapping and Scheduling Strategies
* Dead Code Elimination
* Improved Memory Handling for Channels/Token Transfer
* Profiling Methods

## Bugs and other issues
Please create an Issue on GitHub.
