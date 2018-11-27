#include <ArduinoJson.h>
#include <SoftwareSerial.h>
#include <Ethernet.h>
#include <MySQL_Connection.h>
#include <MySQL_Cursor.h>
#include <Dns.h>


const byte mac_addr[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED }; // MAC Address de la Shield Ethernet

const char hostname[] = "so7.infinitysrv.com"; // Dominio donde se aloja MySQL
const char username[] = "tapsisa1_arduino";    // Username de MySQL
const char password[] = "192837465!";          // Password de Mysql


const char insert_sql[] = "insert into tapsisa1_hidroflu.gasto(gast,id) values ('%s','%s')";  // cadena de formato SQL para realizar el INSERT

char compile_sql[80]  = {0};      // buffer para almacenar el 'SQL' ya construido

char *arduino_id   = nullptr;     // puntero para alamcenar el valor del campo 'gasto.id' en 'tapsisa1_hidroflu.gasto'
double gasto = 0.0f;  // Variable para almacenar el gasto a reportar

IPAddress         server_ip;                // IP a la que apunta el dominio de MySQL
EthernetClient    client;                   // Cliente HTTP
MySQL_Connection  conn((Client *)&client);  // Conexión a MySQL


const byte limit_input = 50;            // Limite de caracteres del flujo de entrada (Serial)
char serial_input[ limit_input + 1];    // Buffer para almacenar los datos del flujo de entrada (Serial)
byte serial_length = 0;                 // Cotador para determinar la longitud del flujo de entrada

#define SERIAL_PORT Serial    // Constante simbolica para determinar el puerto serial de etrada y salida

void setup() {

  
  SERIAL_PORT.begin(38400); // Inicialización del puerto serial 
  
  //while(!SERIAL_PORT){;} // Comentado por que no es necesario esperar la conexión del XBee
  
  delay(1000);  // Pequeña pausa


  // Inicialización del Shield Ethernet
  SERIAL_PORT.print(F("Ethernet: "));
  if( Ethernet.begin(mac_addr) == 0 )
  {
    SERIAL_PORT.println("NO");
  } else {
    SERIAL_PORT.println("OK");
  }

  // Busqueda del ip de host
  DNSClient *dns_client = new DNSClient();          // Inicialización de una instancia del DNS
  dns_client->begin(Ethernet.dnsServerIP());        // Inicilización del DNS
  dns_client->getHostByName(hostname, server_ip);   // Busqueda del IP del Host
  delete dns_client;                                // Se elimina la instancia para liberar memoria
  
  mysql_connect();    // Prueba de conexión a mysql
  
}

void loop() {

  serial_length = 0;    // Contador que sirve para guardar la longitud del flujo de entrada


  // Lee el flujo de entrada minetras haya datos en el buffer
  // y no se haya alcanzao el limite establecido por el usurario 
  
  for( ; serial_length < limit_input && SERIAL_PORT.available(); )
  {
    serial_input[ serial_length++ ] = SERIAL_PORT.read(); // Lee un caracter y lo alamacena en 'serial_input', depués aumenta serial_length
    delay(25);                                            // Una pequeña pausa
  }

  serial_input[ serial_length ] = '\0'; // Añade el caracter de fin de cadena a los datos de entrada
  
  // Si se recibió algun dato...
  if( serial_length > 0 ) {

    // Imprime el dato para fines de depuración
    SERIAL_PORT.print( ">> ");
    SERIAL_PORT.println( serial_input );
    
    // Analiza los datos recibidos, continua en caso de recibir los datos correctamente
    // Ademas extrae las variables 'gasto' y 'arduino_id'
    //
    // IMPORTANTE: Para solicitar que el coordinador envie los dato al servidor, el flujo de entrada debe estar en formato JSON y minúsculas
    // y debe contener las siguinte claves: "cmd", "gasto" y "uid". e.g.:
    //
    // {uid:"arduino-1",cmd:"psh-gasto",dat:"10.15"}
    //
    // DONDE:
    // "uid"    contendrá el id del arduino que envia los datos.
    // "cmd"    contendrá el valor "psh-gasto" indicando que se deven enviar los datos al servidor.
    // "dat"    contendrá el los litros gastados a reportar en punto flotante.
    
    if( parse_json_and_extract( serial_input ) )
    {

      // Construye el 'INSERT SQL' a partir de los datos recibidos
      build_insert_sql();
      

      // Envia la consulta al servidor
      if ( exec_insert_sql() )
      {
        SERIAL_PORT.println(F("Datos enviados correctamente"));
      } else {
        SERIAL_PORT.println(F("No se pudo enviar los datos"));
      }
    }
    
    
  }
  
}



/**
 * Realiza la conexión a MySQL
 * 
 *  @return bool true o false dependiendo de si se conectó correctament
 */

bool mysql_connect()
{
  SERIAL_PORT.print(F("Conexion a MySQL: "));
  if (conn.connect(server_ip, 3306, username, password)) {
    SERIAL_PORT.println("OK");
    return true;
  } else {
    SERIAL_PORT.println("NO");
    return false;
  }
}



/**
 * Construye la consulta que será enviada al servidor
 */

void build_insert_sql( )
{
  char strf[8] = {0};                                   // Bufer donde se alamcenará la variable gasto pero como string
  dtostrf( gasto, 0, 2, strf );                         // genera un string a partir de la variable gasto con 2 posiciones decimales
  sprintf( compile_sql, insert_sql, strf, arduino_id ); // contruye la consulta
  SERIAL_PORT.println( compile_sql );                   // imprime con modtivos de depuración
}



/**
 * Envia la consulta al servvidor (3 intentos)
 * 
 * @return bool true o false dependiendo de si logró conectar en menos de 3 intentos
 */

bool exec_insert_sql()
{
  // número de intento
  int tries = 3;

  // intenta 3 veces
  while( tries-- ) // Intenta 3 veces
  {

    // Intenta conectar
    if( mysql_connect() ) // Abre una nueva conexión a MySQL
    {
      // pequeña pausa
      delay(1000); 

      // inicializa un cursor
      MySQL_Cursor *cur_mem = new MySQL_Cursor(&conn);
    
      // SERIAL_PORT.println(compile_sql);
      
      // ejecuta la consulta
      cur_mem->execute(compile_sql);
      
      // elimina el cursor para liberar memoria
      delete cur_mem;

      return true;
    }
  }

  return false;
}



/**
 * Analiza gramáticamente la cadena proporcionada y extrae los datos 
 * 
 * @return bool true o false dependiendo de si se puedieron extraer todos los datos.
 */
boolean parse_json_and_extract( char *json )
{
  StaticJsonBuffer< limit_input + 10> jsonBuffer;

  JsonObject& root = jsonBuffer.parseObject(json);

  
  if (!root.success()) {
    SERIAL_PORT.println(F("ERROR DE SINTAXIS"));
    return false;
  }

  // Verifica si se ha enviado la orden de enviar (psh -> push)
  if( strcmp( root["cmd"], "psh-gasto") == 0 ){

    // Obtiene el valor del campo "gasto" y lo devuelve
    gasto = root["dat"].as<double>();

    // Obtiene el id del arduino
    arduino_id = root["uid"].as<char*>();

    // Devuelve true en caso de obtener el id del arduino
    return arduino_id != nullptr;
  }

  return false;
  
}
