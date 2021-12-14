#include <algorithm>
#include <array>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#pragma GCC diagnostic pop

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
        criarJanela();
        criarInstancia();
        criarSuperficie();
        escolherDispositivoFisico();
        // criarDispositivoLogicoEFilas();
    }

    void criarJanela() {
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        janela_ = glfwCreateWindow(kLarguraDaJanela, kAlturaDaJanela,
                                   kTituloDaJanela, nullptr, nullptr);
    }

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

        uint32_t numDeExtensoesGLFW;
        const char** extensoesGLFW =
            glfwGetRequiredInstanceExtensions(&numDeExtensoesGLFW);

        vk::InstanceCreateInfo info;
        info.pApplicationInfo = &infoApp;
        info.enabledExtensionCount = numDeExtensoesGLFW;
        info.ppEnabledExtensionNames = extensoesGLFW;
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

    void criarSuperficie() {
        VkSurfaceKHR superficiePura;
        if (glfwCreateWindowSurface(instancia_, janela_, nullptr,
                                    &superficiePura) != VK_SUCCESS) {
            throw std::runtime_error("Não foi possível criar a superfície.");
        }
        superficie_ = vk::SurfaceKHR(superficiePura);
    }

    void escolherDispositivoFisico() {
        auto dispositivosFisicos = instancia_.enumeratePhysicalDevices();
        auto resultado =
            std::find_if(dispositivosFisicos.begin(), dispositivosFisicos.end(),
                         [this](vk::PhysicalDevice d){ return verificarDispositivo(d); });

        if (resultado == dispositivosFisicos.end()) {
            throw std::runtime_error(
                "Não foi encontrado um dispositivo físico compartível.");
        }

        dispositivoFisico_ = *resultado;
    }

    bool verificarDispositivo(const vk::PhysicalDevice& dispositivo) {
        // auto propriedades = dispositivo.getProperties();
        // auto capacidades = dispositivo.getFeatures();

        return verificarFilasDoDispositivo(dispositivo);
    }

    bool verificarFilasDoDispositivo(const vk::PhysicalDevice& dispositivo) {
        bool possuiFilaGrafica =
            buscarFamiliaDeFilas(dispositivo, vk::QueueFlagBits::eGraphics)
                .has_value();

        bool possuiFilaDeApresentacao =
            buscarFamiliaDeFilasDePresentacao(dispositivo).has_value();

        return possuiFilaGrafica && possuiFilaDeApresentacao;
    }

    std::optional<uint32_t> buscarFamiliaDeFilasDePresentacao(
        vk::PhysicalDevice dispositivo) {
        auto familias = dispositivo.getQueueFamilyProperties();

        std::optional<uint32_t> valor = {};
        for (uint32_t i = 0; i < familias.size(); i++) {
            if (dispositivo.getSurfaceSupportKHR(i, superficie_)) {
                valor = i;
                break;
            }
        }

        return valor;
    }

    void criarDispositivoLogicoEFilas() {
        vk::PhysicalDeviceFeatures capacidades;

        auto familias = obterFamiliaDoDispositivo();

        std::vector<vk::DeviceQueueCreateInfo> infos;
        float prioridade = 1.0f;
        for (auto familia : familias) {
            infos.push_back({{}, familia, 1, &prioridade});
        }

        vk::DeviceCreateInfo info;
        info.pEnabledFeatures = &capacidades;
        info.queueCreateInfoCount = static_cast<uint32_t>(infos.size());
        info.pQueueCreateInfos = infos.data();
        if (kAtivarCamadasDeValidacao) {
            info.enabledLayerCount =
                static_cast<uint32_t>(kCamadasDeValidacao.size());
            info.ppEnabledLayerNames = kCamadasDeValidacao.data();
        }

        dispositivo_ = dispositivoFisico_.createDevice(info);
        filaDeApresentacao_ = dispositivo_.getQueue(familiaDeApresentacao_, 0);
        filaDeGraficos_ = dispositivo_.getQueue(familiaDeGraficos_, 0);
    }

    std::vector<uint32_t> obterFamiliaDoDispositivo() {
        familiaDeGraficos_ = buscarFamiliaDeFilas(dispositivoFisico_,
                                                  vk::QueueFlagBits::eGraphics)
                                 .value();

        familiaDeApresentacao_ =
            buscarFamiliaDeFilasDePresentacao(dispositivoFisico_).value();

        if (familiaDeApresentacao_ == familiaDeGraficos_) {
            return { familiaDeGraficos_ };
        } else {
            return { familiaDeGraficos_, familiaDeApresentacao_ };
        }
    }

    static std::optional<uint32_t> buscarFamiliaDeFilas(
        const vk::PhysicalDevice& dispositivo,
        vk::QueueFlagBits tipo) {
        auto familias = dispositivo.getQueueFamilyProperties();

        auto familia =
            find_if(familias.begin(), familias.end(),
                    [tipo](auto familia) { return familia.queueFlags & tipo; });

        bool foiEncontrada = familia == familias.end();
        if (foiEncontrada) {
            return {};
        }

        return std::distance(familias.begin(), familia);
    }

    void loopPrincipal() {
        while (!glfwWindowShouldClose(janela_)) {
            glfwPollEvents();
        }
    }

    void destruir() {
        dispositivo_.destroy();
        instancia_.destroySurfaceKHR(superficie_);
        instancia_.destroy();
        glfwDestroyWindow(janela_);
        glfwTerminate();
    }

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
    GLFWwindow* janela_;
    vk::SurfaceKHR superficie_;

    vk::Instance instancia_;
    vk::PhysicalDevice dispositivoFisico_;
    vk::Device dispositivo_;

    uint32_t familiaDeGraficos_;
    vk::Queue filaDeGraficos_;
    uint32_t familiaDeApresentacao_;
    vk::Queue filaDeApresentacao_;
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