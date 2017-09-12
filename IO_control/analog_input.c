#include "analog_input.h"
#include "bitmap.h"
#include "bacnet.h" 
#include "stm32f10x.h"
#include "stm32f10x_adc.h"
#include "stm32f10x_dma.h"
#include "delay.h"
#include "controls.h"
#define DMA_BUFFER_SIZE			40

/* Private define ------------------------------------------------------------*/
 
#define ADC1_DR_Address    				((u32)0x4001244C)	

__IO uint16_t DMA_Buffer[DMA_BUFFER_SIZE];


 


static void ADC_Config(void)
{

  ADC_InitTypeDef     ADC_InitStructure;
  DMA_InitTypeDef     DMA_InitStructure;
  GPIO_InitTypeDef    GPIO_InitStructure;
  uint8 i;
	
  for(i=0;i<DMA_BUFFER_SIZE;i++)
  {
	 DMA_Buffer[i] = 0;
  }
 
  
	/* Configure ADC Channel 0,1,2,3��4��5  as analog input */
	GPIO_InitStructure.GPIO_Pin =  GPIO_Pin_0| GPIO_Pin_1| GPIO_Pin_2 | GPIO_Pin_3 ;//| GPIO_Pin_4 | GPIO_Pin_5 ;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
	GPIO_Init(GPIOC, &GPIO_InitStructure);
    
//    GPIO_InitStructure.GPIO_Pin =    GPIO_Pin_4  ;
//	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
//	GPIO_Init(GPIOA, &GPIO_InitStructure);
	/* Enable DMA clock */
	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);
 
 
 
	/* DMA1 Channel1 Config */
	DMA_DeInit(DMA1_Channel1);
	DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)ADC1_DR_Address;
	DMA_InitStructure.DMA_MemoryBaseAddr = (uint32_t)DMA_Buffer;
	DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralSRC;
	DMA_InitStructure.DMA_BufferSize = DMA_BUFFER_SIZE;
	DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
	DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
	DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
	DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
	DMA_InitStructure.DMA_Mode = DMA_Mode_Circular;
	DMA_InitStructure.DMA_Priority = DMA_Priority_High;
	DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;
	DMA_Init(DMA1_Channel1, &DMA_InitStructure); 
	/* DMA1 Channel1 enable */
	DMA_Cmd(DMA1_Channel1, ENABLE);



//ADC���� 
	/* ADC1 Periph clock enable */
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);

	/* ADCת��ʱ�䣺 �� STM32F103xx��ǿ�Ͳ�Ʒ��ʱ��Ϊ56MHzʱΪ1��s(ʱ��Ϊ72MHzΪ1.17��s)
	ADC������Χ0-3.3V    */
	RCC_ADCCLKConfig(RCC_PCLK2_Div6);                   //����ADC��ʱ��Ϊ72MHZ/6=12M 

	ADC_InitStructure.ADC_Mode = ADC_Mode_Independent;	//ADC1�����ڶ���ģʽ
	ADC_InitStructure.ADC_ScanConvMode = ENABLE;		//ģ��ת��������ɨ��ģʽ����ͨ�������ǵ��Σ���ͨ����ģʽ
	ADC_InitStructure.ADC_ContinuousConvMode = ENABLE;	//ģ��ת������������ģʽ�����ǵ���ģʽ
	ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;//ת��������������ⲿ��������
	ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;//ADC�����Ҷ���
	ADC_InitStructure.ADC_NbrOfChannel = 4;               //�涨��˳����й���ת����ADCͨ������Ŀ�������Ŀ��ȡֵ��Χ��1��16
	ADC_Init(ADC1, &ADC_InitStructure);
	
	/* ADC1 regular channels configuration [����ģʽͨ������]*/ 

	//ADC1 ����ͨ������
  	ADC_RegularChannelConfig(ADC1, ADC_Channel_10, 1, ADC_SampleTime_55Cycles5);	  //ͨ��11����ʱ�� 55.5����
  	ADC_RegularChannelConfig(ADC1, ADC_Channel_11, 2, ADC_SampleTime_55Cycles5);
	ADC_RegularChannelConfig(ADC1, ADC_Channel_12, 3, ADC_SampleTime_55Cycles5);
	ADC_RegularChannelConfig(ADC1, ADC_Channel_13, 4, ADC_SampleTime_55Cycles5);
//	ADC_RegularChannelConfig(ADC1, ADC_Channel_14, 5, ADC_SampleTime_55Cycles5);
//	ADC_RegularChannelConfig(ADC1, ADC_Channel_15,  6, ADC_SampleTime_55Cycles5);

	//ʹ��ADC1 DMA 
	ADC_DMACmd(ADC1, ENABLE);
	//ʹ��ADC1
	ADC_Cmd(ADC1, ENABLE);	
	
	// ��ʼ��ADC1У׼�Ĵ���
	ADC_ResetCalibration(ADC1);
	//���ADC1У׼�Ĵ�����ʼ���Ƿ����
	while(ADC_GetResetCalibrationStatus(ADC1));
	
	//��ʼУ׼ADC1
	ADC_StartCalibration(ADC1);
	//����Ƿ����У׼
	while(ADC_GetCalibrationStatus(ADC1));
	
	//ADC1ת������
	ADC_SoftwareStartConvCmd(ADC1, ENABLE);	
  
}


 

void input_initial(void)
{
	ADC_Config(); 
}
 
uint16 get_ad_val(uint8 channel)
{
	uint16 read_buf[10],min,max; 
    uint16 datacore = 0; 
    uint8  i; 
	switch (channel)
	{
		case 0:
			for(i=0;i<10;i++)
				read_buf[i] = DMA_Buffer[i * 4 ]; 
			break;
		case 1:
			for(i=0;i<10;i++)
				read_buf[i] = DMA_Buffer[i * 4 + 1]; 
			break;
		case 2:
			for(i=0;i<10;i++)
				read_buf[i] = DMA_Buffer[i * 4 + 2]; 
			break;
		
		case 3:
			for(i=0;i<10;i++)
				read_buf[i] = DMA_Buffer[i * 4 + 3  ]; 
			break;
		 
		default:
			break;
	}
	
	min = read_buf[0];
    max = read_buf[0];
    for(i = 0; i < 10; i++)
    { 
        datacore += read_buf[i];  
		min = min < read_buf[i] ? min : read_buf[i];
		max = max > read_buf[i] ? max : read_buf[i]; 
    }
	
    return(datacore - min - max) / 8;   
}


void vInputTask(void *pvParameters)
{
	uint8 i;
	input_initial();
	for(i = 0;i<MAX_INS;i++) input_raw[i] = 0;
	delay_ms(100);
	for(;;)
	{
		
		for(i=0;i<MAX_INS;i++)
		{
			input_raw[i] = get_ad_val(i);
		}
		control_input();
		delay_ms(500);
	}
}











