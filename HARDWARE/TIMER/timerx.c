#include "timerx.h"
#include "led.h"
#include "modbus.h"
#include "portmacro.h"
extern volatile uint16_t SilenceTime;
//u32 dhcp_run_time = 30000;
//��ʱ��3�жϷ������	 
void TIM3_IRQHandler(void)
{ 
	unsigned portBASE_TYPE uxSavedInterruptStatus;
	uxSavedInterruptStatus = portSET_INTERRUPT_MASK_FROM_ISR();
//	if(TIM3->SR & 0X0001)		//����ж�
	if(TIM_GetFlagStatus(TIM3, TIM_IT_Update) == SET)
	{
		//LED1 = !LED1;			    				   				     	    	
	}
//	TIM3->SR &= ~(1 << 0);		//����жϱ�־λ 
	TIM_ClearFlag(TIM3, TIM_IT_Update);	
	portCLEAR_INTERRUPT_MASK_FROM_ISR(uxSavedInterruptStatus);	
}

//ͨ�ö�ʱ��3�жϳ�ʼ��
//����ʱ��ѡ��ΪAPB1��2������APB1Ϊ36M
//arr���Զ���װֵ��
//psc��ʱ��Ԥ��Ƶ��
//����ʹ�õ��Ƕ�ʱ��3!
void TIM3_Int_Init(u16 arr, u16 psc)
{
	TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
	NVIC_InitTypeDef NVIC_InitStructure;
	
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);
	
	TIM_TimeBaseStructure.TIM_Period = arr;
	TIM_TimeBaseStructure.TIM_Prescaler = psc; 
	TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1; 
	TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_TimeBaseInit(TIM3, &TIM_TimeBaseStructure);
	
	//Timer3 NVIC ����
    NVIC_InitStructure.NVIC_IRQChannel = TIM3_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;	//��ռ���ȼ�0
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 2;			//�����ȼ�2
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;				//IRQͨ��ʹ��
	NVIC_Init(&NVIC_InitStructure);								//����ָ���Ĳ�����ʼ��NVIC�Ĵ���
	
	TIM_ITConfig(TIM3, TIM_IT_Update, DISABLE);
	TIM_Cmd(TIM3, DISABLE);
}

u32 uip_timer = 0;	//uip ��ʱ����ÿ10ms����1.
//��ʱ��6�жϷ������	 
void TIM6_IRQHandler(void)//1ms
{
	unsigned portBASE_TYPE uxSavedInterruptStatus;
	uxSavedInterruptStatus = portSET_INTERRUPT_MASK_FROM_ISR();
	if(TIM_GetFlagStatus(TIM6, TIM_IT_Update) == SET)
	{
		uip_timer++;		//uip��ʱ������1
	}
	if(SilenceTime < 1000)
	{
		//SilenceTime ++ ;
		//miliseclast_cur = miliseclast_cur + SWTIMER_INTERVAL;
		SilenceTime = SilenceTime + SWTIMER_INTERVAL;
	}
	else
	{
		SilenceTime = 0 ;
	}
	TIM_ClearFlag(TIM6, TIM_IT_Update);	
	portCLEAR_INTERRUPT_MASK_FROM_ISR(uxSavedInterruptStatus);		
}

//������ʱ��6�жϳ�ʼ��					  
//arr���Զ���װֵ��		
//psc��ʱ��Ԥ��Ƶ��		 
//Tout= ((arr+1)*(psc+1))/Tclk��
void TIM6_Int_Init(u16 arr, u16 psc)
{
	TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
	NVIC_InitTypeDef NVIC_InitStructure;
	
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM6, ENABLE);
	
	TIM_TimeBaseStructure.TIM_Period = arr;
	TIM_TimeBaseStructure.TIM_Prescaler = psc; 
	TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1; 
	TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_TimeBaseInit(TIM6, &TIM_TimeBaseStructure);
	
	//Timer3 NVIC ����
    NVIC_InitStructure.NVIC_IRQChannel = TIM6_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;	//��ռ���ȼ�3
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 3;			//�����ȼ�3
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;				//IRQͨ��ʹ��
	NVIC_Init(&NVIC_InitStructure);								//����ָ���Ĳ�����ʼ��NVIC�Ĵ���
	
	TIM_ITConfig(TIM6, TIM_IT_Update, ENABLE);
	TIM_Cmd(TIM6, ENABLE);
}
