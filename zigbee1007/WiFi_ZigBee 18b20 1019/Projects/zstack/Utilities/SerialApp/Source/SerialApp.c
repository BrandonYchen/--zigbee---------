/*********************************************************************
* INCLUDES
*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "AF.h"
#include "OnBoard.h"
#include "OSAL_Tasks.h"
#include "SerialApp.h"
#include "ZDApp.h"
#include "ZDObject.h"
#include "ZDProfile.h"

#include "hal_drivers.h"
#include "hal_key.h"
#if defined ( LCD_SUPPORTED )
#include "hal_lcd.h"
#endif
#include "hal_led.h"
#include "hal_uart.h"

#include "ds18b20.h"
#include "nwk_globals.h"
/*********************************************************************
* MACROS
*/
#define COORD_ADDR   0x00
#define ED_ADDR      0x01
#define UART0        0x00
#define MAX_NODE     0x04
#define UART_DEBUG   0x00        //���Ժ�,ͨ���������Э�������ն˵�IEEE���̵�ַ
#define LAMP_PIN     P0_4        //����P0.4��Ϊ�̵��������
#define GAS_PIN      P0_5        //����P0.5��Ϊ�����������������  
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof(arr)[0])

//---------------------------------------------------------------------
//��׼�治ͬ���ն���Ҫ�޸Ĵ�ID,����ʶ��Э���������������ݣ�ID��ͬ����
//רҵ���Զ���Flash��õ�ַ�������ն˹̼���ͬ���ʺ�����
static uint16 EndDeviceID = 0x0004; //�ն�ID����Ҫ
//---------------------------------------------------------------------


unsigned char tempRX;
//����

unsigned char  Recdata[128];


/*********************************************************************/

#define STEP1 0x00

#define STEP2 0x01

#define STEP3 0x02

#define STEP4 0x03
static uint8 tasksteps = STEP1;
uint8 adr=0x00;
uint8 fc=0x00;


uint8 Data_Report[30]={0};
uint8 flag=0x00;

/*********************************************************************/
/* CONSTANTS
*/

#if !defined( SERIAL_APP_PORT )
#define SERIAL_APP_PORT  0
#endif

#if !defined( SERIAL_APP_BAUD )
//#define SERIAL_APP_BAUD  HAL_UART_BR_38400
#define SERIAL_APP_BAUD  HAL_UART_BR_115200
#endif

// When the Rx buf space is less than this threshold, invoke the Rx callback.
#if !defined( SERIAL_APP_THRESH )
#define SERIAL_APP_THRESH  64
#endif

#if !defined( SERIAL_APP_RX_SZ )
#define SERIAL_APP_RX_SZ  128
#endif

#if !defined( SERIAL_APP_TX_SZ )
#define SERIAL_APP_TX_SZ  128
#endif

// Millisecs of idle time after a byte is received before invoking Rx callback.
#if !defined( SERIAL_APP_IDLE )
#define SERIAL_APP_IDLE  6
#endif

// Loopback Rx bytes to Tx for throughput testing.
#if !defined( SERIAL_APP_LOOPBACK )
#define SERIAL_APP_LOOPBACK  FALSE
#endif

// This is the max byte count per OTA message.
#if !defined( SERIAL_APP_TX_MAX )
//#define SERIAL_APP_TX_MAX  30
#define SERIAL_APP_TX_MAX  40
#endif

#define SERIAL_APP_RSP_CNT  4

// This list should be filled with Application specific Cluster IDs.
const cId_t SerialApp_ClusterList[SERIALAPP_MAX_CLUSTERS] =
{
	SERIALAPP_CLUSTERID
};

const SimpleDescriptionFormat_t SerialApp_SimpleDesc =
{
	SERIALAPP_ENDPOINT,              //  int   Endpoint;
    SERIALAPP_PROFID,                //  uint16 AppProfId[2];
    SERIALAPP_DEVICEID,              //  uint16 AppDeviceId[2];
    SERIALAPP_DEVICE_VERSION,        //  int   AppDevVer:4;
    SERIALAPP_FLAGS,                 //  int   AppFlags:4;
    SERIALAPP_MAX_CLUSTERS,          //  byte  AppNumInClusters;
    (cId_t *)SerialApp_ClusterList,  //  byte *pAppInClusterList;
    SERIALAPP_MAX_CLUSTERS,          //  byte  AppNumOutClusters;
    (cId_t *)SerialApp_ClusterList   //  byte *pAppOutClusterList;
};

