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
include snapshot/CMakeFiles/snapshot.dir/depend.make
# Include any dependencies generated by the compiler for this target.
include snapshot/CMakeFiles/snapshot.dir/compiler_depend.make

# Include the progress variables for this target.
include snapshot/CMakeFiles/snapshot.dir/progress.make

# Include the compile flags for this target's objects.
include snapshot/CMakeFiles/snapshot.dir/flags.make

snapshot/CMakeFiles/snapshot.dir/snapshot-test.c.o: snapshot/CMakeFiles/snapshot.dir/flags.make
snapshot/CMakeFiles/snapshot.dir/snapshot-test.c.o: ../snapshot/snapshot-test.c
snapshot/CMakeFiles/snapshot.dir/snapshot-test.c.o: snapshot/CMakeFiles/snapshot.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/student/cmu15746-project2/src/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building C object snapshot/CMakeFiles/snapshot.dir/snapshot-test.c.o"
	cd /home/student/cmu15746-project2/src/build/snapshot && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -MD -MT snapshot/CMakeFiles/snapshot.dir/snapshot-test.c.o -MF CMakeFiles/snapshot.dir/snapshot-test.c.o.d -o CMakeFiles/snapshot.dir/snapshot-test.c.o -c /home/student/cmu15746-project2/src/snapshot/snapshot-test.c

snapshot/CMakeFiles/snapshot.dir/snapshot-test.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/snapshot.dir/snapshot-test.c.i"
	cd /home/student/cmu15746-project2/src/build/snapshot && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /home/student/cmu15746-project2/src/snapshot/snapshot-test.c > CMakeFiles/snapshot.dir/snapshot-test.c.i

snapshot/CMakeFiles/snapshot.dir/snapshot-test.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/snapshot.dir/snapshot-test.c.s"
	cd /home/student/cmu15746-project2/src/build/snapshot && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /home/student/cmu15746-project2/src/snapshot/snapshot-test.c -o CMakeFiles/snapshot.dir/snapshot-test.c.s

# Object files for target snapshot
snapshot_OBJECTS = \
"CMakeFiles/snapshot.dir/snapshot-test.c.o"

# External object files for target snapshot
snapshot_EXTERNAL_OBJECTS =

snapshot/snapshot: snapshot/CMakeFiles/snapshot.dir/snapshot-test.c.o
snapshot/snapshot: snapshot/CMakeFiles/snapshot.dir/build.make
snapshot/snapshot: snapshot/CMakeFiles/snapshot.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/home/student/cmu15746-project2/src/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Linking C executable snapshot"
	cd /home/student/cmu15746-project2/src/build/snapshot && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/snapshot.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
snapshot/CMakeFiles/snapshot.dir/build: snapshot/snapshot
.PHONY : snapshot/CMakeFiles/snapshot.dir/build

snapshot/CMakeFiles/snapshot.dir/clean:
	cd /home/student/cmu15746-project2/src/build/snapshot && $(CMAKE_COMMAND) -P CMakeFiles/snapshot.dir/cmake_clean.cmake
.PHONY : snapshot/CMakeFiles/snapshot.dir/clean

snapshot/CMakeFiles/snapshot.dir/depend:
	cd /home/student/cmu15746-project2/src/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/student/cmu15746-project2/src /home/student/cmu15746-project2/src/snapshot /home/student/cmu15746-project2/src/build /home/student/cmu15746-project2/src/build/snapshot /home/student/cmu15746-project2/src/build/snapshot/CMakeFiles/snapshot.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : snapshot/CMakeFiles/snapshot.dir/depend

