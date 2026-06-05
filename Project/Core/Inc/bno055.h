#ifndef __BNO055_H__
#define __BNO055_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "drv_bno055/inc/bno055_driver.h"


#define BNO055_DEVICE_INDEX 0U

extern BNO_Status_t BnoStatus;
extern BNO055_StatusRequest_t BnoStatusRequest;
extern BNO055_ReadRequest_t BnoReadRequest;

BNO055_Error_t BNO055_Init(void);
BNO055_Error_t BNO055_Recover(void);

#ifdef __cplusplus
}
#endif

#endif /* __BNO055_H__ */
