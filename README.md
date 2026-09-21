# STM32 UART Komut İşleyici ve LED Kontrolü

Bu proje, USART2 üzerinden alınan satır tabanlı komutlarla bir LED'i kontrol eder.
Komut çözümleme kodu STM32 HAL'den bağımsızdır ve PC üzerinde otomatik olarak
test edilebilir. UART kesmesi yalnızca alınan baytı sabit boyutlu ring buffer'a
yazar; komutlar bloklamayan ana döngüde işlenir.

## Donanım ve seri port ayarları

- MCU projesi: STM32F103R6Tx (`STM32F103x6`)
- Sistem saati: dahili HSI, 8 MHz
- UART: USART2, 9600 baud, 8 data biti, parity yok, 1 stop biti
- `PA2 / USART2_TX -> COMPIM TXD` (DB9 pin 3)
- `PA3 / USART2_RX <- COMPIM RXD` (DB9 pin 2)
- `PA1 -> seri direnç -> LED -> GND` (aktif-yüksek)
- COMPIM: COM5, 9600-8-N-1, handshake yok
- com0com: COM5 ile COM6 eşleşmiş çift
- Termite: COM6, 9600-8-N-1, handshake yok, satır sonu CR-LF

Proteus'ta bulunan STM32F103C6 modeli bu uygulamada kullanılan `x6` bellek
sınıfı, USART2 ve PA1/PA2/PA3 çevre birimleri açısından uyumludur. Bağlantılar
fiziksel paket pin numarasına göre değil, `PA1`, `PA2` ve `PA3` isimlerine göre
yapılmalıdır.

COM5'i yalnız Proteus, COM6'yı yalnız Termite açmalıdır. Daha önce COM5 ile
test yapmak için kullanılan ikinci Termite penceresi Proteus başlamadan önce
kapatılmalıdır. Proteus Virtual Terminal aynı USART2 hatlarına paralel
bağlanmamalıdır.

## Komut protokolü

Komutlar büyük harfli ve tam biçimde yazılmalıdır. LF ve CR-LF satır sonları
kabul edilir. Bir komut, satır sonu hariç en fazla 32 karakter olabilir.

| Komut | Davranış | Cevap |
| --- | --- | --- |
| `LED ON` | Blink'i durdurur, LED'i yakar | `OK` |
| `LED OFF` | Blink'i durdurur, LED'i söndürür | `OK` |
| `BLINK 250` | LED'i söndürür ve 250 ms sonra blink başlatır | `OK` |
| `STATUS` | Mod, LED ve periyot bilgisini verir | Durum satırı |

`BLINK` periyodu 10 ile 5000 ms arasında olmalıdır. Geçersiz biçim, boş satır,
uzun satır veya hasarlı giriş `ERR` cevabı üretir. Başlangıçta mod `OFF`, LED
kapalı ve kayıtlı periyot 250 ms'dir.

Durum cevabının biçimi sabittir:

```text
MODE=OFF LED=OFF PERIOD=250
MODE=ON LED=ON PERIOD=250
MODE=BLINK LED=ON PERIOD=250
```

Termite'ta local echo açılırsa yazdığınız komut da görünür; `OK`, `ERR` ve
`MODE=...` satırları STM32'den geri gelen gerçek verilerdir. Firmware açılışta
kendiliğinden mesaj göndermez; ilk kontrol için `STATUS` yazıp Enter'a basın.

## Uçtan uca Proteus testi

1. com0com üzerinde COM5-COM6 çiftinin açık olduğunu doğrulayın.
2. Proteus COMPIM'i COM5'e, Termite'ı COM6'ya ayarlayın.
3. Proteus'a `Debug/tasarim.hex` dosyasını yükleyip simülasyonu başlatın.
4. Termite'tan `STATUS` gönderin; `MODE=OFF LED=OFF PERIOD=250` gelmelidir.
5. `LED ON` gönderin; `OK` gelmeli ve Proteus'taki PA1 LED'i yanmalıdır.
6. Tekrar `STATUS` gönderin; `MODE=ON LED=ON PERIOD=250` gelmelidir.
7. `BLINK 250` gönderin; `OK` gelmeli ve LED 250 ms aralıkla değişmelidir.
8. `led on` gönderin; katı komut biçimi nedeniyle `ERR` gelmelidir.

Bu işlem hem Termite -> com0com -> COMPIM -> STM32 komut yolunu hem de ters
yöndeki cevap yolunu doğrular.

## PC build ve otomatik testler

Standart C11 derleyicisi ve CMake ile:

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Testler; başlangıç durumu, komutlar, 10/5000 ms sınırları, katı ayrıştırma,
LF/CR-LF, parçalı ve ardışık komutlar, 32 karakter sınırı, RX taşması,
blink zamanlaması, gecikmiş işlem ve 32-bit tick taşmasını kapsar. GitHub
Actions aynı komutları Ubuntu üzerinde otomatik çalıştırır.

## Tasarım kararları ve hata davranışı

- RX ring buffer 64 bayttır; dinamik bellek kullanılmaz.
- STM32 üzerinde RX tek üreticili (ISR), tek tüketicili (ana döngü) çalışır.
- UART TX için 256 baytlık sabit kuyruk ve `HAL_UART_Transmit_IT()` kullanılır.
- TX kuyruğuna tam cevap sığmazsa o cevap bütünüyle reddedilir; mevcut kuyruk
  bozulmaz ve tanılama sayacı artırılır.
- RX taşmasında hasarlı satır bir sonraki LF'ye kadar atılır ve tek `ERR`
  gönderilir; sonraki satır normal işlenir.
- Blink hesabı `uint32_t` zaman farkıyla yapılır, tick taşmasına dayanıklıdır ve
  ana döngü gecikse bile zaman fazını korur.
- ISR içinde komut ayrıştırma, LED durumu değiştirme veya bloklayan bekleme
  yapılmaz.
- Sistem sonlu tamponlara sahiptir; UART'ın uzun süre tüketemediği sınırsız bir
  veri akışının kayıpsız işlenmesi garanti edilmez.
