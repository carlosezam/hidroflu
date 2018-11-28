/*
 * Librerias usadas en el sketch
 */
#include <SPI.h>
#include <Wire.h>                 // Librería para el control del I2C.
#include <LiquidCrystal_I2C.h>    // Librería para el control del LCD por medio de I2C.
#include <RTClib.h>               // Librería para el control del RTC DS1307.


/*
 * Instancias de los periféricos
 */
RTC_DS3231 rtc;                     // Objeto rtc (Pila de reloj).
LiquidCrystal_I2C LCD(0X3F, 20, 4); // Objeto LCD. (Dirección, No. de Columnas, No. de Filas).


/*
 * Pines de los sensores y actuadores
 */
const int controlRele = 9;    // Pin del relevador. 
const int YFS201      = 2;    // Pin del sensor YF-S201. //Caudal 1-30 Litros/minuto.

/*
 * Variables usadas en el calculo del caudal
 */
const int intervalo = 2500;   // Medidor de intervalo.
const float factorK = 7.5;    // Factor K de conversión entre la frecuencia y el caudal. //Precisiòn del -/+ 10%.

volatile int pulsoContador;   // Contador de pulsos.
float frecuencia;             // Frecuencia del sensor YFS201
unsigned long t = 0;                   //Tiempo.

float volumen = 0;            // Volumen. Almacena la cantidad de agua acumulada. 
float flow_Lmin;


const char arduino_id[] = "051-edj-bn1";        // valor del campo 'gasto.id' en 'tapsisa1_hidroflu.gasto'

unsigned long ultimo_muestreo = 0;
unsigned long tiempo_muestreo_gasto = 60;  // Tiempo en minutos entre cada envio de información al servidor

//float tope_gasto_agua = 50;      // Cantidad de agua en Litros antes de hacer el corte del suministro


/*
 * Función atada a una interrupcion para contar los pulsos del sensor YFS201
 */
void contadorPulsos() 
{
  pulsoContador++; //Contador de pulsos del sensor.
}


float obtenerFrecuencia() 
{
 pulsoContador = 0;
 //interrupts();
 delay(intervalo);
 //noInterrupts();
 return (float)pulsoContador * 1000 / intervalo;
}

void sumaVolumen(float dV)
{

  volumen += dV /60 * (millis() - t) / 1000.0;
  t = millis();
}

void setup()
{
  Serial.begin(38400); //Inicialización del puerto.
  Serial.println("Bootstraping...");

  pinMode(13, OUTPUT);
  digitalWrite(13, HIGH);
  
  Wire.begin(); //Inicialización de I2C.
  LCD.init(); //Inicialización del LCD.
  
  if(! rtc.begin()) //Verifica que existe comunicación con el módulo RTC DS1307.
  {
    LCD.setCursor(0, 0);
    LCD.print("Módulo RTC DS1307 No Encontrado."); //Muestra en la LCD el mensaje en " ".
    Serial.println("Módulo RTC DS1307 No Encontrado."); //Muestra en monitor el mensaje en " ".
    while (1); //Bucle que detiene el programa hasta tener una comunicación con el módulo RTC DS3231.
  }

  
  if (rtc.lostPower()) {
  rtc.adjust(DateTime(__DATE__,__TIME__)); //Toma de referencia la fecha, hora y día del ordenador. //Solo se carga una vez.
  Serial.println("lostpower");
  }
  LCD.clear(); //Limpieza del cache del LCD.
  LCD.backlight(); //Encendido de iluminación del LCD.
  attachInterrupt(digitalPinToInterrupt(YFS201), contadorPulsos,RISING);
  
  t = millis();

  ultimo_muestreo = millis();
  
}

unsigned long m = 0;

void loop()
{
  
 //Serial.println("=================================================");
 
 frecuencia = obtenerFrecuencia(); //Obtenciòn de la frecuencia (Hz).
 flow_Lmin = frecuencia / factorK;
 
  sumaVolumen(flow_Lmin);
  //sumaVolumen(60);
  //Serial.print("volumen: "); Serial.println(volumen);
  lcd_print_info();
 
  m = (millis() - ultimo_muestreo) / 60000;
  
  //Serial.println( s );
  if ( m >= tiempo_muestreo_gasto ) {
    
    psh_gasto();
    
    
    ultimo_muestreo = millis();

    /*if( volumen > tope_gasto_agua ){
      Serial.print("aviso");
    }*/
    
    volumen = 0;
  }

}

void psh_gasto()
{
  DateTime now = rtc.now();  // Instancia del reloj

  
  char buff[50];    // buffer para almacenar los strings que se muestran en el LCD
  char fstr[7];     // buffer para almacenar los numeros ya formateados

  // Crea un string formateada de la variable 'volumen'
  dtostrf(volumen,5,2,fstr);
  
  sprintf( buff,                            // buffer donde se va a guardar el string
    "{cmd:\"psh-gasto\",dat:%s,uid:\"%s\"}", fstr, arduino_id // obtiene la hora, minutos y segundos
  );

   Serial.print(buff);
}

void lcd_print_info()
{
  
  DateTime now = rtc.now();  // Instancia del reloj

  
  char buff[21];    // buffer para almacenar los strings que se muestran en el LCD
  char fstr[6];     // buffer para almacenar los numeros ya formateados

  LCD.clear(); //Limpieza del cache del LCD.

  // 
  // Crea un string con la fecha y hora actuales 
  //
  sprintf( buff,                            // buffer donde se va a guardar el string
    "%02d/%02d/%02d    %02d:%02d:%02d",     // Format del string: "DD/MM/AA    HH/MM/SS"
    now.day(), now.month(), now.year()%100, // obtiene el dia, mes y los dos ultimos digitos del año
    now.hour(), now.minute(), now.second()  // obtiene la hora, minutos y segundos
  ); 
  LCD.setCursor(0, 0);                      // Posiciona el cursor en el LCD
  LCD.print(buff);                          // Imprime el string

  unsigned long tempo = (tiempo_muestreo_gasto - m) > 0 ? (tiempo_muestreo_gasto - m) : 0;
  sprintf( buff, "Envio en: %d min.", tempo );  // Crea el string a mostrar en el LCD
  LCD.setCursor(0, 1);                        // Posiciona el cursor en el LCD
  LCD.print(buff);                            // Imprime el string

  dtostrf(flow_Lmin,5,2,fstr);                // Da formato a la variable flow_Lmin
  sprintf( buff, "Caudal:  %s L/min", fstr);  // Crea el string a mostrar en el LCD
  LCD.setCursor(0, 2);                        // Posiciona el cursor en el LCD
  LCD.print(buff);                            // Imprime el string

  
  dtostrf(volumen,5,2,fstr);                  // Da formato a la variable volumen
  sprintf( buff, "Volumen: %s L.", fstr);     // Crea el string a mostrar en el LCD
  LCD.setCursor(0, 3);                        // Posiciona el cursor en el LCD
  LCD.print(buff);                            // Imprime el string

}
