/*
 * app_join.h
 *
 *  Created on: Apr 13, 2022
 *      Author: ASUS
 */

#ifndef SMART_DOOR_H_
#define SMART_DOOR_H_

#include "MQTTClient.h"


static MQTTCtx mqt = {0};
/**
 * initialize the application
 * @return 0 on success else -1
 */
int app_init(void);

/**
 * main application routine.
 */
void run_app(void);

#endif /* SMART_DOOR_H_ */
