#include <ArduinoJson.h>
#include <Ethernet.h>
#include <MySQL_Connection.h>
#include <MySQL_Cursor.h>
#include <Dns.h>

//#include "ArduinoJson-v5.13.3.h"


const byte mac_addr[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };

const char hostname[] = "so7.infinitysrv.com"; // change to your server's hostname/URL
const char user[] = "tapsisa1_arduino";               // MySQL user login username
const char password[] = "192837465!";         // MySQL user login password

const char arduino_id[] = "051-edj-bn1";        // campo gasto.id 
//const char raw_sql[] = "insert into tapsisa1_hidroflu.gasto(gast,id) values ('%s','%s')";
      

IPAddress server_ip;
EthernetClient client;
MySQL_Connection conn((Client *)&client);
//DNSClient dns_client;   // DNS instance


bool fetch_gasto_from_json( String &cmd, double &gasto )
{
  StaticJsonDocument<70> doc;
  
  
  DeserializationError error = deserializeJson(doc, cmd.c_str());
  
  if (error) {
    Serial.print(F("deserializeJson() fallido: "));
    Serial.println(error.c_str());
    return false;
  }

  // Obtiene el objeto raiz
  JsonObject root = doc.as<JsonObject>();

  // Verifica si se ha enviado la orden de insertar 
  if( strcmp( root["cmd"], "insert") == 0 ){

    // Obtiene el valor del campo "gasto" y lo devuelve
    gasto = root["gasto"].as<double>();

    // Confirma la ejecución exitosa
    return true;
  }
  
  
  return false;
}

void build_insert_sql( char *const insert_sql, double &gasto )
{
  char strf[8] = {0};
  dtostrf( gasto, 0, 2, strf );
  sprintf( insert_sql, "insert into tapsisa1_hidroflu.gasto(gast,id) values ('%s','%s')", strf, arduino_id );
}

bool exec_insert_sql(const char *const insert_sql)
{
  if (conn.connect(server_ip, 3306, user, password)) {
    delay(1000);

    Serial.println(F("mysql cursor"));

    // Initiate the query class instance
    MySQL_Cursor *cur_mem = new MySQL_Cursor(&conn);
  
    Serial.println(F("execute query."));
    // Execute the query
    cur_mem->execute(insert_sql);
    // Note: since there are no results, we do not need to read any data
    // Deleting the cursor also frees up memory used
  
    Serial.println(F("delete"));
    delete cur_mem;
    

  }
}


void setup() {
  Serial.begin(115200);
  while (!Serial); // wait for serial port to connect

  
  Serial.println(F("Connecting ethernet...."));
  Ethernet.begin(mac_addr);

  // Begin DNS lookup
  DNSClient *dns_client = new DNSClient();   // DNS instance
  dns_client->begin(Ethernet.dnsServerIP());
  dns_client->getHostByName(hostname, server_ip);
  delete dns_client;
  
  Serial.println(server_ip);
  // End DNS lookup
  Serial.println(F("Connecting..."));
  if (conn.connect(server_ip, 3306, user, password)) {
    Serial.println(F("success."));
  }
  else
    Serial.println(F("failed."));
  
}

void loop() {

  String input = "";
  double gasto = 0.0f;
  
  while (Serial.available()) {  
    input.concat( (char) Serial.read() );
    delay(25);
  }

  if(input.length() > 0) {
    Serial.println( "input: " + input );
    
    if( fetch_gasto_from_json( input, gasto ) )
    {
      char insert_sql[80] = {0};
      
      build_insert_sql( insert_sql, gasto );
      
      Serial.println(insert_sql);
      exec_insert_sql( insert_sql );
    }
    
    
  }

  

  
}
