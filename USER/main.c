#include <string.h>
#include "stm32f10x.h"
#include "usart.h"
#include "delay.h"
#include "led.h"
#include "24cxx.h"
#include "spi.h"
#include "flash.h"
#include "stmflash.h"
#include "enc28j60.h"
#include "timerx.h"
#include "uip.h"
#include "uip_arp.h"
#include "tapdev.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "modbus.h"
#include "define.h"
#include "registerlist.h"
#include "rs485.h"
#include "ade7753.h"
#include "analog_input.h"
#include "analog_output.h"
static void vPowerMeterTask(void *pvParameters);
static void vCOMMTask(void *pvParameters);

static void vNETTask(void *pvParameters);
 

void uip_polling(void);

#define	BUF	((struct uip_eth_hdr *)&uip_buf[0])	
	
u8 update = 0;

static void debug_config(void)
{
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO | RCC_APB2Periph_GPIOB | RCC_APB2Periph_GPIOA, ENABLE);
	GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE);
}

int main(void)
{
  	NVIC_SetVectorTable(NVIC_VectTab_FLASH, 0x8008000);
	debug_config();
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);
 	delay_init(72);
	
//	delay_ms(3000);
	EEP_Dat_Init();
//	uart1_init(115200);
	printf("\r\n main test \n\r");
	SPI2_Init();
 	TIM3_Int_Init(100,2);
//	SPI1_Init(); //initialise it in ade7753_init() function
	xTaskCreate( vPowerMeterTask, ( signed portCHAR * ) "PowerMeter", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 1, NULL );
 	
   	xTaskCreate( vLED0Task, ( signed portCHAR * ) "LED0", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 1, NULL );
 	xTaskCreate( vCOMMTask, ( signed portCHAR * ) "COMM", configMINIMAL_STACK_SIZE + 512, NULL, tskIDLE_PRIORITY + 3, NULL );
   	xTaskCreate( vNETTask, ( signed portCHAR * ) "NET",  configMINIMAL_STACK_SIZE + 256, NULL, tskIDLE_PRIORITY + 3, NULL );
	
	xTaskCreate( vInputTask, ( signed portCHAR * ) "AnalogInput",  256, NULL, tskIDLE_PRIORITY + 2, NULL );
	xTaskCreate( vOutputTask, ( signed portCHAR * ) "AnalogOutput",  256, NULL, tskIDLE_PRIORITY + 2, NULL );
	
	xTaskCreate( vMSTP_TASK, ( signed portCHAR * ) "MSTP", configMINIMAL_STACK_SIZE + 256  , NULL, tskIDLE_PRIORITY + 3, NULL );
 
	vTaskStartScheduler();
}

void vPowerMeterTask( void *pvParameters )
{ 
	int8 PHASE_TEMP = 0;
	int8 i;
	ade7753_init();
	delay_ms(100);
	while(1)
	{ 
//		if(PHASE_TEMP > 2) PHASE_TEMP = 0;
//		else
//			PHASE_TEMP++; 
//		printf("\r\n vPowerMeterTask \n\r");
		PHASE_TEMP = 1;
		for(i=0;i<30;i++)
		{
			read_frequency(PHASE_TEMP);
			read_voltage_peak(PHASE_TEMP);
			read_current_peak(PHASE_TEMP); 
			
			get_vrms(PHASE_TEMP);
			get_irms(PHASE_TEMP);
			 
			
			if(wh_calibration[PHASE_TEMP].cf_calibration_enable == 1)
			{
				if(watt_hour_calibration_cf(PHASE_TEMP))
				{
					wh_calibration[PHASE_TEMP].cf_calibration_enable = 0;
				}
				else
				{
					wh_calibration[PHASE_TEMP].cf_calibration_enable = 2;
				}
			}
			
			delay_ms(100);
		}
		delay_ms(1000);
	}
}
 
void vCOMMTask(void *pvParameters )
{
	modbus_init();
	delay_ms(100);
	for( ;; )
	{
		if(dealwithTag)
		{  
			dealwithTag--;
			if(dealwithTag == 1)//&& !Serial_Master)	
				dealwithData();
		}
		
		if(serial_receive_timeout_count > 0)  
		{
			serial_receive_timeout_count--; 
			if(serial_receive_timeout_count == 0)
			{
				serial_restart();
			}
		}
		delay_ms(5);
	}
}

