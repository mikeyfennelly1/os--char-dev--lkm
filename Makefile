# pwd at beginning of make command
PROJ_ROOT:=$(shell pwd)
# directory containing sources
SRCS:=$(PROJ_ROOT)/src
# directory containing scripts
SCRIPTS:=$(PROJ_ROOT)/scripts
# directory containing build objects
BUILD:=$(PROJ_ROOT)/build

# default target
#
# build the target .o and .ko files
all: $(BUILD)
	# build kernel module from sources in ./src directory
	make -C /lib/modules/$(shell uname -r)/build M=$(PROJ_ROOT)/src modules
	
	# move targets out to build directory
	cd $(SRCS) && $(SCRIPTS)/moveTargetsToBuild.sh > /dev/null

	# remove any other generated files from source directory
	make -C /lib/modules/$(shell uname -r)/build M=$(SRCS) clean > /dev/null
	
# create ./test/bin directory if it doesn't exist
$(TEST_BIN_DIR):
	mkdir -p $(TEST_BIN_DIR)

# create ./build directory if it doesn't exist
$(BUILD):
	@mkdir -p ./build

# remove build directory recursively
clean:
	@rm -rf ./build

# install project dependencies
install:
	sudo apt update
	sudo apt install libcunit1 libcunit1-doc libcunit1-dev
	sudo apt install libjson-c-dev