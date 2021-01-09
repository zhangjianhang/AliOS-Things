/*
 * Copyright (C) 2015-2019 Alibaba Group Holding Limited
 */
#include "stm32f4xx_hal.h"

#ifdef HAL_SPI_MODULE_ENABLED

#include "hal_spi_stm32f4.h"
#include <stdio.h>
#include <stdlib.h>

#include "k_api.h"
#include "aos/hal/spi.h"
#include "stm32f4xx_hal_spi.h"

#define INVALID_PIN_ALTERNATE   0xffff

static int32_t isSPIIRQEnable(IRQn_Type IRQn);
typedef struct {
    aos_mutex_t             spi_tx_mutex;
    aos_mutex_t             spi_rx_mutex;
    uint8_t                 inited;
    SPI_OPERATING_MODE_TYPE rx_mode; //0:polling, 1:interrupt, 2:DMA
    SPI_OPERATING_MODE_TYPE tx_mode; //0:polling, 1:interrupt, 2:DMA
    SPI_HandleTypeDef       hal_spi_handle;
    uint32_t                spi_tx_complete;
    uint32_t                spi_rx_complete;
}stm32_spi_t;

static stm32_spi_t stm32_spi[PORT_SPI_SIZE] = {0};

/* function used to transform hal para to stm32l4 para */
static int32_t spi_mode_transform(uint32_t mode_hal, uint32_t *mode_stm32l4);

static int32_t spi_freq_transform(SPI_TypeDef* spiIns, uint32_t freq_hal, uint32_t *BaudRatePrescaler_stm32l4);

void spi_flush_rxregister(SPI_HandleTypeDef *SPIHandle);

GPIO_TypeDef *hal_gpio_typedef(uint16_t hal_pin);

uint32_t hal_gpio_pin(uint16_t hal_pin);

//Get Instanse & attribute from Logical Port
static SPI_MAPPING* GetSPIMapping(const PORT_SPI_TYPE port)
{
    SPI_MAPPING* rc = NULL;
    int8_t       i  = 0;

    for (i=0; i<PORT_SPI_SIZE; i++) {
        if (SPI_MAPPING_TABLE[i].spiFuncP == port) {
            rc = &SPI_MAPPING_TABLE[i];
            break;
        }
    }

    return rc;
}

static PORT_SPI_TYPE GetAppPortFromPhyInstanse(const void* spiIns)
{
    PORT_SPI_TYPE rc = PORT_SPI_INVALID;
    int8_t        i  = 0;

    for(i=0; i<PORT_SPI_SIZE; i++)
    {
        if( (SPI_TypeDef*)SPI_MAPPING_TABLE[i].spiPhyP == (SPI_TypeDef*)spiIns)
        {
            rc = SPI_MAPPING_TABLE[i].spiFuncP;
            break;
        }
    }
    return rc;
}

static int32_t isSPIIRQEnable(IRQn_Type IRQn)
{
    return ((NVIC->ICER[(((uint32_t)(int32_t)IRQn) >> 5UL)] & (uint32_t)(1UL << (((uint32_t)(int32_t)IRQn) & 0x1FUL)))!=0UL)?1UL:0UL;
}

static uint32_t getSPIIRQNumber(SPI_TypeDef* spi_ins)
{
    uint32_t spi_irq_number=0xffff;

    if(spi_ins == SPI1) {
        spi_irq_number = SPI1_IRQn;
    }
    else if (spi_ins ==SPI2) {
        spi_irq_number = SPI2_IRQn;
    }
#ifdef SPI3
   else if(spi_ins ==SPI3) {
        spi_irq_number = SPI3_IRQn;
   }
#endif
#ifdef SPI4
    else if(spi_ins ==SPI4) {
        spi_irq_number = SPI4_IRQn;
    }
#endif
#ifdef SPI5
    else if(spi_ins ==SPI5) {
        spi_irq_number = SPI5_IRQn;
    }
#endif
#ifdef SPI6
    else if(spi_ins ==SPI6) {
        spi_irq_number = SPI6_IRQn;
    }
#endif

    return spi_irq_number;
}

