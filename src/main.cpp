#include <exception>
#include <iostream>

#include "aegis/core/application.hpp"

int main(int argc, char** argv) {
    try {
        aegis::Application app;
        return app.run(argc, argv);
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << '\n';
        return 1;
    }
}
