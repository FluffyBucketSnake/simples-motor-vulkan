#include <algorithm>
#include <array>
#include <fstream>
#include <iostream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vulkan/vulkan.hpp>

namespace smv {
struct Vertice {
    glm::vec3 posicao;
    glm::vec3 cor;

    static vk::VertexInputBindingDescription
    descricaoDeAssociacao() {
        vk::VertexInputBindingDescription descricaoDeAssociacao;
        descricaoDeAssociacao.binding = 0;
        descricaoDeAssociacao.stride = sizeof(Vertice);
        descricaoDeAssociacao.inputRate =
            vk::VertexInputRate::eVertex;

        return descricaoDeAssociacao;
    }

    static std::array<vk::VertexInputAttributeDescription, 2>
    descricaoDeAtributos() {
        return {vk::VertexInputAttributeDescription{
                    0, 0, vk::Format::eR32G32B32Sfloat,
                    offsetof(Vertice, posicao)},
                vk::VertexInputAttributeDescription{
                    1, 0, vk::Format::eR32G32B32Sfloat,
                    offsetof(Vertice, cor)}};
    }
};

struct OBU {
    alignas(16) glm::mat4 modelo;
    alignas(16) glm::mat4 visao;
    alignas(16) glm::mat4 projecao;
};

class App {
  public:
    void rodar() {
        iniciar();
        carregarRecursos();
        loopPrincipal();
        destruir();
    }

  private:
    void iniciar() {
        criarJanela();
        criarInstancia();
        criarSuperficie();
        escolherDispositivoFisico();
        criarDispositivoLogicoEFilas();
        criarPoolDeComandos();
        criarSwapchain();
        criarPasseDeRenderizacao();
        criarFramebuffers();
        criarLayoutsDosSetsDeDescritores();
        criarLayoutDaPipeline();
        carregarShaders();
        criarPipeline();
        criarBuffersDeComandos();
        criarPrimitivosDeSincronizacao();
        criarPoolDeDescritores();
    }

