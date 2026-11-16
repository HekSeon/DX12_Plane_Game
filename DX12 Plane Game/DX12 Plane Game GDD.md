# DX12 Plane Game — Oyun Tasarımı

## Görsel yapı ve teknoloji

- Oyun DirectX 12 kullanacak.
- Kamera üçüncü şahıs bakış açısında olacak ve uçağı takip edecek.
- Dünya küresel olacak; ufka doğru uçarken dünyanın eğriliği görülebilecek.
- Dünya üzerinde dağlık arazi ve su alanları bulunacak. Arazi ve su, eklenecek dokularla kaplanacak; dağların biçimi geometriyle oluşturulacak.

## Daha sonra eklenecek varlıklar

Varlıklar daha sonra `data` klasörüne eklenecek; şu aşamada dosyaların eklenmesi beklenmiyor.

| Dosya | Kullanım |
| --- | --- |
| `plane.fbx` | Hazır uçak modeli (mesh) |
| `field.png` | Kara ve dağlık arazi dokusu |
| `sea.png` | Su/deniz dokusu |

## Sistemler

- **EngineSystem** – motor gücü (throttle), pervane RPM'i, yakıt tüketimi
- **FuelSystem** – yakıt göstergesi, motor gücüyle orantılı tüketim, yakıt biterse motor kesilmesi
- **FlightModel / LiftSystem** – senin tarif ettiğin mantık burada:
    - Hız eşiği (stall speed) altında + burun yukarıdaysa → düşüş (stall durumu)
    - Hız eşiği üstünde + burun yukarıdaysa → yükselme
    - Motor gücü yüksek + burun yukarı + hız yetersiz → havalanamama, düşme
    - Bu aslında **pitch (burun açısı) + speed + thrust** üçlüsünün bir fonksiyonu, gerçek uçuş simlerinde "angle of attack" mantığına yakın basitleştirilmiş hali
- **InputSystem** – kalkış, burun kontrolü, throttle kontrolü
- **CameraSystem** – üçüncü şahıs takip kamerası; küresel dünya üzerinde uçağın yerel yukarı yönüne uyum sağlayacak
- **WorldGenSystem** – eğriliği ufukta görünen küresel dünya; dağlık arazi ve su alanları. Parçalı (chunk-based) üretim değerlendirilebilir
- **AirportSystem** – pist, spawn/respawn noktaları, iniş algılama (landing gear + hız + açı kontrolü)
- **HUD/InstrumentSystem** – yakıt göstergesi, hız göstergesi, irtifa göstergesi
