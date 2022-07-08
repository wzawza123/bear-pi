
/*
 * Copyright (c) 2020 Nanjing Xiaoxiongpai Intelligent Technology Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "ohos_init.h"
#include "cmsis_os2.h"

#include "wifiiot_gpio_ex.h"
#include "wifiiot_errno.h"
#include "wifiiot_adc.h"

#include "wifi_connect.h"
#include "lwip/sockets.h"

#include "oc_mqtt.h"
#include "E53_IA1.h"

#define MSGQUEUE_OBJECTS 16 // number of Message Queue Objects

typedef struct
{ // object data type
    char *Buf;
    uint8_t Idx;
} MSGQUEUE_OBJ_t;

MSGQUEUE_OBJ_t msg;
osMessageQueueId_t mid_MsgQueue; // message queue id

#define CLIENT_ID "61eb6fcade9933029be3f52d_bearpi_test_1_0_0_2022012203"
#define USERNAME "61eb6fcade9933029be3f52d_bearpi_test_1"
#define PASSWORD "a6bb8dd415c1a41b2a4a8e9bbc4190cb8acf71145f4a8494d7cc9b602e0bbdea"
//data type definition
typedef enum
{
    en_msg_cmd = 0,
    en_msg_report,
} en_msg_type_t;

typedef struct
{
    char *request_id;
    char *payload;
} cmd_t;

typedef struct
{
    char ph[10];
    char tds[10];
    char turbidity[10];
    char temp[10];
} report_t;

typedef struct
{
    en_msg_type_t msg_type;
    union
    {
        cmd_t cmd;
        report_t report;
    } msg;
} app_msg_t;

typedef struct
{
    float ph;
    float tds;
    float turbidity;
    float temp;
    int led;
    int motor;
} app_cb_t;
static app_cb_t g_app_cb;

static void deal_report_msg(report_t *report)
{
    oc_mqtt_profile_service_t service;
    oc_mqtt_profile_kv_t ph;
    oc_mqtt_profile_kv_t tds;
    oc_mqtt_profile_kv_t turbidity;
    oc_mqtt_profile_kv_t temp;

    //assembly the report msg
    service.event_time = NULL;

    service.service_id = "water";
    service.service_property = &ph;
    service.nxt = NULL;

    ph.key = "ph";
    ph.value = &report->ph;
    ph.type = EN_OC_MQTT_PROFILE_VALUE_STRING;
    ph.nxt = &tds; //its the last message

    tds.key = "tds";
    tds.value = &report->tds;
    tds.type = EN_OC_MQTT_PROFILE_VALUE_STRING;
    tds.nxt = &turbidity; //its the last message

    turbidity.key = "turbidity";
    turbidity.value = &report->turbidity;
    turbidity.type = EN_OC_MQTT_PROFILE_VALUE_STRING;
    turbidity.nxt = &temp; //its the last message
    
    temp.key = "temperature";
    temp.value = &report->temp;
    temp.type = EN_OC_MQTT_PROFILE_VALUE_STRING;
    temp.nxt = NULL; //its the last message

    oc_mqtt_profile_propertyreport(USERNAME, &service);
    return;
}

void oc_cmd_rsp_cb(uint8_t *recv_data, size_t recv_size, uint8_t **resp_data, size_t *resp_size)
{
    app_msg_t *app_msg;

    int ret = 0;
    app_msg = malloc(sizeof(app_msg_t));
    app_msg->msg_type = en_msg_cmd;
    app_msg->msg.cmd.payload = (char *)recv_data;

    printf("recv data is %.*s\n", recv_size, recv_data);
    ret = osMessageQueuePut(mid_MsgQueue, &app_msg, 0U, 0U);
    if (ret != 0)
    {
        free(recv_data);
    }
    *resp_data = NULL;
    *resp_size = 0;
}

///< COMMAND DEAL
#include <cJSON.h>
static void deal_cmd_msg(cmd_t *cmd)
{
    cJSON *obj_root;
    cJSON *obj_cmdname;
    cJSON *obj_paras;
    cJSON *obj_para;

    int cmdret = 1;
    oc_mqtt_profile_cmdresp_t cmdresp;
    obj_root = cJSON_Parse(cmd->payload);
    if (NULL == obj_root)
    {
        goto EXIT_JSONPARSE;
    }

    obj_cmdname = cJSON_GetObjectItem(obj_root, "command_name");
    if (NULL == obj_cmdname)
    {
        goto EXIT_CMDOBJ;
    }
    if (0 == strcmp(cJSON_GetStringValue(obj_cmdname), "Agriculture_Control_light"))
    {
        obj_paras = cJSON_GetObjectItem(obj_root, "paras");
        if (NULL == obj_paras)
        {
            goto EXIT_OBJPARAS;
        }
        obj_para = cJSON_GetObjectItem(obj_paras, "Light");
        if (NULL == obj_para)
        {
            goto EXIT_OBJPARA;
        }
        ///< operate the LED here
        if (0 == strcmp(cJSON_GetStringValue(obj_para), "ON"))
        {
            g_app_cb.led = 1;
            Light_StatusSet(ON);
            printf("Light On!");
        }
        else
        {
            g_app_cb.led = 0;
            Light_StatusSet(OFF);
            printf("Light Off!");
        }
        cmdret = 0;
    }
    else if (0 == strcmp(cJSON_GetStringValue(obj_cmdname), "Agriculture_Control_Motor"))
    {
        obj_paras = cJSON_GetObjectItem(obj_root, "Paras");
        if (NULL == obj_paras)
        {
            goto EXIT_OBJPARAS;
        }
        obj_para = cJSON_GetObjectItem(obj_paras, "Motor");
        if (NULL == obj_para)
        {
            goto EXIT_OBJPARA;
        }
        ///< operate the Motor here
        if (0 == strcmp(cJSON_GetStringValue(obj_para), "ON"))
        {
            g_app_cb.motor = 1;
            Motor_StatusSet(ON);
            printf("Motor On!");
        }
        else
        {
            g_app_cb.motor = 0;
            Motor_StatusSet(OFF);
            printf("Motor Off!");
        }
        cmdret = 0;
    }

EXIT_OBJPARA:
EXIT_OBJPARAS:
EXIT_CMDOBJ:
    cJSON_Delete(obj_root);
EXIT_JSONPARSE:
    ///< do the response
    cmdresp.paras = NULL;
    cmdresp.request_id = cmd->request_id;
    cmdresp.ret_code = cmdret;
    cmdresp.ret_name = NULL;
    (void)oc_mqtt_profile_cmdresp(NULL, &cmdresp);
    return;
}

static int task_main_entry(void)
{
    app_msg_t *app_msg;

    uint32_t ret = WifiConnect("wifi_312", "87651234"); //connect to the wifi

    device_info_init(CLIENT_ID, USERNAME, PASSWORD);
    oc_mqtt_init();
    oc_set_cmd_rsp_cb(oc_cmd_rsp_cb); //set the callback function

    while (1)
    {
        app_msg = NULL;
        (void)osMessageQueueGet(mid_MsgQueue, (void **)&app_msg, NULL, 0U);
        if (NULL != app_msg)
        {
            switch (app_msg->msg_type)
            {
            case en_msg_cmd:
                deal_cmd_msg(&app_msg->msg.cmd);
                break;
            case en_msg_report:
                deal_report_msg(&app_msg->msg.report);
                break;
            default:
                break;
            }
            free(app_msg);
        }
    }
    return 0;
}
//sensors
static float getPH(void)
{
    float voltage;
    //获取电压值
    unsigned int ret;
    unsigned short data;
    printf("==============================PH==========================\r\n");

    ret = AdcRead(WIFI_IOT_ADC_CHANNEL_0, &data, WIFI_IOT_ADC_EQU_MODEL_8, WIFI_IOT_ADC_CUR_BAIS_DEFAULT, 0xff); //adc channel,read data,average of 8 times,power set to default, timer 0xFF channel1:IO04
    if (ret != WIFI_IOT_SUCCESS)
    {
        printf("ADC Read Fail\n");
    }

    voltage=data * 1.8 * 4 / 4096.0;

    printf("vlt:%.2fV\n", voltage);
    printf("ph:%.2f\n",-5.7514*voltage+16.654);
    return -5.7514*voltage+16.654;
}
static float calTDS(float voltage){
    return (133.42 * voltage * voltage * voltage - 255.86 * voltage * voltage + 857.39 * voltage) * 0.5;
}
static float getTDS(void)
{
    float voltage;
    //获取电压值
    unsigned int ret;
    unsigned short data;
    printf("==============================TDS==========================\r\n");

    ret = AdcRead(WIFI_IOT_ADC_CHANNEL_2, &data, WIFI_IOT_ADC_EQU_MODEL_8, WIFI_IOT_ADC_CUR_BAIS_DEFAULT, 0xff); //adc channel,read data,average of 8 times,power set to default, timer 0xFF channel1:IO04
    if (ret != WIFI_IOT_SUCCESS)
    {
        printf("ADC Read Fail\n");
    }

    voltage=data * 1.8 * 4 / 4096.0;

    printf("vlt:%.2fV\n", voltage);
    printf("tds:%.2f\n",calTDS(voltage));
    return calTDS(voltage);
}
// static float calTurbidity(float voltage){
//     float temp_data=25;
//     float TU_calibration;
//     float TU;
//     float TU_value;
//     float K_Value;
//     TU_calibration=-0.0192*(temp_data-25)+TU;  
//   TU_value=-865.68*TU_calibration + K_Value;
//     return TU_value;
// }
// static float getTurbidity(void)
// {
//     float voltage;
//     //获取电压值
//     unsigned int ret;
//     unsigned short data;
//     printf("==============================turbidity==========================\r\n");

//     ret = AdcRead(WIFI_IOT_ADC_CHANNEL_3, &data, WIFI_IOT_ADC_EQU_MODEL_8, WIFI_IOT_ADC_CUR_BAIS_DEFAULT, 0xff); //adc channel,read data,average of 8 times,power set to default, timer 0xFF channel1:IO04
//     if (ret != WIFI_IOT_SUCCESS)
//     {
//         printf("ADC Read Fail\n");
//     }

//     voltage=data * 1.8 * 4 / 4096.0;

//     printf("vlt:%.2fV\n", voltage);
//     printf("tds:%.2f\n",calTDS(voltage));
//     return calTDS(voltage);
// }
static int task_sensor_entry(void)
{
    app_msg_t *app_msg;
    while (1)
    {
        app_msg = malloc(sizeof(app_msg_t));
        g_app_cb.ph=getPH();
        g_app_cb.tds=getTDS();
        g_app_cb.turbidity=0;
        g_app_cb.temp=0;
        printf("SENSOR:ph:%.2f tds:%.2fppm tur:%.2f temp:%.2f\r\n", g_app_cb.ph,g_app_cb.tds,g_app_cb.turbidity,g_app_cb.temp);
        if (NULL != app_msg)
        {
            app_msg->msg_type = en_msg_report;
            //print the float data into string
            sprintf(app_msg->msg.report.ph,"%.2f\0",g_app_cb.ph);
            sprintf(app_msg->msg.report.tds,"%.2f\0",g_app_cb.tds);
            sprintf(app_msg->msg.report.temp,"%.2f\0",g_app_cb.temp);
            sprintf(app_msg->msg.report.turbidity,"%.2f\0",g_app_cb.turbidity);
            if (0 != osMessageQueuePut(mid_MsgQueue, &app_msg, 0U, 0U))
            {
                free(app_msg);
            }
        }
        sleep(3);
    }
    return 0;
}

static void OC_Demo(void)
{
    mid_MsgQueue = osMessageQueueNew(MSGQUEUE_OBJECTS, 10, NULL);
    if (mid_MsgQueue == NULL)
    {
        printf("Falied to create Message Queue!\n");
    }

    osThreadAttr_t attr;

    attr.name = "task_main_entry";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = 10240;
    attr.priority = 24;

    if (osThreadNew((osThreadFunc_t)task_main_entry, NULL, &attr) == NULL)
    {
        printf("Falied to create task_main_entry!\n");
    }
    attr.stack_size = 2048;
    attr.priority = 25;
    attr.name = "task_sensor_entry";
    if (osThreadNew((osThreadFunc_t)task_sensor_entry, NULL, &attr) == NULL)
    {
        printf("Falied to create task_sensor_entry!\n");
    }
}

APP_FEATURE_INIT(OC_Demo);