/*
 * ============================================================================
 *  Security Awareness Demo - Beetle BadUSB (ATmega32U4 / Arduino Leonardo R3)
 * ============================================================================
 *
 *  PROPOSITO (uso etico e educativo):
 *  Demonstra, de forma CONTROLADA e INOFENSIVA, por que nunca devemos conectar
 *  dispositivos USB desconhecidos. Ao ser plugado em um PC Windows, o Beetle
 *  age como teclado (HID) e abre um POPUP NATIVO com uma mensagem de
 *  conscientizacao. Nada e baixado, instalado, exfiltrado ou alterado.
 *
 *  >>> USE SOMENTE em maquinas suas ou com AUTORIZACAO EXPLICITA e por ESCRITO.
 *  >>> Rodar isto em computadores de terceiros sem permissao pode ser crime.
 *
 *  Hardware: HiLetgo Beetle BadUSB - ATmega32U4 (compativel Arduino Leonardo).
 *
 *  ==========================================================================
 *  CORRECAO DE TECLADO ABNT2 (o ponto central deste sketch)
 *  ==========================================================================
 *  A biblioteca Keyboard do Arduino digita assumindo layout US. Em Windows
 *  ABNT2 (PT-BR) os SIMBOLOS saem errados (" ' : ( ) etc. viram acentos).
 *
 *  Aqui NAO usamos a tabela de layout da biblioteca. Em vez disso, enviamos
 *  RELATORIOS HID CRUS (scancode USB + modificador) escolhendo exatamente a
 *  TECLA FISICA de um teclado ABNT2 para cada caractere. O mapeamento foi
 *  derivado do layout ABNT2 (referencia: projeto duck2spark_abnt2,
 *  encoder/resources/br.properties). Assim o texto sai correto no Windows
 *  PT-BR, independentemente da tabela de layout da lib.
 *
 *  Tambem digitamos DEVAGAR (atraso por tecla) para a VM nao perder teclas.
 * ============================================================================
 */

#include <Keyboard.h> // usada apenas para Win+R (teclas especiais) e begin/end
#include <HID.h>

/* ------------------------------------------------------------------ */
/*  CONFIGURACAO                                                       */
/* ------------------------------------------------------------------ */

// Mensagem do popup. Linha unica: o stager via caixa Executar nao
// permite newlines literais (Enter submeteria o comando). Para
// quebras de linha seria necessario escape PowerShell (`n ou
// $([char]10)), que exige caracteres fora do mapa ABNT2 atual.
const char *AWARENESS_MESSAGE =
    "USB DESCONHECIDO DETECTADO! "
    "Isto e uma demonstracao de SEGURANCA. "
    "NUNCA conecte dispositivos USB de procedencia duvidosa.";

const char *AWARENESS_TITLE = "ALERTA DE SEGURANCA";

// Modo da demo:
//   1 = POPUP      -> abre um popup nativo via mshta (inofensivo, nada baixa).
//   2 = DOWNLOAD   -> baixa aviso.txt do seu GitHub e abre no Notepad.
//                     NAO executa o conteudo baixado (so exibe). Demonstra
//                     que o USB consegue puxar conteudo arbitrario da
//                     internet e dropar no disco em segundos, sem clique.
#define DEMO_MODE 2

// URL crua (raw) do aviso.txt no seu GitHub. Substitua USER/REPO e o
// branch/arquivo conforme o seu repo. Use o link raw.githubusercontent.com.
// Restricao: so caracteres mapeados no ABNT2 (letras, digitos, : / . - _).
const char *DOWNLOAD_URL =
    "https://raw.githubusercontent.com/mmantova/secaware/main/aviso.txt";

// Espera inicial (ms) para o Windows/VM reconhecer o teclado.
const unsigned int STARTUP_DELAY_MS = 6000;
// Pausa (ms) depois do Win+R, antes de digitar. 1200 ms e curto demais em
// algumas maquinas — o Run abre mas nao pega o foco a tempo, e a digitacao
// vai pra janela anterior (ex.: PowerShell). 2500 ms da folga. Se o comando
// ainda cair noutra janela, suba mais.
const unsigned int RUN_DIALOG_DELAY_MS = 2500;
// Atraso (ms) entre cada tecla. 18 ms = valor PROVADO no modo 1 (popup
// funcionou). Valores muito baixos (6 ms) causam "modifier bleed" no
// Windows: o Shift nao registra a tempo e `:` vira `^`, `;` vira `~`.
// Se quiser testar mais rapido, desca com cuidado e valide no Windows.
const unsigned int PER_KEY_DELAY_MS = 18;
// Pausa curta (ms) em transicoes (Win+R, Enter).
const unsigned int KEY_DELAY_MS = 80;

