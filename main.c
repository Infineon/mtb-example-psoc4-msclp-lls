/******************************************************************************
 * File Name: main.c
 *
 * Description: This is the source code for the PSoC 4 MSCLP CAPSENSE™ liquid level sensing code example for ModusToolbox.
 *
 * Related Document: See README.md
 *
 *******************************************************************************
 * Copyright 2021-2023, Cypress Semiconductor Corporation (an Infineon company) or
 * an affiliate of Cypress Semiconductor Corporation.  All rights reserved.
 *
 * This software, including source code, documentation and related
 * materials ("Software") is owned by Cypress Semiconductor Corporation
 * or one of its affiliates ("Cypress") and is protected by and subject to
 * worldwide patent protection (United States and foreign),
 * United States copyright laws and international treaty provisions.
 * Therefore, you may use this Software only as provided in the license
 * agreement accompanying the software package from which you
 * obtained this Software ("EULA").
 * If no EULA applies, Cypress hereby grants you a personal, non-exclusive,
 * non-transferable license to copy, modify, and compile the Software
 * source code solely for use in connection with Cypress's
 * integrated circuit products.  Any reproduction, modification, translation,
 * compilation, or representation of this Software except as specified
 * above is prohibited without the express written permission of Cypress.
 *
 * Disclaimer: THIS SOFTWARE IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, NONINFRINGEMENT, IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. Cypress
 * reserves the right to make changes to the Software without notice. Cypress
 * does not assume any liability arising out of the application or use of the
 * Software or any product or circuit described in the Software. Cypress does
 * not authorize its products for use in any products where a malfunction or
 * failure of the Cypress product may reasonably be expected to result in
 * significant property damage, injury or death ("High Risk Product"). By
 * including Cypress's product in a High Risk Product, the manufacturer
 * of such system or application assumes all risk of such use and in doing
 * so agrees to indemnify Cypress against all liability.
 *******************************************************************************/

/*******************************************************************************
 * Include header files
 ******************************************************************************/
#include "cy_pdl.h"
#include "cybsp.h"
#include "cycfg.h"
#include "cycfg_capsense.h"
#include "cy_em_eeprom.h"
/*******************************************************************************
 * User configurable Macros
 ********************************************************************************/
#define STATE_VALID                     (1u)
#define CYBSP_LED_OFF                   (0u)
#define CYBSP_LED_ON                    (1u)

/*******************************************************************************
 * One time factory CDAC calibration Macros
 ********************************************************************************/

/* It is mandatory to perform one time factory CDAC calibration for liquid level widgets on each devices. 
 * Using this macro "LLS_FACTORY_CALIBRATION_ENABLE" helps to identify the relevant source code involved in
 * enabling this emulated EERPROM based one time factory calibration.
 * Not recommended to disable this macro. 
 */
#define LLS_FACTORY_CALIBRATION_ENABLE     (1u) // Don't set this to 0

#if LLS_FACTORY_CALIBRATION_ENABLE

/* ASCII "P" */
#define ASCII_P                     (0x50)

#define POWERON_STATE_OFFSET        (0u)
#define POWERON_STATUS_SIZE         (1u)

#define WD_CDAC_PARMS_SIZE              (1u+ 1u +1u +2u ) // Status+CRef+CFine+CComp_div
#define TOTAL_WD_CDAC_PARMS_SIZE        (CY_CAPSENSE_TOTAL_WIDGET_COUNT * WD_CDAC_PARMS_SIZE)
#define SNS_CDAC_PARMS_SIZE             (CY_CAPSENSE_SENSOR_COUNT * 1u) // CComp
#define TOTAL_CDAC_PARMS_SIZE           (TOTAL_WD_CDAC_PARMS_SIZE + SNS_CDAC_PARMS_SIZE)

#define EEPROM_STATE_OFFSET         (0u)
#define EEPROM_DATA_OFFSET          (1u)





/*******************************************************************************
 * EMULATED EEPROM configuration parameters
 ********************************************************************************/
/* Emulated EEPROM Configuration details. All the sizes mentioned are in bytes.
 * For details on how to configure these values refer to cy_em_eeprom.h. The
 * middleware documentation is provided in Emulated EEPROM API Reference Manual.
 * The user can access it from the Documentation section in the Quick Panel.
 */

/* Logical Size of Emulated EEPROM in bytes */
#define LOGICAL_EM_EEPROM_SIZE      POWERON_STATUS_SIZE + TOTAL_CDAC_PARMS_SIZE
#define LOGICAL_EM_EEPROM_START     (0u)

