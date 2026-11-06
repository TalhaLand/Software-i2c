# STM32F401RE ile Yazılımsal I²C Haberleşmesi

Bu projede STM32F401RE üzerinde SDA ve SCL pinlerini yazılımla kontrol ederek bir I²C gönderici geliştirdim. STM32F401RE üzerindeki butona her basıldığında STM32F407VG’ye bir mesaj gönderilir. STM32F407VG, mesajı donanımsal I2C1 üzerinden alır ve USB CDC ile bilgisayardaki seri terminale aktarır.

| Proje klasörü | Kart | Görevi |
|---|---|---|
| `Software_i2c/` | STM32F401RE | Butonu okur ve GPIO üzerinden I²C sinyallerini üretir. |
| `stm32f407_software/` | STM32F407VG | I2C1 ile veriyi alır ve USB üzerinden bilgisayara gönderir. |

## Bağlantılar

| Hat | STM32F401RE | STM32F407VG |
|---|---|---|
| SDA | PA4 | PB7 · I2C1_SDA |
| SCL | PA6 | PB8 · I2C1_SCL |
| GND | GND | GND |

SDA ve SCL hatlarının her birini ayrı bir pull-up direnciyle **3,3 V** seviyesine bağlayın. İki projede de bu pinlerin dahili pull-up dirençleri kapalıdır. F401 tarafında pinler `Output Open Drain`, F407 tarafında `Alternate Function Open Drain` olarak ayarlanmıştır.

## Çalıştırma

1. İki proje klasörünü STM32CubeIDE’ye aktarın. Her projeyi derleyip ilgili karta yükleyin.
2. F407’nin **USB OTG FS** portunu veri destekleyen bir USB kablosuyla bilgisayara bağlayın.
3. Seri terminalde F407’nin USB CDC sanal seri portunu açın.
4. F401 üzerindeki kullanıcı butonuna basın. Her basışta terminalde yeni bir satır görünmesi beklenir.

```text
kadir 0
kadir 1
kadir 2
```

Sayaç `9` değerinden sonra tekrar `0` olur.

## Yazılımsal I²C nasıl çalışıyor?

Gönderici sürücüsü `Software_i2c/Core/Src/software_i2c.c` içindedir. GPIO erişimi için STM32 HAL kullanılır. START, STOP, saat darbeleri ve ACK kontrolünü sürücü üretir. Open-drain çıkışta `RESET` hattı aşağı çeker; `SET` ise hattı serbest bırakır ve pull-up direnci hattı yukarı çeker.

`i2c_send(&timer, 0x28, veri)` çağrısı bir bayt gönderir. Adres, kaydırılmadan **7 bit** olarak verilir. Sürücü adresi bir bit sola kaydırır ve yazma işlemi için hatta `0x50` çıkarır.

```text
START → 0x50 (adres + yazma biti) → ACK → veri baytı → ACK → STOP
```

Sürücü, adres ve veri baytından sonraki dokuzuncu saat darbesinde SDA’yı okuyarak ACK kontrolü yapar. SCL’yi serbest bıraktığında pinin gerçekten yükseldiğini de kontrol eder. Aktarım 25 ms içinde tamamlanamazsa hatları serbest bırakır ve `zaman_asimi` bayrağını ayarlar.

Örnekte `kadir ` metni, sayaç karakteri ve `\n` toplam **8 bayt** oluşturur. Her bayt kendi START ve STOP koşullarıyla ayrı bir I²C işleminde gönderilir.

## Clock ayarları ve yazılımsal zamanlayıcı

| Ayar | STM32F401RE | STM32F407VG |
|---|---:|---:|
| Harici saat kaynağı (HSE) | 8 MHz | 8 MHz |
| PLL M / N / P / Q | 4 / 84 / 2 / 4 | 4 / 72 / 2 / 3 |
| SYSCLK / HCLK | 84 MHz | 72 MHz |
| APB1 çevre birimi saati | 42 MHz | 36 MHz |
| APB1 timer saati | 84 MHz | 72 MHz |
| APB2 çevre birimi saati | 84 MHz | 72 MHz |

**F401’de mikrosaniye ölçümü:** TIM3, APB1 timer saatinden 84 MHz alır. Prescaler `84 - 1` olduğundan sayaç 1 MHz’de çalışır. Böylece `TIM3->CNT` her **1 µs**’de bir artar. `mikrosaniye_timer()` başlangıç değerini saklar, sayaç taşmasını hesaba katar ve istenen süre dolduğunda `sure_doldu` bayrağını ayarlar.

