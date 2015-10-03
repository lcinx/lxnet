PLATS = win-debug win-release linux-debug linux-release
none:
	@echo "Please choose a platform:"
	@echo " $(PLATS)"

win-debug:
	g++ -o connect connect.cpp -I"./../" -I"./../../../base" -Wall -DWIN32 -DDEBUG -g -L"./../" -llxnet -lws2_32
	g++ -o listen listen.cpp -I"./../" -I"./../../../base" -Wall -DWIN32 -DDEBUG -g -L"./../" -llxnet -lws2_32

win-release:
	g++ -o connect connect.cpp -I"./../" -I"./../../../base" -Wall -DWIN32 -DNDEBUG -O2 -L"./../" -llxnet -lws2_32
	g++ -o listen listen.cpp -I"./../" -I"./../../../base" -Wall -DWIN32 -DNDEBUG -O2 -L"./../" -llxnet -lws2_32

linux-debug:
	g++ -o connect connect.cpp -I"./../" -I"./../../../base" -Wall -DDEBUG -g -L"./../" -llxnet -lpthread -lrt
	g++ -o listen listen.cpp -I"./../" -I"./../../../base" -Wall -DDEBUG -g -L"./../" -llxnet -lpthread -lrt


linux-release:
	g++ -o connect connect.cpp -I"./../" -I"./../../../base" -Wall -DNDEBUG -O2 -L"./../" -llxnet -lpthread -lrt
	g++ -o listen listen.cpp -I"./../" -I"./../../../base" -Wall -DNDEBUG -O2 -L"./../" -llxnet -lpthread -lrt
