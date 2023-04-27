/********************************************************************************************************
 * @file	user_app.c
 *
 * @brief	for TLSR chips
 *
 * @author	telink
 * @date	Sep. 30, 2010
 *
 * @par     Copyright (c) 2017, Telink Semiconductor (Shanghai) Co., Ltd. ("TELINK")
 *          All rights reserved.
 *
 *          Licensed under the Apache License, Version 2.0 (the "License");
 *          you may not use this file except in compliance with the License.
 *          You may obtain a copy of the License at
 *
 *              http://www.apache.org/licenses/LICENSE-2.0
 *
 *          Unless required by applicable law or agreed to in writing, software
 *          distributed under the License is distributed on an "AS IS" BASIS,
 *          WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *          See the License for the specific language governing permissions and
 *          limitations under the License.
 *
 *******************************************************************************************************/
#include "proj/tl_common.h"
#if !WIN32
#include "proj/mcu/watchdog_i.h"
#include "proj_lib/mesh_crypto/mesh_md5.h"
#include "vendor/common/myprintf.h"
#endif 
#include "proj_lib/ble/ll/ll.h"
#include "proj_lib/ble/blt_config.h"
#include "vendor/common/user_config.h"
#include "proj_lib/ble/service/ble_ll_ota.h"
#include "vendor/common/app_health.h"
#include "proj_lib/sig_mesh/app_mesh.h"
#include "vendor/common/app_provison.h"
#include "vendor/common/lighting_model.h"
#include "vendor/common/sensors_model.h"
#include "vendor/common/remote_prov.h"
#include "proj_lib/mesh_crypto/sha256_telink.h"
#include "proj_lib/mesh_crypto/le_crypto.h"
#include "vendor/common/lighting_model_LC.h"
#include "vendor/common/mesh_ota.h"
#include "vendor/common/mesh_common.h"
#include "vendor/common/mesh_config.h"
#include "vendor/common/directed_forwarding.h"
#include "vendor/common/certify_base/certify_base_crypto.h"
#include "vendor/user_app/user_app.h"

#if(__TL_LIB_8258__ || (MCU_CORE_TYPE == MCU_CORE_8258))
#include "stack/ble/ble.h"
#elif(MCU_CORE_TYPE == MCU_CORE_8278)
#include "stack/ble_8278/ble.h"
#endif

#if FAST_PROVISION_ENABLE
#include "vendor/common/fast_provision_model.h"
#endif

#if (HCI_ACCESS==HCI_USE_UART)
#include "proj/drivers/uart.h"
#endif



void cb_user_factory_reset_additional()
{
    // TODO
}

void cb_user_proc_led_onoff_driver(int on)
{
    // TODO
}

unsigned char Mesh_GW_MacID[6] = {0x20,0x19,0x11,0x22,0xFF,0x12};
u8 pre_provision_working_flag = 0xFF;
u8 provision_working_flag = 0xFF;
u16 Need_Send_ADV_CMD = NO_USE_COMMAND;
u8 Need_Send_Mesh_CMD = 0;
GW_Role_enum GW_Role= {GW_INIT};
User_Beacon_ST user_beacon_send_ADV=
{
      {0x02, 0x01, 0x06},
0xFFFF,
0xFF,
{0xFF,0xFF,0xFF,0xFF},
FB_INIT_STATE
};

User_Beacon_ST Get_ADV_Message=
{
     {0x02, 0x01, 0x06},
0xFFFF,
0xFF,
{0xFF,0xFF,0xFF,0xFF},
FB_INIT_STATE
};


User_Beacon_ST Unfilter_ADV_Message = 
{
     {0x02, 0x01, 0x06},
0xFFFF,
0xFF,
{0xFF,0xFF,0xFF,0xFF},
FB_INIT_STATE
};

const User_Beacon_ST Init_ADV_Message = 
{
     {0x02, 0x01, 0x06},
0xFFFF,
0xFF,
{0xFF,0xFF,0xFF,0xFF},
FB_INIT_STATE
};

