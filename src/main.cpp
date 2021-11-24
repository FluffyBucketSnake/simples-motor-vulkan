#include <algorithm>
#include <array>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
#pragma GCC diagnostic pop

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
        vk::DescriptorSetLayoutBinding associacaoImagem;
        associacaoImagem.binding = 0;
        associacaoImagem.descriptorType = vk::DescriptorType::eStorageImage;
        associacaoImagem.descriptorCount = 1;
        associacaoImagem.stageFlags = vk::ShaderStageFlagBits::eCompute;
        // associacaoImagem.pImmutableSamplers = nullptr;

        vk::DescriptorSetLayoutCreateInfo info;
        // info.flags = {};
        info.bindingCount = 1;
        info.pBindings = &associacaoImagem;

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
            vk::DescriptorPoolSize{vk::DescriptorType::eStorageImage, 1}};

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
        carregarImagem(kCaminhoDaImagem, vk::ImageLayout::eGeneral,
                       vk::PipelineStageFlagBits::eComputeShader, imagem_,
                       memoriaImagem_, dimensoesImagem_);
        visaoImagem_ = criarVisaoDeImagem(imagem_);
        atualizarSetDeDescritores();
    }

    void carregarImagem(const std::string& caminho,
                        vk::ImageLayout layoutFinal,
                        vk::PipelineStageFlagBits estagioDestino,
                        vk::Image& imagem,
                        vk::DeviceMemory& memoria,
                        vk::Extent3D& dimensoes) {
        int largura, altura, _canais;
        stbi_uc* pixels = stbi_load(caminho.c_str(), &largura, &altura,
                                    &_canais, STBI_rgb_alpha);
        dimensoes = vk::Extent3D{static_cast<uint32_t>(largura),
                                 static_cast<uint32_t>(altura), 1u};
        size_t tamanho = dimensoes.width * dimensoes.height * 4u;

        criarImagem(vk::Format::eR8G8B8A8Unorm, dimensoes,
                    vk::ImageUsageFlagBits::eTransferDst |
                        vk::ImageUsageFlagBits::eTransferSrc |
                        vk::ImageUsageFlagBits::eStorage,
                    imagem, memoria);

        vk::Buffer bufferDePreparo;
        vk::DeviceMemory memoriaBufferDePreparo;
        criarBuffer(vk::BufferUsageFlagBits::eTransferSrc, tamanho,
                    vk::MemoryPropertyFlagBits::eHostVisible |
                        vk::MemoryPropertyFlagBits::eHostCoherent,
                    bufferDePreparo, memoriaBufferDePreparo);

        void* dados =
            dispositivo_.mapMemory(memoriaBufferDePreparo, 0, tamanho);
        std::memcpy(dados, pixels, tamanho);
        dispositivo_.unmapMemory(memoriaBufferDePreparo);
        stbi_image_free(pixels);

        alterarLayout(imagem, vk::PipelineStageFlagBits::eTopOfPipe,
                      vk::PipelineStageFlagBits::eTransfer,
                      vk::ImageLayout::eUndefined,
                      vk::ImageLayout::eTransferDstOptimal);
        copiarDeBufferParaImagem(bufferDePreparo, imagem, dimensoes.width,
                                 dimensoes.height);
        alterarLayout(imagem, vk::PipelineStageFlagBits::eTransfer,
                      estagioDestino, vk::ImageLayout::eTransferDstOptimal,
                      layoutFinal);

        dispositivo_.destroyBuffer(bufferDePreparo);
        dispositivo_.freeMemory(memoriaBufferDePreparo);
    }

    void alterarLayout(const vk::Image& imagem,
                       vk::PipelineStageFlags estagioFonte,
                       vk::PipelineStageFlags estagioDestino,
                       vk::ImageLayout layoutAntigo,
                       vk::ImageLayout layoutNovo) {
        vk::ImageMemoryBarrier barreira;
        // barreira.srcAccessMask = {}; TODO
        // barreira.dstAccessMask = {}; TODO
        barreira.oldLayout = layoutAntigo;
        barreira.newLayout = layoutNovo;
        // barreira.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        // barreira.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

        barreira.image = imagem;
        barreira.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        barreira.subresourceRange.baseMipLevel = 0;
        barreira.subresourceRange.levelCount = 1;
        barreira.subresourceRange.baseArrayLayer = 0;
        barreira.subresourceRange.layerCount = 1;

        vk::CommandBuffer comando = iniciarComandoDeUsoUnico();
        comando.pipelineBarrier(estagioFonte, estagioDestino, {}, nullptr,
                                nullptr, barreira);
        finalizarComandoDeUsoUnico(comando);
    }

    vk::CommandBuffer iniciarComandoDeUsoUnico() {
        vk::CommandBufferAllocateInfo infoAlloc;
        infoAlloc.commandPool = poolDeComandos_;
        infoAlloc.commandBufferCount = 1;

        vk::CommandBuffer comando =
            dispositivo_.allocateCommandBuffers(infoAlloc)[0];

        vk::CommandBufferBeginInfo infoBegin;
        infoBegin.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

        comando.begin(infoBegin);

        return comando;
    }

    void finalizarComandoDeUsoUnico(vk::CommandBuffer comando) {
        comando.end();

        vk::SubmitInfo infoSubmit;
        infoSubmit.commandBufferCount = 1;
        infoSubmit.pCommandBuffers = &comando;

        filaComputacao_.submit(infoSubmit, nullptr);
        filaComputacao_.waitIdle();

        dispositivo_.freeCommandBuffers(poolDeComandos_, {comando});
    }

    void copiarDeBufferParaImagem(vk::Buffer bufferFonte,
                                  vk::Image imagemDestino,
                                  uint32_t largura,
                                  uint32_t altura) {
        vk::BufferImageCopy regiao;

        // regiao.bufferOffset = 0;
        // regiao.bufferRowLength = 0;
        // regiao.bufferImageHeight = 0;

        regiao.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
        regiao.imageSubresource.mipLevel = 0;
        regiao.imageSubresource.baseArrayLayer = 0;
        regiao.imageSubresource.layerCount = 1;

        regiao.imageOffset = vk::Offset3D{0, 0, 0};
        regiao.imageExtent = vk::Extent3D{largura, altura, 1u};

        vk::CommandBuffer comando = iniciarComandoDeUsoUnico();
        comando.copyBufferToImage(bufferFonte, imagemDestino,
                                  vk::ImageLayout::eTransferDstOptimal,
                                  {regiao});
    }

    void criarImagem(vk::Format formato,
                     vk::Extent3D dimensoes,
                     vk::ImageUsageFlags usos,
                     vk::Image& imagem,
                     vk::DeviceMemory& memoria) {
        vk::ImageCreateInfo info;
        // info.flags = {};
        info.imageType = vk::ImageType::e2D;
        info.format = formato;
        info.extent = dimensoes;
        info.mipLevels = 1;
        info.arrayLayers = 1;
        info.samples = vk::SampleCountFlagBits::e1;
        info.tiling = vk::ImageTiling::eOptimal;
        info.usage = usos;
        info.sharingMode = vk::SharingMode::eExclusive;
        // info.queueFamilyIndexCount = 0;
        // info.pQueueFamilyIndices = nullptr;
        info.initialLayout = vk::ImageLayout::eUndefined;

        imagem = dispositivo_.createImage(info);

        auto requisitosDeMemoria =
            dispositivo_.getImageMemoryRequirements(imagem);
        auto tipoDeMemoria =
            buscarTipoDeMemoria(requisitosDeMemoria.memoryTypeBits,
                                vk::MemoryPropertyFlagBits::eDeviceLocal);
        memoria = alocarMemoria(requisitosDeMemoria.size, tipoDeMemoria);
        dispositivo_.bindImageMemory(imagem, memoria, 0);
    }

    vk::ImageView criarVisaoDeImagem(const vk::Image& imagem) {
        vk::ImageViewCreateInfo info;
        // info.flags = {};
        info.image = imagem;
        info.viewType = vk::ImageViewType::e2D;
        info.format = vk::Format::eR8G8B8A8Unorm;
        // info.components = {};
        info.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        info.subresourceRange.baseMipLevel = 0;
        info.subresourceRange.levelCount = 1;
        info.subresourceRange.baseArrayLayer = 0;
        info.subresourceRange.layerCount = 1;

        return dispositivo_.createImageView(info);
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
        vk::DescriptorImageInfo infoImagem;
        // infoImagem.sampler = nullptr;
        infoImagem.imageView = visaoImagem_;
        infoImagem.imageLayout = vk::ImageLayout::eGeneral;

        vk::WriteDescriptorSet writeImagem;
        writeImagem.dstSet = setDeEntrada_;
        writeImagem.dstBinding = 0;
        // writeImagem.dstArrayElement = 0;
        writeImagem.descriptorCount = 1;
        writeImagem.descriptorType = vk::DescriptorType::eStorageImage;
        writeImagem.pImageInfo = &infoImagem;
        // writeImagem.pBufferInfo = nullptr;
        // writeImagem.pTexelBufferView = nullptr;

        dispositivo_.updateDescriptorSets({writeImagem}, {});
    }

    void executar() {
        alocarBufferDeComandos();
        gravarBufferDeComandos();
        executarComandos();
        salvarImagem();
    }

    void alocarBufferDeComandos() {
        vk::CommandBufferAllocateInfo info;
        info.commandPool = poolDeComandos_;
        // info.level = vk::CommandBufferLevel::ePrimary;
        info.commandBufferCount = 1;

        bufferDeComandos_ = dispositivo_.allocateCommandBuffers(info)[0];
    }

    void gravarBufferDeComandos() {
        vk::CommandBufferBeginInfo info;
        info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
        info.pInheritanceInfo = nullptr;

        bufferDeComandos_.begin(info);
        bufferDeComandos_.bindPipeline(vk::PipelineBindPoint::eCompute,
                                       pipeline_);
        bufferDeComandos_.bindDescriptorSets(vk::PipelineBindPoint::eCompute,
                                             layoutDaPipeline_, 0,
                                             {setDeEntrada_}, {});
        bufferDeComandos_.dispatch(dimensoesImagem_.width / 32,
                                   dimensoesImagem_.height / 32, 1);
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

    void salvarImagem() {
        // vk::Buffer bufferDestino;
        // vk::DeviceMemory memoriaBufferDestino;
        // criarBuffer(vk::BufferUsageFlagBits::eTransferDst, )
    }

    void destruir() {
        dispositivo_.destroyDescriptorPool(poolDeDescritores_);
        dispositivo_.destroyImageView(visaoImagem_);
        dispositivo_.destroyImage(imagem_);
        dispositivo_.freeMemory(memoriaImagem_);
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

    const std::string kCaminhoDaImagem = "res/statue-g162b3a07b_640.jpg";
    vk::Extent3D dimensoesImagem_;
    vk::Image imagem_;
    vk::DeviceMemory memoriaImagem_;
    vk::ImageView visaoImagem_;
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