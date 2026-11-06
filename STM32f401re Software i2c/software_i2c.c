#include "main.h"
#include "software_i2c.h"


static uint8_t flag = 0;
static uint8_t sayac = 0, first = 0;
static uint8_t j = 0;
uint8_t clock_sayac;
uint8_t i; uint8_t hat_sal = 0;
void i2c_init()
{
	// sda ve scl bitleri yüksek seviyeye alındı
	HAL_GPIO_WritePin(SDA_GPIO_Port, SDA_Pin, SET);
	HAL_GPIO_WritePin(SCL_GPIO_Port, SCL_Pin, SET);

}

void i2c_send(timer_t *timer , uint8_t address, uint8_t message)
{

	uint32_t time = HAL_GetTick();

	timer->zaman_asimi = 0;

	while(1)
	{
		timer->i2c_bekleme = HAL_GetTick();

		if(timer->i2c_bekleme - time >= 25)
		{
			// burada belli süre içinde veri gitmezse salıyoruz
			//tim3 durmus olabilir ya da baska bir zamanlayiici hatası

			HAL_GPIO_WritePin(SCL_GPIO_Port,SCL_Pin, GPIO_PIN_SET);
			HAL_GPIO_WritePin(SDA_GPIO_Port, SDA_Pin, GPIO_PIN_SET);

			timer->veri_iletildi = 0;
			timer->zaman_asimi = 1;
			hat_sal = 0;
			first = 0;
			j = 0;
			clock_sayac = 0;
			sayac = 0;

			flag = 0;

			return;
		}

		if(first == 0)
		{
			 i = 7;

			//sda düşük çektik
			HAL_GPIO_WritePin(SDA_GPIO_Port, SDA_Pin, GPIO_PIN_RESET);

			// address kaydırdık bir bit
			address = address * 2;

			timer->hata_adres = 0;
			timer->hata_veri = 0;
			timer->veri_iletildi = 0;
			timer->adres_iletildi = 0;
			timer->zaman_asimi = 0;

			first = 1;

		}



		if (first == 1)
		{
		    mikrosaniye_timer(timer, 10);

		    if (timer->sure_doldu)
		    {
		        // Sayaç 0 iken ilk adım SCL'yi düşürmek.
		        // Sonra sırayla yükseltip düşürüyoruz.
		        GPIO_PinState hedef;

		        if (clock_sayac % 2 == 0)
		            hedef = GPIO_PIN_RESET;
		        else
		            hedef = GPIO_PIN_SET;

		        HAL_GPIO_WritePin(SCL_GPIO_Port, SCL_Pin, hedef);

		        timer->pinDurumu = HAL_GPIO_ReadPin(SCL_GPIO_Port, SCL_Pin);

		        if (timer->pinDurumu == hedef)
		        {
		            clock_sayac++;
		            sayac = 1;
		        }
		        else
		        {
		            sayac = 0;
		        }
		    }
		}




		if(sayac == 1)
		{
			if(timer->pinDurumu == GPIO_PIN_RESET)
			{

				if(j <= 7)
				{

					/// maskeleme ile bit kontrol ettik böylese 0 ve 1 değerlerini gördük
					if(address & (1 << i) )
					{
						HAL_GPIO_WritePin(SDA_GPIO_Port, SDA_Pin, SET);

					}
					else
					{
						HAL_GPIO_WritePin(SDA_GPIO_Port, SDA_Pin, RESET);
					}

					i--;

					j++;

					sayac = 0;


				}
				else if(j == 8)
				{
					HAL_GPIO_WritePin(SDA_GPIO_Port, SDA_Pin, SET);
					j++;
					sayac = 0;

				}
				else if(j >= 10 && j < 18)
				{

					if(message & (1 << i))
					{
						HAL_GPIO_WritePin(SDA_GPIO_Port, SDA_Pin, SET);
					}
					else
					{
						HAL_GPIO_WritePin(SDA_GPIO_Port, SDA_Pin, RESET);

					}
					i--;

					j++;
					sayac = 0;

				}
				else if(j == 18)
				{
					HAL_GPIO_WritePin(SDA_GPIO_Port, SDA_Pin, SET);
					j++;
					sayac = 0;
				}

			}

			if(timer->pinDurumu == GPIO_PIN_SET)
			{
				if(clock_sayac == 18 && sayac == 1)
				{


					timer->ack_nack = HAL_GPIO_ReadPin(SDA_GPIO_Port, SDA_Pin);

					if(timer->ack_nack == 1)
					{
						timer->hata_adres = 1;
						timer->adres_iletildi = 0;

						while(hat_sal < 5)
						{

							timer->i2c_bekleme = HAL_GetTick();
							if(timer->i2c_bekleme - time >= 25)
							{
								// burada belli süre içinde veri gitmezse salıyoruz
								//tim3 durmus olabilir ya da baska bir zamanlayiici hatası

								HAL_GPIO_WritePin(SCL_GPIO_Port,SCL_Pin, GPIO_PIN_SET);
								HAL_GPIO_WritePin(SDA_GPIO_Port, SDA_Pin, GPIO_PIN_SET);

								timer->veri_iletildi = 0;
								timer->zaman_asimi = 1;
								hat_sal = 0;
								first = 0;
								j = 0;
								clock_sayac = 0;
								sayac = 0;

								flag = 0;

								return;
							}

							mikrosaniye_timer(timer, 5);
							if(timer->sure_doldu == true)
							{
								if(hat_sal == 3 && HAL_GPIO_ReadPin(SCL_GPIO_Port, SCL_Pin) == 0)
								{
									hat_sal = 3;
								}
								else
								{
									hat_sal++;
								}
							}

							if(hat_sal == 1)
							{
								HAL_GPIO_WritePin(SCL_GPIO_Port, SCL_Pin, RESET);
							}
							if(hat_sal == 2)
							{
								HAL_GPIO_WritePin(SDA_GPIO_Port, SDA_Pin, RESET);
							}
							if(hat_sal == 3)
							{

								HAL_GPIO_WritePin(SCL_GPIO_Port, SCL_Pin, SET);

							}
							if(hat_sal == 4)
							{
								HAL_GPIO_WritePin(SDA_GPIO_Port, SDA_Pin, SET);
							}

						}
						hat_sal = 0;
						first = 0;
						j = 0;
						clock_sayac = 0;



						return;
					}
					else
					{
						timer->hata_adres = 0;
						timer->adres_iletildi = 1;
					}

					i = 7;
					j++;
					sayac = 0;

				}

				if(clock_sayac == 36 && sayac == 1)
				{
					timer->ack_nack2 = HAL_GPIO_ReadPin(SDA_GPIO_Port, SDA_Pin);

					first = 0;
					j = 0;
					clock_sayac = 0;

					if(timer->ack_nack2 == 0 && timer->ack_nack == 0 )
					{
						timer->veri_iletildi = 1;
						timer->hata_veri = 0;

					}
					else
					{
						timer->veri_iletildi = 0;
						timer->hata_veri = 1;
					}

					while(hat_sal < 5)
					{

						timer->i2c_bekleme = HAL_GetTick();
						if(timer->i2c_bekleme - time >= 25)
						{
							// burada belli süre içinde veri gitmezse salıyoruz
							//tim3 durmus olabilir ya da baska bir zamanlayiici hatası

							HAL_GPIO_WritePin(SCL_GPIO_Port,SCL_Pin, GPIO_PIN_SET);
							HAL_GPIO_WritePin(SDA_GPIO_Port, SDA_Pin, GPIO_PIN_SET);

							timer->veri_iletildi = 0;
							timer->zaman_asimi = 1;
							hat_sal = 0;
							first = 0;
							j = 0;
							clock_sayac = 0;
							sayac = 0;

							flag = 0;

							return;
						}

						mikrosaniye_timer(timer, 5);

						if(timer->sure_doldu == true)
						{
							if(hat_sal == 3 && HAL_GPIO_ReadPin(SCL_GPIO_Port, SCL_Pin) == 0)
							{
								hat_sal = 3;
							}
							else
							{
								hat_sal++;
							}
						}

						if(hat_sal == 1)
						{
							HAL_GPIO_WritePin(SCL_GPIO_Port, SCL_Pin, RESET);
						}
						if(hat_sal == 2)
						{
							HAL_GPIO_WritePin(SDA_GPIO_Port, SDA_Pin, RESET);
						}
						if(hat_sal == 3)
						{
							HAL_GPIO_WritePin(SCL_GPIO_Port, SCL_Pin, SET);
						}
						if(hat_sal == 4)
						{
							HAL_GPIO_WritePin(SDA_GPIO_Port, SDA_Pin, SET);
						}

					}
					hat_sal = 0;

					return;


				}
			}
		}



	}

}


/************************************************************************************/
//
//	Timer Kısmı 1uS timer
//
/*************************************************************************************/
void mikrosaniye_timer_init(TIM_HandleTypeDef *htim)
{
	HAL_TIM_Base_Start(htim);

}

void mikrosaniye_timer(timer_t *timer, uint32_t zaman)
{

	timer->guncel_zaman = TIM3->CNT;

	if(flag == 0)
	{

		timer->baslangic_zaman = timer->guncel_zaman ;
		timer->sure_doldu = false;

		flag = 1;
	}

	if(timer->baslangic_zaman > timer->guncel_zaman)
	{
		if(((65535 - timer->baslangic_zaman ) + timer->guncel_zaman ) >= zaman)
		{
			flag = 0;
			timer->sure_doldu = true;

		}
		else
		{
			return;
		}

	}
	else
	{
		if((timer->guncel_zaman - timer->baslangic_zaman) >= zaman)
		{
			flag = 0;
			timer->sure_doldu = true;

		}
		else
		{
			return;
		}
	}


}