GW_CONNECT_STATE GW_Connect_State_Var = GW_CONNECT_IDEL;

user_tx_cmd_info_store user_tx_cmd_info ={
    NULL,
    0,
    0,
    1,
};

u8 gen_onoff_cmd = 0xFF;
u8 last_gen_onoff_cmd = 0xFF;

u32 PARK_CMD_SEND_TIME = 0;
u32 PARK_CMD_RECEIVE_TIME = 0;


/*Qin Wei function*/
#define LED_SIGAL_SEND_PIN    (GPIO_PD4)        //register GPIO output
#define SIGNAL_IO_REG_ADDR    (0x583+((LED_SIGAL_SEND_PIN>>8)<<3)) //register GPIO output

#define SINGAL_OUT_LOW      ( write_reg8 (read_reg8(SIGNAL_IO_REG_ADDR) & ( ~(LED_SIGAL_SEND_PIN & 0xff)  ) ) )

#define SINGAL_OUT_HIGH     ( write_reg8 (read_reg8(SIGNAL_IO_REG_ADDR) | ( LED_SIGAL_SEND_PIN & 0xff)) )



/* Put it into a function independently, to prevent the compiler from 
 * optimizing different pins, resulting in inaccurate baud rates.
 */
_attribute_ram_code_ 
_attribute_no_inline_ 
static void Sned_24bit_Signal(u8 *bit)
{
	int j;
        u8 tmp_out_low  = read_reg8(SIGNAL_IO_REG_ADDR) & (~(LED_SIGAL_SEND_PIN & 0xff));
	    u8 tmp_out_high = read_reg8(SIGNAL_IO_REG_ADDR) | (LED_SIGAL_SEND_PIN & 0xff);
	/*! Make sure the following loop instruction starts at 4-byte alignment: (which is destination address of "tjne") */
	// _ASM_NOP_; 
	for(j = 0;j<24;j++) 
	{
		 if(bit[j]!=0)
		 {
            write_reg8(SIGNAL_IO_REG_ADDR,tmp_out_high);
			CLOCK_DLY_10_CYC;CLOCK_DLY_10_CYC;
			CLOCK_DLY_8_CYC;CLOCK_DLY_8_CYC;
            write_reg8(SIGNAL_IO_REG_ADDR,tmp_out_low);
			CLOCK_DLY_8_CYC;CLOCK_DLY_8_CYC;
		 }
		 else
		 {
            write_reg8(SIGNAL_IO_REG_ADDR,tmp_out_high);
            CLOCK_DLY_8_CYC;CLOCK_DLY_8_CYC;
			write_reg8(SIGNAL_IO_REG_ADDR,tmp_out_low);
			CLOCK_DLY_10_CYC;CLOCK_DLY_10_CYC;
			CLOCK_DLY_8_CYC;CLOCK_DLY_8_CYC;
		 }
	}
}

/*
_attribute_ram_code_ static void Sned_Reset_Signal(void)
{

        u8 tmp_out_low  = read_reg8(SIGNAL_IO_REG_ADDR) & (~(LED_SIGAL_SEND_PIN & 0xff));
	    
        write_reg8(SIGNAL_IO_REG_ADDR,tmp_out_low);
		sleep_us(50);
}
*/


/**
 * @brief  Send a byte of serial data.
 * @param  byte: Data to send.
 * @retval None
 */
_attribute_ram_code_ static void Send_One_Ctr_Signal(u32 One_Signal)   //0x00 XX XX XX 24bit valid
{
	int i;
	//volatile u32 pcTxReg = SIGNAL_IO_REG_ADDR;//register GPIO output
	u8 bit[24] = {0};
	
	for(i=0;i<24;i++)
	{
        bit[i] = ( (One_Signal>>i) & 0x01)? 0x01 : 0x00;
	}
	
	/*! Minimize the time for interrupts to close and ensure timely 
	    response after interrupts occur. */
	u8 r = 0;
	r = irq_disable();
	    Sned_24bit_Signal(bit);
	irq_restore(r);
	 
}

