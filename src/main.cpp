#include <iostream>

#include <vulkan/vulkan.hpp>

namespace smv {
class App {
  public:
    void rodar() {
        iniciar();
        destruir();
    }

  private:
    void iniciar() { criarInstancia(); }

    void criarInstancia() {
        vk::ApplicationInfo infoApp;
        infoApp.pApplicationName = "App";
        infoApp.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        infoApp.pEngineName = "Simples Motor Vulkan";
        infoApp.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        infoApp.apiVersion = VK_API_VERSION_1_0;

        vk::InstanceCreateInfo info;
        info.pApplicationInfo = &infoApp;
        // info.enabledExtensionCount = 0;
        // info.ppEnabledExtensionNames = nullptr;
        // info.enabledLayerCount = 0;
        // info.ppEnabledLayerNames = nullptr;

        instancia_ = vk::createInstance(info);
    }

    void destruir() { instancia_.destroy(); }

    vk::Instance instancia_;
};
}  // namespace smv

int main() {
    smv::App app;

    try {
        app.rodar();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}