static uint32_t get_alternate(SPI_TypeDef* spi_ins)
{
    uint32_t alternate = INVALID_PIN_ALTERNATE;

    if(spi_ins == SPI1) {
        alternate = GPIO_AF5_SPI1;
    }
    else if(spi_ins ==SPI2) {
        alternate = GPIO_AF5_SPI2;
    }
#ifdef SPI3
    else if(spi_ins ==SPI3) {
        alternate = GPIO_AF5_SPI3;
    }
#endif
#ifdef SPI4
    else if(spi_ins ==SPI4) {
        alternate = GPIO_AF5_SPI4;
    }
#endif
#ifdef SPI5
    else if(spi_ins ==SPI5) {
        alternate = GPIO_AF5_SPI5;
    }
#endif
#ifdef SPI6
    else if(spi_ins ==SPI6) {
        alternate = GPIO_AF5_SPI6;
    }
#endif

    return alternate;
}


static uint32_t hal_spi_pins_map(uint8_t logic_spi)
{
    GPIO_TypeDef          *GPIOx     = NULL;
    GPIO_TypeDef          *GPIO_cs   = NULL;
    GPIO_InitTypeDef       GPIO_InitStruct;
    gpio_spi_pin_config_t *spi_pin_conf;
    uint32_t               pin_cnt;
    uint32_t               pin, cs_pin;
    int                    i;

    SPI_MAPPING* spiIns = GetSPIMapping(logic_spi);
    if (spiIns == NULL) {
        return -1;
    }

    if (spiIns->needmap != HAL_SPI_GPIO_NEED_MAP) {
        return -1;
    }

    spi_pin_conf = spiIns->pin_conf;
    if (NULL == spi_pin_conf) {
        return -1;
    }

    pin_cnt = spiIns->pin_cnt;

    /* config CLK,CS,MOSI,MISO pin */
    for (i = 0; i < pin_cnt ; i++) {
        GPIOx = hal_gpio_typedef(spi_pin_conf[i].pin);
        pin   = hal_gpio_pin(spi_pin_conf[i].pin);

        hal_gpio_enable_clk(spi_pin_conf[i].pin);

        if (spi_pin_conf[i].pin_name == SPI_PIN_CS) {
            GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
            GPIO_InitStruct.Pull  = GPIO_PULLUP;
            cs_pin  = pin;
            GPIO_cs = GPIOx;
        } else {
            GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
            GPIO_InitStruct.Pull      = GPIO_NOPULL;
            GPIO_InitStruct.Alternate = get_alternate(spiIns->spiPhyP);
        }

        GPIO_InitStruct.Pin   = pin;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_MEDIUM;
        HAL_GPIO_Init(GPIOx, &GPIO_InitStruct);
    }

    if (NULL != GPIO_cs) {
        HAL_GPIO_WritePin(GPIO_cs, cs_pin, GPIO_PIN_SET);
    }
}

static uint32_t hal_spi_pins_unmap(uint8_t logic_spi)
{
    GPIO_TypeDef          *GPIOx;
    gpio_spi_pin_config_t *spi_pin_conf;
    GPIO_InitTypeDef       GPIO_InitStruct;
    uint32_t               pin_cnt;
    uint32_t               pin;
    int                    i;

    SPI_MAPPING* spiIns = GetSPIMapping(logic_spi);
    if (spiIns==NULL) {
        return -1;
    }

    if (spiIns->needmap != HAL_SPI_GPIO_NEED_MAP) {
        return -1;
    }

    spi_pin_conf = spiIns->pin_conf;
    if (NULL == spi_pin_conf) {
        return -1;
    }

    pin_cnt = spiIns->pin_cnt;

    /*CLK,CS,MOSI,MISO*/
    for (i = 0; i < pin_cnt; i++) {
        GPIOx = hal_gpio_typedef(spi_pin_conf[i].pin);
        pin   = hal_gpio_pin(spi_pin_conf[i].pin);
        HAL_GPIO_DeInit(GPIOx, pin);
    }

}

static void hal_spi_cs_set(uint8_t logic_spi, uint8_t val)
{
    GPIO_TypeDef          *GPIOx;
    gpio_spi_pin_config_t *spi_pin_conf;
    uint32_t               pin_cnt;
    uint32_t               pin;
    int                    i;

    SPI_MAPPING* spiIns = GetSPIMapping(logic_spi);
    if (spiIns==NULL) {
        return;
    }

    if (spiIns->needmap != HAL_SPI_GPIO_NEED_MAP) {
        return;
    }

    spi_pin_conf = spiIns->pin_conf;
    if (NULL == spi_pin_conf) {
        return;
    }

    pin_cnt = spiIns->pin_cnt;

    for (i = 0; i < pin_cnt; i++) {
        if (spi_pin_conf[i].pin_name == SPI_PIN_CS) {
            GPIOx = hal_gpio_typedef(spi_pin_conf[i].pin);
            pin   = hal_gpio_pin(spi_pin_conf[i].pin);
            HAL_GPIO_WritePin(GPIOx, pin, (val)? GPIO_PIN_SET: GPIO_PIN_RESET);
            break;
        }
    }
}