/*ToDo: the function need to be test
_attribute_ram_code_ static void Send_Multiple_Ctr_Signal(u32 *Ctr_Signal, U32 len)    
{
	int i,j;
	u32 len_mod;
	U32 len_int;
    U32 Send_Count;
	

	len_mod =  len%3;
	len_int =  len/3 + 1;
	Send_Count =  (len_mod != 0) ? (len_int-1) : (len_int);


	for(j=0;j<Send_Count;j++)
	{
	    for(i=j;i<3;i++)
	    {
		    Send_One_Ctr_Signal(Ctr_Signal[i]);
	    }
		
		Sned_Reset_Signal();
	}

    for(i=0;i<len_mod;i++)
    {
        Send_One_Ctr_Signal(Ctr_Signal[i]);
    }
 
}
*/


/*Qin Wei function*/

#define RUNNING_REPORT_TIME    (10000 * 1000)  //2000MS

#define LED_YELLOW_FLASH_TIME  (500 * 1000)  //500MS

#define LED_YELLOW_INIT_TIME  (3000 * 1000)  //3S

#define LED_YELLOW_PASSIVE_TIME  (1000 * 1000)  //1S

#define LED_YELLOW_ACTIVE_TIME  (250 * 1000)  //250MS

void User_General_Running_Function(void)
{
	static u32 Running_Report_Start_Tick = 0;
	static u32 Led_Yellow_Flash_Start_Tick = 0;
	if( clock_time_exceed(Running_Report_Start_Tick, RUNNING_REPORT_TIME))
	{
		Running_Report_Start_Tick = clock_time();
		LOG_USER_MSG_INFO(0, 0, "Program is Running!", 0);
	}

	switch(GW_Role)
        {
            case GW_INIT:
                if( clock_time_exceed(Led_Yellow_Flash_Start_Tick, LED_YELLOW_INIT_TIME))
	            {
		            Led_Yellow_Flash_Start_Tick = clock_time();
		            gpio_toggle(GPIO_PA4);
	            }
			break;

            case GW_PASSIVE:
			    if( clock_time_exceed(Led_Yellow_Flash_Start_Tick, LED_YELLOW_PASSIVE_TIME))
                {
                    Led_Yellow_Flash_Start_Tick = clock_time();
		            gpio_toggle(GPIO_PA4);
                }

            break;

            case GW_ACTIVE:
			    if( clock_time_exceed(Led_Yellow_Flash_Start_Tick, LED_YELLOW_ACTIVE_TIME))
                {
                    Led_Yellow_Flash_Start_Tick = clock_time();
		            gpio_toggle(GPIO_PA4);
                }
            break;
		}
	
}




#define MAX_ALLOW_CONNECT_TINE    (10000 * 1000)  //10s


#define AS_ACTIVE_ROLE_TIME  (5000 * 1000)  //5s