const endPointDesc_t SerialApp_epDesc =
{
	SERIALAPP_ENDPOINT,
    &SerialApp_TaskID,
    (SimpleDescriptionFormat_t *)&SerialApp_SimpleDesc,
    noLatencyReqs
};

/*********************************************************************
* TYPEDEFS
*/

/*********************************************************************
* GLOBAL VARIABLES
*/

uint8 SerialApp_TaskID;    // Task ID for internal task/event processing.

/*********************************************************************
* EXTERNAL VARIABLES
*/

/*********************************************************************
* EXTERNAL FUNCTIONS
*/

/*********************************************************************
* LOCAL VARIABLES
*/
static bool SendFlag = 0;

static uint8 SerialApp_MsgID;

static afAddrType_t SerialApp_TxAddr;
static afAddrType_t Broadcast_DstAddr;

static uint8 SerialApp_TxSeq;
static uint8 SerialApp_TxBuf[SERIAL_APP_TX_MAX+1];
static uint8 SerialApp_TxLen;

static afAddrType_t SerialApp_RxAddr;
static uint8 SerialApp_RspBuf[SERIAL_APP_RSP_CNT];

static devStates_t SerialApp_NwkState;
static afAddrType_t SerialApp_TxAddr;
static uint8 SerialApp_MsgID;

uint8 NodeData[MAX_NODE][5];         //�ն����ݻ����� 0=�¶� 1=ʪ�� 2=���� 3=��



/*********************************************************************
* LOCAL FUNCTIONS
*/

static void SerialApp_HandleKeys( uint8 shift, uint8 keys );
static void SerialApp_ProcessMSGCmd( afIncomingMSGPacket_t *pkt );
static void SerialApp_Send(void);
static void SerialApp_Resp(void);
static void SerialApp_CallBack(uint8 port, uint8 event);

static void PrintAddrInfo(uint16 shortAddr, uint8 *pIeeeAddr);
static void AfSendAddrInfo(void);
static void GetIeeeAddr(uint8 * pIeeeAddr, uint8 *pStr);
static void SerialApp_SendPeriodicMessage( void );
static uint8 GetDataLen(uint8 fc);
static uint8 XorCheckSum(uint8 * pBuf, uint8 len);
uint8 SendData(uint8 addr, uint8 FC);

void Init_Wifi(void)
{

  HalLcdWriteString( "initwifi", HAL_LCD_LINE_2 );      
 
  HalUARTWrite(UART0, "AT+CIPMUX=1\r\n", 12);
  Delay_ms(500);
  tasksteps=STEP2;

} 

//��ʼ������0
void initUART0(void)
{
   //����
      CLKCONCMD &= ~0x40;                         //����ϵͳʱ��ԴΪ32MHZ����
      while(CLKCONSTA & 0x40);                    //�ȴ������ȶ�
      CLKCONCMD &= ~0x47;                         //����ϵͳ��ʱ��Ƶ��Ϊ32MHZ
  
      PERCFG = 0x00;	//λ��1 P0 ��
      P0SEL = 0x0c;	//P0_2,P0_3�������ڣ��ⲿ�豸���ܣ�
      P2DIR &= ~0XC0; //P0������ΪUART0

      U0CSR |= 0x80;	//����ΪUART��ʽ
      
      U0GCR |= 11;
      U0BAUD |= 216;	//��������Ϊ115200 ����������л�õ�����
      UTX0IF = 1;	//UART0 TX �жϱ�־��ʼ��λ0

      U0CSR |= 0x40; //��������
      IEN0 |= 0x84; //�����ж����������ж�  
  
  
}

