########
#
#   Simplified Makefile, for each source we call a function (make-goal) to
#	compile, this is done in the last line of this file.
#
########

#CC=clang-3.8
#CC=gcc
CXX=g++ --std=c++11
CC=gcc

########
#  List of all modules' directories
########
MODULES   := io concurrency config filesystem global ds log mkfs \
	mlfs posix storage distributed experimental intf

########
#  List of all modules' directories
########

########
#  All modules' directories in src and build
########
SRC_DIR   := $(addprefix src/,$(MODULES))
BUILD_DIR := $(addprefix build/,$(MODULES))

########
#  Source and Object files in their  module directories
########
SRC       := $(foreach sdir,$(SRC_DIR),$(wildcard $(sdir)/*.c))
SRCCPP    := $(foreach sdir,$(SRC_DIR),$(wildcard $(sdir)/*.cpp))
SRC				:= $(filter-out src/mkfs/mkfs.c, $(SRC))
OBJ       := $(patsubst src/%.c,build/%.o,$(SRC))
OBJCPP    := $(patsubst src/%.cpp,build/%.o,$(SRCCPP))

########
#  Lib directories
########
LAIO_DIR := $(abspath $(CURDIR)/lib/libaio/src)
RDMA_DIR := $(abspath $(CURDIR)/lib/rdma)

INCLUDES  := $(addprefix -I,src/ $(RDMA_DIR) $(LAIO_DIR))

LD_FLAGS = -lpthread -lstdc++ -lm -lnuma -lrt -L$(RDMA_DIR) -lrdmacm -libverbs -lpthread -Wl,-rpath=$(abspath $(RDMA_DIR)) -lrdma -L$(LAIO_DIR) -Wl,-rpath=$(abspath $(LAIO_DIR)) -laio

#MLFS_FLAGS = -DUSE_SLAB -DLIBFS -DMLFS_INFO
MLFS_FLAGS = -DLIBFS
#MLFS_FLAGS = -DLIBFS -DMLFS_INFO
#MLFS_FLAGS = -DLIBFS -DMLFS_POSIX
#MLFS_FLAGS = -DLIBFS -DMLFS_DEBUG
MLFS_FLAGS += -DCONCURRENT
MLFS_FLAGS += -DDISTRIBUTED
MLFS_FLAGS += -DMASTER
#MLFS_FLAGS += -DCC_OPT
#MLFS_FLAGS += -DENABLE_PERF_MODEL 
#MLFS_FLAGS += -DUSE_LEASE
MLFS_FLAGS += -DLAZY_SURRENDER
#MLFS_FLAGS += -DNVM_READ_REDIRECT
#MLFS_FLAGS += -DSSD_READ_REDIRECT
#MLFS_FLAGS += -DHDD_READ_REDIRECT
#MLFS_FLAGS += -DCOALESCING
MLFS_FLAGS += -DINVALIDATION
MLFS_FLAGS += -DKLIB_HASH
MLFS_FLAGS += -DUSE_SSD
#MLFS_FLAGS += -DUSE_HDD
#MLFS_FLAGS += -DMLFS_LOG

#DEBUG = -g3 -O0
#DEBUG = -O0 -gdwarf-2 -g3 -rdynamic
DEBUG = -g -O0
#DEBUG = -g -Ofast
#DEBUG = -O3

########
#  vpath and compile function for each file
########
vpath %.c $(SRC_DIR)
vpath %.cpp $(SRC_DIR)

define make-goal
$1/%.o: %.c
	$(CC) $(INCLUDES) $(MLFS_FLAGS) $(DEBUG) -fPIC -c $$< -o $$@ -DLIBFS
$1/%.o: %.cpp
	$(CXX) $(INCLUDES) $(MLFS_FLAGS) $(DEBUG) -fPIC -c $$< -o $$@ -DLIBFS
endef

########
#  Phony targets
########
.PHONY: all checkdirs clean mkfs

all: checkdirs libmlfs mkfs
checkdirs: $(BUILD_DIR)
	@mkdir -p bin

clean:
	@rm -rf $(BUILD_DIR)

########
#  Create dirs recursively
########
$(BUILD_DIR):
	@mkdir -p $@

########
#  Targets
########
#build/libmlfs.a: $(OBJ)
libmlfs: $(OBJ) $(OBJCPP)
	ar cr build/libmlfs.a $(OBJ) $(OBJCPP)
	$(CC) -shared $(DEBUG) -o build/libmlfs.so $(OBJ) $(OBJCPP) $(LD_FLAGS) -DLIBFS $(MLFS_FLAGS)


mkfs: $(OBJ) libmlfs mkfs.c
	$(CC) -g -o build/mkfs.o -c src/mkfs/mkfs.c $(INCLUDES) $(MLFS_FLAGS)
	$(CC) -g -o bin/mkfs.mlfs build/mkfs.o build/libmlfs.a $(INCLUDES) $(LD_FLAGS) -DLIBFS


########
#  Compile each source into an object
########
$(foreach bdir,$(BUILD_DIR),$(eval $(call make-goal,$(bdir))))
