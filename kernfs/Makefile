########
#
#   Simplified Makefile, for each source we call a function (make-goal) to
#	compile, this is done in the last line of this file.
#
########

SUBDIR	:= io conf cache concurrency global ds mlfs storage filesystem distributed experimental .

SRC_DIR   := $(addprefix , $(SUBDIR))
BUILD_DIR := $(addprefix build/, $(SUBDIR))
LIBFS_DIR := $(CURDIR)/../libfs

########
#  Source and Object files in their  module directories
########
SRC	:= $(foreach sdir, $(SRC_DIR), $(wildcard $(sdir)/*.c))
SRCCPP	:= $(foreach sdir, $(SRC_DIR), $(wildcard $(sdir)/*.cpp))
SRC := $(filter-out %mkfs.c %kernf.c,$(SRC))
OBJ	:= $(patsubst %.c,build/%.o,$(SRC))
OBJCPP	:= $(patsubst %.cpp,build/%.o,$(SRCCPP))
HDER := $(foreach sdir, $(SRC_DIR), $(wildcard $(sdir)/*.h)) $(wildcard storage/aio/*.h)

########
#  Lib directories
########
LAIO_DIR := $(abspath $(LIBFS_DIR)/lib/libaio/src)
RDMA_DIR := $(abspath $(LIBFS_DIR)/lib/rdma)
ACONF_DIR := $(abspath $(LIBFS_DIR)/lib/aconf)

INCLUDES  := $(addprefix -I, . $(LAIO_DIR) $(RDMA_DIR) $(ACONF_DIR))

LD_FLAGS = -lpthread -lstdc++ -lm -lrt -L$(RDMA_DIR) -Wl,-rpath=$(abspath $(RDMA_DIR)) -lrdma -lrdmacm -libverbs -ldl -luuid -lnuma -L$(LAIO_DIR) -Wl,-rpath=$(abspath $(LAIO_DIR)) -laio -L$(ACONF_DIR) -Wl,-rpath=$(abspath $(ACONF_DIR)) -laconf -lstdc++

#MLFS_FLAGS = -DUSE_SLAB -DMLFS_INFO -DKERNFS
MLFS_FLAGS = -DKERNFS
#MLFS_FLAGS = -DKERNFS -DMLFS_INFO
#MLFS_FLAGS += -DKERNFS -DMLFS_DEBUG
MLFS_FLAGS += -DDISTRIBUTED
MLFS_FLAGS += -DMASTER
#MLFS_FLAGS += -DENABLE_MEMCPY_OFFLOAD
#MLFS_FLAGS += -DENABLE_PERF_MODEL 
#MLFS_FLAGS += -DUSE_LEASE
#MLFS_FLAGS += -DLEASE_OPT
MLFS_FLAGS += -DLEASE_MIGRATION
#MLFS_FLAGS += -DACTIVE
MLFS_FLAGS += -DBALLOC
#MLFS_FLAGS += -DDIGEST_OPT
#MLFS_FLAGS += -DIOMERGE
#MLFS_FLAGS += -DCONCURRENT
#MLFS_FLAGS += -DFCONCURRENT
MLFS_FLAGS += -DUSE_SSD
#MLFS_FLAGS += -DUSE_HDD
MLFS_FLAGS += -DMIGRATION
#MLFS_FLAGS += -DEXPERIMENTAL

DEBUG = -g -O2
#DEBUG = -g -O2
#DEBUG = -g3 -Ofast
#DEBUG = -O3

########
#  vpath and compile function for each file
########
vpath %.c $(SRC_DIR)
vpath %.cpp $(SRC_DIR)

#CC = clang-3.8
CC = gcc
CXX = g++ -std=c++11

define make-goal
$1/%.o: %.c
	$(CC) $(INCLUDES) $(DEBUG) -fPIC -c $$< -o $$@ -DKERNFS $(MLFS_FLAGS)
$1/%.o: %.cpp
	$(CXX) $(INCLUDES) $(DEBUG) -fPIC -c $$< -o $$@ -DKERNFS $(MLFS_FLAGS)
endef

########
#  Phony targets
########
.PHONY: all checkdirs clean filesystem

all: $(BUILD_DIR) filesystem

clean:
	@rm -rf ./build

########
#  Create dirs recursively
########
$(BUILD_DIR):
	@mkdir -p $@

########
#  Targets
########
filesystem: $(OBJ) $(OBJCPP) $(SRC) $(HDER)
	$(CC) -shared $(DEBUG) -o build/libkernfs.so $(OBJ) $(OBJCPP) -Wl,--no-whole-archive $(LD_FLAGS) $(MLFS_FLAGS)

########
#  Compile each source into an object
########
$(foreach bdir,$(BUILD_DIR),$(eval $(call make-goal,$(bdir))))
