/*  
    Projeto 2 – Compactação de Imagens BMP 24 bits
    Universidade Presbiteriana Mackenzie – Algoritmos e Programação II
    Nome: Leandro Solovjovas dos Santos
    RA: 10438426
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

//Nomes dos arquivos
#define ARQUIVO_ORIGINAL        "imagemOriginal.bmp"
#define ARQUIVO_COMPACTADO      "imagemCompactada.zmp"
#define ARQUIVO_DESCOMPACTADO   "imagemDescompactada.bmp"

//Estruturas de cabeçalho BMP
#pragma pack(push, 1)
typedef struct 
{
    uint16_t tipo;
    uint32_t tamanho;
    uint16_t reservado1;
    uint16_t reservado2;
    uint32_t offset_dados;
} CABECALHO_ARQUIVO;

typedef struct 
{
    uint32_t tamanho;
    int32_t  largura;
    int32_t  altura;
    uint16_t planos;
    uint16_t bits_por_pixel;
    uint32_t compressao;
    uint32_t tamanho_imagem;
    int32_t  resolucaoX;
    int32_t  resolucaoY;
    uint32_t cores_usadas;
    uint32_t cores_importantes;
} CABECALHO_INFO;
#pragma pack(pop)

//Estrutura de pixel RGB
typedef struct 
{ 
    uint8_t b; 
    uint8_t g;
    uint8_t r;
} Pixel;

//Vetor dinâmico para armazenar bytes
typedef struct 
{
    uint8_t *dados;
    size_t   tamanho;
    size_t   capacidade;
} Vetor;

void adicionar_byte(Vetor *vetor, uint8_t valor) 
{
    if (vetor->tamanho == vetor->capacidade) 
    {
        vetor->capacidade = vetor->capacidade ? vetor->capacidade * 2 : 256;
        vetor->dados = realloc(vetor->dados, vetor->capacidade);
        if (!vetor->dados) 
        {
            printf("Erro de memória\n");
            exit(1);
        }
    }
    vetor->dados[vetor->tamanho++] = valor;
}

long tamanho_arquivo(FILE *arquivo) 
{
    long atual = ftell(arquivo);
    fseek(arquivo, 0, SEEK_END);
    long tamanho = ftell(arquivo);
    fseek(arquivo, atual, SEEK_SET);
    return tamanho;
}

Pixel **carregar_pixels(FILE *arquivo, int largura, int altura, int bytes_por_linha) 
{
    Pixel **imagem = malloc(sizeof(Pixel*) * altura);
    for (int i = 0; i < altura; i++)
    {
        imagem[i] = malloc(sizeof(Pixel) * largura);
    }

    uint8_t lixo[3];
    for (int linha = altura - 1; linha >= 0; linha--) 
    {
        fread(imagem[linha], sizeof(Pixel), largura, arquivo);
        fread(lixo, 1, bytes_por_linha - largura * 3, arquivo);
    }

    return imagem;
}

void salvar_pixels(FILE *arquivo, Pixel **imagem, int largura, int altura, int bytes_por_linha) 
{
    uint8_t zeros[3] = {0};
    for (int linha = altura - 1; linha >= 0; linha--) 
    {
        fwrite(imagem[linha], sizeof(Pixel), largura, arquivo);
        fwrite(zeros, 1, bytes_por_linha - largura * 3, arquivo);
    }
}

void compactar_area(Pixel **imagem, int lin, int col, int alt, int larg, Vetor *R, Vetor *G, Vetor *B) 
{
    if (alt <= 3 || larg <= 3) 
    {
        Pixel p = imagem[lin + alt / 2][col + larg / 2];
        adicionar_byte(R, p.r);
        adicionar_byte(G, p.g);
        adicionar_byte(B, p.b);
        return;
    }

    int meio_alt = alt / 2;
    int meio_larg = larg / 2;

    compactar_area(imagem, lin, col, meio_alt, meio_larg, R, G, B);
    compactar_area(imagem, lin, col + meio_larg, meio_alt, larg - meio_larg, R, G, B);
    compactar_area(imagem, lin + meio_alt, col, alt - meio_alt, meio_larg, R, G, B);
    compactar_area(imagem, lin + meio_alt, col + meio_larg, alt - meio_alt, larg - meio_larg, R, G, B);
}

void descompactar_area(Pixel **imagem, int lin, int col, int alt, int larg, FILE *arquivo) 
{
    if (alt <= 3 || larg <= 3) 
    {
        uint8_t r = fgetc(arquivo);
        uint8_t g = fgetc(arquivo);
        uint8_t b = fgetc(arquivo);

        for (int i = 0; i < alt; i++) 
        {
            for (int j = 0; j < larg; j++) 
            {
                imagem[lin + i][col + j].r = r;
                imagem[lin + i][col + j].g = g;
                imagem[lin + i][col + j].b = b;
            }
        }
        return;
    }

    int meio_alt = alt / 2;
    int meio_larg = larg / 2;

    descompactar_area(imagem, lin, col, meio_alt, meio_larg, arquivo);
    descompactar_area(imagem, lin, col + meio_larg, meio_alt, larg - meio_larg, arquivo);
    descompactar_area(imagem, lin + meio_alt, col, alt - meio_alt, meio_larg, arquivo);
    descompactar_area(imagem, lin + meio_alt, col + meio_larg, alt - meio_alt, larg - meio_larg, arquivo);
}

void compactar_imagem() 
{
    FILE *entrada = fopen(ARQUIVO_ORIGINAL, "rb");
    if (!entrada) 
    {
        printf("Erro ao abrir imagem.\n");
        exit(1);
    }

    CABECALHO_ARQUIVO cab_arquivo;
    CABECALHO_INFO cab_info;

    fread(&cab_arquivo, sizeof(cab_arquivo), 1, entrada);
    fread(&cab_info, sizeof(cab_info), 1, entrada);

    if (cab_arquivo.tipo != 0x4D42 || cab_info.bits_por_pixel != 24) 
    {
        printf("Apenas imagens BMP 24 bits são aceitas.\n");
        exit(1);
    }

    size_t tam_cabecalho = cab_arquivo.offset_dados;
    uint8_t *dados_cabecalho = malloc(tam_cabecalho);
    fseek(entrada, 0, SEEK_SET);
    fread(dados_cabecalho, 1, tam_cabecalho, entrada);

    int largura = cab_info.largura;
    int altura = cab_info.altura;
    int bytes_linha = ((largura * 3 + 3) / 4) * 4;

    Pixel **imagem = carregar_pixels(entrada, largura, altura, bytes_linha);
    fclose(entrada);

    Vetor vr = {0}, vg = {0}, vb = {0};
    compactar_area(imagem, 0, 0, altura, largura, &vr, &vg, &vb);

    FILE *saida = fopen(ARQUIVO_COMPACTADO, "wb");
    fwrite(dados_cabecalho, 1, tam_cabecalho, saida);
    for (size_t i = 0; i < vr.tamanho; i++) 
    {
        fputc(vr.dados[i], saida);
        fputc(vg.dados[i], saida);
        fputc(vb.dados[i], saida);
    }
    fclose(saida);

    long tam_original = tam_cabecalho + (long)bytes_linha * altura;
    FILE *arq_comp = fopen(ARQUIVO_COMPACTADO, "rb");
    long tam_comp = tamanho_arquivo(arq_comp);
    fclose(arq_comp);

    printf("Imagem original:     %ld bytes\n", tam_original);
    printf("Imagem compactada:   %ld bytes\n", tam_comp);
}

void descompactar_imagem() 
{
    FILE *entrada = fopen(ARQUIVO_COMPACTADO, "rb");
    if (!entrada) 
    {
        printf("Erro ao abrir arquivo compactado.\n");
        exit(1);
    }

    CABECALHO_ARQUIVO cab_arquivo;
    CABECALHO_INFO cab_info;

    fread(&cab_arquivo, sizeof(cab_arquivo), 1, entrada);
    fread(&cab_info, sizeof(cab_info), 1, entrada);

    size_t tam_cabecalho = cab_arquivo.offset_dados;
    uint8_t *dados_cabecalho = malloc(tam_cabecalho);
    fseek(entrada, 0, SEEK_SET);
    fread(dados_cabecalho, 1, tam_cabecalho, entrada);

    int largura = cab_info.largura;
    int altura = cab_info.altura;
    int bytes_linha = ((largura * 3 + 3) / 4) * 4;

    fseek(entrada, tam_cabecalho, SEEK_SET);

    Pixel **imagem = malloc(sizeof(Pixel*) * altura);
    for (int i = 0; i < altura; i++) 
    {
        imagem[i] = calloc(largura, sizeof(Pixel));
    }

    descompactar_area(imagem, 0, 0, altura, largura, entrada);
    fclose(entrada);

    FILE *saida = fopen(ARQUIVO_DESCOMPACTADO, "wb");
    fwrite(dados_cabecalho, 1, tam_cabecalho, saida);
    salvar_pixels(saida, imagem, largura, altura, bytes_linha);
    fclose(saida);

    printf("\nImagem descompactada salva como %s.\n", ARQUIVO_DESCOMPACTADO);
}

int main()
{
    printf("Iniciando processamento da imagem %s.\n\n", ARQUIVO_ORIGINAL);
    compactar_imagem();
    descompactar_imagem();
    printf("\nProcesso finalizado com sucesso.\n");
    return 0;
}