#define EM_EEPROM_SIZE   ((TOTAL_CDAC_PARMS_SIZE % CY_EM_EEPROM_FLASH_SIZEOF_ROW) ? ((TOTAL_CDAC_PARMS_SIZE / CY_EM_EEPROM_FLASH_SIZEOF_ROW) + 1) * CY_EM_EEPROM_FLASH_SIZEOF_ROW  : (TOTAL_CDAC_PARMS_SIZE / CY_EM_EEPROM_FLASH_SIZEOF_ROW) *  CY_EM_EEPROM_FLASH_SIZEOF_ROW)

/*If enabled (1 - enabled, 0 - disabled), a checksum (stored in a row) is calculated on each row of data,
 * while a redundant copy of Em_EEPROM is stored in another location.
 * When data is read, first the checksum is checked. If that checksum is bad,
 and the redundant copy's checksum is good, the copy is restored.
 */
#define REDUNDANT_COPY              (1u)

/*If enabled (1 - enabled, 0 - disabled), the blocking writes to nvm are used in the design.
 * Otherwise, non-blocking nvm writes are used. From the user's perspective,
 * the behavior of blocking and non-blocking writes are the same - the difference is that
 * the non-blocking writes do not block the interrupts.
 Note
 Non-blocking nvm write is only supported by PSoC 6.
 */
#define BLOCKING_WRITE              (1u)

/*The higher the factor is, the more nvm is used, but a higher number of erase/write cycles can be done on Em_EEPROM.
 * Multiply this number by the datasheet write endurance spec to determine the max of write cycles.
 * The amount of wear leveling from 1 to 10. 1 means no wear leveling is used.
 */
#define WEAR_LEVELLING_FACTOR       (2u)

/*Simple mode, when enabled (1 - enabled, 0 - disabled),
 * means no additional service information is stored by the Em_EEPROM middleware
 * like checksums, headers, a number of writes, etc.
 */
#define SIMPLE_MODE                 (0u)

#define EM_EEPROM_PHYSICAL_SIZE     (CY_EM_EEPROM_GET_PHYSICAL_SIZE(EM_EEPROM_SIZE, SIMPLE_MODE, WEAR_LEVELLING_FACTOR, REDUNDANT_COPY))

#endif

/*******************************************************************************
 * Fixed Macros
 *******************************************************************************/
#define CAPSENSE_MSC0_INTR_PRIORITY     (3u)
#define CY_ASSERT_FAILED                (0u)

/* EZI2C interrupt priority must be higher than CAPSENSE&trade; interrupt. */
#define EZI2C_INTR_PRIORITY             (2u)

/*******************************************************************************
 * Global Variables
 *******************************************************************************/
cy_stc_scb_ezi2c_context_t ezi2c_context;

#if LLS_FACTORY_CALIBRATION_ENABLE

cy_stc_eeprom_context_t em_eeprom_context;

static cy_stc_eeprom_config_t em_eeprom_config = { .eepromSize = EM_EEPROM_SIZE, /* 256 bytes */
        .blockingWrite = BLOCKING_WRITE, /* Blocking writes enabled */
        .redundantCopy = REDUNDANT_COPY, /* Redundant copy enabled */
        .wearLevelingFactor = WEAR_LEVELLING_FACTOR, /* Wear levelling factor of 2 */
        .simpleMode = SIMPLE_MODE, /* Simple mode disabled */
};

/* EEPROM storage Emulated EEPROM flash. */
CY_ALIGN( CY_EM_EEPROM_FLASH_SIZEOF_ROW)
const uint8_t em_eeprom_storage[EM_EEPROM_PHYSICAL_SIZE] = { 0u };

/* RAM arrays for holding Emulated EEPROM read and write data respectively. */
uint8_t eeprom_data[LOGICAL_EM_EEPROM_SIZE];

#endif

/*******************************************************************************
 * Function Prototypes
 *******************************************************************************/
static void initialize_capsense(void);
static void capsense_msc0_isr(void);
static void ezi2c_isr(void);
static void initialize_capsense_tuner(void);
void led_control();

#if LLS_FACTORY_CALIBRATION_ENABLE
static void initialize_em_eeprom(void);
static uint32_t get_eeprom_buffer_position(uint32_t wd_id);
static void liquid_level_OneTime_Calibration(void);
#endif

/*******************************************************************************
 * Function Name: main
 ********************************************************************************
 * Summary:
 *  System entrance point. This function performs
 *  - initial setup of device
 *  - initialize CAPSENSE
 *  - initialize tuner communication
 *  - scan liquid level sensor and the liquid level foam rejection sensor continuously
 *  - send the processed sensor data to the tuner
 *
 * Return:
 *  int
 *
 *******************************************************************************/

