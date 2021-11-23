#include <algorithm>
#include <array>
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
        carregarRecursos();
        executar();
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
        criarPoolDeComandos();
        criarPoolDeDescritores();
        alocarSetDeDescritores();
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
        familiaComputacao_ = buscarFamiliaDeFilas(dispositivoFisico_,
                                                  vk::QueueFlagBits::eCompute)
                                 .value();

        float prioridadeComputacao = 1.0f;
        vk::DeviceQueueCreateInfo infoComputacao;
        infoComputacao.queueFamilyIndex = familiaComputacao_;
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
        filaComputacao_ = dispositivo_.getQueue(familiaComputacao_, 0);
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

    void criarPoolDeComandos() {
        vk::CommandPoolCreateInfo info;
        info.flags = vk::CommandPoolCreateFlagBits::eTransient;
        info.queueFamilyIndex = familiaComputacao_;

        poolDeComandos_ = dispositivo_.createCommandPool(info);
    }

    void criarPoolDeDescritores() {
        const auto descritoresTotais = std::array{
            vk::DescriptorPoolSize{vk::DescriptorType::eStorageBuffer, 1}};

        vk::DescriptorPoolCreateInfo info;
        // info.flags = {}
        info.maxSets = 1;
        info.poolSizeCount = descritoresTotais.size();
        info.pPoolSizes = descritoresTotais.data();

        poolDeDescritores_ = dispositivo_.createDescriptorPool(info);
    }

    void alocarSetDeDescritores() {
        const auto layouts = std::array{layoutDoSetDeEntrada_};

        vk::DescriptorSetAllocateInfo info;
        info.descriptorPool = poolDeDescritores_;
        info.descriptorSetCount = layouts.size();
        info.pSetLayouts = layouts.data();

        setDeEntrada_ = dispositivo_.allocateDescriptorSets(info)[0];
    }

    void carregarRecursos() {
        criarBufferDeEntrada();
        atualizarSetDeDescritores();
    }

    void criarBufferDeEntrada() {
        criarBuffer(vk::BufferUsageFlagBits::eStorageBuffer, kTamanhoDoBuffer,
                    vk::MemoryPropertyFlagBits::eHostVisible |
                        vk::MemoryPropertyFlagBits::eHostCoherent,
                    buffer_, memoriaBuffer_);

        std::vector<int> dados(kNumDeItens, 4);
        void* localDaMemoria =
            dispositivo_.mapMemory(memoriaBuffer_, 0, kTamanhoDoBuffer);
        std::memcpy(localDaMemoria, dados.data(), kTamanhoDoBuffer);
        dispositivo_.unmapMemory(memoriaBuffer_);
    }

    void criarBuffer(vk::BufferUsageFlags usos,
                     size_t tamanho,
                     vk::MemoryPropertyFlags propriedades,
                     vk::Buffer& buffer,
                     vk::DeviceMemory& memoria) {
        vk::BufferCreateInfo infoBuffer;
        // infoBuffer.flags = {};
        infoBuffer.size = tamanho;
        infoBuffer.usage = usos;
        infoBuffer.sharingMode = vk::SharingMode::eExclusive;
        // infoBuffer.queueFamilyIndexCount = 0;
        // infoBuffer.pQueueFamilyIndices = nullptr;

        buffer = dispositivo_.createBuffer(infoBuffer);

        auto requisitosDeMemoria =
            dispositivo_.getBufferMemoryRequirements(buffer);
        auto tipoDeMemoria = buscarTipoDeMemoria(
            requisitosDeMemoria.memoryTypeBits, propriedades);

        memoria = alocarMemoria(requisitosDeMemoria.size, tipoDeMemoria);
        
        dispositivo_.bindBufferMemory(buffer, memoria, 0);
    }

    vk::DeviceMemory alocarMemoria(size_t tamanho, uint32_t tipoDeMemoria) {
        vk::MemoryAllocateInfo infoAlloc;
        infoAlloc.allocationSize = tamanho;
        infoAlloc.memoryTypeIndex = tipoDeMemoria;

        return dispositivo_.allocateMemory(infoAlloc);
    }

    uint32_t buscarTipoDeMemoria(uint32_t filtro,
                                 vk::MemoryPropertyFlags propriedades) {
        auto tiposDeMemorias = dispositivoFisico_.getMemoryProperties();
        for (uint32_t i = 0; i < tiposDeMemorias.memoryTypeCount; i++) {
            const auto& tipoDeMemoria = tiposDeMemorias.memoryTypes[i];

            bool passaPeloFiltro = (1u << i) & filtro;
            bool possuiAsPropriedades =
                (tipoDeMemoria.propertyFlags & propriedades) == propriedades;

            if (passaPeloFiltro && possuiAsPropriedades) {
                return i;
            }
        }

        throw std::runtime_error(
            "Não foi encontrada um tipo de memória adequado.");
    }

    void atualizarSetDeDescritores() {
        vk::DescriptorBufferInfo infoBuffer;
        infoBuffer.buffer = buffer_;
        infoBuffer.offset = 0;
        infoBuffer.range = kTamanhoDoBuffer;

        vk::WriteDescriptorSet writeBuffer;
        writeBuffer.dstSet = setDeEntrada_;
        writeBuffer.dstBinding = 0;
        // writeBuffer.dstArrayElement = 0;
        writeBuffer.descriptorCount = 1;
        writeBuffer.descriptorType = vk::DescriptorType::eStorageBuffer;
        // writeBuffer.pImageInfo = nullptr;
        writeBuffer.pBufferInfo = &infoBuffer;
        // writeBuffer.pTexelBufferView = nullptr;

        dispositivo_.updateDescriptorSets({writeBuffer}, {});
    }

    void executar() {
        alocarBufferDeComandos();
        gravarBufferDeComando();
        executarComandos();
        confirmarResultados();
    }

    void alocarBufferDeComandos() {
        vk::CommandBufferAllocateInfo info;
        info.commandPool = poolDeComandos_;
        // info.level = vk::CommandBufferLevel::ePrimary;
        info.commandBufferCount = 1;

        bufferDeComandos_ = dispositivo_.allocateCommandBuffers(info)[0];
    }

    void gravarBufferDeComando() {
        vk::CommandBufferBeginInfo info;
        info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
        info.pInheritanceInfo = nullptr;

        bufferDeComandos_.begin(info);
        bufferDeComandos_.bindPipeline(vk::PipelineBindPoint::eCompute,
                                       pipeline_);
        bufferDeComandos_.bindDescriptorSets(vk::PipelineBindPoint::eCompute,
                                             layoutDaPipeline_, 0,
                                             {setDeEntrada_}, {});
        bufferDeComandos_.dispatch(
            static_cast<uint32_t>(kNumDeGruposDeTrabalho), 1, 1);
        bufferDeComandos_.end();
    }

    void executarComandos() {
        vk::SubmitInfo info;
        // info.waitSemaphoreCount = 0;
        // info.pWaitSemaphores = nullptr;
        // info.pWaitDstStageMask = nullptr;
        info.commandBufferCount = 1;
        info.pCommandBuffers = &bufferDeComandos_;
        // info.signalSemaphoreCount = 0;
        // info.pSignalSemaphores = nullptr;

        filaComputacao_.submit({info}, nullptr);
        filaComputacao_.waitIdle();
    }

    void confirmarResultados() {
        void* localDaMemoria =
            dispositivo_.mapMemory(memoriaBuffer_, 0, kTamanhoDoBuffer);
        auto resultados = reinterpret_cast<const int*>(localDaMemoria);
        for (size_t i = 0; i < kNumDeItens; i++) {
            assert(resultados[i] == 8);
        }
        std::cout << std::endl;
        dispositivo_.unmapMemory(memoriaBuffer_);
    }

    void destruir() {
        dispositivo_.destroyDescriptorPool(poolDeDescritores_);
        dispositivo_.destroyBuffer(buffer_);
        dispositivo_.freeMemory(memoriaBuffer_);
        dispositivo_.destroyCommandPool(poolDeComandos_);
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

    uint32_t familiaComputacao_;
    vk::Queue filaComputacao_;
    vk::CommandPool poolDeComandos_;
    vk::CommandBuffer bufferDeComandos_;

    vk::DescriptorSetLayout layoutDoSetDeEntrada_;
    vk::PipelineLayout layoutDaPipeline_;

    const std::string kCamingoDoCodigoDoShader = "shaders/filtro.comp.spv";
    vk::ShaderModule moduloDoShader_;
    vk::Pipeline pipeline_;

    vk::DescriptorPool poolDeDescritores_;
    vk::DescriptorSet setDeEntrada_;

    const size_t kNumDeGruposDeTrabalho = 1024;
    const size_t kNumDeItens = 64 * kNumDeGruposDeTrabalho;
    const size_t kTamanhoDoBuffer = sizeof(int) * kNumDeItens;
    vk::Buffer buffer_;
    vk::DeviceMemory memoriaBuffer_;
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