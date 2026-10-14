# Auditoría de Seguridad: Uso de `millis()` y Plan de Corrección

**Proyecto:** ESPSomfy-RTS  
**Rama:** `fix/securing-millis-usage`  
**Fecha:** Octubre 2026  
**Ámbito:** Análisis estático y corrección de desbordamientos de temporizadores (`millis()` y `micros()`) en ESP32 / Arduino.

---

## 1. Resumen Ejecutivo y Base Teórica

### 1.1 El problema del desbordamiento (*Rollover*)
En plataformas Arduino y ESP32 (Xtensa / RISC-V), `millis()` retorna un entero sin signo de 32 bits (`uint32_t` o `unsigned long`):
* **Rango:** $0$ a $2^{32} - 1 = 4.294.967.295$ milisegundos.
* **Periodo de desbordamiento:** A los **~49,71 días** de funcionamiento ininterrumpido, el contador de hardware/FreeRTOS da la vuelta (*rollover*) y regresa a 0.
* De forma análoga, `micros()` cuenta microsegundos en 32 bits sin signo, produciendo un *rollover* cada **~71,58 minutos** ($4.294,96$ segundos).

### 1.2 Regla de Oro de la Aritmética Modular
En C/C++, la aritmética sobre enteros sin signo está definida como módulo $2^N$. Para variables `uint32_t` ($N=32$), la resta `(uint32_t)(ahora - inicio)` calcula de forma matemáticamente exacta el tiempo transcurrido, **incluso si ha ocurrido un rollover entre `inicio` y `ahora`**, siempre que el intervalo medido sea inferior a $2^{32}-1$ milisegundos (~49,7 días).

```
Ejemplo con rollover:
  inicio = 4.294.967.200U   (falta poco para el límite)
  ahora  = 100U             (ya hizo rollover)
  diferencia = (ahora - inicio) = 100U - 4.294.967.200U = 196U ms (¡Exacto!)
```

### 1.3 Causas Raíz de las Vulnerabilidades Encontradas
1. **Promoción a 64 bits (`uint64_t`)**: Al almacenar marcas de `millis()` en variables de 64 bits, la resta en 64 bits no aplica el módulo $2^{32}$. Tras el desbordamiento, la resta produce valores astronómicos ($\approx 1,84 \times 10^{19}$), corrompiendo la lógica.
2. **Marcas de tiempo futuras (`target = millis() + timeout`)**: Evaluar `millis() > target` falla si `target` desborda antes que `millis()`, o si `millis()` desborda mientras se esperaba a `target`.
3. **Suma antes de comparar (`millis() > start + timeout`)**: Si `start + timeout` desborda en 32 bits, la condición se cumple de forma prematura e inesperada.
4. **Tipos con signo (`int` o `long`)**: El tipo `int` en arquitectura de 32 bits desborda con signo a los **~24,85 días** ($2^{31}-1 = 2.147.483.647$ ms), provocando comportamiento indefinido (*Undefined Behavior*). En el caso de `micros()`, una variable `long` con signo desborda a negativo a los **35,79 minutos**.

---

## 2. Inventario Exhaustivo de Vulnerabilidades

