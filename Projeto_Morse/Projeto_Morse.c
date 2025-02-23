#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"
#include "hardware/adc.h"
#include "hardware/gpio.h"
#include "pico/binary_info.h"
#include "inc/ssd1306.h"
#include "hardware/i2c.h"
#include "hardware/pio.h"

// Biblioteca gerada pelo arquivo .pio durante compilação.
#include "ws2818b.pio.h"

// Definição do número de LEDs e pino.
#define LED_COUNT 25
#define LED_PIN 7

#define LED_ROWS 5    // matriz 5x5  (5 Linhas)
#define LED_COLS 5   //matriz 5x5    (5 colunas)

#define TAMANHO_MAXIMO_MENSAGEM 15 // Máximo de 15 caracteres na mensagem

int led_x = 2, led_y = 2; // Posições iniciais na matriz de LEDs (central)

char mensagem[TAMANHO_MAXIMO_MENSAGEM + 1]; // Buffer para a mensagem atual
int tamanho_mensagem = 0; // Tamanho atual da mensagem

const uint I2C_SDA = 14;  //SDA
const uint I2C_SCL = 15;  // SCL

// Definição dos pinos usados para o joystick e LEDs
const int VRX = 26;          // Pino de leitura do eixo X do joystick (conectado ao ADC)
const int VRY = 27;          // Pino de leitura do eixo Y do joystick (conectado ao ADC)
const int ADC_CHANNEL_0 = 0; // Canal ADC para o eixo X do joystick
const int ADC_CHANNEL_1 = 1; // Canal ADC para o eixo Y do joystick
const int SW = 22;           // Pino de leitura do botão do joystick

const int LED_R = 11;                    // Pino do LED vermelho

// Configuração do pino do buzzer
#define BUZZER_PIN 21

// Configuração da frequência do buzzer (em Hz)
#define BUZZER_FREQUENCY 100

#define BOTAO_A 5  // Botão A
#define BOTAO_B 6  // Botão B
#define MAX_SEQ 5  // Sequência máxima do código morse
#define TIMEOUT 1500  // tempo de espera para identificação do caractere



    // MORSE E CARACTERE 


//Tradução Morse e Caractere
const char *morse_alfabeto[] = {
    ".-",  // A
    "-...", // B
    "-.-.", // C
    "-..", // D
    ".",  // E
    "..-.", // F
    "--.", // G
    "....", // H
    "..",   // I
    ".---", // J
    "-.-",  // K
    ".-..", // L
    "--", // M
    "-.", // N
    "---", // O
    ".--.", // P
    "--.-", // Q
    ".-.", // R
    "...", // S
    "-", // T
    "..-", // U
    "...-", // V
    ".--",  // W
    "-..-", // X
    "-.--",  // Y
    "--..", // Z
    "-----", // 0 
    ".----", // 1
    "..---", // 2
    "...--", // 3
    "....-", // 4
    ".....", // 5
    "-....", // 6
    "--...", // 7
    "---..", // 8
    "----."  // 9
};


const char alfabeto[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

const char *morse_espaco = "----"; // 4 traços representará o espaço entre os caracteres

// Buffer armazena o código Morse inserido pelo usuário
char codigo[MAX_SEQ + 1];  //tamanho máximo do código Morse de um caractere (+1 para o terminador '\0')

// Índice que controla a posição atual dentro do buffer 'codigo'
int indice = 0; //Inicialmente não há símbolos armazenado

absolute_time_t ultima_entrada; // Variável para armazenar o tempo da última entrada de um símbolo Morse

void adicionar_simbolo(char simbolo) 
{
    if (indice < MAX_SEQ)
    // Garante que o buffer não exceda o tamanho máximo permitido
  
    {
        codigo[indice] = simbolo;  // Armazena o símbolo (ponto ou traço) na posição atual do buffer
        indice++;
        codigo[indice] = '\0';

        if (indice == 1)  // Se for o primeiro símbolo da sequência, registra o tempo de entrada
        {
            ultima_entrada = get_absolute_time(); //tempo atual para controle de timeout
        }
    }
}

char traduzir_morse()
{
    for (int i = 0; i < 36; i++) // 26 caracteres do alfabeto e 10 números
    {
        if (strcmp(codigo, morse_espaco) == 0) // Verifica se é espaço
        {
            return ' ';
        }
        if (strcmp(codigo, morse_alfabeto[i]) == 0) // Verifica se é caractere
        {
            return alfabeto[i];
        }
    }
    return '?'; // caso não corresponda a nenhum caractere (não aparece no display)
}

// Função para inicializar o I2C e o display SSD1306
void i2c_oled_init() {
    i2c_init(i2c1, 100 * 1000); // Inicializa o I2C a 100kHz
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C); // SDA
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C); // SCL
    gpio_pull_up(I2C_SDA); 
    gpio_pull_up(I2C_SCL);
    ssd1306_init(); // Inicializa o display OLED SSD1306
}

