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


const char insert_sql[] = "insert into tapsisa1_hidroflu.gasto(gast,id) values ('%s','%s')";  // sql para realizar el INSERT

char compile_sql[80]  = {0};
char *arduino_id   = nullptr;        // valor del campo 'gasto.id' en 'tapsisa1_hidroflu.gasto'

IPAddress         server_ip;                // IP a la que apunta el dominio de MySQL
EthernetClient    client;                   // Cliente HTTP
MySQL_Connection  conn((Client *)&client);  // Conexión a MySQL


const byte limit_input = 40;
char serial_input[ limit_input + 1];
byte serial_length = 0;

#define SERIAL_PORT Serial



void setup() {

  
  SERIAL_PORT.begin(38400);
  //while(!SERIAL_PORT){;}
  delay(1000);


  SERIAL_PORT.print(F("Ethernet: "));
  if( Ethernet.begin(mac_addr) == 0 )
  {
    SERIAL_PORT.println("NO");
  } else {
    SERIAL_PORT.println("OK");
  }

  // Begin DNS lookup
  DNSClient *dns_client = new DNSClient();   // DNS instance
  dns_client->begin(Ethernet.dnsServerIP());
  dns_client->getHostByName(hostname, server_ip);
  delete dns_client;
  
  mysql_connect();

  
}

void loop() {

  
  double gasto = 0.0f;

  serial_length = 0;
  
  for( ; serial_length < limit_input && SERIAL_PORT.available(); ++serial_length )
  {
    serial_input[ serial_length ] = SERIAL_PORT.read();
    delay(25);
  }

  SERIAL_PORT.flush();
  serial_input[ serial_length ] = '\0';
  

  if( serial_length > 0 ) {
    SERIAL_PORT.print( ">> ");
    SERIAL_PORT.println( serial_input );
    
    
    if( fetch_gasto_from_json( serial_input, gasto ) )
    {
      
      build_insert_sql( gasto );
      
      
      if ( exec_insert_sql() )
      {
        SERIAL_PORT.println(F("Datos enviados correctamente"));
      } else {
        SERIAL_PORT.println(F("No se pudo enviar los datos"));
      }
    }
    
    
  }
  
}

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

void build_insert_sql( double &gasto )
{
  char strf[8] = {0};
  dtostrf( gasto, 0, 2, strf );
  sprintf( compile_sql, insert_sql, strf, arduino_id );
  Serial.println( compile_sql );
}

bool exec_insert_sql()
{
  int tries = 3;

  while( tries-- ) // Intenta 3 veces
  {
    
    if( mysql_connect() ) // Abre una nueva conexión a MySQL
    {
      delay(1000);

      // Inicializa un cursor
      MySQL_Cursor *cur_mem = new MySQL_Cursor(&conn);
    
      //SERIAL_PORT.println(compile_sql);
      
      // Execute the query
      cur_mem->execute(compile_sql);
      
      // Elimina el cursor para liberar memoria
      delete cur_mem;

      return true;
    }
  }

  return false;
}

boolean fetch_gasto_from_json( char *json, double &gasto )
{
  StaticJsonBuffer< limit_input + 10> jsonBuffer;

  JsonObject& root = jsonBuffer.parseObject(json);

  
  if (!root.success()) {
    SERIAL_PORT.println(F("ERROR DE SINTAXIS"));
    return false;
  }

  // Verifica si se ha enviado la orden de insertar 
  if( strcmp( root["cmd"], "insert") == 0 ){

    // Obtiene el valor del campo "gasto" y lo devuelve
    gasto = root["gasto"].as<double>();

    // Obtiene el id del arduino
    arduino_id = root["uid"].as<char*>();

    // Devuelve true en caso de obtener el id del arduino
    return arduino_id != nullptr;
  }

  return false;
  
}
