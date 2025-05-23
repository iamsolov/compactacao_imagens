/*  Projeto – Compactação de Imagens BMP 24 bits
    Arquivo padrão de entrada : imagem22x20.bmp
    Arquivo gerado compactado : imagem22x20.zmp
    Arquivo reconstruído      : imagem22x20_reconstruida.bmp

    Compilação:
        gcc -std=c99 -Wall -O2 bmp_codec.c -o bmp_codec

    Execução (sem argumentos):
        ./bmp_codec
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

/* ---------- Nomes dos arquivos ---------- */
#define BMP_ORIG   "imagem22x20.bmp"
#define ZMP_FILE   "imagem22x20.zmp"
#define BMP_RECON  "imagem22x20_reconstruida.bmp"

/* ---------- Estruturas de cabeçalhos BMP ---------- */
#pragma pack(push, 1)
typedef struct {
    uint16_t bfType;
    uint32_t bfSize;
    uint16_t bfReserved1;
    uint16_t bfReserved2;
    uint32_t bfOffBits;
} BITMAPFILEHEADER;

typedef struct {
    uint32_t biSize;
    int32_t  biWidth;
    int32_t  biHeight;
    uint16_t biPlanes;
    uint16_t biBitCount;
    uint32_t biCompression;
    uint32_t biSizeImage;
    int32_t  biXPelsPerMeter;
    int32_t  biYPelsPerMeter;
    uint32_t biClrUsed;
    uint32_t biClrImportant;
} BITMAPINFOHEADER;
#pragma pack(pop)

typedef struct { uint8_t b, g, r; } Pixel;

/* ---------- Vetor dinâmico simples ---------- */
typedef struct {
    uint8_t *data;
    size_t   size;
    size_t   cap;
} Vec;

static void vec_push(Vec *v, uint8_t value) {
    if (v->size == v->cap) {
        v->cap = v->cap ? v->cap * 2 : 256;
        v->data = realloc(v->data, v->cap);
        if (!v->data) { perror("realloc"); exit(EXIT_FAILURE); }
    }
    v->data[v->size++] = value;
}

/* ---------- Utilitários ---------- */
static long fsize(FILE *f) {
    long p = ftell(f);
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, p, SEEK_SET);
    return sz;
}

/* ---------- Leitura da matriz de pixels ---------- */
static Pixel **read_pixels(FILE *f, int w, int h, int rowB) {
    Pixel **img = malloc(sizeof(Pixel*) * h);
    for (int i = 0; i < h; ++i) img[i] = malloc(sizeof(Pixel) * w);

    uint8_t pad[3];
    for (int r = h - 1; r >= 0; --r) {
        fread(img[r], sizeof(Pixel), w, f);
        fread(pad, 1, rowB - w * 3, f);
    }
    return img;
}

/* ---------- Escrita da matriz de pixels ---------- */
static void write_pixels(FILE *f, Pixel **img, int w, int h, int rowB) {
    uint8_t zero[3] = {0};
    for (int r = h - 1; r >= 0; --r) {
        fwrite(img[r], sizeof(Pixel), w, f);
        fwrite(zero, 1, rowB - w * 3, f);
    }
}

/* ---------- Compactação recursiva ---------- */
static void compress_reg(Pixel **img,int r0,int c0,int h,int w,
                         Vec *R,Vec *G,Vec *B)
{
    if (h <= 3 || w <= 3) {
        Pixel p = img[r0 + h/2][c0 + w/2];
        vec_push(R,p.r); vec_push(G,p.g); vec_push(B,p.b);
        return;
    }
    int h2=h/2, w2=w/2;
    compress_reg(img,r0      ,c0      ,h2,     w2     ,R,G,B);
    compress_reg(img,r0      ,c0+w2   ,h2,     w-w2   ,R,G,B);
    compress_reg(img,r0+h2   ,c0      ,h-h2   ,w2     ,R,G,B);
    compress_reg(img,r0+h2   ,c0+w2   ,h-h2   ,w-w2   ,R,G,B);
}

/* ---------- Descompactação recursiva ---------- */
typedef struct { FILE *f; } ZmpReader;

