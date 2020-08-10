# Comunicación mediante SPI entre arduino y ESP12

Este ejemplo ilustra como se pueden comunicar cadenas de caracteres (stings) entre un arduino nano y un SP12

## ESP12
El ESP12 esta configurado con varias salidas de contorl para trabajar con varios arduinos. Es necesario tener cuidado de elegir bien estas señales y conectarlas al SS del arduino

## Arduino
- Para el caso practico se ha elegido un arduino nano pero se puede trabajar con cualquier otro arduino.
- La cadena a tranmitir entre el arduino y el ESP tiene un máximo de 14 carácteres para que el software sea ligero pero esta configuración se puede cambiar en el código de una forma sencilla 
