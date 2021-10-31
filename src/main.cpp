#include <algorithm>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
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
    void iniciar() {
        criarInstancia();
        escolherDispositivoFisico();
        criarDispositivoLogicoEFilas();
        criarLayoutDosSetDeDescritores();
        criarLayoutDaPipeline();
        criarModuloDoShader();
        criarPipeline();
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

    void escolherDispositivoFisico() {
        auto dispositivosFisicos = instancia_.enumeratePhysicalDevices();

        auto resultado =
            std::find_if(dispositivosFisicos.begin(), dispositivosFisicos.end(),
                         verificarDispositivo);

        if (resultado == dispositivosFisicos.end()) {
            throw std::runtime_error(
                "Não foi encontrado um dispositivo físico compartível.");
        }

        dispositivoFisico_ = *resultado;
    }

    static bool verificarDispositivo(const vk::PhysicalDevice& dispositivo) {
        // auto propriedades = dispositivo.getProperties();
        // auto capacidades = dispositivo.getFeatures();

        return verificarFilasDoDispositivo(dispositivo);
    }

    static bool verificarFilasDoDispositivo(
        const vk::PhysicalDevice& dispositivo) {
        return buscarFamiliaDeFilas(dispositivo, vk::QueueFlagBits::eCompute)
            .has_value();
    }

    void criarDispositivoLogicoEFilas() {
        uint32_t familiaComputacao =
            buscarFamiliaDeFilas(dispositivoFisico_,
                                 vk::QueueFlagBits::eCompute)
                .value();

        float prioridadeComputacao = 1.0f;
        vk::DeviceQueueCreateInfo infoComputacao;
        infoComputacao.queueFamilyIndex = familiaComputacao;
        infoComputacao.queueCount = 1;
        infoComputacao.pQueuePriorities = &prioridadeComputacao;

        vk::PhysicalDeviceFeatures capacidades;

        vk::DeviceCreateInfo info;
        info.pEnabledFeatures = &capacidades;
        info.queueCreateInfoCount = 1;
        info.pQueueCreateInfos = &infoComputacao;
        if (kAtivarCamadasDeValidacao) {
            info.enabledLayerCount =
                static_cast<uint32_t>(kCamadasDeValidacao.size());
            info.ppEnabledLayerNames = kCamadasDeValidacao.data();
        }

        dispositivo_ = dispositivoFisico_.createDevice(info);
        filaComputacao_ = dispositivo_.getQueue(familiaComputacao, 0);
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

    void criarLayoutDosSetDeDescritores() {
        vk::DescriptorSetLayoutBinding associacaoBuffer;
        associacaoBuffer.binding = 0;
        associacaoBuffer.descriptorType = vk::DescriptorType::eStorageBuffer;
        associacaoBuffer.descriptorCount = 1;
        associacaoBuffer.stageFlags = vk::ShaderStageFlagBits::eCompute;
        // associacaoBuffer.pImmutableSamplers = nullptr;

        vk::DescriptorSetLayoutCreateInfo info;
        // info.flags = {};
        info.bindingCount = 1;
        info.pBindings = &associacaoBuffer;

        layoutDoSetDeEntrada_ = dispositivo_.createDescriptorSetLayout(info);
    }

    void criarLayoutDaPipeline() {
        vk::PipelineLayoutCreateInfo info;
        // info.flags = {}
        info.setLayoutCount = 1;
        info.pSetLayouts = &layoutDoSetDeEntrada_;
        // info.pushConstantRangeCount = 0;
        // info.pPushConstantRanges = nullptr;

        layoutDaPipeline_ = dispositivo_.createPipelineLayout(info);
    }

    void criarModuloDoShader() {
        std::ifstream arquivoDoShader(kCamingoDoCodigoDoShader,
                                      std::ios::binary);

        if (!arquivoDoShader.is_open()) {
            throw std::runtime_error("Não foi possível abrir o arquivo '" +
                                     kCamingoDoCodigoDoShader + "'!");
        }

        std::vector<char> codigoDoShader(
            (std::istreambuf_iterator<char>(arquivoDoShader)),
            (std::istreambuf_iterator<char>()));

        vk::ShaderModuleCreateInfo info;
        // info.flags = {}
        info.codeSize = codigoDoShader.size();
        info.pCode = reinterpret_cast<const uint32_t*>(codigoDoShader.data());

        moduloDoShader_ = dispositivo_.createShaderModule(info);
    }

    void criarPipeline() {
        vk::PipelineShaderStageCreateInfo infoShader;
        // infoShader.flags = {};
        infoShader.stage = vk::ShaderStageFlagBits::eCompute;
        infoShader.module = moduloDoShader_;
        infoShader.pName = "main";
        // infoShader.pSpecializationInfo = nullptr;

        vk::ComputePipelineCreateInfo info;
        // info.flags = {};
        info.stage = infoShader;
        info.layout = layoutDaPipeline_;
        // info.basePipelineHandle = nullptr;
        // info.basePipelineIndex = -1;

        pipeline_ = dispositivo_.createComputePipeline({}, info);
    }

    void destruir() {
        dispositivo_.destroyPipeline(pipeline_);
        dispositivo_.destroyShaderModule(moduloDoShader_);
        dispositivo_.destroyPipelineLayout(layoutDaPipeline_);
        dispositivo_.destroyDescriptorSetLayout(layoutDoSetDeEntrada_);
        dispositivo_.destroy();
        instancia_.destroy();
    }

#ifdef NDEBUG
    const bool kAtivarCamadasDeValidacao = false;
#else
    const bool kAtivarCamadasDeValidacao = true;
#endif

    const std::vector<const char*> kCamadasDeValidacao = {
        "VK_LAYER_KHRONOS_validation"};

    vk::Instance instancia_;
    vk::PhysicalDevice dispositivoFisico_;
    vk::Device dispositivo_;

    vk::Queue filaComputacao_;

    vk::DescriptorSetLayout layoutDoSetDeEntrada_;
    vk::PipelineLayout layoutDaPipeline_;

    const std::string kCamingoDoCodigoDoShader = "shaders/filtro.comp.spv";
    vk::ShaderModule moduloDoShader_;
    vk::Pipeline pipeline_;
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