u8 REQUEST_AS_PASSIVE = 0;
u8 SWITCH_STATE_FLAG = 0;
void cb_User_SW_Function(void)
{
    static u32 SW1_Press_start_time = 0;
    static u32 SW2_Press_start_time = 0;
	static u8 SW1_Last_State = 0;
	static u8 SW2_Last_State = 0; 
	u8 SW1_Current_State=0;
	u8 SW2_Current_State=0;
	SW1_Current_State = gpio_read(SW1_GPIO);
	SW2_Current_State = gpio_read(SW2_GPIO);
	if(SW1_Current_State != SW1_Last_State)
	{
		SW1_Last_State = SW1_Current_State;
		if( (SW1_Current_State & BIT(6)) != 0)
		{
			LOG_USER_MSG_INFO(&SW1_Current_State, 1, "Button_1 CLOSE State: ", 0);
			SWITCH_STATE_FLAG = 0;
		}
		else
		{
			LOG_USER_MSG_INFO(&SW1_Current_State, 1, "Button_1 OPEN State: ", 0);
			SW1_Press_start_time = clock_time();
			SWITCH_STATE_FLAG = 1;
		}
	}

	if(SW2_Current_State != SW2_Last_State)
	{
		SW2_Last_State = SW2_Current_State;
		if( (SW2_Current_State & BIT(5)) != 0)
		{
			LOG_USER_MSG_INFO(&SW2_Current_State, 1, "Button_2 CLOSE State: ", 0);
		}
		else
		{
            LOG_USER_MSG_INFO(&SW2_Current_State, 1, "Button_2 OPEN State: ", 0);
            SW2_Press_start_time = clock_time();
		}
	}

	if( (SW1_Current_State & BIT(6)) != 0  || (SWITCH_STATE_FLAG == 0))
	{
            
	}
	else
	{
		//press 5s GW switch mode
		switch(GW_Role)
        {
            case GW_INIT:
                if(clock_time_exceed(SW1_Press_start_time, AS_ACTIVE_ROLE_TIME))
                {
                    LOG_USER_MSG_INFO(0, 0, "The GW CHANGE INIT ROLE TO ACTIVE ROLE!", 0);
					SWITCH_STATE_FLAG = 0;
                    GW_Role = GW_ACTIVE;
                }
			break;

            case GW_PASSIVE:
			    if(clock_time_exceed(SW1_Press_start_time, AS_ACTIVE_ROLE_TIME))
                {
                    LOG_USER_MSG_INFO(0, 0, "The GW CHANGE FROM GW_PASSIVE TO INIT ROLE!", 0);
					SWITCH_STATE_FLAG = 0;
                    GW_Role = GW_INIT;
                }

            break;

            case GW_ACTIVE:
			    if(clock_time_exceed(SW1_Press_start_time, AS_ACTIVE_ROLE_TIME))
                {
                    LOG_USER_MSG_INFO(0, 0, "The GW CHANGE FROM GW_ACTIVE TO INIT ROLE!", 0);
					SWITCH_STATE_FLAG = 0;
                    GW_Role = GW_INIT;
                }

            break;
		}
	}

	if( (SW2_Current_State & BIT(5)) != 0)
	{
        REQUEST_AS_PASSIVE = 0;
	}
	else
	{
		switch(GW_Role)
        {
            case GW_ACTIVE:
                
				break;

            case GW_PASSIVE:

            break;

            case GW_INIT:
			    if(clock_time_exceed(SW2_Press_start_time, AS_ACTIVE_ROLE_TIME))
                {
                    LOG_USER_MSG_INFO(0, 0, "The GW REQUEST TO AS PASSIVE ROLE", 0);
                    REQUEST_AS_PASSIVE = 1;
                }
            break;
		}

	}
}

void cb_User_Init_info(void)
{
	static u8 Need_User_Init_Info = 0;
	//u8 device_name[] = DEV_NAME;
	
	if(Need_User_Init_Info == 0)
	{
		Need_User_Init_Info = 1;
		//bls_att_setDeviceName(device_name, sizeof(DEV_NAME));
	}
	
}
void cb_User_Init_Hardware(void)
{
	static u8 Need_User_Init_HW = 0;
	if(Need_User_Init_HW == 0)
	{
        timer2_set_mode(TIMER_MODE_SYSCLK,0,48*1000*100);// 100*1000 * 48* (1/48Mhz)=100ms
		//timer2_set_mode(TIMER_MODE_SYSCLK,0,48*1000*100);// 100*1000 * 48* (1/48Mhz)=100ms
		//timer_start(TIMER2);
		//timer_start(TIMER2);
		Need_User_Init_HW = 1;
	}

}

typedef struct User_Para_for_OP{

               u8 Value;

}User_OP_Para;



