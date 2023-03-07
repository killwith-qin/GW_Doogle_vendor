/********************************************************************************************************
 * @file	user_app.h
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
#pragma once

#include "proj/tl_common.h"
#include "vendor/mesh_provision/app.h"
/** @addtogroup user_app
  * @{
  */
  
/** @defgroup user_app
  * @brief Common User Code.
  * @{
  */
#define MAX_PAR_LEN 4

typedef enum Feed_Back_enum
{
FB_INIT_STATE,
NEED_FEEDBACK,
ALREADY_GET_FEEDBACK,
OTHER_ACTION,
MAX_FEEDBACK_INDEX
}Feed_back_status;

typedef struct User_Beacon_tag
{
    u8 Fixed_Format[3];
    u16 cmd;
    u8 len;
    u8 par[MAX_PAR_LEN];
    Feed_back_status feedback;
}User_Beacon_ST;

typedef enum GW_Role_tag
{
 GW_INIT,
 GW_ACTIVE,
 GW_PASSIVE

}GW_Role_enum;

// --------- function  ----------------
void cb_user_factory_reset_additional();
void cb_user_proc_led_onoff_driver(int on);

extern unsigned char Mesh_GW_MacID[6];
extern u8 Need_Send_ADV_CMD;
extern u8 Need_Send_Mesh_CMD;
extern GW_Role_enum GW_Role;
extern User_Beacon_ST user_beacon_send_ADV;
extern User_Beacon_ST Get_ADV_Message;
/**
  * @}
  */

/**
  * @}
  */


