# STM32F401RE ile Software I²C

Bu projede STM32F401RE’nin iki GPIO pinini kullanarak yazılımsal I²C ile veri gönderdim. STM32F407VG tarafında ise veriyi donanımsal I2C1 ile alıp USB üzerinden bilgisayara aktardım.

## 1. STM32F401RE’de SDA ve SCL pinlerinin ayarlanması

CubeMX’te **PA4** ve **PA6** pinlerini normal birer dijital çıkış (`GPIO_Output`) olarak seçtim. Ardından GPIO ayarlarından her iki pinin çıkış modunu **Output Open Drain**, başlangıç seviyesini **High** olarak ayarladım. PA4’e `SDA`, PA6’ya `SCL` etiketini verdim.

| Pin | Etiket | Görevi |
|---|---|---|
| PA4 | `SDA` | Adres ve veri bitlerini gönderir, alıcının ACK cevabını okur. |
| PA6 | `SCL` | Veri aktarımı için saat darbelerini üretir. |

**Open-drain seçiminin nedeni**, aynı hattı hem göndericinin hem de alıcının aşağı çekebilmesidir. GPIO’ya `RESET` yazdığımda hat `0` olur. `SET` yazdığımda ise pin hattı serbest bırakır ve pull-up direnci hattı `1` seviyesine çıkarır. Böylece gönderici, ACK sırasında SDA’yı serbest bıraktığında alıcı aynı hattı aşağı çekerek cevap verebilir.

Projede dahili pull-up’lar kapalıdır. Bağlantıda SDA ve SCL hatlarının her biri ayrı bir pull-up direnciyle **3,3 V**’a bağlanmalıdır.

![STM32F401RE üzerinde PA4 ve PA6 pinlerinin open-drain çıkış ayarları](docs/Pinler.png)

## 2. STM32F401RE clock ve TIM3 ayarları

F401 tarafında **8 MHz HSE** kullandım. PLL ayarlarını `M = 4`, `N = 84`, `P = 2` seçerek sistem saatini **84 MHz** yaptım. APB1 bölücüsü `/2` olduğu için APB1 çevre birimi saati **42 MHz**, TIM3’ün kullandığı APB1 timer saati ise **84 MHz** oldu.

![STM32F401RE için 84 MHz sistem saati ve APB1 timer saati](docs/clock.png)

I²C sinyalleri arasındaki süreyi mikrosaniye cinsinden ölçmek için TIM3’ü şu şekilde ayarladım:

| TIM3 ayarı | Değer |
|---|---|
| Clock Source | Internal Clock |
| Counter Mode | Up |
| Prescaler | `84 - 1` |
| Counter Period | `65535 - 1` |

Prescaler, 84 MHz timer saatini 84’e böler. Böylece sayaç **1 MHz**’de çalışır ve `TIM3->CNT` her **1 µs**’de bir artar.

## 3. `software_i2c` sürücüsü

Sürücü `software_i2c.c` ve `software_i2c.h` dosyalarından oluşur. GPIO pinlerini STM32 HAL fonksiyonlarıyla kontrol eder. START, adres, veri, ACK ve STOP adımlarını yazılım üretir.

### `i2c_init()` — hattı başlangıç durumuna getirme

```c
i2c_init();
```

Bu fonksiyon SDA ve SCL pinlerine `SET` yazarak iki hattı da serbest bırakır. Pull-up dirençleri sayesinde hatlar HIGH seviyesine çıkar ve I²C hattı ilk aktarım için hazır olur. Pinlerin open-drain ayarı daha önce `MX_GPIO_Init()` içinde yapılmış olmalıdır.

### `mikrosaniye_timer_init()` — zamanlayıcıyı başlatma

```c
mikrosaniye_timer_init(&htim3);
```

Fonksiyon, parametre olarak verilen timer’ı `HAL_TIM_Base_Start()` ile başlatır. `&htim3`, CubeMX’in oluşturduğu TIM3 tanıtıcısının adresidir.

Sürücü içindeki `mikrosaniye_timer()` ise `TIM3->CNT` değerini okuyarak geçen süreyi hesaplar. İstenen süre dolduğunda `timer->sure_doldu` bayrağını set eder. Sürücü bu mekanizmayı SCL seviyesini **10 µs** aralıklarla değiştirmek ve STOP adımlarını **5 µs** aralıklarla yürütmek için kullanır.

### `i2c_send()` — bir bayt gönderme

```c
void i2c_send(timer_t *timer, uint8_t address, uint8_t message);
```

| Parametre | Açıklama |
|---|---|
| `timer` | Zamanlama bilgilerini ve aktarım sonucunu tutan `timer_t` yapısının adresi. |
| `address` | Alıcının **7 bit I²C adresi**. Bu projede `0x28` kullanılır. |
| `message` | Gönderilecek **tek bayt**. Bir karakter veya `uint8_t` değer olabilir. |

Örneğin `i2c_send(&timer, 0x28, 'A')` çağrısında sürücü önce START üretir. `0x28` adresini bir bit sola kaydırıp yazma bitiyle birlikte `0x50` olarak gönderir. Ardından SDA’yı serbest bırakıp dokuzuncu saat darbesinde alıcının ACK cevabını okur. Adres onaylanırsa `'A'` baytını gönderir, ikinci ACK’i kontrol eder ve STOP üretir.

```text
START → adres + yazma biti → ACK → veri baytı → ACK → STOP
```

Fonksiyon aktarım bitene kadar bekler. Sonuç `timer.veri_iletildi`, `timer.hata_adres` ve `timer.hata_veri` alanlarından kontrol edilebilir. İşlem **25 ms** içinde tamamlanamazsa `timer.zaman_asimi` set edilir.

