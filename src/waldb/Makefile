BUILD_OUTPUT=libwaldb.so

CFLAGS+=   -fPIC -g

INCLUDE+= -I$(ROOT_DIR)/build-pacexi3v2-wifi/tmp/sysroots/pacexi3v2-wifi/usr/include/ \
           

LIB+= -L$(ROOT_DIR)/build-pacexi3v2/tmp/sysroots/pacexi3v2/usr/lib/
LIB+=-ltinyxml

CC=$(GCC)
CXX=$(GXX)

SOURCE+=waldb.cpp

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
	$(CC) -fPIC -O0 $(OBJ) $(LIB) -shared -o $(BUILD_OUTPUT) 

.PHONY : clean
clean:
	-$(RM) $(OBJ) $(DEP) $(BUILD_OUTPUT)

