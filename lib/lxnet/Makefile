
#
# c.cpp混合编译的makefile模板
#
#

BIN = liblxnet.a
CC = gcc
CPP = g++
RM = rm -f
#系统库的包含路径、库列表
INCS = 
LIBS = 
SUBDIRS =

#源文件包含路径、库列表
SOURCEINC = -I"./../../base/" -I"./../../base/buf/" -I"./src/buf/" -I"./src/event/" -I"./src/sock/" -I"./../../3rd/quicklz/"
#
#
#maintest.c tree/rbtree.c  多了子目录，那就直接添加 目录/*.c即可   所有的源文件--  .c文件列表
CSRCS = $(wildcard  ./*.c ./../../base/crosslib.c ./../../base/log.c ./../../base/pool.c ./../../base/catomic.c ./../../base/cthread.c ./../../base/cthread_pool.c ./src/buf/*.c ./src/event/*.c ./src/sock/*.c ./../../3rd/quicklz/*.c)
CPPSRCS = $(wildcard ./*.cpp)

#
#
#所有的.o文件列表
COBJS := $(CSRCS:.c=.o)
CPPOBJS := $(CPPSRCS:.cpp=.o)
#
#生成依赖信息 -MM是只生成自己的头文件信息，-M 包含了标准库头文件信息。
#-MT 或 -MQ都可以改变生成的依赖  xxx.o:src/xxx.h 为 src/xxx.o:src/xxx.h 当然。前面的 src/xxx.o需自己指定
#格式为 -MM 输入.c或.cpp  查找依赖路径  -MT或-MQ  生成规则，比如src/xxx.o 
MAKEDEPEND = gcc -MM -MT
CFLAGS =
CPPFLAGS =

#-g 生成调试信息
#-pedantic参数与-ansi一起使用 会自动拒绝编译非ANSI程序
#-fomit-frame-pointer 去除函数框架
#-Wmissing-prototypes -Wstrict-prototypes 检查函数原型
#针对每个.c文件的.d依赖文件列表
CDEF = $(CSRCS:.c=.d)
CPPDEF = $(CPPSRCS:.cpp=.d)

PLATS = win32-debug win32-release linux-debug linux-release
none:
	@echo "Please choose a platform:"
	@echo " $(PLATS)"

win32-debug:
	$(MAKE) all INCS=-I"c:/mingw/include" LIBS=-L"c:/mingw/lib" CFLAGS="-Wall -DWIN32 -DDEBUG -g" CPPFLAGS="-Wall -DWIN32 -DDEBUG -g"

win32-release:
	$(MAKE) all INCS=-I"c:/mingw/include" LIBS=-L"c:/mingw/lib" CFLAGS="-Wall -DWIN32 -DNDEBUG -O2" CPPFLAGS="-Wall -DWIN32 -DNDEBUG -O2"

linux-debug:
	$(MAKE) all INCS=-I"/usr/include" LIBS=-L"/usr/lib" CFLAGS="-fPIC -Wall -DDEBUG -g" CPPFLAGS="-fPIC -Wall -DDEBUG -g"

linux-release:
	$(MAKE) all INCS=-I"/usr/include" LIBS=-L"/usr/lib" CFLAGS="-fPIC -Wall -DNDEBUG -O2" CPPFLAGS="-fPIC -Wall -DNDEBUG -O2"

all:$(BIN)

#$(OBJS):%.o :%.c  先用$(OBJS)中的一项，比如foo.o: %.o : %.c  含义为:试着用%.o匹配foo.o。如果成功%就等于foo。如果不成功，
# Make就会警告，然后。给foo.o添加依赖文件foo.c(用foo替换了%.c里的%)
# 也可以不要下面的这个生成规则，因为下面的 include $(DEF)  就隐含了。此处为了明了，易懂。故留着
$(COBJS) : %.o: %.c
	$(CC) -c $< -o $@ $(INCS) $(SOURCEINC) $(CFLAGS)
$(CPPOBJS) : %.o: %.cpp
	$(CPP) -c $< -o $@ $(INCS) $(SOURCEINC) $(CPPFLAGS)

# $@--目标文件，$^--所有的依赖文件，$<--第一个依赖文件。每次$< $@ 代表的值就是列表中的
#
$(BIN) : $(COBJS) $(CPPOBJS)
	ar r $(BIN) $(COBJS) $(CPPOBJS)
	ranlib $(BIN)
	$(RM) $(COBJS) $(CPPOBJS)
# 链接为最终目标


#引入了.o文件对.c和.h的依赖情况。以后.h被修改也会重新生成，可看看.d文件内容即知道为何
#引入了依赖就相当于引入了一系列的规则，因为依赖内容例如： 目录/xxx.o:目录/xxx.c 目录/xxx.h 也相当于隐含的引入了生成规则
#故上面不能在出现如： $(OBJS) : $(DEF)之类。切记
#include $(CDEF) $(CPPDEF)
.PHONY:clean cleanall

#清除所有目标文件以及生成的最终目标文件
clean:			
	$(RM) $(BIN) $(COBJS) $(CPPOBJS)
#rm *.d
cleanall:
	$(RM) $(BIN) $(COBJS) $(CPPOBJS)