#define USER_ADV_PACKET_CYCLE_TIME  (4000 * 1000)  //4S

void User_Test_ADV_Paekct_Send(void)
{
	static u32 User_Adv_Pacekt_Start_Tick = 0;
	static u8 Switch_To_ON = 0;
	User_OP_Para par;
	int Send_Success_Flag = 0;

	if( clock_time_exceed(User_Adv_Pacekt_Start_Tick, USER_ADV_PACKET_CYCLE_TIME))
	{
		User_Adv_Pacekt_Start_Tick = clock_time();
		/*
		op: the opcode for the model
		par: the parameter for the model
		par_len :  the length for the parameter
		adr_dst: the adr for the dst (can be group adr or the unicast adr)
		rsp_max: the rsp cnt for the cmd ,if the adr_dst is unicast adr ,it will be the 1,if it's the group adr
				 he cnt will be the cnt you need .
		Descryption: for the example of the cmd sending part for the light on and other model operation ,can
					see the interface in the cmd_interface.h
		*/
		if(Switch_To_ON != 0)
		{
			par.Value = 1;
			Switch_To_ON = 0;
			Send_Success_Flag = mesh_tx_cmd2normal_primary(USER_SEND_COMMAND_TEST,(u8 *)&par,sizeof(User_OP_Para),0xFFFF, 0);
		}
		else
		{
			par.Value = 0;
			Switch_To_ON = 1;
			Send_Success_Flag = mesh_tx_cmd2normal_primary(USER_SEND_COMMAND_TEST,(u8 *)&par,sizeof(User_OP_Para),0xFFFF, 0);
		}

		if(Send_Success_Flag == 0)
		{

			LOG_USER_MSG_INFO((u8 *)&Send_Success_Flag, sizeof(Send_Success_Flag), "ADV Send State is Success:", 0);
		}
		else
		{
			Switch_To_ON = !Switch_To_ON;
            par.Value = !par.Value;
			LOG_USER_MSG_INFO((u8 *)&Send_Success_Flag, sizeof(Send_Success_Flag), "ADV Send State is Failed: ", 0);
		}
	}
}

/*
		RED: #00FF00
		ORAGE: #7DFF00
		YELLOW: #FFFF00
		GREEN: #FF0000
		CYAN: #FF00FF
		BULU: #0000FF
		PURPLE: #00FFFF
*/


typedef enum RGB_Color
{
	RED    = 0x0000FF00,
	ORAGE  = 0x007DFF00,
	YELLOW = 0x00FFFF00,
    GREEN  = 0x00FF0000,
    CYAN   = 0x00FF00FF,
    BULU   = 0x000000FF,
    PURPLE = 0x0000FFFF,
    DROWN  = 0x00000000
}Seven_Color;

U32 Ctr_Signal_Array[8] ={RED,ORAGE,YELLOW,GREEN,CYAN,BULU,PURPLE,DROWN};

/*
enum Switch_Color_index
{
	Color_INDEX_0,
	Color_INDEX_1,
	Color_INDEX_2,
	Color_INDEX_3,
	Color_INDEX_4,
	Color_INDEX_5,
	Color_INDEX_6,
	Color_INDEX_7,
	Color_INDEX_8,
	Color_INDEX_9,
	Color_INDEX_10,
	Color_INDEX_11,
	Color_INDEX_12,
	Color_INDEX_MAX
};
*/

#define EVERY_SEND_COUNT 30