// Atualiza o display com o conteúdo armazenado
void atualizar_display() {
    // Preparar área de renderização para o display
    struct render_area frame_area = {
        .start_column = 0,
        .end_column = ssd1306_width - 1, // Define a largura total do display
        .start_page = 0,
        .end_page = ssd1306_n_pages - 1 // Define a altura total do display
    };

    // Calcula o tamanho do buffer de renderização
    calculate_render_area_buffer_length(&frame_area);

    // Cria e zera o buffer de pixels
    uint8_t ssd[ssd1306_buffer_length];
    memset(ssd, 0, ssd1306_buffer_length); // Limpa o display temporariamente

    // Escreve a mensagem no buffer
    ssd1306_draw_string(ssd, 0, 0, mensagem);

    // Renderiza o buffer no display
    render_on_display(ssd, &frame_area);
}

void adicionar_caractere_na_mensagem(char letra) {
    if (tamanho_mensagem < TAMANHO_MAXIMO_MENSAGEM) {
        mensagem[tamanho_mensagem] = letra; // Adiciona o novo caractere
        tamanho_mensagem++; // Atualiza o tamanho da mensagem
        mensagem[tamanho_mensagem] = '\0'; // Adiciona o terminador de string
    } else {
        // Move todos os caracteres uma posição para a esquerda se estiver cheio
        for (int i = 0; i < TAMANHO_MAXIMO_MENSAGEM - 1; i++) {
            mensagem[i] = mensagem[i + 1];
        }
        mensagem[TAMANHO_MAXIMO_MENSAGEM - 1] = letra; // Adiciona novo caractere na última posição
    }
    atualizar_display();
}

void limpar_display() {
    // Prepara a área de renderização para o display
    struct render_area frame_area = {
        .start_column = 0,     // Início da coluna (posição 0)
        .end_column = ssd1306_width - 1,   // Fim da coluna (largura total do display)
        .start_page = 0,     // Início da página (linha superior do display)
        .end_page = ssd1306_n_pages - 1    // Fim da página (altura total do display)
    };

    // Calcula o comprimento do buffer de renderização
    calculate_render_area_buffer_length(&frame_area);

    // Cria e zera o buffer de pixels
    uint8_t ssd[ssd1306_buffer_length];  // cria
    memset(ssd, 0, ssd1306_buffer_length);  // apaga

    // Renderiza o buffer no display para limpar
    render_on_display(ssd, &frame_area);

    // Limpa a mensagem armazenada
    memset(mensagem, 0, sizeof(mensagem));
    tamanho_mensagem = 0;
}


        // MATRIZ DE LEDS


// Definição de pixel RGB
struct pixel_t {
    uint8_t G, R, B; //  Cada pixel é composto por três componentes de cor: Verde (G), Vermelho (R) e Azul (B)
  };
  typedef struct pixel_t pixel_t;
  typedef pixel_t npLED_t; // Mudança de nome de "struct pixel_t" para "npLED_t" para ser mais intuitivo
  
  // Declaração do buffer de cores dos leds da matriz.
  npLED_t leds[LED_COUNT];
  
  // Variáveis para uso da máquina PIO.
  PIO np_pio; // Representa a interface PIO utilizada
  uint sm;  // Número da máquina de estado PIO usada para controlar os LEDs
  
  