void SerialApp_Init( uint8 task_id )
{
	halUARTCfg_t uartConfig;
    
    P0SEL &= 0xEf;                  //����P0.4��Ϊ��ͨIO
    P0DIR |= 0x10;                  //����P0.4Ϊ���
    LAMP_PIN = 0;                   //�ߵ�ƽ�̵����Ͽ�;�͵�ƽ�̵�������
    P0SEL &= ~0x20;                 //����P0.5Ϊ��ͨIO��
    P0DIR &= ~0x20;                 //P0.5����Ϊ�����
    P0SEL &= 0xbf;                  //P0_6���ó�ͨ��io
	
	SerialApp_TaskID = task_id;
	//SerialApp_RxSeq = 0xC3;
	
	afRegister( (endPointDesc_t *)&SerialApp_epDesc );
	
	RegisterForKeys( task_id );
	
	uartConfig.configured           = TRUE;              // 2x30 don't care - see uart driver.
	uartConfig.baudRate             = SERIAL_APP_BAUD;
	uartConfig.flowControl          = FALSE;
	uartConfig.flowControlThreshold = SERIAL_APP_THRESH; // 2x30 don't care - see uart driver.
	uartConfig.rx.maxBufSize        = SERIAL_APP_RX_SZ;  // 2x30 don't care - see uart driver.
	uartConfig.tx.maxBufSize        = SERIAL_APP_TX_SZ;  // 2x30 don't care - see uart driver.
	uartConfig.idleTimeout          = SERIAL_APP_IDLE;   // 2x30 don't care - see uart driver.
	uartConfig.intEnable            = TRUE;              // 2x30 don't care - see uart driver.
	uartConfig.callBackFunc         = SerialApp_CallBack;
	HalUARTOpen (UART0, &uartConfig);
        ////cici
//������նˣ��ͳ�ʼ����
#if defined(ZDO_COORDINATOR)       
        
#else      
    initUART0();
#endif
    
#if defined ( LCD_SUPPORTED )
	HalLcdWriteString( "SerialApp", HAL_LCD_LINE_2 );
#endif
	//HalUARTWrite(UART0, "Init", 4);
        
	ZDO_RegisterForZDOMsg( SerialApp_TaskID, End_Device_Bind_rsp );
	ZDO_RegisterForZDOMsg( SerialApp_TaskID, Match_Desc_rsp );
 //       Init_ESP8266();
//�����Э��������ʼ��wifi
#if defined(ZDO_COORDINATOR)       
        Init_Wifi();
#else        
        //������նˣ��ɼ���
        osal_start_timerEx( SerialApp_TaskID,
                            SERIALAPP_SEND_PERIODIC_EVT,
                            SERIALAPP_SEND_PERIODIC_TIMEOUT ); 
#endif
}

UINT16 SerialApp_ProcessEvent( uint8 task_id, UINT16 events )
{
	(void)task_id;  // Intentionally unreferenced parameter
	
	if ( events & SYS_EVENT_MSG )
	{
		afIncomingMSGPacket_t *MSGpkt;
		
		while ( (MSGpkt = (afIncomingMSGPacket_t *)osal_msg_receive( SerialApp_TaskID )) )
		{
			switch ( MSGpkt->hdr.event )
			{
			case ZDO_CB_MSG:
				//SerialApp_ProcessZDOMsgs( (zdoIncomingMsg_t *)MSGpkt );
				break;
				
			case KEY_CHANGE:
				SerialApp_HandleKeys( ((keyChange_t *)MSGpkt)->state, ((keyChange_t *)MSGpkt)->keys );
				break;
				
			case AF_INCOMING_MSG_CMD:
				SerialApp_ProcessMSGCmd( MSGpkt );
				break;
                
            case ZDO_STATE_CHANGE:
              SerialApp_NwkState = (devStates_t)(MSGpkt->hdr.status);
              if ( (SerialApp_NwkState == DEV_ZB_COORD)
                  || (SerialApp_NwkState == DEV_ROUTER)
                  || (SerialApp_NwkState == DEV_END_DEVICE) )
              {
                #if defined(ZDO_COORDINATOR) //Э����ͨ��������������̵�ַ��IEEE  
                    Broadcast_DstAddr.addrMode = (afAddrMode_t)AddrBroadcast;
                    Broadcast_DstAddr.endPoint = SERIALAPP_ENDPOINT;
                    Broadcast_DstAddr.addr.shortAddr = 0xFFFF;
                    #if UART_DEBUG           
                    PrintAddrInfo( NLME_GetShortAddr(), aExtendedAddress + Z_EXTADDR_LEN - 1);
                    #endif 
                    //��ʼ���Ƶ�״̬��1ΪϨ��״̬��0Ϊ����
                    NodeData[0][3] = 1;
                    NodeData[1][3] = 1;
                    NodeData[2][3] = 1;
                    NodeData[3][3] = 1;
                #else                        //�ն����߷��Ͷ̵�ַ��IEEE   
                    AfSendAddrInfo();
                #endif
                
              }
              break;				
			default:
				break;
			}
			
			osal_msg_deallocate( (uint8 *)MSGpkt );
		}
		
		return ( events ^ SYS_EVENT_MSG );
	}
    
    //�ڴ��¼��п��Զ�ʱ��Э�������ͽڵ㴫����������Ϣ
    if ( events & SERIALAPP_SEND_PERIODIC_EVT )
    {
        SerialApp_SendPeriodicMessage();
        
        osal_start_timerEx( SerialApp_TaskID, SERIALAPP_SEND_PERIODIC_EVT,
            (SERIALAPP_SEND_PERIODIC_TIMEOUT + (osal_rand() & 0x00FF)) );
        
        return (events ^ SERIALAPP_SEND_PERIODIC_EVT);
    }
    
	if ( events & SERIALAPP_SEND_EVT )
	{
		SerialApp_Send();
		return ( events ^ SERIALAPP_SEND_EVT );
	}
	
	if ( events & SERIALAPP_RESP_EVT )
	{
		SerialApp_Resp();
		return ( events ^ SERIALAPP_RESP_EVT );
	}
	
	return ( 0 ); 
}

