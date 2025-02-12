#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "lwip/tcp.h"
#include "lwip/dns.h"
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
    uint id;
    char nome[20];
    float preco;
    float promocao;
};

uint cliente_presente = 0;


#define LED_VERMELHO 13          // Define o pino do LED de falha de Conexão
#define LED_VERDE 11          // Define o pino do LED de conexão bem sucedida 
#define WIFI_SSID "Vava"  // Substitua pelo nome da sua rede Wi-Fi
#define WIFI_PASS "Akira#@2718" // Substitua pela senha da sua rede Wi-Fi

// Thingspeak
#define CHANNEL_ID 2836570  // Id do canal
#define WRITE_API_KEY "8AS7O6H17JAT1HHH" //API Key para o canal Tempo de Olho

// URL do ThingSpeak
#define THINGSPEAK_HOST "api.thingspeak.com"
#define THINGSPEAK_PORT 80

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


struct tcp_pcb *pcb;

// Callback chamado quando a conexão TCP é estabelecida
static err_t tcp_connected(void *arg, struct tcp_pcb *tpcb, err_t err) {
    if (err != ERR_OK) {
        printf("Erro ao conectar ao servidor: %d\n", err);
        tcp_close(tpcb);
        return err;
    }

    printf("Conectado ao ThingSpeak!\n");

    // Enviar dados usando tcp_write()
    char request[256];
    float tempo = *(float *)arg;  // Recuperar o valor do argumento passado
    snprintf(request, sizeof(request),
             "GET /update?api_key=%s&field1=%.2f HTTP/1.1\r\n"
             "Host: %s\r\n"
             "Connection: close\r\n\r\n",
             WRITE_API_KEY, tempo, THINGSPEAK_HOST);

    err = tcp_write(tpcb, request, strlen(request), TCP_WRITE_FLAG_COPY);
    if (err != ERR_OK) {
        printf("Erro ao tentar enviar dados ao ThingSpeak\n");
        tcp_close(tpcb);
        return err;
    }

    // Garantir que os dados sejam enviados
    tcp_output(tpcb);
    printf("Dados enviados para ThingSpeak!\n");

    return ERR_OK;
}

// Callback para a resolução DNS
void dns_callback(const char *name, const ip_addr_t *ipaddr, void *arg) {
    if (!ipaddr) {
        printf("Falha ao resolver DNS\n");
        return;
    }

    printf("Endereço IP resolvido: %s\n", ipaddr_ntoa(ipaddr));

    // Criar um novo PCB TCP
    pcb = tcp_new();
    if (!pcb) {
        printf("Erro ao criar PCB TCP\n");
        return;
    }

    // Conectar-se ao servidor ThingSpeak
    err_t err = tcp_connect(pcb, ipaddr, THINGSPEAK_PORT, tcp_connected);
    if (err != ERR_OK) {
        printf("Falha ao conectar ao servidor: %d\n", err);
        tcp_close(pcb);
    }
}

// Função para iniciar o envio de dados ao ThingSpeak
void enviar_dados_thingspeak(float tempo) {
    ip_addr_t dest_ip;
    err_t err = dns_gethostbyname(THINGSPEAK_HOST, &dest_ip, dns_callback, &tempo);

    if (err == ERR_OK) {
        // O endereço IP já está em cache, podemos conectar diretamente
        dns_callback(THINGSPEAK_HOST, &dest_ip, &tempo);
    } else if (err == ERR_INPROGRESS) {
        // A resolução DNS será feita de forma assíncrona
        printf("Resolvendo DNS...\n");
    } else {
        printf("Erro ao iniciar resolução DNS: %d\n", err);
    }
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
    produto1.id = 1;
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

        gpio_put(LED_VERMELHO, 0);
        gpio_put(LED_VERDE, 1);
    }

    printf("Wi-Fi conectado! Galera\n");
    printf("Para ligar ou desligar o LED acesse o Endereço IP seguido de /led/on ou /led/off\n");

    enviar_dados_thingspeak(2000);

    // Configuração do Joystick
    adc_init();

    adc_gpio_init(26);
    adc_gpio_init(27);

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