// Inicializa a máquina PIO para controle da matriz de LEDs.   
  void npInit(uint pin) {

    // Cria programa PIO.
    uint offset = pio_add_program(pio0, &ws2818b_program);
    np_pio = pio0;  // Define que a interface PIO usada será a pio0
  
    // Toma posse da máquina PIO livre.
    sm = pio_claim_unused_sm(np_pio, false);
    if (sm < 0) {  // Caso não haja uma máquina disponível na pio0, tenta usar a pio1
      np_pio = pio1;
      sm = pio_claim_unused_sm(np_pio, true); // Se nenhuma máquina estiver livre, ocorre um erro
    }
  
    // Inicia programa na máquina PIO obtida, configurando o pino e a frequencia do sinal
    ws2818b_program_init(np_pio, sm, offset, pin, 800000.f);
  
    // Limpa buffer de leds.
    for (uint i = 0; i < LED_COUNT; ++i) {
      leds[i].R = 0;  // Define o valor do componente vermelho como 0
      leds[i].G = 0;  // Define o valor do componente verde como 0
      leds[i].B = 0;  // Define o valor do componente azul como 0
    }
  }
  
  
// Atribui uma cor RGB a um LED e define a intensidade.
  void npSetLED(const uint index, const uint8_t r, const uint8_t g, const uint8_t b) {
    leds[index].R = r;  // vermelho
    leds[index].G = g;  // verde
    leds[index].B = b;  // azul
  }
 

// Limpa o buffer de leds, apagando os leds da matriz.
  void npClear() {
    for (uint i = 0; i < LED_COUNT; ++i)
      npSetLED(i, 0, 0, 0);  // Define a cor de cada LED como desligado
  }
  
// Escreve os dados do buffer nos LEDs.
  void npWrite() {
    // Percorre todos os LEDs e envia os valores de cor para a máquina PIO
    for (uint i = 0; i < LED_COUNT; ++i) {
      pio_sm_put_blocking(np_pio, sm, leds[i].G);  // envia o valor do canal verde
      pio_sm_put_blocking(np_pio, sm, leds[i].R);  // envia o valor do canal vermelho
      pio_sm_put_blocking(np_pio, sm, leds[i].B);  // envia o valor do canal azul
    }
    sleep_us(100); // Espera 100us, sinal de RESET do datasheet.
  }  

        // BUZZER

// Inicializa o buzzer utilizando PWM para gerar tons sonoros
void pwm_init_buzzer(uint pin) {
    // Configurar o pino como saída de PWM
    gpio_set_function(pin, GPIO_FUNC_PWM);

    // Obter o slice do PWM associado ao pino
    uint slice_num = pwm_gpio_to_slice_num(pin);

    // Configurar o PWM com frequência desejada
    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, clock_get_hz(clk_sys) / (BUZZER_FREQUENCY * 4096)); // Divisor de clock para gerar frequencia
    pwm_init(slice_num, &config, true);  // Inicializa o PWM no slice correspondente

    // Configurar o duty cycle inicial para zero (buzzer desativado)
    pwm_set_gpio_level(pin, 0);
}

// Função para emitir um beep com duração especificada e aumentar o volume
void beep(uint pin, uint duration_ms) {
    // Obter o slice do PWM associado ao pino
    uint slice_num = pwm_gpio_to_slice_num(pin);

    // Configurar o duty cycle para um nível mais alto (som mais alto)
    pwm_set_gpio_level(pin, 3000); // Aumentar o duty cycle para aumentar o volume

    // Temporização
    sleep_ms(duration_ms);

    // Desativar o sinal PWM (duty cycle 0)
    pwm_set_gpio_level(pin, 0);

    // Pausa entre os beeps
    sleep_ms(100); // Pausa de 100ms
}

        // JOYSTICK MATRIZ


