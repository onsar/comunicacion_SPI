// master

/*
 * 
 * PROBLEMA: El esp12 recibe los datos por miso dependiendo del voltage de mosi
 * 
 */

/*
 * Descubrimiento de esclavos
 * ===============================
 * estado = 0 inicio de la comunicacion          03   15   15 
 * estado = 1 confirmacion esclavo preparado     03   15   15                                                    
 */

/*
 *Transmision de la configuracion de ESP a Arduino
 *================================================
 *
 *    Tx  Rx  Pr  estado 
 *-------------------------------
 *            15  espera
 *            06
 *  0x11  06  13  inicio
 *    r1  13  13  txOrder < txLeng
 *    r2  13  13  txOrder < txLeng 
 *    rn  13  13  txOrder < txLeng
 *    00  13  13  tx = 0x00
 *    su  13  14  comprobar 0x14/0x18   
 *  0x03  14  15  Reset     Maestro recibe si suma ok
 *                espera
 *
 */

/*
 * diagrama de estados del servidor
 * ================================
 * 
 * 
 * estado = 0 datos NO preparados. Esperar a recibir x06                02   15
 * 
 * estado = 0 inicio de la comunicacion       (inicio STX     02)       02   06   r1
 * estado = 2 recepción de registros          (siguente registro)       12   r1   r2
 *                                            (siguente registro)       12   r2   rn
 *                                            (Line Feed \n     )       12   rn   00
 *                                            (suma de registros)       12   00   suma                                        
 * estado = 3 comprabar la trama recibida     (Final  ETX     03)       03   suma 15(NACK)
 *
 * estado = 4 procesando y transmision de valores
 * 
 * estado = 5 Reset: Final del texto
 * 
 * estado = 6 espera entre comuniaciones
 * 
 * CAN 0x18    Cancel: Error en el procesados
 * 
*/
 

#define DEBUG 0

#define REGISTROS_MAX 14


#include <SPI.h>

// control de los esclavos
uint8_t spiPin[]={D0,D1,D2,D8};
bool confPendiente;
const int max_esclavos = 4;
int numero_esclavos;              // esclavos reales detectados
int posicion_esclavo;             // orden del esclavo seleccionado

// control de los registros
char registros_recibidos[REGISTROS_MAX];    // matriz para almacenar los registros recibidos
uint8_t registros_pendientes;               // registros pendientes de recibir desde el esclavo
uint8_t registros_suma;                     // suma de los registros recibidos para comprobar

//Control de la división de tiempos
uint32_t t_last_tx;               // tiempo de la ultima transmision de datos

// Cotrol del estado de la recepcion de datos
uint8_t estado;


// VARIABLES TRANSMISION

char confExclavo[]= "f1f1ff";

// FIN VARIABLES TRANSMISION

// FUNCIONES
void reset_seleccion_esclavo();
uint8_t readRegister(uint8_t b);
void transmision(String this_string);
void rcepcion_de_datos();


void setup() {
  Serial.begin(115200);
  spi_setup();
}

void loop() {
  spi_loop();
}

void spi_setup(){
  
  SPI.begin();
  SPI.setClockDivider(SPI_CLOCK_DIV8);  // 2 MHz

  // configuracion pines de selección (SS) a 5V
  for(int n= 0; n < max_esclavos; n++){
    pinMode(spiPin[n], OUTPUT);
  } 
  reset_seleccion_esclavo();  
  delay(1000);  
  
  
  // DETECTAR LOS ESCLAVOS CONECTADOS
  
  numero_esclavos = 0;
  
  for(int n= 0; n < max_esclavos; n++){  
    uint8_t registro_leido = 0x00; 
    reset_seleccion_esclavo();
    digitalWrite(spiPin[n], LOW); 
    if(DEBUG) {Serial.print("seleccion esclavo: "); Serial.println(n);}
    delay(50);

    registro_leido = readRegister(0x03);
    if(DEBUG) {Serial.print("inicio descubrimiento, 03 registro: "); Serial.println(registro_leido, HEX);}
    delay(10);
    registro_leido = readRegister(0x03);
    if(DEBUG) {Serial.print("descubrimiento, 03 registro: "); Serial.println(registro_leido, HEX);}
    if (registro_leido == 0x15) numero_esclavos = n+1;
    else break;
  }

  Serial.print("ESCALVOS DETECTADOS: ");  Serial.println(numero_esclavos);

  // CONFIGURACION DE LAS VARIABLES INICIALES
  posicion_esclavo = numero_esclavos; // Se iniciará con el esclavo 0
  confPendiente= true;               // Ejecutar una sola vez la configuracion
  t_last_tx=0;
  
}


void spi_loop(){
  
  // EJECUCION EN BASE A TIEMPOS
  uint32_t current_time= millis();  
  if (current_time < t_last_tx) t_last_tx=0;         // para el desbordamiento de millis()
  if (current_time - t_last_tx > 10000){             // inicio de la lectura del esclavo
    t_last_tx = current_time;

    // Un esclavo cada vez
    posicion_esclavo++;                              
    if ( posicion_esclavo > (numero_esclavos-1)) {posicion_esclavo=0;} 
    reset_seleccion_esclavo();
    digitalWrite(spiPin[posicion_esclavo], LOW); 

    // inicio de los estados de recepcion de datos
    
    estado = 0;

    for(int n=0; n<REGISTROS_MAX; n++){registros_recibidos[n] = 0x00;}      
    registros_pendientes = REGISTROS_MAX;
    registros_suma = 0;

    if(DEBUG){
      Serial.print(F("******inicio comunicaciones estado=0 sgs: "));
      Serial.println(millis() / 1000);
      Serial.print("freeHeap: "); Serial.println(ESP.getFreeHeap()); 
      Serial.print("esclavo: ");  Serial.println(posicion_esclavo);  
    } 
 

  // EJECUCION DE ESTADOS DE LECTURA
  while (estado != 6) {rcepcion_de_datos();}
  
  digitalWrite(spiPin[posicion_esclavo], HIGH);
  
 }// fin t_last_tx
}//fin spi_loop 



