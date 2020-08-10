// Slave

/*
 * diagrama de estados del servidor
 * ================================
 * 
 * 
 * estado = 0 datos NO preparados                                       02   15
 * 
 * estado = 0 inicio de la comunicacion       (inicio STX     02)       02   06   r1
 * 
 * estado = 2 recepción de registros          (siguente registro)       12   r1   r2
 *                                            (siguente registro)       12   r2   rn
 *                                            (Line Feed \n   0A)       12   rn   00
 *                                            (suma de registros)       12   00   suma
 *                                            
 * estado = 3 comprabar la trama recibida     (Final  ETX     03)       03   suma 15(NACK)
 *
 * estado = 4 procesando y transmision de valores
 * 
 * estado = 5 Reset: Final del texto
 * 
 * estado = 6 espera entre comuniaciones
 * 
 * CAN 0x18    Cancel: Error en el procesados
*/
 
#define DEBUG 0

#include <SPI.h>

char datos_matrix[]= "potencia:22";
bool dataReady; 
int8_t data_leng;
int8_t datos_pendientes;
uint8_t registros_suma;
uint32_t t_last_tx; // ultima transmision de datos


void setup (void){
  
  Serial.begin(115200);

  pinMode(MISO, OUTPUT);

  // turn on SPI in slave mode
  SPCR |= bit(SPE);

  // turn on interrupts
  SPCR |= bit(SPIE);
  
  SPDR = 0x15;
  datos_pendientes = 0;
  dataReady=false;  
  
}  // end of setup


void loop (void){
  uint32_t current_time= millis();
  if (current_time < t_last_tx) t_last_tx=0;         // para el desbordamiento de millis()
  if (current_time - t_last_tx > 20000){             // inicio de la lectura del esclavo
    t_last_tx = current_time;
    data_leng = sizeof(datos_matrix) / sizeof(datos_matrix[0]);
    datos_pendientes = data_leng;
    dataReady=true;
    registros_suma  = sumar_registros();
    SPDR=0x06; //datos listos para Tx
  }

}  // end of loop

uint8_t sumar_registros(){
  uint8_t suma = 0;
  for(uint8_t n=0; n < data_leng; n++){
    suma = suma + datos_matrix[n];
  }
  if(DEBUG) {Serial.print("suma: "); Serial.println(suma,HEX);}
  return suma;
}


ISR (SPI_STC_vect){

  byte c = SPDR;
  switch (c){
    
    case(0x02): // inicio
      if(dataReady==false) {
        SPDR = 0x15;
        if(DEBUG) {
          Serial.print("case_0x02_bitRx = "); Serial.print(c,HEX);
          Serial.println(" *** datos no preparados  ");
        }     
      }
      else if(datos_pendientes > 0) {
        SPDR = datos_matrix[data_leng - datos_pendientes];  // 4-4=0, 4-3=1, 4-2=2, 4-1=3,
        datos_pendientes = datos_pendientes-1;
        if(DEBUG) {
          Serial.print("bit recibido =  "); Serial.println(c,HEX);
          Serial.print("datos_pendientes =  "); Serial.println(datos_pendientes);
        }
      }

      else{
        SPDR = 0x18;
        datos_pendientes = 0;
        if(DEBUG) {
          Serial.print("bit recibido =  "); Serial.print(c,HEX);
          Serial.println(" Error en el prcesado");
        }
      }
      break;
      
    case(0x12):
      
      if(datos_pendientes > 0) {
        SPDR = datos_matrix[data_leng - datos_pendientes];  // 4-4=0, 4-3=1, 4-2=2, 4-1=3,
        datos_pendientes = datos_pendientes-1;
        if(DEBUG) {
          Serial.print("bit recibido =  "); Serial.println(c,HEX);
          Serial.print("datos_pendientes =  "); Serial.println(datos_pendientes);
        }
      }
      else if(datos_pendientes == 0){
        SPDR = registros_suma;
        datos_pendientes = -1;
        if(DEBUG) {
          Serial.print("bit recibido =  "); Serial.println(c,HEX);
          Serial.print("datos_pendientes:  ");Serial.println(datos_pendientes);
        }
      }
      else{
        SPDR = 0x15;
        datos_pendientes = -2;
        if(DEBUG) {
          Serial.print("bit recibido =  "); Serial.println(c,HEX);
          Serial.print("datos_pendientes:  ");Serial.println(datos_pendientes);
        }
      }
      break; 

    case(0x03): // reset  
      SPDR = 0x15;
      dataReady=false;
      datos_pendientes = 0;
      if(DEBUG) {
        Serial.print("case_0x03_bitRx = "); Serial.print(c,HEX);
        Serial.println(" ***reset  ");
      }
      break;

  } // fin swich
} // fin (ISR) SPI_STC_vect


  