void User_Ctr_LED_Function(void)
{
	static u32 User_Send_LED_Signal_Tick = 0;
	static U32 Color_Count = 0;
	int j;
	U8 switch_Led_light;


		//gpio_toggle(GPIO_PB5);
		//gpio_write(GPIO_PB5,1);
    if( clock_time_exceed(User_Send_LED_Signal_Tick, 100*1000))
	{
		User_Send_LED_Signal_Tick = clock_time();
		Color_Count++;
		switch_Led_light = Color_Count%30;

			for(j=0;j<EVERY_SEND_COUNT;j++)
			{
				if(j==switch_Led_light)
				{
					Send_One_Ctr_Signal(RED);
					Send_One_Ctr_Signal(YELLOW);
					Send_One_Ctr_Signal(BULU);
					Send_One_Ctr_Signal(CYAN);
					Send_One_Ctr_Signal(PURPLE);
					Send_One_Ctr_Signal(ORAGE);
					Send_One_Ctr_Signal(GREEN);
				}
				else
				{
					Send_One_Ctr_Signal(DROWN);
				}
				//һ�η�30��ָ��
			}

		/*
		switch(switch_Led_light)
	    {
			case 0:
				for(i=0;i<EVERY_SEND_COUNT;i++)
				{


				}
			break;
			case 1:
				for(i=0;i<EVERY_SEND_COUNT;i++)
                {
					Send_One_Ctr_Signal(Ctr_Signal_Double_Array[1][i]);
				}
			break;
			case 2:
				for(i=0;i<EVERY_SEND_COUNT;i++)
                {
					Send_One_Ctr_Signal(Ctr_Signal_Double_Array[2][i]);
                }
            break;
            case 3:
            	for(i=0;i<EVERY_SEND_COUNT;i++)
            	{
            		Send_One_Ctr_Signal(Ctr_Signal_Double_Array[3][i]);
            	}
			break;
			case 4:
				for(i=0;i<EVERY_SEND_COUNT;i++)
			    {
					Send_One_Ctr_Signal(Ctr_Signal_Double_Array[4][i]);
			    }
			break;
			case 5:
				for(i=0;i<EVERY_SEND_COUNT;i++)
				{
					Send_One_Ctr_Signal(Ctr_Signal_Double_Array[5][i]);
				}
			break;
			case 6:
				for(i=0;i<EVERY_SEND_COUNT;i++)
                {
					Send_One_Ctr_Signal(Ctr_Signal_Double_Array[6][i]);
			    }
			break;
	    }
*/

	    //gpio_toggle(GPIO_PB5);
	}


}

#define MAX_CONNECT_ADV_TIME (500*1000)

void Active_Process_Connect(void)
{
    static u32 Connect_Start_Tick = 0;
    if((Get_ADV_Message.cmd == REQUEST_AS_PASSIVE_CMD) && (Get_ADV_Message.feedback == NEED_FEEDBACK))
	{
		user_beacon_send_ADV.cmd = REQUEST_AS_PASSIVE_CMD;
	    user_beacon_send_ADV.feedback= ALREADY_GET_FEEDBACK;
	    user_beacon_send_ADV.par[0] = 0x08;
	    user_beacon_send_ADV.len = 1; 

		//Send ADV flag
	    Need_Send_ADV_CMD = REQUEST_AS_PASSIVE_CMD;

	    //record inter time
	    Connect_Start_Tick = clock_time();

		Get_ADV_Message.cmd =  NO_USE_COMMAND; //avoid keep entry condition
		Get_ADV_Message.feedback = FB_INIT_STATE;
		LOG_USER_MSG_INFO(0, 0, "Keep in here!", 0);
	}

    if(Need_Send_ADV_CMD == REQUEST_AS_PASSIVE_CMD)
    {
        if( clock_time_exceed(Connect_Start_Tick, MAX_CONNECT_ADV_TIME))
		{
            Need_Send_ADV_CMD = NO_USE_COMMAND;
			LOG_USER_MSG_INFO(0, 0, "reach in here!", 0);
		}
    }

}

#define CMD_ADV_SEND_MAX_TIME  (300 * 1000)  //300mS
    u32 GW_Inter_Start_Tick = 0;


