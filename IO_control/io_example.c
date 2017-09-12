#include "modbus.h"
#include "ud_str.h"
#include "controls.h"
//#include "bacnet.h"
//#include "inputs.h"
uint16_t output_raw[MAX_OUTS]={0} ;
uint16_t input_raw[MAX_INS]={0} ;
U16_T far Test[50];
// ����input_type[point]������Ӧ��Ӳ��
void Set_Input_Type(uint8_t point)
{
	
	
	
}


// ����input��ԭʼADCֵ��10bit�� �������10λ����Ҫ����Ӧת��
uint16_t get_input_raw(uint8_t point)
{
	return (input_raw[point]>>2);
}

// ����input��ԭֵ
// if digital output, 0 - off, 1000 - on
// if analog ouput, 0 - 10v ��Ӧ 0-1023
 void set_output_raw(uint8_t point,uint16_t value) 
{
	output_raw[point] = value;
}

uint16_t get_output_raw(uint8_t point)
{
	return output_raw[point];
}
// ��adcֵת����5v��range��ʾ
// �����input����ģ�鲻��Ҫ�޸�
uint32_t conver_by_unit_5v(uint32_t sample)
{	
	return sample =  ( sample * 5000L ) >> 10;		
}


// ��adcֵת����10v��range��ʾ
// �����input����ģ�鲻��Ҫ�޸�
uint32_t conver_by_unit_10v(uint32_t sample)
{
	return  ( sample * 10000L ) >> 10;

}

// ��adcֵת����customer table��range��ʾ
// �����input����ģ�鲻��Ҫ�޸�
uint32_t conver_by_unit_custable(uint8_t point,uint32_t sample)
{	
	if(input_type[point] == INPUT_V0_5)
	{
			return  ( sample * 50L ) >> 10;		
	}
	else if(input_type[point] == INPUT_I0_20ma)
	{
		return ( 20000L * sample ) >> 10; 
	}
	else if(input_type[point] == INPUT_0_10V)
	{
		return (sample * 10000) >> 10;
	}	
	else if(input_type[point] == INPUT_THERM)
	{
		return get_input_value_by_range( inputs[point].range, sample );
	}
	return 0;
		
}


// �������input��Ŀ
unsigned char get_max_input(void)
{	
	return MAX_INS;
}
// �������output��Ŀ
unsigned char get_max_output(void)
{	
	return MAX_OUTS;
}

 

uint16_t Filter(uint8_t channel,uint16_t input)
{
	 
	u16  uiResult = 0; 
	float temp; 
	static float pre_value[MAX_INS];
	if(channel < MAX_INS)
	{
		temp = pre_value[channel] * inputs[channel].filter;
		pre_value[channel] = (temp + input) / (inputs[channel].filter + 1);  
		uiResult = (u16)pre_value[channel];
	}
	else
	{
		uiResult = (u16)input;
	} 
	return uiResult;	
}


uint8_t get_max_internal_input(void)
{	
	 
	return MAX_INS;
}

uint8_t get_max_internal_output(void)
{	
	 
	return MAX_OUTS;
}



// if��high speed ���ܣ�����high_spd_counter
uint32_t get_high_spd_counter(uint8_t point)
{
//	inputs[point].value = swap_double((high_spd_counter[point] + high_spd_counter_tempbuf[point]) * 1000);
	return 1000;//(high_spd_counter[point] + high_spd_counter_tempbuf[point]) * 1000;
}


void map_extern_output(uint8_t point)
{ 
	
}


S8_T check_external_in_on_line(U8_T index)
{
	 return (S8_T)-1;
}


S8_T check_external_out_on_line(U8_T index)
{
	return (S8_T)-1;
}