void vNETTask( void *pvParameters )
{
	u8 count = 0 ;
	while(tapdev_init())	//��ʼ��ENC28J60����
	{								   
		delay_ms(50);
		printf("tapdev_init() failed ...\r\n");
	}
	delay_ms(100);
    for( ;; )
	{
		uip_polling();	//����uip�¼���������뵽�û������ѭ������ 
		
		if((IP_Change == 1) || (update == 1))
		{
			count++ ;
			if(count == 10)
			{
//				if(IP_Change)
//				{
//					app2boot_type = 0x55;
//					AT24CXX_WriteOneByte(EEP_APP2BOOT_TYPE, app2boot_type);
//				}
				count = 0;
				IP_Change = 0;
				SoftReset();
			}
			
		}
		
		delay_ms(5);
    }
}




//uip�¼�������
//���뽫�ú��������û���ѭ��,ѭ������.
void uip_polling(void)
{
	u8 i;
	static struct timer periodic_timer, arp_timer;
	static u8 timer_ok = 0;	 
	if(timer_ok == 0)		//����ʼ��һ��
	{
		timer_ok = 1;
		timer_set(&periodic_timer, CLOCK_SECOND / 2); 	//����1��0.5��Ķ�ʱ�� 
		timer_set(&arp_timer, CLOCK_SECOND * 10);	   	//����1��10��Ķ�ʱ�� 
	}
	
	uip_len = tapdev_read();							//�������豸��ȡһ��IP��,�õ����ݳ���.uip_len��uip.c�ж���
	if(uip_len > 0)							 			//������
	{   
		//����IP���ݰ�(ֻ��У��ͨ����IP���Żᱻ����) 
		if(BUF->type == htons(UIP_ETHTYPE_IP))			//�Ƿ���IP��? 
		{
			uip_arp_ipin();								//ȥ����̫��ͷ�ṹ������ARP��
			uip_input();   								//IP������			
			//������ĺ���ִ�к������Ҫ�������ݣ���ȫ�ֱ��� uip_len > 0
			//��Ҫ���͵�������uip_buf, ������uip_len  (����2��ȫ�ֱ���)		    
			if(uip_len > 0)								//��Ҫ��Ӧ����
			{
				uip_arp_out();							//����̫��ͷ�ṹ������������ʱ����Ҫ����ARP����
				tapdev_send();							//�������ݵ���̫��
			}
		}
		else if (BUF->type == htons(UIP_ETHTYPE_ARP))	//����arp����,�Ƿ���ARP�����?
		{
			uip_arp_arpin();
			
 			//������ĺ���ִ�к������Ҫ�������ݣ���ȫ�ֱ���uip_len>0
			//��Ҫ���͵�������uip_buf, ������uip_len(����2��ȫ�ֱ���)
 			if(uip_len > 0)
				tapdev_send();							//��Ҫ��������,��ͨ��tapdev_send����	 
		}
	}
	else if(timer_expired(&periodic_timer))				//0.5�붨ʱ����ʱ
	{
		timer_reset(&periodic_timer);					//��λ0.5�붨ʱ�� 
		
		//��������ÿ��TCP����, UIP_CONNSȱʡ��40��  
		for(i = 0; i < UIP_CONNS; i++)
		{
			 uip_periodic(i);							//����TCPͨ���¼�
			
	 		//������ĺ���ִ�к������Ҫ�������ݣ���ȫ�ֱ���uip_len>0
			//��Ҫ���͵�������uip_buf, ������uip_len (����2��ȫ�ֱ���)
	 		if(uip_len > 0)
			{
				uip_arp_out();							//����̫��ͷ�ṹ������������ʱ����Ҫ����ARP����
				tapdev_send();							//�������ݵ���̫��
			}
		}
		
#if UIP_UDP	//UIP_UDP 
		//��������ÿ��UDP����, UIP_UDP_CONNSȱʡ��10��
		for(i = 0; i < UIP_UDP_CONNS; i++)
		{
			uip_udp_periodic(i);						//����UDPͨ���¼�
			
	 		//������ĺ���ִ�к������Ҫ�������ݣ���ȫ�ֱ���uip_len>0
			//��Ҫ���͵�������uip_buf, ������uip_len (����2��ȫ�ֱ���)
			if(uip_len > 0)
			{
				uip_arp_out();							//����̫��ͷ�ṹ������������ʱ����Ҫ����ARP����
				tapdev_send();							//�������ݵ���̫��
			}
		}
#endif 
		//ÿ��10�����1��ARP��ʱ������ ���ڶ���ARP����,ARP��10�����һ�Σ��ɵ���Ŀ�ᱻ����
		if(timer_expired(&arp_timer))
		{
			timer_reset(&arp_timer);
			uip_arp_timer();
		}
	}
}