void User_GW_ACTIVE_Process(void)
{
    if(last_gen_onoff_cmd !=  gen_onoff_cmd)
	{
	    LOG_USER_MSG_INFO(0, 0, "Send ADV CMD ", 0);
        last_gen_onoff_cmd = gen_onoff_cmd; //As event
        //Set Send adv data
        user_beacon_send_ADV.cmd =USER_SEND_COMMAND_TEST;
	    user_beacon_send_ADV.feedback=NEED_FEEDBACK;
	    user_beacon_send_ADV.par[0] = gen_onoff_cmd;
	    user_beacon_send_ADV.len = 1;

        //Send ADV flag
	    Need_Send_ADV_CMD = USER_SEND_COMMAND_TEST;

	    //record inter time
	    GW_Inter_Start_Tick = clock_time();
	}
	
// if  gen_onoff_cmd not change, ADV cmd only send MAX_TIME
    if(Need_Send_ADV_CMD == USER_SEND_COMMAND_TEST)
    {
        if( clock_time_exceed(GW_Inter_Start_Tick, CMD_ADV_SEND_MAX_TIME))
		{
            Need_Send_ADV_CMD = NO_USE_COMMAND;
		}
    }


Active_Process_Connect();


/*
	if(Get_ADV_Message.feedback == ALREADY_GET_FEEDBACK)
    {
		Need_Send_ADV_CMD = 0;
		GW_Role = GW_INIT;
        user_beacon_send_ADV.cmd =0x0280;
		LOG_USER_MSG_INFO(0, 0, "CMD is Finish ", 0);

	}
*/   

}

void User_GW_PASSIVE_Process(void)
{
	static u8 Last_Passive_State = 0xFF;
	int gen_onoff_cmd_feedback = 1;
	u8 GW_Passive_Send_CMD = Get_ADV_Message.par[0];
    
    if(Get_ADV_Message.feedback == NEED_FEEDBACK && (Get_ADV_Message.cmd = USER_SEND_COMMAND_TEST)) //Get_ADV_Message can't change,even adv data not receive.
    {
	    //GW_PASSIVE don't need to send ADV data
        user_beacon_send_ADV.feedback = ALREADY_GET_FEEDBACK;
	    user_beacon_send_ADV.cmd =USER_SEND_COMMAND_TEST;
		if(Last_Passive_State != GW_Passive_Send_CMD)
		{
            Need_Send_Mesh_CMD  = 1;			
			GW_Inter_Start_Tick = clock_time();
			LOG_USER_MSG_INFO(0, 0, "ADV CMD is Changed!", 0);
		}
		
        if(Need_Send_Mesh_CMD == 1)
	    {
		    //gen_onoff_cmd_feedback = mesh_tx_cmd2normal_primary(USER_SEND_COMMAND_TEST,(u8 *)&GW_Passive_Send_CMD,1,0xFFFF, 0);
			if(GW_Passive_Send_CMD != 0)
			{
		        gen_onoff_cmd_feedback = access_cmd_onoff(0xffff, 0, G_ON, CMD_NO_ACK, 0);
			}
			else
			{
                gen_onoff_cmd_feedback = access_cmd_onoff(0xffff, 0, G_OFF, CMD_NO_ACK, 0);
			}
		    if(gen_onoff_cmd_feedback != 0)
		    {
                LOG_USER_MSG_INFO((u8 *)&gen_onoff_cmd_feedback, sizeof(gen_onoff_cmd_feedback), "GW_Passive Send CMD is is Failed: ", 0);
		    }
		    else
		    {
                LOG_USER_MSG_INFO((u8 *)&gen_onoff_cmd_feedback, sizeof(gen_onoff_cmd_feedback), " GW_Passive Send CMD is Success: ", 0);
				Last_Passive_State = GW_Passive_Send_CMD;
				Get_ADV_Message.feedback = FB_INIT_STATE; //need active Change feedback, avoid entry this state.
			    Need_Send_Mesh_CMD = 0;
				LOG_USER_MSG_INFO(0, 0, "USER_SEND_COMMAND_TEST is Changed!", 0);
		    }
	    }
	    else
	    {
			Get_ADV_Message.feedback = FB_INIT_STATE; //need active Change feedback, avoid entry this state.
            //LOG_USER_MSG_INFO(0, 0, "No action ", 0);
	    }

	}
}