void reset_seleccion_esclavo(){
  for(int n= 0; n < max_esclavos; n++){
    digitalWrite(spiPin[n], HIGH); 
  }   
}

uint8_t readRegister(uint8_t b) { // b=byte a transmitir e= esclavo
  uint8_t bitRx = 0x00;
  delayMicroseconds(120);
  bitRx = SPI.transfer(b); // (unsigned int)
  if(DEBUG){Serial.print("bitTx: ");  Serial.print(b,HEX);} 
  if(DEBUG){Serial.print(" - bitRx: ");  Serial.println(bitRx,HEX);}  
  return (bitRx);
}

void transmision(String this_string){
  Serial.print("valor a transmitir: ");
  Serial.println(this_string);
}


void rcepcion_de_datos(){
  uint8_t registros_esclavo = REGISTROS_MAX;   // numero de registros del dispositivo slave (recibido)
  uint8_t registro_orden;                      // posicion del string
  uint8_t registro_leido=0x00;

  // Primero transmitir la configuracion. Solo una vez
  if(confPendiente == true){txConfiguracion();}

  switch (estado) {
    case 0 : //inicio de la comunicacion
      registro_leido = readRegister(0x02); 

      if(registro_leido == 0x06) estado = 2;
      else                       estado = 6;
 
      if(DEBUG) {Serial.print(registro_leido, HEX);Serial.print("  Estado 0 -> Estado "); Serial.println(estado);}
      break;
      
    case 2: //recepción de registros
      if(registros_pendientes > 0){  
        registro_leido = readRegister(0x12);
        registro_orden = registros_esclavo - registros_pendientes;
        registros_recibidos[registro_orden] = registro_leido;
        registros_suma = registros_suma + registro_leido;            
        registros_pendientes--;

        if(registro_leido == 0x00) estado = 3;
        else                       estado = 2;
        
        if(DEBUG){
          Serial.print("iniciada, registros_recibidos[]: "); Serial.print(registro_orden);
          Serial.print("  registro: "); Serial.println(registro_leido,HEX);
          Serial.print("registros_suma: "); Serial.println(registros_suma,HEX);
        }
      } 
      else {
        estado = 5;
        if(DEBUG) {
          Serial.print("Estado 2 -> Estado "); Serial.println(estado);
          Serial.println("superada la longitud maxima de la cadena");
        }
      }
      break;
      
    case 3: //comprabar la trama recibida
      registro_leido = readRegister(0x03);
      if(registro_leido == registros_suma){
        estado = 4;
        if(DEBUG) {
          Serial.print("Estado 3 -> Estado: "); Serial.print(estado);Serial.print("  trama correcta ");
          Serial.print("  registro_leido: ");Serial.println((registro_leido), HEX);     
        }      
      }
      else {
        estado=5;
        if(DEBUG) {
            Serial.print("Estado 3 -> Estado: "); Serial.print(estado); Serial.print("trama IN-correcta ");
            Serial.print("  registro_leido: ");Serial.println((registro_leido), HEX);     
        }
      } 
      break;
      
    case 4: //procesando y transmision de valores
      transmision(registros_recibidos);
      estado=6;
      if(DEBUG) {
        Serial.print("Estado 4 -> Estado: "); Serial.println(estado); 
        Serial.print("cadena: ");Serial.println(registros_recibidos);     
      }   
      break; 
      
    case 5:
      estado=6;
      registro_leido = readRegister(0x03);
      if(DEBUG) {Serial.print("Estado: "); Serial.println(estado);}
      break; 
      
  }//fin case
}// fin recepcion_de_datos

/*
 * DEFINICION DE LA TRANSMISION
 * confExclavo[] Cadena con la configuracion a transmitir
 * El esclavo debe tener tiempo para prepara el bit de respuesta
 * antes de transmitir la siguiente informacion
*/

void txConfiguracion(){ 
    if(DEBUG) Serial.println("****txConfiguracion");
    byte regLeidoTx;
    int8_t txOrder;
    uint8_t regsSumaTx;
    uint8_t txLeng;

    regLeidoTx = readRegister(0x11);
    
    txOrder = 0;
    regsSumaTx = 0;
    txLeng= sizeof(confExclavo)/sizeof(confExclavo[0]);

    if(DEBUG){Serial.println(confExclavo);}
    if(DEBUG){Serial.print("txLeng: ");Serial.println(txLeng);}
    
    while(txOrder < txLeng){ 
        if(DEBUG){Serial.print("txOrder: ");Serial.println(txOrder);}
        regLeidoTx = readRegister(confExclavo[txOrder]);
        regsSumaTx += confExclavo[txOrder];
        txOrder++;
    }

    if(DEBUG) {Serial.print("txOrder: ");Serial.println(txOrder);}
    
    // Para no transmitir una suma que sea un código de control
    if (regsSumaTx<32) regsSumaTx += 32; 
    regLeidoTx = readRegister(regsSumaTx);   

    if(DEBUG){
      Serial.print("regsSumaTx: ");Serial.println(regsSumaTx);
      delay(1);
    }
    
    regLeidoTx = readRegister(0x03);
    if(regLeidoTx==0x14) {
      confPendiente = false;
      Serial.println("Configuracion correcta");
    } 
    else Serial.println("Configuracion INcorrecta");
}
// FIN DE LA DEFINICION DE LA TRANSMISION