/* ------------------------------------------------------------------ */
/*  HID: envio de scancode cru                                         */
/*                                                                     */
/*  Modificadores (bitmask do byte "modifiers" do relatorio HID):      */
/*    0x02 = Left Shift                                                */
/* ------------------------------------------------------------------ */
#define MOD_NONE      0x00
#define MOD_SHIFT     0x02
#define MOD_RIGHT_ALT 0x40 // AltGr — necessario para / ? no ABNT2 (/ = AltGr+Q, ? = AltGr+W)

// Relatorio de teclado: {modifiers, reserved, k0..k5}
struct RawKeyReport {
  uint8_t modifiers;
  uint8_t reserved;
  uint8_t keys[6];
};

// Envia UMA tecla (scancode HID + modificador) e solta em seguida.
void sendRawKey(uint8_t usage, uint8_t modifiers) {
  RawKeyReport r = {modifiers, 0, {usage, 0, 0, 0, 0, 0}};
  HID().SendReport(2, &r, sizeof(r)); // report id 2 = teclado (igual a lib)
  delay(PER_KEY_DELAY_MS);
  RawKeyReport rel = {0, 0, {0, 0, 0, 0, 0, 0}};
  HID().SendReport(2, &rel, sizeof(rel)); // solta todas as teclas
  delay(PER_KEY_DELAY_MS);
}

/* ------------------------------------------------------------------ */
/*  MAPA ABNT2: caractere ASCII -> (scancode HID, modificador)         */
/*                                                                     */
/*  Scancodes HID USB relevantes:                                      */
/*    letras a-z: 0x04 (a) .. tabela abaixo                            */
/*    1-9: 0x1E..0x26 ; 0 = 0x27 ; espaco = 0x2C                       */
/*    - = 0x2D ; . = 0x37 ; , = 0x36 ; = = 0x2E                       */
/*  Teclas especificas do ABNT2:                                       */
/*    tecla a esquerda do "1" (HID 0x35): "'" sem shift, '"' com shift */
/*    tecla a direita do L/Ç (HID 0x34): ';' sem shift, ':' com shift  */
/*    tecla '/?' a direita do M (HID 0x38): '/' sem shift, '?' shift   */
/*    parenteses: Shift+9 e Shift+0 (HID 0x26/0x27)                    */
/* ------------------------------------------------------------------ */

// Retorna true e preenche usage/mod se o caractere for suportado.
bool mapCharAbnt2(char c, uint8_t *usage, uint8_t *mod) {
  // letras minusculas a..z
  if (c >= 'a' && c <= 'z') {
    *usage = 0x04 + (c - 'a');
    *mod = MOD_NONE;
    return true;
  }
  // letras maiusculas A..Z
  if (c >= 'A' && c <= 'Z') {
    *usage = 0x04 + (c - 'A');
    *mod = MOD_SHIFT;
    return true;
  }
  // digitos 1..9 e 0
  if (c >= '1' && c <= '9') {
    *usage = 0x1E + (c - '1');
    *mod = MOD_NONE;
    return true;
  }
  if (c == '0') {
    *usage = 0x27;
    *mod = MOD_NONE;
    return true;
  }

  switch (c) {
    case ' ':  *usage = 0x2C; *mod = MOD_NONE;  return true; // espaco
    case '-':  *usage = 0x2D; *mod = MOD_NONE;  return true; // hifen
    case '.':  *usage = 0x37; *mod = MOD_NONE;  return true; // ponto
    case ',':  *usage = 0x36; *mod = MOD_NONE;  return true; // virgula

    // aspas: tecla a esquerda do "1" (HID 0x35) no ABNT2 (CONFIRMADO OK)
    case '\'': *usage = 0x35; *mod = MOD_NONE;  return true; // '
    case '"':  *usage = 0x35; *mod = MOD_SHIFT; return true; // "

    // ABNT2: ';' e ':' na tecla a direita do M (HID 0x38). Confirmado por
    // calibracao + duck2spark_abnt2/encoder/resources/br.properties.
    case ';':  *usage = 0x38; *mod = MOD_NONE;  return true; // ;
    case ':':  *usage = 0x38; *mod = MOD_SHIFT; return true; // :

    // ABNT2: '/' e '?' NAO sao teclas diretas — sao AltGr+Q e AltGr+W.
    // (Confirmado por br.properties: ASCII_2F = KEY_Q,RIGHT_ALT ; ASCII_3F = KEY_W,RIGHT_ALT)
    case '/':  *usage = 0x14; *mod = MOD_RIGHT_ALT; return true; // / = AltGr+Q
    case '?':  *usage = 0x1A; *mod = MOD_RIGHT_ALT; return true; // ? = AltGr+W

    // parenteses: Shift+9 e Shift+0 (CONFIRMADO OK)
    case '(':  *usage = 0x26; *mod = MOD_SHIFT; return true; // ( = Shift+9
    case ')':  *usage = 0x27; *mod = MOD_SHIFT; return true; // ) = Shift+0

    // outros simbolos uteis
    case '$':  *usage = 0x21; *mod = MOD_SHIFT; return true; // $ = Shift+4
    case '=':  *usage = 0x2E; *mod = MOD_NONE;  return true; // = (tecla =/+)

    // Shift+digito (ABNT2: 1! 2@ 3# 4$ 5% 6^ 7& 8* 9( 0))
    case '!':  *usage = 0x1E; *mod = MOD_SHIFT; return true; // ! = Shift+1
    case '@':  *usage = 0x1F; *mod = MOD_SHIFT; return true; // @ = Shift+2
    case '#':  *usage = 0x20; *mod = MOD_SHIFT; return true; // # = Shift+3
    case '%':  *usage = 0x22; *mod = MOD_SHIFT; return true; // % = Shift+5
    case '&':  *usage = 0x24; *mod = MOD_SHIFT; return true; // & = Shift+7
    case '*':  *usage = 0x25; *mod = MOD_SHIFT; return true; // * = Shift+8

    // Shift + tecla =/+ e -/_
    case '+':  *usage = 0x2E; *mod = MOD_SHIFT; return true; // + = Shift+=
    case '_':  *usage = 0x2D; *mod = MOD_SHIFT; return true; // _ = Shift+-
  }
  return false; // caractere nao suportado
}