static void hal_spi_cs_low(uint8_t logic_spi)
{
    hal_spi_cs_set(logic_spi, 0);
}

static void hal_spi_cs_high(uint8_t logic_spi)
{
    hal_spi_cs_set(logic_spi, 1);
}

int32_t hal_spi_init(spi_dev_t *spi)
{

    SPI_HandleTypeDef* spi_handle =NULL;
    int32_t            ret        = 0;

    if (spi == NULL) {
        return -1;
    }

    SPI_MAPPING* spiIns = GetSPIMapping(spi->port);
    if(spiIns==NULL)
    {
        return -1;
    }

    /*releated pins map*/
    hal_spi_pins_map(spi->port);

    memset(&stm32_spi[spi->port],0,sizeof(stm32_spi_t));
    spi_handle = &stm32_spi[spi->port].hal_spi_handle;

    spi_handle->Instance = (SPI_TypeDef*)spiIns->spiPhyP;

    ret  = spi_mode_transform(spi->config.mode,&spi_handle->Init.Mode);
    ret |= spi_freq_transform(spiIns->spiPhyP,spi->config.freq,&spi_handle->Init.BaudRatePrescaler);

    spi_handle->Init.BaudRatePrescaler = spiIns->attr.BaudRatePrescaler;
    spi_handle->Init.Direction         = spiIns->attr.Direction;
    spi_handle->Init.DataSize          = spiIns->attr.DataSize;
    spi_handle->Init.CLKPolarity       = spiIns->attr.CLKPolarity;
    spi_handle->Init.CLKPhase          = spiIns->attr.CLKPhase;
    spi_handle->Init.FirstBit          = spiIns->attr.FirstBit;
    spi_handle->Init.NSS               = spiIns->attr.NSS;
    spi_handle->Init.CRCCalculation    = spiIns->attr.CRCCalculation;
    spi_handle->Init.CRCPolynomial     = spiIns->attr.CRCPolynomial;
    spi_handle->Init.TIMode            = spiIns->attr.TIMode;

    if (HAL_SPI_Init(spi_handle) != HAL_OK) {
        ret = -1;
    }

    __HAL_SPI_ENABLE(spi_handle);

    aos_mutex_new(&stm32_spi[spi->port].spi_tx_mutex);
    aos_mutex_new(&stm32_spi[spi->port].spi_rx_mutex);

    stm32_spi->inited =1;

    if (spi_handle->hdmarx != NULL) {
        stm32_spi[spi->port].rx_mode = SPI_DMA_MODE;
    } else if (isSPIIRQEnable(getSPIIRQNumber(spiIns->spiPhyP))) {
        stm32_spi[spi->port].rx_mode = SPI_INTERRUPT_MODE;
    } else {
        stm32_spi[spi->port].rx_mode = SPI_POLLING_MODE;
    }

    if (spi_handle->hdmatx!=NULL) {
        stm32_spi[spi->port].tx_mode = SPI_DMA_MODE;
    } else if (isSPIIRQEnable(getSPIIRQNumber(spiIns->spiPhyP))) {
        stm32_spi[spi->port].tx_mode = SPI_INTERRUPT_MODE;
    } else {
        stm32_spi[spi->port].tx_mode = SPI_POLLING_MODE;
    }

    return ret;
}