static void decompress_reg(Pixel **img,int r0,int c0,int h,int w,ZmpReader*z)
{
    if (h <= 3 || w <= 3) {
        uint8_t r=fgetc(z->f), g=fgetc(z->f), b=fgetc(z->f);
        for(int i=0;i<h;++i) for(int j=0;j<w;++j){
            img[r0+i][c0+j].r=r; img[r0+i][c0+j].g=g; img[r0+i][c0+j].b=b;
        }
        return;
    }
    int h2=h/2, w2=w/2;
    decompress_reg(img,r0      ,c0      ,h2,     w2     ,z);
    decompress_reg(img,r0      ,c0+w2   ,h2,     w-w2   ,z);
    decompress_reg(img,r0+h2   ,c0      ,h-h2   ,w2     ,z);
    decompress_reg(img,r0+h2   ,c0+w2   ,h-h2   ,w-w2   ,z);
}

/* ---------- Etapa de compactação ---------- */
static void compactar(void)
{
    FILE *in=fopen(BMP_ORIG,"rb");
    if(!in){perror("BMP_ORIG");exit(EXIT_FAILURE);}
    BITMAPFILEHEADER bfh; BITMAPINFOHEADER bih;
    fread(&bfh,sizeof(bfh),1,in);
    fread(&bih,sizeof(bih),1,in);
    if(bfh.bfType!=0x4D42||bih.biBitCount!=24){
        fprintf(stderr,"Apenas BMP 24 bits aceitos.\n"); exit(EXIT_FAILURE);
    }

    size_t hdrSize=bfh.bfOffBits;
    uint8_t *hdr=malloc(hdrSize);
    fseek(in,0,SEEK_SET); fread(hdr,1,hdrSize,in);

    int W=bih.biWidth, H=bih.biHeight;
    int rowB=((W*3+3)/4)*4;
    Pixel **img=read_pixels(in,W,H,rowB);
    fclose(in);

    Vec vr={0},vg={0},vb={0};
    compress_reg(img,0,0,H,W,&vr,&vg,&vb);

    FILE *out=fopen(ZMP_FILE,"wb");
    if(!out){perror("ZMP_FILE");exit(EXIT_FAILURE);}
    fwrite(hdr,1,hdrSize,out);
    for(size_t i=0;i<vr.size;++i){
        fputc(vr.data[i],out);
        fputc(vg.data[i],out);
        fputc(vb.data[i],out);
    }
    fclose(out);

    long orig = hdrSize + (long)rowB * H;
    long comp = fsize(out=fopen(ZMP_FILE,"rb")); fclose(out);
    printf("=== Compactação ===\n");
    printf("Original    : %ld bytes\n", orig);
    printf("Compactado  : %ld bytes\n", comp);
    printf("Taxa        : %.2f %%\n\n", 100.0*(orig-comp)/orig);

    free(hdr);
}

/* ---------- Etapa de descompactação ---------- */
static void descompactar(void)
{
    FILE *in=fopen(ZMP_FILE,"rb");
    if(!in){perror("ZMP_FILE");exit(EXIT_FAILURE);}
    BITMAPFILEHEADER bfh; BITMAPINFOHEADER bih;
    fread(&bfh,sizeof(bfh),1,in);
    fread(&bih,sizeof(bih),1,in);

    size_t hdrSize=bfh.bfOffBits;
    uint8_t *hdr=malloc(hdrSize);
    fseek(in,0,SEEK_SET); fread(hdr,1,hdrSize,in);

    int W=bih.biWidth, H=bih.biHeight;
    int rowB=((W*3+3)/4)*4;

    fseek(in,hdrSize,SEEK_SET);
    ZmpReader zr={.f=in};

    Pixel **img=malloc(sizeof(Pixel*)*H);
    for(int i=0;i<H;++i) img[i]=calloc(W,sizeof(Pixel));

    decompress_reg(img,0,0,H,W,&zr);
    fclose(in);

    FILE *out=fopen(BMP_RECON,"wb");
    if(!out){perror("BMP_RECON");exit(EXIT_FAILURE);}
    fwrite(hdr,1,hdrSize,out);
    write_pixels(out,img,W,H,rowB);
    fclose(out);

    long comp=fsize(in=fopen(ZMP_FILE,"rb")); fclose(in);
    long rec =fsize(out=fopen(BMP_RECON,"rb")); fclose(out);
    printf("=== Descompactação ===\n");
    printf("Compactado  : %ld bytes\n", comp);
    printf("Reconstruído: %ld bytes\n\n", rec);

    free(hdr);
}

/* ---------- Função principal ---------- */
int main(void)
{
    printf("Processando %s...\n\n", BMP_ORIG);
    compactar();
    descompactar();
    puts("Processo concluído. Verifique os arquivos gerados.");
    return 0;
}
