# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.22

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:

#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:

# Disable VCS-based implicit rules.
% : %,v

# Disable VCS-based implicit rules.
% : RCS/%

# Disable VCS-based implicit rules.
% : RCS/%,v

# Disable VCS-based implicit rules.
% : SCCS/s.%

# Disable VCS-based implicit rules.
% : s.%

.SUFFIXES: .hpux_make_needs_suffix_list

# Command-line flag to silence nested $(MAKE).
$(VERBOSE)MAKESILENT = -s

#Suppress display of executed commands.
$(VERBOSE).SILENT:

# A target that is always out of date.
cmake_force:
.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /usr/bin/cmake

# The command to remove a file.
RM = /usr/bin/cmake -E rm -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/student/cmu15746-project2/src

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/student/cmu15746-project2/src/build

# Include any dependencies generated for this target.
include CMakeFiles/cloudfs.dir/depend.make
# Include any dependencies generated by the compiler for this target.
include CMakeFiles/cloudfs.dir/compiler_depend.make

# Include the progress variables for this target.
include CMakeFiles/cloudfs.dir/progress.make

# Include the compile flags for this target's objects.
include CMakeFiles/cloudfs.dir/flags.make

CMakeFiles/cloudfs.dir/cloud-lib/cloudapi.cc.o: CMakeFiles/cloudfs.dir/flags.make
CMakeFiles/cloudfs.dir/cloud-lib/cloudapi.cc.o: ../cloud-lib/cloudapi.cc
CMakeFiles/cloudfs.dir/cloud-lib/cloudapi.cc.o: CMakeFiles/cloudfs.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/student/cmu15746-project2/src/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object CMakeFiles/cloudfs.dir/cloud-lib/cloudapi.cc.o"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT CMakeFiles/cloudfs.dir/cloud-lib/cloudapi.cc.o -MF CMakeFiles/cloudfs.dir/cloud-lib/cloudapi.cc.o.d -o CMakeFiles/cloudfs.dir/cloud-lib/cloudapi.cc.o -c /home/student/cmu15746-project2/src/cloud-lib/cloudapi.cc

CMakeFiles/cloudfs.dir/cloud-lib/cloudapi.cc.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/cloudfs.dir/cloud-lib/cloudapi.cc.i"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/student/cmu15746-project2/src/cloud-lib/cloudapi.cc > CMakeFiles/cloudfs.dir/cloud-lib/cloudapi.cc.i

CMakeFiles/cloudfs.dir/cloud-lib/cloudapi.cc.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/cloudfs.dir/cloud-lib/cloudapi.cc.s"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/student/cmu15746-project2/src/cloud-lib/cloudapi.cc -o CMakeFiles/cloudfs.dir/cloud-lib/cloudapi.cc.s

CMakeFiles/cloudfs.dir/cloud-lib/cloudapi_print.cc.o: CMakeFiles/cloudfs.dir/flags.make
CMakeFiles/cloudfs.dir/cloud-lib/cloudapi_print.cc.o: ../cloud-lib/cloudapi_print.cc
CMakeFiles/cloudfs.dir/cloud-lib/cloudapi_print.cc.o: CMakeFiles/cloudfs.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/student/cmu15746-project2/src/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Building CXX object CMakeFiles/cloudfs.dir/cloud-lib/cloudapi_print.cc.o"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT CMakeFiles/cloudfs.dir/cloud-lib/cloudapi_print.cc.o -MF CMakeFiles/cloudfs.dir/cloud-lib/cloudapi_print.cc.o.d -o CMakeFiles/cloudfs.dir/cloud-lib/cloudapi_print.cc.o -c /home/student/cmu15746-project2/src/cloud-lib/cloudapi_print.cc

CMakeFiles/cloudfs.dir/cloud-lib/cloudapi_print.cc.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/cloudfs.dir/cloud-lib/cloudapi_print.cc.i"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/student/cmu15746-project2/src/cloud-lib/cloudapi_print.cc > CMakeFiles/cloudfs.dir/cloud-lib/cloudapi_print.cc.i

CMakeFiles/cloudfs.dir/cloud-lib/cloudapi_print.cc.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/cloudfs.dir/cloud-lib/cloudapi_print.cc.s"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/student/cmu15746-project2/src/cloud-lib/cloudapi_print.cc -o CMakeFiles/cloudfs.dir/cloud-lib/cloudapi_print.cc.s

