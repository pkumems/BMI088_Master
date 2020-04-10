#ifndef _BSP_BMI088_H
#define _BSP_BMI088_H
#include "struct_typedef.h"
#include "FreeRtos.h"

#define RADIUS_ANGLE 57.2957795130822f

#define SPI_DMA_GYRO_LENGHT       8
#define SPI_DMA_ACCEL_LENGHT      9
#define SPI_DMA_ACCEL_TEMP_LENGHT 4

#define IMU_DR_SHFITS        0
#define IMU_SPI_SHFITS       1
#define IMU_UPDATE_SHFITS        2

#define BMI088_GYRO_RX_BUF_DATA_OFFSET  1
#define BMI088_ACCEL_RX_BUF_DATA_OFFSET 2

#define INS_YAW_ADDRESS_OFFSET    0
#define INS_PITCH_ADDRESS_OFFSET  1
#define INS_ROLL_ADDRESS_OFFSET   2

#define IMU_TASK_PERIOD 2      //���������Ǹ���ʱ�� ��λ:ms
#define IMU_CALIBRATION_TIME 10 //У׼ʱ�� ��λ:s

#define BMI088_BOARD_INSTALL_SPIN_MATRIX    \
    {0.0f, 1.0f, 0.0f},                     \
    {-1.0f, 0.0f, 0.0f},                     \
    {0.0f, 0.0f, 1.0f}                      \


#define IST8310_BOARD_INSTALL_SPIN_MATRIX   \
    {1.0f, 0.0f, 0.0f},                     \
    {0.0f, 1.0f, 0.0f},                     \
    {0.0f, 0.0f, 1.0f}
typedef  struct
{
    struct
    {
        float yaw;
		float yaw_last;
        float pitch;
        float roll;
		int yaw_count;
		float yaw_angle;
    } gyro;
    struct
    {
        float ax;
        float ay;
        float az;
    } accel;
    float temp;
    float time;
}imu_sensor;
typedef struct
{
    bool_t cali_status;//0ֹͣУ׼ 1����У׼
    uint16_t cali_time;//У׼ʱ������
    uint16_t offset_time_count;
}IMU_Calibration;
void imu_cali_gyro(fp32 cali_scale[3],fp32 cali_offset[3], uint16_t *time_count);
void imu_set_cali_gyro(fp32 cali_scale[3], fp32 cali_offset[3]);
const imu_sensor* get_imu_point(void);
extern fp32 IMU_mag[3] ;
#endif