int32_t hal_spi_send(spi_dev_t *spi, const uint8_t *data, uint16_t size, uint32_t timeout)
{
    HAL_StatusTypeDef ret = HAL_BUSY;
    uint32_t          tickstart;

    if ((spi == NULL) || (data == NULL)) {
        return -1;
    }

    aos_mutex_lock(&stm32_spi[spi->port].spi_tx_mutex,AOS_WAIT_FOREVER);

    hal_spi_cs_low(spi->port);

    if (stm32_spi[spi->port].tx_mode == SPI_POLLING_MODE) {
        ret = HAL_SPI_Transmit(&stm32_spi[spi->port].hal_spi_handle, (uint8_t *)data, size, timeout);
    } else if (stm32_spi[spi->port].tx_mode == SPI_INTERRUPT_MODE) {
        stm32_spi[spi->port].spi_tx_complete = 0;
        if ((ret = HAL_SPI_Transmit_IT(&stm32_spi[spi->port].hal_spi_handle, (uint8_t *)data, size)) == HAL_OK) {
            /*wait for the end of transfer
            this is a blocking operation, can use semaphore to replace it if needed
            */
            tickstart = HAL_GetTick();
            while (stm32_spi[spi->port].spi_tx_complete != 1) {
                if ((HAL_GetTick()-tickstart) > timeout) {
                    ret = HAL_TIMEOUT;
                    break;
                }
            }
        }
    } else {
        stm32_spi[spi->port].spi_tx_complete = 0;
        if ((ret = HAL_SPI_Transmit_DMA(&stm32_spi[spi->port].hal_spi_handle, (uint8_t *)data, size)) == HAL_OK) {
            /*wait for the end of transfer
            this is a blocking operation, can use semaphore to replace it if needed
            */
            tickstart = HAL_GetTick();
            while (stm32_spi[spi->port].spi_tx_complete!=1) {
                if ((HAL_GetTick()-tickstart) > timeout) {
                    ret = HAL_TIMEOUT;
                    break;
                }
            }
        }
    }

    hal_spi_cs_high(spi->port);

    aos_mutex_unlock(&stm32_spi[spi->port].spi_tx_mutex);

    return (ret == HAL_OK)? 0: -1;
}

int32_t hal_spi_recv(spi_dev_t *spi, uint8_t *data, uint16_t size, uint32_t timeout)
{
    HAL_StatusTypeDef ret = HAL_BUSY;
    uint32_t          tickstart;

    if((spi == NULL) || (data == NULL)) {
        return -1;
    }
    //clear overun and RXNE flag,to resolve unexpected overrun flag
    //enable it if needed
    //spi_flush_rxregister(&stm32_spi[spi->port].hal_spi_handle);

    aos_mutex_lock(&stm32_spi[spi->port].spi_rx_mutex,AOS_WAIT_FOREVER);

    hal_spi_cs_low(spi->port);

    if (stm32_spi[spi->port].rx_mode==SPI_POLLING_MODE) {
        ret = HAL_SPI_Receive(&stm32_spi[spi->port].hal_spi_handle, (uint8_t *)data, size, timeout);
    } else if (stm32_spi[spi->port].rx_mode == SPI_INTERRUPT_MODE) {
        stm32_spi[spi->port].spi_rx_complete = 0;
        if ((ret = HAL_SPI_Receive_IT(&stm32_spi[spi->port].hal_spi_handle, (uint8_t *)data, size)) == HAL_OK)  {
            /*wait for the end of transfer
            this is a blocking operation, can use semaphore to replace it if needed
            */
            tickstart = HAL_GetTick();
            while (stm32_spi[spi->port].spi_rx_complete!=1) {
                if ((HAL_GetTick()-tickstart) > timeout) {
                    ret = HAL_TIMEOUT;
                    break;
                }
            }
        }
    }  else {
        stm32_spi[spi->port].spi_rx_complete = 0;
        if ((ret = HAL_SPI_Receive_DMA(&stm32_spi[spi->port].hal_spi_handle, (uint8_t *)data, size)) == HAL_OK) {
            /*wait for the end of transfer
            this is a blocking operation, can use semaphore to replace it if needed
            */
            tickstart = HAL_GetTick();
            while (stm32_spi[spi->port].spi_rx_complete != 1) {
                if((HAL_GetTick()-tickstart) > timeout) {
                    ret = HAL_TIMEOUT;
                    break;
                }
            }
        }
    }

    hal_spi_cs_high(spi->port);

    aos_mutex_unlock(&stm32_spi[spi->port].spi_rx_mutex);

    return (ret == HAL_OK)? 0: -1;

}

