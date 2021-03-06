#!/usr/bin/env bash

PASTA_SCRIPT="$(dirname "$(realpath "$0")")"
PASTA_PROJETO="${PASTA_SCRIPT}"
PASTA_ALVOS="${PASTA_PROJETO}/códigos"

obterAlvos() {
    if ! cd "${PASTA_ALVOS}"; then
        printf "Erro: não foi possível acessar a pasta de alvos(caminho='%s').\n" "${PASTA_ALVOS}"
        exit 1
    fi
    local ALVOS_NAO_ORDEDNADOS=(*)
    local ALVOS_ORDENADOS
    IFS=$'\n'
    mapfile -t ALVOS_ORDENADOS < <(sort -g -k1 -t- <<<"${ALVOS_NAO_ORDEDNADOS[*]}")
    unset IFS
    echo "${ALVOS_ORDENADOS[@]}"
}

#
# formatar | f - aplica o clang-format sobre todos os alvos
#
cmdFormatar() {
    shopt -s nullglob
    CLANG_FORMAT="clang-format"
    ARQUIVOS_FONTES=("${PASTA_ALVOS}"/**/src/*.cpp)
    ARQUIVOS_CABECALHOS=("${PASTA_ALVOS}"/**/src/*.hpp)
    printf "Formatando %i arquivos...\n" "$((${#ARQUIVOS_FONTES[@]} + ${#ARQUIVOS_CABECALHOS[@]}))"
    (${CLANG_FORMAT} -i "${ARQUIVOS_FONTES[@]}" "${ARQUIVOS_CABECALHOS[@]}")
    printf "Pronto!\n"
}

filtrarTodos() {
    local ALVOS
    local FILTROS
    IFS=" " read -r -a ALVOS <<<"$1"
    IFS=" " read -r -a FILTROS <<<"$2"
    local ALVOS_NAOSELECIONADOS=("${ALVOS[@]}")
    local ALVOS_SELECIONADOS=()
    for FILTRO in "${FILTROS[@]}"; do
        local ALVOS_RECEM_SELECIONADOS=()
        for ALVO in "${ALVOS_NAOSELECIONADOS[@]}"; do
            if [[ ${ALVO} =~ ${FILTRO} ]]; then
                ALVOS_RECEM_SELECIONADOS+=("${ALVO}")
            fi
        done
        for ALVO in "${ALVOS_RECEM_SELECIONADOS[@]}"; do
            ALVOS_NAOSELECIONADOS=("${ALVOS_NAOSELECIONADOS/${ALVO}/}")
            ALVOS_SELECIONADOS+=("${ALVO}")
        done
    done
    echo "${ALVOS_SELECIONADOS[@]}"
}

testarAlvo() {
    local ALVO=$1
    [[ -z "${ALVO}" ]] && return 1
    printf "Testando o código '%s'...\n" "${ALVO}"
    local PASTA_ALVO="${PASTA_ALVOS}/${ALVO}"
    if ! cd "${PASTA_ALVO}"; then
        printf "Erro: não foi possível acessar a pasta do alvo.\n"
        printf "Caminho: %s\n" "${PASTA_ALVO}"
        return 1
    fi
    if cmake -B build && cmake --build build; then
        printf "\nCódigo '%s' montado com sucesso!\n\n" "${ALVO}"
        return 0
    else
        printf "\nFalha ao montar %s!\n\n" "${ALVO}"
        return 1
    fi
}

mostrarAlvos() {
    local ALVOS=("$@")
    printf "%i alvos disponíveis:\n" "${#ALVOS[@]}"
    for ALVO in "${ALVOS[@]}"; do
        printf -- "- %s\n" "${ALVO}"
    done
}

mostrarFiltros() {
    local FILTROS=("$@")
    printf "Executando com %i filtros:\n" "${#FILTROS[@]}"
    for FILTRO in "${FILTROS[@]}"; do
        printf -- "- %s\n" "${FILTRO}"
    done
}

mostrarAlvosATestar() {
    local ALVOS_A_TESTAR=("$@")
    printf "%i alvos a serem testados:\n" "${#ALVOS_A_TESTAR[@]}"
    for ALVO in "${ALVOS_A_TESTAR[@]}"; do
        printf -- "- %s\n" "${ALVO}"
    done
}

mostrarAlvosSucedidos() {
    local SUCESSOS=("$@")
    printf "%i códigos montados com sucesso:\n" "${#SUCESSOS[@]}"
    for ALVO in "${SUCESSOS[@]}"; do
        printf "+ %s\n" "${ALVO}"
    done
}

mostrarAlvosFalhados() {
    local FALHAS=("$@")
    printf "%i códigos com falha na montagem:\n" "${#FALHAS[@]}"
    for ALVO in "${FALHAS[@]}"; do
        printf -- "- %s\n" "${ALVO}"
    done
}

testarAlvos() {
    local ALVOS_A_TESTAR=("$@")
    SUCESSOS=()
    FALHAS=()
    for ALVO in "${ALVOS_A_TESTAR[@]}"; do
        if testarAlvo "${ALVO}"; then
            SUCESSOS+=("${ALVO}")
        else
            FALHAS+=("${ALVO}")
        fi
    done
}

#
# testar | t - verifica se todos os exemplos compilam corretamente
#
# Parâmetros: um conjunto de filtros REGEX, utilizados para selecionar
# os testes. Caso seja vazio, executa-se todos os testes.
#
cmdTestar() {
    local FILTROS=("$@")
    local ALVOS
    IFS=" " read -r -a ALVOS <<<"$(obterAlvos)"
    mostrarAlvos "${ALVOS[@]}"
    local ALVOS_A_TESTAR=()
    if [[ -n "${FILTROS[*]}" ]]; then
        mostrarFiltros "${FILTROS[@]}"
        IFS=" " read -r -a ALVOS_A_TESTAR <<<"$(filtrarTodos "${ALVOS[*]}" "${FILTROS[*]}")"
        mostrarAlvosATestar "${ALVOS_A_TESTAR[@]}"
    else
        printf "Excutando todos os alvos...\n"
        ALVOS_A_TESTAR=("${ALVOS[@]}")
    fi
    testarAlvos "${ALVOS_A_TESTAR[@]}"
    printf "\n"
    mostrarAlvosSucedidos "${SUCESSOS[@]}"
    mostrarAlvosFalhados "${FALHAS[@]}"
    exit $((${#FALHAS[@]} > 0))
}

erroComandoErrado() {
    if [[ -z ${COMANDO} ]]; then
        printf "Nenhum comando foi submetido.\n"
    else
        printf "Comando %s desconhecido.\n" "${COMANDO}"
    fi
    exit 1
}

COMANDO="$1"
PARAMS=("${@:2}")
case "$1" in
formatar | f)
    cmdFormatar
    ;;
testar | t)
    cmdTestar "${PARAMS[@]}"
    ;;
*)
    erroComandoErrado "${COMANDO}"
    ;;
esac
