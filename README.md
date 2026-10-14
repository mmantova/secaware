# Security Awareness Demo — Beetle BadUSB

Demonstração **educativa e controlada** de por que nunca se deve conectar
dispositivos USB desconhecidos. Ao plugar o Beetle (ATmega32U4 / Arduino
Leonardo R3) em um PC **Windows**, ele age como um teclado (HID) e abre um
**popup nativo** com uma mensagem de conscientização. Nada é baixado,
instalado, exfiltrado ou alterado no computador.

---

## ⚠️ Aviso de uso ético (leia antes de tudo)

- Use **somente** em máquinas suas ou com **autorização explícita e por
  escrito** do responsável.
- Executar isto em computadores de terceiros sem permissão pode configurar
  **crime** (acesso não autorizado a sistema).
- O objetivo é **conscientizar**, não causar dano. O payload apenas mostra
  uma mensagem.

---

## Hardware

- **HiLetgo Beetle BadUSB** — microcontrolador **ATMEGA32U4**, 5V / 16 MHz.
- Compatível com o board **Arduino Leonardo** (HID nativo via `Keyboard.h`).

---

## Como gravar na placa

Você pode gravar tanto pela **Arduino IDE** quanto pela **`arduino-cli`**
(linha de comando, mais rápido e scriptável).

### Opção A — Arduino IDE (Windows da demo, de preferência)

1. Instale a **Arduino IDE**.
2. Em **Tools → Board**, selecione **Arduino Leonardo**.
3. Conecte o Beetle. Em **Tools → Port**, selecione a porta COM que aparecer.
   - Dica: o Leonardo expõe uma janela curta de gravação ao resetar. Se o
     upload falhar, clique em **Upload** e dê um toque no reset da placa
     quando a IDE indicar "Uploading".
4. Abra `security_awareness.ino`.
5. Clique em **Upload** (seta →).
6. Pronto. Da próxima vez que a placa for energizada em um Windows, a
   sequência dispara.

### Opção B — arduino-cli (macOS ou Windows)

Pré-requisito: ter o core AVR instalado.

```bash
# Instalar o core (uma vez)
arduino-cli core install arduino:avr

# Descobrir a porta e o FQBN do Beetle conectado
arduino-cli board list

# Compilar
arduino-cli compile --fqbn arduino:avr:leonardo security_awareness

# Gravar (substitua /dev/cu.usbmodemXXXX pela porta listada acima)
arduino-cli upload -p /dev/cu.usbmodemXXXX --fqbn arduino:avr:leonardo security_awareness
```

> ⚠️ **Atenção ao gravar a partir do macOS:** ao finalizar o upload, o
> Leonardo **reseta automaticamente** e executa o sketch. Como o alvo do
> payload é Windows, no Mac a demo "dispara" sem efeito útil: o
> `KEY_LEFT_GUI` vira **Cmd** (Cmd+R recarrega a aba ativa) e o comando
> PowerShell é digitado como texto solto na janela com foco. Nenhum popup
> aparece (PowerShell é Windows-only). Para evitar distrações, deixe uma
> janela inerte (Terminal vazio ou Finder) com foco durante os ~6 s de
> espera após o upload.

---

## Setup mode (jumper de segurança) — opcional, exige GPIO acessível

> **Beetle blindado (HiLetgo):** este modelo NÃO expõe GPIO — não há como
> ligar o jumper. Mantenha `SETUP_MODE_ENABLED` como `false` (padrão) no
> `.ino`. A placa sempre dispara ao ser energizada; use a **janela de
> `STARTUP_DELAY_MS` (6 s)** como mecanismo de segurança (ver abaixo).

Para quem usa uma placa **com GPIO exposto** (ex.: Arduino Leonardo com
pinos acessíveis), o setup mode permite regravar/testar na própria máquina
sem que o payload dispare: ligue um jumper entre `SETUP_MODE_PIN` (padrão
**D2**) e **GND**, e defina `SETUP_MODE_ENABLED` como `true` no `.ino`.

- **Com o jumper e flag ligada:** a placa energiza, **não dispara**, e pisca
  o NumLock do host **3× lentas** como confirmação.
- **Sem o jumper (ou flag desligada):** comportamento normal — dispara ao
  energizar.

### Segurança no Beetle blindado (sem jumper)

Como não há pino acessível, use estas duas salvaguardas:

1. **Janela de 6 segundos:** após gravar no Windows, a IDE reinicia a placa
   automaticamente e o sketch dispara. Você tem `STARTUP_DELAY_MS` (6 s)
   antes da primeira tecla HID — **desconecte o Beetle nesse intervalo** se
   não quiser que ele dispare no seu PC. Aumente `STARTUP_DELAY_MS` se
   precisar de mais tempo.
2. **Regrave no macOS:** no Mac o payload misfire de forma inofensiva
   (Win+R vira Cmd+R e o comando `mshta` é digitado como texto solto na
   janela com foco — nada executa). Deixe uma janela inerte (Terminal vazio)
   com foco durante os ~6 s após o upload. Veja o aviso em "Opção B —
   arduino-cli".

