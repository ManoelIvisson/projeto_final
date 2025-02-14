#include "inc/ssd1306.h"
#include "hardware/i2c.h"
#include <string.h>
#include <stdio.h>

// Estrutura de um produto
struct Produto {
    uint id;
    char nome[20];
    float preco;
    float promocao;
};

// Produtos para serem exibidos (Apenas na versão protótipo, na real os produtos seriam recuperados de um banco)
struct Produto produto_atual;
struct Produto produto1;
struct Produto produto2;

const uint I2C_SDA = 14;
const uint I2C_SCL = 15;

// Preparar área de renderização para o display (ssd1306_width pixels por ssd1306_n_pages páginas)
struct render_area frame_area = {
    start_column : 0,
    end_column : ssd1306_width - 1,
    start_page : 0,
    end_page : ssd1306_n_pages - 1
};

uint8_t ssd[ssd1306_buffer_length];

void exibirMensagem(char *mensagem[], uint num_mensagens, uint8_t *ssd, struct render_area *frame_area) {
    // Zera o display
    memset(ssd, 0, ssd1306_buffer_length);
    render_on_display(ssd, frame_area);

    int x = 3;  // Posição inicial do texto
    int y = 0;  // Linha inicial
    int char_width = 8;  // Largura média de um caractere (ajuste se necessário)
    int max_width = ssd1306_width - x;  // Largura máxima para o texto

    for (int i = 0; i < num_mensagens; i++) {
        char *str = mensagem[i];

        while (*str) {  // Percorre a string caracter por caracter
            if (x + char_width > max_width) {  // Se ultrapassar a largura do display
                x = 3;   // Volta para a margem esquerda
                y += 8;  // Vai para a próxima linha (altura de um caractere)
                
                if (y >= frame_area->end_page * 8) {  // Se atingir o final do display
                    return;  // Para de desenhar
                }
            }

            ssd1306_draw_char(ssd, x, y, *str);
            x += char_width;  // Move a posição para a direita
            str++;  // Passa para o próximo caractere
        }

        x = 3;   // Reinicia x para o início da linha
        y += 8;  // Vai para a próxima linha após cada mensagem

        if (y >= frame_area->end_page * 8) {  // Se atingir o final do display
            break;  // Para de desenhar
        }
    }

    render_on_display(ssd, frame_area);  // Atualiza o display com o novo conteúdo
}

void exibirProduto(struct Produto produto, uint8_t *ssd, struct render_area *frame_area) {
    // Zera o display
    memset(ssd, 0, ssd1306_buffer_length);
    render_on_display(ssd, frame_area);

    int y = 0;
    int largura_palavra = 8 * strlen(produto.nome);
    int inicio = (128 - largura_palavra) / 2;

    // Desenha o nome do produto e a linha de separação
    ssd1306_draw_string(ssd, inicio, y, produto.nome);
    ssd1306_draw_line(ssd, 0, 8, 127, 8, true);

    char preco[10];

    sprintf(preco, "R$ %.2f", produto.preco);  // Converte float para string com 2 casas decimais

    // Verifica se o produto possui promoção
    if (produto.promocao > 0) {
        char promocao[10];
        sprintf(promocao, "R$ %.2f", produto.promocao);  

        ssd1306_draw_string(ssd, 60, 32, promocao); // Promoção em "evidência"
        ssd1306_draw_string(ssd, 25, 50, preco);
        ssd1306_draw_line(ssd, 24, 40, 70, 60, true); // Linha de corte no preço original para indicar promoção
    } else {
        ssd1306_draw_string(ssd, 60, 32, preco); // Preço em "evidência"
    }
    

    render_on_display(ssd, frame_area);  // Atualiza o display com o novo conteúdo
}