# BOM: кермові поверхні, роз'єми та інтеграція турбіни

**Дата:** 2026-09-18  
**Призначення:** безпечний стендовий, руліжний і перший ручний польотний тест. Цей документ не дозволяє автономне наведення або керування тягою комп'ютером зору.

## 1. Спочатку визначити конфігурацію планера

«Задні планки» — це не універсальна покупна деталь, а **кермові поверхні**, геометрія яких є частиною конкретного планера.

| Схема планера | Мінімальні приводи |
|---|---:|
| Delta / flying wing | 2 незалежні серви: ліва й права elevon |
| Звичайне оперення | 2 aileron + 1 elevator + 1 rudder = 4 серви |
| V-tail | 2 aileron + 2 V-tail = 4 серви |
| Flaps / airbrake / retracts | Додаткові канали й окремо розраховані приводи |

Не використовувати Y-кабель між основними поверхнями: Cube повинен бачити й налаштовувати кожен привід окремо.

### BLOCKED: дані, без яких не можна вибрати точну серву

- тип планера та фото верх/низ/хвіст;
- довжина і хорда кожної рухомої поверхні;
- максимальна швидкість **першого** тесту;
- довжина servo arm і control horn, потрібний кут відхилення;
- розмір servo bay і допустима маса;
- напруга servo rail: 6.0, 7.4 або 8.4 V;
- чи вже є петлі, кабанчики, тяги та закладні в крилі.

Після цих вимірів рахується hinge moment, потрібний torque і запас. Маркетингового напису «20/40 kg» недостатньо.

## 2. Додати в замовлення: flight controls

| Позиція | К-сть | Статус | Примітка |
|---|---:|---|---|
| Digital HV metal-gear servo, однакова серія | 2 або 4 | **BLOCKED: torque calc** | Без пластикових редукторів; основні поверхні — незалежні канали |
| Запасна серва того самого типу | 1 | BUY разом із вибраними | Для заміни без зміни геометрії/центрування |
| Жорсткі servo frames / trays | по 1 на серву | VERIFY/BUY | Кріплення до силових елементів, не лише до обшивки |
| Control horns | по 1 на поверхню | VERIFY/BUY | Геометрія узгоджена з петлею та servo arm |
| M3/M4 threaded pushrods або carbon pushrods | комплект | VERIFY/BUY | Розмір після вимірювання довжини |
| Ball links / clevises + locknuts + safety clips | комплект + запас | BUY | Без люфту; механічна фіксація |
| Петлі / hinge pins / retainers | за планером | **BLOCKED: airframe** | Якщо рухомі поверхні ще не сформовані — це структурна робота виробника планера |
| Twisted servo extensions 20–22 AWG | за трасою | BUY після заміру | З фіксаторами роз'ємів і strain relief |
| Зовнішній redundant servo-power controller або dual-input BEC | 1 | **BLOCKED: servo V/A** | Cube **не живить servo rail** |
| Окремі servo/receiver battery packs | 2 для redundancy | **BLOCKED: servo V/A** | Не від Jetson і не від ECU турбіни |
| Fuse / distribution block для servo rail | 1 | BUY після розрахунку | Запобіжник і вимір струму мають відповідати stall current усіх серв |
| Digital airspeed sensor + pitot + tube/mount | 1 | RECOMMENDED | Потрібен перед автоматичними режимами швидкості/посадки; спочатку калібрувати |

## 3. Додати в замовлення: роз'єми та проводка

| Контур | Що потрібно |
|---|---|
| Cube TELEM1/TELEM2 | Готові 6-pin JST-GH 1.25 mm pigtails **або** GHR-06V-S + SSHL-002T-P0.2; по 2 запасні корпуси/комплекти контактів |
| Here3+ CAN | 4-pin JST-GH CAN cable; перевірити, чи є в комплекті Here3+ |
| RP1 ELRS/CRSF | Штатний 4-wire harness: power, GND, TX, RX; підключення до окремого serial port Cube |
| PWM servo rail | Якісні 3-pin servo housings/pins, locking clips, pin extractor; не застосовувати Dupont jumpers |
| USB3 камери | Два короткі екрановані USB3 cables точного типу роз'єму камер, right-angle за потреби, механічний clamp/strain relief |
| Jetson Dev Kit | Регульований DC-DC із запасом по піковій потужності + правильний barrel connector; остаточний PN лише після freeze бортової батареї |
| Високий струм | Тільки роз'єми, вказані виробником обладнання; для JetCat Option A силовий pigtail штатно має XT60, Option B використовує штатний MIL connector |
| Загальний loom | Automotive/aviation wire потрібного AWG, braided sleeve, heat-shrink, grommets, P-clamps, ferrites, service loops, labels |

### Інструменти для складання loom

- ratcheting crimper для JST-GH contacts;
- окремий якісний crimper для servo contacts;
- pin extractors, heat-shrink gun, multimeter;
- DC current clamp або power analyzer;
- servo tester і USB logic analyzer;
- запасні contacts/housings: щонайменше +20%.

## 4. Живлення: три незалежні гілки

```text
[ECU battery] ── штатний JetCat harness ──► turbine ECU / starter / pump / valves

[Servo batteries A/B] ── redundant power/BEC ──► Cube servo rail ──► servos
                                      └────────► RP1 (за схемою Cube/receiver)

[Avionics battery] ── filter/DC-DC ──► Cube power module
                                  ├──► Jetson
                                  ├──► cameras
                                  └──► окремий BEC для RFD, якщо потрібен
```