int32_t hal_spi_send_and_recv(spi_dev_t *spi, uint8_t *tx_data, uint16_t tx_size, uint8_t *rx_data,
                                        uint16_t rx_size, uint32_t timeout)
{
    HAL_StatusTypeDef ret = HAL_BUSY;
    uint32_t          tickstart;

    if((spi == NULL) || (tx_data == NULL) || (rx_data == NULL))
    {
        return -1;
    }

    //clear overun and RXNE flag,to resolve unexpected overrun flag
    //enable it if needed
    spi_flush_rxregister(&stm32_spi[spi->port].hal_spi_handle);

    aos_mutex_lock(&stm32_spi[spi->port].spi_tx_mutex,AOS_WAIT_FOREVER);
    aos_mutex_lock(&stm32_spi[spi->port].spi_rx_mutex,AOS_WAIT_FOREVER);

    hal_spi_cs_low(spi->port);

    ret = HAL_SPI_Transmit(&stm32_spi[spi->port].hal_spi_handle, tx_data, tx_size, timeout);
    if (ret == HAL_OK) {
       ret = HAL_SPI_Receive(&stm32_spi[spi->port].hal_spi_handle, rx_data, rx_size, timeout);
    }

    hal_spi_cs_high(spi->port);

    aos_mutex_unlock(&stm32_spi[spi->port].spi_tx_mutex);
    aos_mutex_unlock(&stm32_spi[spi->port].spi_rx_mutex);

    return (ret == HAL_OK)? 0: -1;

}

int32_t hal_spi_send_and_send(spi_dev_t *spi, uint8_t *tx1_data, uint16_t tx1_size, uint8_t *tx2_data,
                                        uint16_t tx2_size, uint32_t timeout)
{
    HAL_StatusTypeDef ret = HAL_BUSY;
    uint32_t          tickstart;

    if ((spi == NULL) || (tx1_data == NULL) || (tx2_data == NULL)) {
        return -1;
    }

    //clear overun and RXNE flag,to resolve unexpected overrun flag
    //enable it if needed
    spi_flush_rxregister(&stm32_spi[spi->port].hal_spi_handle);

    aos_mutex_lock(&stm32_spi[spi->port].spi_tx_mutex, AOS_WAIT_FOREVER);

    hal_spi_cs_low(spi->port);

    ret = HAL_SPI_Transmit(&stm32_spi[spi->port].hal_spi_handle, tx1_data, tx1_size, timeout);
    if (ret == HAL_OK) {
        ret = HAL_SPI_Transmit(&stm32_spi[spi->port].hal_spi_handle, tx2_data, tx2_size, timeout);
    }

    hal_spi_cs_high(spi->port);

    aos_mutex_unlock(&stm32_spi[spi->port].spi_tx_mutex);

    return (ret == HAL_OK)? 0: -1;
}

int32_t hal_spi_send_recv(spi_dev_t *spi, uint8_t *tx_data, uint8_t *rx_data,
                          uint16_t size, uint32_t timeout)
{
    HAL_StatusTypeDef ret = HAL_BUSY;
    uint32_t         tickstart;

    if ((spi == NULL) || (tx_data == NULL) || (rx_data == NULL)) {
        return -1;
    }

    //clear overun and RXNE flag,to resolve unexpected overrun flag
    //enable it if needed
    spi_flush_rxregister(&stm32_spi[spi->port].hal_spi_handle);

    aos_mutex_lock(&stm32_spi[spi->port].spi_tx_mutex,AOS_WAIT_FOREVER);
    aos_mutex_lock(&stm32_spi[spi->port].spi_rx_mutex,AOS_WAIT_FOREVER);

    hal_spi_cs_low(spi->port);

     if (stm32_spi[spi->port].tx_mode==SPI_DMA_MODE && stm32_spi[spi->port].rx_mode==SPI_DMA_MODE) {
        stm32_spi[spi->port].spi_rx_complete = 0;
        stm32_spi[spi->port].spi_tx_complete = 0;
        if ((ret = HAL_SPI_TransmitReceive_DMA(&stm32_spi[spi->port].hal_spi_handle, tx_data,rx_data, size)) == HAL_OK) {
            /*wait for the end of transfer
            this is a blocking operation, can use semaphore to replace it if needed
            */
            tickstart = HAL_GetTick();

            while(stm32_spi[spi->port].spi_rx_complete!=1 || stm32_spi[spi->port].spi_tx_complete!=1) {
                if ((HAL_GetTick()-tickstart) > timeout)  {
                    ret = HAL_TIMEOUT;
                    break;
                }
            }
        }
    } else if(stm32_spi[spi->port].tx_mode==SPI_INTERRUPT_MODE && stm32_spi[spi->port].rx_mode==SPI_INTERRUPT_MODE) {
        stm32_spi[spi->port].spi_rx_complete = 0;
        stm32_spi[spi->port].spi_tx_complete = 0;
        if ((ret = HAL_SPI_TransmitReceive_IT(&stm32_spi[spi->port].hal_spi_handle, tx_data,rx_data, size)) == HAL_OK)  {
            printf("start rx/tx ,wait for the end of transfer\n");//@sz test
            /*wait for the end of transfer
            this is a blocking operation, can use semaphore to replace it if needed
            */
            tickstart = HAL_GetTick();
            while(stm32_spi[spi->port].spi_rx_complete!=1|| stm32_spi[spi->port].spi_tx_complete!=1) {
                if ((HAL_GetTick()-tickstart) > timeout) {
                    ret = HAL_TIMEOUT;
                    break;
                }
            }
        }
    } else  {
        ret = HAL_SPI_TransmitReceive(&stm32_spi[spi->port].hal_spi_handle,tx_data,rx_data,size,timeout);
    }

    hal_spi_cs_high(spi->port);

    aos_mutex_unlock(&stm32_spi[spi->port].spi_tx_mutex);
    aos_mutex_unlock(&stm32_spi[spi->port].spi_rx_mutex);

   return (ret == HAL_OK)? 0: -1;
}

