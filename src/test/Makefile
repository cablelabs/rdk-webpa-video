BUILD_OUTPUT=webpatest

CFLAGS+= -g -O0
INCLUDE+= -I$(ROOT_DIR)/generic/webpa/include \

LIB+= -L$(ROOT_DIR)/build-pacexi3v2/tmp/sysroots/pacexi3v2/usr/lib/ -L. -lwal

CC=$(GCC)
CXX=$(GXX)

SOURCE+=webpa_test.c

CXXFLAGS=$(CFLAGS)

OBJCPP=$(SOURCE:.cpp=.o)
OBJ=$(OBJCPP:.c=.o)
DEPCPP=$(SOURCE:.cpp=.d)
DEP=$(DEPCPP:.c=.d)

.SUFFIXES: .o .c .cpp .cc .cxx .C

.cpp.o:
	$(CXX) -c $(CXXFLAGS) $(INCLUDE) -o "$@" "$<"

.cc.o:
	$(CXX) -c $(CXXFLAGS) $(INCLUDE) -o "$@" "$<"

.cxx.o:
	$(CXX) -c $(CXXFLAGS) $(INCLUDE) -o "$@" "$<"

.C.o:
	$(CC) -c -fPIC $(CXXFLAGS) $(INCLUDE) -o "$@" "$<"

.c.o:
	$(CC) -c -fPIC $(CFLAGS) $(INCLUDE) -o "$@" "$<"

all: build

build: clean $(OBJ)
	$(CC) $(OBJ) $(LIB) -o $(BUILD_OUTPUT) 

.PHONY : clean
clean:
	-$(RM) $(OBJ) $(DEP) $(BUILD_OUTPUT)