- Не живити серви від Cube: carrier board подає PWM signal, а servo rail потребує зовнішнього джерела.
- Не живити Jetson/камери від ECU battery турбіни.
- Не з'єднувати силові виходи різних BEC. Signal ground/reference підключати лише за офіційними схемами пристроїв.
- До кожної гілки: окремий fuse, доступний master disconnect, вимір напруги/струму, маркування.

## 5. Підключення JetCat P250-PRO-S V2

Перед замовленням сфотографувати nameplate і роз'єм двигуна та визначити **Option A чи Option B**.

### Якщо це повний P250-PRO-S V2 set

ECU, BLDC starter, fuel pump і два solenoid valves уже інтегровані — дублювати їх не потрібно.

| Позиція | Статус | Примітка |
|---|---|---|
| Оригінальний JetCat engine/data/power harness | VERIFY | Option A: 15-pin D-sub + окремий XT60 power pigtail; Option B: 23-pin MIL connector |
| JetCat PRO-Interface V2 | VERIFY/BUY | Інтерфейс між ECU, THR/AUX receiver inputs, telemetry/GSU |
| JetCat GSU / programmer | VERIFY/BUY | Налаштування, діагностика, fault readout |
| Окремий ECU battery | VERIFY/BUY | Для P250-PRO-S-V2 JetCat рекомендує 3S LiPo; capacity перевірити за актуальним manual і тривалістю тесту |
| Паливний бак | **BLOCKED: endurance** | Об'єм = потрібний час + резерв, за реальною витратою й схемою виробника планера |
| Fuel-rated tubing/fittings/filter/vent/fill hardware | VERIFY/BUY | Лише сумісні з паливом та за схемою JetCat/airframe builder |
| Fire shield, heat shield, exhaust clearance hardware | VERIFY/BUY | За кресленням силової установки, не імпровізувати біля гарячої секції |
| Доступний manual fuel shutoff і ECU master disconnect | BUY/VERIFY | Повинен бути доступний наземній команді |

### Сигнальна схема першого тесту

```text
Pilot TX ──ELRS──► RP1 ──CRSF──► Cube
                                  ├── PWM output assigned to pilot throttle ──► JetCat PRO-Interface THR
                                  └── separate PWM output assigned only to
                                      pilot engine-stop switch ──────────────► JetCat PRO-Interface AUX
```

- THR/AUX підключаються за JetCat connection chart як receiver-style inputs.
- На першому gate **лише пілот** керує throttle й engine stop; ці два outputs не приймають commands від vision computer.
- Не подавати напругу servo rail у power input ECU та не замінювати штатний JetCat harness саморобним.
- Точні endpoint, failsafe, start/stop sequence і telemetry protocol беруться з manual конкретного serial/version.

## 6. Наступний етап робіт

1. **Airframe ICD freeze:** фото, схема оперення, виміри поверхонь/важелів/відсіків, маса, test-speed envelope, engine Option A/B.
2. **Torque і power calculation:** вибрати конкретні серви, servo voltage, redundant power controller, packs, wire gauge і fuses.
3. **Iron-bird bench без палива:** TX → RP1 → Cube → серви; перевірити напрямки, endpoint, механічні упори, втрату RC, brownout і сумарний stall current.
4. **ECU bench:** тільки штатний JetCat harness/PRO-Interface; підтвердити manual throttle, AUX stop і failsafe до запуску двигуна.
5. **Закріплений engine test stand:** EMI/brownout/temperature test з окремими гілками живлення та emergency procedure.
6. **Taxi test**, потім перший ручний flight envelope. Автоматичні режими додаються після калібрування airspeed та підтверджених logs.

## 7. Мінімальні докази PASS до першого польоту

- кожна поверхня рухається у правильний бік і не впирається механічно;
- RC loss переводить control surfaces і throttle у наперед визначений безпечний стан;
- engine stop працює незалежно від Jetson/GCS;
- від'єднання/перезапуск Jetson не впливає на Cube, receiver, servos або ECU;
- при одночасному русі всіх серв немає brownout;
- при запуску/роботі турбіни немає reset Cube/RP1/RFD і помилкових PWM commands;
- CG, throw limits і fasteners підписані та перевірені другою людиною.

## 8. Офіційні джерела

- CubePilot: https://docs.cubepilot.org/user-guides/autopilot/the-cube-user-manual
- ArduPilot servo functions: https://ardupilot.org/plane/docs/servo-functions.html
- ArduPilot elevon setup: https://ardupilot.org/plane/docs/guide-elevon-plane.html
- ArduPilot servo power: https://ardupilot.org/plane/docs/common-servo.html
- ArduPilot airspeed: https://ardupilot.org/plane/docs/airspeed.html
- JetCat P250-PRO-S-V2: https://www.jetcat.de/en/productdetails/produkte/jetcat/produkte/Professionell/p250%20pro%20s%20v2
- JetCat P250 connection chart Option A: https://www.jetcat.de/jetcat/produkte/pro/Connection%20Chart%20Pro/Pro%20s/P250%20PRO-S%20Connection%20Chart%20Option%20A.pdf
- JetCat P250 basic technical information: https://www.jetcat.de/jetcat/anleitungen/P250-PRO-BasicTechn-Information-V1-4.pdf