void User_GW_INIT_Process(void)
{
    if(REQUEST_AS_PASSIVE == 1)
    {
		user_beacon_send_ADV.cmd = REQUEST_AS_PASSIVE_CMD;
	    user_beacon_send_ADV.feedback = NEED_FEEDBACK;
	    user_beacon_send_ADV.par[0] = gen_onoff_cmd;
	    user_beacon_send_ADV.len = 1;

		//Send ADV flag
		if(Need_Send_ADV_CMD == NO_USE_COMMAND)
		{
	        Need_Send_ADV_CMD = REQUEST_AS_PASSIVE_CMD;
		}
		else
		{
            LOG_USER_MSG_INFO((u8*)&Need_Send_ADV_CMD, 2, "ADV SEND FAILED, This ADV NEED SEND: ", 0);
		}

	    if(Get_ADV_Message.feedback == ALREADY_GET_FEEDBACK)
        {
		    Need_Send_ADV_CMD = NO_USE_COMMAND;
			GW_Role =  GW_PASSIVE;
		    LOG_USER_MSG_INFO(0, 0, "AS GW PASSIVE SUCCESS: ", 0); 
	    }
    }
    else
    {
		user_beacon_send_ADV.cmd = GW_INIT_IDLE_CMD;
	    user_beacon_send_ADV.feedback = FB_INIT_STATE;
	    user_beacon_send_ADV.par[0] = 0x01;
	    user_beacon_send_ADV.len = 1;
		Need_Send_ADV_CMD = NO_USE_COMMAND;
	}



}

#define USER_PARK_LOCK_CYCLE_TIME  (1000 * 1000)  //1S

User_OP_Para par;

void cb_Parking_Lock_Function(void)
{
    static u32 User_Park_Lock_Start_Tick = 0;

	int Send_Success_Flag = 0;
	 
	if( clock_time_exceed(User_Park_Lock_Start_Tick, USER_PARK_LOCK_CYCLE_TIME))
	{
		User_Park_Lock_Start_Tick = clock_time();
		if(par.Value != 1)
		{
		    par.Value = 1;
		}
		else
		{
			par.Value = 0;
		}
        Send_Success_Flag = mesh_tx_cmd2normal_primary(PARK_LOCK_SET_NOACK,(u8 *)&par,sizeof(User_OP_Para),0xFFFF, 0);
        if(Send_Success_Flag == 0)
		{
        	PARK_CMD_SEND_TIME = clock_time();
		    LOG_USER_MSG_INFO((u8*)&PARK_CMD_SEND_TIME, 4, "Send time is: ", 0);
		}

		LOG_USER_MSG_INFO((u8 *)&Send_Success_Flag, sizeof(Send_Success_Flag), "Parking Lock Info send", 0);
    }
}

void cb_My_Main_Loop_function(void)
{
	
	
	//MAC,Dirve name.etc
	cb_User_Init_info();
	//GPIO,COMMS;
	cb_User_Init_Hardware();
	//LED falsh Usart print
	User_General_Running_Function();
	//indicate SW Pressed
	cb_User_SW_Function();
	//Send adv packet
	//User_Test_ADV_Paekct_Send();

	//Control LED
	//User_Ctr_LED_Function();


    //GW INIT
	if(GW_Role == GW_INIT) {User_GW_INIT_Process();}
    //GW PASSIVE
	if(GW_Role == GW_PASSIVE) {User_GW_PASSIVE_Process();}
    //GW ACTIVE
    if(GW_Role == GW_ACTIVE)  {User_GW_ACTIVE_Process();}


	//Parking Lock function

	cb_Parking_Lock_Function();

}