/*********************************************************************
* @fn      SerialApp_HandleKeys
*
* @brief   Handles all key events for this device.
*
* @param   shift - true if in shift/alt.
* @param   keys  - bit field for key events.
*
* @return  none
*/
void SerialApp_HandleKeys( uint8 shift, uint8 keys )
{
	zAddrType_t txAddr;
	
    if ( keys & HAL_KEY_SW_6 ) //��S1��������ֹͣ�ն˶�ʱ�ϱ����� 
    {
      if(SendFlag == 0)
        {
        SendFlag = 1;
        HalLedSet ( HAL_LED_1, HAL_LED_MODE_ON );
        osal_start_timerEx( SerialApp_TaskID,
                            SERIALAPP_SEND_PERIODIC_EVT,
                            SERIALAPP_SEND_PERIODIC_TIMEOUT );
        }
        else
        {      
            SendFlag = 0;
            HalLedSet ( HAL_LED_1, HAL_LED_MODE_OFF );
            osal_stop_timerEx(SerialApp_TaskID, SERIALAPP_SEND_PERIODIC_EVT);
        }
    }
    
    if ( keys & HAL_KEY_SW_1 ) //��S2
    {
        LAMP_PIN = ~LAMP_PIN;
    }
    
    if ( keys & HAL_KEY_SW_2 )
    {
        HalLedSet ( HAL_LED_4, HAL_LED_MODE_OFF );
        
        // Initiate an End Device Bind Request for the mandatory endpoint
        txAddr.addrMode = Addr16Bit;
        txAddr.addr.shortAddr = 0x0000; // Coordinator
        ZDP_EndDeviceBindReq( &txAddr, NLME_GetShortAddr(), 
            SerialApp_epDesc.endPoint,
            SERIALAPP_PROFID,
            SERIALAPP_MAX_CLUSTERS, (cId_t *)SerialApp_ClusterList,
            SERIALAPP_MAX_CLUSTERS, (cId_t *)SerialApp_ClusterList,
            FALSE );
    }
    
    if ( keys & HAL_KEY_SW_3 )
    {
    }
    
    if ( keys & HAL_KEY_SW_4 )
    {
        HalLedSet ( HAL_LED_4, HAL_LED_MODE_OFF );
        
        // Initiate a Match Description Request (Service Discovery)
        txAddr.addrMode = AddrBroadcast;
        txAddr.addr.shortAddr = NWK_BROADCAST_SHORTADDR;
        ZDP_MatchDescReq( &txAddr, NWK_BROADCAST_SHORTADDR,
            SERIALAPP_PROFID,
            SERIALAPP_MAX_CLUSTERS, (cId_t *)SerialApp_ClusterList,
            SERIALAPP_MAX_CLUSTERS, (cId_t *)SerialApp_ClusterList,
            FALSE );
    }

}