int main(void)
{
        cy_rslt_t result;

        /* Initialize the device and board peripherals */
        result = cybsp_init();

        /* Board init failed. Stop program execution */
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(CY_ASSERT_FAILED);
    }

    /* Enable global interrupts */
    __enable_irq();

#if LLS_FACTORY_CALIBRATION_ENABLE
    initialize_em_eeprom();
#endif

    /* Initialize EZI2C */
    initialize_capsense_tuner();

    /* Initialize MSC CAPSENSE */
    initialize_capsense();

    for (;;)
    {
        uint32_t level_w_FR, level_wo_FR;

                /* Scan the normal Liquid Level Widget */
        Cy_CapSense_ScanWidget(CY_CAPSENSE_LIQUIDLEVEL0_WDGT_ID,
                &cy_capsense_context);
        /* Wait until the scan is finished */
        while (Cy_CapSense_IsBusy(&cy_capsense_context))
        {
        }

        /* Scan the Foam Rejection Widget */
        Cy_CapSense_ScanWidget(CY_CAPSENSE_LIQUIDLEVEL0_FR_WDGT_ID,
                &cy_capsense_context);
        /* Wait until the scan is finished */
        while (Cy_CapSense_IsBusy(&cy_capsense_context))
        {
        }

        /* Process all th widgets */
        Cy_CapSense_ProcessAllWidgets(&cy_capsense_context);

        /* Send capsense data to the Tuner */
        Cy_CapSense_RunTuner(&cy_capsense_context);


        /* store the liquid level before and after foam rejection */
        level_wo_FR = CY_CAPSENSE_LIQUIDLEVEL0_PTRPOSITION_VALUE->x;
        level_w_FR = CY_CAPSENSE_LIQUIDLEVEL0_FR_PTRPOSITION_VALUE->x;

        /* keep the compiler happy */
        (void) level_wo_FR;
        (void) level_w_FR;

        led_control();
    }
}

/*******************************************************************************
 * Function Name: initialize_capsense
 ********************************************************************************
 * Summary:
 *  This function initializes the CAPSENSE and configures the CAPSENSE
 *  interrupt.
 *
 *******************************************************************************/
static void initialize_capsense(void)
{
    cy_capsense_status_t status = CY_CAPSENSE_STATUS_SUCCESS;

    /* CAPSENSE interrupt configuration MSCLP 0 */
    const cy_stc_sysint_t capsense_msc0_interrupt_config = { .intrSrc =
            CY_MSCLP0_LP_IRQ, .intrPriority = CAPSENSE_MSC0_INTR_PRIORITY, };

    /* Capture the MSC HW block and initialize it to the default state. */
    status = Cy_CapSense_Init(&cy_capsense_context);

    if (CY_CAPSENSE_STATUS_SUCCESS == status)
    {
        /* Initialize CAPSENSE interrupt for MSCLP 0 */
        Cy_SysInt_Init(&capsense_msc0_interrupt_config, capsense_msc0_isr);
        NVIC_ClearPendingIRQ(capsense_msc0_interrupt_config.intrSrc);
        NVIC_EnableIRQ(capsense_msc0_interrupt_config.intrSrc);

#if LLS_FACTORY_CALIBRATION_ENABLE
        /* Verify the EEPROM and perform one time factory calibration*/
        liquid_level_OneTime_Calibration();
#endif
        /* Initialize the CAPSENSE firmware modules. */
        status = Cy_CapSense_Enable(&cy_capsense_context);

    }

    if (status != CY_CAPSENSE_STATUS_SUCCESS)
    {
        /* This status could fail before tuning the sensors correctly.
         * Ensure that this function passes after the CAPSENSE sensors are tuned
         * as per procedure give in the Readme.md file */
    }
}

/*******************************************************************************
 * Function Name: capsense_msc0_isr
 ********************************************************************************
 * Summary:
 *  Wrapper function for handling interrupts from CAPSENSE MSC0 block.
 *
 *******************************************************************************/
static void capsense_msc0_isr(void)
{
    Cy_CapSense_InterruptHandler(CY_MSCLP0_HW, &cy_capsense_context);
}

/*******************************************************************************
 * Function Name: initialize_capsense_tuner
 ********************************************************************************
 * Summary:
 * EZI2C module to communicate with the CAPSENSE Tuner tool.
 *
 *******************************************************************************/
