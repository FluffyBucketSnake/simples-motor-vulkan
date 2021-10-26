#include <iostream>

namespace smv {
class App {
  public:
    void rodar() {}
  private:
};
}  // namespace smv

int main() {
    smv::App app;

    try {
        app.rodar();
    } catch(const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}