| ID | Archivo | Líneas | Patrón Inseguro | Severidad | Impacto en el Sistema |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **VULN-01** | `src/Somfy.h` | 272-278 | `uint64_t moveStart, tiltStart, noSunStart, sunStart, windStart, windLast, noWindStart` | **CRÍTICA** | Rotura de la aritmética modular. Al día 49, la posición de las persianas se corrompe y se disparan falsas alarmas de sol/viento. |
| **VULN-02** | `src/Somfy.cpp` | 1053, 2201, 2565 | `const uint64_t curTime = millis();` | **CRÍTICA** | Extensión a 64 bits de `millis()`. `curTime - moveStart` produce $\approx 1,84 \times 10^{19}$ ms, sumándose a `msFrom0` (`int32_t`). |
| **VULN-03** | `src/MQTT.h` | 12 | `uint64_t lastConnect = 0;` | **CRÍTICA** | Almacenamiento en 64 bits del timestamp de conexión MQTT. |
| **VULN-04** | `src/MQTT.cpp` | 115 | `(this->lastConnect + 10000 > millis())` | **CRÍTICA** | **Deadlock permanente:** `lastConnect + 10000` en 64 bits supera `UINT32_MAX`. Tras rollover, `millis()` (32 bits) nunca alcanza ese valor y MQTT jamás reconecta. |
| **VULN-05** | `src/Utils.h` | 60 | `int rebootTime = 0;` en `rebootDelay_t` | **ALTA** | `int` con signo desborda a negativo a los **24,8 días**. Provoca reinicios erráticos o bloqueo del reinicio. |
| **VULN-06** | `src/SomfyController.ino` | 56, 89 | `if(rebootDelay.reboot && millis() > rebootDelay.rebootTime)` | **ALTA** | Comparación de fecha futura con variable `int`. Fallo de reinicio tras 24,8 días. |
| **VULN-07** | `src/Somfy.cpp` | 976, 1048 | `this->gpioRelease = millis() + (frame.repeats * 200);`<br>`if(millis() > this->gpioRelease)` | **ALTA** | **Riesgo Físico:** El relé se desactiva tras 1 ms (pérdida de orden) o se queda activado durante 49 días. Además causa saturación continua de CPU en reposo (millis() > this->gpioRelease = 0). |
| **VULN-08** | `src/Somfy.cpp` | 2066 | `if(this->lastFrame.await > 0 && (millis() > this->lastFrame.await))` | **ALTA** | Bloqueo de 49 días en la espera de tramas intermedias si el tiempo límite cruza el rollover. |
| **VULN-09** | `src/Somfy.cpp` | 5168 | `if(tx_queue.length > 0 && millis() > tx_queue.delay_time && ...)` | **ALTA** | Congelación de la cola de transmisión RF CC1101 durante 49 días tras el rollover. |
| **VULN-10** | `src/Network.cpp` | 130 | `if(millis() > SSID_SCAN_INTERVAL + this->lastWifiScan)` | **MEDIA** | La suma previa desborda; genera ráfagas descontroladas de escaneos Wi-Fi. |
| **VULN-11** | `src/Network.cpp` | 469 | `(!this->connected() && millis() > this->disconnectTime + CONNECT_TIMEOUT)` | **MEDIA** | La suma previa desborda; abre SoftAP antes de tiempo o nunca lo abre. |
| **VULN-12** | `src/Network.cpp` | 535 | `if(this->_connecting && millis() > this->connectStart + CONNECT_TIMEOUT)` | **MEDIA** | Timeout de conexión Wi-Fi inconsistente ante rollover. |
| **VULN-13** | `src/GitOTA.cpp` | 276-277 | `millis() > net.connectTime + 60000`<br>`this->lastCheck + 86400000 < millis()` | **ALTA** | `lastCheck + 86400000` (24 h) desborda en el día 48,7. Lanza peticiones HTTP de comprobación a GitHub en bucle infinito cada milisegundo. |
| **VULN-14** | `src/SSDP.cpp` | 438 | `(elapsed * 5) > (this->_interval * 1000)` | **MEDIA** | Si `elapsed > 858.993.459` ms (~10 días de inactividad), `elapsed * 5` desborda en 32 bits, impidiendo la renovación periódica SSDP. |
| **VULN-15** | `src/SSDP.cpp` | 642 | `if(q->sendTime < millis())` | **MEDIA** | Inseguro ante rollover en cola de respuestas diferidas SSDP. |
| **VULN-16** | `src/Somfy.cpp` | 4456 | `const long time = micros();` | **MEDIA** | `long` con signo desborda a valores negativos a los **35,8 minutos**, corrompiendo la medición de anchos de pulso RF en interrupción. |

---

## 3. Casos de Estudio Numéricos

### Caso 1: El Deadlock Eterno en MQTT
* **Configuración:** `lastConnect` declarado como `uint64_t`, comprobación `(lastConnect + 10000 > millis())`.
* **Minuto 49,70:** Faltan 7 segundos para el límite de 32 bits:
  $$\text{millis}() = 4.294.960.000\text{ ms}$$
* Se registra la conexión:
  $$\text{lastConnect} = 4.294.960.000\text{ULL}$$
* Se calcula el límite en 64 bits:
  $$\text{lastConnect} + 10.000 = \mathbf{4.294.970.000}\text{ULL} \quad (> 4.294.967.295 \equiv \text{UINT32\_MAX})$$
* **Rollover:** Pasan 8 segundos; `millis()` hace rollover y marca $1.000\text{ ms}$.
* **Evaluación:**
  $$4.294.970.000\text{ULL} > 1.000\text{ULL} \implies \textbf{TRUE}$$
* **Resultado:** Como `millis()` jamás devolverá un número mayor que $4.294.967.295$, la desigualdad se mantiene **TRUE permanentemente**. El cliente MQTT nunca vuelve a intentar conectar hasta un reinicio físico por corte de luz.

### Caso 2: El Peligro Físico de `gpioRelease`
* **Configuración:** Salidas a relé para motores de persiana (`radio_proto::GP_Remote`).
* **Activación a los 49,71 días:**
  $$\text{millis}() = 4.294.967.000\text{ ms}$$
  $$\text{Duración pulso: } 200\text{ ms} \longrightarrow \text{gpioRelease} = 4.294.967.200\text{ ms}$$
* El relé se activa (`digitalWrite(..., HIGH)`).
* Pasan 205 ms; `millis()` hace rollover y pasa a valer $5\text{ ms}$.
* El bucle evalúa si apaga el relé:
  $$\text{millis}() > \text{gpioRelease} \longrightarrow 5 > 4.294.967.200 \implies \textbf{FALSO}$$
* **Resultado:** El relé se queda **conduciendo corriente de forma ininterrumpida durante 49,71 días**. Peligro real de quemar el motor tubular de la persiana o la bobina del relé.

---

## 4. Patrones de Diseño Correctos Implementados

