/* ###################################################################
**     Filename    : main.c
**     Project     : Roteiro1_IoT
**     Processor   : MKL25Z128VLK4
**     Version     : Driver 01.01
**     Compiler    : GNU C Compiler
**     Date/Time   : 2019-03-14, 09:41, # CodeGen: 0
**     Abstract    :
**         Main module.
**         This module contains user's application code.
**     Settings    :
**     Contents    :
**         No public methods
**
** ###################################################################*/
/*!
** @file main.c
** @version 01.01
** @brief
**         Main module.
**         This module contains user's application code.
*/         
/*!
**  @addtogroup main_module main module documentation
**  @{
*/         
/* MODULE main */


/* Including needed modules to compile this module/procedure */
#include "Cpu.h"
#include "Events.h"
#include "Uart0_OpenSDA.h"
#include "ASerialLdd1.h"
#include "Uart2_ESP.h"
#include "ASerialLdd2.h"
#include "UTIL1.h"
#include "MCUC1.h"
#include "TempSensor.h"
#include "AdcLdd1.h"
#include "RTC1.h"
#include "TU1.h"
#include "PWM1.h"
#include "PwmLdd1.h"
#include "Bit1.h"
#include "BitIoLdd1.h"
#include "Bit2.h"
#include "BitIoLdd2.h"
/* Including shared modules, which are used for whole project */
#include "PE_Types.h"
#include "PE_Error.h"
#include "PE_Const.h"
#include "IO_Map.h"

/* User includes (#include below this line is not maintained by Processor Expert) */

volatile char ESP_receive = 0;
volatile char OPEN_SDA_receive = 0;
volatile char TimeStatus = 0;
volatile char PWM = 0;

/**
 * @brief
 * Envia string apontada por \a s ao terminal via o OpenSDA da placa FRDMKL25.
 */
void send_strTerminal(char *s){
    while(*s){
      while(Uart0_OpenSDA_SendChar(*s));
      s++;
    }    
    while(Uart0_OpenSDA_SendChar(0x0d));
    while(Uart0_OpenSDA_SendChar(0x0a));    
}

/**
 * @brief
 * Envia string apontada por \a s ao ESP.
 */
void send_strESP(char *s){
    while(*s){
      while(Uart2_ESP_SendChar(*s));
      s++;
    }
    while(Uart2_ESP_SendChar(0x0d));
    while(Uart2_ESP_SendChar(0x0a));   
}

/**
 * @brief
 * Aguarda recebimento completo de string enviada pelo ESP e a guarda na memoria apontada por \a s.
 */
void receive_strfromESP(char *s){
  char c = 0;
  extern volatile char ESP_receive;
  
  while(c != 0x0a){
    if(ESP_receive){
      ESP_receive = 0;
      if(Uart2_ESP_RecvChar((Uart2_ESP_TComData *)&c) == ERR_OK){
        *s = c;
        s++;
      }
    }
  }
  s -= 2;
  *s = '\0';  
}

/**
 * @brief
 * Converte e seta o valor em porcentagem dado por \a dc na razao adequada do duty cycle do PWM do motor.
 */
void Duty_Cycle(char dc){
  uint16_t n;
  
  dc = 100 - dc;
  n = dc*655.35;
  PWM1_SetRatio16(n);
}


/*lint -save  -e970 Disable MISRA rule (6.3) checking. */
//! Funcao que faz a comunicacao entre o controlador e o broker e ajusta os parametros do motor.
/*!
 * A funcao main realiza as conexoes a rede local e ao broker, alem de assinar todos os topicos necessarios.
 * Nela eh feito a leitura da temperatura e publicacao a cada 1 segundo, o recebimento das demais 
 * mensagens e o tratamento das mesmas, setando adequadamente os parametros do motor.
 * Existem 3 mensagens esperadas para o topico EA076/grupoM2/mode:
 *   On - O motor fica ligado.
 *   Off - O motor fica desligado.
 *   Auto - O motor eh ligado se a temperatura medida eh maior que o threshold.
 * Ja o topico EA076/grupoM2/dir determina a direcao do motor. Mensagens aceitas:
 *   Vent
 *   Exaust
 * O topico EA076/grupoM2/power determina a porcentagem do duty cycle do PWM do motor e o topico
 * EA076/grupoM2/limiar determina o threshold de temperatura com uma casa decimal.
 */