CMakeFiles/cloudfs.dir/cloudfs/cloudfs.cc.o: CMakeFiles/cloudfs.dir/flags.make
CMakeFiles/cloudfs.dir/cloudfs/cloudfs.cc.o: ../cloudfs/cloudfs.cc
CMakeFiles/cloudfs.dir/cloudfs/cloudfs.cc.o: CMakeFiles/cloudfs.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/student/cmu15746-project2/src/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_3) "Building CXX object CMakeFiles/cloudfs.dir/cloudfs/cloudfs.cc.o"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT CMakeFiles/cloudfs.dir/cloudfs/cloudfs.cc.o -MF CMakeFiles/cloudfs.dir/cloudfs/cloudfs.cc.o.d -o CMakeFiles/cloudfs.dir/cloudfs/cloudfs.cc.o -c /home/student/cmu15746-project2/src/cloudfs/cloudfs.cc

CMakeFiles/cloudfs.dir/cloudfs/cloudfs.cc.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/cloudfs.dir/cloudfs/cloudfs.cc.i"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/student/cmu15746-project2/src/cloudfs/cloudfs.cc > CMakeFiles/cloudfs.dir/cloudfs/cloudfs.cc.i

CMakeFiles/cloudfs.dir/cloudfs/cloudfs.cc.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/cloudfs.dir/cloudfs/cloudfs.cc.s"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/student/cmu15746-project2/src/cloudfs/cloudfs.cc -o CMakeFiles/cloudfs.dir/cloudfs/cloudfs.cc.s

CMakeFiles/cloudfs.dir/cloudfs/main.cc.o: CMakeFiles/cloudfs.dir/flags.make
CMakeFiles/cloudfs.dir/cloudfs/main.cc.o: ../cloudfs/main.cc
CMakeFiles/cloudfs.dir/cloudfs/main.cc.o: CMakeFiles/cloudfs.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/student/cmu15746-project2/src/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_4) "Building CXX object CMakeFiles/cloudfs.dir/cloudfs/main.cc.o"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT CMakeFiles/cloudfs.dir/cloudfs/main.cc.o -MF CMakeFiles/cloudfs.dir/cloudfs/main.cc.o.d -o CMakeFiles/cloudfs.dir/cloudfs/main.cc.o -c /home/student/cmu15746-project2/src/cloudfs/main.cc

CMakeFiles/cloudfs.dir/cloudfs/main.cc.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/cloudfs.dir/cloudfs/main.cc.i"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/student/cmu15746-project2/src/cloudfs/main.cc > CMakeFiles/cloudfs.dir/cloudfs/main.cc.i

CMakeFiles/cloudfs.dir/cloudfs/main.cc.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/cloudfs.dir/cloudfs/main.cc.s"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/student/cmu15746-project2/src/cloudfs/main.cc -o CMakeFiles/cloudfs.dir/cloudfs/main.cc.s

CMakeFiles/cloudfs.dir/cloudfs/util.cc.o: CMakeFiles/cloudfs.dir/flags.make
CMakeFiles/cloudfs.dir/cloudfs/util.cc.o: ../cloudfs/util.cc
CMakeFiles/cloudfs.dir/cloudfs/util.cc.o: CMakeFiles/cloudfs.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/student/cmu15746-project2/src/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_5) "Building CXX object CMakeFiles/cloudfs.dir/cloudfs/util.cc.o"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT CMakeFiles/cloudfs.dir/cloudfs/util.cc.o -MF CMakeFiles/cloudfs.dir/cloudfs/util.cc.o.d -o CMakeFiles/cloudfs.dir/cloudfs/util.cc.o -c /home/student/cmu15746-project2/src/cloudfs/util.cc

CMakeFiles/cloudfs.dir/cloudfs/util.cc.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/cloudfs.dir/cloudfs/util.cc.i"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/student/cmu15746-project2/src/cloudfs/util.cc > CMakeFiles/cloudfs.dir/cloudfs/util.cc.i

CMakeFiles/cloudfs.dir/cloudfs/util.cc.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/cloudfs.dir/cloudfs/util.cc.s"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/student/cmu15746-project2/src/cloudfs/util.cc -o CMakeFiles/cloudfs.dir/cloudfs/util.cc.s

CMakeFiles/cloudfs.dir/cloudfs/metadata.cc.o: CMakeFiles/cloudfs.dir/flags.make
CMakeFiles/cloudfs.dir/cloudfs/metadata.cc.o: ../cloudfs/metadata.cc
CMakeFiles/cloudfs.dir/cloudfs/metadata.cc.o: CMakeFiles/cloudfs.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/student/cmu15746-project2/src/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_6) "Building CXX object CMakeFiles/cloudfs.dir/cloudfs/metadata.cc.o"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT CMakeFiles/cloudfs.dir/cloudfs/metadata.cc.o -MF CMakeFiles/cloudfs.dir/cloudfs/metadata.cc.o.d -o CMakeFiles/cloudfs.dir/cloudfs/metadata.cc.o -c /home/student/cmu15746-project2/src/cloudfs/metadata.cc

