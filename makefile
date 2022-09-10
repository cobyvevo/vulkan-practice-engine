CFLAGS = -std=c++20 -g -Wall -I include
LDFLAGS = -lglfw -lvulkan -ldl -lpthread -lX11 -lXxf86vm -lXrandr -lXi

VulkanTest: ./src/*.cpp
	g++ $(CFLAGS) -o VulkanTest ./src/*.cpp $(LDFLAGS)

.PHONY: test clean

test: VulkanTest
	glslc ./shaders/frag.frag -o ./shaders/frag.spv
	glslc ./shaders/vertex.vert -o ./shaders/vert.spv
	konsole -e ./VulkanTest
	#xterm ./VulkanTest

clean:
	rm -f VulkanTest
