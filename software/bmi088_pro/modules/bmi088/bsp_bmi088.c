#include "bsp_bmi088.h"

#include "ahrs.h"
#include "bsp_spi.h"
#include "spi.h"
#include "bmi088driver.h"
#include "ist8310driver.h"

void BMI088_Drive_Init(void);
void BMI088_Read_update(void);
void bim088_gyro_offset_update(void);
void gyro_offset_init(void);
static void imu_cmd_spi_dma(void);
static void imu_cali_slove(fp32 gyro[3], fp32 accel[3], fp32 mag[3], bmi088_real_data_t *bmi088, ist8310_real_data_t *ist8310);
void gyro_offset_calc(fp32 gyro_offset[3], fp32 gyro[3], uint16_t *offset_time_count);
void imu_continuous_processing(imu_sensor *imu_sensor_t);
/*����DMA������*/
uint8_t gyro_dma_rx_buf[SPI_DMA_GYRO_LENGHT];
uint8_t gyro_dma_tx_buf[SPI_DMA_GYRO_LENGHT] = {0x82,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

uint8_t accel_dma_rx_buf[SPI_DMA_ACCEL_LENGHT];
uint8_t accel_dma_tx_buf[SPI_DMA_ACCEL_LENGHT] = {0x92,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

uint8_t accel_temp_dma_rx_buf[SPI_DMA_ACCEL_TEMP_LENGHT];
uint8_t accel_temp_dma_tx_buf[SPI_DMA_ACCEL_TEMP_LENGHT] = {0xA2,0xFF,0xFF,0xFF};
/*����DMA������*/

/*DMA״̬��־*/
volatile uint8_t gyro_update_flag = 0;
volatile uint8_t accel_update_flag = 0;
volatile uint8_t accel_temp_update_flag = 0;
volatile uint8_t mag_update_flag = 0;
volatile uint8_t imu_start_dma_flag = 0;




bmi088_real_data_t bmi088_real_data;
ist8310_real_data_t ist8310_real_data;
imu_sensor bmi088_real_angle;
IMU_Calibration imu_cali;

//���ٶȼƵ�ͨ�˲�
static fp32 accel_fliter_1[3] = {0.0f, 0.0f, 0.0f};
static fp32 accel_fliter_2[3] = {0.0f, 0.0f, 0.0f};
static fp32 accel_fliter_3[3] = {0.0f, 0.0f, 0.0f};
static const fp32 fliter_num[3] = {1.929454039488895f, -0.93178349823448126f, 0.002329458745586203f};

fp32 gyro_scale_factor[3][3] = {BMI088_BOARD_INSTALL_SPIN_MATRIX};
fp32 gyro_offset[3];
fp32 gyro_cali_offset[3];

fp32 accel_scale_factor[3][3] = {BMI088_BOARD_INSTALL_SPIN_MATRIX};
fp32 accel_offset[3];
fp32 accel_cali_offset[3];

ist8310_real_data_t ist8310_real_data;
fp32 mag_scale_factor[3][3] = {IST8310_BOARD_INSTALL_SPIN_MATRIX};
fp32 mag_offset[3];
fp32 mag_cali_offset[3];

static fp32 IMU_gyro[3] = {0.0f, 0.0f, 0.0f};
static fp32 IMU_accel[3] = {0.0f, 0.0f, 0.0f};
static fp32 IMU_mag[3] = {0.0f, 0.0f, 0.0f};
static fp32 IMU_quat[4] = {0.0f, 0.0f, 0.0f, 0.0f};
static fp32 IMU_angle[3] = {0.0f, 0.0f, 0.0f};

void BMI088_Drive_Init(void)
{
    BMI088_Read(bmi088_real_data.gyro, bmi088_real_data.accel, &bmi088_real_data.temp);
    imu_cali_slove(IMU_gyro, IMU_accel, IMU_mag, &bmi088_real_data, &ist8310_real_data);
    AHRS_init(IMU_quat, IMU_accel, IMU_mag);

    accel_fliter_1[0] = accel_fliter_2[0] = accel_fliter_3[0] = IMU_accel[0];
    accel_fliter_1[1] = accel_fliter_2[1] = accel_fliter_3[1] = IMU_accel[1];
    accel_fliter_1[2] = accel_fliter_2[2] = accel_fliter_3[2] = IMU_accel[2];
    //����SPIƵ��
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;
    if (HAL_SPI_Init(&hspi1) != HAL_OK)
        {
            Error_Handler();
        }
    SPI1_DMA_init((uint32_t)gyro_dma_tx_buf, (uint32_t)gyro_dma_rx_buf, SPI_DMA_GYRO_LENGHT);
    imu_start_dma_flag = 1;
}
void BMI088_Read_update(void)
{
    if(gyro_update_flag & (1 << IMU_UPDATE_SHFITS))//�ж�bit2�Ƿ�Ϊ1
        {
            //bit2λ��0
            gyro_update_flag &= ~(1 << IMU_UPDATE_SHFITS);
            BMI088_gyro_read_over(gyro_dma_rx_buf + BMI088_GYRO_RX_BUF_DATA_OFFSET,bmi088_real_data.gyro);
        }
    if(accel_update_flag & (1 << IMU_UPDATE_SHFITS))
        {
            accel_update_flag &= ~(1 << IMU_UPDATE_SHFITS);
            BMI088_accel_read_over(accel_dma_rx_buf + BMI088_ACCEL_RX_BUF_DATA_OFFSET,bmi088_real_data.accel,&bmi088_real_data.time);
        }
    if(accel_temp_update_flag & (1 << IMU_UPDATE_SHFITS))
        {
            accel_temp_update_flag &= ~(1 << IMU_UPDATE_SHFITS);
            BMI088_temperature_read_over(accel_temp_dma_rx_buf + BMI088_ACCEL_RX_BUF_DATA_OFFSET,&bmi088_real_data.temp);
        }
    //��ת����Ư��
    imu_cali_slove(IMU_gyro,IMU_accel,IMU_mag, &bmi088_real_data, &ist8310_real_data);
    //���ٶȼƵ�ͨ�˲�
    accel_fliter_1[0] = accel_fliter_2[0];
    accel_fliter_2[0] = accel_fliter_3[0];
    accel_fliter_3[0] = accel_fliter_2[0] * fliter_num[0] + accel_fliter_1[0] * fliter_num[1] + IMU_accel[0] * fliter_num[2];

    accel_fliter_1[1] = accel_fliter_2[1];
    accel_fliter_2[1] = accel_fliter_3[1];
    accel_fliter_3[1] = accel_fliter_2[1] * fliter_num[0] + accel_fliter_1[1] * fliter_num[1] + IMU_accel[1] * fliter_num[2];

    accel_fliter_1[2] = accel_fliter_2[2];
    accel_fliter_2[2] = accel_fliter_3[2];
    accel_fliter_3[2] = accel_fliter_2[2] * fliter_num[0] + accel_fliter_1[2] * fliter_num[1] + IMU_accel[2] * fliter_num[2];

    AHRS_update(IMU_quat,0.002,IMU_gyro,accel_fliter_3,IMU_mag);
    get_angle(IMU_quat,IMU_angle + INS_YAW_ADDRESS_OFFSET,IMU_angle + INS_PITCH_ADDRESS_OFFSET,IMU_angle + INS_ROLL_ADDRESS_OFFSET);

    bmi088_real_angle.gyro.yaw = IMU_angle[INS_YAW_ADDRESS_OFFSET]*RADIUS_ANGLE;
    bmi088_real_angle.gyro.pitch = IMU_angle[INS_PITCH_ADDRESS_OFFSET]*RADIUS_ANGLE;
    bmi088_real_angle.gyro.roll = IMU_angle[INS_ROLL_ADDRESS_OFFSET]*RADIUS_ANGLE;
    bmi088_real_angle.accel.ax = accel_fliter_3[0];
    bmi088_real_angle.accel.ay = accel_fliter_3[1];
    bmi088_real_angle.accel.az = accel_fliter_3[2];
    bmi088_real_angle.temp = bmi088_real_data.temp;
    bmi088_real_angle.temp = bmi088_real_data.time;
	imu_continuous_processing(&bmi088_real_angle);
}
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if(GPIO_Pin == INT1_ACCEL_Pin)
        {
            accel_update_flag |= 1 << IMU_DR_SHFITS;
            accel_temp_update_flag |= 1 << IMU_DR_SHFITS;
            if(imu_start_dma_flag)
                {
                    imu_cmd_spi_dma();
                }
        }
    else if(GPIO_Pin == INT3_GYRO_Pin)
        {
            gyro_update_flag |= 1 << IMU_DR_SHFITS;
            if(imu_start_dma_flag)
                {
                    imu_cmd_spi_dma();
                }
        }
//    else if(GPIO_Pin == DRDY_IST8310_Pin)
//        {
//            mag_update_flag |= 1 << IMU_DR_SHFITS;
//            if(mag_update_flag &= 1 << IMU_DR_SHFITS)
//                {
//                    mag_update_flag &= ~(1<< IMU_DR_SHFITS);
//                    mag_update_flag |= (1 << IMU_SPI_SHFITS);
////            ist8310_read_mag(ist8310_real_data.mag);
//                }
//        }
    //��ʹ���жϻ��ѷ�ʽ
//    else if(GPIO_Pin == GPIO_PIN_0)
//    {
//        //��������
//        if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED)
//        {
//            static BaseType_t xHigherPriorityTaskWoken;
//            vTaskNotifyGiveFromISR(imu_Task_handle, &xHigherPriorityTaskWoken);
//            portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
//        }
//    }
}

/**
  * @brief          ����imu_update_flag��ֵ����SPI DMA
  * @param[in]      temp:bmi088���¶�
  * @retval         none
  */
static void imu_cmd_spi_dma(void)
{
    //���������ǵ�DMA����
    //�ж�gyro_update_flag bit0�Ƿ�Ϊ1,accel_update_flag bit1�Ƿ�Ϊ1,accel_temp_update_flag bit1�Ƿ�Ϊ1
    if( (gyro_update_flag & (1 << IMU_DR_SHFITS) ) && !(hspi1.hdmatx->Instance->CR & DMA_SxCR_EN) && !(hspi1.hdmarx->Instance->CR & DMA_SxCR_EN)
            && !(accel_update_flag & (1 << IMU_SPI_SHFITS)) && !(accel_temp_update_flag & (1 << IMU_SPI_SHFITS)))
        {
            gyro_update_flag &= ~(1 << IMU_DR_SHFITS);//bit0��0
            gyro_update_flag |= (1 << IMU_SPI_SHFITS);//bit1��1
            //GYRO��ʼ����
            HAL_GPIO_WritePin(CS1_GYRO_GPIO_Port, CS1_GYRO_Pin, GPIO_PIN_RESET);//Ƭѡ��GYRO
            SPI1_DMA_enable((uint32_t)gyro_dma_tx_buf, (uint32_t)gyro_dma_rx_buf,SPI_DMA_GYRO_LENGHT);
            return;
        }
    //�������ٶȼƵ�DMA����
    //accel_update_flag bit0�Ƿ�Ϊ1,gyro_update_flag bit1�Ƿ�Ϊ1,accel_temp_update_flag bit1�Ƿ�Ϊ1
    if((accel_update_flag & (1 << IMU_DR_SHFITS)) && !(hspi1.hdmatx->Instance->CR & DMA_SxCR_EN) && !(hspi1.hdmarx->Instance->CR & DMA_SxCR_EN)
            && !(gyro_update_flag & (1 << IMU_SPI_SHFITS)) && !(accel_temp_update_flag & (1 << IMU_SPI_SHFITS)))
        {
            accel_update_flag &= ~(1 << IMU_DR_SHFITS);//bit0��0
            accel_update_flag |= (1 << IMU_SPI_SHFITS);//bit1��1
            //ACCEL��ʼ����
            HAL_GPIO_WritePin(CS1_ACCEL_GPIO_Port, CS1_ACCEL_Pin, GPIO_PIN_RESET);//Ƭѡ��ACCEL
            SPI1_DMA_enable((uint32_t)accel_dma_tx_buf, (uint32_t)accel_dma_rx_buf, SPI_DMA_ACCEL_LENGHT);
            return;
        }
    //accel_temp_update_flag bit0�Ƿ�Ϊ1,gyro_update_flag bit1�Ƿ�Ϊ1,accel_update_flag bit1�Ƿ�Ϊ1
    if((accel_temp_update_flag & (1 << IMU_DR_SHFITS)) && !(hspi1.hdmatx->Instance->CR & DMA_SxCR_EN) && !(hspi1.hdmarx->Instance->CR & DMA_SxCR_EN)
            && !(gyro_update_flag & (1 << IMU_SPI_SHFITS)) && !(accel_update_flag & (1 << IMU_SPI_SHFITS)))
        {
            accel_temp_update_flag &= ~(1 << IMU_DR_SHFITS);//bit0��0
            accel_temp_update_flag |= (1 << IMU_SPI_SHFITS);//bit1��1

            HAL_GPIO_WritePin(CS1_ACCEL_GPIO_Port, CS1_ACCEL_Pin, GPIO_PIN_RESET);//Ƭѡ��ACCEL
            SPI1_DMA_enable((uint32_t)accel_temp_dma_tx_buf, (uint32_t)accel_temp_dma_rx_buf, SPI_DMA_ACCEL_TEMP_LENGHT);
            return;
        }
}
void DMA2_Stream2_IRQHandler(void)
{
    if(__HAL_DMA_GET_FLAG(hspi1.hdmarx, __HAL_DMA_GET_TC_FLAG_INDEX(hspi1.hdmarx)) != RESET)
        {
            __HAL_DMA_CLEAR_FLAG(hspi1.hdmarx, __HAL_DMA_GET_TC_FLAG_INDEX(hspi1.hdmarx));
            //�����Ƕ�ȡ���
            //�ж�bit1�Ƿ�Ϊ1
            if(gyro_update_flag & (1 << IMU_SPI_SHFITS))
                {
                    gyro_update_flag &= ~(1 << IMU_SPI_SHFITS);//bit1��0
                    gyro_update_flag |= (1 << IMU_UPDATE_SHFITS);//bit2��1

                    HAL_GPIO_WritePin(CS1_GYRO_GPIO_Port, CS1_GYRO_Pin, GPIO_PIN_SET); //ȡ��GYROƬѡ
                }
            //���ٶȼƶ�ȡ���
            if(accel_update_flag & (1 << IMU_SPI_SHFITS))
                {
                    accel_update_flag &= ~(1 << IMU_SPI_SHFITS);
                    accel_update_flag |= (1 << IMU_UPDATE_SHFITS);
                    HAL_GPIO_WritePin(CS1_ACCEL_GPIO_Port, CS1_ACCEL_Pin, GPIO_PIN_SET);
                }
            //�¶ȶ�ȡ���
            if(accel_temp_update_flag & (1 << IMU_SPI_SHFITS))
                {
                    accel_temp_update_flag &= ~(1 << IMU_SPI_SHFITS);
                    accel_temp_update_flag |= (1 << IMU_UPDATE_SHFITS);
                    HAL_GPIO_WritePin(CS1_ACCEL_GPIO_Port, CS1_ACCEL_Pin, GPIO_PIN_SET);
                }
            imu_cmd_spi_dma();
//		if(gyro_update_flag & (1 << IMU_UPDATE_SHFITS))
//        {
////            __HAL_GPIO_EXTI_GENERATE_SWIT(GPIO_PIN_0);
//        }
        }
}

/**
  * @brief          ��ת������,���ٶȼƺʹ�����,��������Ư,��Ϊ�豸�в�ͬ��װ��ʽ
  * @param[out]     gyro: ������Ư����ת
  * @param[out]     accel: ������Ư����ת
  * @param[out]     mag: ������Ư����ת
  * @param[in]      bmi088: �����Ǻͼ��ٶȼ�����
  * @param[in]      ist8310: ����������
  * @retval         none
  */
static void imu_cali_slove(fp32 gyro[3], fp32 accel[3], fp32 mag[3], bmi088_real_data_t *bmi088, ist8310_real_data_t *ist8310)
{
    for (uint8_t i = 0; i < 3; i++)
        {
            gyro[i] = bmi088->gyro[0] * gyro_scale_factor[i][0] + bmi088->gyro[1] * gyro_scale_factor[i][1] + bmi088->gyro[2] * gyro_scale_factor[i][2] + gyro_offset[i];
            accel[i] = bmi088->accel[0] * accel_scale_factor[i][0] + bmi088->accel[1] * accel_scale_factor[i][1] + bmi088->accel[2] * accel_scale_factor[i][2] + accel_offset[i];
            mag[i] = ist8310->mag[0] * mag_scale_factor[i][0] + ist8310->mag[1] * mag_scale_factor[i][1] + ist8310->mag[2] * mag_scale_factor[i][2] + mag_offset[i];
        }
}

void gyro_offset_init(void)
{
    imu_cali.cali_status = 1;
    imu_cali.cali_time = IMU_CALIBRATION_TIME*1000.0f/IMU_TASK_PERIOD;
    imu_cali.offset_time_count = 0;
}

void gyro_offset_calc(fp32 gyro_offset_g[3], fp32 gyro[3], uint16_t *offset_time_count)
{
	gyro_offset_g[0] = gyro_offset_g[0] - 0.0003f * gyro[0];
    gyro_offset_g[1] = gyro_offset_g[1] - 0.0003f * gyro[1];
    gyro_offset_g[2] = gyro_offset_g[2] - 0.0003f * gyro[2];
    (*offset_time_count)++;
}

void imu_cali_gyro(fp32 cali_scale[3],fp32 cali_offset[3], uint16_t *time_count)
{
        if( *time_count == 0)
        {
            gyro_offset[0] = gyro_cali_offset[0];
            gyro_offset[1] = gyro_cali_offset[1];
            gyro_offset[2] = gyro_cali_offset[2];
        }
        gyro_offset_calc(gyro_offset,IMU_gyro,time_count);
        cali_offset[0] = gyro_offset[0];
        cali_offset[1] = gyro_offset[1];
        cali_offset[2] = gyro_offset[2];
        cali_scale[0] = 1.0f;
        cali_scale[1] = 1.0f;
        cali_scale[2] = 1.0f;
}

void imu_set_cali_gyro(fp32 cali_scale[3], fp32 cali_offset[3])
{
    gyro_cali_offset[0] = cali_offset[0];
    gyro_cali_offset[1] = cali_offset[1];
    gyro_cali_offset[2] = cali_offset[2];
    gyro_offset[0] = gyro_cali_offset[0];
    gyro_offset[1] = gyro_cali_offset[1];
    gyro_offset[2] = gyro_cali_offset[2];
}

//����У׼
void bim088_gyro_offset_update(void)
{
    if(imu_cali.cali_status != 0)
        {
            if(imu_cali.offset_time_count < imu_cali.cali_time)gyro_offset_calc(gyro_offset,IMU_gyro,&imu_cali.offset_time_count);
            else if(imu_cali.offset_time_count >= imu_cali.cali_time)
                {
                    imu_cali.cali_status = 0 ;
                    imu_cali.offset_time_count = 0 ;
                }
        }
}

const imu_sensor* get_imu_point(void)
{
		return &bmi088_real_angle;
}
void imu_continuous_processing(imu_sensor *imu_sensor_t)
{
	if(imu_sensor_t->gyro.yaw-imu_sensor_t->gyro.yaw_last>180)
	{
		imu_sensor_t->gyro.yaw_count--;
	}
	else if(imu_sensor_t->gyro.yaw-imu_sensor_t->gyro.yaw_last<-180)
	{
		imu_sensor_t->gyro.yaw_count++;
	}
	imu_sensor_t->gyro.yaw_angle=imu_sensor_t->gyro.yaw+imu_sensor_t->gyro.yaw_count*360;
	
	imu_sensor_t->gyro.yaw_last = imu_sensor_t->gyro.yaw;
}