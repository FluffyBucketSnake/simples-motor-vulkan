#include <iostream>
#include <stdexcept>
#include <vector>

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
        if (kAtivarCamadasDeValidacao &&
            !verificarDisponibilidadeDasCamadasDeValidacao()) {
            throw std::runtime_error(
                "Esta máquina não suporta as camadas de validação "
                "necessárias.");
        }

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
        if (kAtivarCamadasDeValidacao) {
            info.enabledLayerCount =
                static_cast<uint32_t>(kCamadasDeValidacao.size());
            info.ppEnabledLayerNames = kCamadasDeValidacao.data();
        }

        instancia_ = vk::createInstance(info);
    }

    bool verificarDisponibilidadeDasCamadasDeValidacao() {
        auto propriedadesDasCamadas = vk::enumerateInstanceLayerProperties();

        for (auto&& camada : kCamadasDeValidacao) {
            bool camadaEncontrada = false;

            for (auto&& propriedades : propriedadesDasCamadas) {
                if (strcmp(camada, propriedades.layerName) == 0) {
                    camadaEncontrada = true;
                    break;
                }
            }

            if (!camadaEncontrada) {
                return false;
            }
        }

        return true;
    }

    void destruir() { instancia_.destroy(); }

#ifdef NDEBUG
    const bool kAtivarCamadasDeValidacao = false;
#else
    const bool kAtivarCamadasDeValidacao = true;
#endif

    const std::vector<const char*> kCamadasDeValidacao = {
        "VK_LAYER_KHRONOS_validation"};

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