> Verifique o pinout da sua placa se for usar o jumper. D0/D1 são Serial
> RX/TX (evite). D2, D4, D6, D8 costumam ser boas escolhas em placas com
> pinos expostos. No macOS o sinal do NumLock pode não ser visível.

---

## Como funciona

1. A placa é energizada e espera `STARTUP_DELAY_MS` para o Windows
   reconhecê-la como teclado.
2. **Confere o setup mode** (se `SETUP_MODE_ENABLED` estiver ligado): com
   jumper `SETUP_MODE_PIN`→GND, a demo **não dispara** — pisca o NumLock
   do host **3× lentas** e para. No Beetle blindado a flag fica desligada
   e este passo é pulado (a placa sempre dispara; use a janela de 6 s).
3. **Valida o payload**: confere se todos os caracteres da string relevante
   são mapeáveis no layout ABNT2. No modo 1, valida `AWARENESS_MESSAGE` e
   `AWARENESS_TITLE`; no modo 2, valida `DOWNLOAD_URL`. Se algum caractere
   não for suportado, o **NumLock do host pisca rapidamente 20×** (via HID)
   e a demo **não dispara** — sinal de que o texto precisa ser ajustado.
   Não depende de LED onboard: funciona mesmo em placas sem LED.
4. Envia **Win+R** (abre a caixa "Executar").
5. **Modo 1** "digita" um comando **`mshta` + VBScript** que abre um popup
   nativo via `WScript.Shell` (ver abaixo). **Modo 2** digita um PowerShell
   que baixa `aviso.txt` do GitHub e abre no Notepad (ver seção "Modo 2").
   ```text
   mshta vbscript:Close(CreateObject("WScript.Shell").Popup("<mensagem>",0,"<título>",0x40))
   ```
   `0x40` = ícone de aviso (warning). `Close()` encerra o `mshta` após o popup.
6. Pressiona **Enter** → o popup de conscientização aparece.
7. `loop()` fica vazio: a demo dispara **uma única vez**.

> **Por que `mshta` e não `powershell`?** O `mshta` é subsystem GUI: nenhuma
> janela de console (janela preta) aparece atrás do popup, e ele é bem mais
> rápido para iniciar (não carrega o runtime .NET do PowerShell). É a forma
> **rápida e silenciosa** de mostrar um popup a partir da caixa Executar.
> O custo: usa aspas duplas `"` (mapeadas no ABNT2) em vez de simples `'`, e
> mensagem/título não podem conter aspas duplas.

> A mensagem é em **linha única**: o stager via caixa Executar não aceita
> newlines literais (Enter submeteria o comando).

---

## Modo 2 — Download + Notepad (demo do risco de "download cego")

Para conscientizar **desenvolvedores**, o modo 2 (`DEMO_MODE = 2`) demonstra
que o USB consegue **baixar conteúdo arbitrário da internet e dropar no
disco** em segundos, sem clique e sem permissão. Diferença crucial: **não
executa** o que baixa — só abre no Notepad. É o mesmo canal de download de
um ataque real, sem o passo de execução.

Fluxo do modo 2:

1. Win+R abre o Executar.
2. Digita:
   ```text
   cmd /c "curl -o aviso.txt <DOWNLOAD_URL> && notepad aviso.txt"
   ```
3. Enter → uma janela de cmd preta aparece brevemente, o `curl` baixa o
   `aviso.txt` do seu GitHub para o diretório atual (perfil do usuário) e o
   Notepad abre com o aviso.

> **Por que `cmd + curl` e não `powershell`?** Muitas máquinas corporativas
> bloqueiam `powershell.exe` por AppLocker/SRP/WDAC — o comando PowerShell
> falha com "O Windows não pode acessar o dispositivo...". O `curl.exe` é
> nativo do Windows 10+ (em `System32`) e raramente é bloqueado. O custo é
> uma janela de cmd preta que pisca brevemente (não é 100% silencioso). Se o
> PowerShell estiver liberado e você preferir silêncio, há um bloco
> PowerShell comentado em `showDownloadDemo()` no `.ino`.

O `aviso.txt` (incluído neste projeto) é uma mensagem forte para devs: explica
o que aconteceu, o que **poderia** ter acontecido (C2, exfil de secrets,
ransomware) e as mitigações. Commit `aviso.txt` no seu repo e aponte
`DOWNLOAD_URL` para o link `raw.githubusercontent.com/.../aviso.txt`.

### Por que isso é seguro (e não é um stager)

- O conteúdo baixado é **texto inerte** aberto no Notepad — nada é executado.
- Não há `Invoke-Expression`, `iex`, `&`, `.ps1` rodado, nem `.exe` lançado.
- Mudar a URL para algo malicioso **não vira ataque**: o fluxo sempre abre no
  Notepad, nunca executa. O limite é exibir, não rodar.
- A lição para o dev é justamente o oposto: "este canal de download já
  funcionou; em um ataque real, em vez de Notepad seria um beacon C2".