Sürücü SCL seviyesini değiştirmek için **10 µs**, STOP adımları için **5 µs** aralık kullanır. İki yarım periyottan hesaplanan nominal SCL frekansı yaklaşık **50 kHz**’dir. GPIO işlemleri ve yazılımın çalışma süresi gerçek frekansı etkiler. Zamanlama, ana döngü içinde sayacı okuyarak yapılır. `i2c_send()` aktarım bitene veya zaman aşımına kadar çağıran kodu bekletir.

**F407’de USB saati:** PLLQ bölücüsü `3` seçilerek USB OTG FS için gereken **48 MHz** üretilir.

## Buton ve debounce

F401 üzerindeki kullanıcı butonu **PC13** pininden okunur ve basıldığında `0` seviyesine geçer. Mekanik kontak titreşiminin birden fazla gönderim başlatmasını önlemek için yazılımsal debounce uygulanır.

- Ham pin değeri her değiştiğinde `HAL_GetTick()` ile son değişim zamanı kaydedilir.
- Değer **50 ms** boyunca sabit kalırsa yeni buton durumu kabul edilir.
- Basılı duruma geçildiğinde **PA5** üzerindeki LED durum değiştirir ve mesaj bir kez gönderilir. Yeni bir gönderim için butonun bırakılıp tekrar basılması gerekir.

## Alıcı ve USB CDC

F407 üzerinde I2C1, **7 bit adresleme** ile alıcı olarak çalışır. `OwnAddress1 = 80` değeri `0x50`’ye, yani göndericide kullanılan `0x28` adresinin bir bit sola kaydırılmış hâline karşılık gelir.

`HAL_I2C_Slave_Receive()` her çağrıda bir bayt alır. Sekiz bayt tamamlandığında **PD12** üzerindeki LED durum değiştirir. USB bağlantısı yapılandırılmışsa `CDC_Transmit_FS()` bu sekiz baytı bilgisayara gönderir. Alıcı mesaj sınırını bayt sayısına göre belirlediği için bir bayt kaybolursa sonraki mesajların hizası kayabilir.

USB ayarı `USB_OTG_FS → Device_Only`, cihaz sınıfı ise **CDC (Virtual COM Port)** olarak seçilmiştir. USB veri pinleri **PA11 (D−)** ve **PA12 (D+)**’dır.

## Sürücüyü başka bir projeye ekleme

1. `Software_i2c/Core/Inc/software_i2c.h` dosyasını projenizin `Core/Inc/` klasörüne, `Software_i2c/Core/Src/software_i2c.c` dosyasını `Core/Src/` klasörüne kopyalayın.
2. CubeMX’te iki GPIO pinini `Output Open Drain` olarak ayarlayın. Başlangıç seviyesini `High` seçin ve pinlere `SDA` ile `SCL` etiketlerini verin.
3. TIM3’ü yukarı sayacak şekilde ayarlayın. Sayaç frekansı **1 MHz** olmalı. Örnekte prescaler `84 - 1`, counter period `65535 - 1` olarak ayarlıdır. Sürücü doğrudan `TIM3->CNT` okuduğu için başka timer kullanırsanız bu satırı da güncelleyin.
4. GPIO ve TIM3 başlatıldıktan sonra sürücüyü aşağıdaki gibi kullanın.

```c
#include "main.h"
#include "software_i2c.h"

static timer_t timer;

/* main() içinde, MX_GPIO_Init() ve MX_TIM3_Init() sonrasında */
mikrosaniye_timer_init(&htim3);
i2c_init();

/* 7 bit adresi 0x28 olan alıcıya bir bayt gönder */
i2c_send(&timer, 0x28, 'A');
```

Aktarım sonucunu `timer.veri_iletildi`, `timer.hata_adres`, `timer.hata_veri` ve `timer.zaman_asimi` alanlarından kontrol edebilirsiniz. Bu sürüm **tek bayt yazma** işlemini destekler. Örnekteki F407 yazılımıyla deneme yaparken terminal çıktısı için sekiz bayt gönderin.

## CubeMX ekran görüntüleri

| STM32F401RE | STM32F407VG |
|---|---|
| [GPIO pin ayarları](docs/Pinler.png) | [I2C1 ve USB pinleri](docs/STMF407_pin.png) |
| [84 MHz clock ayarları](docs/clock.png) | [72 MHz clock ayarları](docs/stm32f407_clock.png) |
| | [USB OTG FS ayarları](docs/usb_ayar.png) |
