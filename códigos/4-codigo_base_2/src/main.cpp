#include <algorithm>
#include <array>
#include <fstream>
#include <iostream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

#include <vulkan/vulkan.hpp>

namespace smv {
class App {
  public:
    void rodar() {
        iniciar();
        loopPrincipal();
        destruir();
    }

  private:
    void iniciar() {
        criarInstancia();
        escolherDispositivoFisico();
        // criarDispositivoLogicoEFilas();
    }

    void criarInstancia() {
        if (kAtivarCamadasDeValidacao &&
            !verificarDisponibilidadeDasCamadasDeValidacao()) {
            throw std::runtime_error(
                "Esta máquina não suporta as camadas de "
                "validação "
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
            info.enabledLayerCount = static_cast<uint32_t>(
                kCamadasDeValidacao.size());
            info.ppEnabledLayerNames =
                kCamadasDeValidacao.data();
        }

        instancia_ = vk::createInstance(info);
    }

    bool verificarDisponibilidadeDasCamadasDeValidacao() {
        auto propriedadesDasCamadas =
            vk::enumerateInstanceLayerProperties();

        for (auto&& camada : kCamadasDeValidacao) {
            bool camadaEncontrada = false;

            for (auto&& propriedades : propriedadesDasCamadas) {
                if (strcmp(camada, propriedades.layerName) ==
                    0) {
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

    void escolherDispositivoFisico() {
        auto dispositivosFisicos =
            instancia_.enumeratePhysicalDevices();

        auto resultado = std::find_if(
            dispositivosFisicos.begin(),
            dispositivosFisicos.end(), verificarDispositivo);

        if (resultado == dispositivosFisicos.end()) {
            throw std::runtime_error(
                "Não foi encontrado um dispositivo físico "
                "compartível.");
        }

        dispositivoFisico_ = *resultado;
    }

    static bool verificarDispositivo(
        const vk::PhysicalDevice& dispositivo) {
        // auto propriedades = dispositivo.getProperties();
        // auto capacidades = dispositivo.getFeatures();

        return verificarFilasDoDispositivo(dispositivo);
    }

    static bool verificarFilasDoDispositivo(
        const vk::PhysicalDevice&) {
        return true;
    }

    void criarDispositivoLogicoEFilas() {
        vk::PhysicalDeviceFeatures capacidades;

        vk::DeviceCreateInfo info;
        info.pEnabledFeatures = &capacidades;
        info.queueCreateInfoCount = 0;
        if (kAtivarCamadasDeValidacao) {
            info.enabledLayerCount = static_cast<uint32_t>(
                kCamadasDeValidacao.size());
            info.ppEnabledLayerNames =
                kCamadasDeValidacao.data();
        }

        dispositivo_ = dispositivoFisico_.createDevice(info);
    }

    static std::optional<uint32_t> buscarFamiliaDeFilas(
        const vk::PhysicalDevice& dispositivo,
        vk::QueueFlagBits tipo) {
        auto familias = dispositivo.getQueueFamilyProperties();

        auto familia =
            find_if(familias.begin(), familias.end(),
                    [tipo](auto familia) {
                        return familia.queueFlags & tipo;
                    });

        bool foiEncontrada = familia == familias.end();
        if (foiEncontrada) {
            return {};
        }

        return std::distance(familias.begin(), familia);
    }

    void loopPrincipal() {}

    void destruir() { instancia_.destroy(); }

#ifdef NDEBUG
    const bool kAtivarCamadasDeValidacao = false;
#else
    const bool kAtivarCamadasDeValidacao = true;
#endif

    const std::vector<const char*> kCamadasDeValidacao = {
        "VK_LAYER_KHRONOS_validation"};

    const int kLarguraDaJanela = 800;
    const int kAlturaDaJanela = 600;
    const char* kTituloDaJanela = "Simples Motor Vulkan";

    vk::Instance instancia_;
    vk::PhysicalDevice dispositivoFisico_;
    vk::Device dispositivo_;
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