### Setup do modo 2

1. O `aviso.txt` já está publicado em [github.com/mmantova/secaware](https://github.com/mmantova/secaware) (repo público).
2. No `.ino`, `DOWNLOAD_URL` já aponta para:
   `https://raw.githubusercontent.com/mmantova/secaware/main/aviso.txt`
3. Confirme `#define DEMO_MODE 2`.
4. Grave a placa e plugue no Windows → o Notepad abre com o aviso baixado do GitHub.

> Restrição de caracteres: `DOWNLOAD_URL` só pode conter letras, dígitos,
> `: / . - _` (todos mapeados no ABNT2). Sem `?`, `=`, `+`, `%`, acentos.
> URLs do GitHub raw costumam respeitar isso. Se a URL tiver caracteres não
> mapeados, a validação falha e o NumLock pisca 20×.

> O `aviso.txt` baixado fica em `%TEMP%\aviso.txt` — pode apagar sem risco.

---

## Personalização

Tudo que você normalmente vai editar está no topo do `.ino`:

| Constante             | O que faz                                              |
|-----------------------|--------------------------------------------------------|
| `DEMO_MODE`           | `1` = popup (mshta). `2` = download + Notepad.        |
| `DOWNLOAD_URL`        | URL raw do `aviso.txt` no seu GitHub (modo 2).         |
| `AWARENESS_MESSAGE`   | Texto do popup (linha única; sem aspas duplas `"`).    |
| `AWARENESS_TITLE`     | Título da janela (sem aspas duplas `"`).               |
| `STARTUP_DELAY_MS`    | Espera inicial para o Windows reconhecer o teclado.    |
| `RUN_DIALOG_DELAY_MS` | Pausa após abrir o Executar, antes de digitar.         |
| `PER_KEY_DELAY_MS`    | Atraso entre cada tecla. ~6 ms (rápido); suba se houver perda. |
| `KEY_DELAY_MS`        | Pausa curta em transições (Win+R, Enter).              |
| `SETUP_MODE_PIN`      | Pino do jumper de setup mode (pino→GND = não dispara). |
| `SETUP_MODE_ENABLED`  | Liga/desliga o setup mode. `false` no Beetle blindado. |

> Ao editar `AWARENESS_MESSAGE`/`AWARENESS_TITLE`, mantenha-se no conjunto
> de caracteres mapeado no ABNT2 (letras, dígitos, espaço, `- . , ' " ; : / ? ( ) $ =`).
> Caracteres fora desse conjunto (acentos, `[`, `]`, `@`, `*`, `+`, etc.)
> fazem a validação falhar e o **NumLock pisca** em vez de disparar — sinal
> visual (no teclado do host) de que o texto precisa ser ajustado.

Se o popup não aparecer de forma consistente, **aumente** `STARTUP_DELAY_MS`
e `RUN_DIALOG_DELAY_MS` — teclados virtuais variam de máquina para máquina.
Se caracteres do comando sumirem (texto corrompido no Run), **suba**
`PER_KEY_DELAY_MS` (ex.: 10–18) — 6 ms é rápido mas pode perder teclas em
VM/host lento.

---

## Fallback (se `mshta` estiver bloqueado)

Algumas políticas corporativas (AppLocker/WDAC) bloqueiam `mshta.exe`. Nesse
caso, edite `showAwarenessPopup()` no `.ino` para usar uma das alternativas:

**Alternativa 1 — PowerShell oculto** (console não aparece, mas é mais lento):

```cpp
// Substitui o bloco typeAbnt2(...) do mshta por:
typeAbnt2("powershell -NoProfile -w hidden -c \"(New-Object -ComObject WScript.Shell).Popup('");
typeAbnt2(AWARENESS_MESSAGE);
typeAbnt2("',0,'");
typeAbnt2(AWARENESS_TITLE);
typeAbnt2("',0x40)\"");
```

**Alternativa 2 — `msg` nativo** (sem título nem ícone, mais simples):

```cpp
// Substitui o bloco typeAbnt2(...) do mshta por:
typeAbnt2("msg * /TIME:30 ");
typeAbnt2(AWARENESS_MESSAGE);
```

Como o `msg` não aceita título nem ícone, o popup fica mais simples —
suficiente para a mensagem de conscientização.

---

## Roteiro curto de apresentação

1. **Antes:** pergunte à plateia "o que acontece se eu plugar este pendrive
   que achei no estacionamento?".
2. **Conecte** o Beetle e deixe o popup aparecer.
3. **Depois:** explique que o popup foi inofensivo, mas poderia ter sido o
   lançamento de um ataque real em segundos, sem nenhum clique da vítima.
4. **Mensagem final:** só conecte dispositivos USB de origem confiável.

### Checklist de limpeza

- [ ] Remover a placa do computador após a demo.
- [ ] Fechar a janela do popup e qualquer janela do Executar.
- [ ] Guardar o Beetle (ele dispara de novo se reconectado).