### Patrón A: Intervalo Transcurrido (Para tareas periódicas)
```cpp
// CORRECTO: Inmune a rollover por la propiedad modular binaria
if (millis() - lastTime >= INTERVAL) {
    lastTime = millis();
    ...
}
```

### Patrón B: Diferencia Signada (Para marcas límite o Deadlines)
Equivalente a la macro `time_after(a, b)` del kernel de Linux. Permite medir intervalos de hasta $2^{31}-1$ ms (~24,8 días):
```cpp
// CORRECTO: Comprueba si 'now' ha alcanzado o superado 'deadline'
if ((int32_t)(millis() - deadline) >= 0) {
    ...
}
```

### Patrón C: Encapsulación de Temporizadores (`rebootDelay_t`)
En [Utils.h](file:///home/jcrespoc/Documents/PlatformIO/Projects/bb/cc/src/Utils.h#L58-L70):
```cpp
struct rebootDelay_t {
  bool reboot = false;
  uint32_t rebootTime = 0;
  bool closed = false;

  void trigger(uint32_t delayMs = 500) {
    reboot = true;
    rebootTime = millis() + delayMs;
  }
  bool expired() const {
    return reboot && ((int32_t)(millis() - rebootTime) >= 0);
  }
};
```

---

## 5. Plan de Corrección y Fases Ejecutadas

### Fase 1: Unificación de Tipos en Headers
* `src/Somfy.h`: Tipos de `moveStart`, `tiltStart`, `noSunStart`, `sunStart`, `windStart`, `windLast`, `noWindStart` cambiados de `uint64_t` a `uint32_t`.
* `src/MQTT.h`: `uint64_t lastConnect` cambiado a `uint32_t lastConnect`.
* `src/Utils.h`: `int rebootTime` corregido a `uint32_t rebootTime` y creación de métodos `trigger()` y `expired()`.

### Fase 2: Motor Somfy y Transceptor RF
* `src/Somfy.cpp`:
  - `curTime` unificado a `const uint32_t curTime = millis();` en `checkMovement()`, `processFrame()` y `processInternalCommand()`.
  - `gpioRelease`: Sustituido por `if(this->gpioRelease != 0 && (int32_t)(millis() - this->gpioRelease) >= 0)`.
  - `lastFrame.await`: Sustituido por `if(this->lastFrame.await > 0 && (int32_t)(millis() - this->lastFrame.await) >= 0)`.
  - `tx_queue.delay_time`: Sustituido por `if(tx_queue.length > 0 && (int32_t)(millis() - tx_queue.delay_time) >= 0 && ...)`.
  - `Transceiver::handleReceive()`: `const long time = micros();` corregido a `const unsigned long time = micros();`.

### Fase 3: Capa de Red, OTA y MQTT
* `src/Network.cpp`:
  - L130: `millis() - this->lastWifiScan >= SSID_SCAN_INTERVAL`.
  - L469: `millis() - this->disconnectTime >= CONNECT_TIMEOUT`.
  - L535: `millis() - this->connectStart >= CONNECT_TIMEOUT`.
* `src/GitOTA.cpp`:
  - L276: `millis() - net.connectTime >= 60*1000UL`.
  - L277: `this->lastCheck == 0 || millis() - this->lastCheck >= 24*60*60*1000UL`.
* `src/MQTT.cpp`:
  - L126: `millis() - this->lastConnect < 10000`.
  - L131: Actualización de `this->lastConnect = millis();` en el intento de conexión para limitar la tasa de reintento en caso de fallo del broker.

### Fase 4: Integración del Reinicio Seguro
* `src/SomfyController.ino`: Uso de `if(rebootDelay.expired())` al inicio y final del bucle principal.
* `src/Web.cpp`: Reemplazo de todas las asignaciones directas por llamadas a `rebootDelay.trigger(500)` o `rebootDelay.trigger(1000)`.
* `src/GitOTA.cpp`: Sustitución por `rebootDelay.trigger(500)`.

### Fase 5: Protocolo SSDP
* `src/SSDP.cpp`:
  - L438: Simplificación algebraica segura `elapsed >= (this->_interval * 200UL)` para anular desbordamiento por multiplicación.
  - L642: Comparación segura de cola con diferencia signada: `(int32_t)(millis() - q->sendTime) >= 0`.

---

## 6. Verificación y Auditoría Final
Tras completar las cinco fases, se ejecutó una búsqueda global de `millis()` y `micros()` en todo el árbol de código fuente:
* No queda ninguna resta de 64 bits sobre variables de `millis()`.
* No queda ninguna suma antes de comparación (`millis() > a + b`).
* No queda ninguna variable de marca de tiempo declarada con tipo entero con signo (`int` o `long`).
* Todas las comparaciones de fechas futuras utilizan la diferencia signada de 32 bits `(int32_t)(ahora - deadline) >= 0`.
* Todas las tareas recurrentes utilizan la resta modular sin signo `ahora - inicio >= intervalo`.

El sistema es ahora completamente tolerante y estable frente a la ejecución ininterrumpida por tiempo indefinido.