## 4. Sürücüyü projeye dahil etme

1. `software_i2c.h` dosyasını `Core/Inc/`, `software_i2c.c` dosyasını `Core/Src/` klasörüne kopyalayın.
2. CubeMX’te SDA ve SCL pinlerini ilk bölümdeki gibi ayarlayın. `SDA` ve `SCL` etiketlerini kullanın. Sürücü, bu etiketlerden üretilen `SDA_Pin`, `SDA_GPIO_Port`, `SCL_Pin` ve `SCL_GPIO_Port` tanımlarını kullanır.
3. TIM3’ü **1 µs sayaç adımıyla** yapılandırın. Farklı bir timer kullanacaksanız sürücüdeki `TIM3->CNT` okumasını da değiştirin.
4. `main.c` dosyasında `main.h` sonrasına sürücü başlığını ekleyin ve zamanlayıcı yapısını tanımlayın.

```c
#include "main.h"
#include "software_i2c.h"

static timer_t timer;
```

`main()` içinde, CubeMX’in `MX_GPIO_Init()` ve `MX_TIM3_Init()` çağrılarından sonra sürücüyü başlatın:

```c
mikrosaniye_timer_init(&htim3);
i2c_init();
```

Artık her çağrıda bir bayt gönderebilirsiniz:

```c
i2c_send(&timer, 0x28, 'A');
```

### Örnekte buton ve debounce kullanımı

F401 projesinde gönderimi **PC13** üzerindeki kullanıcı butonu başlatır. Butonun ham değeri her değiştiğinde `HAL_GetTick()` ile zaman kaydedilir. Değer **50 ms** boyunca sabit kaldığında yeni durum kabul edilir. Böylece mekanik kontak titreşimi tek basışta birden fazla mesaj gönderilmesine yol açmaz.

Butona basıldığında PA5 üzerindeki LED durum değiştirir. Ardından `kadir ` metni, bir sayaç karakteri ve satır sonu (`\n`) olmak üzere **8 bayt** gönderilir. Her bayt için `i2c_send()` ayrı çağrılır. Sayaç `0` ile `9` arasında döner.

## 5. STM32F407VG tarafında verinin alınması

Alıcı kartta CubeMX üzerinden **I2C1** birimini etkinleştirdim. İki kart arasındaki bağlantı şu şekildedir:

| Hat | STM32F401RE · Gönderici | STM32F407VG · Alıcı |
|---|---|---|
| SDA | PA4 | PB7 · I2C1_SDA |
| SCL | PA6 | PB8 · I2C1_SCL |
| GND | GND | GND |

![STM32F407VG üzerinde I2C1 ve USB pinleri](docs/STMF407_pin.png)

F407’de 7 bit adresleme kullanılır. Göndericiye verilen adres `0x28`’dir. F407 HAL yapılandırmasında bunun bir bit sola kaydırılmış karşılığı olan `0x50` (`80`) değeri `OwnAddress1` alanına yazılır.

Alıcı, ana döngüde her çağrıda bir bayt bekler:

```c
durum = HAL_I2C_Slave_Receive(&hi2c1, &gelen_mesaj[i], 1, 1000);
```

Burada `&hi2c1` kullanılan I²C birimini, `&gelen_mesaj[i]` alınan baytın yazılacağı yeri, `1` beklenecek bayt sayısını ve `1000` milisaniye cinsinden zaman aşımını belirtir. Fonksiyon `HAL_OK` döndürdüğünde indeks artırılır. **8 bayt** tamamlandığında PD12 üzerindeki LED durum değiştirir ve mesaj USB üzerinden gönderilmek üzere hazır olur.

## 6. F407’den USB ile bilgisayara gönderme

F407’nin bilgisayarda sanal seri port olarak görünmesi için CubeMX’te iki ayar yaptım:

1. **Connectivity → USB_OTG_FS** altında modu **Device_Only** seçtim. Böylece F407, **PA11 (USB_DM)** ve **PA12 (USB_DP)** pinleri üzerinden USB cihazı olarak çalışır.
2. **Middleware and Software Packs → USB_DEVICE** bölümünü etkinleştirip Full Speed cihaz sınıfını **Communication Device Class (Virtual Port Com)** seçtim. Bu ayar projeye USB CDC desteğini ve `CDC_Transmit_FS()` fonksiyonunu ekler.

![STM32F407VG USB_OTG_FS Device_Only ayarı ve USB_DEVICE seçimi](docs/usb_ayar.png)

USB için gereken **48 MHz** saat de F407’nin PLL çıkışından sağlanır. Bu kartta **8 MHz HSE**, `M = 4`, `N = 72`, `P = 2` ve `Q = 3` kullanılır. Böylece sistem saati **72 MHz**, USB saati **48 MHz** olur.

![STM32F407VG için 72 MHz sistem saati ve 48 MHz USB saati](docs/stm32f407_clock.png)

Başlangıçta `MX_USB_DEVICE_Init()` USB cihazını başlatır. Sekiz bayt toplandığında ve bilgisayar USB cihazını yapılandırdığında alıcı şu çağrıyla mesajı gönderir:

```c
CDC_Transmit_FS(gelen_mesaj, sizeof(gelen_mesaj));
```

Çıktıyı görmek için her projeyi ilgili karta yükleyin, F407’nin **USB OTG FS portunu** veri destekleyen bir kabloyla bilgisayara bağlayın ve seri terminalde o karta ait sanal portu açın. F401 üzerindeki butona art arda basıldığında beklenen çıktı:

```text
kadir 0
kadir 1
kadir 2
```