void SerialApp_ProcessMSGCmd( afIncomingMSGPacket_t *pkt )
{
    uint16 shortAddr;
    uint8 *pIeeeAddr; 
    uint8 delay;
    uint8 afRxData[30]={0};

    
    
	//��ѯ�����ն������д����������� 3A 00 01 02 39 23  ��Ӧ��3A 00 01 02 00 00 00 00 xor 23
	switch ( pkt->clusterId )
	{
	// A message with a serial data block to be transmitted on the serial port.
	case SERIALAPP_CLUSTERID:
        osal_memcpy(afRxData, pkt->cmd.Data, pkt->cmd.DataLength);
        
        #if defined(ZDO_COORDINATOR)
        #else 
            HalUARTWrite (UART0, afRxData, pkt->cmd.DataLength); //����ʱͨ���������
        #endif    
		switch(afRxData[0]) //��Э�������ֽ���
		{
#if defined(ZDO_COORDINATOR)
		case 0x3B:  //�յ��ն����߷������Ķ̵�ַ��IEEE��ַ,ͨ�����������ʾ      
			shortAddr=(afRxData[1]<<8)|afRxData[2];
			pIeeeAddr = &afRxData[3];
            #if UART_DEBUG
			PrintAddrInfo(shortAddr, pIeeeAddr + Z_EXTADDR_LEN - 1);
            #endif   
			break;
		case 0x3A:	
            if(afRxData[3] == 0x02) //�յ��ն˴������Ĵ��������ݲ�����
            {  
                NodeData[afRxData[2]-1][0] = afRxData[4];
                NodeData[afRxData[2]-1][1] = afRxData[5];
                NodeData[afRxData[2]-1][2] = afRxData[6];
                NodeData[afRxData[2]-1][3] = afRxData[7];
                NodeData[afRxData[2]-1][4] = 0x00;
                
 /////////////////////////////cici////*******************************////////////////               
                //Э�����ɼ������� ������
              osal_memset(Data_Report,0x00,30);
              osal_memcpy(Data_Report, afRxData, 30); 

              osal_memset(afRxData,0x00,30);
//////////////////**************************************/////////////////////////////////
              }
           ///////////////////////////////////////////////   
            
        #if UART_DEBUG
            HalUARTWrite (UART0, NodeData[afRxData[3]-1], 4); //����ʱͨ���������
            HalUARTWrite (UART0, "\n", 1);
        #endif            
           break;
#else  
		case 0x3A:  //���ص��豸          
        if(afRxData[3] == 0x0A || afRxData[3] == 0x0B || afRxData[3] == 0x0C) //�����ն�          
        {  
			if(EndDeviceID == afRxData[2] || afRxData[2]==0xFF)
			{
				if(afRxData[4] == 0)
                {
                    LAMP_PIN = 1;
				//	HalLedSet ( HAL_LED_2, HAL_LED_MODE_OFF );
                }
				else
                {
                    LAMP_PIN = 0;
				//	HalLedSet ( HAL_LED_2, HAL_LED_MODE_ON );
                }
			}
			break;
        }		
#endif
        default :
            break;
        }
        break;
		// A response to a received serial data block.
		case SERIALAPP_CLUSTERID2:
			if ((pkt->cmd.Data[1] == SerialApp_TxSeq) &&
				((pkt->cmd.Data[0] == OTA_SUCCESS) || (pkt->cmd.Data[0] == OTA_DUP_MSG)))
			{
				SerialApp_TxLen = 0;
				osal_stop_timerEx(SerialApp_TaskID, SERIALAPP_SEND_EVT);
			}
			else
			{
				// Re-start timeout according to delay sent from other device.
				delay = BUILD_UINT16( pkt->cmd.Data[2], pkt->cmd.Data[3] );
				osal_start_timerEx( SerialApp_TaskID, SERIALAPP_SEND_EVT, delay );
			}
			break;
			
		default:
			break;
	}
}

uint8 TxBuffer[128];

uint8 SendData(uint8 addr, uint8 FC)
{
	uint8 ret, i, index=4;

        osal_memset(TxBuffer,0x00,128);
         
	TxBuffer[0] = 0x3A;
	TxBuffer[1] = 0x00;
	TxBuffer[2] = addr;
	TxBuffer[3] = FC;

	switch(FC)
	{
	case 0x01: //��ѯ�����ն˴�����������
		for (i=0; i<MAX_NODE; i++)
		{
			osal_memcpy(&TxBuffer[index], NodeData[i], 4);
			index += 4;
		}
		TxBuffer[index] = XorCheckSum(TxBuffer, index);
		TxBuffer[index+1] = 0x23; 
		
		HalUARTWrite(UART0, TxBuffer, index+2);
        ret = 1;
		break;
	case 0x02: //��ѯ�����ն������д�����������
		osal_memcpy(&TxBuffer[index], NodeData[addr-1], 4);
		index += 4;
		TxBuffer[index] = XorCheckSum(TxBuffer, index);
		TxBuffer[index+1] = 0x23; 
	
		HalUARTWrite(UART0, TxBuffer, index+2);		
        ret = 1;
		break;   
	default:
        ret = 0;
		break;
	}

    return ret;
}

