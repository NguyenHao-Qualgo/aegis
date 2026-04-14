#include "aegis/application.h"

#include "aegis/utils.h"

#include <exception>
#include <iostream>

int main(int argc, char* argv[]) {
    try {
        aegis::Application app;
        return app.run(argc, argv);
    } catch (const aegis::AegisError& e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Unexpected error: " << e.what() << "\n";
        return 1;
    }
}