/* ------------------------------------------------------------------ */
/*  Digitacao usando o mapa ABNT2                                      */
/* ------------------------------------------------------------------ */
void typeAbnt2(const char *text) {
  for (const char *p = text; *p != '\0'; ++p) {
    if (*p == '\n' || *p == '\r') continue; // newlines ignorados (Enter submeteria)
    uint8_t usage, mod;
    if (mapCharAbnt2(*p, &usage, &mod)) {
      sendRawKey(usage, mod);
    }
    // caracteres nao mapeados sao ignorados (nenhum no payload atual)
  }
}

// Verifica se TODOS os caracteres das strings sao mapeaveis no ABNT2.
// Retorna true se tudo OK. Usado para validar antes de disparar a demo
// e piscar o LED onboard em caso de caractere nao suportado.
bool validateString(const char *text) {
  for (const char *p = text; *p != '\0'; ++p) {
    if (*p == '\n' || *p == '\r') continue; // newlines OK (ignorados na digitacao)
    uint8_t usage, mod;
    if (!mapCharAbnt2(*p, &usage, &mod)) {
      return false;
    }
  }
  return true;
}

// Scancode HID do NumLock (tecla do teclado numerico).
#define HID_NUMLOCK 0x53

// Alterna o estado do NumLock do host (pisca o LED do teclado do PC).
// Usado no lugar do LED onboard em placas sem LED. Cada chamada = 1 toggle.
// O NumLock e seguro de alternar aqui: a demo nao usa teclas do numpad,
// entao o estado so afeta o LED indicador, nao a digitacao ABNT2.
void toggleNumLock() {
  RawKeyReport press = {MOD_NONE, 0, {HID_NUMLOCK, 0, 0, 0, 0, 0}};
  HID().SendReport(2, &press, sizeof(press));
  delay(40);
  RawKeyReport rel = {MOD_NONE, 0, {0, 0, 0, 0, 0, 0}};
  HID().SendReport(2, &rel, sizeof(rel));
  delay(120);
}

// Sinaliza erro (caractere nao mapeado) piscando o NumLock do host 20x.
// Sao 20 blinks = 40 toggles (numero par), entao o NumLock volta ao estado
// original ao final. Nao depende de LED onboard.
void blinkError() {
  for (int i = 0; i < 20; ++i) {
    toggleNumLock(); // acende
    toggleNumLock(); // apaga
  }
}

/* ------------------------------------------------------------------ */
/*  Sequencia da demo                                                  */
/* ------------------------------------------------------------------ */