void setup_joystick() {
  // Inicializa o ADC e os pinos de entrada analógica
  adc_init();         // Inicializa o módulo ADC
  adc_gpio_init(VRX); // Configura o pino VRX (eixo X) para entrada ADC
  adc_gpio_init(VRY); // Configura o pino VRY (eixo Y) para entrada ADC

  // Inicializa o pino do botão do joystick
  gpio_init(SW);             // Inicializa o pino do botão
  gpio_set_dir(SW, GPIO_IN); // Configura o pino do botão como entrada
  gpio_pull_up(SW);          // Ativa o pull-up no pino do botão para evitar flutuações
}

// Função de configuração geral
void setup() {
    stdio_init_all(); // Inicializa a porta serial para saída de dados
    setup_joystick(); // Chama a função de configuração do joystick

    // Configurar o pino do LED central como saída
    gpio_init(LED_R);
    gpio_set_dir(LED_R, GPIO_OUT);

    // Configurar o pino do buzzer como saída e inicializar PWM
    gpio_init(BUZZER_PIN);
    gpio_set_dir(BUZZER_PIN, GPIO_OUT);
    pwm_init_buzzer(BUZZER_PIN);
}


// Função para ler os valores dos eixos do joystick (X e Y)
void joystick_read_axis(uint16_t *vrx_value, uint16_t *vry_value)
{
  // Leitura do valor do eixo X do joystick
  adc_select_input(ADC_CHANNEL_0); // Seleciona o canal ADC para o eixo X
  sleep_us(2);                     // Pequeno delay para estabilidade
  *vrx_value = adc_read();         // Lê o valor do eixo X (0-4095)

  // Leitura do valor do eixo Y do joystick
  adc_select_input(ADC_CHANNEL_1); // Seleciona o canal ADC para o eixo Y
  sleep_us(2);                     // Pequeno delay para estabilidade
  *vry_value = adc_read();         // Lê o valor do eixo Y (0-4095)
}

// Função para exibir caracteres no display OLED com delay de 200ms
void display_caractere(char caractere, char opcao_b, bool dual_opcao) {
    // Preparar área de renderização para o display
    struct render_area frame_area = {
        .start_column = 0,
        .end_column = ssd1306_width - 1,
        .start_page = 0,
        .end_page = ssd1306_n_pages - 1
    };

    // Calcula o tamanho do buffer
    calculate_render_area_buffer_length(&frame_area);

    // Criar e zerar o buffer de pixels
    uint8_t ssd[ssd1306_buffer_length];
    memset(ssd, 0, ssd1306_buffer_length);

    if (dual_opcao) {
        // Exibir duas opções (esquerda e direita) no display
        char display_message[20];
        snprintf(display_message, sizeof(display_message), "<  %c       %c>", caractere, opcao_b);
        ssd1306_draw_string(ssd, 0, 0, display_message);
    } else {
        // Exibir um caractere centralizado no display
        char display_message[2] = {caractere, '\0'};
        ssd1306_draw_string(ssd, (ssd1306_width / 2) - 4, 0, display_message);
    }

    render_on_display(ssd, &frame_area);
    sleep_ms(200); // Delay de 200ms
}

// Função para obter o caractere baseado no índice do LED
void obter_caractere(uint index, char *caractere_a, char *caractere_b, bool *dual_opcao) {
    if (index < 12) {
        *caractere_a = 'A' + index; // Letras A-Y para o botão A
        *caractere_b = (index == 0) ? 'Z' : '0' + (index - 1); // Z ou números 0-9 para o botão B
        *dual_opcao = true;
    } else {
        *caractere_a = 'A' + index; // existe apenas um caractere correspondente
        *dual_opcao = false;
    }
}

int led_index;

