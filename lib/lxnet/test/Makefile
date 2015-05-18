PLATS = win32-debug win32-release linux-debug linux-release
none:
	@echo "Please choose a platform:"
	@echo " $(PLATS)"

win32-debug:
	g++ -o connect connect.cpp -I"c:/mingw/include" -I"./../" -I"./../../../base" -L"c:/mingw/lib" -Wall -DWIN32 -DDEBUG -g -L"./../" -llxnet -lws2_32
	g++ -o listen listen.cpp -I"c:/mingw/include" -I"./../" -I"./../../../base" -L"c:/mingw/lib" -Wall -DWIN32 -DDEBUG -g -L"./../" -llxnet -lws2_32

win32-release:
	g++ -o connect connect.cpp -I"c:/mingw/include" -I"./../" -I"./../../../base" -L"c:/mingw/lib" -Wall -DWIN32 -DNDEBUG -O2 -L"./../" -llxnet -lws2_32
	g++ -o listen listen.cpp -I"c:/mingw/include" -I"./../" -I"./../../../base" -L"c:/mingw/lib" -Wall -DWIN32 -DNDEBUG -O2 -L"./../" -llxnet -lws2_32

linux-debug:
	g++ -o connect connect.cpp -I"/usr/include" -I"./../" -I"./../../../base" -L"/usr/lib" -Wall -DDEBUG -g -L"./../" -llxnet -lpthread -lrt
	g++ -o listen listen.cpp -I"/usr/include" -I"./../" -I"./../../../base" -L"/usr/lib" -Wall -DDEBUG -g -L"./../" -llxnet -lpthread -lrt


linux-release:
	g++ -o connect connect.cpp -I"/usr/include" -I"./../" -I"./../../../base" -L"/usr/lib" -Wall -DNDEBUG -O2 -L"./../" -llxnet -lpthread -lrt
	g++ -o listen listen.cpp -I"/usr/include" -I"./../" -I"./../../../base" -L"/usr/lib" -Wall -DNDEBUG -O2 -L"./../" -llxnet -lpthread -lrt