void EEP_Dat_Init(void)
{
	u8 loop ;
	u8 temp[6]; 
	AT24CXX_Init();
	modbus.serial_Num[0] = AT24CXX_ReadOneByte(EEP_SERIALNUMBER_LOWORD);
	modbus.serial_Num[1] = AT24CXX_ReadOneByte(EEP_SERIALNUMBER_LOWORD+1);
	modbus.serial_Num[2] = AT24CXX_ReadOneByte(EEP_SERIALNUMBER_HIWORD);
	modbus.serial_Num[3] = AT24CXX_ReadOneByte(EEP_SERIALNUMBER_HIWORD+1);

	if((modbus.serial_Num[0]==0xff)&&(modbus.serial_Num[1]== 0xff)&&(modbus.serial_Num[2] == 0xff)&&(modbus.serial_Num[3] == 0xff))
	{
		modbus.serial_Num[0] = 1 ;
		modbus.serial_Num[1] = 1 ;
		modbus.serial_Num[2] = 2 ;
		modbus.serial_Num[3] = 2 ;
		AT24CXX_WriteOneByte(EEP_SERIALNUMBER_LOWORD, modbus.serial_Num[0]);
		AT24CXX_WriteOneByte(EEP_SERIALNUMBER_LOWORD+1, modbus.serial_Num[1]);
		AT24CXX_WriteOneByte(EEP_SERIALNUMBER_LOWORD+2, modbus.serial_Num[2]);
		AT24CXX_WriteOneByte(EEP_SERIALNUMBER_LOWORD+3, modbus.serial_Num[3]);
	}

	AT24CXX_WriteOneByte(EEP_VERSION_NUMBER_LO, SOFTREV&0XFF);
	AT24CXX_WriteOneByte(EEP_VERSION_NUMBER_HI, (SOFTREV>>8)&0XFF);
	modbus.address = AT24CXX_ReadOneByte(EEP_ADDRESS);
	if((modbus.address == 255) || (modbus.address == 0))
	{
		modbus.address = 254;
		AT24CXX_WriteOneByte(EEP_ADDRESS, modbus.address);
	}
//	modbus.product = AT24CXX_ReadOneByte(EEP_PRODUCT_MODEL);
//	if((modbus.product == 255)||(modbus.product == 0))
//	{
//		modbus.product = PRODUCT_ID ;
//		AT24CXX_WriteOneByte(EEP_PRODUCT_MODEL, modbus.product);
//	}
	modbus.product = PRODUCT_ID ;
	modbus.hardware_Rev = AT24CXX_ReadOneByte(EEP_HARDWARE_REV);
	if((modbus.hardware_Rev == 255)||(modbus.hardware_Rev == 0))
	{
		modbus.hardware_Rev = HW_VER ;
		AT24CXX_WriteOneByte(EEP_HARDWARE_REV, modbus.hardware_Rev);
	}
	modbus.update = AT24CXX_ReadOneByte(EEP_UPDATE_STATUS);
	modbus.SNWriteflag = AT24CXX_ReadOneByte(EEP_SERIALNUMBER_WRITE_FLAG);
	
	modbus.baud = AT24CXX_ReadOneByte(EEP_BAUDRATE);
	if(modbus.baud > 4) 
	{	
		modbus.baud = 1;
		AT24CXX_WriteOneByte(EEP_BAUDRATE, modbus.baud);
	}
	
	modbus.baud = 1;
	switch(modbus.baud)
	{
		case 0:
			modbus.baudrate = BAUDRATE_9600;
			uart1_init(BAUDRATE_9600);
			SERIAL_RECEIVE_TIMEOUT = 6;
		break ;
		case 1:
			modbus.baudrate = BAUDRATE_19200;
			uart1_init(BAUDRATE_19200);	
			SERIAL_RECEIVE_TIMEOUT = 3;
		break;
		case 2:
			modbus.baudrate = BAUDRATE_38400;
			uart1_init(BAUDRATE_38400);
			SERIAL_RECEIVE_TIMEOUT = 2;
		break;
		case 3:
			modbus.baudrate = BAUDRATE_57600;
			uart1_init(BAUDRATE_57600);	
			SERIAL_RECEIVE_TIMEOUT = 1;
		break;
		case 4:
			modbus.baudrate = BAUDRATE_115200;
			uart1_init(BAUDRATE_115200);
			SERIAL_RECEIVE_TIMEOUT = 1;
		break;
		default:
			modbus.baud = 4;
			modbus.baudrate = BAUDRATE_115200;
			uart1_init(BAUDRATE_115200);
			SERIAL_RECEIVE_TIMEOUT = 1;
		break ;				
	}
	
	for(loop = 0 ; loop<6; loop++)
	{
		temp[loop] = AT24CXX_ReadOneByte(EEP_MAC_ADDRESS_1+loop); 
	}
	
	if((temp[0]== 0xff)&&(temp[1]== 0xff)&&(temp[2]== 0xff)&&(temp[3]== 0xff)&&(temp[4]== 0xff)&&(temp[5]== 0xff) )
	{
		temp[0] = 0x04 ;
		temp[1] = 0x02 ;
		temp[2] = 0x35 ;
		temp[3] = 0xaF ;
		temp[4] = 0x00 ;
		temp[5] = 0x01 ;
		AT24CXX_WriteOneByte(EEP_MAC_ADDRESS_1, temp[0]);
		AT24CXX_WriteOneByte(EEP_MAC_ADDRESS_2, temp[1]);
		AT24CXX_WriteOneByte(EEP_MAC_ADDRESS_3, temp[2]);
		AT24CXX_WriteOneByte(EEP_MAC_ADDRESS_4, temp[3]);
		AT24CXX_WriteOneByte(EEP_MAC_ADDRESS_5, temp[4]);
		AT24CXX_WriteOneByte(EEP_MAC_ADDRESS_6, temp[5]);		
	}
	for(loop =0; loop<6; loop++)
	{
		modbus.mac_addr[loop] =  temp[loop]	;
	}
	
	for(loop = 0 ; loop<4; loop++)
	{
		temp[loop] = AT24CXX_ReadOneByte(EEP_IP_ADDRESS_1+loop); 
	}
	if((temp[0]== 0xff)&&(temp[1]== 0xff)&&(temp[2]== 0xff)&&(temp[3]== 0xff) )
	{
		temp[0] = 192 ;
		temp[1] = 168 ;
		temp[2] = 0 ;
		temp[3] = 183 ;
		AT24CXX_WriteOneByte(EEP_IP_ADDRESS_1, temp[0]);
		AT24CXX_WriteOneByte(EEP_IP_ADDRESS_2, temp[1]);
		AT24CXX_WriteOneByte(EEP_IP_ADDRESS_3, temp[2]);
		AT24CXX_WriteOneByte(EEP_IP_ADDRESS_4, temp[3]);
	}
	for(loop = 0 ; loop<4; loop++)
	{
		modbus.ip_addr[loop] = 	temp[loop] ;
		modbus.ghost_ip_addr[loop] = modbus.ip_addr[loop];
	}
	
	temp[0] = AT24CXX_ReadOneByte(EEP_IP_MODE);
	if(temp[0] > 0)
	{
		temp[0] = 1;
		AT24CXX_WriteOneByte(EEP_IP_MODE, temp[0]);	
	}
	modbus.ip_mode = temp[0];
	modbus.ip_mode = 0;//////////////////////////////////////
	modbus.ghost_ip_mode = modbus.ip_mode;
	
	for(loop = 0 ; loop<4; loop++)
	{
		temp[loop] = AT24CXX_ReadOneByte(EEP_SUB_MASK_ADDRESS_1+loop); 
	}
	if((temp[0]== 0xff)&&(temp[1]== 0xff)&&(temp[2]== 0xff)&&(temp[3]== 0xff) )
	{
		temp[0] = 0xff ;
		temp[1] = 0xff ;
		temp[2] = 0xff ;
		temp[3] = 0 ;
		AT24CXX_WriteOneByte(EEP_SUB_MASK_ADDRESS_1, temp[0]);
		AT24CXX_WriteOneByte(EEP_SUB_MASK_ADDRESS_2, temp[1]);
		AT24CXX_WriteOneByte(EEP_SUB_MASK_ADDRESS_3, temp[2]);
		AT24CXX_WriteOneByte(EEP_SUB_MASK_ADDRESS_4, temp[3]);
	
	}				
	for(loop = 0 ; loop<4; loop++)
	{
		modbus.mask_addr[loop] = 	temp[loop] ;
		modbus.ghost_mask_addr[loop] = modbus.mask_addr[loop] ;
	}
	
	for(loop = 0 ; loop<4; loop++)
	{
		temp[loop] = AT24CXX_ReadOneByte(EEP_GATEWAY_ADDRESS_1+loop); 
	}
	if((temp[0]== 0xff)&&(temp[1]== 0xff)&&(temp[2]== 0xff)&&(temp[3]== 0xff) )
	{
		temp[0] = 192 ;
		temp[1] = 168 ;
		temp[2] = 0 ;
		temp[3] = 4 ;
		AT24CXX_WriteOneByte(EEP_GATEWAY_ADDRESS_1, temp[0]);
		AT24CXX_WriteOneByte(EEP_GATEWAY_ADDRESS_2, temp[1]);
		AT24CXX_WriteOneByte(EEP_GATEWAY_ADDRESS_3, temp[2]);
		AT24CXX_WriteOneByte(EEP_GATEWAY_ADDRESS_4, temp[3]);
	
	}				
	for(loop = 0 ; loop<4; loop++)
	{
		modbus.gate_addr[loop] = 	temp[loop] ;
		modbus.ghost_gate_addr[loop] = modbus.gate_addr[loop] ;
	}
	
	temp[0] = AT24CXX_ReadOneByte(EEP_TCP_SERVER);
	if(temp[0] == 0xff)
	{
		temp[0] = 0 ;
		AT24CXX_WriteOneByte(EEP_TCP_SERVER, temp[0]);
	}
	modbus.tcp_server = temp[0];
	modbus.ghost_tcp_server = modbus.tcp_server  ;
	
	temp[0] =AT24CXX_ReadOneByte(EEP_LISTEN_PORT_HI);
	temp[1] =AT24CXX_ReadOneByte(EEP_LISTEN_PORT_LO);
	if(temp[0] == 0xff && temp[1] == 0xff )
	{
		modbus.listen_port = 502 ;
		temp[0] = (modbus.listen_port>>8)&0xff ;
		temp[1] = modbus.listen_port&0xff ;				
	}
	modbus.listen_port = (temp[0]<<8)|temp[1] ;
	modbus.ghost_listen_port = modbus.listen_port ;
	
	modbus.write_ghost_system = 0 ;
	modbus.reset = 0 ;

	// restore the vrms&irms calibration
	//phrase A
	/////////VRMS
	temp[0] = AT24CXX_ReadOneByte(EEP_VRMS_CAL0_VALUE_BYTE_0);
	temp[1] = AT24CXX_ReadOneByte(EEP_VRMS_CAL0_VALUE_BYTE_1);
	vrms_cal[PHASE_A].value = (temp[0] << 8) | temp[1];
	
	temp[0] = AT24CXX_ReadOneByte(EEP_VRMS_CAL0_ADC_BYTE_0);
	temp[1] = AT24CXX_ReadOneByte(EEP_VRMS_CAL0_ADC_BYTE_1);
	temp[2] = AT24CXX_ReadOneByte(EEP_VRMS_CAL0_ADC_BYTE_2);
	temp[3] = AT24CXX_ReadOneByte(EEP_VRMS_CAL0_ADC_BYTE_3);
	vrms_cal[PHASE_A].adc = (temp[0] << 24) | (temp[1] << 16) | (temp[2] << 8) | temp[3];
	
	temp[0] = AT24CXX_ReadOneByte(EEP_VRMS_CAL0_SLOPE_BYTE_0);
	temp[1] = AT24CXX_ReadOneByte(EEP_VRMS_CAL0_SLOPE_BYTE_1);
	temp[2] = AT24CXX_ReadOneByte(EEP_VRMS_CAL0_SLOPE_BYTE_2);
	temp[3] = AT24CXX_ReadOneByte(EEP_VRMS_CAL0_SLOPE_BYTE_3);
	vrms_cal[PHASE_A].k_slope.l = (temp[0] << 24) | (temp[1] << 16) | (temp[2] << 8) | temp[3];
	
	///////////IRMS
	temp[0] = AT24CXX_ReadOneByte(EEP_IRMS_CAL0_VALUE_BYTE_0);
	temp[1] = AT24CXX_ReadOneByte(EEP_IRMS_CAL0_VALUE_BYTE_1);
	irms_cal[PHASE_A].value = (temp[0] << 8) | temp[1];
	
	temp[0] = AT24CXX_ReadOneByte(EEP_IRMS_CAL0_ADC_BYTE_0);
	temp[1] = AT24CXX_ReadOneByte(EEP_IRMS_CAL0_ADC_BYTE_1);
	temp[2] = AT24CXX_ReadOneByte(EEP_IRMS_CAL0_ADC_BYTE_2);
	temp[3] = AT24CXX_ReadOneByte(EEP_IRMS_CAL0_ADC_BYTE_3);
	irms_cal[PHASE_A].adc = (temp[0] << 24) | (temp[1] << 16) | (temp[2] << 8) | temp[3];
	
	temp[0] = AT24CXX_ReadOneByte(EEP_IRMS_CAL0_SLOPE_BYTE_0);
	temp[1] = AT24CXX_ReadOneByte(EEP_IRMS_CAL0_SLOPE_BYTE_1);
	temp[2] = AT24CXX_ReadOneByte(EEP_IRMS_CAL0_SLOPE_BYTE_2);
	temp[3] = AT24CXX_ReadOneByte(EEP_IRMS_CAL0_SLOPE_BYTE_3);
	irms_cal[PHASE_A].k_slope.l = (temp[0] << 24) | (temp[1] << 16) | (temp[2] << 8) | temp[3];



//phrase B
	/////////VRMS
	temp[0] = AT24CXX_ReadOneByte(EEP_VRMS_CAL1_VALUE_BYTE_0);
	temp[1] = AT24CXX_ReadOneByte(EEP_VRMS_CAL1_VALUE_BYTE_1);
	vrms_cal[PHASE_B].value = (temp[0] << 8) | temp[1];
	
	temp[0] = AT24CXX_ReadOneByte(EEP_VRMS_CAL1_ADC_BYTE_0);
	temp[1] = AT24CXX_ReadOneByte(EEP_VRMS_CAL1_ADC_BYTE_1);
	temp[2] = AT24CXX_ReadOneByte(EEP_VRMS_CAL1_ADC_BYTE_2);
	temp[3] = AT24CXX_ReadOneByte(EEP_VRMS_CAL1_ADC_BYTE_3);
	vrms_cal[PHASE_B].adc = (temp[0] << 24) | (temp[1] << 16) | (temp[2] << 8) | temp[3];
	
	temp[0] = AT24CXX_ReadOneByte(EEP_VRMS_CAL1_SLOPE_BYTE_0);
	temp[1] = AT24CXX_ReadOneByte(EEP_VRMS_CAL1_SLOPE_BYTE_1);
	temp[2] = AT24CXX_ReadOneByte(EEP_VRMS_CAL1_SLOPE_BYTE_2);
	temp[3] = AT24CXX_ReadOneByte(EEP_VRMS_CAL1_SLOPE_BYTE_3);
	vrms_cal[PHASE_B].k_slope.l = (temp[0] << 24) | (temp[1] << 16) | (temp[2] << 8) | temp[3];
	
	///////////IRMS
	temp[0] = AT24CXX_ReadOneByte(EEP_IRMS_CAL1_VALUE_BYTE_0);
	temp[1] = AT24CXX_ReadOneByte(EEP_IRMS_CAL1_VALUE_BYTE_1);
	irms_cal[PHASE_B].value = (temp[0] << 8) | temp[1];
	
	temp[0] = AT24CXX_ReadOneByte(EEP_IRMS_CAL1_ADC_BYTE_0);
	temp[1] = AT24CXX_ReadOneByte(EEP_IRMS_CAL1_ADC_BYTE_1);
	temp[2] = AT24CXX_ReadOneByte(EEP_IRMS_CAL1_ADC_BYTE_2);
	temp[3] = AT24CXX_ReadOneByte(EEP_IRMS_CAL1_ADC_BYTE_3);
	irms_cal[PHASE_B].adc = (temp[0] << 24) | (temp[1] << 16) | (temp[2] << 8) | temp[3];
	
	temp[0] = AT24CXX_ReadOneByte(EEP_IRMS_CAL1_SLOPE_BYTE_0);
	temp[1] = AT24CXX_ReadOneByte(EEP_IRMS_CAL1_SLOPE_BYTE_1);
	temp[2] = AT24CXX_ReadOneByte(EEP_IRMS_CAL1_SLOPE_BYTE_2);
	temp[3] = AT24CXX_ReadOneByte(EEP_IRMS_CAL1_SLOPE_BYTE_3);
	irms_cal[PHASE_B].k_slope.l = (temp[0] << 24) | (temp[1] << 16) | (temp[2] << 8) | temp[3];

//phrase C
	/////////VRMS
	temp[0] = AT24CXX_ReadOneByte(EEP_VRMS_CAL2_VALUE_BYTE_0);
	temp[1] = AT24CXX_ReadOneByte(EEP_VRMS_CAL2_VALUE_BYTE_1);
	vrms_cal[PHASE_C].value = (temp[0] << 8) | temp[1];
	
	temp[0] = AT24CXX_ReadOneByte(EEP_VRMS_CAL2_ADC_BYTE_0);
	temp[1] = AT24CXX_ReadOneByte(EEP_VRMS_CAL2_ADC_BYTE_1);
	temp[2] = AT24CXX_ReadOneByte(EEP_VRMS_CAL2_ADC_BYTE_2);
	temp[3] = AT24CXX_ReadOneByte(EEP_VRMS_CAL2_ADC_BYTE_3);
	vrms_cal[PHASE_C].adc = (temp[0] << 24) | (temp[1] << 16) | (temp[2] << 8) | temp[3];
	
	temp[0] = AT24CXX_ReadOneByte(EEP_VRMS_CAL2_SLOPE_BYTE_0);
	temp[1] = AT24CXX_ReadOneByte(EEP_VRMS_CAL2_SLOPE_BYTE_1);
	temp[2] = AT24CXX_ReadOneByte(EEP_VRMS_CAL2_SLOPE_BYTE_2);
	temp[3] = AT24CXX_ReadOneByte(EEP_VRMS_CAL2_SLOPE_BYTE_3);
	vrms_cal[PHASE_C].k_slope.l = (temp[0] << 24) | (temp[1] << 16) | (temp[2] << 8) | temp[3];
	
	///////////IRMS
	temp[0] = AT24CXX_ReadOneByte(EEP_IRMS_CAL2_VALUE_BYTE_0);
	temp[1] = AT24CXX_ReadOneByte(EEP_IRMS_CAL2_VALUE_BYTE_1);
	irms_cal[PHASE_C].value = (temp[0] << 8) | temp[1];
	
	temp[0] = AT24CXX_ReadOneByte(EEP_IRMS_CAL2_ADC_BYTE_0);
	temp[1] = AT24CXX_ReadOneByte(EEP_IRMS_CAL2_ADC_BYTE_1);
	temp[2] = AT24CXX_ReadOneByte(EEP_IRMS_CAL2_ADC_BYTE_2);
	temp[3] = AT24CXX_ReadOneByte(EEP_IRMS_CAL2_ADC_BYTE_3);
	irms_cal[PHASE_C].adc = (temp[0] << 24) | (temp[1] << 16) | (temp[2] << 8) | temp[3];
	
	temp[0] = AT24CXX_ReadOneByte(EEP_IRMS_CAL2_SLOPE_BYTE_0);
	temp[1] = AT24CXX_ReadOneByte(EEP_IRMS_CAL2_SLOPE_BYTE_1);
	temp[2] = AT24CXX_ReadOneByte(EEP_IRMS_CAL2_SLOPE_BYTE_2);
	temp[3] = AT24CXX_ReadOneByte(EEP_IRMS_CAL2_SLOPE_BYTE_3);
	irms_cal[PHASE_C].k_slope.l = (temp[0] << 24) | (temp[1] << 16) | (temp[2] << 8) | temp[3];





	AT24CXX_Read(EEP_TSTAT_NAME1, panelname, 21); 
	
	modbus.protocal= AT24CXX_ReadOneByte(EEP_MODBUS_COM_CONFIG);
	if((modbus.protocal!=MODBUS)&&(modbus.protocal!=BAC_MSTP ))
	{
		modbus.protocal = MODBUS ;
		AT24CXX_WriteOneByte(EEP_MODBUS_COM_CONFIG, modbus.protocal);
	}

}