/*********************************************************************
* @fn      SerialApp_Send
*
* @brief   Send data OTA.
*
* @param   none
*
* @return  none
*/
static void SerialApp_Send(void)
{
    uint8  addr, FC;
//    uint8 checksum=0;
    int len=0;
    int i;
    
    uint8 length[3]={0};
    uint8 tmpbuf[256]={0};
    uint8 cmd[256]={0};
 //  char *p=NULL;
	
#if SERIAL_APP_LOOPBACK
	if (SerialApp_TxLen < SERIAL_APP_TX_MAX)
	{
		SerialApp_TxLen += HalUARTRead(SERIAL_APP_PORT, SerialApp_TxBuf+SerialApp_TxLen+1,
			SERIAL_APP_TX_MAX-SerialApp_TxLen);
	}
	
	if (SerialApp_TxLen)
	{
		(void)SerialApp_TxAddr;
		if (HalUARTWrite(SERIAL_APP_PORT, SerialApp_TxBuf+1, SerialApp_TxLen))
		{
			SerialApp_TxLen = 0;
		}
		else
		{
			osal_set_event(SerialApp_TaskID, SERIALAPP_SEND_EVT);
		}
	}
#else
	if (!SerialApp_TxLen && 
		(SerialApp_TxLen = HalUARTRead(UART0, SerialApp_TxBuf, SERIAL_APP_TX_MAX)))
	
        {
        if (SerialApp_TxLen)
        {
            
//////////////////////////***********************/////////////////////////////////////////////
             if(strstr((char *)SerialApp_TxBuf,"+IPD")!=NULL) //���յ�ָ��
             {
               sscanf(SerialApp_TxBuf,"%*[^,]%*c%*[^,]%*c%[^:]%*c%s]",length,tmpbuf);            
               len=atoi(length);
               
               for(i=0;i<SerialApp_TxLen;i++)
               {
                 if(SerialApp_TxBuf[i]==':')          
                   break;
               }
               
               osal_memcpy(cmd,&SerialApp_TxBuf[i+1],len);
               adr = cmd[2];
               fc=cmd[3];
               
               HalLcdWriteString( "cmd ready....", HAL_LCD_LINE_3 );
               
               if(cmd[3]==0x0A) //����ָ�� �㲥��ȥ
	      {   
               //���յ�������ͨ�����ߵ���ʽ���ͳ�ȥ
                if (afStatus_SUCCESS == AF_DataRequest(&Broadcast_DstAddr,
                                                        (endPointDesc_t *)&SerialApp_epDesc,
                                                         SERIALAPP_CLUSTERID,
                                                         len, cmd,
                                                         &SerialApp_MsgID, 0, AF_DEFAULT_RADIUS)) 

                  {
                    Delay_ms(500);
                    HalLcdWriteString( "cmd send[ok]....", HAL_LCD_LINE_3 );
                
                  }
                }
              else if(cmd[3]==0x02)//��ѯָ�� �ϱ�����
               {
                    if(flag==0x00)
                    {
                      HalUARTWrite (UART0,"AT+CIPSEND=0,10\r\n",16);
                      flag=0x01;
                    }
             //       Delay_ms(500);
               }
               else if(cmd[3]==0x01)//��ѯָ�� �ϱ�����
               {
                    if(flag==0x00)
                    {
                      HalUARTWrite (UART0,"AT+CIPSEND=0,22\r\n",16);
                      flag=0x01;
                    }
             //       Delay_ms(500);
               }              
               
                                              
               HalLcdWriteString( "wait cmd....", HAL_LCD_LINE_3 );
               
               
             }        
              //    sprintf(Recdata,"%s",SerialApp_TxBuf); ////????
             //     HalLcdWriteString( Recdata, HAL_LCD_LINE_2 );
             else if((strstr(&SerialApp_TxBuf[1],"OK")!=NULL)||(strstr(&SerialApp_TxBuf[1],"no change")!=NULL))
             {
                   if(tasksteps == STEP2)
                   {                     
                     HalLcdWriteString( "CIPMUX...[ok].", HAL_LCD_LINE_2 );
                     HalUARTWrite(UART0, "AT+CIPSERVER=1,33333\r\n", 21);
                     Delay_ms(500);
               
                     tasksteps = STEP3;
                   }
                   else if(tasksteps == STEP3)
                   {
                      HalLcdWriteString( "Init Server.[ok]", HAL_LCD_LINE_2 );
                      
                       tasksteps = STEP4;                                 
                   }
              /*     else if((strstr(SerialApp_TxBuf,"Link")!=NULL))
                   {
                      HalLcdWriteString( "Link [ok]..", HAL_LCD_LINE_3 );
                   } */
                   
              }
              else if(strstr(&SerialApp_TxBuf[1],">")!=NULL)
              {
                osal_memset(SerialApp_TxBuf,0x00,31);
                if(flag==0x01)
                {
  /*               if(cmd[3]==0x02) //��ѯ�����ն�
                 {
                  HalUARTWrite(UART0, Data_Report, 10);//���Ͳɼ�������
                  Delay_ms(500);
                  flag=0x00;
                 }
                 else if(cmd[3]==0x01) //��ѯ�����ն�
                 {
                   
                 }
                 
  */
                
                  
                   SendData(adr, fc);
                   flag=0x00;
                   
                }
                
              }
              else if((strstr(SerialApp_TxBuf,"Link")!=NULL))
              {
                   HalLcdWriteString( "Link [ok]..", HAL_LCD_LINE_3 );
              } 
              else if((strstr(SerialApp_TxBuf,"UnLink")!=NULL))
              {
                   HalLcdWriteString( "UnLink ..", HAL_LCD_LINE_3 );
               } 
             
               osal_memset(tmpbuf,0x00,31);
              
               osal_memset(SerialApp_TxBuf,0x00,31);            
             
               len=0;
               SerialApp_TxLen = 0;  
 //////////////////////////***********************/////////////////////////////////////////////           
        }
    }
#endif
}

