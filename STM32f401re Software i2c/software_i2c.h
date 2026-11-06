
#ifndef INC_SOFTWARE_I2C_H_
#define INC_SOFTWARE_I2C_H_

#include "software_i2c.h"
#include <stdint.h>
#include <stdbool.h>


typedef struct
{
	uint32_t baslangic_zaman;

	uint32_t guncel_zaman;

	GPIO_PinState pinDurumu;

	GPIO_PinState ack_nack;
	GPIO_PinState ack_nack2;

	bool sure_doldu;

	uint8_t hata_veri,hata_adres, veri_iletildi, adres_iletildi, zaman_asimi;

	uint32_t i2c_bekleme;

}timer_t;


void i2c_init();

void i2c_send(timer_t *timer , uint8_t address, uint8_t message);

void mikrosaniye_timer(timer_t *timer, uint32_t zaman);

void mikrosaniye_timer_init(TIM_HandleTypeDef *htim);



#endif /* INC_SOFTWARE_I2C_H_ */