// Calcula a posição do LED na matriz 5x5 com base nos valores do joystick (0-4095)
void atualizar_leds_com_joystick(uint16_t vrx_value, uint16_t vry_value) {
    int new_led_x = (vry_value * LED_COLS) / 4096;
    int new_led_y = (vrx_value * LED_ROWS) / 4096;

     // Garante que as posições calculadas não saiam dos limites da matriz
    new_led_x = new_led_x < 0 ? 0 : (new_led_x >= LED_COLS ? LED_COLS - 1 : new_led_x);
    new_led_y = new_led_y < 0 ? 0 : (new_led_y >= LED_ROWS ? LED_ROWS - 1 : new_led_y);

    npClear(); // Limpa a matriz de led para acender outro

    // define a posição correta do led na matriz 
    if (new_led_y % 2 == 0) {
        led_index = new_led_y * LED_COLS + (LED_COLS - 1 - new_led_x);
    } else {
        led_index = new_led_y * LED_COLS + new_led_x;
    }

     // Acende o LED em vermelho
    npSetLED(led_index, 255, 0, 0);
    npWrite();

    // Variáveis para armazenar os caracteres a serem exibidos
    char caractere_a, caractere_b;
    bool dual_opcao;

    // Obter o caractere baseado no índice do LED
    obter_caractere(led_index, &caractere_a, &caractere_b, &dual_opcao);

    // Exibir o caractere no display com delay
    display_caractere(caractere_a, caractere_b, dual_opcao);
}


        // Alternancia do modo


#define MORSE_MODE 1  // Modo Morse
#define JOYSTICK_MODE 2 // Modo Joystick

int current_mode = 0; // Modo inicial (0 = parado)
absolute_time_t button_press_start_time; // Armazena o tempo do último pressionamento do botão
bool button_a_pressed = false; // Estado do botão A
bool button_b_pressed = false; // Estado do botão B

// Função analisa o modo
void check_buttons() {
    // Verifica se os botões A e B estão pressionados (nível lógico 0)
    bool button_a_state = gpio_get(BOTAO_A) == 0;
    bool button_b_state = gpio_get(BOTAO_B) == 0;

    // Se o botão A for pressionado pela primeira vez, registra o tempo de início
    if (button_a_state && !button_a_pressed) {
        button_press_start_time = get_absolute_time();
        button_a_pressed = true;
    } else if (!button_a_state && button_a_pressed) {
        button_a_pressed = false;  // Reseta o estado do botão A ao soltar
    }

    // Analisa o botão B
    if (button_b_state && !button_b_pressed) {
        button_press_start_time = get_absolute_time();
        button_b_pressed = true;
    } else if (!button_b_state && button_b_pressed) {
        button_b_pressed = false;
    }

    // Se o botão A for mantido pressionado por 2 segundos, muda para o modo Morse
    if (button_a_pressed && absolute_time_diff_us(button_press_start_time, get_absolute_time()) / 1000 >= 2000) {
        current_mode = MORSE_MODE;
        button_a_pressed = false; // Reseta o estado do botão
    }
    // Se o botão B for mantido pressionado por 2 segundos, muda para o modo Morse
    if (button_b_pressed && absolute_time_diff_us(button_press_start_time, get_absolute_time()) / 1000 >= 1000) {
        current_mode = JOYSTICK_MODE;
        button_b_pressed = false; // Reseta o estado do botão
    }
}

void morse_mode() {
    printf("Modo Morse\n");  // Modo Morse

    // Limpar todos os LEDs ao entrar no modo Morse
    limpar_display();
    npClear();
    npWrite();

    // Declarar o buffer do display OLED dentro da função
    uint8_t ssd[ssd1306_buffer_length];
    memset(ssd, 0, ssd1306_buffer_length); // Inicializar o buffer do display
    ssd1306_draw_string(ssd, 0, 0, "Modo Morse"); // Exibe que está no modo morse (somente no terminal)
    atualizar_display();
    
    while (current_mode == MORSE_MODE) {
        check_buttons(); // Verifica se houve de mudança de modo
        if (gpio_get(BOTAO_A) == 0) {
            adicionar_simbolo('.');  // Botão A é ponto
            sleep_ms(200);
        }
        if (gpio_get(BOTAO_B) == 0) {
            adicionar_simbolo('-'); // Botão B é traço
            sleep_ms(200);
        }

        if (gpio_get(SW) == 0) { // Se o botão do joystick for pressionado, limpa o display
            limpar_display();
            sleep_ms(200); // Pequeno delay para evitar múltiplas leituras
        }

         // Verifica se há um código Morse digitado
        if (indice > 0) {
            // Calcula o tempo decorrido desde a última entrada
            int tempo_decorrido = absolute_time_diff_us(ultima_entrada, get_absolute_time()) / 1000;

            // Se o tempo decorrido for maior que TIMEOUT, traduz o código Morse
            if (tempo_decorrido > TIMEOUT) {
                char letra = traduzir_morse();
                printf("%c", letra);  //Exibe no terminal

                // Adicionar o caractere traduzido à mensagem e atualiza o display
                adicionar_caractere_na_mensagem(letra);

                // Reseta o buffer
                indice = 0;
                memset(codigo, 0, sizeof(codigo));
            }
        }
        
        // Verifica se houve mudança de modo
        if (current_mode != MORSE_MODE) {
            break;
        }
        sleep_ms(100); // Pequeno delay para evitar múltiplas leituras
    }
    
}

