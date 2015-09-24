
#
# c、cpp混合编译的makefile模板
#


PLAT = none

TARGET_NAME = liblxnet.a
CC = $(CROSS)gcc
CXX = $(CROSS)g++
AR = $(CROSS)ar rcu
RANLIB = $(CROSS)ranlib
STRIP = $(CROSS)strip
RM = rm -f

CFLAGS = $(ADD_CFLAGS)
CXXFLAGS = $(ADD_CXXFLAGS)

ADD_CFLAGS = 
ADD_CXXFLAGS = 

ADD_INCS = 
ADD_LIBS = 


SRC_INCS = -I"./../../base/" -I"./src/buf/" -I"./src/event/" -I"./src/sock/" -I"./../../3rd/quicklz/"

SRC_LIBS = 


C_SRC_ALL = $(wildcard  ./*.c ./../../base/crosslib.c ./../../base/log.c ./../../base/pool.c ./../../base/catomic.c ./../../base/cthread.c ./../../base/cthread_pool.c ./../../base/buf/*.c ./src/buf/*.c ./src/event/net_module.c ./src/event/net_eventmgr.c ./src/sock/*.c ./../../3rd/quicklz/*.c)

CXX_SRC_ALL = $(wildcard ./*.cpp)




PLATS = win32-debug win32-release linux-debug linux-release
none:
	@echo "Please choose a platform:"
	@echo " $(PLATS)"

win32-debug:
	$(MAKE) all ADD_INCS="" ADD_LIBS="" ADD_CFLAGS="-Wall -march=i686 -DWIN32 -DDEBUG -g" ADD_CXXFLAGS="-Wall -march=i686 -DWIN32 -DDEBUG -g"

win32-release:
	$(MAKE) all ADD_INCS="" ADD_LIBS="" ADD_CFLAGS="-Wall -march=i686 -DWIN32 -DNDEBUG -O2" ADD_CXXFLAGS="-Wall -march=i686 -DWIN32 -DNDEBUG -O2"


linux-debug:
	$(MAKE) all ADD_INCS=-I"/usr/include" ADD_LIBS=-L"/usr/lib" ADD_CFLAGS="-fPIC -Wall -DDEBUG -g" ADD_CXXFLAGS="-fPIC -Wall -DDEBUG -g"

linux-release:
	$(MAKE) all ADD_INCS=-I"/usr/include" ADD_LIBS=-L"/usr/lib" ADD_CFLAGS="-fPIC -Wall -DNDEBUG -O2" ADD_CXXFLAGS="-fPIC -Wall -DNDEBUG -O2"

all:$(TARGET_NAME)


echo:
	@echo "PLAT = $(PLAT)"
	@echo "CC = $(CC)"
	@echo "CXX = $(CXX)"
	@echo "AR = $(AR)"
	@echo "RANLIB = $(RANLIB)"
	@echo "RM = $(RM)"
	@echo "ADD_CFLAGS = $(ADD_CFLAGS)"
	@echo "ADD_CXXFLAGS = $(ADD_CXXFLAGS)"



C_OBJ_ALL := $(C_SRC_ALL:.c=.o)
CXX_OBJ_ALL := $(CXX_SRC_ALL:.cpp=.o)

#$(OBJS):%.o :%.c  先用$(OBJS)中的一项，比如foo.o: %.o : %.c  含义为:试着用%.o匹配foo.o。如果成功%就等于foo。如果不成功，Make就会警告，然后。给foo.o添加依赖文件foo.c(用foo替换了%.c里的%)
# $@--目标文件，$^--所有的依赖文件，$<--第一个依赖文件。每次$< $@ 代表的值就是列表中的
$(C_OBJ_ALL) : %.o: %.c
	$(CC) -c $< -o $@ $(ADD_INCS) $(SRC_INCS) $(CFLAGS)
$(CXX_OBJ_ALL) : %.o: %.cpp
	$(CXX) -c $< -o $@ $(ADD_INCS) $(SRC_INCS) $(CXXFLAGS)


$(TARGET_NAME): $(C_OBJ_ALL) $(CXX_OBJ_ALL)
	$(AR) $(TARGET_NAME) $(C_OBJ_ALL) $(CXX_OBJ_ALL)
	$(RANLIB) $(TARGET_NAME)
	$(RM) $(C_OBJ_ALL) $(CXX_OBJ_ALL)


.PHONY: all $(PLATS) clean cleanall echo

clean:
	$(RM) $(TARGET_NAME) $(C_OBJ_ALL) $(CXX_OBJ_ALL)

cleanall:
	$(RM) $(TARGET_NAME) $(C_OBJ_ALL) $(CXX_OBJ_ALL)

