#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

//#define GLM_FORCE_RADIANS
//#define GLM_FORCE_DEPTH_ZERO_TO_ONE

//#include <glm/vec4.hpp>
//#include <glm/mat4x4.hpp>
//#include <vulkan/vulkan.h>

#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <vector>
#include <string.h>
#include <cstring>
#include <optional>
#include <set>
#include <limits>
#include <algorithm>
#include <fstream>

#include "CobertEngine.hpp"

int main() {
 
    //VEngine app(900,500);

    try {
        CobertEngine::start();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        std::cout << "caught" << std::endl;
        int n;
        std::cin >> n;
        return EXIT_FAILURE;
    }
    std::cout << "ended" << std::endl;
   // int n;
    //std::cin >> n;
    
    return EXIT_SUCCESS;

}