void piscar_led_central_morse(char caractere) {
    const char *morse_code = NULL;

    // Procurar o caractere na tabela e obter o código Morse correspondente
    for (int i = 0; i < 36; i++) {
        if (alfabeto[i] == caractere) {
            morse_code = morse_alfabeto[i];  // Obtém o código Morse associado ao caractere
            break;
        }
    }

    // Preparar o display para mostrar o caractere selecionado
    uint8_t ssd[ssd1306_buffer_length];
    memset(ssd, 0, ssd1306_buffer_length); // Limpa o buffer do display

    char display_message[2] = {caractere, '\0'};  // Cria uma string contendo apenas o caractere a ser exibido
    ssd1306_draw_string(ssd, (ssd1306_width / 2) - 4, 0, display_message);  // Exibe o caractere centralizado na tela do OLED
    struct render_area frame_area = {
        .start_column = 0,
        .end_column = ssd1306_width - 1,
        .start_page = 0,
        .end_page = ssd1306_n_pages - 1
    };
    render_on_display(ssd, &frame_area);

    if (morse_code != NULL) {
        // Iterar sobre cada símbolo no código Morse e piscar o LED central e emitir som pelo buzzer
        for (const char *p = morse_code; *p != '\0'; p++) {
            if (*p == '.') {
                // Piscar o LED central e emitir som para um ponto (100 ms)
                gpio_put(LED_R, 1);
                beep(BUZZER_PIN, 200); //200ms para ponto (.)
                gpio_put(LED_R, 0);
                sleep_ms(100); // Pequeno intervalo entre sinais
            } else if (*p == '-') {
                // Piscar o LED central e emitir som para um traço (500 ms)
                gpio_put(LED_R, 1);
                beep(BUZZER_PIN, 1000); //1000ms para traço (-)
                gpio_put(LED_R, 0);
                sleep_ms(100); // Pequeno intervalo entre sinais
            }
        }
    } else {
        // Código Morse não encontrado para o caractere
        printf("Erro: Código Morse não encontrado para o caractere '%c'\n", caractere); //somente no terminal
    }
}


/*
    Alguns leds representaram 2 caracteres, outros 1
    
    "A/Z", "B/0", "C/1", "D/2", "E/3", 
    "F/4", "G/5", "H/6", "I/7", "J/8", 
    "K/9", "L", "M", "N", "O", 
    "P", "Q", "R", "S", "T", 
    "U", "V", "W", "X", "Y"                  

*/

