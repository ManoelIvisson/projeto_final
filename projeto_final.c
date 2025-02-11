#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "lwip/tcp.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include "pico/binary_info.h"
#include "inc/ssd1306.h"
#include "hardware/i2c.h"
#include "hardware/adc.h"

// Estrutura de um produto
struct Produto {
    char nome[20];
    float preco;
    float promocao;
};

uint cliente_presente = 0;


#define LED_VERMELHO 13          // Define o pino do LED de falha de Conexão
#define LED_VERDE 11          // Define o pino do LED de conexão bem sucedida 
#define WIFI_SSID "Vava"  // Substitua pelo nome da sua rede Wi-Fi
#define WIFI_PASS "Akira#@2718" // Substitua pela senha da sua rede Wi-Fi

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

// Buffer para respostas HTTP
#define HTTP_RESPONSE "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n" \
                      "<!DOCTYPE html><html><body>" \
                      "<h1>Controle do LED</h1>" \
                      "<p><a href=\"/led/on\">Ligar LED</a></p>" \
                      "<p><a href=\"/led/off\">Desligar LED</a></p>" \
                      "<form action=\"/mensagem\" method=\"post\">" \
                      "<input type=\"text\" name=\"mensagem\"/>" \
                      "<input type=\"submit\" value=\"Enviar\"/>" \
                      "</form>" \
                      "</body></html>\r\n"

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

    if (produto.promocao > 0) {
        char promocao[10];
        char preco[10];

        sprintf(promocao, "R$ %.2f", produto.promocao);  // Converte float para string com 2 casas decimais
        sprintf(preco, "R$ %.2f", produto.preco);  

        ssd1306_draw_string(ssd, 60, 32, promocao);
        ssd1306_draw_string(ssd, 25, 50, preco);
        ssd1306_draw_line(ssd, 24, 40, 70, 60, true);
    }
    

    

    render_on_display(ssd, frame_area);  // Atualiza o display com o novo conteúdo
}

// Função de callback para processar requisições HTTP
static err_t http_callback(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    char *display_mensagem[80] = {};

    if (p == NULL) {
        // Cliente fechou a conexão
        tcp_close(tpcb);
        return ERR_OK;
    }

    // Envia a resposta HTTP
    tcp_write(tpcb, HTTP_RESPONSE, strlen(HTTP_RESPONSE), TCP_WRITE_FLAG_COPY);

    // Libera o buffer recebido
    pbuf_free(p);

    return ERR_OK;
}

// Callback de conexão: associa o http_callback à conexão
static err_t connection_callback(void *arg, struct tcp_pcb *newpcb, err_t err) {
    tcp_recv(newpcb, http_callback);  // Associa o callback HTTP
    return ERR_OK;
}

// Função de setup do servidor TCP
static void start_http_server(void) {
    struct tcp_pcb *pcb = tcp_new();
    if (!pcb) {
        printf("Erro ao criar PCB\n");
        return;
    }

    // Liga o servidor na porta 80
    if (tcp_bind(pcb, IP_ADDR_ANY, 80) != ERR_OK) {
        printf("Erro ao ligar o servidor na porta 80\n");
        return;
    }

    pcb = tcp_listen(pcb);  // Coloca o PCB em modo de escuta
    tcp_accept(pcb, connection_callback);  // Associa o callback de conexão

    printf("Servidor HTTP rodando na porta 80...\n");
}

void gerenciadorInterrupcoes(uint gpio, uint32_t events){
    static uint32_t ultimo_tempo = 0;
    uint32_t tempo_atual = to_ms_since_boot(get_absolute_time());

    if (tempo_atual - ultimo_tempo < 200) return; // Debounce de 200ms
    ultimo_tempo = tempo_atual;

    cliente_presente = !cliente_presente;
}

int main() {
    // Configura o LED que indica que o Wifi ainda não foi conectado
    gpio_init(LED_VERMELHO);
    gpio_set_dir(LED_VERMELHO, GPIO_OUT);
    gpio_put(LED_VERMELHO, 1);

    // Configura o LED que indica que o Wifi foi conectado com sucesso
    gpio_init(LED_VERDE);
    gpio_set_dir(LED_VERDE, GPIO_OUT);

    // Produto para exemplo
    struct Produto produto1;

    strcpy(produto1.nome, "Feijao Preto");
    produto1.preco = 8.98;
    produto1.promocao = 6.75;


    // Inicialização do i2c
    i2c_init(i2c1, ssd1306_i2c_clock * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    // Processo de inicialização completo do OLED SSD1306
    ssd1306_init();

    calculate_render_area_buffer_length(&frame_area);

    // zera o display inteiro
    memset(ssd, 0, ssd1306_buffer_length);
    render_on_display(ssd, &frame_area);

    char *mensagem[2]; 

    mensagem[0] = "Conectando no Wifi";
    mensagem[1] = "Aguarde um momento";

    exibirMensagem(mensagem, 2, ssd, &frame_area);

    stdio_init_all();  // Inicializa a saída padrão
    sleep_ms(10000);
    printf("Iniciando servidor HTTP\n");

    // Inicializa o Wi-Fi
    if (cyw43_arch_init()) {
        printf("Erro ao inicializar o Wi-Fi\n");
        return 1;
    }

    cyw43_arch_enable_sta_mode();
    printf("Conectando ao Wi-Fi...\n");

    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASS, CYW43_AUTH_WPA2_AES_PSK, 10000)) {
        printf("Falha ao conectar ao Wi-Fi\n");
        return 1;
    }else {
        printf("Connected.\n");
        // Read the ip address in a human readable way
        uint8_t *ip_address = (uint8_t*)&(cyw43_state.netif[0].ip_addr.addr);
        printf("Endereço IP %d.%d.%d.%d\n", ip_address[0], ip_address[1], ip_address[2], ip_address[3]);

        static char ip_str[20];  // Buffer para armazenar o IP formatado
        snprintf(ip_str, sizeof(ip_str), "%d.%d.%d.%d", 
                ip_address[0], ip_address[1], ip_address[2], ip_address[3]);

        mensagem[0] = "Conectado no IP";
        mensagem[1] = ip_str;

        gpio_put(LED_VERMELHO, 0);
        gpio_put(LED_VERDE, 1);
    }

    printf("Wi-Fi conectado! Galera\n");
    printf("Para ligar ou desligar o LED acesse o Endereço IP seguido de /led/on ou /led/off\n");

    // Configuração do Joystick
    adc_init();

    adc_gpio_init(26);
    adc_gpio_init(27);

    // Inicia o servidor HTTP
    start_http_server();

    // zera o display inteiro
    memset(ssd, 0, ssd1306_buffer_length);
    render_on_display(ssd, &frame_area);
    
    // Loop principal
    while (true) {
        cyw43_arch_poll();  // Necessário para manter o Wi-Fi ativo

        // Lendo valores dos eixos do Joystick
        adc_select_input(0);
        uint adc_y_raw = adc_read();

        adc_select_input(1);
        uint adc_x_raw = adc_read();
        
        if (adc_y_raw > 4000) {
            if (!cliente_presente) {
                exibirProduto(produto1, ssd, &frame_area);
                cliente_presente = 1;
            }
        } else if (adc_y_raw < 100) {
            if (cliente_presente) {
                // zera o display inteiro
                memset(ssd, 0, ssd1306_buffer_length);
                render_on_display(ssd, &frame_area);
                cliente_presente = 0;
            }
        }
        

        sleep_ms(100);
    }

    cyw43_arch_deinit();  // Desliga o Wi-Fi (não será chamado, pois o loop é infinito)
    return 0;
}
