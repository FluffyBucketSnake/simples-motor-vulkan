#include <algorithm>
#include <array>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

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
        criarDispositivoLogicoEFilas();
        criarSwapchain();
        criarPasseDeRenderizacao();
        criarFramebuffers();
        carregarShaders();
        criarPipeline();
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
        auto resultado = std::find_if(
            dispositivosFisicos.begin(), dispositivosFisicos.end(),
            [this](vk::PhysicalDevice d) { return verificarDispositivo(d); });

        if (resultado == dispositivosFisicos.end()) {
            throw std::runtime_error(
                "Não foi encontrado um dispositivo físico compartível.");
        }

        dispositivoFisico_ = *resultado;
    }

    bool verificarDispositivo(const vk::PhysicalDevice& dispositivo) {
        bool possuiFilas = verificarFilasDoDispositivo(dispositivo);
        bool suportaExtensoes = verificarSuporteDeExtensoes(dispositivo);
        bool adequadoParaSwapchain =
            suportaExtensoes && verificarSuporteDeSwapchain(dispositivo);

        return possuiFilas && adequadoParaSwapchain;
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

    bool verificarSuporteDeExtensoes(const vk::PhysicalDevice& dispositivo) {
        auto extensoesDisponiveis =
            dispositivo.enumerateDeviceExtensionProperties();
        std::unordered_set<std::string> extensoesRestantes(
            kExtensoesDeDispositivo.begin(), kExtensoesDeDispositivo.end());

        for (const auto& extensao : extensoesDisponiveis) {
            extensoesRestantes.erase(extensao.extensionName);
        }

        return extensoesRestantes.empty();
    }

    bool verificarSuporteDeSwapchain(const vk::PhysicalDevice& dispositivo) {
        bool possuiFormatos =
            !dispositivo.getSurfaceFormatsKHR(superficie_).empty();
        bool possuiModosDeApresentacao =
            !dispositivo.getSurfacePresentModesKHR(superficie_).empty();

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
        info.queueCreateInfoCount = static_cast<uint32_t>(infos.size());
        info.pQueueCreateInfos = infos.data();
        info.enabledExtensionCount =
            static_cast<uint32_t>(kExtensoesDeDispositivo.size());
        info.ppEnabledExtensionNames = kExtensoesDeDispositivo.data();
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
                    [tipo](auto familia) { return familia.queueFlags & tipo; });

        bool foiEncontrada = familia == familias.end();
        if (foiEncontrada) {
            return {};
        }

        return std::distance(familias.begin(), familia);
    }

    void criarSwapchain() {
        auto capacidades =
            dispositivoFisico_.getSurfaceCapabilitiesKHR(superficie_);
        auto formatosDisponiveis =
            dispositivoFisico_.getSurfaceFormatsKHR(superficie_);
        auto modosDeApresentacaoDisponiveis =
            dispositivoFisico_.getSurfacePresentModesKHR(superficie_);

        auto formato = escolherFormatoDaSwapchain(formatosDisponiveis);
        auto modoDeApresentacao =
            escolherModoDeApresentacao(modosDeApresentacaoDisponiveis);
        dimensoesDaSwapchain_ = escolherDimensoesDaSwapchain(capacidades);

        uint32_t numeroDeImagens =
            std::max(capacidades.minImageCount + 1, capacidades.maxImageCount);

        vk::SwapchainCreateInfoKHR info;

        info.surface = superficie_;
        info.minImageCount = numeroDeImagens;
        info.imageFormat = formatoDaSwapchain_ = formato.format;
        info.imageColorSpace = formato.colorSpace;
        info.imageExtent = dimensoesDaSwapchain_;
        info.imageArrayLayers = 1;
        info.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;

        info.preTransform = capacidades.currentTransform;
        info.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
        info.presentMode = modoDeApresentacao;
        info.clipped = true;
        info.oldSwapchain = nullptr;

        if (familiaDeApresentacao_ != familiaDeGraficos_) {
            std::array<uint32_t, 2> familias{familiaDeApresentacao_,
                                             familiaDeGraficos_};
            info.imageSharingMode = vk::SharingMode::eConcurrent;
            info.queueFamilyIndexCount = static_cast<uint32_t>(familias.size());
            info.pQueueFamilyIndices = familias.data();

            swapChain_ = dispositivo_.createSwapchainKHR(info);
        } else {
            info.imageSharingMode = vk::SharingMode::eExclusive;
            // info.queueFamilyIndexCount = 0;
            // info.pQueueFamilyIndices = nullptr;

            swapChain_ = dispositivo_.createSwapchainKHR(info);
        }

        imagensDaSwapchain_ = dispositivo_.getSwapchainImagesKHR(swapChain_);
        visoesDasImagensDaSwapchain_.reserve(imagensDaSwapchain_.size());
        for (const auto& imagem : imagensDaSwapchain_) {
            visoesDasImagensDaSwapchain_.push_back(
                criarVisaoDeImagem(imagem, formatoDaSwapchain_));
        }
    }

    vk::SurfaceFormatKHR escolherFormatoDaSwapchain(
        const std::vector<vk::SurfaceFormatKHR>& formatosDisponiveis) {
        for (const auto& formato : formatosDisponiveis) {
            if (formato.format == vk::Format::eB8G8R8A8Srgb &&
                formato.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
                return formato;
            }
        }
        return formatosDisponiveis[0];
    }

    vk::PresentModeKHR escolherModoDeApresentacao(
        const std::vector<vk::PresentModeKHR>& modosDeApresentacaoDisponiveis) {
        auto comeco = modosDeApresentacaoDisponiveis.begin();
        auto fim = modosDeApresentacaoDisponiveis.end();

        if (std::find(comeco, fim, vk::PresentModeKHR::eMailbox) != fim) {
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

            vk::Extent2D dimensoes = {static_cast<uint32_t>(largura),
                                      static_cast<uint32_t>(altura)};

            vk::Extent2D minimo = capacidades.minImageExtent;
            vk::Extent2D maximo = capacidades.maxImageExtent;

            dimensoes.width =
                std::clamp(dimensoes.width, minimo.width, maximo.width);
            dimensoes.height =
                std::clamp(dimensoes.height, minimo.height, maximo.height);

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
        info.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
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
        anexoCor.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
        anexoCor.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
        anexoCor.initialLayout = vk::ImageLayout::eUndefined;
        anexoCor.finalLayout = vk::ImageLayout::ePresentSrcKHR;

        vk::AttachmentReference refAnexoCor;
        refAnexoCor.attachment = 0;
        refAnexoCor.layout = vk::ImageLayout::eColorAttachmentOptimal;

        vk::SubpassDescription subpasse;
        // subpasse.flags = {};
        // subpasse.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
        // subpasse.inputAttachmentCount = 0;
        // subpasse.pInputAttachments = nullptr;
        subpasse.colorAttachmentCount = 1;
        subpasse.pColorAttachments = &refAnexoCor;
        // subpasse.pResolveAttachments = nullptr;
        // subpasse.pDepthStencilAttachment = nullptr;
        // subpasse.preserveAttachmentCount = 0;
        // subpasse.pPreserveAttachments = nullptr;

        vk::RenderPassCreateInfo info;
        info.attachmentCount = 1;
        info.pAttachments = &anexoCor;
        info.subpassCount = 1;
        info.pSubpasses = &subpasse;
        // info.dependencyCount = 0;
        // info.pDependencies = nullptr;

        passeDeRenderizacao_ = dispositivo_.createRenderPass(info);
    }

    void criarFramebuffers() {
        std::transform(
            visoesDasImagensDaSwapchain_.begin(),
            visoesDasImagensDaSwapchain_.end(),
            std::back_inserter(framebuffers_),
            [this](const vk::ImageView& i) { return criarFramebuffer(i); });
    }

    vk::Framebuffer criarFramebuffer(const vk::ImageView& imagemDaSwapchain) {
        vk::FramebufferCreateInfo info;
        info.renderPass = passeDeRenderizacao_;
        info.attachmentCount = 1;
        info.pAttachments = &imagemDaSwapchain;
        info.width = dimensoesDaSwapchain_.width;
        info.height = dimensoesDaSwapchain_.height;
        info.layers = 1;

        return dispositivo_.createFramebuffer(info);
    }

    void carregarShaders() {
        shaderDeVertices = carregarShader(kCaminhoShaderDeVertices);
        shaderDeFragmentos = carregarShader(kCaminhoShaderDeFragmento);
    }

    vk::ShaderModule carregarShader(const std::string& caminho) {
        std::ifstream arquivoDoShader(caminho, std::ios::binary);

        if (!arquivoDoShader.is_open()) {
            throw std::runtime_error("Não foi possível abrir o arquivo '" +
                                     caminho + "'!");
        }

        std::vector<char> codigoDoShader(
            (std::istreambuf_iterator<char>(arquivoDoShader)),
            (std::istreambuf_iterator<char>()));

        vk::ShaderModuleCreateInfo info;
        info.codeSize = codigoDoShader.size();
        info.pCode = reinterpret_cast<const uint32_t*>(codigoDoShader.data());

        return dispositivo_.createShaderModule(info);
    }

    void criarPipeline() {
        std::array<vk::PipelineShaderStageCreateInfo, 2> estagios{
            vk::PipelineShaderStageCreateInfo{
                {}, vk::ShaderStageFlagBits::eVertex, shaderDeVertices, "main"},
            vk::PipelineShaderStageCreateInfo{
                {},
                vk::ShaderStageFlagBits::eFragment,
                shaderDeFragmentos,
                "main"}};
        
        vk::PipelineVertexInputStateCreateInfo infoVertices;

        vk::PipelineInputAssemblyStateCreateInfo infoEntrada;
        infoEntrada.topology = vk::PrimitiveTopology::eTriangleList;
        infoEntrada.primitiveRestartEnable = false;

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

        vk::PipelineRasterizationStateCreateInfo infoRasterizador;
        infoRasterizador.polygonMode = vk::PolygonMode::eFill;
        infoRasterizador.cullMode = vk::CullModeFlagBits::eBack;
        infoRasterizador.frontFace = vk::FrontFace::eCounterClockwise;

        vk::PipelineMultisampleStateCreateInfo infoAmostragem;

        vk::PipelineColorBlendStateCreateInfo infoMistura;

        vk::GraphicsPipelineCreateInfo info;
        // info.flags = {}
        info.stageCount = static_cast<uint32_t>(estagios.size());
        info.pStages = estagios.data();
        info.pVertexInputState = &infoVertices;
        info.pInputAssemblyState = &infoEntrada;
        info.pViewportState = &infoViewport;
        info.pRasterizationState = &infoRasterizador;
        info.pMultisampleState = &infoAmostragem;
        info.pColorBlendState = &infoMistura;

        // pipeline_ = dispositivo_.createGraphicsPipeline({}, info);
    }

    void loopPrincipal() {
        while (!glfwWindowShouldClose(janela_)) {
            glfwPollEvents();
        }
    }

    void destruir() {
        dispositivo_.destroyShaderModule(shaderDeFragmentos);
        dispositivo_.destroyShaderModule(shaderDeVertices);
        for (auto&& framebuffer : framebuffers_) {
            dispositivo_.destroyFramebuffer(framebuffer);
        }
        dispositivo_.destroyRenderPass(passeDeRenderizacao_);
        for (auto&& visao : visoesDasImagensDaSwapchain_) {
            dispositivo_.destroyImageView(visao);
        }
        dispositivo_.destroySwapchainKHR(swapChain_);
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

    vk::Format formatoDaSwapchain_;
    vk::Extent2D dimensoesDaSwapchain_;
    vk::SwapchainKHR swapChain_;
    std::vector<vk::Image> imagensDaSwapchain_;
    std::vector<vk::ImageView> visoesDasImagensDaSwapchain_;
    std::vector<vk::Framebuffer> framebuffers_;

    vk::RenderPass passeDeRenderizacao_;

    const std::string kCaminhoShaderDeVertices = "shaders/shader.vert.spv";
    vk::ShaderModule shaderDeVertices;
    const std::string kCaminhoShaderDeFragmento = "shaders/shader.frag.spv";
    vk::ShaderModule shaderDeFragmentos;
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