int32_t hal_spi_finalize(spi_dev_t *spi)
{
    HAL_StatusTypeDef ret = HAL_BUSY;

    if (spi == NULL) {
        return -1;
    }

    hal_spi_pins_unmap(spi->port);

    ret = HAL_SPI_DeInit(&stm32_spi[spi->port].hal_spi_handle);

    if (aos_mutex_is_valid(&stm32_spi[spi->port].spi_tx_mutex)) {
        aos_mutex_free(&stm32_spi[spi->port].spi_tx_mutex);
    }

    if(aos_mutex_is_valid(&stm32_spi[spi->port].spi_rx_mutex)) {
        aos_mutex_free(&stm32_spi[spi->port].spi_rx_mutex);
    }

    stm32_spi[spi->port].inited=0;

    return (ret == HAL_OK)? 0: -1;
}

void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *SPIHandle)
{
    const PORT_SPI_TYPE appPort = GetAppPortFromPhyInstanse(SPIHandle->Instance);

    stm32_spi[appPort].spi_tx_complete =1;

}

void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef *SPIHandle)
{
    const PORT_SPI_TYPE appPort = GetAppPortFromPhyInstanse(SPIHandle->Instance);

    stm32_spi[appPort].spi_rx_complete =1;

}

void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *SPIHandle)
{
    const PORT_SPI_TYPE appPort = GetAppPortFromPhyInstanse(SPIHandle->Instance);

    stm32_spi[appPort].spi_tx_complete =1;
    stm32_spi[appPort].spi_rx_complete =1;
}

void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *SPIHandle)
{
    static uint32_t     errorcount=0;
    uint32_t            error_code;
    PORT_SPI_TYPE       appPort;

    appPort    = GetAppPortFromPhyInstanse(SPIHandle->Instance);
    error_code = HAL_SPI_GetError(SPIHandle);
    errorcount++;
}

static void SPI_IRQProcessor(const SPI_TypeDef* ins)
{
    const PORT_SPI_TYPE appPort= GetAppPortFromPhyInstanse(ins);

    if (appPort!=PORT_SPI_INVALID) {
        HAL_SPI_IRQHandler(&stm32_spi[appPort].hal_spi_handle);
    }
}

void SPI1_IRQHandler(void)
{
    krhino_intrpt_enter();
    HAL_SPI_IRQHandler(&stm32_spi[0].hal_spi_handle);
    krhino_intrpt_exit();

}

void SPI2_IRQHandler(void)
{
    krhino_intrpt_enter();
    SPI_IRQProcessor(SPI2);
    krhino_intrpt_exit();
}

void SPI3_IRQHandler(void)
{
    krhino_intrpt_enter();
    SPI_IRQProcessor(SPI3);
    krhino_intrpt_exit();
}

void SPI4_IRQHandler(void)
{
    krhino_intrpt_enter();
    SPI_IRQProcessor(SPI4);
    krhino_intrpt_exit();
}

void SPI5_IRQHandler(void)
{
    krhino_intrpt_enter();
    SPI_IRQProcessor(SPI5);
    krhino_intrpt_exit();
}