/*********************************************************************
* @fn      SerialApp_Resp
*
* @brief   Send data OTA.
*
* @param   none
*
* @return  none
*/
static void SerialApp_Resp(void)
{
	if (afStatus_SUCCESS != AF_DataRequest(&SerialApp_RxAddr,
		(endPointDesc_t *)&SerialApp_epDesc,
		SERIALAPP_CLUSTERID2,
		SERIAL_APP_RSP_CNT, SerialApp_RspBuf,
		&SerialApp_MsgID, 0, AF_DEFAULT_RADIUS))
	{
		osal_set_event(SerialApp_TaskID, SERIALAPP_RESP_EVT);
	}
}

/*********************************************************************
* @fn      SerialApp_CallBack
*
* @brief   Send data OTA.
*
* @param   port - UART port.
* @param   event - the UART port event flag.
*
* @return  none
*/
static void SerialApp_CallBack(uint8 port, uint8 event)
{
	(void)port;
	
	if ((event & (HAL_UART_RX_FULL | HAL_UART_RX_ABOUT_FULL | HAL_UART_RX_TIMEOUT)) &&
#if SERIAL_APP_LOOPBACK
		(SerialApp_TxLen < SERIAL_APP_TX_MAX))
#else
		!SerialApp_TxLen)
#endif
	{
          	
	SerialApp_Send();
                  
        }      
          
          
	
}