static void initialize_capsense_tuner(void)
{
    cy_en_scb_ezi2c_status_t status = CY_SCB_EZI2C_SUCCESS;

    /* EZI2C interrupt configuration structure */
    const cy_stc_sysint_t ezi2c_intr_config = { .intrSrc = CYBSP_EZI2C_IRQ,
            .intrPriority = EZI2C_INTR_PRIORITY, };

    /* Initialize the EzI2C firmware module */
    status = Cy_SCB_EZI2C_Init(CYBSP_EZI2C_HW, &CYBSP_EZI2C_config,
            &ezi2c_context);
    if (status != CY_SCB_EZI2C_SUCCESS)
    {
        CY_ASSERT(CY_ASSERT_FAILED);
    }
    Cy_SysInt_Init(&ezi2c_intr_config, ezi2c_isr);
    NVIC_EnableIRQ(ezi2c_intr_config.intrSrc);

    /* Set the CAPSENSE data structure as the I2C buffer to be exposed to the
     * master on primary slave address interface. Any I2C host tools such as
     * the Tuner or the Bridge Control Panel can read this buffer but you can
     * connect only one tool at a time.
     */
    Cy_SCB_EZI2C_SetBuffer1(CYBSP_EZI2C_HW, (uint8_t*) &cy_capsense_tuner,
            sizeof(cy_capsense_tuner), sizeof(cy_capsense_tuner),
            &ezi2c_context);

    Cy_SCB_EZI2C_Enable(CYBSP_EZI2C_HW);
}

/*******************************************************************************
 * Function Name: ezi2c_isr
 ********************************************************************************
 * Summary:
 * Wrapper function for handling interrupts from EZI2C block.
 *
 *******************************************************************************/
static void ezi2c_isr(void)
{
    Cy_SCB_EZI2C_Interrupt(CYBSP_EZI2C_HW, &ezi2c_context);
}

#if LLS_FACTORY_CALIBRATION_ENABLE

/*******************************************************************************
 * Function Name: initialize_em_eeprom
 ********************************************************************************
 * Summary:
 * Initializes the emulated EEPROM to store the ne time factory calibrated values 
 * and verified the first power on condition 
 *
 *******************************************************************************/
static void initialize_em_eeprom(void)
{
    cy_en_em_eeprom_status_t em_eeprom_status;

    /* Initialize the flash start address in Emulated EEPROM configuration structure*/
    em_eeprom_config.userFlashStartAddr = (uint32_t) em_eeprom_storage;

    /* Initialize Emulated EEPROM */
    em_eeprom_status = Cy_Em_EEPROM_Init(&em_eeprom_config, &em_eeprom_context);

    /* Read the factory calibrated values from EEPROM*/
    Cy_Em_EEPROM_Read(LOGICAL_EM_EEPROM_START, eeprom_data,
            LOGICAL_EM_EEPROM_SIZE, &em_eeprom_context);

    // Verify for the first power on condition
    if (ASCII_P != eeprom_data[POWERON_STATE_OFFSET])
    {
        /* Erase Emulated EEPROM */
        em_eeprom_status = Cy_Em_EEPROM_Erase(&em_eeprom_context);
        eeprom_data[POWERON_STATE_OFFSET] = 'P';
        /* Write the factory calibrated values to EEPROM*/
        Cy_Em_EEPROM_Write(LOGICAL_EM_EEPROM_START, eeprom_data,
                POWERON_STATUS_SIZE, &em_eeprom_context);
    }

    /* Emulated EEPROM init failed. Stop program execution */
    if (em_eeprom_status != CY_EM_EEPROM_SUCCESS)
    {
        CY_ASSERT(CY_ASSERT_FAILED);
    }
}
#endif

#if LLS_FACTORY_CALIBRATION_ENABLE

/*******************************************************************************
 * Function Name: get_eeprom_buffer_position
 ********************************************************************************
 * Summary:
 * Returns the EEPROM pointer location of the widget
 *
 *******************************************************************************/
static uint32_t get_eeprom_buffer_position(uint32_t wd_id)
{
    uint32_t wd_index;
    uint32_t buffer_index = 0;

    for (wd_index = 0u; wd_index < wd_id; wd_index++)
    {
        buffer_index += (WD_CDAC_PARMS_SIZE
                + cy_capsense_context.ptrWdConfig[wd_index].numSns);
    }
    return buffer_index;
}
#endif

#if LLS_FACTORY_CALIBRATION_ENABLE
/*******************************************************************************
 * Function Name: liquid_level_OneTime_Calibration
 ********************************************************************************
 * Summary:
 * Verifies and performs the one time factory auto calibration status of the liquid level widgets
 *
 *******************************************************************************/