CMakeFiles/cloudfs.dir/cloudfs/metadata.cc.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/cloudfs.dir/cloudfs/metadata.cc.i"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/student/cmu15746-project2/src/cloudfs/metadata.cc > CMakeFiles/cloudfs.dir/cloudfs/metadata.cc.i

CMakeFiles/cloudfs.dir/cloudfs/metadata.cc.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/cloudfs.dir/cloudfs/metadata.cc.s"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/student/cmu15746-project2/src/cloudfs/metadata.cc -o CMakeFiles/cloudfs.dir/cloudfs/metadata.cc.s

CMakeFiles/cloudfs.dir/cloudfs/data.cc.o: CMakeFiles/cloudfs.dir/flags.make
CMakeFiles/cloudfs.dir/cloudfs/data.cc.o: ../cloudfs/data.cc
CMakeFiles/cloudfs.dir/cloudfs/data.cc.o: CMakeFiles/cloudfs.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/student/cmu15746-project2/src/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_7) "Building CXX object CMakeFiles/cloudfs.dir/cloudfs/data.cc.o"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT CMakeFiles/cloudfs.dir/cloudfs/data.cc.o -MF CMakeFiles/cloudfs.dir/cloudfs/data.cc.o.d -o CMakeFiles/cloudfs.dir/cloudfs/data.cc.o -c /home/student/cmu15746-project2/src/cloudfs/data.cc

CMakeFiles/cloudfs.dir/cloudfs/data.cc.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/cloudfs.dir/cloudfs/data.cc.i"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/student/cmu15746-project2/src/cloudfs/data.cc > CMakeFiles/cloudfs.dir/cloudfs/data.cc.i

CMakeFiles/cloudfs.dir/cloudfs/data.cc.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/cloudfs.dir/cloudfs/data.cc.s"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/student/cmu15746-project2/src/cloudfs/data.cc -o CMakeFiles/cloudfs.dir/cloudfs/data.cc.s

# Object files for target cloudfs
cloudfs_OBJECTS = \
"CMakeFiles/cloudfs.dir/cloud-lib/cloudapi.cc.o" \
"CMakeFiles/cloudfs.dir/cloud-lib/cloudapi_print.cc.o" \
"CMakeFiles/cloudfs.dir/cloudfs/cloudfs.cc.o" \
"CMakeFiles/cloudfs.dir/cloudfs/main.cc.o" \
"CMakeFiles/cloudfs.dir/cloudfs/util.cc.o" \
"CMakeFiles/cloudfs.dir/cloudfs/metadata.cc.o" \
"CMakeFiles/cloudfs.dir/cloudfs/data.cc.o"

# External object files for target cloudfs
cloudfs_EXTERNAL_OBJECTS =

cloudfs: CMakeFiles/cloudfs.dir/cloud-lib/cloudapi.cc.o
cloudfs: CMakeFiles/cloudfs.dir/cloud-lib/cloudapi_print.cc.o
cloudfs: CMakeFiles/cloudfs.dir/cloudfs/cloudfs.cc.o
cloudfs: CMakeFiles/cloudfs.dir/cloudfs/main.cc.o
cloudfs: CMakeFiles/cloudfs.dir/cloudfs/util.cc.o
cloudfs: CMakeFiles/cloudfs.dir/cloudfs/metadata.cc.o
cloudfs: CMakeFiles/cloudfs.dir/cloudfs/data.cc.o
cloudfs: CMakeFiles/cloudfs.dir/build.make
cloudfs: /usr/lib/x86_64-linux-gnu/libssl.so
cloudfs: /usr/lib/x86_64-linux-gnu/libcrypto.so
cloudfs: archive-lib/libarchive-util.a
cloudfs: dedup-lib/libdedup.a
cloudfs: /usr/lib/x86_64-linux-gnu/libssl.so
cloudfs: /usr/lib/x86_64-linux-gnu/libcrypto.so
cloudfs: CMakeFiles/cloudfs.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/home/student/cmu15746-project2/src/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_8) "Linking CXX executable cloudfs"
	$(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/cloudfs.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
CMakeFiles/cloudfs.dir/build: cloudfs
.PHONY : CMakeFiles/cloudfs.dir/build

CMakeFiles/cloudfs.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/cloudfs.dir/cmake_clean.cmake
.PHONY : CMakeFiles/cloudfs.dir/clean

CMakeFiles/cloudfs.dir/depend:
	cd /home/student/cmu15746-project2/src/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/student/cmu15746-project2/src /home/student/cmu15746-project2/src /home/student/cmu15746-project2/src/build /home/student/cmu15746-project2/src/build /home/student/cmu15746-project2/src/build/CMakeFiles/cloudfs.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : CMakeFiles/cloudfs.dir/depend