void joystick_mode() {
    limpar_display();  // Limpa o display
    printf("Modo Joystick\n");

    uint8_t ssd[ssd1306_buffer_length];
    memset(ssd, 0, ssd1306_buffer_length);  // Inicializa o buffer zerando os pixels do display 
    ssd1306_draw_string(ssd, 0, 0, "Modo Joystick");  // Exibe que está no modo joystick (somente no terminal)
    atualizar_display();

    // Declaração de variáveis para armazenar os valores lidos do joystick
    uint16_t vrx_value, vry_value;
    
    while (current_mode == JOYSTICK_MODE) {
        check_buttons(); // Verifica se houve mudança no modo
        joystick_read_axis(&vrx_value, &vry_value);  // Le os valores do eixo X e Y
        atualizar_leds_com_joystick(vrx_value, vry_value); // Atualiza o led conforme a posição do joystick

        // Verifica se o botão foi apertado
        if (gpio_get(BOTAO_A) == 0 || gpio_get(BOTAO_B) == 0) {
            // Variáveis para armazenar os caracteres a serem exibidos
            char caractere_a, caractere_b;
            bool dual_opcao;

            // Obtém o caractere baseado no índice do LED
            obter_caractere(led_index, &caractere_a, &caractere_b, &dual_opcao);

            // Confirmar a escolha do caractere e piscar o código Morse correspondente no LED central e buzzer
            if (gpio_get(BOTAO_A) == 0) {  // Se o botão A for pressionado
                adicionar_caractere_na_mensagem(caractere_a);
                piscar_led_central_morse(caractere_a);
            } else if (gpio_get(BOTAO_B) == 0 && dual_opcao) {  // Se o botão B for pressionado entre os leds que possuem 2 opções
                adicionar_caractere_na_mensagem(caractere_b);
                piscar_led_central_morse(caractere_b);
            }

            // Pequeno delay para evitar múltiplas leituras
            sleep_ms(500);
        }

        if (gpio_get(SW) == 0) { // Se o botão do joystick for pressionado, limpar o display
            limpar_display();
            sleep_ms(200); // Pequeno delay para evitar múltiplas leituras
        }

        if (current_mode != JOYSTICK_MODE) {
            break;
        }
        sleep_ms(100); 
    }
}

int main() 
{
    stdio_init_all();   // Inicializa os tipos stdio padrão presentes ligados ao binário

    // Inicialização do i2c para comunicação com o display
    i2c_init(i2c1, ssd1306_i2c_clock * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    i2c_oled_init();  // Inicializa o display OLED

    // Inicializa os botões de controle
    gpio_init(BOTAO_A);
    gpio_init(BOTAO_B);
    gpio_set_dir(BOTAO_A, GPIO_IN);
    gpio_set_dir(BOTAO_B, GPIO_IN);
    gpio_pull_up(BOTAO_A);
    gpio_pull_up(BOTAO_B);
    
    // Configurar joystick
    setup_joystick();

    memset(mensagem, 0, sizeof(mensagem));  // Limpa a variável que armazena a mensagem
    tamanho_mensagem = 0;  // Reinicia o tamanho da mensagem

    // Processo de inicialização completo do OLED SSD1306
    ssd1306_init();

    // Preparar área de renderização para o display (ssd1306_width pixels por ssd1306_n_pages páginas)
    struct render_area frame_area = {
        .start_column = 0,
        .end_column = ssd1306_width - 1,
        .start_page = 0,
        .end_page = ssd1306_n_pages - 1 
    };

    calculate_render_area_buffer_length(&frame_area);

    // zera o display inteiro
    uint8_t ssd[ssd1306_buffer_length]; // Criação do buffer de renderização
    memset(ssd, 0, ssd1306_buffer_length);  // Zerar o buffer
    render_on_display(ssd, &frame_area);  // Aplica o buffer zerado no display (limpando a tela)
 
restart:

    uint16_t vrx_value, vry_value, sw_value; // Variáveis para armazenar os valores do joystick (eixos X e Y) e botão
    setup();                                 // Chama a função de configuração geral do programa
    printf("Joystick-PWM\n");                // Exibe uma mensagem inicial via porta serial

    // Inicializa matriz de LEDs NeoPixel.
    npInit(LED_PIN);  // Configura a matriz de LEDs no pino correspondente
    npClear();  // Garante que todos os LEDs comecem apagados

    // Exibe que está no modo inicial (somente no terminal)
    ssd1306_draw_string(ssd, 0, 0, "Modo Inicial");
    atualizar_display();


    // Loop principal
    while (1)
    {

        check_buttons(); // Verifica se os botões foram pressionados por 3 segundos
        if (current_mode == MORSE_MODE) {
            morse_mode(); // Executa o modo morse caso esteja nessa opção
        } else if (current_mode == JOYSTICK_MODE) {
            joystick_mode();  // Executa o modo Joystick caso esteja nessa opção
        }
        sleep_ms(100); // Pequeno delay antes da próxima verificação
    }

    return 0;
}