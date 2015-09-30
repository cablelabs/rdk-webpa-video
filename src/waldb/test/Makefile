$(CC)=gcc

#CFLAGS+=-I/home/saravanan/work/xone/stable_recent/opensource/src/tinyxml
#CFLAGS+=-I/home/saravanan/work/xone/stable_recent//ave/third_party/zlib/
CFLAGS+=-I../
CXXFLAGS=$(CFLAGS)
LIB=-L../

OBJCPP=$(SOURCE:.cpp=.o)
OBJ=$(OBJCPP:.c=.o)

waldb_test.o:waldb_test.c
	$(CC) -g -c $(CXXFLAGS) $(INCLUDE) waldb_test.c

all: build

waldb_test: clean waldb_test.o
	$(CC) -g waldb_test.o $(LIB) -o waldb_test -lwaldb

.PHONY : clean
clean:
	$(RM) $(OBJ) waldb_test 


