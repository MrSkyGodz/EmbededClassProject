/*
 * UartApplication.h
 *
 *  Created on: 22 May 2026
 *      Author: akdal
 */

#ifndef INC_UARTAPPLICATION_H_
#define INC_UARTAPPLICATION_H_
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

void UartStart();
bool WriteIcdFrame(void* context, const uint8_t* frame, uint8_t frameLength);
bool IsIcdFrameTxBusy(void* context);
void* GetDebugUartTransfer();


#endif /* INC_UARTAPPLICATION_H_ */