static void liquid_level_OneTime_Calibration(void)
{
    uint8_t *ptr_eeprom_data;
    uint32_t wd_id = 0u;
    uint8_t calib_status = 0u;

    /* Read the factory calibrated values from EEPROM*/
    Cy_Em_EEPROM_Read(LOGICAL_EM_EEPROM_START, eeprom_data,
            LOGICAL_EM_EEPROM_SIZE, &em_eeprom_context);

    for (wd_id = 0u; wd_id < CY_CAPSENSE_TOTAL_WIDGET_COUNT; wd_id++)
    {
        /* Apply one time calibration only to Liquid level widgets */
        if ((uint8_t) cy_capsense_context.ptrWdConfig[wd_id].wdType == CY_CAPSENSE_WD_LIQUID_LEVEL_E ||
                (uint8_t) cy_capsense_context.ptrWdConfig[wd_id].wdType == CY_CAPSENSE_WD_LIQUID_PRESENCE_E)
        {
            /* Get the EEPROM pointer location of the widget*/
            ptr_eeprom_data = &eeprom_data[get_eeprom_buffer_position(wd_id)
                                           + POWERON_STATUS_SIZE];

            /* Verify the LLS CDAC calibration and level calibration completion*/
            if (Cy_CapSense_IsLlwCalibrationValid(wd_id, &cy_capsense_context))
            {
                if (ptr_eeprom_data[EEPROM_STATE_OFFSET] == STATE_VALID)
                {
                    /* Set the status flag to ready state before writing into EEPROM.*/
                    Cy_CapSense_SetWidgetCalibrationState(wd_id,
                            STATE_VALID, &cy_capsense_context);

                    /* Update CDAC parameters with the factory calibrated values read from EEPROM */
                    Cy_CapSense_WriteWidgetCdacParam(
                            &ptr_eeprom_data[EEPROM_DATA_OFFSET], wd_id,
                            &cy_capsense_context);
                }
                else
                {
                    /* Call Cy_CapSense_Enable() only once and use the CDAC values of widgets to store in EEPROM */
                    if (calib_status == 0u)
                    {
                        /* Initialize the CAPSENSE firmware modules. */
                        Cy_CapSense_Enable(&cy_capsense_context);
                        calib_status = 1;
                    }
                    /* Reads the calibrated values from context to eeprom buffer*/
                    Cy_CapSense_ReadWidgetCdacParam(
                            &ptr_eeprom_data[EEPROM_DATA_OFFSET], wd_id,
                            &cy_capsense_context);

                    /* Set the status flag to valid before writing into EEPROM*/
                    ptr_eeprom_data[EEPROM_STATE_OFFSET] = STATE_VALID;

                    /* Update the local calibration status flag */
                    Cy_CapSense_SetWidgetCalibrationState(wd_id,
                            STATE_VALID, &cy_capsense_context);

                    /* Write the factory calibrated values to EEPROM*/
                    Cy_Em_EEPROM_Write(LOGICAL_EM_EEPROM_START, eeprom_data,
                            LOGICAL_EM_EEPROM_SIZE, &em_eeprom_context);
                }
            }
        }
        else
        {
            /* Update the local calibration status flag to avoid auto calibration of LLS widget */
            Cy_CapSense_SetWidgetCalibrationState(wd_id,
                    STATE_VALID, &cy_capsense_context);
        }
    }
}
#endif

/*******************************************************************************
 * Function Name: led_control
 ********************************************************************************
 * Summary:
 * Control LED2 and LED3 in the kit to show the Tank removal detection and Liquid presence/absence
 *******************************************************************************/
void led_control()
{
    uint32_t tank_status;

#if (CY_CAPSENSE_LIQUID_LEVEL_TANK_REMOVAL_DETECTION_EN)
    tank_status = Cy_CapSense_IsTankRemoved(cy_capsense_context.ptrWdContext);
    if (tank_status==1)
        Cy_GPIO_Write(CYBSP_USER_LED2_PORT, CYBSP_USER_LED2_NUM, CYBSP_LED_ON);
    else
        Cy_GPIO_Write(CYBSP_USER_LED2_PORT, CYBSP_USER_LED2_NUM, CYBSP_LED_OFF);
#endif

    if(cy_capsense_context.ptrWdConfig->ptrWdContext->status & CY_CAPSENSE_WD_ACTIVE_MASK )
        Cy_GPIO_Write(CYBSP_USER_LED1_PORT, CYBSP_USER_LED1_NUM, CYBSP_LED_ON);
    else
        Cy_GPIO_Write(CYBSP_USER_LED1_PORT, CYBSP_USER_LED1_NUM, CYBSP_LED_OFF);

}
/* [] END OF FILE */