// Win+R usando a lib (teclas especiais, independe de layout de texto).
void openRunDialog() {
  Keyboard.press(KEY_LEFT_GUI);
  delay(KEY_DELAY_MS);
  Keyboard.press('r'); // 'r' e letra, posicao igual em US e ABNT2
  delay(KEY_DELAY_MS);
  Keyboard.releaseAll();
}

void showAwarenessPopup() {
  // Pulso unico no NumLock do host: sinal visual de que a validacao
  // passou e a digitacao vai comecar. 2 toggles (numero par) restauram
  // o estado original do NumLock. Nao depende de LED onboard.
  toggleNumLock();
  delay(200);
  toggleNumLock();

  // Abordagem OTIMIZADA E SILENCIOSA: usa mshta (subsystem GUI, NENHUMA
  // janela de console aparece) com VBScript inline. O powershell mostrava
  // uma janela preta de console atras do popup; o mshta nao mostra nada.
  // Tambem e mais rapido: mshta e minúsculo vs powershell que carrega o .NET.
  //
  // Comando unico que cabe no limite do Executar (~260 chars):
  //   mshta vbscript:Close(CreateObject("WScript.Shell").Popup("msg",0,"titulo",0x40))
  // 0x40 = icone de aviso (warning). Close() fecha o mshta apos o popup.
  // Usa aspas duplas (mapeadas no ABNT2: shift + tecla a esq. do "1").
  // Mensagem e titulo NAO podem conter aspas duplas.
  typeAbnt2("mshta vbscript:Close(CreateObject(\"WScript.Shell\").Popup(\"");
  typeAbnt2(AWARENESS_MESSAGE);
  typeAbnt2("\",0,\"");
  typeAbnt2(AWARENESS_TITLE);
  typeAbnt2("\",0x40))");
  delay(KEY_DELAY_MS);
  Keyboard.press(KEY_RETURN);
  delay(KEY_DELAY_MS);
  Keyboard.releaseAll();
}

// Demo DOWNLOAD: baixa aviso.txt do GitHub e abre no Notepad.
// NAO executa o conteudo baixado — apenas exibe. Demonstra o canal de
// download (o USB puxa conteudo arbitrario da internet e dropa no disco
// em segundos, sem clique) sem construir um stager de execucao.
//
// Usa cmd + curl (Windows 10+ tem curl.exe nativo em System32) em vez de
// PowerShell, porque muitas maquinas corporativas bloqueiam powershell.exe
// por AppLocker/SRP/WDAC. Custo: uma janela de cmd preta aparece brevemente.
//
// Comando (cabe no Executar, ~130 chars):
//   cmd /c "curl -o aviso.txt <url> && notepad aviso.txt"
//
// So chars mapeados no ABNT2: letras, espaco, / " - . : & (todos mapeados).
void showDownloadDemo() {
  // Pulso unico no NumLock: validacao passou, digitacao vai comecar.
  toggleNumLock();
  delay(200);
  toggleNumLock();

  typeAbnt2("cmd /c \"curl -o aviso.txt ");
  typeAbnt2(DOWNLOAD_URL);
  typeAbnt2(" && notepad aviso.txt\"");
  delay(KEY_DELAY_MS);
  Keyboard.press(KEY_RETURN);
  delay(KEY_DELAY_MS);
  Keyboard.releaseAll();
}

/* ------------------------------------------------------------------ */
void setup() {
  delay(STARTUP_DELAY_MS);

  Keyboard.begin(); // inicializa o endpoint HID de teclado. Precisa estar
                    // ativo antes do blinkError(), que usa NumLock via HID.

  // Valida payload antes de qualquer acao HID de texto. Se houver caractere
  // nao mapeado no ABNT2, pisca o NumLock do host e NAO dispara (evita texto
  // corrompido). Sinal visivel mesmo em placas sem LED onboard.
  #if DEMO_MODE == 1
  if (!validateString(AWARENESS_MESSAGE) ||
      !validateString(AWARENESS_TITLE)) {
    blinkError();
    Keyboard.end();
    return;
  }
  #elif DEMO_MODE == 2
  if (!validateString(DOWNLOAD_URL)) {
    blinkError();
    Keyboard.end();
    return;
  }
  #endif

  openRunDialog();
  delay(RUN_DIALOG_DELAY_MS);

  #if DEMO_MODE == 1
  showAwarenessPopup();
  #elif DEMO_MODE == 2
  showDownloadDemo();
  #endif

  Keyboard.end();
}

void loop() {
  // vazio de proposito: dispara so uma vez.
}