    void criarJanela() {
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        janela_ =
            glfwCreateWindow(kLarguraDaJanela, kAlturaDaJanela,
                             kTituloDaJanela, nullptr, nullptr);
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

        uint32_t numDeExtensoesGLFW;
        const char** extensoesGLFW =
            glfwGetRequiredInstanceExtensions(
                &numDeExtensoesGLFW);

        vk::InstanceCreateInfo info;
        info.pApplicationInfo = &infoApp;
        info.enabledExtensionCount = numDeExtensoesGLFW;
        info.ppEnabledExtensionNames = extensoesGLFW;
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

    void criarSuperficie() {
        VkSurfaceKHR superficiePura;
        if (glfwCreateWindowSurface(instancia_, janela_,
                                    nullptr, &superficiePura) !=
            VK_SUCCESS) {
            throw std::runtime_error(
                "Não foi possível criar a superfície.");
        }
        superficie_ = vk::SurfaceKHR(superficiePura);
    }

    void escolherDispositivoFisico() {
        auto dispositivosFisicos =
            instancia_.enumeratePhysicalDevices();
        auto resultado =
            std::find_if(dispositivosFisicos.begin(),
                         dispositivosFisicos.end(),
                         [this](vk::PhysicalDevice d) {
                             return verificarDispositivo(d);
                         });

        if (resultado == dispositivosFisicos.end()) {
            throw std::runtime_error(
                "Não foi encontrado um dispositivo físico "
                "compartível.");
        }

        dispositivoFisico_ = *resultado;
    }

    bool verificarDispositivo(
        const vk::PhysicalDevice& dispositivo) {
        bool possuiFilas =
            verificarFilasDoDispositivo(dispositivo);
        bool suportaExtensoes =
            verificarSuporteDeExtensoes(dispositivo);
        bool adequadoParaSwapchain =
            suportaExtensoes &&
            verificarSuporteDeSwapchain(dispositivo);

        return possuiFilas && adequadoParaSwapchain;
    }

    bool verificarFilasDoDispositivo(
        const vk::PhysicalDevice& dispositivo) {
        bool possuiFilaGrafica =
            buscarFamiliaDeFilas(dispositivo,
                                 vk::QueueFlagBits::eGraphics)
                .has_value();

        bool possuiFilaDeApresentacao =
            buscarFamiliaDeFilasDePresentacao(dispositivo)
                .has_value();

        return possuiFilaGrafica && possuiFilaDeApresentacao;
    }

    std::optional<uint32_t> buscarFamiliaDeFilasDePresentacao(
        vk::PhysicalDevice dispositivo) {
        auto familias = dispositivo.getQueueFamilyProperties();

        std::optional<uint32_t> valor = {};
        for (uint32_t i = 0; i < familias.size(); i++) {
            if (dispositivo.getSurfaceSupportKHR(i,
                                                 superficie_)) {
                valor = i;
                break;
            }
        }

        return valor;
    }

    bool verificarSuporteDeExtensoes(
        const vk::PhysicalDevice& dispositivo) {
        auto extensoesDisponiveis =
            dispositivo.enumerateDeviceExtensionProperties();
        std::unordered_set<std::string> extensoesRestantes(
            kExtensoesDeDispositivo.begin(),
            kExtensoesDeDispositivo.end());

        for (const auto& extensao : extensoesDisponiveis) {
            extensoesRestantes.erase(extensao.extensionName);
        }

        return extensoesRestantes.empty();
    }

    bool verificarSuporteDeSwapchain(
        const vk::PhysicalDevice& dispositivo) {
        bool possuiFormatos =
            !dispositivo.getSurfaceFormatsKHR(superficie_)
                 .empty();
        bool possuiModosDeApresentacao =
            !dispositivo.getSurfacePresentModesKHR(superficie_)
                 .empty();

        return possuiFormatos && possuiModosDeApresentacao;
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
        info.queueCreateInfoCount =
            static_cast<uint32_t>(infos.size());
        info.pQueueCreateInfos = infos.data();
        info.enabledExtensionCount = static_cast<uint32_t>(
            kExtensoesDeDispositivo.size());
        info.ppEnabledExtensionNames =
            kExtensoesDeDispositivo.data();
        if (kAtivarCamadasDeValidacao) {
            info.enabledLayerCount = static_cast<uint32_t>(
                kCamadasDeValidacao.size());
            info.ppEnabledLayerNames =
                kCamadasDeValidacao.data();
        }

        dispositivo_ = dispositivoFisico_.createDevice(info);
        filaDeApresentacao_ =
            dispositivo_.getQueue(familiaDeApresentacao_, 0);
        filaDeGraficos_ =
            dispositivo_.getQueue(familiaDeGraficos_, 0);
    }

    std::vector<uint32_t> obterFamiliaDoDispositivo() {
        familiaDeGraficos_ =
            buscarFamiliaDeFilas(dispositivoFisico_,
                                 vk::QueueFlagBits::eGraphics)
                .value();

        familiaDeApresentacao_ =
            buscarFamiliaDeFilasDePresentacao(
                dispositivoFisico_)
                .value();

        if (familiaDeApresentacao_ == familiaDeGraficos_) {
            return {familiaDeGraficos_};
        } else {
            return {familiaDeGraficos_, familiaDeApresentacao_};
        }
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

    void criarPoolDeComandos() {
        vk::CommandPoolCreateInfo info;
        info.queueFamilyIndex = familiaDeGraficos_;

        poolDeComandos_ = dispositivo_.createCommandPool(info);
    }

    void criarSwapchain() {
        auto capacidades =
            dispositivoFisico_.getSurfaceCapabilitiesKHR(
                superficie_);
        auto formatosDisponiveis =
            dispositivoFisico_.getSurfaceFormatsKHR(
                superficie_);
        auto modosDeApresentacaoDisponiveis =
            dispositivoFisico_.getSurfacePresentModesKHR(
                superficie_);

        auto formato =
            escolherFormatoDaSwapchain(formatosDisponiveis);
        auto modoDeApresentacao = escolherModoDeApresentacao(
            modosDeApresentacaoDisponiveis);
        dimensoesDaSwapchain_ =
            escolherDimensoesDaSwapchain(capacidades);

        uint32_t numeroDeImagens =
            std::max(capacidades.minImageCount + 1,
                     capacidades.maxImageCount);

        vk::SwapchainCreateInfoKHR info;

        info.surface = superficie_;
        info.minImageCount = numeroDeImagens;
        info.imageFormat = formatoDaSwapchain_ = formato.format;
        info.imageColorSpace = formato.colorSpace;
        info.imageExtent = dimensoesDaSwapchain_;
        info.imageArrayLayers = 1;
        info.imageUsage =
            vk::ImageUsageFlagBits::eColorAttachment;

        info.preTransform = capacidades.currentTransform;
        info.compositeAlpha =
            vk::CompositeAlphaFlagBitsKHR::eOpaque;
        info.presentMode = modoDeApresentacao;
        info.clipped = true;
        info.oldSwapchain = nullptr;

        if (familiaDeApresentacao_ != familiaDeGraficos_) {
            std::array<uint32_t, 2> familias{
                familiaDeApresentacao_, familiaDeGraficos_};
            info.imageSharingMode =
                vk::SharingMode::eConcurrent;
            info.queueFamilyIndexCount =
                static_cast<uint32_t>(familias.size());
            info.pQueueFamilyIndices = familias.data();

            swapChain_ = dispositivo_.createSwapchainKHR(info);
        } else {
            info.imageSharingMode = vk::SharingMode::eExclusive;
            // info.queueFamilyIndexCount = 0;
            // info.pQueueFamilyIndices = nullptr;

            swapChain_ = dispositivo_.createSwapchainKHR(info);
        }

        imagensDaSwapchain_ =
            dispositivo_.getSwapchainImagesKHR(swapChain_);
        visoesDasImagensDaSwapchain_.reserve(
            imagensDaSwapchain_.size());
        for (const auto& imagem : imagensDaSwapchain_) {
            visoesDasImagensDaSwapchain_.push_back(
                criarVisaoDeImagem(imagem,
                                   formatoDaSwapchain_));
        }
        imagensEmExecucao_.resize(imagensDaSwapchain_.size());
    }

    vk::SurfaceFormatKHR escolherFormatoDaSwapchain(
        const std::vector<vk::SurfaceFormatKHR>&
            formatosDisponiveis) {
        for (const auto& formato : formatosDisponiveis) {
            if (formato.format == vk::Format::eB8G8R8A8Srgb &&
                formato.colorSpace ==
                    vk::ColorSpaceKHR::eSrgbNonlinear) {
                return formato;
            }
        }
        return formatosDisponiveis[0];
    }

    vk::PresentModeKHR escolherModoDeApresentacao(
        const std::vector<vk::PresentModeKHR>&
            modosDeApresentacaoDisponiveis) {
        auto comeco = modosDeApresentacaoDisponiveis.begin();
        auto fim = modosDeApresentacaoDisponiveis.end();

        if (std::find(comeco, fim,
                      vk::PresentModeKHR::eMailbox) != fim) {
            return vk::PresentModeKHR::eMailbox;
        }

        return vk::PresentModeKHR::eFifo;
    }

    vk::Extent2D escolherDimensoesDaSwapchain(
        const vk::SurfaceCapabilitiesKHR& capacidades) {
        if (capacidades.currentExtent.width !=
            std::numeric_limits<uint32_t>::max()) {
            return capacidades.currentExtent;
        } else {
            int largura, altura;
            glfwGetFramebufferSize(janela_, &largura, &altura);

            vk::Extent2D dimensoes = {
                static_cast<uint32_t>(largura),
                static_cast<uint32_t>(altura)};

            vk::Extent2D minimo = capacidades.minImageExtent;
            vk::Extent2D maximo = capacidades.maxImageExtent;

            dimensoes.width = std::clamp(
                dimensoes.width, minimo.width, maximo.width);
            dimensoes.height = std::clamp(
                dimensoes.height, minimo.height, maximo.height);

            return dimensoes;
        }
    }

    vk::ImageView criarVisaoDeImagem(const vk::Image& imagem,
                                     vk::Format formato) {
        vk::ImageViewCreateInfo info;
        // info.flags = {};
        info.image = imagem;
        info.viewType = vk::ImageViewType::e2D;
        info.format = formato;
        // info.components = {};
        info.subresourceRange.aspectMask =
            vk::ImageAspectFlagBits::eColor;
        info.subresourceRange.baseMipLevel = 0;
        info.subresourceRange.levelCount = 1;
        info.subresourceRange.baseArrayLayer = 0;
        info.subresourceRange.layerCount = 1;

        return dispositivo_.createImageView(info);
    }

    void criarPasseDeRenderizacao() {
        vk::AttachmentDescription anexoCor;
        anexoCor.format = formatoDaSwapchain_;
        // anexoCor.samples = vk::SampleCountFlagBits::e1;
        anexoCor.loadOp = vk::AttachmentLoadOp::eClear;
        anexoCor.storeOp = vk::AttachmentStoreOp::eStore;
        anexoCor.stencilLoadOp =
            vk::AttachmentLoadOp::eDontCare;
        anexoCor.stencilStoreOp =
            vk::AttachmentStoreOp::eDontCare;
        anexoCor.initialLayout = vk::ImageLayout::eUndefined;
        anexoCor.finalLayout = vk::ImageLayout::ePresentSrcKHR;

        vk::AttachmentReference refAnexoCor;
        refAnexoCor.attachment = 0;
        refAnexoCor.layout =
            vk::ImageLayout::eColorAttachmentOptimal;

        vk::SubpassDescription subpasse;
        // subpasse.flags = {};
        // subpasse.pipelineBindPoint =
        // vk::PipelineBindPoint::eGraphics;
        // subpasse.inputAttachmentCount = 0;
        // subpasse.pInputAttachments = nullptr;
        subpasse.colorAttachmentCount = 1;
        subpasse.pColorAttachments = &refAnexoCor;
        // subpasse.pResolveAttachments = nullptr;
        // subpasse.pDepthStencilAttachment = nullptr;
        // subpasse.preserveAttachmentCount = 0;
        // subpasse.pPreserveAttachments = nullptr;

        vk::SubpassDependency dependenciaAnexoDeCor;
        dependenciaAnexoDeCor.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependenciaAnexoDeCor.dstSubpass = 0;
        dependenciaAnexoDeCor.srcStageMask =
            vk::PipelineStageFlagBits::eColorAttachmentOutput;
        dependenciaAnexoDeCor.dstStageMask =
            vk::PipelineStageFlagBits::eColorAttachmentOutput;
        dependenciaAnexoDeCor.srcAccessMask = {};
        dependenciaAnexoDeCor.dstAccessMask =
            vk::AccessFlagBits::eColorAttachmentWrite;

        vk::RenderPassCreateInfo info;
        info.attachmentCount = 1;
        info.pAttachments = &anexoCor;
        info.subpassCount = 1;
        info.pSubpasses = &subpasse;
        info.dependencyCount = 1;
        info.pDependencies = &dependenciaAnexoDeCor;

        passeDeRenderizacao_ =
            dispositivo_.createRenderPass(info);
    }

    void criarFramebuffers() {
        std::transform(visoesDasImagensDaSwapchain_.begin(),
                       visoesDasImagensDaSwapchain_.end(),
                       std::back_inserter(framebuffers_),
                       [this](const vk::ImageView& i) {
                           return criarFramebuffer(i);
                       });
    }

    vk::Framebuffer criarFramebuffer(
        const vk::ImageView& imagemDaSwapchain) {
        vk::FramebufferCreateInfo info;
        info.renderPass = passeDeRenderizacao_;
        info.attachmentCount = 1;
        info.pAttachments = &imagemDaSwapchain;
        info.width = dimensoesDaSwapchain_.width;
        info.height = dimensoesDaSwapchain_.height;
        info.layers = 1;

        return dispositivo_.createFramebuffer(info);
    }

    void criarLayoutsDosSetsDeDescritores() {
        std::array<vk::DescriptorSetLayoutBinding, 1>
            associacoes = {vk::DescriptorSetLayoutBinding{
                0, vk::DescriptorType::eUniformBuffer, 1,
                vk::ShaderStageFlagBits::eVertex}};

        vk::DescriptorSetLayoutCreateInfo info;
        info.bindingCount =
            static_cast<uint32_t>(associacoes.size());
        info.pBindings = associacoes.data();

        layoutDoSetDeDescritores_ =
            dispositivo_.createDescriptorSetLayout(info);
    }

    void criarLayoutDaPipeline() {
        vk::PipelineLayoutCreateInfo info;
        info.setLayoutCount = 1;
        info.pSetLayouts = &layoutDoSetDeDescritores_;
        info.pushConstantRangeCount = 0;

        layoutDaPipeline_ =
            dispositivo_.createPipelineLayout(info);
    }

    void carregarShaders() {
        shaderDeVertices =
            carregarShader(kCaminhoShaderDeVertices);
        shaderDeFragmentos =
            carregarShader(kCaminhoShaderDeFragmento);
    }

    vk::ShaderModule carregarShader(
        const std::string& caminho) {
        std::ifstream arquivoDoShader(caminho,
                                      std::ios::binary);

        if (!arquivoDoShader.is_open()) {
            throw std::runtime_error(
                "Não foi possível abrir o arquivo '" + caminho +
                "'!");
        }

        std::vector<char> codigoDoShader(
            (std::istreambuf_iterator<char>(arquivoDoShader)),
            (std::istreambuf_iterator<char>()));

        vk::ShaderModuleCreateInfo info;
        info.codeSize = codigoDoShader.size();
        info.pCode = reinterpret_cast<const uint32_t*>(
            codigoDoShader.data());

        return dispositivo_.createShaderModule(info);
    }

    void criarPipeline() {
        std::array<vk::PipelineShaderStageCreateInfo, 2>
            estagios{vk::PipelineShaderStageCreateInfo{
                         {},
                         vk::ShaderStageFlagBits::eVertex,
                         shaderDeVertices,
                         "main"},
                     vk::PipelineShaderStageCreateInfo{
                         {},
                         vk::ShaderStageFlagBits::eFragment,
                         shaderDeFragmentos,
                         "main"}};

        auto descricaoDeAssociacao =
            Vertice::descricaoDeAssociacao();

        auto atributosDosVertices =
            Vertice::descricaoDeAtributos();

        vk::PipelineVertexInputStateCreateInfo infoVertices;
        infoVertices.vertexBindingDescriptionCount = 1;
        infoVertices.pVertexBindingDescriptions =
            &descricaoDeAssociacao;
        infoVertices.vertexAttributeDescriptionCount =
            static_cast<uint32_t>(atributosDosVertices.size());
        infoVertices.pVertexAttributeDescriptions =
            atributosDosVertices.data();

        vk::PipelineInputAssemblyStateCreateInfo infoEntrada;
        infoEntrada.topology =
            vk::PrimitiveTopology::eTriangleList;

        vk::Viewport viewport = {
            0.0f,
            0.0f,
            static_cast<float>(dimensoesDaSwapchain_.width),
            static_cast<float>(dimensoesDaSwapchain_.height),
            0.0f,
            1.0f};
        vk::Rect2D recorte = {{0, 0}, dimensoesDaSwapchain_};
        vk::PipelineViewportStateCreateInfo infoViewport;
        infoViewport.viewportCount = 1;
        infoViewport.pViewports = &viewport;
        infoViewport.scissorCount = 1;
        infoViewport.pScissors = &recorte;

        vk::PipelineRasterizationStateCreateInfo
            infoRasterizador;
        infoRasterizador.polygonMode = vk::PolygonMode::eFill;
        infoRasterizador.cullMode = vk::CullModeFlagBits::eBack;
        infoRasterizador.frontFace =
            vk::FrontFace::eCounterClockwise;
        infoRasterizador.lineWidth = 1.0f;

        vk::PipelineMultisampleStateCreateInfo infoAmostragem;

        vk::PipelineColorBlendAttachmentState
            misturaDoAnexoDeCor;
        misturaDoAnexoDeCor.colorWriteMask =
            vk::ColorComponentFlagBits::eR |
            vk::ColorComponentFlagBits::eG |
            vk::ColorComponentFlagBits::eB |
            vk::ColorComponentFlagBits::eA;
        misturaDoAnexoDeCor.blendEnable = false;

        // Sobrescrita
        // misturaDoAnexoDeCor.srcColorBlendFactor =
        // vk::BlendFactor::eOne;
        // misturaDoAnexoDeCor.dstColorBlendFactor =
        // vk::BlendFactor::eZero;
        // misturaDoAnexoDeCor.colorBlendOp = vk::BlendOp::eAdd;
        // misturaDoAnexoDeCor.srcAlphaBlendFactor =
        // vk::BlendFactor::eOne;
        // misturaDoAnexoDeCor.dstAlphaBlendFactor =
        // vk::BlendFactor::eZero;
        // misturaDoAnexoDeCor.alphaBlendOp = vk::BlendOp::eAdd;

        // Alpha blending
        // misturaDoAnexoDeCor.srcColorBlendFactor =
        // vk::BlendFactor::eSrcAlpha;
        // misturaDoAnexoDeCor.dstColorBlendFactor =
        //     vk::BlendFactor::eOneMinusSrcAlpha;
        // misturaDoAnexoDeCor.colorBlendOp = vk::BlendOp::eAdd;
        // misturaDoAnexoDeCor.srcAlphaBlendFactor =
        // vk::BlendFactor::eOne;
        // misturaDoAnexoDeCor.dstAlphaBlendFactor =
        // vk::BlendFactor::eZero;
        // misturaDoAnexoDeCor.alphaBlendOp = vk::BlendOp::eAdd;

        vk::PipelineColorBlendStateCreateInfo infoMistura;
        infoMistura.attachmentCount = 1;
        infoMistura.pAttachments = &misturaDoAnexoDeCor;

        vk::GraphicsPipelineCreateInfo info;
        // info.flags = {}
        info.stageCount =
            static_cast<uint32_t>(estagios.size());
        info.pStages = estagios.data();
        info.pVertexInputState = &infoVertices;
        info.pInputAssemblyState = &infoEntrada;
        info.pViewportState = &infoViewport;
        info.pRasterizationState = &infoRasterizador;
        info.pMultisampleState = &infoAmostragem;
        info.pColorBlendState = &infoMistura;
        info.layout = layoutDaPipeline_;
        info.renderPass = passeDeRenderizacao_;
        info.subpass = 0;

        pipeline_ =
            dispositivo_.createGraphicsPipeline({}, info).value;
    }

    void criarBuffersDeComandos() {
        uint32_t numDeFramebuffers =
            static_cast<uint32_t>(framebuffers_.size());

        vk::CommandBufferAllocateInfo info;
        info.commandPool = poolDeComandos_;
        info.commandBufferCount = numDeFramebuffers;

        buffersDeComandos_ =
            dispositivo_.allocateCommandBuffers(info);
    }

    void gravarBufferDeComandos(
        vk::CommandBuffer bufferDeComandos,
        vk::Framebuffer framebuffer) {
        vk::CommandBufferBeginInfo info;
        bufferDeComandos.begin(info);

        vk::ClearValue corDeLimpeza = {vk::ClearColorValue{
            std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f}}};

        vk::RenderPassBeginInfo infoPasse;
        infoPasse.renderPass = passeDeRenderizacao_;
        infoPasse.framebuffer = framebuffer;
        infoPasse.renderArea =
            vk::Rect2D{{0, 0}, dimensoesDaSwapchain_};
        infoPasse.clearValueCount = 1;
        infoPasse.pClearValues = &corDeLimpeza;
        bufferDeComandos.beginRenderPass(
            infoPasse, vk::SubpassContents::eInline);

        bufferDeComandos.bindPipeline(
            vk::PipelineBindPoint::eGraphics, pipeline_);
        bufferDeComandos.bindDescriptorSets(
            vk::PipelineBindPoint::eGraphics, layoutDaPipeline_,
            0, setDeDescritores_, {});
        bufferDeComandos.bindVertexBuffers(0, bufferDeVertices_,
                                           {0});
        bufferDeComandos.bindIndexBuffer(
            bufferDeIndices_, 0, vk::IndexType::eUint16);

        uint32_t numIndices =
            static_cast<uint32_t>(kIndices.size());
        bufferDeComandos.drawIndexed(numIndices, 1, 0, 0, 0);

        bufferDeComandos.endRenderPass();

        bufferDeComandos.end();
    }

    void criarPrimitivosDeSincronizacao() {
        std::generate(semaforosDeImagemDisponivel_.begin(),
                      semaforosDeImagemDisponivel_.end(),
                      [this]() { return criarSemaforo(); });
        std::generate(semaforosDeRenderizacaoCompleta_.begin(),
                      semaforosDeRenderizacaoCompleta_.end(),
                      [this]() { return criarSemaforo(); });
        std::generate(
            cercasDeQuadros_.begin(), cercasDeQuadros_.end(),
            [this]() {
                return criarCerca(
                    vk::FenceCreateFlagBits::eSignaled);
            });
    }

    vk::Semaphore criarSemaforo() {
        vk::SemaphoreCreateInfo infoSemaforo;
        return dispositivo_.createSemaphore(infoSemaforo);
    }

    vk::Fence criarCerca(vk::FenceCreateFlags flags) {
        vk::FenceCreateInfo info;
        info.flags = flags;
        return dispositivo_.createFence(info);
    }

    void criarPoolDeDescritores() {
        std::array<vk::DescriptorPoolSize, 1> tamanhos = {
            vk::DescriptorPoolSize{
                vk::DescriptorType::eUniformBuffer, 1}};

        vk::DescriptorPoolCreateInfo info;
        info.maxSets = 1;
        info.poolSizeCount =
            static_cast<uint32_t>(tamanhos.size());
        info.pPoolSizes = tamanhos.data();

        poolDeDescritores_ =
            dispositivo_.createDescriptorPool(info);
    }

    void carregarRecursos() {
        criarBufferImutavel(
            vk::BufferUsageFlagBits::eVertexBuffer, kVertices,
            bufferDeVertices_, memoriaBufferDeVertices_);
        criarBufferImutavel(
            vk::BufferUsageFlagBits::eIndexBuffer, kIndices,
            bufferDeIndices_, memoriaBufferDeIndices_);

        criarBuffer(vk::BufferUsageFlagBits::eUniformBuffer |
                        vk::BufferUsageFlagBits::eTransferDst,
                    sizeof(OBU),
                    vk::MemoryPropertyFlagBits::eDeviceLocal,
                    bufferDoOBU_, memoriaBufferDoOBU_);

        obu_.modelo = glm::identity<glm::mat4>();

        glm::vec3 posicaoDaCamera = {0.0f, -2.0f, 0.0f};
        glm::vec3 alvoDaCamera = glm::zero<glm::vec3>();
        glm::vec3 cimaDaCamera = {0.0f, 0.0f, -1.0f};
        obu_.visao = glm::scale(glm::identity<glm::mat4>(),
                                {1, -1, -1}) *
                     glm::lookAt(posicaoDaCamera, alvoDaCamera,
                                 cimaDaCamera);

        obu_.projecao =
            glm::scale(glm::identity<glm::mat4>(), {1, -1, 1}) *
            glm::frustum(-4.0f, 4.0f, 4.0f, -4.0f, 2.0f,
                         100.0f) *
            glm::scale(glm::identity<glm::mat4>(), {1, 1, -1});

        atualizarBuffer(bufferDoOBU_, sizeof(OBU), &obu_);

        criarSetsDeDescritores();

        for (size_t i = 0; i < framebuffers_.size(); i++) {
            gravarBufferDeComandos(buffersDeComandos_[i],
                                   framebuffers_[i]);
        }
    }

    template <typename T>
    void criarBufferImutavel(vk::BufferUsageFlags usos,
                             std::vector<T> dados,
                             vk::Buffer& buffer,
                             vk::DeviceMemory& memoria) {
        size_t tamanho = dados.size() * sizeof(T);

        criarBuffer(
            usos | vk::BufferUsageFlagBits::eTransferDst,
            tamanho, vk::MemoryPropertyFlagBits::eDeviceLocal,
            buffer, memoria);

        atualizarBuffer(buffer, tamanho, dados.data());
    }

    void atualizarBuffer(vk::Buffer buffer,
                         size_t tamanho,
                         void* dados) {
        vk::Buffer bufferDePreparo;
        vk::DeviceMemory memoriaBufferDePreparo;

        criarBuffer(
            vk::BufferUsageFlagBits::eTransferSrc, tamanho,
            vk::MemoryPropertyFlagBits::eHostCoherent |
                vk::MemoryPropertyFlagBits::eHostVisible,
            bufferDePreparo, memoriaBufferDePreparo);

        void* dstDados = dispositivo_.mapMemory(
            memoriaBufferDePreparo, 0, tamanho);
        std::memcpy(dstDados, dados, tamanho);
        dispositivo_.unmapMemory(memoriaBufferDePreparo);

        auto comando = iniciarComandoDeUsoUnico();

        vk::BufferCopy infoCopia;
        infoCopia.srcOffset = 0;
        infoCopia.dstOffset = 0;
        infoCopia.size = tamanho;
        comando.copyBuffer(bufferDePreparo, buffer, infoCopia);

        finalizarComandoDeUsoUnico(comando);

        dispositivo_.destroyBuffer(bufferDePreparo);
        dispositivo_.freeMemory(memoriaBufferDePreparo);
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

        memoria = alocarMemoria(requisitosDeMemoria.size,
                                tipoDeMemoria);

        dispositivo_.bindBufferMemory(buffer, memoria, 0);
    }

    vk::DeviceMemory alocarMemoria(size_t tamanho,
                                   uint32_t tipoDeMemoria) {
        vk::MemoryAllocateInfo infoAlloc;
        infoAlloc.allocationSize = tamanho;
        infoAlloc.memoryTypeIndex = tipoDeMemoria;

        return dispositivo_.allocateMemory(infoAlloc);
    }

    uint32_t buscarTipoDeMemoria(
        uint32_t filtro,
        vk::MemoryPropertyFlags propriedades) {
        auto tiposDeMemorias =
            dispositivoFisico_.getMemoryProperties();
        for (uint32_t i = 0;
             i < tiposDeMemorias.memoryTypeCount; i++) {
            const auto& tipoDeMemoria =
                tiposDeMemorias.memoryTypes[i];

            bool passaPeloFiltro = (1u << i) & filtro;
            bool possuiAsPropriedades =
                (tipoDeMemoria.propertyFlags & propriedades) ==
                propriedades;

            if (passaPeloFiltro && possuiAsPropriedades) {
                return i;
            }
        }

        throw std::runtime_error(
            "Não foi encontrada um tipo de memória adequado.");
    }

    vk::CommandBuffer iniciarComandoDeUsoUnico() {
        vk::CommandBufferAllocateInfo infoAlloc;
        infoAlloc.commandPool = poolDeComandos_;
        infoAlloc.commandBufferCount = 1;

        vk::CommandBuffer comando =
            dispositivo_.allocateCommandBuffers(infoAlloc)[0];

        vk::CommandBufferBeginInfo infoBegin;
        infoBegin.flags =
            vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

        comando.begin(infoBegin);

        return comando;
    }

    void finalizarComandoDeUsoUnico(vk::CommandBuffer comando) {
        comando.end();

        vk::SubmitInfo infoSubmit;
        infoSubmit.commandBufferCount = 1;
        infoSubmit.pCommandBuffers = &comando;

        filaDeGraficos_.submit(infoSubmit, nullptr);
        filaDeGraficos_.waitIdle();

        dispositivo_.freeCommandBuffers(poolDeComandos_,
                                        {comando});
    }

    void criarSetsDeDescritores() {
        vk::DescriptorSetAllocateInfo infoAloc;
        infoAloc.descriptorPool = poolDeDescritores_;
        infoAloc.descriptorSetCount = 1;
        infoAloc.pSetLayouts = &layoutDoSetDeDescritores_;

        setDeDescritores_ =
            dispositivo_.allocateDescriptorSets(infoAloc)[0];

        vk::DescriptorBufferInfo infoOBU = {bufferDoOBU_, 0,
                                            sizeof(OBU)};

        std::array<vk::WriteDescriptorSet, 1> escreverOBU = {
            vk::WriteDescriptorSet{
                setDeDescritores_,
                0,
                0,
                1,
                vk::DescriptorType::eUniformBuffer,
                {},
                &infoOBU}};

        dispositivo_.updateDescriptorSets(escreverOBU, {});
    }

    void loopPrincipal() {
        while (!glfwWindowShouldClose(janela_)) {
            glfwPollEvents();
            renderizar();
        }
        dispositivo_.waitIdle();
    }

    void renderizar() {
        auto cercaAtual = cercasDeQuadros_[quadroAtual_];
        auto semaforoDeImagemDisponivelAtual =
            semaforosDeImagemDisponivel_[quadroAtual_];
        auto semaforoDeRenderizacaoCompletaAtual =
            semaforosDeRenderizacaoCompleta_[quadroAtual_];

        std::ignore = dispositivo_.waitForFences(
            cercaAtual, false,
            std::numeric_limits<uint64_t>::max());

        uint32_t indiceDaImagem =
            dispositivo_
                .acquireNextImageKHR(
                    swapChain_,
                    std::numeric_limits<uint64_t>::max(),
                    semaforoDeImagemDisponivelAtual, nullptr)
                .value;

        auto& cercaDaImagemAtual =
            imagensEmExecucao_[indiceDaImagem];
        if (cercaDaImagemAtual.has_value()) {
            std::ignore = dispositivo_.waitForFences(
                cercaDaImagemAtual.value(), false,
                std::numeric_limits<uint64_t>::max());
        }
        cercaDaImagemAtual = cercaAtual;

        dispositivo_.resetFences(cercaAtual);

        vk::PipelineStageFlags estagiosAEsperar =
            vk::PipelineStageFlagBits::eColorAttachmentOutput;
        vk::SubmitInfo infoSubmissao;
        infoSubmissao.waitSemaphoreCount = 1;
        infoSubmissao.pWaitSemaphores =
            &semaforoDeImagemDisponivelAtual;
        infoSubmissao.pWaitDstStageMask = &estagiosAEsperar;
        infoSubmissao.commandBufferCount = 1;
        infoSubmissao.pCommandBuffers =
            &buffersDeComandos_[indiceDaImagem];
        infoSubmissao.signalSemaphoreCount = 1;
        infoSubmissao.pSignalSemaphores =
            &semaforoDeRenderizacaoCompletaAtual;

        filaDeGraficos_.submit(infoSubmissao, cercaAtual);

        vk::PresentInfoKHR infoApresentacao;
        infoApresentacao.waitSemaphoreCount = 1;
        infoApresentacao.pWaitSemaphores =
            &semaforoDeRenderizacaoCompletaAtual;
        infoApresentacao.swapchainCount = 1;
        infoApresentacao.pSwapchains = &swapChain_;
        infoApresentacao.pImageIndices = &indiceDaImagem;

        std::ignore =
            filaDeApresentacao_.presentKHR(infoApresentacao);

        quadroAtual_ =
            (quadroAtual_ + 1) % kMaximoQuadrosEmExecucao;
    }

    void destruir() {
        dispositivo_.destroyBuffer(bufferDoOBU_);
        dispositivo_.freeMemory(memoriaBufferDoOBU_);
        dispositivo_.destroyBuffer(bufferDeIndices_);
        dispositivo_.freeMemory(memoriaBufferDeIndices_);
        dispositivo_.destroyBuffer(bufferDeVertices_);
        dispositivo_.freeMemory(memoriaBufferDeVertices_);
        dispositivo_.destroyDescriptorPool(poolDeDescritores_);
        for (auto&& cerca : cercasDeQuadros_) {
            dispositivo_.destroyFence(cerca);
        }
        for (auto&& semaforo :
             semaforosDeRenderizacaoCompleta_) {
            dispositivo_.destroySemaphore(semaforo);
        }
        for (auto&& semaforo : semaforosDeImagemDisponivel_) {
            dispositivo_.destroySemaphore(semaforo);
        }
        dispositivo_.destroyPipeline(pipeline_);
        dispositivo_.destroyShaderModule(shaderDeFragmentos);
        dispositivo_.destroyShaderModule(shaderDeVertices);
        dispositivo_.destroyPipelineLayout(layoutDaPipeline_);
        dispositivo_.destroyDescriptorSetLayout(
            layoutDoSetDeDescritores_);
        for (auto&& framebuffer : framebuffers_) {
            dispositivo_.destroyFramebuffer(framebuffer);
        }
        dispositivo_.destroyRenderPass(passeDeRenderizacao_);
        for (auto&& visao : visoesDasImagensDaSwapchain_) {
            dispositivo_.destroyImageView(visao);
        }
        dispositivo_.destroySwapchainKHR(swapChain_);
        dispositivo_.destroyCommandPool(poolDeComandos_);
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

    const std::vector<const char*> kExtensoesDeDispositivo = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME};

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

    vk::CommandPool poolDeComandos_;

    vk::Format formatoDaSwapchain_;
    vk::Extent2D dimensoesDaSwapchain_;
    vk::SwapchainKHR swapChain_;
    std::vector<vk::Image> imagensDaSwapchain_;
    std::vector<vk::ImageView> visoesDasImagensDaSwapchain_;
    std::vector<vk::Framebuffer> framebuffers_;

    vk::RenderPass passeDeRenderizacao_;

    vk::DescriptorSetLayout layoutDoSetDeDescritores_;
    vk::PipelineLayout layoutDaPipeline_;
    const std::string kCaminhoShaderDeVertices =
        "shaders/shader.vert.spv";
    vk::ShaderModule shaderDeVertices;
    const std::string kCaminhoShaderDeFragmento =
        "shaders/shader.frag.spv";
    vk::ShaderModule shaderDeFragmentos;
    vk::Pipeline pipeline_;

    std::vector<vk::CommandBuffer> buffersDeComandos_;

    size_t quadroAtual_ = 0;
    static const size_t kMaximoQuadrosEmExecucao = 2;
    std::array<vk::Semaphore, kMaximoQuadrosEmExecucao>
        semaforosDeImagemDisponivel_;
    std::array<vk::Semaphore, kMaximoQuadrosEmExecucao>
        semaforosDeRenderizacaoCompleta_;
    std::array<vk::Fence, kMaximoQuadrosEmExecucao>
        cercasDeQuadros_;
    std::vector<std::optional<vk::Fence>> imagensEmExecucao_;

    vk::DescriptorPool poolDeDescritores_;
    vk::DescriptorSet setDeDescritores_;

    const std::vector<Vertice> kVertices = {
        {{3, 0, -2}, {1, 0, 0}},  {{3, 0, 2}, {1, 0, 0}},
        {{-1, 0, -2}, {1, 0, 0}}, {{-1, 0, 2}, {1, 0, 0}},
        {{1, 1, -2}, {0, 0, 1}},  {{1, 1, 2}, {0, 0, 1}},
        {{-3, 1, -2}, {0, 0, 1}}, {{-3, 1, 2}, {0, 0, 1}}};
    vk::Buffer bufferDeVertices_;
    vk::DeviceMemory memoriaBufferDeVertices_;

    const std::vector<uint16_t> kIndices = {4, 5, 6, 5, 7, 6,
                                            0, 1, 2, 1, 3, 2};
    vk::Buffer bufferDeIndices_;
    vk::DeviceMemory memoriaBufferDeIndices_;

    OBU obu_;
    vk::Buffer bufferDoOBU_;
    vk::DeviceMemory memoriaBufferDoOBU_;
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