void SPI6_IRQHandler(void)
{
    krhino_intrpt_enter();
    SPI_IRQProcessor(SPI6);
    krhino_intrpt_exit();
}


void SPI_DMA_RX_IRQHandler(const void* spiIns)
{
    const PORT_SPI_TYPE appPort = GetAppPortFromPhyInstanse(spiIns);

    if( appPort!=PORT_SPI_INVALID ) {
        HAL_DMA_IRQHandler(stm32_spi[appPort].hal_spi_handle.hdmarx);
    }
}

void SPI_DMA_TX_IRQHandler(const void* spiIns)
{
    const PORT_SPI_TYPE appPort = GetAppPortFromPhyInstanse(spiIns);

    if( appPort!=PORT_SPI_INVALID ) {
        HAL_DMA_IRQHandler(stm32_spi[appPort].hal_spi_handle.hdmatx);
    }
}

void DMA1_Stream1_IRQHandler(void)
{
    SPI_DMA_TX_IRQHandler(SPI1);
}

void DMA1_Stream2_IRQHandler(void)
{
    SPI_DMA_RX_IRQHandler(SPI1);
}

void DMA1_Stream3_IRQHandler(void)
{
    SPI_DMA_TX_IRQHandler(SPI3);
}

void DMA1_Stream4_IRQHandler(void)
{
    SPI_DMA_RX_IRQHandler(SPI3);
}


static int32_t spi_mode_transform(uint32_t mode_hal, uint32_t *mode_stm32f4)
{
    uint32_t mode = 0;
    int32_t  ret  = 0;

    if (mode_hal == HAL_SPI_MODE_MASTER) {
        mode = SPI_MODE_MASTER;
    }  else if(mode_hal == HAL_SPI_MODE_SLAVE) {
        mode = SPI_MODE_SLAVE;
    } else {
        ret = -1;
    }

    if (ret == 0) {
        *mode_stm32f4 = mode;
    }

    return ret;
}

static int32_t spi_freq_transform(SPI_TypeDef* spiIns, uint32_t freq_hal, uint32_t *BaudRatePrescaler_stm32l4)
{
    int32_t  ret        = 0;
    uint32_t plck       =0;
    uint32_t freq_ratio =0;

    /* calc BaudRatePrescaler according to freq_hal */
    if (spiIns==SPI2 || spiIns==SPI3) {
        plck=HAL_RCC_GetPCLK1Freq();
    } else {
        plck=HAL_RCC_GetPCLK2Freq();
    }

    freq_ratio = (uint32_t)plck/freq_hal;

    if (freq_ratio<=2) {
        *BaudRatePrescaler_stm32l4 = SPI_BAUDRATEPRESCALER_2;
    } else if(freq_ratio<=4) {
        *BaudRatePrescaler_stm32l4 = SPI_BAUDRATEPRESCALER_4;
    } else if(freq_ratio<=8) {
        *BaudRatePrescaler_stm32l4 = SPI_BAUDRATEPRESCALER_8;
    } else if(freq_ratio<=16) {
        *BaudRatePrescaler_stm32l4 = SPI_BAUDRATEPRESCALER_16;
    } else if(freq_ratio<=32) {
        *BaudRatePrescaler_stm32l4 = SPI_BAUDRATEPRESCALER_32;
    } else if(freq_ratio<=64) {
        *BaudRatePrescaler_stm32l4 = SPI_BAUDRATEPRESCALER_64;
    } else if(freq_ratio<=128) {
        *BaudRatePrescaler_stm32l4 = SPI_BAUDRATEPRESCALER_128;
    } else {
        *BaudRatePrescaler_stm32l4 = SPI_BAUDRATEPRESCALER_256;
    }

    return ret;
}

/*
   this function is used before startup of a new communication
   check if there is data not read out of DR register
   clear overun and RXNE flag,to resolve unexpected overrun flag issue
*/
void spi_flush_rxregister(SPI_HandleTypeDef *SPIHandle)
{
    __IO uint32_t tmpreg_ovr=0 ;

    if (SPIHandle->Instance->SR & SPI_SR_RXNE)  {
        tmpreg_ovr = SPIHandle->Instance->DR;
        tmpreg_ovr = SPIHandle->Instance->SR;
        UNUSED(tmpreg_ovr);
    }
}

#endif /* HAL_SPI_MODULE_ENABLED */