int main(void)
/*lint -restore Enable MISRA rule (6.3) checking. */
{
  /* Write your local variable definition here */
  char c, bt[128], bc[128], br[128], btemp[128];
  uint8_t flag_rcv_msg = 0, ibr = 0, ibc, ibt = 0, duty_cicle = 0, i, dir, modo = 2;
  uint16_t ADC_TempValue, TempValue = 0, threshold = 0, minuto = 0, hora = 0, segundo = 0;
  LDD_RTC_TTime CurrentTime;
  
  /*** Processor Expert internal initialization. DON'T REMOVE THIS CODE!!! ***/
  PE_low_level_init();
  /*** End of Processor Expert internal initialization.                    ***/
  
  //Conectantdo na rede wifi:
  UTIL1_strcpy((uint8_t *)bt,128,(const unsigned char *)"CONNWIFI \"EA076R\",\"FRDMKL25\"");
  do{
    send_strESP(bt);
    receive_strfromESP(br);    
  }
  while(strcmp((const char *)br,(const char *)"CONNECT WIFI")); //Quando conectar ( = 0) sai do loop.
  send_strTerminal(br); //Replica resposta no terminal 
  
  //GETMAC, adiciona a string, e conecta-se ao broker
  UTIL1_strcpy((uint8_t *)bt,128,(const unsigned char *)"GETMAC");
  //send_strTerminal(bt);
  send_strESP(bt);                  //envia comando
  receive_strfromESP(br);           //aguarda resposta
  //send_strTerminal(br);           //replica resposta do GETMAC
  UTIL1_strcpy((uint8_t *)bt,128,(const unsigned char *)"CONNMQTT \"192.168.1.111\",1883,\"");
  UTIL1_strcat((uint8_t *)bt, 128,(uint8_t *)br);
  UTIL1_strcat((uint8_t *)bt, 128,(const unsigned char *)"\",\"aluno\",\"UNICAMP\"");
  do{
    send_strESP(bt);
    receive_strfromESP(br);    
  }
  while(strcmp(br, "CONNECT MQTT")); //Quando conectar MQTT ( = 0) sai do loop.
  send_strTerminal(br); //Replica resposta no terminal
    
  //Assina os topicos necessarios
  UTIL1_strcpy((uint8_t *)bt,128,(const unsigned char *)"SUBSCRIBE \"EA076/grupoM2/celular\"");
  do{
    send_strESP(bt); //envia comando
    receive_strfromESP(br); //aguarda resposta
  }while(strcmp(br, "OK SUBSCRIBE")); //Quando ok ( = 0) sai do loop.

//  UTIL1_strcpy((uint8_t *)bt,128,(const unsigned char *)"SUBSCRIBE \"EA076/grupoM2/temp\"");
//  do{
//    send_strESP(bt); //envia comando
//    receive_strfromESP(br); //aguarda resposta
//  }while(strcmp(br, "OK SUBSCRIBE")); //Quando ok ( = 0) sai do loop.
    
  UTIL1_strcpy((uint8_t *)bt,128,(const unsigned char *)"SUBSCRIBE \"EA076/grupoM2/dir\"");
  do{
    send_strESP(bt); //envia comando
    receive_strfromESP(br); //aguarda resposta
  }while(strcmp(br, "OK SUBSCRIBE")); //Quando ok ( = 0) sai do loop.
  
  UTIL1_strcpy((uint8_t *)bt,128,(const unsigned char *)"SUBSCRIBE \"EA076/grupoM2/power\"");
  do{
    send_strESP(bt); //envia comando
    receive_strfromESP(br); //aguarda resposta
  }while(strcmp(br, "OK SUBSCRIBE")); //Quando ok ( = 0) sai do loop.
  
  UTIL1_strcpy((uint8_t *)bt,128,(const unsigned char *)"SUBSCRIBE \"EA076/grupoM2/mode\"");
  do{
    send_strESP(bt); //envia comando
    receive_strfromESP(br); //aguarda resposta
  }while(strcmp(br, "OK SUBSCRIBE")); //Quando ok ( = 0) sai do loop.
  
  UTIL1_strcpy((uint8_t *)bt,128,(const unsigned char *)"SUBSCRIBE \"EA076/grupoM2/limiar\"");
  do{
    send_strESP(bt); //envia comando
    receive_strfromESP(br); //aguarda resposta
  }while(strcmp(br, "OK SUBSCRIBE")); //Quando ok ( = 0) sai do loop.
  send_strTerminal(br); //Replica resposta no terminal
 
    
    
  /* For example: for(;;) { } */
  for(;;){
    
    if(ESP_receive){
      ESP_receive = 0;      
      if(Uart2_ESP_RecvChar((Uart2_ESP_TComData *)&c) == ERR_OK){
        br[ibr] = c;
        ibr++;
        if(c == 0x0a) flag_rcv_msg = 1;
      }        
    }
    
    if(flag_rcv_msg){
      flag_rcv_msg = 0;
      ibr -= 2;
      br[ibr] = '\0';
      ibr = 0;
      UTIL1_strcpy((uint8_t *)bc,27,(uint8_t *)br);                      //copia 27 primeiros caracteres de br
      
      if(!strcmp((const char *)bc,(const char *)"MESSAGE [EA076/grupoM2/dir")){
        ibc = UTIL1_strFind((uint8_t *)br,(uint8_t *)"]");
        UTIL1_strcpy((uint8_t *)bc,128,(uint8_t *)br+ibc+3);          //copia os caracteres de br localizados apos '],['
        bc[strlen(bc)-1] = '\0';                              //apaga o ultimo ']'
        send_strTerminal(bc);  
        if(!strcmp((const char *)bc,(const char *)"Vent"))
          dir = 1;
        else if(!strcmp((const char *)bc,(const char *)"Exaust"))
          dir = 0;
      }
      
      else if(!strcmp((const char *)bc,(const char *)"MESSAGE [EA076/grupoM2/pow")){
        ibc = UTIL1_strFind((uint8_t *)br,(uint8_t *)"]");
        UTIL1_strcpy((uint8_t *)bc,128,(uint8_t *)br+ibc+3);          //copia os caracteres de br localizados apos '],['
        bc[strlen(bc)-1] = '\0';                                      //apaga o ultimo ']'
        duty_cicle = 0;
        for(i=0; i < strlen(bc); i++)
          duty_cicle = duty_cicle*10 + bc[i] - 0x30;
        Duty_Cycle(duty_cicle);
        send_strTerminal(bc);                     
      }
      
      else if(!strcmp((const char *)bc,(const char *)"MESSAGE [EA076/grupoM2/mod")){
        ibc = UTIL1_strFind((uint8_t *)br,(uint8_t *)"]");
        UTIL1_strcpy((uint8_t *)bc,128,(uint8_t *)br+ibc+3);          //copia os caracteres de br localizados apos '],['
        bc[strlen(bc)-1] = '\0';                                      //apaga o ultimo ']'
        send_strTerminal(bc);                     
        if(!strcmp((const char *)bc,(const char *)"On"))
          modo = 1;
        else if(!strcmp((const char *)bc,(const char *)"Off"))
          modo = 2;
        else if(!strcmp((const char *)bc,(const char *)"Auto"))
          modo = 3;
      }
      
      else if(!strcmp((const char *)bc,(const char *)"MESSAGE [EA076/grupoM2/lim")){
        ibc = UTIL1_strFind((uint8_t *)br,(uint8_t *)"]");
        UTIL1_strcpy((uint8_t *)bc,128,(uint8_t *)br+ibc+3);          //copia os caracteres de br localizados apos '],['
        bc[strlen(bc)-1] = '\0';                                    //apaga o ultimo ']'
        threshold = 0;
        for(i=0; i < strlen(bc)-2; i++){
          if(bc[i] != '.')
            threshold = threshold*10 + bc[i] - 0x30;
        }          
        send_strTerminal(bc);                     
      }
      
      else send_strTerminal(br);      //Se não é msg, replica codigo de erro
    }
    
    
 
    if(OPEN_SDA_receive){      
      OPEN_SDA_receive = 0;      
      if(Uart0_OpenSDA_RecvChar((Uart2_ESP_TComData *)&c) == ERR_OK){
        while(Uart0_OpenSDA_SendChar(c) != ERR_OK);
        bt[ibt] = c;
        ibt++; 
        if(c == 0x0d){
          while(Uart0_OpenSDA_SendChar(0x0a) != ERR_OK);      //pula linha apos escrita no terminal
          bt[ibt - 1] = '"';        //adiciona " final do comando PUBLISH
          bt[ibt] = '\0';
          ibt = 0;                  //reinicializa ibt para proxima msg
          UTIL1_strcpy((uint8_t *)bc,128, (const unsigned char *)"PUBLISH \"EA076/grupoM2/ESP\",\"");
          UTIL1_strcat((uint8_t *)bc, 128,(uint8_t *)bt);
          UTIL1_strcpy((uint8_t *)bt,128, (uint8_t *)bc);
          //send_strTerminal(bt); //Replica comando no terminal
          do{
            send_strESP(bt);            //envia comando
            receive_strfromESP(bc);     //aguarda resposta
          }while(strcmp(bc, "OK PUBLISH")); //Quando ok ( = 0) sai do loop.
          send_strTerminal(bc); //Replica resposta no terminal
          //while(Uart2_ESP_SendChar(0x0a) != ERR_OK);
          //while(Uart0_OpenSDA_SendChar(0x0a) != ERR_OK);
                  
        }
      }      	
    }
  
    
    //Configurando Sensor de temperatura e RTC
    if(TimeStatus == 1){
      TimeStatus = 0;
      TempSensor_Measure(TRUE);
      TempSensor_GetValue16(&ADC_TempValue);
      TempValue = (((float)ADC_TempValue)*3300/65535 - 600);
    
      RTC1_GetTime(RTC1_DeviceData,&CurrentTime);
      hora = CurrentTime.Hour;
      minuto = CurrentTime.Minute;
      segundo = CurrentTime.Second;  
    
      UTIL1_strcpy((uint8_t *)bc,128, (const unsigned char *)"PUBLISH \"EA076/grupoM2/temp\",\"");
      ibc = UTIL1_strlen(bc);
      bc[ibc] = (hora/10 + 0x30);
      bc[ibc+1] = (hora%10 + 0x30);
      bc[ibc+2] = ':';
      bc[ibc+3] = (minuto/10 + 0x30);
      bc[ibc+4] = (minuto%10 + 0x30);
      bc[ibc+5] = ':';
      bc[ibc+6] = (segundo/10 + 0x30);
      bc[ibc+7] = (segundo%10 + 0x30);
      bc[ibc+8] = ' ';
      bc[ibc+9] = (TempValue/100 + 0x30);
      bc[ibc+10] = ((TempValue%100)/10 + 0x30);
      bc[ibc+11] = '.';
      bc[ibc+12] = ((TempValue%100)%10 + 0x30);
      bc[ibc+13] = '°';
      bc[ibc+14] = 'C';
      bc[ibc+15] = '"';
      bc[ibc+16] = '\0';
      do{
        send_strESP(bc);            //envia comando        
        receive_strfromESP(btemp);     //aguarda resposta
      }while(strcmp(btemp, "OK PUBLISH")); //Quando ok ( = 0) sai do loop.      
    }
    
    if(modo == 1){                            //Modo On
      if(dir){
        Bit1_SetVal();                        //Motor ligado na direcao Vent
        Bit2_ClrVal();
      }
      else{                                   //Motor ligado na direcao Exaust
        Bit2_SetVal();
        Bit1_ClrVal();
      }
    }
    else if(modo == 2){                       //Modo Off - Motor desligado
      Bit1_ClrVal();
      Bit2_ClrVal();      
    }
    else if(modo == 3){                       //Modo Auto
      if(TempValue > threshold){
        Bit1_SetVal();
        Bit2_ClrVal();
      }
      else{
        Bit1_ClrVal();
        Bit2_ClrVal();
      }
    }
  }
  /*** Don't write any code pass this line, or it will be deleted during code generation. ***/
  /*** RTOS startup code. Macro PEX_RTOS_START is defined by the RTOS component. DON'T MODIFY THIS CODE!!! ***/
  #ifdef PEX_RTOS_START
    PEX_RTOS_START();                  /* Startup of the selected RTOS. Macro is defined by the RTOS component. */
  #endif
  /*** End of RTOS startup code.  ***/
  /*** Processor Expert end of main routine. DON'T MODIFY THIS CODE!!! ***/
  for(;;){}
  /*** Processor Expert end of main routine. DON'T WRITE CODE BELOW!!! ***/
} /*** End of main routine. DO NOT MODIFY THIS TEXT!!! ***/

/* END main */
/*!
** @}
*/
/*
** ###################################################################
**
**     This file was created by Processor Expert 10.3 [05.09]
**     for the Freescale Kinetis series of microcontrollers.
**
** ###################################################################
*/