//------------------------------------------------------------------------------------------------------------------------------------------
//��ѯ�����ն������д����������� 3A 00 01 02 XX 23  ��Ӧ��3A 00 01 02 00 00 00 00 xor 23
void SerialApp_SendPeriodicMessage( void )
{
    uint8 SendBuf[10]={0};
    uint8 str[7];
    //float t=0.0;
    int t; 
  
    SendBuf[0] = 0x3A;                          
    SendBuf[1] = HI_UINT16( EndDeviceID );
    SendBuf[2] = LO_UINT16( EndDeviceID );
    SendBuf[3] = 0x02;                       //FC
    
    Delay_ms(500);
    floatReadDs18B20(); //��ȡ�¶�����
    Delay_ms(500);
    
    t=fValue*10;   //���ݴ���
    str[0] = t/100+48;
    str[1] = t%100/10+48;
    str[2] = 46;
    str[3] = t%100%10+48;
    str[4] = ' ';
    str[5] = 'C';
    str[6] = '\0';  
    
    HalUARTWrite(0, str, 7);
    HalUARTWrite(0, "\n",1);
    
    
    SendBuf[4] = fValue;  
    SendBuf[5] = Flag;  
    SendBuf[6] = 0;
    SendBuf[7] = 0; 
    SendBuf[8] = XorCheckSum(SendBuf, 9);
    SendBuf[9] = 0x23;
    
    SerialApp_TxAddr.addrMode = (afAddrMode_t)Addr16Bit;
    SerialApp_TxAddr.endPoint = SERIALAPP_ENDPOINT;
    SerialApp_TxAddr.addr.shortAddr = 0x00;  
    if ( AF_DataRequest( &SerialApp_TxAddr, (endPointDesc_t *)&SerialApp_epDesc,
               SERIALAPP_CLUSTERID,
               10,
               SendBuf,
               &SerialApp_MsgID, 
               0, 
               AF_DEFAULT_RADIUS ) == afStatus_SUCCESS )
    {
    // Successfully requested to be sent.
      HalUARTWrite (UART0, "SendSuccess\n", 13);
    }
    else
    {
    // Error occurred in request to send.
      HalUARTWrite (UART0, "SendFail\n", 10); 
    }
}



//ͨ����������̵�ַ IEEE
void PrintAddrInfo(uint16 shortAddr, uint8 *pIeeeAddr)
{
    uint8 strIeeeAddr[17] = {0};
    char  buff[30] = {0};    
    
    //��ö̵�ַ   
    sprintf(buff, "shortAddr:%04X   IEEE:", shortAddr);  
 
    //���IEEE��ַ
    GetIeeeAddr(pIeeeAddr, strIeeeAddr);
//cici
    #if UART_DEBUG
    HalUARTWrite (UART0, (uint8 *)buff, strlen(buff));
    Delay_ms(10);
    HalUARTWrite (UART0, strIeeeAddr, 16); 
    HalUARTWrite (UART0, "\n", 1);
#endif  
}

void AfSendAddrInfo(void)
{
    uint16 shortAddr;
    uint8 strBuf[11]={0};  
    
    SerialApp_TxAddr.addrMode = (afAddrMode_t)Addr16Bit;
    SerialApp_TxAddr.endPoint = SERIALAPP_ENDPOINT;
    SerialApp_TxAddr.addr.shortAddr = 0x00;   
    
    shortAddr=NLME_GetShortAddr();
    
    strBuf[0] = 0x3B;                          //���͵�ַ��Э���� �����ڵ㲥
    strBuf[1] = HI_UINT16( shortAddr );        //��Ŷ̵�ַ��8λ
    strBuf[2] = LO_UINT16( shortAddr );        //��Ŷ̵�ַ��8λ
    
    osal_memcpy(&strBuf[3], NLME_GetExtAddr(), 8);
        
   if ( AF_DataRequest( &SerialApp_TxAddr, (endPointDesc_t *)&SerialApp_epDesc,
                       SERIALAPP_CLUSTERID,
                       11,
                       strBuf,
                       &SerialApp_MsgID, 
                       0, 
                       AF_DEFAULT_RADIUS ) == afStatus_SUCCESS )
  {
  }
  else
  {
    // Error occurred in request to send.
  }   
}

void GetIeeeAddr(uint8 * pIeeeAddr, uint8 *pStr)
{
  uint8 i;
  uint8 *xad = pIeeeAddr;

  for (i = 0; i < Z_EXTADDR_LEN*2; xad--)
  {
    uint8 ch;
    ch = (*xad >> 4) & 0x0F;
    *pStr++ = ch + (( ch < 10 ) ? '0' : '7');
    i++;
    ch = *xad & 0x0F;
    *pStr++ = ch + (( ch < 10 ) ? '0' : '7');
    i++;
  }
}

uint8 XorCheckSum(uint8 * pBuf, uint8 len)
{
	uint8 i;
	uint8 byRet=0;

	if(len == 0)
		return byRet;
	else
		byRet = pBuf[0];

	for(i = 1; i < len; i ++)
		byRet = byRet ^ pBuf[i];

	return byRet;
}

uint8 GetDataLen(uint8 fc)
{
    uint8 len=0;
    switch(fc)
    {
    case 0x0A:
    case 0x0B:
    case 0x0C:
    case 0x0D:
      len = 1;
      break;
    }
    
    return len;
}

void judgeTemp(temp)
{
  
}
//-------------------------------------------------------------------


