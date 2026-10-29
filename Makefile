CXX      := g++
CXXFLAGS := -std=c++20 -O3 -Wall -Wextra -Wno-unused-parameter -fopenmp -pthread -Icpp_engine/include
LDFLAGS  := -fopenmp -pthread -lcurl -lpng -ljpeg -lz

BIN_DIR  := bin
PACKER   := $(BIN_DIR)/pack_web
EMBED    := cpp_engine/include/embedded_web.hpp
SERVER   := $(BIN_DIR)/falcon_server
CTL      := $(BIN_DIR)/falcon_ctl
CLI      := $(BIN_DIR)/falcon_models_cli
BRIDGE   := $(BIN_DIR)/falcon_dcc_bridge

.PHONY: all clean web win

all: $(SERVER) $(CTL) $(CLI) $(BRIDGE)

win: $(EMBED) cpp_engine/src/main.cpp cpp_engine/src/falcon_dcc_bridge.cpp cpp_engine/include/*.hpp | $(BIN_DIR)
	x86_64-w64-mingw32-g++-posix -std=c++20 -O3 -fopenmp -static -Icpp_engine/include cpp_engine/src/main.cpp -lz -lws2_32 -lwinhttp -lgdiplus -lshlwapi -lole32 -o $(BIN_DIR)/falcon_server.exe
	x86_64-w64-mingw32-strip -s $(BIN_DIR)/falcon_server.exe
	x86_64-w64-mingw32-g++-posix -std=c++20 -O3 -static cpp_engine/src/falcon_dcc_bridge.cpp -o $(BIN_DIR)/falcon_dcc_bridge.exe
	x86_64-w64-mingw32-strip -s $(BIN_DIR)/falcon_dcc_bridge.exe

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(PACKER): cpp_engine/src/pack_web.cpp | $(BIN_DIR)
	$(CXX) -std=c++20 -O2 cpp_engine/src/pack_web.cpp -o $(PACKER) -lz

$(EMBED): $(PACKER) frontend/web_bundle/index.html
	./$(PACKER) frontend/web_bundle $(EMBED)

$(SERVER): cpp_engine/src/main.cpp $(EMBED) cpp_engine/include/*.hpp | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) cpp_engine/src/main.cpp -o $(SERVER) $(LDFLAGS)

$(CTL): cpp_engine/src/openmesh_ctl.cpp cpp_engine/include/hardware.hpp cpp_engine/include/json.hpp | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) cpp_engine/src/openmesh_ctl.cpp -o $(CTL) -pthread -lcurl

$(CLI): cpp_engine/src/models_cli.cpp cpp_engine/include/hardware.hpp cpp_engine/include/json.hpp | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) cpp_engine/src/models_cli.cpp -o $(CLI) -pthread -lcurl

$(BRIDGE): cpp_engine/src/falcon_dcc_bridge.cpp | $(BIN_DIR)
	$(CXX) -std=c++20 -O3 -Wall -Wextra cpp_engine/src/falcon_dcc_bridge.cpp -o $(BRIDGE) -pthread

web:
	cd frontend && npm run build
	$(MAKE) $(EMBED) $(SERVER)

clean:
	rm -rf $(BIN_DIR)/* $(EMBED)
